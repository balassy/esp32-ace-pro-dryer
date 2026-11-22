#pragma once

#include <Arduino.h>

#include "AceProClient.h"
#include "AceProJobManager.h"

class AceProManager {
 public:
  explicit AceProManager(Print& logger);

  using DryerStatus = AceProClient::DryerStatus;
  using DryerInfo = AceProClient::DryerInfo;

  bool setup();
  bool startDrying();
  bool stopDrying();
  bool isConnected() const;

  // Runs the exchange off the main loop so the display keeps updating.
  bool requestDryerInfo();
  bool takeDryerInfo(DryerInfo& dryerInfo);

  bool requestDryerStatus();
  bool takeDryerStatus(DryerStatus& dryerStatus);

  bool requestStart();
  bool takeStart(bool& started);

  bool requestStop();
  bool takeStop(bool& stopped);
  bool isCommandRunning() const;
  const char* commandPhase() const;
  unsigned long commandElapsedMs() const;

 private:
  static void _onExecuteJob(AceProJobManager::Job job, void* manager);
  static const char* _onGetCurrentPhase(void* manager);
  static void _onIdle(void* manager);

  void _releaseIdleLink();

  void _executeJob(AceProJobManager::Job job);
  DryerInfo _runDryerInfo();
  DryerStatus _runDryerStatus();
  bool _runStart();
  bool _runStop();

  Print& _logger;
  AceProClient _client;
  AceProJobManager _jobManager;

  DryerInfo _dryerInfo{};
  DryerStatus _dryerStatus{};
  bool _stopResult = false;
  bool _startResult = false;
};
