#pragma once

#include <Arduino.h>

class AceProManager {
 public:
  explicit AceProManager(Print& logger);

  struct Status {
    bool valid = false;  // Required status fields were parsed successfully.
    bool responseComplete = false;  // The full frame and CRC were validated.
    int currentTemperatureCelsius = 0;
    bool isDrying = false;
    int targetTemperatureCelsius = 0;
    unsigned int targetDurationMinutes = 0;
    unsigned int remainingTimeMinutes = 0;
    unsigned int fanSpeedRpm = 0;
  };

  bool setup();
  bool startDrying();
  bool isConnected() const;
  Status getStatus();

 private:
  Print& _logger;
  Status _status;
  bool _usbHostStarted = false;
};
