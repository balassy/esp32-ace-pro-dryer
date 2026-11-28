#include "DisplayManager.h"

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>

// Font reference: https://learn.adafruit.com/adafruit-gfx-graphics-library/using-fonts
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/TomThumb.h>
#include <Fonts/Org_01.h>

#include "config.h"
#include "LogRouter.h"
#include "AceProManager.h"

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

  _display.display();
}

void DisplayManager::drawCommunicationIndicator(bool isActive) {
  constexpr int indicatorX = SCREEN_WIDTH - 7;
  constexpr int indicatorY =  STATUS_BAR_HEIGHT / 2 - 1;
  
  if (isActive) {
    _display.fillCircle(indicatorX, indicatorY, 3, SSD1306_BLACK);
  } else {
    _display.fillRect(SCREEN_WIDTH - 12, 0, 12, STATUS_BAR_HEIGHT, SSD1306_WHITE);
  }

  _display.display();
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

  int y = 0;
  if (model == nullptr || model[0] == '\0')
  {
    y = _printWrappedText("ACE Pro not found!", STATUS_BAR_HEIGHT + 2, contentBottom);
    y++;
    _display.setCursor(0, y);
    _display.print(F("Connect port 23 on:"));    
  } else {
    y = _printWrappedText(model, STATUS_BAR_HEIGHT + 2, contentBottom);
    y++;
    _display.setCursor(0, y);
    _display.print(firmware);
    _display.print(F(" ("));
    _display.print(bootFirmware);
    _display.print(F(")"));
  }
  y += DEFAULT_CHAR_HEIGHT;
  y++;
  _display.setCursor(0, y);
  _display.print(logRouter.getServerAddress());

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

void DisplayManager::printDryerStatus(const AceProManager::DryerStatus& status) {
  // UI layout is designed with https://lopaka.app/canvas/65042/125106.
  _clearContent();

  // Temperature frame
  _display.drawRoundRect(0, 14, 62, 36, 4, 1);
  // Time frame
  _display.drawRoundRect(66, 14, 62, 36, 4, 1);
  // Temperature icon
  _display.drawBitmap(5, 21, image_Temperature_icon_bits, 16, 16, 1);
  // Temperature current
  _display.setTextColor(1);
  _display.setFont(&FreeSans9pt7b);
  _display.setCursor(31, 34);
  _display.printf("%ld", status.currentTemperatureCelsius);
  // Temperature current degree symbol
  _display.drawEllipse(54, 24, 2, 2, 1);
  // Temperature label
  _display.setFont(&Org_01);
  _display.setCursor(52, 34);
  _display.print("C");

  if (status.isDrying) {
    const unsigned int remainingTimeMinutes = static_cast<unsigned int>(ceilf(status.remainingTimeSeconds / 60.0f));

    // Calculate the completed percentage.
    const unsigned int targetDurationSeconds = status.targetDurationMinutes * 60U;

    unsigned int completedPercentage = 0U;
    if (targetDurationSeconds > 0U) {
      const unsigned int elapsedSeconds = (status.remainingTimeSeconds < targetDurationSeconds)
            ? (targetDurationSeconds - status.remainingTimeSeconds)
            : 0U;
      const float pct = (static_cast<float>(elapsedSeconds) / static_cast<float>(targetDurationSeconds)) * 100.0f;
      const unsigned int rounded = static_cast<unsigned int>(ceilf(pct));
      completedPercentage = (rounded > 100U) ? 100U : rounded;
    }

    // Calculate the progress bars' width.
    int16_t temperatureProgressBarWidth = 0;

    const int currentTempClamped = status.currentTemperatureCelsius < 20
      ? 20
      : ((status.currentTemperatureCelsius > status.targetTemperatureCelsius)
                 ? status.targetTemperatureCelsius
                 : status.currentTemperatureCelsius);
    temperatureProgressBarWidth = static_cast<int16_t>(map(currentTempClamped, 20, status.targetTemperatureCelsius, 0, PROGRESS_BAR_WIDTH));

    int16_t timeProgressBarWidth = static_cast<int16_t>((completedPercentage * static_cast<unsigned int>(PROGRESS_BAR_WIDTH)) / 100U);

    // Temperature progress bar frame
    _display.drawRect(4, 40, 33, 5, 1);
    // Temperature progress bar value
    _display.fillRect(4, 40, temperatureProgressBarWidth, 5, 1);
    // Temperature target
    _display.setCursor(40, 44);
    _display.printf("%ldC", status.targetTemperatureCelsius);
    // Heating icon
    _display.drawBitmap(21, 27, image_Heating_icon_bits, 8, 8, 1);
    // Time icon
    _display.drawBitmap(70, 21, image_Time_icon_bits, 15, 16, 1);
    // Time remaining (right aligned to the label with gap before the label)
    _display.setFont(&FreeSans9pt7b);
    _printRightAlignedText(remainingTimeMinutes, 118 - 6, 34);
    // Time label
    _display.setFont(&Org_01);
    _display.setCursor(118, 34);
    _display.print("M");
    // Time percentage (right aligned)
    char completedPercentageText[4];
    snprintf(completedPercentageText, sizeof(completedPercentageText), "%u%%", completedPercentage);
    _display.setFont(&Org_01);
    _printRightAlignedText(completedPercentageText, 123, 44);
    // Time progress bar frame
    _display.drawRect(70, 40, 34, 5, 1);
    // Time progress bar value
    _display.fillRect(70, 40, timeProgressBarWidth, 5, 1);
  } else {
    // Time Ready label
    _display.setFont(&FreeSans9pt7b);
    _display.setCursor(71, 33);
    _display.print("Ready");
    // Time TO START label
    _display.setFont(&Org_01);
    _display.setCursor(72, 45);
    _display.print("TO START");
    _display.display();
  }

  _display.display();  
}


void DisplayManager::printBootMessage(){
  _display.setTextColor(SSD1306_WHITE);

  _display.setFont(&FreeSans9pt7b);
  _display.setCursor(10, 40);
  _display.print("Waking up...");

  _display.display();  
}

void DisplayManager::drawProfileMenu() {
  _clearContent();
  _display.setFont(nullptr);
  _display.setTextSize(1);
  _display.setTextColor(SSD1306_WHITE);

  int x = 0;
  int y = STATUS_BAR_HEIGHT + 4;
  _display.setCursor(x, y);
  _display.print("> PLA");

  y = y + DEFAULT_CHAR_HEIGHT + 4;
  _display.setCursor(x, y);
  _display.print("> PLA+");

  y = y + DEFAULT_CHAR_HEIGHT + 4;
  _display.setCursor(x, y);
  _display.print("> PLA HS");  

  x = SCREEN_WIDTH / 2;

  y = STATUS_BAR_HEIGHT + 4;
  _display.setCursor(x, y);
  _display.print("> PETG");

  y = y + DEFAULT_CHAR_HEIGHT + 4;
  _display.setCursor(x, y);
  _display.print("> ABS");

  y = y + DEFAULT_CHAR_HEIGHT + 8;
  _display.setFont(&Org_01);
  _display.setCursor(SCREEN_WIDTH - 36, SCREEN_HEIGHT - STATUS_BAR_HEIGHT - 5);
  _display.print("60C 2h");

  _display.display();  
}

void DisplayManager::_printRightAlignedText(const unsigned int value, int16_t rightEdgeX, int16_t baselineY) {
  char text[8];
  snprintf(text, sizeof(text), "%u", value);

  _printRightAlignedText(text, rightEdgeX, baselineY);
}

void DisplayManager::_printRightAlignedText(const char* text, int16_t rightEdgeX, int16_t baselineY) {
  if (text == nullptr || text[0] == '\0') {
    return;
  }

  int16_t x1 = 0;
  int16_t y1 = 0;
  uint16_t measuredWidth = 0;
  uint16_t measuredHeight = 0;
  
  // Measure at x = 0 so proximity to the right edge never wraps or distorts width
  _display.getTextBounds(text, 0, baselineY, &x1, &y1, &measuredWidth, &measuredHeight);

  const int16_t cursorX = static_cast<int16_t>(rightEdgeX - static_cast<int16_t>(measuredWidth));
  _display.setCursor(cursorX, baselineY);
  _display.print(text);
}