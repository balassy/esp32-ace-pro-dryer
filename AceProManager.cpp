#include "AceProManager.h"

#include <Arduino.h>

#include <USBHost.h>
#include <USBHostSerial.h>

#include <cstdlib>
#include <cstring>

#include "config.h"

namespace {

USBHostSerialClass aceProUsbSerial;

constexpr unsigned long USB_CONNECTION_TIMEOUT_MS = 5000;
constexpr unsigned long USB_RESPONSE_TIMEOUT_MS = 3000;
constexpr size_t MAX_RESPONSE_PAYLOAD_SIZE = 512;
constexpr uint8_t GET_INFO_REQUEST_ID = 1;

uint16_t calculateCrc16Mcrf4xx(const uint8_t* data, size_t length) {
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < length; ++i) {
    crc ^= data[i];
    for (uint8_t bit = 0; bit < 8; ++bit) {
      crc = (crc & 1) != 0 ? (crc >> 1) ^ 0x8408 : crc >> 1;
    }
  }
  return crc;
}

bool getJsonIntegerField(const char* json, const char* name, long& value) {
  char key[16];
  const size_t nameLength = strlen(name);
  if (nameLength + 3 > sizeof(key)) {
    return false;
  }

  key[0] = '"';
  memcpy(key + 1, name, nameLength);
  key[nameLength + 1] = '"';
  key[nameLength + 2] = '\0';

  const char* field = strstr(json, key);
  if (field == nullptr) {
    return false;
  }

  field += nameLength + 2;
  while (*field == ' ' || *field == '\t' || *field == '\r' || *field == '\n') {
    ++field;
  }
  if (*field++ != ':') {
    return false;
  }
  while (*field == ' ' || *field == '\t' || *field == '\r' || *field == '\n') {
    ++field;
  }

  char* end = nullptr;
  value = strtol(field, &end, 10);
  return end != field &&
         (*end == ',' || *end == '}' || *end == ' ' || *end == '\t' || *end == '\r' || *end == '\n');
}

bool getJsonStringField(const char* json, const char* name, char* value, size_t capacity) {
  char key[16];
  const size_t nameLength = strlen(name);
  if (nameLength + 3 > sizeof(key) || capacity == 0) {
    return false;
  }

  key[0] = '"';
  memcpy(key + 1, name, nameLength);
  key[nameLength + 1] = '"';
  key[nameLength + 2] = '\0';

  const char* field = strstr(json, key);
  if (field == nullptr) {
    return false;
  }

  field += nameLength + 2;
  while (*field == ' ' || *field == '\t' || *field == '\r' || *field == '\n') {
    ++field;
  }
  if (*field++ != ':') {
    return false;
  }
  while (*field == ' ' || *field == '\t' || *field == '\r' || *field == '\n') {
    ++field;
  }
  if (*field++ != '"') {
    return false;
  }

  size_t valueLength = 0;
  while (*field != '\0' && *field != '"') {
    char character = *field++;
    if (character == '\\') {
      character = *field++;
      if (character == '\0') {
        return false;
      }
      if (character == 'n') {
        character = '\n';
      } else if (character == 'r') {
        character = '\r';
      } else if (character == 't') {
        character = '\t';
      }
    }

    if (valueLength + 1 >= capacity) {
      return false;
    }
    value[valueLength++] = character;
  }

  if (*field != '"') {
    return false;
  }
  value[valueLength] = '\0';
  return true;
}

enum class ResponseReadResult {
  RECEIVED,
  TIMEOUT,
  INVALID_FRAME
};

