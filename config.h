#pragma once

inline constexpr long ENCODER_MIN_VALUE = 30;
inline constexpr long ENCODER_MAX_VALUE = 80;

inline constexpr uint8_t RGB_LED_BRIGHTNESS = 1; // RGB LED brightness level (0-255)

// Hardware settings.
inline constexpr unsigned long SERIAL_BAUD_RATE = 115200;

// Pin configuration.
// Board reference: https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s3/esp32-s3-devkitc-1/user_guide_v1.0.html#pin-layout
// Pinout reference: https://raw.githubusercontent.com/rtek1000/YD-ESP32-23/main/yd-esp32-s3-devkitc-1-clone-pinout.jpg
// Safe pins reference: https://randomnerdtutorials.com/esp32-pinout-reference-gpios/

// Encoder pins:
inline constexpr int ENCODER_A_CLK_PIN = 4;
inline constexpr int ENCODER_B_DT_PIN = 5;
inline constexpr int ENCODER_SW_PIN = 6;
inline constexpr int ENCODER_VCC_PIN = 7;

// Display pins:
inline constexpr uint8_t DISPLAY_ADDRESS = 0x3C; // I2C address for the SSD1306 display
inline constexpr int DISPLAY_SDA_PIN = 8;
inline constexpr int DISPLAY_SCL_PIN = 9;

// ACE Pro USB pins:
inline constexpr int USB_D_PLUS_PIN = 20;
inline constexpr int USB_D_MINUS_PIN = 19;