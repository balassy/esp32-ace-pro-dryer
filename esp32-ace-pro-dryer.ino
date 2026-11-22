// Platform libraries.
#include <Arduino.h>     // To add IntelliSense for platform constants.

// Read configuration and secrets.
#include "config.h"

// Project classes.
#include "AboutPage.h"
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
void onMainAboutClicked();
void onMainStopClicked();
void onAboutBackClicked();
void onSettingsStartClicked();
void onSettingsBackClicked();
void activatePage(Page& page);
bool isAceProConnectedForRemoteLog();
bool requestDryerInfoForAboutPage();

DisplayManager displayManager;
KnobManager knobManager;
LedManager ledManager;
LogRouter logRouter(Serial);
RemoteLogger remoteLogger;
AceProManager aceProManager(logRouter);

MainPage mainPage(displayManager, knobManager, onMainSettingsClicked, onMainStopClicked,
                  onMainAboutClicked);
AboutPage aboutPage(displayManager, knobManager, onAboutBackClicked, requestDryerInfoForAboutPage);
SettingsPage settingsPage(displayManager, knobManager, onSettingsStartClicked, onSettingsBackClicked);
Page* activePage = nullptr;

bool aceDryingActive = false;
bool aceConnectionIndicatorState = false;

bool isAceProConnectedForRemoteLog() {
  return aceProManager.isConnected();
}

bool requestDryerInfoForAboutPage() {
  return aceProManager.requestDryerInfo();
}

void applyDryerStatus(const AceProManager::DryerStatus& dryerStatus) {
  logRouter.print(F("applyDryerStatus: Dryer status: current temp: "));
  logRouter.println(dryerStatus.currentTemperatureCelsius);

  if (!dryerStatus.valid || !dryerStatus.responseComplete) {
    displayManager.printStatus("Status unavailable");
  } else if (dryerStatus.isDrying) {
    aceDryingActive = true;
    mainPage.setDrying(true);
    displayManager.printStatus("Drying");
    displayManager.printCurrentTemperature(dryerStatus.currentTemperatureCelsius);
    ledManager.turnOnRed();
  } else {
    aceDryingActive = false;
    mainPage.setDrying(false);
    displayManager.printStatus("Ready");
    displayManager.printCurrentTemperature(dryerStatus.currentTemperatureCelsius);
    ledManager.turnOnGreen();
  }
}

// ------------------------------ Arduino handlers ------------------------------

void setup() {
  Serial.begin(SERIAL_BAUD_RATE);
  const unsigned long waitStart = millis();
  while (!Serial && millis() - waitStart < 5000) {
    delay(100);
  }
  logRouter.println(F("setup: Initializing ESP32 ACE Pro Dryer..."));

  ledManager.turnOnBlue();

  remoteLogger.setConnectionStatusCallback(isAceProConnectedForRemoteLog);
  if (remoteLogger.setup()) {
    logRouter.println(F("setup: Logging destination switching from Serial to remote logger."));
    logRouter.flush();
    logRouter.setOutput(remoteLogger);
    logRouter.println(F("setup: Logging destination is now the remote logger."));  
  } else {
    logRouter.println(F("setup: RemoteLogger::setup: failed; check the Wi-Fi settings in config.h."));
  }

  displayManager.setup();
  knobManager.setup(onKnobTurned, onKnobClicked);
  
  activatePage(mainPage);

  if(aceProManager.setup()) {
    displayManager.printStatus("Ready.");

    aceConnectionIndicatorState = aceProManager.isConnected();
    displayManager.setAceProConnected(aceConnectionIndicatorState);
  } else {
    displayManager.printStatus("ACE not found!");
  }

  ledManager.turnOnGreen();
  logRouter.println(F("setup: Initializing ESP32 ACE Pro Dryer DONE."));
  logRouter.println();
  logRouter.flush();
}

