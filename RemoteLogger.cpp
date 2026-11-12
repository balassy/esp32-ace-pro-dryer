#include "RemoteLogger.h"

#include <cstring>

#include "config.h"

bool RemoteLogger::setup() {
  Serial.print(F("RemoteLogger::setup: Connecting to Wi-Fi SSID \""));
  Serial.print(WIFI_SSID);
  Serial.println(F("\"..."));

  if (_started) {
    return true;
  }
  if (strlen(WIFI_SSID) == 0) {
    Serial.println(F("RemoteLogger::setup: Failed to connect because Wi-Fi SSID is empty. Check config.h!"));
    return false;
  }
  if (!WiFi.mode(WIFI_STA)) {
    Serial.println(F("RemoteLogger::setup: Failed to set Wi-Fi mode."));
    return false;
  }

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  Serial.print(F("RemoteLogger::setup: Connecting to Wi-Fi DONE."));
  Serial.flush();
  
  _started = true;
  return true;
}

void RemoteLogger::update() {
  if (!_started) {
    return;
  }

  if (WiFi.status() != WL_CONNECTED) {
    if (_serverStarted) {
      _server.end();
      _serverStarted = false;
    }
    for (WiFiClient& client : _clients) {
      client.stop();
    }
    return;
  }

  if (!_serverStarted) {
    _server.begin();
    _server.setNoDelay(true);
    _serverStarted = true;

    Serial.print(F("RemoteLogger::update: Listening at "));
    Serial.print(WiFi.localIP());
    Serial.println(F(":23"));
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

    Serial.print(F("RemoteLogger::update: Client connected from "));
    Serial.println(incoming.remoteIP());

    _clients[slot] = incoming;
    _clients[slot].println(F("Welcome! Remote log stream connected."));
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
}

size_t RemoteLogger::write(uint8_t value) {
  return write(&value, 1);
}

size_t RemoteLogger::write(const uint8_t* buffer, size_t size) {
  if (buffer == nullptr || size == 0) {
    return 0;
  }

  bool delivered = false;
  for (WiFiClient& client : _clients) {
    if (client.connected() && client.write(buffer, size) == size) {
      delivered = true;
    } else if (client) {
      client.stop();
    }
  }

  return delivered ? size : 0;
}