ResponseReadResult readResponseFrame(uint8_t* payload, size_t payloadCapacity, size_t& payloadLength) {
  enum class ReadState {
    HEADER_FF,
    HEADER_AA,
    LENGTH_LOW,
    LENGTH_HIGH,
    PAYLOAD,
    CRC_LOW,
    CRC_HIGH,
    TERMINATOR
  };

  ReadState state = ReadState::HEADER_FF;
  uint8_t lengthLow = 0;
  uint16_t expectedCrc = 0;
  uint16_t receivedCrc = 0;
  size_t payloadIndex = 0;
  payloadLength = 0;
  const unsigned long startTime = millis();

  while (millis() - startTime < USB_RESPONSE_TIMEOUT_MS) {
    if (aceProUsbSerial.available() <= 0) {
      USBHost.task();
      delay(1);
      continue;
    }

    const int input = aceProUsbSerial.read();
    if (input < 0) {
      continue;
    }
    const uint8_t byte = static_cast<uint8_t>(input);

    switch (state) {
      case ReadState::HEADER_FF:
        if (byte == 0xFF) {
          state = ReadState::HEADER_AA;
        }
        break;
      case ReadState::HEADER_AA:
        state = byte == 0xAA ? ReadState::LENGTH_LOW : (byte == 0xFF ? ReadState::HEADER_AA : ReadState::HEADER_FF);
        break;
      case ReadState::LENGTH_LOW:
        lengthLow = byte;
        state = ReadState::LENGTH_HIGH;
        break;
      case ReadState::LENGTH_HIGH:
        payloadLength = static_cast<size_t>(lengthLow | (static_cast<uint16_t>(byte) << 8));
        if (payloadLength == 0 || payloadLength > payloadCapacity) {
          return ResponseReadResult::INVALID_FRAME;
        }
        payloadIndex = 0;
        state = ReadState::PAYLOAD;
        break;
      case ReadState::PAYLOAD:
        payload[payloadIndex++] = byte;
        if (payloadIndex == payloadLength) {
          state = ReadState::CRC_LOW;
        }
        break;
      case ReadState::CRC_LOW:
        receivedCrc = byte;
        state = ReadState::CRC_HIGH;
        break;
      case ReadState::CRC_HIGH:
        receivedCrc |= static_cast<uint16_t>(byte) << 8;
        expectedCrc = calculateCrc16Mcrf4xx(payload, payloadLength);
        state = ReadState::TERMINATOR;
        break;
      case ReadState::TERMINATOR:
        if (byte == 0xFE) {
          return receivedCrc == expectedCrc ? ResponseReadResult::RECEIVED : ResponseReadResult::INVALID_FRAME;
        }
        break;
    }
  }

  return ResponseReadResult::TIMEOUT;
}

