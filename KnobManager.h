#pragma once

#include <ESP32RotaryEncoder.h>

class KnobManager {
 public:
  using TurnCallback = void (*)(long);
  using ButtonCallback = void (*)(unsigned long);

  KnobManager();

  void setup(TurnCallback turnCallback, ButtonCallback buttonCallback);

 private:
  RotaryEncoder _encoder;
};
