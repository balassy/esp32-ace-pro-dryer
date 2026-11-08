#pragma once

#include "DisplayManager.h"

class Page {
 public:
  explicit Page(DisplayManager& display);
  virtual ~Page() = default;

  virtual void activate() = 0;
  virtual void onKnobTurned(long value) = 0;
  virtual void onKnobClicked() = 0;
  virtual void loop() = 0;

 protected:
  DisplayManager& _display;
};
