#pragma once

#include <Arduino.h>
#include <WiFi.h>

class RemoteLogger : public Print {
 public:
  bool setup();
  void update();

  size_t write(uint8_t value) override;
  size_t write(const uint8_t* buffer, size_t size) override;

 private:
  static constexpr uint8_t MAX_CLIENTS = 4;

  WiFiServer _server{23, MAX_CLIENTS};
  WiFiClient _clients[MAX_CLIENTS];
  bool _started = false;
  bool _serverStarted = false;
};
