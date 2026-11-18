#include "MainPage.h"

MainPage::MainPage(DisplayManager& display, KnobManager& knob,
                   Menu::ClickCallback settingsCallback,
                   Menu::ClickCallback stopCallback)
    : Page(display),
      _menu(display, knob, "Settings", settingsCallback),
      _stopCallback(stopCallback) {}

void MainPage::activate() {
  _display.clear();
  _menu.activate();
}

void MainPage::setStopButtonVisible(bool visible) {
  _menu.setSecondaryButton(visible ? "Stop" : nullptr,
                           visible ? _stopCallback : nullptr);
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
