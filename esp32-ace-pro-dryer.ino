// Platform libraries.
#include <Arduino.h>     // To add IntelliSense for platform constants.

// Read configuration and secrets.
#include "config.h"

// Project classes.
#include "DisplayManager.h"

DisplayManager displayManager;

void setupSerial();

void setup() {
  setupSerial();
  setupDisplay();
}

void loop() {}

void setupSerial() {
  Serial.begin(SERIAL_BAUD_RATE);
  Serial.println();
  Serial.println(F("setupSerial: Initializing serial connection DONE."));
}

void setupDisplay() {
  if (!displayManager.setup()) {
    for (;;) {
    }
  }
  displayManager.printHelloWorld();
}
