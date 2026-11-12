#pragma once

#include <Arduino.h>

class AceProManager {
 public:
  explicit AceProManager(Print& logger);

  struct Status {
    int currentTemperatureCelsius = 0;
    bool isDrying = false;
    int targetTemperatureCelsius = 0;
    unsigned int targetDurationMinutes = 0;
    unsigned int remainingTimeMinutes = 0;
    unsigned int fanSpeedRpm = 0;
  };

  void setup();
  void update();
  Status getStatus() const;

 private:
  Print& _logger;
  Status _status;
  bool _usbHostStarted = false;
};
