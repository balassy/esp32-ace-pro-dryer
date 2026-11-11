#pragma once

inline constexpr long ENCODER_MIN_VALUE = 30;
inline constexpr long ENCODER_MAX_VALUE = 80;

// Hardware settings.
inline constexpr unsigned long SERIAL_BAUD_RATE = 115200;
inline constexpr uint8_t RGB_LED_BRIGHTNESS = 1; // RGB LED brightness level (0-255)

// Pin configuration.
// Board reference: https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s3/esp32-s3-devkitc-1/user_guide_v1.0.html#pin-layout
// Safe pins reference: https://randomnerdtutorials.com/esp32-pinout-reference-gpios/

// Encoder pins:
#if defined(CONFIG_IDF_TARGET_ESP32S3)
inline constexpr int ENCODER_A_CLK_PIN = 4;
inline constexpr int ENCODER_B_DT_PIN = 5;
inline constexpr int ENCODER_SW_PIN = 6;
inline constexpr int ENCODER_VCC_PIN = 7;
#elif defined(CONFIG_IDF_TARGET_ESP32)
inline constexpr int ENCODER_A_CLK_PIN = 33;
inline constexpr int ENCODER_B_DT_PIN = 25;
inline constexpr int ENCODER_SW_PIN = 26;
inline constexpr int ENCODER_VCC_PIN = 27;
#else
#error "Unsupported ESP32 target for encoder pin configuration!"
#endif

// Display pins:
inline constexpr uint8_t DISPLAY_ADDRESS = 0x3C; // I2C address for the SSD1306 display
#if defined(CONFIG_IDF_TARGET_ESP32S3)
inline constexpr int DISPLAY_SDA_PIN = 8;
inline constexpr int DISPLAY_SCL_PIN = 9;
#elif defined(CONFIG_IDF_TARGET_ESP32)
inline constexpr int DISPLAY_SDA_PIN = 21;
inline constexpr int DISPLAY_SCL_PIN = 22;
#else
#error "Unsupported ESP32 target for display pin configuration!"
#endif

// ACE Pro USB pins:
inline constexpr int USB_D_PLUS_PIN = 20;
inline constexpr int USB_D_MINUS_PIN = 19;