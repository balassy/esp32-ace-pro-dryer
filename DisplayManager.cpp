#include "DisplayManager.h"

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>

// Font reference: https://learn.adafruit.com/adafruit-gfx-graphics-library/using-fonts
#include <Fonts/FreeSansBold9pt7b.h>
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSerifItalic12pt7b.h>

#include "config.h"
#include "LogRouter.h"

DisplayManager::DisplayManager()
    : _display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1) {}

bool DisplayManager::setup() {
  constexpr unsigned long setupTimeoutMs = 5000;
  constexpr unsigned long retryIntervalMs = 100;
  const unsigned long startTime = millis();

  logRouter.println(F("DisplayManager::setup: Initializing display..."));
  logRouter.print(F("DisplayManager::setup: I2C pins - SDA: "));
  logRouter.print(DISPLAY_SDA_PIN);
  logRouter.print(F(", SCL: "));
  logRouter.println(DISPLAY_SCL_PIN);

  Wire.begin(DISPLAY_SDA_PIN, DISPLAY_SCL_PIN);
  while (!_display.begin(SSD1306_SWITCHCAPVCC, DISPLAY_ADDRESS)) {
    if (millis() - startTime >= setupTimeoutMs) {
      logRouter.println(F("DisplayManager::setup: Display initialization FAILED (timeout)."));
      return false;
    }
    delay(retryIntervalMs);
  }

  // Wipe the Adafruit logo out of the buffer completely.
  _display.clearDisplay(); 

  logRouter.println(F("DisplayManager::setup: Initializing display DONE."));
  return true;
}

void DisplayManager::clear() {
  _display.clearDisplay();
  _drawAceProIndicator();
  _display.display();
}

void DisplayManager::printStatus(const char* text) {
  // Draw the full-width light background bar.
  _display.fillRect(0, 0, SCREEN_WIDTH, STATUS_BAR_HEIGHT, SSD1306_WHITE);
  
  // Set text settings for the bar (dark text on light background)
  _display.setFont(NULL);  // Force system font.
  _display.setTextSize(1);
  _display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
  
  // Calculate centered X position.
  // Subtracting 1 from the text width accounts for the missing trailing pixel space on the last character
  int textWidth = strlen(text) * DEFAULT_CHAR_WIDTH; 
  int startX = (SCREEN_WIDTH - (textWidth - 1)) / 2;

  // Center Y mathematically within the bar height.
  int startY = (STATUS_BAR_HEIGHT - DEFAULT_CHAR_HEIGHT) / 2;

  // Print the centered text inside the bar.
  _display.setCursor(startX, startY);
  _display.print(text);
  _drawAceProIndicator();

  _display.display();
}

void DisplayManager::setAceProConnected(bool connected) {
  if (_aceProConnected == connected) {
    return;
  }

  _aceProConnected = connected;
  _drawAceProIndicator();
  _display.display();
}

void DisplayManager::_drawAceProIndicator() {
  constexpr int indicatorX = SCREEN_WIDTH - 9;
  constexpr int indicatorY = STATUS_BAR_HEIGHT / 2;
  _display.fillRect(SCREEN_WIDTH - 12, 0, 12, STATUS_BAR_HEIGHT, SSD1306_WHITE);
  if (_aceProConnected) {
    _display.fillCircle(indicatorX, indicatorY, 3, SSD1306_BLACK);
  } else {
    _display.drawCircle(indicatorX, indicatorY, 3, SSD1306_BLACK);
  }
}

