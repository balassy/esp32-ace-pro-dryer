#pragma once

#include "Menu.h"
#include "Page.h"

class SettingsPage : public Page {
 public:
  SettingsPage(DisplayManager& display, KnobManager& knob, Menu::ClickCallback startCallback,
               Menu::ClickCallback backCallback);

  void activate() override;
  void onKnobTurned(long value) override;
  void onKnobClicked() override;
  void loop() override;

 private:
  Menu _menu;
};
