#pragma once

#include <Arduino.h>

// USB CDC transport and ACE Pro frame protocol.
// Command methods do not reconnect. When the ACE Pro is not connected they
// return Error::NOT_CONNECTED and leave the link unchanged.
class AceProClient {
 public:
  enum class Error : uint8_t {
    OK = 0,
    NOT_CONNECTED,
    HOST_FAILED,
    TIMEOUT,
    INVALID_RESPONSE,
    REJECTED,
    FAILED
  };

  struct DryerStatus {
    bool valid = false;             // Required dryer status fields were parsed successfully.
    bool responseComplete = false;  // The full frame and CRC were validated.
    int currentTemperatureCelsius = 0;
    bool isDrying = false;
    int targetTemperatureCelsius = 0;
    unsigned int targetDurationMinutes = 0;
    unsigned int remainingTimeSeconds = 0;
    unsigned int fanSpeedRpm = 0;
  };

  struct DryerInfo {
    char model[64] = "";
    char firmware[32] = "";
    char bootFirmware[32] = "";
  };

  explicit AceProClient(Print& logger);

  // Starts the USB host if needed. When the host is already running and the
  // ACE Pro is absent, reopens the CDC port, then waits for enumeration.
  Error connect(unsigned long timeoutMs);

  // Closes and reopens the CDC port so the next command does not reuse a halted pipe.
  Error reopen(unsigned long timeoutMs);

  // Stops the USB host so an idle dryer cannot leave the host task stuck.
  void release();

  unsigned long millisecondsSinceActivity() const;
  bool isConnected() const;
  const char* getCurrentPhase() const;
  unsigned long getPhaseElapsedMs() const;

  Error getDryerInfo(DryerInfo& dryerInfo);
  Error getDryerStatus(DryerStatus& dryerStatus);
  Error startDrying();
  Error stopDrying();

 private:
  Print& _logger;
};
