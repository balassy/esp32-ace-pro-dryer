#pragma once

#include <Adafruit_SSD1306.h>

class DisplayManager {
 public:
  DisplayManager();

  bool setup();
  void clear();
  void printStatus(const char* text);
  void printMenuButtons(const char* primaryText, const char* secondaryText, bool primarySelected);
  void updateSpinner();
  void printCurrentTemperature(long value);
  void printTargetTemperature(long value);

 private:
   void drawSpinner();
   void printCenteredMenuButton(const char* text, int x, int width, int y);

   static constexpr int SCREEN_WIDTH = 128;       // Width of the display in pixels.
   static constexpr int SCREEN_HEIGHT = 64;       // Height of the display in pixels.
   static constexpr int DEFAULT_CHAR_WIDTH = 6;   // Width of the monospace characters in pixels for the default font.
   static constexpr int DEFAULT_CHAR_HEIGHT = 8;  // Height of the monospace characters in pixels for the default font.
   static constexpr int VALUE_CHAR_HEIGHT = 25;   // Height of the character used to display the temperature text for the custom font.
   static constexpr int STATUS_BAR_HEIGHT = 12;   // Height of the status bar in pixels.
   static constexpr int MENU_BAR_HEIGHT = 12;     // Height of the menu bar in pixels. DEFAULT_CHAR_HEIGHT + 4 pixels for padding and horizontal line.
   static constexpr unsigned long SPINNER_INTERVAL_MS = 500;

   Adafruit_SSD1306 _display;
   unsigned long _lastSpinnerUpdate = 0;
   unsigned int _spinnerFrame = 0;
};
