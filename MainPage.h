#pragma once

#include "Menu.h"
#include "Page.h"

class MainPage : public Page {
 public:
  MainPage(DisplayManager& display, 
           KnobManager& knob, 
           AceProManager& aceProManager, 
           LedManager& ledManager,
           Menu::ClickCallback onSettingsButtonClickedCallback,
           Menu::ClickCallback onAboutButtonClickedCallback);

  void activate() override;
  void deactivate() override;
  void onKnobTurned(long value) override;
  void onKnobClicked() override;
  void loop() override;

 private:
  enum class StopDryingState : uint8_t { IDLE, SENT, FAILED };
  enum class GetDryerStatusState : uint8_t { IDLE, SENT, FAILED };

  Menu _menu;
  AceProManager& _aceProManager;
  LedManager& _ledManager;

  unsigned long _lastDryerStatusRequestMsec = 0;

  StopDryingState _stopDryingState = StopDryingState::IDLE;
  GetDryerStatusState _getDryerStatusState = GetDryerStatusState::IDLE;

  void _onSettingsButtonClicked();
  Menu::ClickCallback _onSettingsButtonClickedCallback = nullptr;

  void _onAboutButtonClicked();
  Menu::ClickCallback _onAboutButtonClickedCallback = nullptr;

  void _onStopButtonClicked();

  void _showDryerStatus(const AceProManager::DryerStatus& dryerStatus);
  void _ensureLastGetDryerStatusTaken(); 
};
