#include "AceProManager.h"

#include <Arduino.h>

#include <EspUsbHost.h>

#include <climits>
#include <cstdlib>
#include <cstring>

#include <freertos/FreeRTOS.h>
#include <freertos/stream_buffer.h>
#include <freertos/task.h>

namespace {

EspUsbHost aceUsbHost;
EspUsbHostCdcSerial aceProUsbSerial(aceUsbHost);
volatile bool aceProConnected = false;
volatile bool aceProSerialOpenFailed = false;

constexpr unsigned long USB_CONNECTION_TIMEOUT_MS = 5000;
constexpr unsigned long USB_DIAGNOSTIC_INTERVAL_MS = 1000;
constexpr unsigned long USB_RESPONSE_TIMEOUT_MS = 10000;
constexpr unsigned long USB_STATUS_PROGRESS_INTERVAL_MS = 250;
constexpr size_t MAX_RESPONSE_PAYLOAD_SIZE = 1024;
constexpr size_t USB_CDC_RX_BUFFER_SIZE = 2048;
constexpr size_t USB_RX_STREAM_BUFFER_SIZE = 4096;
constexpr uint8_t GET_INFO_REQUEST_ID = 1;
constexpr uint8_t START_DRYING_REQUEST_ID = 3;

StreamBufferHandle_t usbRxStreamBuffer = nullptr;
TaskHandle_t usbRxTaskHandle = nullptr;
volatile bool usbRxBufferOverflow = false;
volatile uint32_t usbRxTaskBytes = 0;
volatile uint32_t usbRxTaskDroppedBytes = 0;
volatile uint32_t usbRxLastByteTime = 0;
volatile uint32_t usbRxMaxAvailable = 0;
uint32_t usbStatusRequestStartTime = 0;

void usbRxTask(void*) {
  uint8_t inputBuffer[128];
  for (;;) {
    if (!aceProConnected || !aceProUsbSerial.connected()) {
      vTaskDelay(pdMS_TO_TICKS(10));
      continue;
    }

    const int availableBytes = aceProUsbSerial.available();
    if (availableBytes <= 0) {
      vTaskDelay(1);
      continue;
    }
    if (static_cast<uint32_t>(availableBytes) > usbRxMaxAvailable) {
      usbRxMaxAvailable = static_cast<uint32_t>(availableBytes);
    }

    const size_t requestedBytes =
        availableBytes < static_cast<int>(sizeof(inputBuffer))
            ? static_cast<size_t>(availableBytes)
            : sizeof(inputBuffer);
    size_t receivedBytes = 0;
    while (receivedBytes < requestedBytes) {
      const int value = aceProUsbSerial.read();
      if (value < 0) {
        break;
      }
      inputBuffer[receivedBytes++] = static_cast<uint8_t>(value);
    }
    if (receivedBytes == 0) {
      vTaskDelay(1);
      continue;
    }

    const size_t bufferedBytes =
        xStreamBufferSend(usbRxStreamBuffer, inputBuffer, receivedBytes, 0);
    usbRxTaskBytes += static_cast<uint32_t>(receivedBytes);
    usbRxLastByteTime = millis();

    taskYIELD();

    if (bufferedBytes != receivedBytes) {
      usbRxTaskDroppedBytes += static_cast<uint32_t>(receivedBytes - bufferedBytes);
      usbRxBufferOverflow = true;
    }
  }
}

bool startUsbRxTask(Print& logger) {
  if (usbRxTaskHandle != nullptr) {
    return true;
  }

  usbRxStreamBuffer = xStreamBufferCreate(USB_RX_STREAM_BUFFER_SIZE, 1);
  if (usbRxStreamBuffer == nullptr) {
    logger.println(F("AceProManager::setup: ERROR: Failed to allocate USB response buffer."));
    return false;
  }

  const BaseType_t taskCreated =
      xTaskCreate(usbRxTask, "aceUsbRx", 3072, nullptr, 6, &usbRxTaskHandle);
  if (taskCreated != pdPASS) {
    usbRxTaskHandle = nullptr;
    vStreamBufferDelete(usbRxStreamBuffer);
    usbRxStreamBuffer = nullptr;
    logger.println(F("AceProManager::setup: ERROR: Failed to start USB receive task."));
    return false;
  }

  return true;
}

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

bool getJsonObjectField(char* json, const char* name, char*& object) {
  char key[32];
  const size_t nameLength = strlen(name);
  if (nameLength + 3 > sizeof(key)) {
    return false;
  }

  key[0] = '"';
  memcpy(key + 1, name, nameLength);
  key[nameLength + 1] = '"';
  key[nameLength + 2] = '\0';

  char* field = strstr(json, key);
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
  if (*field != '{') {
    return false;
  }

  object = field;
  size_t depth = 0;
  bool inString = false;
  bool escaped = false;
  for (; *field != '\0'; ++field) {
    if (inString) {
      if (escaped) {
        escaped = false;
      } else if (*field == '\\') {
        escaped = true;
      } else if (*field == '"') {
        inString = false;
      }
      continue;
    }

    if (*field == '"') {
      inString = true;
    } else if (*field == '{') {
      ++depth;
    } else if (*field == '}' && --depth == 0) {
      field[1] = '\0';
      return true;
    }
  }

  return false;
}

enum class ResponseReadResult {
  RECEIVED,
  TIMEOUT,
  INVALID_FRAME
};

ResponseReadResult readResponseFrameDynamic(uint8_t* frameBuffer,
                                            size_t bufferCapacity,
                                            size_t& totalBytesRead,
                                            size_t& payloadLength,
                                            Print& logger) {
  totalBytesRead = 0;
  payloadLength = 0;
  size_t expectedTotalFrame = 0;
  const unsigned long startTime = millis();
  unsigned long lastProgressLogTime = startTime;

  while (millis() - startTime < USB_RESPONSE_TIMEOUT_MS) {
    if (totalBytesRead >= 4 && expectedTotalFrame == 0) {
      if (frameBuffer[0] != 0xFF || frameBuffer[1] != 0xAA) {
        return ResponseReadResult::INVALID_FRAME;
      }

      payloadLength = static_cast<size_t>(
          frameBuffer[2] | (static_cast<uint16_t>(frameBuffer[3]) << 8));
      expectedTotalFrame = payloadLength + 7;
      if (payloadLength == 0 || expectedTotalFrame > bufferCapacity) {
        return ResponseReadResult::INVALID_FRAME;
      }
    }

    if (expectedTotalFrame != 0 && totalBytesRead == expectedTotalFrame) {
      return ResponseReadResult::RECEIVED;
    }

    const size_t targetBytes = expectedTotalFrame == 0 ? 4 : expectedTotalFrame;
    if (totalBytesRead >= targetBytes) {
      return ResponseReadResult::INVALID_FRAME;
    }
    const size_t requestedBytes = targetBytes - totalBytesRead;
    const size_t receivedBytes =
        xStreamBufferReceive(usbRxStreamBuffer, frameBuffer + totalBytesRead,
                             requestedBytes, pdMS_TO_TICKS(1));
    totalBytesRead += receivedBytes;
    vTaskDelay(1);

    const unsigned long now = millis();
    if (now - lastProgressLogTime >= USB_STATUS_PROGRESS_INTERVAL_MS) {
      lastProgressLogTime = now;
      logger.print(F("AceProManager::getStatus: Progress at "));
      logger.print(now - usbStatusRequestStartTime);
      logger.print(F(" ms: frame bytes read "));
      logger.print(totalBytesRead);
      logger.print(F(", USB reader bytes received "));
      logger.print(usbRxTaskBytes);
      logger.print(F(", CDC available "));
      logger.print(aceProUsbSerial.available());
      logger.print(F(", mounted "));
      logger.println(aceProUsbSerial.connected() ? F("yes.") : F("no."));
    }
  }

  return ResponseReadResult::TIMEOUT;
}

bool validateResponseFrame(const uint8_t* frameBuffer,
                           size_t totalBytesRead,
                           size_t payloadLength) {
  const size_t expectedTotalFrame = payloadLength + 7;
  if (totalBytesRead != expectedTotalFrame || frameBuffer[0] != 0xFF ||
      frameBuffer[1] != 0xAA || frameBuffer[expectedTotalFrame - 1] != 0xFE) {
    return false;
  }

  const uint16_t receivedCrc =
      static_cast<uint16_t>(frameBuffer[4 + payloadLength]) |
      (static_cast<uint16_t>(frameBuffer[5 + payloadLength]) << 8);
  return receivedCrc == calculateCrc16Mcrf4xx(frameBuffer + 4, payloadLength);
}

void logReceivedPayloadAsText(Print& logger,
                              const uint8_t* frameBuffer,
                              size_t totalBytesRead,
                              size_t payloadLength) {
  if (totalBytesRead <= 4 || frameBuffer[0] != 0xFF || frameBuffer[1] != 0xAA) {
    logger.println(F("AceProManager::getStatus: No complete JSON payload available to log."));
    return;
  }

  const size_t availablePayloadLength = totalBytesRead - 4;
  const size_t textLength =
      availablePayloadLength < payloadLength ? availablePayloadLength : payloadLength;
  char textBuffer[1024];
  for (size_t i = 0; i < textLength; ++i) {
    const uint8_t byte = frameBuffer[4 + i];
    textBuffer[i] = byte >= 0x20 && byte <= 0x7E ? static_cast<char>(byte) : '.';
  }

  logger.print(F("AceProManager::getStatus: Received partial JSON payload ("));
  logger.print(textLength);
  logger.println(F(" byte(s)):"));
  logger.write(reinterpret_cast<const uint8_t*>(textBuffer), textLength);
  logger.println();
}

ResponseReadResult readResponseFrame(uint8_t* payload,
                                     size_t payloadCapacity,
                                     size_t& payloadLength,
                                     size_t& bytesRead,
                                     Print& logger,
                                     const char* operation,
                                     uint8_t* firstBytes,
                                     size_t firstBytesCapacity,
                                     size_t& firstBytesLength,
                                     uint8_t* lastBytes,
                                     size_t lastBytesCapacity,
                                     size_t& lastBytesLength,
                                     size_t& nextLastByteIndex) {
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
  bytesRead = 0;
  firstBytesLength = 0;
  lastBytesLength = 0;
  nextLastByteIndex = 0;
  const unsigned long startTime = millis();
  uint8_t inputBuffer[64];

  while (millis() - startTime < USB_RESPONSE_TIMEOUT_MS) {
    const size_t receivedBytes =
        xStreamBufferReceive(usbRxStreamBuffer, inputBuffer, sizeof(inputBuffer), 1);
    if (receivedBytes == 0) {
      continue;
    }

    for (size_t byteIndex = 0; byteIndex < receivedBytes; ++byteIndex) {
      const uint8_t byte = inputBuffer[byteIndex];
      ++bytesRead;
      if (firstBytesLength < firstBytesCapacity) {
        firstBytes[firstBytesLength++] = byte;
      }
      if (lastBytesCapacity > 0) {
        lastBytes[nextLastByteIndex] = byte;
        nextLastByteIndex = (nextLastByteIndex + 1) % lastBytesCapacity;
        if (lastBytesLength < lastBytesCapacity) {
          ++lastBytesLength;
        }
      }

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
  }

  return ResponseReadResult::TIMEOUT;
}

void logResponseTimeout(Print& logger,
                        const char* operation,
                        size_t bytesRead,
                        size_t payloadLength,
                        const uint8_t* firstBytes,
                        size_t firstBytesLength,
                        const uint8_t* lastBytes,
                        size_t lastBytesCapacity,
                        size_t lastBytesLength,
                        size_t nextLastByteIndex) {
  logger.print(F("AceProManager::"));
  logger.print(operation);
  logger.print(F(": Timed out after receiving "));
  logger.print(bytesRead);
  logger.println(F(" USB byte(s) without a complete response frame."));
  if (payloadLength != 0) {
    logger.print(F("AceProManager::"));
    logger.print(operation);
    logger.print(F(": Frame declares "));
    logger.print(payloadLength);
    logger.print(F(" payload bytes; expected "));
    logger.print(payloadLength + 7);
    logger.println(F(" total frame bytes."));
  }
  if (firstBytesLength == 0) {
    return;
  }

  logger.print(F("AceProManager::"));
  logger.print(operation);
  logger.print(F(": First response bytes:"));
  for (size_t i = 0; i < firstBytesLength; ++i) {
    logger.print(' ');
    if (firstBytes[i] < 0x10) {
      logger.print('0');
    }
    logger.print(firstBytes[i], HEX);
  }
  logger.println();

  logger.print(F("AceProManager::"));
  logger.print(operation);
  logger.print(F(": Last response bytes:"));
  const size_t firstLastByteIndex =
      lastBytesLength == lastBytesCapacity ? nextLastByteIndex : 0;
  for (size_t i = 0; i < lastBytesLength; ++i) {
    const uint8_t byte = lastBytes[(firstLastByteIndex + i) % lastBytesCapacity];
    logger.print(' ');
    if (byte < 0x10) {
      logger.print('0');
    }
    logger.print(byte, HEX);
  }
  logger.println();
}

void logUsbRxDiagnostics(Print& logger, const char* operation) {
  logger.print(F("AceProManager::"));
  logger.print(operation);
  logger.print(F(": USB reader received "));
  logger.print(usbRxTaskBytes);
  logger.print(F(" byte(s), dropped "));
  logger.print(usbRxTaskDroppedBytes);
  logger.print(F(", max CDC available "));
  logger.print(usbRxMaxAvailable);
  logger.print(F(", CDC currently available "));
  logger.print(aceProUsbSerial.available());
  logger.print(F(", ms since last reader data "));
  logger.println(usbRxLastByteTime == 0 ? UINT32_MAX : millis() - usbRxLastByteTime);
}

bool requestAceProInfo(Print& logger) {
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

  uint8_t discardedBytes[64];
  while (xStreamBufferReceive(usbRxStreamBuffer, discardedBytes, sizeof(discardedBytes), 0) > 0) {
  }
  if (usbRxBufferOverflow) {
    logger.println(F("AceProManager::setup: ERROR: USB receive buffer overflowed before get_info response."));
    usbRxBufferOverflow = false;
    return false;
  }
  usbRxTaskBytes = 0;
  usbRxTaskDroppedBytes = 0;
  usbRxLastByteTime = 0;
  usbRxMaxAvailable = 0;

  logger.println(F("AceProManager::setup: Sending get_info connectivity probe..."));
  if (aceProUsbSerial.write(requestFrame, sizeof(requestFrame)) != sizeof(requestFrame)) {
    logger.println(F("AceProManager::setup: ERROR: Failed to send the get_info request."));
    return false;
  }
  aceProUsbSerial.flush();

  uint8_t responsePayload[MAX_RESPONSE_PAYLOAD_SIZE + 1];
  size_t responseLength = 0;
  size_t bytesRead = 0;
  uint8_t firstResponseBytes[16];
  size_t firstResponseBytesLength = 0;
  uint8_t lastResponseBytes[16];
  size_t lastResponseBytesLength = 0;
  size_t nextLastResponseByteIndex = 0;
  const ResponseReadResult readResult =
      readResponseFrame(responsePayload, MAX_RESPONSE_PAYLOAD_SIZE, responseLength,
                        bytesRead, logger, "setup", firstResponseBytes, sizeof(firstResponseBytes),
                        firstResponseBytesLength, lastResponseBytes,
                        sizeof(lastResponseBytes), lastResponseBytesLength,
                        nextLastResponseByteIndex);
  if (readResult == ResponseReadResult::TIMEOUT) {
    logResponseTimeout(logger, "setup", bytesRead, responseLength,
                       firstResponseBytes, firstResponseBytesLength,
                       lastResponseBytes, sizeof(lastResponseBytes),
                       lastResponseBytesLength, nextLastResponseByteIndex);
    return false;
  }
  if (readResult == ResponseReadResult::INVALID_FRAME) {
    logger.println(F("AceProManager::setup: ERROR: Received an invalid ACE Pro response frame."));
    return false;
  }

  responsePayload[responseLength] = '\0';
  long responseId = -1;
  long responseCode = -1;
  const char* responseJson = reinterpret_cast<const char*>(responsePayload);
  if (!getJsonIntegerField(responseJson, "id", responseId) || responseId != GET_INFO_REQUEST_ID) {
    logger.println(F("AceProManager::setup: ERROR: Response ID did not match the get_info request."));
    return false;
  }
  if (!getJsonIntegerField(responseJson, "code", responseCode) || responseCode != 0) {
    logger.println(F("AceProManager::setup: ERROR: ACE Pro returned an unsuccessful get_info response."));
    return false;
  }

  char model[64];
  char firmware[32];
  char bootFirmware[32];
  if (!getJsonStringField(responseJson, "model", model, sizeof(model)) ||
      !getJsonStringField(responseJson, "firmware", firmware, sizeof(firmware)) ||
      !getJsonStringField(responseJson, "boot_firmware", bootFirmware, sizeof(bootFirmware))) {
    logger.println(F("AceProManager::setup: ERROR: get_info response is missing device version information."));
    return false;
  }

  logger.print(F("AceProManager::setup: Device model: "));
  logger.println(model);
  logger.print(F("AceProManager::setup: Firmware: "));
  logger.println(firmware);
  logger.print(F("AceProManager::setup: Boot firmware: "));
  logger.println(bootFirmware);
  logger.println(F("AceProManager::setup: ACE Pro USB CDC connection verified (get_info succeeded)."));
  return true;
}

bool requestAceProDrying(Print& logger) {
  static constexpr char REQUEST_PAYLOAD[] =
      "{\"id\":3,\"method\":\"drying\",\"params\":{\"temp\":45,\"duration\":30,\"fan_speed\":1000}}";
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

  uint8_t discardedBytes[64];
  while (xStreamBufferReceive(usbRxStreamBuffer, discardedBytes, sizeof(discardedBytes), 0) > 0) {
  }
  if (usbRxBufferOverflow) {
    logger.println(F("AceProManager::startDrying: ERROR: USB receive buffer overflowed before the command response."));
    usbRxBufferOverflow = false;
    return false;
  }

  logger.println(F("AceProManager::startDrying: Sending drying command (45 C, 30 min, 1000 RPM)."));
  if (aceProUsbSerial.write(requestFrame, sizeof(requestFrame)) != sizeof(requestFrame)) {
    logger.println(F("AceProManager::startDrying: ERROR: Failed to send the drying command."));
    return false;
  }
  aceProUsbSerial.flush();

  uint8_t responsePayload[MAX_RESPONSE_PAYLOAD_SIZE + 1];
  size_t responseLength = 0;
  size_t bytesRead = 0;
  uint8_t firstResponseBytes[16];
  size_t firstResponseBytesLength = 0;
  uint8_t lastResponseBytes[16];
  size_t lastResponseBytesLength = 0;
  size_t nextLastResponseByteIndex = 0;
  const ResponseReadResult readResult =
      readResponseFrame(responsePayload, MAX_RESPONSE_PAYLOAD_SIZE, responseLength,
                        bytesRead, logger, "startDrying", firstResponseBytes,
                        sizeof(firstResponseBytes), firstResponseBytesLength,
                        lastResponseBytes, sizeof(lastResponseBytes),
                        lastResponseBytesLength, nextLastResponseByteIndex);
  if (readResult == ResponseReadResult::TIMEOUT) {
    logResponseTimeout(logger, "startDrying", bytesRead, responseLength,
                       firstResponseBytes, firstResponseBytesLength,
                       lastResponseBytes, sizeof(lastResponseBytes),
                       lastResponseBytesLength, nextLastResponseByteIndex);
    return false;
  }
  if (readResult == ResponseReadResult::INVALID_FRAME) {
    logger.println(F("AceProManager::startDrying: ERROR: Received an invalid command response frame."));
    return false;
  }

  responsePayload[responseLength] = '\0';
  const char* responseJson = reinterpret_cast<const char*>(responsePayload);
  long responseId = -1;
  long responseCode = 0;
  if (!getJsonIntegerField(responseJson, "id", responseId) ||
      responseId != START_DRYING_REQUEST_ID) {
    logger.println(F("AceProManager::startDrying: ERROR: Response ID did not match the drying command."));
    return false;
  }
  if (getJsonIntegerField(responseJson, "code", responseCode) && responseCode != 0) {
    logger.println(F("AceProManager::startDrying: ERROR: ACE Pro rejected the drying command."));
    return false;
  }

  logger.println(F("AceProManager::startDrying: Drying command acknowledged."));
  return true;
}

// bool requestAceProStatus(Print& logger, AceProManager::Status& status) {
//   static constexpr char REQUEST_PAYLOAD[] = "{\"id\":2,\"method\":\"get_status\"}";
//   constexpr size_t requestPayloadLength = sizeof(REQUEST_PAYLOAD) - 1;
//   uint8_t requestFrame[sizeof(REQUEST_PAYLOAD) + 6];
//   requestFrame[0] = 0xFF;
//   requestFrame[1] = 0xAA;
//   requestFrame[2] = static_cast<uint8_t>(requestPayloadLength & 0xFF);
//   requestFrame[3] = static_cast<uint8_t>((requestPayloadLength >> 8) & 0xFF);
//   memcpy(requestFrame + 4, REQUEST_PAYLOAD, requestPayloadLength);

//   const uint16_t crc = calculateCrc16Mcrf4xx(
//       reinterpret_cast<const uint8_t*>(REQUEST_PAYLOAD), requestPayloadLength);
//   requestFrame[4 + requestPayloadLength] = static_cast<uint8_t>(crc & 0xFF);
//   requestFrame[5 + requestPayloadLength] = static_cast<uint8_t>(crc >> 8);
//   requestFrame[6 + requestPayloadLength] = 0xFE;

//   uint8_t discardedBytes[64];
//   while (xStreamBufferReceive(usbRxStreamBuffer, discardedBytes, sizeof(discardedBytes), 0) > 0) {
//   }
//   if (usbRxBufferOverflow) {
//     logger.println(F("AceProManager::getStatus: ERROR: USB receive buffer overflowed before get_status response."));
//     usbRxBufferOverflow = false;
//     return false;
//   }
//   usbRxTaskBytes = 0;
//   usbRxTaskDroppedBytes = 0;
//   usbRxLastByteTime = 0;
//   usbRxMaxAvailable = 0;

//   logger.println(F("AceProManager::getStatus: Sending get_status request..."));
//   if (aceProUsbSerial.write(requestFrame, sizeof(requestFrame)) != sizeof(requestFrame)) {
//     logger.println(F("AceProManager::getStatus: ERROR: Failed to send the get_status request."));
//     return false;
//   }
//   aceProUsbSerial.flush();

//   uint8_t responsePayload[MAX_RESPONSE_PAYLOAD_SIZE + 1];
//   size_t responseLength = 0;
//   size_t bytesRead = 0;
//   uint8_t firstResponseBytes[16];
//   size_t firstResponseBytesLength = 0;
//   uint8_t lastResponseBytes[16];
//   size_t lastResponseBytesLength = 0;
//   size_t nextLastResponseByteIndex = 0;
//   const ResponseReadResult readResult =
//       readResponseFrame(responsePayload, MAX_RESPONSE_PAYLOAD_SIZE, responseLength,
//                         bytesRead, logger, "getStatus", firstResponseBytes, sizeof(firstResponseBytes),
//                         firstResponseBytesLength, lastResponseBytes,
//                         sizeof(lastResponseBytes), lastResponseBytesLength,
//                         nextLastResponseByteIndex);
//   if (readResult == ResponseReadResult::TIMEOUT) {
//     logger.println(F("AceProManager::getStatus: ERROR: No complete response received."));
//     logResponseTimeout(logger, "getStatus", bytesRead, responseLength,
//                        firstResponseBytes, firstResponseBytesLength,
//                        lastResponseBytes, sizeof(lastResponseBytes),
//                        lastResponseBytesLength, nextLastResponseByteIndex);
//     logUsbRxDiagnostics(logger, "getStatus");
//     if (usbRxBufferOverflow) {
//       logger.println(F("AceProManager::getStatus: WARNING: Application USB receive buffer overflowed."));
//       usbRxBufferOverflow = false;
//     }
//     return false;
//   }
//   if (readResult == ResponseReadResult::INVALID_FRAME) {
//     logger.println(F("AceProManager::getStatus: ERROR: Received an invalid ACE Pro response frame."));
//     return false;
//   }

//   responsePayload[responseLength] = '\0';
//   char* responseJson = reinterpret_cast<char*>(responsePayload);
//   long responseId = -1;
//   long responseCode = -1;
//   if (!getJsonIntegerField(responseJson, "id", responseId) || responseId != 2) {
//     logger.println(F("AceProManager::getStatus: ERROR: Response ID did not match the get_status request."));
//     return false;
//   }
//   if (!getJsonIntegerField(responseJson, "code", responseCode) || responseCode != 0) {
//     logger.println(F("AceProManager::getStatus: ERROR: ACE Pro returned an unsuccessful get_status response."));
//     return false;
//   }

//   AceProManager::Status newStatus;
//   long currentTemperature = 0;
//   long fanSpeed = 0;
//   char* dryerStatusJson = nullptr;
//   char dryerStatus[16];
//   long targetTemperature = 0;
//   long targetDuration = 0;
//   long remainingTime = 0;
//   if (!getJsonIntegerField(responseJson, "temp", currentTemperature) ||
//       !getJsonIntegerField(responseJson, "fan_speed", fanSpeed) ||
//       !getJsonObjectField(responseJson, "dryer_status", dryerStatusJson) ||
//       !getJsonStringField(dryerStatusJson, "status", dryerStatus, sizeof(dryerStatus)) ||
//       !getJsonIntegerField(dryerStatusJson, "target_temp", targetTemperature) ||
//       !getJsonIntegerField(dryerStatusJson, "duration", targetDuration) ||
//       !getJsonIntegerField(dryerStatusJson, "remain_time", remainingTime)) {
//     logger.println(F("AceProManager::getStatus: ERROR: get_status response is missing or has invalid status fields."));
//     return false;
//   }

//   if (strcmp(dryerStatus, "drying") != 0 && strcmp(dryerStatus, "stop") != 0) {
//     logger.println(F("AceProManager::getStatus: ERROR: ACE Pro returned an unknown dryer status."));
//     return false;
//   }
//   if (currentTemperature < INT_MIN || currentTemperature > INT_MAX ||
//       targetTemperature < INT_MIN || targetTemperature > INT_MAX ||
//       fanSpeed < 0 || static_cast<unsigned long>(fanSpeed) > UINT_MAX ||
//       targetDuration < 0 || static_cast<unsigned long>(targetDuration) > UINT_MAX ||
//       remainingTime < 0 || static_cast<unsigned long>(remainingTime) > UINT_MAX) {
//     logger.println(F("AceProManager::getStatus: ERROR: get_status response contains out-of-range values."));
//     return false;
//   }

//   newStatus.valid = true;
//   newStatus.currentTemperatureCelsius = static_cast<int>(currentTemperature);
//   newStatus.isDrying = strcmp(dryerStatus, "drying") == 0;
//   newStatus.targetTemperatureCelsius = static_cast<int>(targetTemperature);
//   newStatus.targetDurationMinutes = static_cast<unsigned int>(targetDuration);
//   newStatus.remainingTimeMinutes = static_cast<unsigned int>(remainingTime);
//   newStatus.fanSpeedRpm = static_cast<unsigned int>(fanSpeed);
//   status = newStatus;
//   return true;
// }

bool parseAceProStatusPayload(char* responseJson,
                              AceProManager::Status& status,
                              Print& logger) {
  long responseId = -1;
  long responseCode = 0;
  if (!getJsonIntegerField(responseJson, "id", responseId) || responseId != 2) {
    logger.println(F("AceProManager::getStatus: ERROR: Response ID did not match the get_status request."));
    return false;
  }
  if (getJsonIntegerField(responseJson, "code", responseCode) && responseCode != 0) {
    logger.println(F("AceProManager::getStatus: ERROR: ACE Pro returned an unsuccessful get_status response."));
    return false;
  }

  long currentTemperature = 0;
  long fanSpeed = 0;
  char* dryerStatusJson = nullptr;
  char dryerStatus[16];
  long targetTemperature = 0;
  long targetDuration = 0;
  long remainingTime = 0;

  if (!getJsonIntegerField(responseJson, "temp", currentTemperature) ||
      !getJsonIntegerField(responseJson, "fan_speed", fanSpeed) ||
      !getJsonObjectField(responseJson, "dryer_status", dryerStatusJson) ||
      !getJsonStringField(dryerStatusJson, "status", dryerStatus, sizeof(dryerStatus)) ||
      !getJsonIntegerField(dryerStatusJson, "target_temp", targetTemperature) ||
      !getJsonIntegerField(dryerStatusJson, "duration", targetDuration) ||
      !getJsonIntegerField(dryerStatusJson, "remain_time", remainingTime)) {
    logger.println(F("AceProManager::getStatus: ERROR: Response does not contain all complete status fields."));
    return false;
  }

  if (strcmp(dryerStatus, "drying") != 0 && strcmp(dryerStatus, "stop") != 0) {
    logger.println(F("AceProManager::getStatus: ERROR: ACE Pro returned an unknown dryer status."));
    return false;
  }
  if (currentTemperature < INT_MIN || currentTemperature > INT_MAX ||
      targetTemperature < INT_MIN || targetTemperature > INT_MAX ||
      fanSpeed < 0 || static_cast<unsigned long>(fanSpeed) > UINT_MAX ||
      targetDuration < 0 || static_cast<unsigned long>(targetDuration) > UINT_MAX ||
      remainingTime < 0 || static_cast<unsigned long>(remainingTime) > UINT_MAX) {
    logger.println(F("AceProManager::getStatus: ERROR: Response contains out-of-range status values."));
    return false;
  }

  status.valid = true;
  status.currentTemperatureCelsius = static_cast<int>(currentTemperature);
  status.isDrying = strcmp(dryerStatus, "drying") == 0;
  status.targetTemperatureCelsius = static_cast<int>(targetTemperature);
  status.targetDurationMinutes = static_cast<unsigned int>(targetDuration);
  status.remainingTimeMinutes = static_cast<unsigned int>(remainingTime);
  status.fanSpeedRpm = static_cast<unsigned int>(fanSpeed);
  return true;
}

}  // namespace

bool requestAceProStatus(Print& logger, AceProManager::Status& status) {
  static constexpr char REQUEST_PAYLOAD[] =
      "{\"id\":2,\"method\":\"get_status\"}";
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

  // Clear any stale bytes in the stream buffer
  uint8_t discardedBytes[64];
  while (xStreamBufferReceive(usbRxStreamBuffer, discardedBytes, sizeof(discardedBytes), 0) > 0) {
  }
  if (usbRxBufferOverflow) {
    logger.println(F("AceProManager::getStatus: ERROR: USB receive buffer overflowed before get_status request."));
    usbRxBufferOverflow = false;
    return false;
  }
  usbRxTaskBytes = 0;
  usbRxTaskDroppedBytes = 0;
  usbRxLastByteTime = 0;
  usbRxMaxAvailable = 0;
  usbStatusRequestStartTime = millis();

  logger.println(F("AceProManager::getStatus: Sending get_status request..."));
  if (aceProUsbSerial.write(requestFrame, sizeof(requestFrame)) != sizeof(requestFrame)) {
    logger.println(F("AceProManager::getStatus: ERROR: Failed to send the get_status request."));
    return false;
  }
  aceProUsbSerial.flush();

  // Allocate buffer large enough for header(4) + payload(~737) + CRC/term(3) = ~744 bytes
  constexpr size_t FRAME_BUFFER_SIZE = 2048;
  uint8_t frameBuffer[FRAME_BUFFER_SIZE];
  size_t totalBytesRead = 0;
  size_t responseLength = 0;

  const ResponseReadResult readResult =
      readResponseFrameDynamic(frameBuffer, FRAME_BUFFER_SIZE, totalBytesRead,
                               responseLength, logger);

  if (readResult == ResponseReadResult::TIMEOUT) {
    logger.println(F("AceProManager::getStatus: ERROR: No complete response received (Timeout)."));
    logReceivedPayloadAsText(logger, frameBuffer, totalBytesRead, responseLength);
    if (totalBytesRead > 4 && totalBytesRead < FRAME_BUFFER_SIZE &&
        frameBuffer[0] == 0xFF && frameBuffer[1] == 0xAA &&
        responseLength <= FRAME_BUFFER_SIZE - 7) {
      frameBuffer[totalBytesRead] = '\0';
      char* partialJson = reinterpret_cast<char*>(frameBuffer + 4);
      if (parseAceProStatusPayload(partialJson, status, logger)) {
        status.responseComplete = false;
        logger.println(F("AceProManager::getStatus: Parsed status from an incomplete, unverified response."));
        logUsbRxDiagnostics(logger, "getStatus");
        return true;
      }
    }
    logUsbRxDiagnostics(logger, "getStatus");
    return false;
  }
  if (readResult == ResponseReadResult::INVALID_FRAME ||
      !validateResponseFrame(frameBuffer, totalBytesRead, responseLength)) {
    logger.println(F("AceProManager::getStatus: ERROR: Received an invalid ACE Pro response frame or CRC failure."));
    return false;
  }

  char* responseJson = reinterpret_cast<char*>(frameBuffer + 4);
  responseJson[responseLength] = '\0';
  if (!parseAceProStatusPayload(responseJson, status, logger)) {
    return false;
  }
  status.responseComplete = true;
  logger.println(F("AceProManager::getStatus: Parsed status from a complete, validated response."));
  return true;
}

AceProManager::AceProManager(Print& logger) : _logger(logger) {}

bool AceProManager::setup() {
  _logger.println(F("AceProManager::setup: Initializing ACE Pro connection..."));

  if (!_usbHostStarted) {
    if (!aceProUsbSerial.setRxBufferSize(USB_CDC_RX_BUFFER_SIZE)) {
      _logger.println(F("AceProManager::setup: ERROR: Failed to allocate the CDC receive buffer."));
      return false;
    }

    aceUsbHost.onDeviceConnected([](const EspUsbHostDeviceInfo& device) {
      if (device.vid != 0x28E9 || device.pid != 0x018A) {
        return;
      }
      if (aceUsbHost.serialPortCount(device.address) == 0) {
        return;
      }

      aceProUsbSerial.setAddress(device.address);
      aceProUsbSerial.setPort(0);
      if (!aceProUsbSerial.begin(115200)) {
        aceProConnected = false;
        aceProSerialOpenFailed = true;
        return;
      }

      aceProSerialOpenFailed = false;
      aceProConnected = true;
    });
    aceUsbHost.onDeviceDisconnected([](const EspUsbHostDeviceInfo& device) {
      if (device.address != aceProUsbSerial.address()) {
        return;
      }
      aceProConnected = false;
      aceProUsbSerial.end();
    });

    if (!aceUsbHost.begin()) {
      _logger.print(F("AceProManager::setup: ERROR: Failed to start EspUsbHost: "));
      _logger.println(aceUsbHost.lastErrorName());
      return false;
    }
    _usbHostStarted = true;
    _logger.println(F("AceProManager::setup: EspUsbHost started."));
  }

  if (!startUsbRxTask(_logger)) {
    return false;
  }
  _logger.println(F("AceProManager::setup: Waiting for the ACE Pro USB CDC interface."));

  const unsigned long startTime = millis();
  unsigned long lastDiagnosticTime = startTime;
  while (!aceProUsbSerial.connected() && !aceProSerialOpenFailed &&
         millis() - startTime < USB_CONNECTION_TIMEOUT_MS) {
    const unsigned long now = millis();
    if (now - lastDiagnosticTime >= USB_DIAGNOSTIC_INTERVAL_MS) {
      lastDiagnosticTime = now;
      _logger.print(F("AceProManager::setup: Still waiting for USB CDC; elapsed "));
      _logger.print(now - startTime);
      _logger.println(F(" ms."));
    }
    delay(10);
  }
  if (!aceProUsbSerial.connected()) {
    if (aceProSerialOpenFailed) {
      _logger.println(F("AceProManager::setup: ERROR: Failed to open the ACE Pro CDC serial port."));
      return false;
    }
    _logger.println(F("AceProManager::setup: ERROR: No USB CDC interface connected within the timeout."));
    return false;
  }

  _logger.println(F("AceProManager::setup: ACE Pro USB CDC device connected."));

  if (!requestAceProInfo(_logger)) {
    _logger.println(F("AceProManager::setup: ACE Pro communication check FAILED."));
    return false;
  }

  _logger.println(F("AceProManager::setup: Initializing ACE Pro connection DONE."));
  return true;
}

bool AceProManager::isConnected() const {
  return _usbHostStarted && aceProConnected && aceProUsbSerial.connected();
}

bool AceProManager::startDrying() {
  if (!isConnected()) {
    _logger.println(F("AceProManager::startDrying: ERROR: USB CDC device is not connected."));
    return false;
  }

  return requestAceProDrying(_logger);
}

AceProManager::Status AceProManager::getStatus() {
  if (!_usbHostStarted || !aceProUsbSerial.connected()) {
    _logger.println(F("AceProManager::getStatus: ERROR: USB CDC device is not connected."));
    _status.valid = false;
    _status.responseComplete = false;
    return _status;
  }

  if (!requestAceProStatus(_logger, _status)) {
    _status.valid = false;
    _status.responseComplete = false;
    return _status;
  }

  if (!_status.responseComplete) {
    _logger.println(F("AceProManager::getStatus: WARNING: Status values are from an incomplete, unverified response."));
  }
  _logger.print(F("AceProManager::getStatus: Current temperature: "));
  _logger.print(_status.currentTemperatureCelsius);
  _logger.println(F(" C"));
  _logger.print(F("AceProManager::getStatus: Dryer status: "));
  _logger.println(_status.isDrying ? F("drying") : F("stop"));
  _logger.print(F("AceProManager::getStatus: Target temperature: "));
  _logger.print(_status.targetTemperatureCelsius);
  _logger.println(F(" C"));
  _logger.print(F("AceProManager::getStatus: Duration: "));
  _logger.print(_status.targetDurationMinutes);
  _logger.println(F(" minutes"));
  _logger.print(F("AceProManager::getStatus: Remaining time: "));
  _logger.print(_status.remainingTimeMinutes);
  _logger.println(F(" minutes"));
  _logger.print(F("AceProManager::getStatus: Fan speed: "));
  _logger.print(_status.fanSpeedRpm);
  _logger.println(F(" RPM"));
  return _status;
}
