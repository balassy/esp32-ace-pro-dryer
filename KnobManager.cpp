#include "KnobManager.h"

#include <Arduino.h>

#include "config.h"

#define DONT_CIRCLE_VALUES false

KnobManager::KnobManager()
    : _encoder(ENCODER_B_DT_PIN, ENCODER_A_CLK_PIN, ENCODER_SW_PIN, ENCODER_VCC_PIN) {}

void KnobManager::setup(TurnCallback turnCallback, ClickCallback clickCallback) {
  Serial.println(F("KnobManager::setup: Initializing knob..."));

  _encoder.setEncoderType(EncoderType::FLOATING);
  _encoder.onTurned(turnCallback);
  _encoder.onPressed(clickCallback);
  _encoder.setMinValue(ENCODER_MIN_VALUE);
  _encoder.setMaxValue(ENCODER_MAX_VALUE);
  _encoder.setStepValue(5);
  _encoder.setCircular(DONT_CIRCLE_VALUES);
  _encoder.begin();

  Serial.println(F("KnobManager::setup: Initializing knob DONE."));
}

void KnobManager::configure(long minValue, long maxValue, long stepValue, long initialValue) {
  _encoder.setBoundaries(minValue, maxValue, DONT_CIRCLE_VALUES);
  _encoder.setStepValue(stepValue);
  _encoder.setEncoderValue(initialValue);
}
