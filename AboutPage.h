#pragma once

#include "AceProManager.h"
#include "Menu.h"
#include "Page.h"

class AboutPage : public Page {
 public:
  AboutPage(DisplayManager& display, KnobManager& knob, AceProManager& aceProManager, Menu::ClickCallback onBackButtonClickedCallback);

  void activate() override;
  void deactivate() override;
  void onKnobTurned(long value) override;
  void onKnobClicked() override;
  void loop() override;

 private:
  enum class GetInfoResult : uint8_t { OK, ERROR };
  enum class GetInfoState : uint8_t { NOT_REQUESTED, REQUESTED, RECEIVED };

  Menu _menu;
  AceProManager& _aceProManager;

  GetInfoResult _getInfo();
  GetInfoState _getInfoState = GetInfoState::NOT_REQUESTED;

  void _showDryerInfo(const char* model, const char* firmware, const char* bootFirmware);

  void _onBackButtonClicked();
  Menu::ClickCallback _onBackButtonClickedCallback = nullptr;
};
