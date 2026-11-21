#pragma once

#include <Arduino.h>
#include <WiFi.h>

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

class RemoteLogger : public Print {
 public:
  using ConnectionStatusCallback = bool (*)();

  void setConnectionStatusCallback(ConnectionStatusCallback callback);
  bool setup();
  void update();

  size_t write(uint8_t value) override;
  size_t write(const uint8_t* buffer, size_t size) override;

 private:
  static constexpr uint8_t MAX_CLIENTS = 4;
  static constexpr uint8_t MAX_MESSAGES = 30;
  static constexpr size_t MAX_MESSAGE_LENGTH = 192;

  void appendToHistory(const uint8_t* buffer, size_t size);
  void writeHistory(WiFiClient& client);

  void lock();
  void unlock();

  WiFiServer _server{23, MAX_CLIENTS};
  WiFiClient _clients[MAX_CLIENTS];
  bool _started = false;
  bool _serverStarted = false;
  ConnectionStatusCallback _connectionStatusCallback = nullptr;
  char _messages[MAX_MESSAGES][MAX_MESSAGE_LENGTH] = {};
  uint8_t _messageCount = 0;
  uint8_t _messageNext = 0;
  char _pendingMessage[MAX_MESSAGE_LENGTH] = {};
  size_t _pendingMessageLength = 0;
  SemaphoreHandle_t _lock = nullptr;
};
