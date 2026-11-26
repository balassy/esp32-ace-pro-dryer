# ESP32 ACE Pro Dryer - Work in progress!

Turn your Anycubic ACE Pro into a standalone filament dryer with an ESP32.

![Arduino](https://img.shields.io/badge/Arduino-00979D?logo=Arduino&logoColor=white) [![Build](https://github.com/balassy/esp32-ace-pro-dryer/actions/workflows/compile.yml/badge.svg)](https://github.com/balassy/esp32-ace-pro-dryer/actions/workflows/compile.yml)

## Hardware 🪛

### Anycubic ACE Pro

> **This app was built with Anycubic ACE Pro version 1 (aka. Original ACE Pro), it was not tested with an ACE 2 Pro.**

The ACE Pro version 1 communicates as a USB CDC device at 115200 baud with `VID=0x28E9` and `PID=0x018A`.

Values returned by the `get_info` method:

```
Device model: Anycubic Color Engine Pro
Firmware: V1.3.863
Boot firmware: V1.0.1
```

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

Make sure that the `nc-gcode` extension by `mietek` is not enabled, as it breaks the syntax highlighting for the `.h` header files.

### Dryer control

The ESP32 starts the USB host at boot and shows the ACE Pro connection state in the right side of the main status bar. If the ACE Pro is already on, the indicator shows connected once enumeration finishes. If it is powered on later, the host detects it and the indicator updates. The ACE Pro drops the USB link after about 3 seconds without traffic. The ESP32 releases the USB host after about 1 second without a command, before that drop can stall the host. Selecting **Start** calls `get_status` only and waits for the response; it does not start drying. When the link is down, that call starts the USB host again and waits for the ACE Pro to enumerate before sending `get_status`. A **Stop** button is available on the main screen while the dryer status reports that drying is active. While the main page is showing, and no command is already running, the loop requests dryer status every 10 seconds and updates the current temperature.

### Layers and classes

- `AceProClient` sends the method with `sendAceProMethod` and parses the JSON left in `responsePayload`. Framing stays in `exchangeAceProCommand`, and this layer does not reconnect.
- `AceProJobManager` gets a new `Job` value, plus a log case in `request()`. Only one job runs, and `take()` is what returns the manager to idle.
- `AceProManager` stores the result, runs the client call through `callWithReconnect`, and exposes `request…()` / `take…()`.
- The sketch queues the job from the UI and calls `take…()` on every `loop()` pass before starting another command. Skipping `take…()` leaves the job in `RESULT_READY`, so later requests stay rejected even though `isCommandRunning()` is `false`.

### Adding an ACE Pro command

A command crosses three layers. `AceProClient` builds the frame and reads the response. `AceProJobManager` runs that work on the `aceCmd` task so `loop()` keeps updating the display. `AceProManager` reconnects once when the link is down and stores the result until the loop collects it. `exchangeAceProCommand` already wraps the JSON payload as `FF AA`, little-endian length, CRC16-MCRF4XX, `FE`. A new command only supplies the method name, optional `params` JSON, and a parser for the response.

1. Add a method on `AceProClient`, following `startDrying()` and `getDryerStatus()`. Call `sendAceProMethod` with the protocol method string, the `params` JSON or `nullptr` when the command has no params, and a short operation name used in the logs. The validated response JSON is left in `responsePayload`. Parse it there and return `Error`. This method must not reconnect; when the ACE Pro is absent it returns `Error::NOT_CONNECTED`.

2. Add a value to `AceProJobManager::Job` and a matching case in `AceProJobManager::request()` so the queued job is named in the log. Only one job runs at a time. `request()` accepts a job only while the manager is idle, and `take()` is what returns it to idle after the result is stored.

3. In `AceProManager`, store the result in a field, handle the new job in `_executeJob()`, and run the client method through `callWithReconnect`. That helper sends on the current link, and on `NOT_CONNECTED` or `TIMEOUT` it restarts USB and sends once more, then releases the host. Expose `request…()` and `take…()` the same way as `requestStart()` and `takeStart()`. `request…()` only queues the job and returns. `take…()` copies the stored result and succeeds only for that job when its state is `RESULT_READY`.

4. From a button or other UI handler, call `request…()`. In `loop()`, call `take…()` on every pass, before any code that starts another command, and update the display from the result. Skip `take…()` and the finished job stays `RESULT_READY`. `isCommandRunning()` is then false, but every later `request…()` is still rejected.

Keep the USB exchange on the command task. Calling `AceProClient` directly from `loop()` blocks the display for the whole write and response.

### Remote logging

The device connects to the configured local Wi-Fi network and serves live log output to up to four TCP clients on port 23. Put `WIFI_SSID` and `WIFI_PASSWORD` in the ignored `config.secrets.h` file; `config.h` uses empty placeholder values when that file is absent. Find the device's assigned IP address in your router's DHCP client list or in the Serial log, then connect to that IP and port 23 with a TCP client such as PuTTY in Raw mode. Logging starts on Serial and switches to the remote logger when ACE Pro setup is requested; the switch is announced on Serial first.

## About the author

This project is created and maintained by [György Balássy](https://www.linkedin.com/in/balassy).