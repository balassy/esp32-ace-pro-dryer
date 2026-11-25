#include "LogRouter.h"
#include "AceProManager.h"
#include "SettingsPage.h"

SettingsPage::SettingsPage(DisplayManager& display, KnobManager& knob, AceProManager& aceProManager, 
                           Menu::ClickCallback onStartButtonClickedCallback,
                           Menu::ClickCallback onBackButtonClickedCallback)
    : Page(display),
      _menu(display, knob, "Start", this, &SettingsPage::_onStartButtonClicked, "Back",
            &SettingsPage::_onBackButtonClicked),
      _aceProManager(aceProManager),
      _onStartButtonClickedCallback(onStartButtonClickedCallback),
      _onBackButtonClickedCallback(onBackButtonClickedCallback) {}

void SettingsPage::activate() {
  _display.clear();
  _display.printStatus("Settings");
  _menu.activate();
}

void SettingsPage::deactivate() {
  _menu.deactivate();
}

void SettingsPage::onKnobTurned(long value) {
  _menu.onKnobTurned(value);
}

void SettingsPage::onKnobClicked() {
  _menu.onKnobClicked();
}

void SettingsPage::loop() {
  switch (_startDryingState)
  {
    case StartDryingState::IDLE:
      break;
  
    case StartDryingState::SENT: {
      logRouter.println(F("SettingsPage::loop: Requesting start DONE."));
      logRouter.println(F("SettingsPage::loop: Taking start..."));
      bool dryingStarted = false;
      if (_aceProManager.takeStart(dryingStarted)) {
        if (dryingStarted) {
          logRouter.println(F("SettingsPage::loop: Taking start DONE."));
          _display.printStatus("Start requested");
        } else {
           logRouter.println(F("SettingsPage::loop: Taking start unconfirmed."));
          _display.printStatus("Start unconfirmed");         
        }
        _startDryingState = StartDryingState::IDLE;

        // Navigate back to the Main page after drying started.
        if (_onStartButtonClickedCallback) {
          _onStartButtonClickedCallback();
        }
      } else {
        logRouter.println(F("SettingsPage::loop: ERROR: Taking start failed!"));
      }
      break;
    }

    case StartDryingState::FAILED: {
      _display.printStatus("Busy");
      break;
    }

    default:
      logRouter.println(F("SettingsPage::loop: ERROR Unknown StartDryingState!"));
      break;
  }

  _menu.loop();
}

void SettingsPage::_onStartButtonClicked() {
  logRouter.println(F("SettingsPage::_onStartButtonClicked: Start button clicked."));

  _display.printStatus("Starting...");
  if (!_aceProManager.requestStart()) {
    logRouter.println(F("SettingsPage::_onStartButtonClicked: ERROR: Requesting dryer start failed!"));
    _display.printStatus("Busy");
    return;
  }

  _startDryingState = StartDryingState::SENT;

  // IMPORTANT! Navigate back to the Main page only after the start command is completed (see the loop() function)!

  logRouter.println(F("SettingsPage::_onStartButtonClicked: Start button clicked DONE."));
}

void SettingsPage::_onBackButtonClicked() {
  logRouter.println(F("SettingsPage::_onBackButtonClicked: Back button clicked."));
  if (_onBackButtonClickedCallback) {
    _onBackButtonClickedCallback();
  }
}
