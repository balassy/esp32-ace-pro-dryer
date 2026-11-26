#pragma once

#include "AceProManager.h"
#include "DisplayManager.h"

class CommunicationIndicator {
 public:
  CommunicationIndicator(DisplayManager& display, AceProManager& aceProManager);

  void setup();
  void update();

 private:
  DisplayManager& _display;
  AceProManager& _aceProManager;

  bool _lastIsActive= false;
};
