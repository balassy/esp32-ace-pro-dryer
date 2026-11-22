#pragma once

#include "AceProManager.h"
#include "Menu.h"
#include "Page.h"

class AboutPage : public Page {
 public:
  AboutPage(DisplayManager& display, KnobManager& knob, AceProManager& aceProManager, Menu::ClickCallback backCallback);

  void activate() override;
  void deactivate() override;
  void showDryerInfo(const char* model, const char* firmware, const char* bootFirmware);
  void onKnobTurned(long value) override;
  void onKnobClicked() override;
  void loop() override;

 private:
  Menu _menu;
  AceProManager& _aceProManager;

  bool _getInfo();
  bool _infoRequested = false;
};
