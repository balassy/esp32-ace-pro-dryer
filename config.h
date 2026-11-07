#pragma once

inline constexpr long ENCODER_MIN_VALUE = 30;
inline constexpr long ENCODER_MAX_VALUE = 80;

// Hardware settings.
inline constexpr unsigned long SERIAL_BAUD_RATE = 115200;

// Pin configuration
// Reference: https://randomnerdtutorials.com/esp32-pinout-reference-gpios/
inline constexpr int ENCODER_A_CLK_PIN = 33;
inline constexpr int ENCODER_B_DT_PIN = 25;
inline constexpr int ENCODER_SW_PIN = 26;
inline constexpr int ENCODER_VCC_PIN = 27;