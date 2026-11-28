#include "LogRouter.h"
#include "AceProManager.h"
#include "LedManager.h"
#include "MainPage.h"

MainPage::MainPage(DisplayManager& display, 
                   KnobManager& knob,
                   AceProManager& aceProManager,
                   LedManager& ledManager,
                   Menu::ClickCallback onSettingsButtonClickedCallback, 
                   Menu::ClickCallback onAboutButtonClickedCallback)
    : Page(display),
      _menu(display, knob, "Start", onSettingsButtonClickedCallback, "About", onAboutButtonClickedCallback),
      _aceProManager(aceProManager),
      _ledManager(ledManager),
      _onSettingsButtonClickedCallback(onSettingsButtonClickedCallback),
      _onAboutButtonClickedCallback(onAboutButtonClickedCallback) {}

void MainPage::activate() {
  logRouter.println(F("MainPage::activate: Activating Main page..."));
  _display.clear();
  _display.printStatus("Main");
  _display.printInfoMessage("Updating...");
  _menu.activate();
  logRouter.println(F("MainPage::activate: Activating Main page DONE."));
}

void MainPage::deactivate() {
  _menu.deactivate();
}

void MainPage::onKnobTurned(long value) {
  _menu.onKnobTurned(value);
}

void MainPage::onKnobClicked() {
  _menu.onKnobClicked();
}

void MainPage::loop() {
  switch (_stopDryingState)
  {
    case StopDryingState::IDLE:
      break;

    case StopDryingState::SENT: {
      bool dryingStopped = false;
      if (_aceProManager.takeStop(dryingStopped)) {
        _stopDryingState = StopDryingState::IDLE;
        if (dryingStopped) {
          logRouter.println(F("MainPage::loop: Taking stop DONE."));
          _display.printStatus("Stopping...");
        } else {
          logRouter.println(F("MainPage::loop: Taking stop unconfirmed."));
        }
      }      
      break;
    }
    case StopDryingState::FAILED: {
      _display.printStatus("Busy");
      _stopDryingState = StopDryingState::IDLE;
      break;
    }

    default:
      logRouter.println(F("MainPage::loop: ERROR: Unknown StopDryingState!"));
      break;
  }

  switch (_getDryerStatusState)
  {
    case GetDryerStatusState::IDLE:
      break;
  
    case GetDryerStatusState::SENT: {
      AceProManager::DryerStatus dryerStatus;
      if (_aceProManager.takeDryerStatus(dryerStatus)) {
        _getDryerStatusState = GetDryerStatusState::IDLE;
        if (dryerStatus.valid && dryerStatus.responseComplete) {
          logRouter.println(F("MainPage::loop: Taking dryer status DONE."));
          _showDryerStatus(dryerStatus);
        } else {
          logRouter.println(F("MainPage::loop: Taking dryer status unconfirmed."));
        }
      }
      break;
    }

    case GetDryerStatusState::FAILED: {
      _display.printStatus("Busy");
      _getDryerStatusState = GetDryerStatusState::IDLE;
      break;
    }

    default:
      break;
  }

  if (!_aceProManager.isCommandRunning() && millis() - _lastDryerStatusRequestMsec >= 10000) {
    if (_aceProManager.requestDryerStatus()) {
      _lastDryerStatusRequestMsec = millis();
      _getDryerStatusState = GetDryerStatusState::SENT;
    } else {
      _getDryerStatusState = GetDryerStatusState::FAILED;
    }
  }

  _menu.loop();
}

void MainPage::_onSettingsButtonClicked() {
  logRouter.println(F("MainPage::_onSettingsButtonClicked: Settings button clicked."));

  _ensureLastGetDryerStatusTaken();
  if (_onSettingsButtonClickedCallback) {
    _onSettingsButtonClickedCallback();
  }
}

void MainPage::_onStopButtonClicked() {
  logRouter.println(F("MainPage::_onStopButtonClicked: Stop button clicked..."));

  _display.printStatus("Stopping...");

  _ensureLastGetDryerStatusTaken();

  if (!_aceProManager.requestStop()) {
    _stopDryingState = StopDryingState::FAILED;
  } else {
    _stopDryingState = StopDryingState::SENT;
  }

  logRouter.println(F("MainPage::_onStopButtonClicked: Stop button clicked DONE."));
}

void MainPage::_onAboutButtonClicked() {
  logRouter.println(F("MainPage::_onAboutButtonClicked: About button clicked."));
  _ensureLastGetDryerStatusTaken();
  if (_onAboutButtonClickedCallback) {
    _onAboutButtonClickedCallback();
  }
}

void MainPage::_showDryerStatus(const AceProManager::DryerStatus& dryerStatus) {
  logRouter.println(F("MainPage::_showDryerStatus: Show dryer status..."));

  if (!dryerStatus.valid || !dryerStatus.responseComplete) {
    logRouter.println(F("MainPage::_showDryerStatus: ERROR: Dryer status invalid or incomplete!"));
    _display.printStatus("Status unavailable");
  } else if (dryerStatus.isDrying) {
    _display.printStatus("Drying");
    _menu.setPrimaryButton("Stop drying", [this]() { _onStopButtonClicked(); });
    _ledManager.turnOnRed();
    _display.printDryerStatus(dryerStatus);
  } else {
    _display.printStatus("Not drying");
    _menu.setPrimaryButton("Start", [this]() { _onSettingsButtonClicked(); });
    _ledManager.turnOnGreen();
    _display.printDryerStatus(dryerStatus);
  }

  logRouter.println(F("MainPage::_showDryerStatus: Show dryer status DONE."));
}

void MainPage::_ensureLastGetDryerStatusTaken() {
  logRouter.println(F("MainPage::_ensureLastGetDryerStatusTaken: Taking last dryer status..."));
  AceProManager::DryerStatus dryerStatus;
  bool result = _aceProManager.takeDryerStatus(dryerStatus);
  _getDryerStatusState = GetDryerStatusState::IDLE;
  logRouter.println(F("MainPage::_ensureLastGetDryerStatusTaken: Taking last dryer status DONE."));

}