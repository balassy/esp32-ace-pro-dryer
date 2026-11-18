#pragma once

#include "Menu.h"
#include "Page.h"

class MainPage : public Page {
 public:
  MainPage(DisplayManager& display, KnobManager& knob,
           Menu::ClickCallback settingsCallback, Menu::ClickCallback stopCallback);

  void activate() override;
  void setStopButtonVisible(bool visible);
  void onKnobTurned(long value) override;
  void onKnobClicked() override;
  void loop() override;

 private:
  Menu _menu;
  Menu::ClickCallback _stopCallback;
};