void DisplayManager::printMenuButtons(const char* primaryText,
                                      const char* secondaryText,
                                      bool primarySelected) {
  constexpr int menuBarY = SCREEN_HEIGHT - MENU_BAR_HEIGHT;
  if (secondaryText == nullptr) {
    _display.setFont(nullptr);
    _display.setTextSize(1);
    _display.fillRect(0, menuBarY, SCREEN_WIDTH, MENU_BAR_HEIGHT, SSD1306_BLACK);
    _display.drawFastHLine(0, menuBarY, SCREEN_WIDTH, SSD1306_WHITE);
    _display.fillRect(0, menuBarY + 1, SCREEN_WIDTH, MENU_BAR_HEIGHT - 1, SSD1306_WHITE);
    _display.setTextColor(SSD1306_BLACK);
    _printCenteredMenuButton(primaryText, 0, SCREEN_WIDTH, menuBarY);
    _display.display();
    return;
  }

  const int primaryTextWidth = static_cast<int>(strlen(primaryText)) * DEFAULT_CHAR_WIDTH;
  const int secondaryTextWidth = static_cast<int>(strlen(secondaryText)) * DEFAULT_CHAR_WIDTH;
  const int extraWidth = SCREEN_WIDTH - primaryTextWidth - secondaryTextWidth;
  const int buttonWidth = extraWidth > 0 ? primaryTextWidth + extraWidth / 2 : SCREEN_WIDTH / 2;
  const uint16_t primaryBackground = primarySelected ? SSD1306_WHITE : SSD1306_BLACK;
  const uint16_t primaryForeground = primarySelected ? SSD1306_BLACK : SSD1306_WHITE;
  const uint16_t secondaryBackground = primarySelected ? SSD1306_BLACK : SSD1306_WHITE;
  const uint16_t secondaryForeground = primarySelected ? SSD1306_WHITE : SSD1306_BLACK;

  _display.setFont(nullptr);
  _display.setTextSize(1);
  _display.fillRect(0, menuBarY, SCREEN_WIDTH, MENU_BAR_HEIGHT, SSD1306_BLACK);
  _display.drawFastHLine(0, menuBarY, SCREEN_WIDTH, SSD1306_WHITE);
  _display.fillRect(0, menuBarY + 1, buttonWidth, MENU_BAR_HEIGHT - 1, primaryBackground);
  _display.fillRect(buttonWidth, menuBarY + 1, SCREEN_WIDTH - buttonWidth, MENU_BAR_HEIGHT - 1, secondaryBackground);
  _display.drawFastVLine(buttonWidth, menuBarY, MENU_BAR_HEIGHT, SSD1306_WHITE);

  _display.setTextColor(primaryForeground);
  _printCenteredMenuButton(primaryText, 0, buttonWidth, menuBarY);
  _display.setTextColor(secondaryForeground);
  if (secondaryText != nullptr) {
    _printCenteredMenuButton(secondaryText, buttonWidth, SCREEN_WIDTH - buttonWidth, menuBarY);
  }

  _display.display();
}

void DisplayManager::_printCenteredMenuButton(const char* text, int x, int width, int y) {
  if (text == nullptr) {
    return;
  }

  const int textWidth = strlen(text) * DEFAULT_CHAR_WIDTH - 1;
  const int textX = x + (width - textWidth) / 2;
  const int textY = y + (MENU_BAR_HEIGHT - DEFAULT_CHAR_HEIGHT) / 2;
  _display.setCursor(textX, textY);
  _display.print(text);
}

void DisplayManager::_clearContent() {
  _display.fillRect(0, STATUS_BAR_HEIGHT, SCREEN_WIDTH,
                    SCREEN_HEIGHT - STATUS_BAR_HEIGHT - MENU_BAR_HEIGHT, SSD1306_BLACK);
}

int DisplayManager::_printWrappedText(const char* text, int y, int maxY) {
  if (text == nullptr) {
    return y;
  }

  const int maxChars = SCREEN_WIDTH / DEFAULT_CHAR_WIDTH;
  const size_t length = strlen(text);
  size_t offset = 0;
  while (offset < length && y + DEFAULT_CHAR_HEIGHT <= maxY) {
    size_t count = length - offset;
    if (count > static_cast<size_t>(maxChars)) {
      count = static_cast<size_t>(maxChars);
    }
    _display.setCursor(0, y);
    for (size_t index = 0; index < count; ++index) {
      _display.write(text[offset + index]);
    }
    offset += count;
    y += DEFAULT_CHAR_HEIGHT;
  }
  return y;
}

