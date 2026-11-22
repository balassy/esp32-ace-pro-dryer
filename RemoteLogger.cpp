#include "RemoteLogger.h"

#include <cstring>
#include <ESPmDNS.h>

#include "config.h"
#include "LogRouter.h"

bool RemoteLogger::setup() {
  logRouter.print(F("RemoteLogger::setup: Connecting to Wi-Fi SSID \""));
  logRouter.print(WIFI_SSID);
  logRouter.println(F("\"..."));

  if (_started) {
    return true;
  }
  if (strlen(WIFI_SSID) == 0) {
    logRouter.println(F("RemoteLogger::setup: Failed to connect because Wi-Fi SSID is empty. Check config.h!"));
    return false;
  }
  if (!WiFi.mode(WIFI_STA)) {
    logRouter.println(F("RemoteLogger::setup: Failed to set Wi-Fi mode."));
    return false;
  }

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  logRouter.println(F("RemoteLogger::setup: Connecting to Wi-Fi...DONE"));

  logRouter.println(F("RemoteLogger::setup: Setting up mDNS..."));
  if (!MDNS.begin(WIFI_HOSTNAME)) {
    logRouter.println(F("RemoteLogger::setup: Setting up mDNS...FAILED!"));
  }
  MDNS.addService("telnet", "tcp", 23);
  logRouter.println(F("RemoteLogger::setup: Setting up mDNS... DONE"));

  _started = true;
  return true;
}

void RemoteLogger::update() {
  if (!_started) {
    return;
  }
  
  _lock();

  if (WiFi.status() != WL_CONNECTED) {
    if (_serverStarted) {
      _server.end();
      _serverStarted = false;
    }
    for (WiFiClient& client : _clients) {
      client.stop();
    }
    _unlock();
    return;
  }

  if (!_serverStarted) {
    _server.begin();
    _server.setNoDelay(true);
    _serverStarted = true;

    const IPAddress address = WiFi.localIP();
    _unlock();
    logRouter.print(F("RemoteLogger::update: Listening at "));
    logRouter.print(address);
    logRouter.println(F(":23"));
    _lock();
  }

  for (uint8_t accepted = 0; accepted < MAX_CLIENTS && _server.hasClient(); ++accepted) {
    WiFiClient incoming = _server.accept();
    if (!incoming) {
      continue;
    }

    uint8_t slot = 0;
    while (slot < MAX_CLIENTS && _clients[slot].connected()) {
      ++slot;
    }
    if (slot == MAX_CLIENTS) {
      incoming.stop();
      continue;
    }

    _clients[slot] = incoming;
    _writeHistory(_clients[slot]);
    const IPAddress remoteAddress = incoming.remoteIP();
    _unlock();
    logRouter.print(F("RemoteLogger::update: Client connected from "));
    logRouter.println(remoteAddress);
    _lock();
    _clients[slot].println(F("RemoteLogger::update: Welcome! Remote log stream connected."));
    if (_connectionStatusCallback != nullptr) {
      _clients[slot].print(F("RemoteLogger::update: ACE Pro connection status: "));
      _clients[slot].println(_connectionStatusCallback() ? F("connected.") : F("disconnected."));
    }
  }

  for (WiFiClient& client : _clients) {
    if (!client.connected()) {
      client.stop();
      continue;
    }

    for (uint8_t discarded = 0; discarded < 64 && client.available() > 0; ++discarded) {
      client.read();
    }
  }
  _unlock();
}


void RemoteLogger::setConnectionStatusCallback(ConnectionStatusCallback callback) {
  _connectionStatusCallback = callback;
}

void RemoteLogger::_lock() {
  if (_mutex == nullptr) {
    _mutex = xSemaphoreCreateRecursiveMutex();
  }
  if (_mutex != nullptr) {
    xSemaphoreTakeRecursive(_mutex, portMAX_DELAY);
  }
}

size_t RemoteLogger::write(uint8_t value) {
  return write(&value, 1);
}

size_t RemoteLogger::write(const uint8_t* buffer, size_t size) {
  if (buffer == nullptr || size == 0) {
    return 0;
  }

  _lock();
  _appendToHistory(buffer, size);

  bool delivered = false;
  for (WiFiClient& client : _clients) {
    if (client.connected() && client.write(buffer, size) == size) {
      delivered = true;
    } else if (client) {
      client.stop();
    }
  }

  _unlock();
  return delivered ? size : 0;
}

void RemoteLogger::_unlock() {
  if (_mutex != nullptr) {
    xSemaphoreGiveRecursive(_mutex);
  }
}

void RemoteLogger::_appendToHistory(const uint8_t* buffer, size_t size) {
  for (size_t i = 0; i < size; ++i) {
    const char character = static_cast<char>(buffer[i]);
    if (character == '\r') {
      continue;
    }
    if (character == '\n') {
      _pendingMessage[_pendingMessageLength] = '\0';
      std::strncpy(_messages[_messageNext], _pendingMessage, MAX_MESSAGE_LENGTH);
      _messages[_messageNext][MAX_MESSAGE_LENGTH - 1] = '\0';
      _messageNext = (_messageNext + 1) % MAX_MESSAGES;
      if (_messageCount < MAX_MESSAGES) {
        ++_messageCount;
      }
      _pendingMessageLength = 0;
      _pendingMessage[0] = '\0';
      continue;
    }
    if (_pendingMessageLength + 1 < MAX_MESSAGE_LENGTH) {
      _pendingMessage[_pendingMessageLength++] = character;
      _pendingMessage[_pendingMessageLength] = '\0';
    }
  }
}

void RemoteLogger::_writeHistory(WiFiClient& client) {
  const uint8_t oldestIndex =
      (_messageNext + MAX_MESSAGES - _messageCount) % MAX_MESSAGES;
  for (uint8_t i = 0; i < _messageCount; ++i) {
    const uint8_t index = (oldestIndex + i) % MAX_MESSAGES;
    client.println(_messages[index]);
  }
}
