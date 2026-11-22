#include "AceProManager.h"

namespace {

constexpr unsigned long USB_STARTUP_WAIT_MS = 3000;
constexpr unsigned long USB_RECONNECT_TIMEOUT_MS = 3000;
constexpr unsigned long USB_IDLE_RELEASE_MS = 1000;

template <typename Command>
AceProClient::Error callWithReconnect(AceProClient& client, Print& logger, Command command) {
  if (!client.isConnected()) {
    logger.println(F("AceProManager::callWithReconnect: ACE Pro link is down. Restarting USB before the command."));

    if (client.reopen(USB_RECONNECT_TIMEOUT_MS) != AceProClient::Error::OK) {
      logger.println(F("AceProManager::callWithReconnect: ERROR: Failed to reopen the ACE Pro link."));
      client.release();
      return AceProClient::Error::NOT_CONNECTED;
    }
  } else {
    logger.println(F("AceProManager::callWithReconnect: ACE Pro link is up. Sending on the current CDC port."));
  }

  logger.println(F("AceProManager::callWithReconnect: Sending the command."));
  AceProClient::Error result = command();
  if (result == AceProClient::Error::NOT_CONNECTED || result == AceProClient::Error::TIMEOUT) {
    logger.println(F("AceProManager::callWithReconnect: ERROR: Command did not complete because timeout or disconnection. Restarting USB and resending once."));
    if (client.reopen(USB_RECONNECT_TIMEOUT_MS) != AceProClient::Error::OK) {
      logger.println(F("AceProManager::callWithReconnect: ERROR: Failed to rebuild the ACE Pro connection."));
      client.release();
      return result;
    }
    result = command();
  }
  // The dryer drops USB after a few idle seconds and the host task then blocks
  // inside the next write. Drop the host while it is still healthy.
  logger.println(F("AceProManager::callWithReconnect: Releasing the USB host after the command."));
  client.release();
  return result;
}

}  // namespace

AceProManager::AceProManager(Print& logger) : _logger(logger), _client(logger), _jobManager(logger) {}

bool AceProManager::setup() {
  _logger.println(F("AceProManager::setup: Initializing ACE Pro connection..."));
  if (!_jobManager.setup(&AceProManager::_onExecuteJob, &AceProManager::_onGetCurrentPhase,
                         &AceProManager::_onIdle, this)) {
    return false;
  }

  const AceProClient::Error result = _client.connect(USB_STARTUP_WAIT_MS);
  if (result == AceProClient::Error::HOST_FAILED) {
    _logger.println(F("AceProManager::setup: ERROR: Failed to start the ACE Pro USB host."));
    return false;
  }

  if (result == AceProClient::Error::OK) {
    _logger.println(F("AceProManager::setup: ACE Pro is connected."));
  } else {
    _logger.println(F("AceProManager::setup: ACE Pro is not connected. It can be powered on later."));
  }

  _logger.println(F("AceProManager::setup: Initializing ACE Pro connection DONE."));
  return true;
}

void AceProManager::_onExecuteJob(AceProJobManager::Job job, void* manager) {
  static_cast<AceProManager*>(manager)->_executeJob(job);
}

const char* AceProManager::_onGetCurrentPhase(void* manager) {
  return static_cast<AceProManager*>(manager)->_client.getCurrentPhase();
}

void AceProManager::_onIdle(void* manager) {
  static_cast<AceProManager*>(manager)->_releaseIdleLink();
}

void AceProManager::_releaseIdleLink() {
  if (_jobManager.isRunning() || !_client.isConnected()) {
    return;
  }
  if (_client.millisecondsSinceActivity() < USB_IDLE_RELEASE_MS) {
    return;
  }
  _logger.println(F("AceProManager: ACE Pro link is idle. Releasing the USB host."));
  _client.release();
}

void AceProManager::_executeJob(AceProJobManager::Job job) {
  switch (job) {
    case AceProJobManager::Job::NONE:
      _logger.println(F("AceProManager::_executeJob: No job requested."));
      return;
    case AceProJobManager::Job::GET_DRYER_INFO:
      _logger.println(F("AceProManager::_executeJob: Requesting dryer info..."));
      _dryerInfo = _runDryerInfo();
      break;
    case AceProJobManager::Job::DRYER_STATUS:
      _logger.println(F("AceProManager::_executeJob: Requesting dryer status..."));
      _dryerStatus = _runDryerStatus();
      break;
    case AceProJobManager::Job::START_DRYING:
      _logger.println(F("AceProManager::_executeJob: Requesting start drying..."));
      _startResult = _runStart();
      break;
    case AceProJobManager::Job::STOP_DRYING:
      _logger.println(F("AceProManager::_executeJob: Requesting stop drying..."));
      _stopResult = _runStop();
      break;
  }
}

AceProManager::DryerInfo AceProManager::_runDryerInfo() {
  DryerInfo dryerInfo;
  const AceProClient::Error result = callWithReconnect(
      _client, _logger, [this, &dryerInfo]() { return _client.getDryerInfo(dryerInfo); });

  if (result == AceProClient::Error::NOT_CONNECTED) {
    _logger.println(F("AceProManager::_runDryerInfo: ERROR: ACE Pro is not connected."));
    return DryerInfo{};
  }

  if (result != AceProClient::Error::OK) {
    _logger.print(F("AceProManager::_runDryerInfo: ERROR: Request failed, phase "));
    _logger.print(_client.getCurrentPhase());
    _logger.print(F(", phase elapsed "));
    _logger.print(_client.getPhaseElapsedMs());
    _logger.println(F(" ms."));
    return DryerInfo{};
  }

  _logger.print(F("AceProManager::_runDryerInfo: Model: "));
  _logger.println(dryerInfo.model);
  _logger.print(F("AceProManager::_runDryerInfo: Firmware: "));
  _logger.println(dryerInfo.firmware);
  _logger.print(F("AceProManager::_runDryerInfo: Boot firmware: "));
  _logger.println(dryerInfo.bootFirmware);
  return dryerInfo;
}

