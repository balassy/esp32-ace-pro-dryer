#pragma once

#include <Adafruit_SSD1306.h>

class DisplayManager {
 public:
  DisplayManager();

  bool setup();
  void printHelloWorld();

 private:
   static constexpr int _screenWidth = 128;
   static constexpr int _screenHeight = 64;

   Adafruit_SSD1306 _display;
};
