#include "CommunicationIndicator.h"

CommunicationIndicator::CommunicationIndicator(DisplayManager& display, AceProManager& aceProManager)
  : _display(display),
    _aceProManager(aceProManager) {}

void CommunicationIndicator::setup() {
  const bool isActive = _aceProManager.isConnected();
  _display.drawCommunicationIndicator(isActive);
}

void CommunicationIndicator::update() {
  const bool isActive = _aceProManager.isConnected();

  if (isActive != _lastIsActive)
  {
    _display.drawCommunicationIndicator(isActive);
    _lastIsActive = isActive;
  }  
}

