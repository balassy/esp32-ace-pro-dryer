#pragma once

#include <ESP32RotaryEncoder.h>

class KnobManager {
 public:
  using TurnCallback = void (*)(long);
  using ClickCallback = void (*)(unsigned long);

  KnobManager();

  void setup(TurnCallback turnCallback, ClickCallback clickCallback);
  void configure(long minValue, long maxValue, long stepValue, long initialValue);

 private:
  RotaryEncoder _encoder;
};