AceProManager::DryerStatus AceProManager::_runDryerStatus() {
  DryerStatus dryerStatus;
  const AceProClient::Error result = callWithReconnect(
      _client, _logger, [this, &dryerStatus]() { return _client.getDryerStatus(dryerStatus); });

  if (result == AceProClient::Error::NOT_CONNECTED) {
    _logger.println(F("AceProManager::_runDryerStatus: ERROR: ACE Pro is not connected."));
    return DryerStatus{};
  }

  if (result != AceProClient::Error::OK) {
    _logger.print(F("AceProManager::_runDryerStatus: ERROR: Request failed, phase "));
    _logger.print(_client.getCurrentPhase());
    _logger.print(F(", phase elapsed "));
    _logger.print(_client.getPhaseElapsedMs());
    _logger.println(F(" ms."));
    return DryerStatus{};
  }

  _logger.print(F("AceProManager::_runDryerStatus: Current temperature: "));
  _logger.print(dryerStatus.currentTemperatureCelsius);
  _logger.println(F(" C"));
  _logger.print(F("AceProManager::_runDryerStatus: Dryer status: "));
  _logger.println(dryerStatus.isDrying ? F("drying") : F("stop"));
  _logger.print(F("AceProManager::_runDryerStatus: Target temperature: "));
  _logger.print(dryerStatus.targetTemperatureCelsius);
  _logger.println(F(" C"));
  _logger.print(F("AceProManager::_runDryerStatus: Duration: "));
  _logger.print(dryerStatus.targetDurationMinutes);
  _logger.println(F(" minutes"));
  _logger.print(F("AceProManager::_runDryerStatus: Remaining time: "));
  _logger.print(dryerStatus.remainingTimeSeconds);
  _logger.println(F(" seconds"));
  _logger.print(F("AceProManager::_runDryerStatus: Fan speed: "));
  _logger.print(dryerStatus.fanSpeedRpm);
  _logger.println(F(" RPM"));
  return dryerStatus;
}

bool AceProManager::_runStart() {
  const AceProClient::Error result =
      callWithReconnect(_client, _logger, [this]() { return _client.startDrying(); });
  if (result != AceProClient::Error::OK) {
    _logger.print(F("AceProManager::_runStart: ERROR: Request failed, phase "));
    _logger.print(_client.getCurrentPhase());
    _logger.print(F(", phase elapsed "));
    _logger.print(_client.getPhaseElapsedMs());
    _logger.println(F(" ms."));
    return false;
  }
  return true;
}

bool AceProManager::_runStop() {
  const AceProClient::Error result =
      callWithReconnect(_client, _logger, [this]() { return _client.stopDrying(); });
  if (result != AceProClient::Error::OK) {
    _logger.print(F("AceProManager::_runStop: ERROR: Request failed, phase "));
    _logger.print(_client.getCurrentPhase());
    _logger.print(F(", phase elapsed "));
    _logger.print(_client.getPhaseElapsedMs());
    _logger.println(F(" ms."));
    return false;
  }
  return true;
}



bool AceProManager::isConnected() const {
  return _client.isConnected();
}

bool AceProManager::startDrying() {
  const AceProClient::Error result =
      callWithReconnect(_client, _logger, [this]() { return _client.startDrying(); });
  return result == AceProClient::Error::OK;
}

bool AceProManager::stopDrying() {
  return _runStop();
}

// AceProManager::DryerStatus AceProManager::getDryerStatus() {
//   return _runDryerStatus();
// }

bool AceProManager::requestDryerInfo() {
  return _jobManager.request(AceProJobManager::Job::GET_DRYER_INFO);
}

bool AceProManager::takeDryerInfo(DryerInfo& dryerInfo) {
  if (!_jobManager.take(AceProJobManager::Job::GET_DRYER_INFO)) {
    return false;
  }
  dryerInfo = _dryerInfo;
  return true;
}

bool AceProManager::requestDryerStatus() {
  return _jobManager.request(AceProJobManager::Job::DRYER_STATUS);
}

bool AceProManager::takeDryerStatus(DryerStatus& dryerStatus) {
  if (!_jobManager.take(AceProJobManager::Job::DRYER_STATUS)) {
    return false;
  }
  dryerStatus = _dryerStatus;
  return true;
}

bool AceProManager::requestStart() {
  return _jobManager.request(AceProJobManager::Job::START_DRYING);
}

bool AceProManager::takeStart(bool& started) {
  if (!_jobManager.take(AceProJobManager::Job::START_DRYING)) {
    return false;
  }
  started = _startResult;
  return true;
}

bool AceProManager::requestStop() {
  return _jobManager.request(AceProJobManager::Job::STOP_DRYING);
}

bool AceProManager::takeStop(bool& stopped) {
  if (!_jobManager.take(AceProJobManager::Job::STOP_DRYING)) {
    return false;
  }
  stopped = _stopResult;
  return true;
}

bool AceProManager::isCommandRunning() const {
  return _jobManager.isRunning();
}

const char* AceProManager::commandPhase() const {
  return _client.getCurrentPhase();
}

unsigned long AceProManager::commandElapsedMs() const {
  return _client.getPhaseElapsedMs();
}
