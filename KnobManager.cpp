#include "KnobManager.h"

#include <Arduino.h>

#include "config.h"

KnobManager::KnobManager()
    : _encoder(ENCODER_B_DT_PIN, ENCODER_A_CLK_PIN, ENCODER_SW_PIN, ENCODER_VCC_PIN) {}

void KnobManager::setup(TurnCallback turnCallback, ButtonCallback buttonCallback) {
  Serial.println(F("KnobManager::setup: Initializing knob..."));

  _encoder.setEncoderType(EncoderType::FLOATING);
  _encoder.onTurned(turnCallback);
  _encoder.onPressed(buttonCallback);
  _encoder.setMinValue(ENCODER_MIN_VALUE);
  _encoder.setMaxValue(ENCODER_MAX_VALUE);
  _encoder.setStepValue(5);
  _encoder.setCircular(false);
  _encoder.begin();

  Serial.println(F("KnobManager::setup: Initializing knob DONE."));
}
