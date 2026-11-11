#include "Menu.h"

Menu::Menu(DisplayManager& display, KnobManager& knob, const char* primaryText,
           ClickCallback primaryCallback, const char* secondaryText,
           ClickCallback secondaryCallback)
    : _display(display),
      _knob(knob),
      _primaryText(primaryText),
      _primaryCallback(primaryCallback),
      _secondaryText(secondaryText),
      _secondaryCallback(secondaryCallback) {}

void Menu::activate() {
  _isActive.store(true);
  _primarySelected = true;
  const bool hasSecondaryButton = _secondaryText != nullptr;
  _knob.configure(0, hasSecondaryButton ? 1 : 0, 1, 0);
  _display.printMenuButtons(_primaryText, _secondaryText, _primarySelected);
}

void Menu::onKnobTurned(long value) {
  if (!_isActive.load()) {
    return;
  }

  _pendingTurnValue.store(value);
  _isTurnPending.store(true);
}

void Menu::onKnobClicked() {
  if (!_isActive.load()) {
    return;
  }

  _isClickPending.store(true);
}

void Menu::loop() {
  if (_isTurnPending.exchange(false)) {
    _primarySelected = _pendingTurnValue.load() <= 0;
    _display.printMenuButtons(_primaryText, _secondaryText, _primarySelected);
  }

  if (!_isClickPending.exchange(false)) {
    return;
  }

  ClickCallback callback = _primarySelected ? _primaryCallback : _secondaryCallback;
  if (callback != nullptr) {
    Serial.print(F("Menu: Button clicked: "));
    Serial.println(_primarySelected ? _primaryText : _secondaryText);
    callback();
  }
}
