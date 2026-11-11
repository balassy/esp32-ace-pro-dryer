#include "LedManager.h"

#include <Arduino.h>

#include "config.h"

void LedManager::turnOn() {
  rgbLedWrite(RGB_BUILTIN, RGB_LED_BRIGHTNESS, RGB_LED_BRIGHTNESS, RGB_LED_BRIGHTNESS);
}

void LedManager::turnOnRed() {
  rgbLedWrite(RGB_BUILTIN, RGB_LED_BRIGHTNESS, 0, 0);
}

void LedManager::turnOnGreen() {
  rgbLedWrite(RGB_BUILTIN, 0, RGB_LED_BRIGHTNESS, 0);
}

void LedManager::turnOnBlue() {
  rgbLedWrite(RGB_BUILTIN, 0, 0, RGB_LED_BRIGHTNESS);
}

void LedManager::turnOff() {
  rgbLedWrite(RGB_BUILTIN, 0, 0, 0);
}
