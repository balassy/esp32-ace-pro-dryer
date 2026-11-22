#include "Menu.h"
#include "LogRouter.h"

Menu::Menu(DisplayManager& display, KnobManager& knob, const char* primaryText,
           ClickCallback primaryCallback, const char* secondaryText,
           ClickCallback secondaryCallback)
    : _display(display),
      _knob(knob),
      _primaryText(primaryText),
      _primaryCallback(primaryCallback),
      _secondaryText(secondaryText),
      _secondaryCallback(secondaryCallback) {}

void Menu::deactivate() {
  _isActive.store(false);
}

void Menu::activate() {
  _isActive.store(true);
  _isTurnPending.store(false);
  _isClickPending.store(false);
  _primarySelected = true;
  const bool hasSecondaryButton = _secondaryText != nullptr;
  _knob.configure(0, hasSecondaryButton ? 1 : 0, 1, 0);
  _display.printMenuButtons(_primaryText, _secondaryText, _primarySelected);
}

void Menu::setPrimaryButton(const char* text, ClickCallback callback) {
  _primaryText = text;
  _primaryCallback = callback;
  if (!_isActive.load()) {
    return;
  }

  _display.printMenuButtons(_primaryText, _secondaryText, _primarySelected);
}

void Menu::setSecondaryButton(const char* text, ClickCallback callback) {
  _secondaryText = text;
  _secondaryCallback = callback;
  if (!_isActive.load()) {
    return;
  }

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
  const bool hasSecondaryButton = _secondaryText != nullptr;
  if (_isTurnPending.exchange(false) && hasSecondaryButton) {
    _primarySelected = _pendingTurnValue.load() <= 0;
    _display.printMenuButtons(_primaryText, _secondaryText, _primarySelected);
  }

  if (!_isClickPending.exchange(false)) {
    return;
  }

  const bool primaryClicked = _primarySelected || !hasSecondaryButton;
  ClickCallback callback = primaryClicked ? _primaryCallback : _secondaryCallback;
  if (callback != nullptr) {
    logRouter.print(F("Menu: Button clicked: "));
    logRouter.println(primaryClicked ? _primaryText : _secondaryText);
    callback();
  }
}
