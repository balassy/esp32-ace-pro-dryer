#pragma once

#include <Arduino.h>

class AceProManager {
 public:
  struct Status {
    int currentTemperatureCelsius = 0;
    bool isDrying = false;
    int targetTemperatureCelsius = 0;
    unsigned int targetDurationMinutes = 0;
    unsigned int remainingTimeMinutes = 0;
    unsigned int fanSpeedRpm = 0;
  };

  void setup();
  Status getStatus() const;

 private:
  Status _status;
};
