#pragma once

#include <atomic>

#include "DisplayManager.h"
#include "KnobManager.h"

class Menu {
 public:
  using ClickCallback = void (*)();

  Menu(DisplayManager& display, KnobManager& knob, const char* primaryText,
       ClickCallback primaryCallback, const char* secondaryText = nullptr,
       ClickCallback secondaryCallback = nullptr);

  void activate();
  void onKnobTurned(long value);
  void onKnobClicked();
  void loop();

 private:
  DisplayManager& _display;
  KnobManager& _knob;
  const char* _primaryText;
  ClickCallback _primaryCallback;
  const char* _secondaryText;
  ClickCallback _secondaryCallback;
  
  std::atomic<bool> _isActive{false};
  std::atomic<long> _pendingTurnValue{0};
  std::atomic<bool> _isTurnPending{false};
  std::atomic<bool> _isClickPending{false};
  bool _primarySelected = true;
};
