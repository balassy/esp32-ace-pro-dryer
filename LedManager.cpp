#include "LedManager.h"

#include <Arduino.h>

#include "config.h"

void LedManager::turnOn() {
#ifdef RGB_BUILTIN
  rgbLedWrite(RGB_BUILTIN, RGB_LED_BRIGHTNESS, RGB_LED_BRIGHTNESS, RGB_LED_BRIGHTNESS);
#endif
}

void LedManager::turnOnRed() {
#ifdef RGB_BUILTIN
  rgbLedWrite(RGB_BUILTIN, RGB_LED_BRIGHTNESS, 0, 0);
#endif
}

void LedManager::turnOnGreen() {
#ifdef RGB_BUILTIN
  rgbLedWrite(RGB_BUILTIN, 0, RGB_LED_BRIGHTNESS, 0);
#endif
}

void LedManager::turnOnBlue() {
#ifdef RGB_BUILTIN
  rgbLedWrite(RGB_BUILTIN, 0, 0, RGB_LED_BRIGHTNESS);
#endif
}

void LedManager::turnOff() {
#ifdef RGB_BUILTIN
  rgbLedWrite(RGB_BUILTIN, 0, 0, 0);
#endif
}
