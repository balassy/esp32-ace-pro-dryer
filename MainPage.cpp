#include "MainPage.h"

MainPage::MainPage(DisplayManager& display, KnobManager& knob,
                   Menu::ClickCallback settingsCallback, Menu::ClickCallback stopCallback,
                   Menu::ClickCallback aboutCallback)
    : Page(display),
      _menu(display, knob, "Settings", settingsCallback, "About", aboutCallback),
      _settingsCallback(settingsCallback),
      _stopCallback(stopCallback) {}

void MainPage::activate() {
  _display.clear();
  _menu.activate();
}

void MainPage::deactivate() {
  _menu.deactivate();
}

void MainPage::setDrying(bool drying) {
  if (drying) {
    _menu.setPrimaryButton("Stop drying", _stopCallback);
  } else {
    _menu.setPrimaryButton("Settings", _settingsCallback);
  }
}

void MainPage::onKnobTurned(long value) {
  _menu.onKnobTurned(value);
}

void MainPage::onKnobClicked() {
  _menu.onKnobClicked();
}

void MainPage::loop() {
  _menu.loop();
}
