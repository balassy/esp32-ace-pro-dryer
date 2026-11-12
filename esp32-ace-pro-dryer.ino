// Platform libraries.
#include <Arduino.h>     // To add IntelliSense for platform constants.

// Read configuration and secrets.
#include "config.h"

// Project classes.
#include "AceProManager.h"
#include "DisplayManager.h"
#include "KnobManager.h"
#include "LedManager.h"
#include "MainPage.h"
#include "Page.h"
#include "SettingsPage.h"

void onKnobTurned(long value);
void onKnobClicked(unsigned long duration);
void onMainSettingsClicked();
void onSettingsStartClicked();
void onSettingsBackClicked();
void activatePage(Page& page);

DisplayManager displayManager;
KnobManager knobManager;
LedManager ledManager;
AceProManager aceProManager;

MainPage mainPage(displayManager, knobManager, onMainSettingsClicked);
SettingsPage settingsPage(displayManager, knobManager, onSettingsStartClicked, onSettingsBackClicked);
Page* activePage = nullptr;

// ------------------------------ Arduino handlers ------------------------------

void setup() {
  Serial.begin(SERIAL_BAUD_RATE);
  const unsigned long waitStart = millis();
  while (!Serial && millis() - waitStart < 5000) {
    delay(100);
  }
  Serial.println(F("setup: Initializing ESP32 ACE Pro Dryer..."));

  ledManager.turnOnBlue();

  displayManager.setup();
  knobManager.setup(onKnobTurned, onKnobClicked);

  activatePage(mainPage);
  displayManager.printStatus("Ready.");

  ledManager.turnOnGreen();
  Serial.println(F("setup: Initializing ESP32 ACE Pro Dryer DONE."));
  Serial.flush();
}

void loop() {
  //aceProManager.update();
  displayManager.updateSpinner();
  if (activePage != nullptr) {
    activePage->loop();
  }
}

// ------------------------------ Knob event handlers --------------------------

void onKnobTurned(long value) {
  Serial.print(F("Encoder value: "));
  Serial.println(value);
  if (activePage != nullptr) {
    activePage->onKnobTurned(value);
  }
}

void onKnobClicked(unsigned long duration) {
  Serial.print(F("Encoder button clicked for "));
  Serial.print(duration);
  Serial.println(F(" ms."));
  if (activePage != nullptr) {
    activePage->onKnobClicked();
  }
}

// ------------------------------ Page navigation handlers -----------------------

void activatePage(Page& page) {
  activePage = &page;
  activePage->activate();
}

void onMainSettingsClicked() {
  activatePage(settingsPage);
}

void onSettingsStartClicked() {
  activatePage(mainPage);
  displayManager.printStatus("Heating...");
  ledManager.turnOnRed();
    aceProManager.setup();
}

void onSettingsBackClicked() {
  activatePage(mainPage);
  displayManager.printStatus("Ready.");
  ledManager.turnOnGreen();
}
