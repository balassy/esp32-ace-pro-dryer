#include "SettingsPage.h"

SettingsPage::SettingsPage(DisplayManager& display, KnobManager& knob,
                           Menu::ClickCallback startCallback,
                           Menu::ClickCallback backCallback)
    : Page(display),
      _menu(display, knob, "Start", startCallback, "Back", backCallback) {}

void SettingsPage::activate() {
  _display.clear();
  _display.printStatus("Settings");
  _menu.activate();
}

void SettingsPage::onKnobTurned(long value) {
  _menu.onKnobTurned(value);
}

void SettingsPage::onKnobClicked() {
  _menu.onKnobClicked();
}

void SettingsPage::loop() {
  _menu.loop();
}
