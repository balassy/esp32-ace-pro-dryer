#pragma once

#include "Menu.h"
#include "Page.h"

class AboutPage : public Page {
 public:
  using RequestInfoCallback = bool (*)();

  AboutPage(DisplayManager& display, KnobManager& knob, Menu::ClickCallback backCallback,
            RequestInfoCallback requestInfoCallback);

  void activate() override;
  void deactivate() override;
  void showDryerInfo(const char* model, const char* firmware, const char* bootFirmware);
  void onKnobTurned(long value) override;
  void onKnobClicked() override;
  void loop() override;

 private:
  Menu _menu;
  RequestInfoCallback _requestInfoCallback;
  bool _infoRequested = false;
};