bool requestAceProInfo() {
  static constexpr char REQUEST_PAYLOAD[] = "{\"id\":1,\"method\":\"get_info\"}";
  constexpr size_t requestPayloadLength = sizeof(REQUEST_PAYLOAD) - 1;
  uint8_t requestFrame[sizeof(REQUEST_PAYLOAD) + 6];
  requestFrame[0] = 0xFF;
  requestFrame[1] = 0xAA;
  requestFrame[2] = static_cast<uint8_t>(requestPayloadLength & 0xFF);
  requestFrame[3] = static_cast<uint8_t>((requestPayloadLength >> 8) & 0xFF);
  memcpy(requestFrame + 4, REQUEST_PAYLOAD, requestPayloadLength);

  const uint16_t crc = calculateCrc16Mcrf4xx(
      reinterpret_cast<const uint8_t*>(REQUEST_PAYLOAD), requestPayloadLength);
  requestFrame[4 + requestPayloadLength] = static_cast<uint8_t>(crc & 0xFF);
  requestFrame[5 + requestPayloadLength] = static_cast<uint8_t>(crc >> 8);
  requestFrame[6 + requestPayloadLength] = 0xFE;

  while (aceProUsbSerial.available() > 0) {
    aceProUsbSerial.read();
  }

  Serial.println(F("AceProManager::setup: Sending get_info connectivity probe..."));
  if (aceProUsbSerial.write(requestFrame, sizeof(requestFrame)) != sizeof(requestFrame)) {
    Serial.println(F("AceProManager::setup: ERROR: Failed to send the get_info request."));
    return false;
  }
  aceProUsbSerial.flush();

  uint8_t responsePayload[MAX_RESPONSE_PAYLOAD_SIZE + 1];
  size_t responseLength = 0;
  const ResponseReadResult readResult =
      readResponseFrame(responsePayload, MAX_RESPONSE_PAYLOAD_SIZE, responseLength);
  if (readResult == ResponseReadResult::TIMEOUT) {
    Serial.println(F("AceProManager::setup: ERROR: Timed out waiting for an ACE Pro response."));
    return false;
  }
  if (readResult == ResponseReadResult::INVALID_FRAME) {
    Serial.println(F("AceProManager::setup: ERROR: Received an invalid ACE Pro response frame."));
    return false;
  }

  responsePayload[responseLength] = '\0';
  long responseId = -1;
  long responseCode = -1;
  const char* responseJson = reinterpret_cast<const char*>(responsePayload);
  if (!getJsonIntegerField(responseJson, "id", responseId) || responseId != GET_INFO_REQUEST_ID) {
    Serial.println(F("AceProManager::setup: ERROR: Response ID did not match the get_info request."));
    return false;
  }
  if (!getJsonIntegerField(responseJson, "code", responseCode) || responseCode != 0) {
    Serial.println(F("AceProManager::setup: ERROR: ACE Pro returned an unsuccessful get_info response."));
    return false;
  }

  char model[64];
  char firmware[32];
  char bootFirmware[32];
  if (!getJsonStringField(responseJson, "model", model, sizeof(model)) ||
      !getJsonStringField(responseJson, "firmware", firmware, sizeof(firmware)) ||
      !getJsonStringField(responseJson, "boot_firmware", bootFirmware, sizeof(bootFirmware))) {
    Serial.println(F("AceProManager::setup: ERROR: get_info response is missing device version information."));
    return false;
  }

  Serial.print(F("AceProManager::setup: Device model: "));
  Serial.println(model);
  Serial.print(F("AceProManager::setup: Firmware: "));
  Serial.println(firmware);
  Serial.print(F("AceProManager::setup: Boot firmware: "));
  Serial.println(bootFirmware);
  Serial.println(F("AceProManager::setup: ACE Pro USB CDC connection verified (get_info succeeded)."));
  return true;
}
}  // namespace

void AceProManager::setup() {
  Serial.println(F("AceProManager::setup: Initializing ACE Pro connection..."));
  Serial.print(F("AceProManager::setup: USB pins - D+: "));
  Serial.print(USB_D_PLUS_PIN);
  Serial.print(F(", D-: "));
  Serial.println(USB_D_MINUS_PIN);
  Serial.flush();

  if (!USBHost.begin()) {
    Serial.println(F("AceProManager::setup: ERROR: Failed to start the USB host."));
    Serial.flush();
    return;
  }

  Serial.println(F("AceProManager::setup: USB host started."));
  Serial.flush();

  _usbHostStarted = true;
  aceProUsbSerial.begin(115200);

  const unsigned long startTime = millis();
  while (!aceProUsbSerial && millis() - startTime < USB_CONNECTION_TIMEOUT_MS) {
    USBHost.task();
    delay(10);
  }
  if (!aceProUsbSerial) {
    Serial.println(F("AceProManager::setup: ERROR: No USB CDC device detected; check the ACE Pro and cable."));
    return;
  }

  Serial.println(F("AceProManager::setup: USB CDC device enumerated."));
  if (!requestAceProInfo()) {
    Serial.println(F("AceProManager::setup: ACE Pro communication check FAILED."));
    return;
  }

  Serial.println(F("AceProManager::setup: Initializing ACE Pro connection DONE."));
}

void AceProManager::update() {
  if (_usbHostStarted) {
    USBHost.task();
  }
}

AceProManager::Status AceProManager::getStatus() const {
  Serial.println(F("AceProManager::getStatus: Dryer status polling is not implemented; returning default status."));
  return _status;
}