void loop() {
  displayManager.updateSpinner();
  remoteLogger.update();
  const bool aceConnected = aceProManager.isConnected();
  if (aceConnected != aceConnectionIndicatorState) {
    aceConnectionIndicatorState = aceConnected;
    displayManager.setAceProConnected(aceConnected);
  }

  AceProManager::DryerInfo dryerInfo;
  if (aceProManager.takeDryerInfo(dryerInfo)) {
    logRouter.print(F("loop: Dryer info: model "));
    logRouter.print(dryerInfo.model);
    logRouter.print(F(", firmware "));
    logRouter.print(dryerInfo.firmware);
    logRouter.print(F(", boot firmware "));
    logRouter.println(dryerInfo.bootFirmware);
    if (activePage == &aboutPage) {
      aboutPage.showDryerInfo(dryerInfo.model, dryerInfo.firmware, dryerInfo.bootFirmware);
    }
  }

  AceProManager::DryerStatus dryerStatus;
  if (aceProManager.takeDryerStatus(dryerStatus)) {
    applyDryerStatus(dryerStatus);
  }
  bool dryingStarted = false;
  if (aceProManager.takeStart(dryingStarted)) {
    if (dryingStarted) {
      aceDryingActive = true;
      mainPage.setDrying(true);
      displayManager.printStatus("Drying");
      ledManager.turnOnRed();
    } else {
      displayManager.printStatus("Start unconfirmed");
    }
  }
  bool dryingStopped = false;
  if (aceProManager.takeStop(dryingStopped)) {
    if (dryingStopped) {
      aceDryingActive = false;
      mainPage.setDrying(false);
      displayManager.printStatus("Drying stopped");
      ledManager.turnOnGreen();
    } else {
      displayManager.printStatus("Stop unconfirmed");
    }
  }

  static unsigned long lastDryerStatusRequestMs = 0;
  if (activePage == &mainPage && !aceProManager.isCommandRunning() &&
      millis() - lastDryerStatusRequestMs >= 10000) {
    if (aceProManager.requestDryerStatus()) {
      lastDryerStatusRequestMs = millis();
    }
  }

  static unsigned long lastCommandLogMs = 0;
  if (aceProManager.isCommandRunning() && millis() - lastCommandLogMs >= 1000) {
    lastCommandLogMs = millis();
    logRouter.print(F("loop: ACE command still running, phase "));
    logRouter.print(aceProManager.commandPhase());
    logRouter.print(F(", elapsed "));
    logRouter.print(aceProManager.commandElapsedMs());
    logRouter.println(F(" ms."));
  }

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
  if (activePage != nullptr) {
    activePage->deactivate();
  }
  activePage = &page;
  activePage->activate();
}

void onMainSettingsClicked() {
  mainPage.deactivate();
  activatePage(settingsPage);
}

void onMainAboutClicked() {
  activatePage(aboutPage);
}

void onAboutBackClicked() {
  aboutPage.deactivate();
  activatePage(mainPage);
  mainPage.setDrying(aceDryingActive);
  if (aceDryingActive) {
    displayManager.printStatus("Drying");
    ledManager.turnOnRed();
  } else {
    displayManager.printStatus("Ready.");
    ledManager.turnOnGreen();
  }
}

void onMainStopClicked() {
  logRouter.println(F("Stopping dryer..."));
  if (!aceProManager.requestStop()) {
    displayManager.printStatus("Busy");
    return;
  }
  displayManager.printStatus("Stopping...");
}

void onSettingsStartClicked() {
  settingsPage.deactivate();
  activatePage(mainPage);
  if (!aceProManager.requestStart()) {
    displayManager.printStatus("Busy");
    return;
  }
  displayManager.printStatus("Checking ACE Pro...");
}

void onSettingsBackClicked() {
  settingsPage.deactivate();
  activatePage(mainPage);
  mainPage.setDrying(aceDryingActive);
  displayManager.printStatus("Ready.");
  if (aceDryingActive) {
    ledManager.turnOnRed();
  } else {
    ledManager.turnOnGreen();
  }
}
