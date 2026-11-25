#pragma once

#include <atomic>
#include <functional>

#include "DisplayManager.h"
#include "KnobManager.h"

class Menu {
 public:
  using ClickCallback = std::function<void()>;

  Menu(DisplayManager& display, KnobManager& knob, const char* primaryText,
       ClickCallback primaryCallback, const char* secondaryText = nullptr,
       ClickCallback secondaryCallback = nullptr);

  // Calls a member function of the page that owns this menu.
  template <class Owner>
  Menu(DisplayManager& display, KnobManager& knob, const char* primaryText, Owner* owner,
       void (Owner::*primaryCallback)(), const char* secondaryText = nullptr,
       void (Owner::*secondaryCallback)() = nullptr)
      : _display(display),
        _knob(knob),
        _primaryText(primaryText),
        _primaryCallback(memberCallback(owner, primaryCallback)),
        _secondaryText(secondaryText),
        _secondaryCallback(memberCallback(owner, secondaryCallback)) {}

  void activate();
  void deactivate();
  void setPrimaryButton(const char* text, ClickCallback callback);
  void setSecondaryButton(const char* text, ClickCallback callback);
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

  template <class Owner>
  static ClickCallback memberCallback(Owner* owner, void (Owner::*method)()) {
    if (owner == nullptr || method == nullptr) {
      return nullptr;
    }
    return [owner, method]() { (owner->*method)(); };
  }
};
