// Platform libraries.
#include <Arduino.h>     // To add IntelliSense for platform constants.

// Read configuration and secrets.
#include "config.h"

// Project classes.
#include "AboutPage.h"
#include "AceProManager.h"
#include "CommunicationIndicator.h"
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
void onAboutBackClicked();
void onSettingsStartClicked();
void onSettingsBackClicked();
void activatePage(Page& page);

DisplayManager displayManager;
KnobManager knobManager;
LedManager ledManager;
LogRouter logRouter(Serial);
RemoteLogger remoteLogger;
AceProManager aceProManager(logRouter);

MainPage mainPage(displayManager, knobManager, aceProManager, ledManager, onMainSettingsClicked, onMainAboutClicked);
AboutPage aboutPage(displayManager, knobManager, aceProManager, onAboutBackClicked);
SettingsPage settingsPage(displayManager, knobManager, aceProManager, onSettingsStartClicked, onSettingsBackClicked);

CommunicationIndicator communicationIndicator(displayManager, aceProManager);

Page* activePage = nullptr;

// ------------------------------ Arduino handlers ------------------------------

void setup() {
  Serial.begin(SERIAL_BAUD_RATE);
  const unsigned long waitStart = millis();
  while (!Serial && millis() - waitStart < 5000) {
    delay(100);
  }
  logRouter.println(F("setup: Initializing ESP32 ACE Pro Dryer..."));

  ledManager.turnOnBlue();

  if (remoteLogger.setup()) {
    logRouter.println(F("setup: Logging destination switching from Serial to remote logger."));
    logRouter.flush();
    logRouter.setOutput(remoteLogger);
    logRouter.println(F("setup: Logging destination is now the remote logger."));  
  } else {
    logRouter.println(F("setup: RemoteLogger::setup: failed; check the Wi-Fi settings in config.h."));
  }

  displayManager.setup();
  displayManager.printBootMessage();

  knobManager.setup(onKnobTurned, onKnobClicked);
  
  if(aceProManager.setup()) {
    activatePage(mainPage);
    displayManager.printInfoMessage("Connecting...");
    
    ledManager.turnOnGreen();

    communicationIndicator.setup();
  } else {
    displayManager.printInfoMessage("ACE not found!");
    ledManager.turnOnRed();
  }

  logRouter.println(F("setup: Initializing ESP32 ACE Pro Dryer DONE."));
  logRouter.println();
  logRouter.flush();
}

void loop() {
  displayManager.updateSpinner();
  remoteLogger.update();

  communicationIndicator.update();

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
  logRouter.print(F("onKnobTurned: Encoder value: "));
  logRouter.println(value);
  if (activePage != nullptr) {
    activePage->onKnobTurned(value);
  }
}

void onKnobClicked(unsigned long duration) {
  logRouter.print(F("onKnobClicked: Encoder button clicked for "));
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
  activatePage(settingsPage);
}

void onMainAboutClicked() {
  activatePage(aboutPage);
}

void onAboutBackClicked() {
  activatePage(mainPage);
}

void onSettingsStartClicked() {
  activatePage(mainPage);
}

void onSettingsBackClicked() {
  activatePage(mainPage);
}
