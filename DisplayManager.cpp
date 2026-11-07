#include "DisplayManager.h"

#include <Arduino.h>
#include <Wire.h>

DisplayManager::DisplayManager()
    : _display(_screenWidth, _screenHeight, &Wire, -1) {}

bool DisplayManager::setup() {
  constexpr unsigned long setupTimeoutMs = 5000;
  constexpr unsigned long retryIntervalMs = 100;
  const unsigned long startTime = millis();

  Serial.println(F("DisplayManager::setup: Initializing display..."));

  while (!_display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    if (millis() - startTime >= setupTimeoutMs) {
      Serial.println(F("DisplayManager::setup: Display initialization FAILED (timeout)."));
      return false;
    }
    delay(retryIntervalMs);
  }

  Serial.println(F("DisplayManager::setup: Initializing display DONE."));
  return true;
}

void DisplayManager::printHelloWorld() {
  _display.clearDisplay();
  _display.setTextSize(1);
  _display.setTextColor(SSD1306_WHITE);
  _display.setCursor(0, 0);
  _display.println("Hello World!");
  _display.display();
}
