#include "AceProJobManager.h"

AceProJobManager::AceProJobManager(Print& logger) : _logger(logger) {}

bool AceProJobManager::setup(OnExecuteJobCallback executeJobCallback,
                             OnGetCurrentPhaseCallback onGetCurrentPhaseCallback,
                             OnIdleCallback idleCallback, void* manager) {
  _logger.println(F("AceProJobManager::setup: Initializing..."));

  _executeJobCallback = executeJobCallback;
  _getCurrentPhaseCallback = onGetCurrentPhaseCallback;
  _idleCallback = idleCallback;
  _manager = manager;

  // If the task is already running, we don't need to create it again.
  if (_task != nullptr) {
    _logger.println(F("AceProJobManager::setup: WARNING: Command task is already running. Don't call setup() again!"));
    return true;
  }

  // Create a new concurrent task that runs independently under the FreeRTOS scheduler.
  if (xTaskCreate(_onCreateTaskHandler, "aceCmd", 8192, this, 1, &_task) != pdPASS) {
    _task = nullptr;
    _logger.println(F("AceProJobManager::setup: ERROR: Failed to start the command task."));
    return false;
  }

  _logger.println(F("AceProJobManager::setup: Initializing DONE."));
  return true;
}

// Create a new task that runs the command processing loop.
void AceProJobManager::_onCreateTaskHandler(void* jobManager) {
  static_cast<AceProJobManager*>(jobManager)->_runTask();
}

void AceProJobManager::_runTask() {
  _logger.println(F("AceProJobManager::_runTask: Command task started."));

  for (;;) {
    // Sleep until request() notifies this task. Wake every second while idle so
    // the manager can drop the USB host before the dryer disconnects it.
    // pdTRUE clears the notification count when the wait ends.
    const uint32_t notified = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1000));
    if (notified == 0) {
      if (_state.load(std::memory_order_acquire) == State::IDLE && _idleCallback != nullptr) {
        _idleCallback(_manager);
      }
      continue;
    }
    if (_state.load(std::memory_order_acquire) != State::RUNNING) {
      continue;
    }

    const Job job = _currentJob.load(std::memory_order_acquire);

    _logger.print(F("AceProJobManager::_runTask: Start job "));
    switch (job) {
      case Job::NONE:
        _logger.println(F("NONE."));
        break;
      case Job::DRYER_STATUS:
        _logger.println(F("DRYER_STATUS."));
        break;
      case Job::STOP:
        _logger.println(F("STOP."));
        break;
    }

    if (_executeJobCallback != nullptr) {
      _executeJobCallback(job, _manager);
    }

    const char* phase = _getCurrentPhaseCallback != nullptr ? _getCurrentPhaseCallback(_manager) : "idle";
    _logger.print(F("AceProJobManager::_runTask: Command task finished after "));
    _logger.print(millis() - _jobStartedTimestampMs);
    _logger.print(F(" ms, phase "));
    _logger.print(phase);
    _logger.println(F("."));

    _logger.println(F("AceProJobManager::_runTask: Set state to RESULT_READY."));
    _state.store(State::RESULT_READY, std::memory_order_release);
  }
}

bool AceProJobManager::request(Job job) {
  _logger.print(F("AceProJobManager::request: Requesting job "));
  switch (job) {
    case Job::NONE:
      _logger.println(F("NONE."));
      break;
    case Job::DRYER_STATUS:
      _logger.println(F("DRYER_STATUS."));
      break;
    case Job::STOP:
      _logger.println(F("STOP."));
      break;
  }

  if (_task == nullptr) {
    _logger.println(F("AceProJobManager::request: ERROR: Command task is not running!"));
    return false;
  }

  State expected = State::IDLE;
  if (!_state.compare_exchange_strong(expected, State::RUNNING, std::memory_order_acq_rel)) {
    _logger.print(F("AceProJobManager::request: WARNING: Command already running, phase "));
    _logger.print(_getCurrentPhaseCallback != nullptr ? _getCurrentPhaseCallback(_manager) : "idle");
    _logger.print(F(", elapsed "));
    _logger.print(getJobRunningTimeElapsedMs());
    _logger.println(F(" ms."));
    return false;
  }

  _currentJob.store(job, std::memory_order_release);
  _jobStartedTimestampMs = millis();

  // Increase the notification count for the command task and wake it up. 
  // This will unblock the task waiting in ulTaskNotifyTake() in the _runTask() loop, allowing it to read _currentJob and execute the requested job.
  _logger.println(F("AceProJobManager::request: Request queued. Notifying the command task..."));
  xTaskNotifyGive(_task);
  _logger.println(F("AceProJobManager::request: Request queued. Notifying the command task DONE."));

  _logger.println(F("AceProJobManager::request: Requesting job DONE."));
  return true;
}

bool AceProJobManager::take(Job job) {
  if (_currentJob.load(std::memory_order_acquire) != job ||
      _state.load(std::memory_order_acquire) != State::RESULT_READY) {
    return false;
  }

  _currentJob.store(Job::NONE, std::memory_order_release);
  _state.store(State::IDLE, std::memory_order_release);
  return true;
}

bool AceProJobManager::isRunning() const {
  return _state.load(std::memory_order_acquire) == State::RUNNING;
}

unsigned long AceProJobManager::getJobRunningTimeElapsedMs() const {
  if (!isRunning()) {
    return 0;
  }
  return millis() - _jobStartedTimestampMs;
}
