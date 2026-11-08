#pragma once

#include "Menu.h"
#include "Page.h"

class MainPage : public Page {
 public:
  MainPage(DisplayManager& display, KnobManager& knob, Menu::ClickCallback settingsCallback);

  void activate() override;
  void onKnobTurned(long value) override;
  void onKnobClicked() override;
  void loop() override;

 private:
  Menu _menu;
};
