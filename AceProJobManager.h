#pragma once

#include <Arduino.h>
#include <atomic>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// One ACE Pro command at a time, running off the main loop.
class AceProJobManager {
 public:
  enum class Job : uint8_t { NONE, DRYER_STATUS, STOP };

  using OnExecuteJobCallback = void (*)(Job job, void* manager);
  using OnGetCurrentPhaseCallback = const char* (*)(void* context);
  using OnIdleCallback = void (*)(void* manager);

  explicit AceProJobManager(Print& logger);

  bool setup(OnExecuteJobCallback handler, OnGetCurrentPhaseCallback phase, OnIdleCallback idle,
             void* manager);
  bool request(Job job);
  bool take(Job job);
  bool isRunning() const;
  unsigned long getJobRunningTimeElapsedMs() const;

 private:
  enum class State : uint8_t { IDLE, RUNNING, RESULT_READY };

  static void _onCreateTaskHandler(void* jobManager);
  void _runTask();

  Print& _logger;
  OnExecuteJobCallback _executeJobCallback = nullptr;
  OnGetCurrentPhaseCallback _getCurrentPhaseCallback = nullptr;
  OnIdleCallback _idleCallback = nullptr;
  void* _manager = nullptr;
  TaskHandle_t _task = nullptr;
  std::atomic<Job> _currentJob{Job::NONE};
  std::atomic<State> _state{State::IDLE};
  unsigned long _jobStartedTimestampMs = 0;
};
