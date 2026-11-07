// Platform libraries.
#include <Arduino.h>     // To add IntelliSense for platform constants.

// Read configuration and secrets.
#include "config.h"

// Project classes.
#include "DisplayManager.h"
#include "KnobManager.h"

DisplayManager displayManager;
KnobManager knobManager;

void setupSerial();
void onKnobTurned(long value);
void onKnobClicked(unsigned long duration);

void setup() {
  Serial.begin(SERIAL_BAUD_RATE);
  Serial.println(F("setup: Initializing ESP32 Ace Pro Dryer..."));

  displayManager.setup();
  knobManager.setup(onKnobTurned, onKnobClicked);

  displayManager.printStatus("Ready.");

  Serial.println(F("setup: Initializing ESP32 Ace Pro Dryer DONE."));
}

void loop() {
  displayManager.updateSpinner();
}

void setupSerial() {

}

void onKnobTurned(long value) {
  Serial.print(F("Encoder value: "));
  Serial.println(value);
  displayManager.printCurrentTemperature(value);
}

void onKnobClicked(unsigned long duration) {
  Serial.print(F("Encoder button pressed for "));
  Serial.print(duration);
  Serial.println(F(" ms."));
  displayManager.printTargetTemperature(duration);
}