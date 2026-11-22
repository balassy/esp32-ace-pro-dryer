#include "LogRouter.h"
#include "AceProManager.h"

#include "AboutPage.h"

AboutPage::AboutPage(DisplayManager& display, KnobManager& knob, AceProManager& aceProManager, Menu::ClickCallback backCallback)
    : Page(display),
      _menu(display, knob, "Back", backCallback),
      _aceProManager(aceProManager) {}

void AboutPage::activate() {
  logRouter.println(F("AboutPage::activate: Activating About page..."));
  _display.clear();
  _display.printStatus("About");
  _menu.activate();
  _infoRequested = false;
  logRouter.println(F("AboutPage::activate: Activating About page DONE."));
}

void AboutPage::deactivate() {
  _menu.deactivate();
}

void AboutPage::showDryerInfo(const char* model, const char* firmware, const char* bootFirmware) {
  logRouter.println(F("AboutPage::showDryerInfo: Displaying dryer info..."));
  if (model == nullptr || model[0] == '\0') {
    _display.printInfoMessage("Disconnected");
    return;
  }
  _display.printDryerInfo(model, firmware, bootFirmware);
  logRouter.println(F("AboutPage::showDryerInfo: Displaying dryer info DONE."));
}

void AboutPage::onKnobTurned(long value) {
  _menu.onKnobTurned(value);
}

void AboutPage::onKnobClicked() {
  _menu.onKnobClicked();
}

void AboutPage::loop() {
  if (!_infoRequested && _getInfo()) {
    logRouter.println(F("AboutPage::loop: Dryer info requested."));
    _display.printInfoMessage("Reading...");
  }

  _menu.loop();
}

bool AboutPage::_getInfo() {
  if (!_aceProManager.requestDryerInfo()) {
    return false;
  }
  _infoRequested = true;
  return true;
}
