#include "LogRouter.h"
#include "AceProManager.h"
#include "Menu.h"

#include "AboutPage.h"

AboutPage::AboutPage(DisplayManager& display, KnobManager& knob, AceProManager& aceProManager, Menu::ClickCallback onBackButtonClickedCallback)
    : Page(display),
      _menu(display, knob, "Back", this, &AboutPage::_onBackButtonClicked),
      _aceProManager(aceProManager),
      _onBackButtonClickedCallback(onBackButtonClickedCallback) {}

void AboutPage::activate() {
  logRouter.println(F("AboutPage::activate: Activating About page..."));
  _display.clear();
  _display.printStatus("About");
  _menu.activate();
  _getInfoState = GetInfoState::NOT_REQUESTED;
  logRouter.println(F("AboutPage::activate: Activating About page DONE."));
}

void AboutPage::deactivate() {
  _menu.deactivate();
}

void AboutPage::onKnobTurned(long value) {
  _menu.onKnobTurned(value);
}

void AboutPage::onKnobClicked() {
  _menu.onKnobClicked();
}

void AboutPage::loop() {
  switch (_getInfoState) {
    case GetInfoState::NOT_REQUESTED: {
      logRouter.println(F("AboutPage::loop: Requesting dryer info..."));

      _display.printInfoMessage("Connecting...");

      AboutPage::GetInfoResult result = _getInfo();
      switch (result)
      {
        case GetInfoResult::OK:
          logRouter.println(F("AboutPage::loop: Requesting dryer info DONE."));
          break;

        case GetInfoResult::ERROR:
          logRouter.println(F("AboutPage::loop: ERROR: Failed to request dryer info."));
          _display.printInfoMessage("ACE Pro not found :("); // TODO: SPecifically handle the Disconnected case.
          break;

        default:
          logRouter.println(F("AboutPage::loop: ERROR: Unknown result from _getInfo()."));
          break;
      }
      _getInfoState = GetInfoState::REQUESTED;
      break;
    }

    case GetInfoState::REQUESTED: {
      AceProManager::DryerInfo dryerInfo;
      if (_aceProManager.takeDryerInfo(dryerInfo)) {
        _getInfoState = GetInfoState::RECEIVED;
        _showDryerInfo(dryerInfo.model, dryerInfo.firmware, dryerInfo.bootFirmware);
      }
      break;
    }

    case GetInfoState::RECEIVED:
      break;
  }

  _menu.loop();
}

AboutPage::GetInfoResult AboutPage::_getInfo() {
  _getInfoState = GetInfoState::REQUESTED;
  if (!_aceProManager.requestDryerInfo()) {
    return GetInfoResult::ERROR;
  }
  return GetInfoResult::OK;
}

void AboutPage::_showDryerInfo(const char* model, const char* firmware, const char* bootFirmware) {
  logRouter.println(F("AboutPage::showDryerInfo: Displaying dryer info..."));
  _display.printDryerInfo(model, firmware, bootFirmware);
  logRouter.println(F("AboutPage::showDryerInfo: Displaying dryer info DONE."));
}

void AboutPage::_onBackButtonClicked() {
  logRouter.println(F("AboutPage::_onBackButtonClicked: Back button clicked."));
  if (_onBackButtonClickedCallback) {
    _onBackButtonClickedCallback();
  }
}