# ESP32 ACE Pro Dryer - Work in progress!

Turn your Anycubic ACE Pro into a standalone filament dryer with an ESP32.

![Arduino](https://img.shields.io/badge/Arduino-00979D?logo=Arduino&logoColor=white) [![Build](https://github.com/balassy/esp32-ace-pro-dryer/actions/workflows/compile.yml/badge.svg)](https://github.com/balassy/esp32-ace-pro-dryer/actions/workflows/compile.yml)

## Hardware 🪛

### Components 🔩

- [ESP32-S3-DevKitC-1 microcontroller](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s3/esp32-s3-devkitc-1/index.html) (dual USB)
- SSD1306 0.96" OLED display module with 128×64 pixel resolution (GME12864-12)
- KY-040 rotary encoder (with resistors)

## Software 💻

### Getting Started 🚀

Install the [Arduino Maker Workshop](https://marketplace.visualstudio.com/items?itemName=TheLastOutpostWorkshop.arduino-maker-workshop) Visual Studio Code extension.

Install the following libraries:
- [Adafruit BusIO](https://github.com/adafruit/Adafruit_BusIO)
- [Adafruit GFX Library](https://github.com/adafruit/Adafruit-GFX-Library)
- [Adafruit SSD1306](https://github.com/adafruit/Adafruit_SSD1306)
- [ESP32RotaryEncoder by Matthew Clark](https://github.com/MaffooClock/ESP32RotaryEncoder) 

The exact versions of these libraries used to create this code are visible in the [sketch.yaml](./sketch.yaml) file.

### Dryer control

Selecting **Start** sends the ACE Pro a drying command for 45 °C, 30 minutes, and a 1000 RPM fan speed. While connected, the ESP32 requests dryer status every 10 seconds. Verify the settings and operation on the ACE Pro during initial hardware testing.

### Remote logging

The device connects to the configured local Wi-Fi network and serves live log output to up to four TCP clients on port 23. Put `WIFI_SSID` and `WIFI_PASSWORD` in the ignored `config.secrets.h` file; `config.h` uses empty placeholder values when that file is absent. Find the device's assigned IP address in your router's DHCP client list or in the Serial log, then connect to that IP and port 23 with a TCP client such as PuTTY in Raw mode. Logging starts on Serial and switches to the remote logger when ACE Pro setup is requested; the switch is announced on Serial first.

## About the author

This project is created and maintained by [György Balássy](https://www.linkedin.com/in/balassy).