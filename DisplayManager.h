#pragma once

#include <Adafruit_SSD1306.h>

class DisplayManager {
 public:
  DisplayManager();

  bool setup();
  void printStatus(const char* text);
  void printCurrentTemperature(long value);
  void printTargetTemperature(long value);

 private:
   static constexpr int SCREEN_WIDTH = 128;       // Width of the display in pixels.
   static constexpr int SCREEN_HEIGHT = 64;       // Height of the display in pixels.
   static constexpr int DEFAULT_CHAR_WIDTH = 6;   // Width of the monospace characters in pixels for the default font.
   static constexpr int DEFAULT_CHAR_HEIGHT = 8;  // Height of the monospace characters in pixels for the default font.
   static constexpr int VALUE_CHAR_HEIGHT = 25;   // Height of the character used to display the temperature text for the custom font.
   static constexpr int STATUS_BAR_HEIGHT = 12;   // Height of the status bar in pixels.

   Adafruit_SSD1306 _display;
};
