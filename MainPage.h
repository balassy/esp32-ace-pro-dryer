#pragma once

#include "Menu.h"
#include "Page.h"

class MainPage : public Page {
 public:
  MainPage(DisplayManager& display, KnobManager& knob, Menu::ClickCallback settingsCallback,
           Menu::ClickCallback stopCallback, Menu::ClickCallback aboutCallback);

  void activate() override;
  void deactivate() override;
  void setDrying(bool drying);
  void onKnobTurned(long value) override;
  void onKnobClicked() override;
  void loop() override;

 private:
  Menu _menu;
  Menu::ClickCallback _settingsCallback;
  Menu::ClickCallback _stopCallback;
};
