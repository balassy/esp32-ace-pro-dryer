// Platform libraries.
#include <Arduino.h>     // To add IntelliSense for platform constants.
#include <atomic>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

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
bool isAceProConnectedForRemoteLog();

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
bool aceConnectionIndicatorState = false;
enum class AceStatusTaskState : uint8_t {
  IDLE,
  RUNNING,
  COMPLETE
};
AceProManager::Status aceStatusResult;
std::atomic<AceStatusTaskState> aceStatusTaskState{AceStatusTaskState::IDLE};

bool isAceProConnectedForRemoteLog() {
  return aceProManager.isConnected();
}

void aceStatusRequestTask(void*) {
  aceStatusResult = aceProManager.getStatus();
  aceStatusTaskState.store(AceStatusTaskState::COMPLETE, std::memory_order_release);
  vTaskDelete(nullptr);
}

void applyAceStatus(const AceProManager::Status& status) {
  if (!status.valid || !status.responseComplete) {
    displayManager.printStatus("Status unavailable");
  } else if (status.isDrying) {
    aceDryingActive = true;
    mainPage.setStopButtonVisible(true);
    displayManager.printStatus("Drying");
  } else {
    aceDryingActive = false;
    mainPage.setStopButtonVisible(false);
    displayManager.printStatus("Ready");
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
  if (!remoteLogger.setup()) {
    logRouter.println(F("RemoteLogger::setup: failed; check the Wi-Fi SSID in config.h."));
  }

  displayManager.setup();
  knobManager.setup(onKnobTurned, onKnobClicked);

  activatePage(mainPage);
  displayManager.printStatus("Ready.");

  aceProManager.setup();
  aceConnectionIndicatorState = aceProManager.isConnected();
  displayManager.setAceProConnected(aceConnectionIndicatorState);

  ledManager.turnOnGreen();
  logRouter.println(F("setup: Initializing ESP32 ACE Pro Dryer DONE."));
  logRouter.flush();
}

void loop() {
  remoteLogger.update();
  displayManager.updateSpinner();
  const bool aceConnected = aceProManager.isConnected();
  if (aceConnected != aceConnectionIndicatorState) {
    aceConnectionIndicatorState = aceConnected;
    displayManager.setAceProConnected(aceConnected);
  }
  if (aceStatusTaskState.load(std::memory_order_acquire) == AceStatusTaskState::COMPLETE) {
    applyAceStatus(aceStatusResult);
    aceStatusTaskState.store(AceStatusTaskState::IDLE, std::memory_order_release);
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
  Serial.println(F("Logging destination switching from Serial to remote logger."));
  Serial.flush();
  logRouter.setOutput(remoteLogger);
  logRouter.println(F("Logging destination is now the remote logger."));
  logRouter.println(F("Requesting ACE Pro status..."));

  AceStatusTaskState expectedState = AceStatusTaskState::IDLE;
  if (!aceStatusTaskState.compare_exchange_strong(
          expectedState, AceStatusTaskState::RUNNING, std::memory_order_acq_rel)) {
    displayManager.printStatus("Status request busy");
    return;
  }

  if (xTaskCreate(aceStatusRequestTask, "aceStatusReq", 8192, nullptr, 1, nullptr) != pdPASS) {
    aceStatusTaskState.store(AceStatusTaskState::IDLE, std::memory_order_release);
    logRouter.println(F("ERROR: Failed to start the ACE status request task."));
    displayManager.printStatus("Status unavailable");
    return;
  }
  displayManager.printStatus("Checking ACE Pro...");
}

void onSettingsBackClicked() {
  activatePage(mainPage);
  mainPage.setStopButtonVisible(aceDryingActive);
  displayManager.printStatus("Ready.");
  ledManager.turnOnGreen();
}