void DisplayManager::printInfoMessage(const char* text) {
  _clearContent();
  _display.setFont(nullptr);
  _display.setTextSize(1);
  _display.setTextColor(SSD1306_WHITE);

  const int textWidth = text == nullptr ? 0 : static_cast<int>(strlen(text)) * DEFAULT_CHAR_WIDTH - 1;
  const int contentTop = STATUS_BAR_HEIGHT;
  const int contentHeight = SCREEN_HEIGHT - STATUS_BAR_HEIGHT - MENU_BAR_HEIGHT;
  const int x = (SCREEN_WIDTH - textWidth) / 2;
  const int y = contentTop + (contentHeight - DEFAULT_CHAR_HEIGHT) / 2;
  _display.setCursor(x, y);
  _display.print(text);
  _display.display();
}

void DisplayManager::printDryerInfo(const char* model, const char* firmware, const char* bootFirmware) {
  _clearContent();
  _display.setFont(nullptr);
  _display.setTextSize(1);
  _display.setTextColor(SSD1306_WHITE);

  const int contentBottom = SCREEN_HEIGHT - MENU_BAR_HEIGHT;
  int y = _printWrappedText(model, STATUS_BAR_HEIGHT + 2, contentBottom);
  y++;
  if (y + DEFAULT_CHAR_HEIGHT <= contentBottom) {
    _display.setCursor(0, y);
    _display.print(F("Firmware: "));
    _display.print(firmware);
    y += DEFAULT_CHAR_HEIGHT;
  }
  y++;
  if (y + DEFAULT_CHAR_HEIGHT <= contentBottom) {
    _display.setCursor(0, y);
    _display.print(F("Boot fw.: "));
    _display.print(bootFirmware);
  }
  _display.display();
}

void DisplayManager::updateSpinner() {
  const unsigned long now = millis();
  if (now - _lastSpinnerUpdate < SPINNER_INTERVAL_MS) {
    return;
  }

  _lastSpinnerUpdate = now;
  _spinnerFrame = (_spinnerFrame + 1) % 4;

  constexpr char spinnerFrames[] = {'|', '/', '-', '\\'};

  _display.setFont(nullptr);
  _display.setTextSize(1);
  _display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
  _display.setCursor(2, (STATUS_BAR_HEIGHT - DEFAULT_CHAR_HEIGHT) / 2);
  _display.write(spinnerFrames[_spinnerFrame]);

  _display.display();
}

void DisplayManager::printCurrentTemperature(long value) {
  char text[16];
  snprintf(text, sizeof(text), "%ld C", value);
  _display.setTextColor(SSD1306_WHITE);

  _display.setFont(&FreeSans9pt7b);
  _display.setCursor(0, 35);
  _display.print("Current:");

  _display.fillRect(75, STATUS_BAR_HEIGHT, SCREEN_WIDTH - 75, VALUE_CHAR_HEIGHT, SSD1306_BLACK); // Clear the right side of the display

  _display.setFont(&FreeSansBold9pt7b);
  _display.setCursor(75, 35);
  _display.print(text);

  _display.display();
}

void DisplayManager::printTargetTemperature(long value) {
  char text[16];
  snprintf(text, sizeof(text), "%ld C", value);
  _display.setTextColor(SSD1306_WHITE);

  _display.setFont(&FreeSans9pt7b);
  _display.setCursor(0, SCREEN_HEIGHT - 8);
  _display.print("Target:");

  _display.fillRect(75, STATUS_BAR_HEIGHT + VALUE_CHAR_HEIGHT, SCREEN_WIDTH - 75, VALUE_CHAR_HEIGHT, SSD1306_BLACK); // Clear the right side of the display

  _display.setFont(&FreeSansBold9pt7b);
  _display.setCursor(75, SCREEN_HEIGHT - 8);
  _display.print(text);

  _display.display();
}
