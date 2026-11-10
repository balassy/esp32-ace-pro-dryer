#pragma once

#include <Arduino.h>

class LedManager {
 public:
  void turnOn();
  void turnOnRed();
  void turnOnGreen();
  void turnOnBlue();
  void turnOff();
};
