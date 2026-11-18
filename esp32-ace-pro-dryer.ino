// Platform libraries.
#include <Arduino.h>     // To add IntelliSense for platform constants.

// Read configuration and secrets.
#include "config.h"

// Project classes.
#include "AceProManager.h"
#include "DisplayManager.h"
#include "KnobManager.h"
#include "LedManager.h"
#include "LogRouter.h"
#include "MainPage.h"
#include "Page.h"
#include "RemoteLogger.h"
#include "SettingsPage.h"

void onKnobTurned(long value);
void onKnobClicked(unsigned long duration);
void onMainSettingsClicked();
void onMainStopClicked();
void onSettingsStartClicked();
void onSettingsBackClicked();
void activatePage(Page& page);

DisplayManager displayManager;
KnobManager knobManager;
LedManager ledManager;
LogRouter logRouter(Serial);
RemoteLogger remoteLogger;
AceProManager aceProManager(logRouter);

MainPage mainPage(displayManager, knobManager, onMainSettingsClicked, onMainStopClicked);
SettingsPage settingsPage(displayManager, knobManager, onSettingsStartClicked, onSettingsBackClicked);
Page* activePage = nullptr;

bool aceDryingActive = false;

// ------------------------------ Arduino handlers ------------------------------

void setup() {
  Serial.begin(SERIAL_BAUD_RATE);
  const unsigned long waitStart = millis();
  while (!Serial && millis() - waitStart < 5000) {
    delay(100);
  }
  logRouter.println(F("setup: Initializing ESP32 ACE Pro Dryer..."));

  ledManager.turnOnBlue();

  if (!remoteLogger.setup()) {
    logRouter.println(F("RemoteLogger::setup: failed; check the Wi-Fi SSID in config.h."));
  }

  displayManager.setup();
  knobManager.setup(onKnobTurned, onKnobClicked);

  activatePage(mainPage);
  displayManager.printStatus("Ready.");

  ledManager.turnOnGreen();
  logRouter.println(F("setup: Initializing ESP32 ACE Pro Dryer DONE."));
  logRouter.flush();
}

void loop() {
  remoteLogger.update();
  displayManager.updateSpinner();
  if (activePage != nullptr) {
    activePage->loop();
  }
}

// ------------------------------ Knob event handlers --------------------------

void onKnobTurned(long value) {
  logRouter.print(F("Encoder value: "));
  logRouter.println(value);
  if (activePage != nullptr) {
    activePage->onKnobTurned(value);
  }
}

void onKnobClicked(unsigned long duration) {
  logRouter.print(F("Encoder button clicked for "));
  logRouter.print(duration);
  logRouter.println(F(" ms."));
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

void onMainStopClicked() {
  logRouter.println(F("Stopping dryer..."));
  if (aceProManager.stopDrying()) {
    aceDryingActive = false;
    mainPage.setStopButtonVisible(false);
    displayManager.printStatus("Drying stopped");
    ledManager.turnOnGreen();
  } else {
    displayManager.printStatus("Stop unconfirmed");
  }
}

void onSettingsStartClicked() {
  activatePage(mainPage);
  displayManager.printStatus("Heating...");
  ledManager.turnOnRed();
  Serial.println(F("Logging destination switching from Serial to remote logger."));
  Serial.flush();
  logRouter.setOutput(remoteLogger);
  logRouter.println(F("Logging destination is now the remote logger."));
  logRouter.println(F("Starting dryer..."));
  if (!aceProManager.setup()) {
    displayManager.printStatus("ACE connection failed");
    ledManager.turnOnBlue();
    return;
  }

  const bool startAcknowledged = aceProManager.startDrying();
  if (startAcknowledged) {
    aceDryingActive = true;
    mainPage.setStopButtonVisible(true);
    displayManager.printStatus("Drying...");
  } else {
    displayManager.printStatus("Start unconfirmed");
  }
}

void onSettingsBackClicked() {
  activatePage(mainPage);
  mainPage.setStopButtonVisible(aceDryingActive);
  displayManager.printStatus("Ready.");
  ledManager.turnOnGreen();
}
