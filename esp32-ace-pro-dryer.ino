// Platform libraries.
#include <Arduino.h>     // To add IntelliSense for platform constants.

// Read configuration and secrets.
#include "config.h"

// Project classes.
#include "DisplayManager.h"
#include "KnobManager.h"
#include "Menu.h"

void setupSerial();
void onKnobTurned(long value);
void onKnobClicked(unsigned long duration);
void onPrimaryButtonClicked();
void onSecondaryButtonClicked();

DisplayManager displayManager;
KnobManager knobManager;
Menu menu(displayManager, knobManager, "Start", onPrimaryButtonClicked,
          "Back", onSecondaryButtonClicked);

void setup() {
  Serial.begin(SERIAL_BAUD_RATE);
  Serial.println(F("setup: Initializing ESP32 Ace Pro Dryer..."));

  displayManager.setup();
  knobManager.setup(onKnobTurned, onKnobClicked);

  displayManager.printStatus("Ready.");
  menu.activate();

  Serial.println(F("setup: Initializing ESP32 Ace Pro Dryer DONE."));
}

void loop() {
  displayManager.updateSpinner();
  menu.loop();
}

void onKnobTurned(long value) {
  Serial.print(F("Encoder value: "));
  Serial.println(value);
  menu.onKnobTurned(value);
}

void onKnobClicked(unsigned long duration) {
  Serial.print(F("Encoder button clicked for "));
  Serial.print(duration);
  Serial.println(F(" ms."));
  menu.onKnobClicked();
}

void onPrimaryButtonClicked() {
  Serial.println(F("Primary button clicked."));
}

void onSecondaryButtonClicked() {
  Serial.println(F("Secondary button clicked."));
}