#include "MainPage.h"

MainPage::MainPage(DisplayManager& display, KnobManager& knob,
                   Menu::ClickCallback settingsCallback)
    : Page(display),
      _menu(display, knob, "Settings", settingsCallback) {}

void MainPage::activate() {
  _display.clear();
  _menu.activate();
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
