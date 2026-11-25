#pragma once

#include "Menu.h"
#include "Page.h"

class SettingsPage : public Page {
 public:
  SettingsPage(DisplayManager& display, KnobManager& knob, 
               AceProManager& aceProManager, 
               Menu::ClickCallback onStartButtonClickedCallback,
               Menu::ClickCallback onBackButtonClickedCallback);

  void activate() override;
  void deactivate() override;
  void onKnobTurned(long value) override;
  void onKnobClicked() override;
  void loop() override;

 private:
  enum class StartDryingState : uint8_t { IDLE, SENT, FAILED }; 

  Menu _menu;
  AceProManager& _aceProManager;

  StartDryingState _startDryingState = StartDryingState::IDLE;

  void _onStartButtonClicked();
  Menu::ClickCallback _onStartButtonClickedCallback = nullptr;

  void _onBackButtonClicked();
  Menu::ClickCallback _onBackButtonClickedCallback = nullptr;
};
