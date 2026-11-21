#include "AceProClient.h"

#include <EspUsbHost.h>

#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

EspUsbHost aceUsbHost;
EspUsbHostCdcSerial aceProUsbSerial(aceUsbHost);
volatile bool aceProConnected = false;
unsigned long lastLinkActivityMs = 0;

void noteLinkActivity() {
  lastLinkActivityMs = millis();
}

constexpr unsigned long USB_RESPONSE_TIMEOUT_MS = 2000;
constexpr size_t MAX_RESPONSE_PAYLOAD_SIZE = 4096;
constexpr size_t USB_CDC_RX_BUFFER_SIZE = 4096;
uint16_t nextAceRequestId = 1;
uint8_t responsePayload[MAX_RESPONSE_PAYLOAD_SIZE + 1];
const char* commandPhase = "idle";
unsigned long commandPhaseStartedMs = 0;

void setPhase(const char* phase) {
  commandPhase = phase;
  commandPhaseStartedMs = millis();
}

uint16_t allocateAceRequestId() {
  const uint16_t requestId = nextAceRequestId;
  ++nextAceRequestId;
  if (nextAceRequestId == 0) {
    nextAceRequestId = 1;
  }
  return requestId;
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
  DISCONNECTED,
  INVALID_FRAME
};

bool usbLinkIsUp() {
  return aceUsbHost.ready() && aceProConnected && aceProUsbSerial.connected();
}

const __FlashStringHelper* responseReadStateName(int state) {
  switch (state) {
    case 0:
      return F("header");
    case 1:
      return F("header");
    case 2:
      return F("length");
    case 3:
      return F("length");
    case 4:
      return F("payload");
    case 5:
      return F("crc");
    case 6:
      return F("crc");
    case 7:
      return F("terminator");
    default:
      return F("unknown");
  }
}

ResponseReadResult readResponseFrame(uint8_t* payload,
                                     size_t payloadCapacity,
                                     size_t& payloadLength,
                                     Print& logger,
                                     const char* operation) {
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
  size_t bytesRead = 0;
  const unsigned long startTime = millis();
  unsigned long lastProgressLogTime = startTime;
  logger.print(F("AceProClient::"));
  logger.print(operation);
  logger.println(F(": Waiting for a response frame."));
  setPhase("read");

  while (millis() - startTime < USB_RESPONSE_TIMEOUT_MS) {
    const unsigned long now = millis();
    if (now - lastProgressLogTime >= 250) {
      lastProgressLogTime = now;
      logger.print(F("AceProClient::"));
      logger.print(operation);
      logger.print(F(": Response progress at "));
      logger.print(now - startTime);
      logger.print(F(" ms, bytes "));
      logger.print(bytesRead);
      logger.print(F(", state "));
      logger.print(responseReadStateName(static_cast<int>(state)));
      logger.print(F(", payload "));
      logger.print(payloadLength);
      logger.print(F(", link "));
      logger.println(usbLinkIsUp() ? F("up.") : F("down."));
    }

    const int value = aceProUsbSerial.read();
    if (value < 0) {
      if (!usbLinkIsUp()) {
        logger.print(F("AceProClient::"));
        logger.print(operation);
        logger.print(F(": USB link dropped after "));
        logger.print(bytesRead);
        logger.println(F(" response byte(s)."));
        return ResponseReadResult::DISCONNECTED;
      }
      delay(1);
      continue;
    }
    ++bytesRead;

    const uint8_t byte = static_cast<uint8_t>(value);
    switch (state) {
      case ReadState::HEADER_FF:
        if (byte == 0xFF) {
          state = ReadState::HEADER_AA;
        }
        break;
      case ReadState::HEADER_AA:
        state = byte == 0xAA ? ReadState::LENGTH_LOW
                             : (byte == 0xFF ? ReadState::HEADER_AA : ReadState::HEADER_FF);
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
          return receivedCrc == expectedCrc ? ResponseReadResult::RECEIVED
                                            : ResponseReadResult::INVALID_FRAME;
        }
        return ResponseReadResult::INVALID_FRAME;
    }
  }

  return usbLinkIsUp() ? ResponseReadResult::TIMEOUT : ResponseReadResult::DISCONNECTED;
}

void logOperation(Print& logger, const char* operation, const __FlashStringHelper* message) {
  logger.print(F("AceProClient::"));
  logger.print(operation);
  logger.print(F(": "));
  logger.println(message);
}

bool waitForConnection(unsigned long timeoutMs) {
  const unsigned long startTime = millis();
  while (millis() - startTime < timeoutMs) {
    noteLinkActivity();
    if (aceProUsbSerial.connected() &&
        (aceProConnected || aceProUsbSerial.address() != ESP_USB_HOST_ANY_ADDRESS)) {
      aceProConnected = true;
    }
    if (usbLinkIsUp()) {
      noteLinkActivity();
      return true;
    }
    delay(10);
  }
  return usbLinkIsUp();
}

bool startUsbHost(Print& logger) {
  logger.println(F("AceProClient::startUsbHost: Starting USB host..."));

  if (aceUsbHost.ready()) {
    logger.println(F("AceProClient::startUsbHost: Starting USB host DONE. USB host is already ready."));
    return true;
  }

  if (!aceProUsbSerial.setRxBufferSize(USB_CDC_RX_BUFFER_SIZE)) {
    logger.println(F("AceProClient::startUsbHost: ERROR: Failed to allocate the CDC receive buffer."));
    return false;
  }

  aceProUsbSerial.setPort(0);
  if (!aceProUsbSerial.begin(115200)) {
    logger.println(F("AceProClient::startUsbHost: ERROR: Failed to initialize the CDC serial interface."));
    return false;
  }

  aceUsbHost.onDeviceConnected([](const EspUsbHostDeviceInfo& device) {
    if (device.vid != 0x28E9 || device.pid != 0x018A) {
      return;
    }
    if (aceUsbHost.serialPortCount(device.address) == 0) {
      aceProConnected = false;
      return;
    }

    aceProUsbSerial.setAddress(device.address);
    aceProConnected = true;
  });

  aceUsbHost.onDeviceDisconnected([](const EspUsbHostDeviceInfo& device) {
    if (device.vid != 0x28E9 || device.pid != 0x018A) {
      return;
    }
    aceProConnected = false;
    aceProUsbSerial.clearAddress();
  });

  if (!aceUsbHost.begin()) {
    aceProUsbSerial.end();
    logger.print(F("AceProClient::startUsbHost: ERROR: Failed to start EspUsbHost: "));
    logger.println(aceUsbHost.lastErrorName());
    return false;
  }

  logger.println(F("AceProClient::startUsbHost: Starting USB host DONE."));
  return true;
}

bool reopenCdcPort(Print& logger) {
  logger.println(F("AceProClient::reopenCdcPort: Reopening the ACE Pro CDC connection..."));
  aceProUsbSerial.end();
  aceProConnected = false;
  if (!aceProUsbSerial.begin(115200)) {
    logger.println(F("AceProClient::reopenCdcPort: ERROR: Failed to reopen the ACE Pro CDC serial port."));
    return false;
  }

  logger.println(F("AceProClient::reopenCdcPort: Reopening the ACE Pro CDC connection DONE."));
  return true;
}

size_t discardPendingUsbBytes() {
  size_t discardedBytes = 0;
  while (aceProUsbSerial.available() > 0) {
    if (aceProUsbSerial.read() < 0) {
      break;
    }
    ++discardedBytes;
  }
  return discardedBytes;
}

bool parseDryerStatusPayload(char* responseJson, AceProClient::DryerStatus& dryerStatus, Print& logger) {
  long currentTemperature = 0;
  long fanSpeed = 0;
  char* dryerStatusJson = nullptr;
  char dryerStatusText[16];
  long targetTemperature = 0;
  long targetDuration = 0;
  long remainingTime = 0;

  if (!getJsonIntegerField(responseJson, "temp", currentTemperature) ||
      !getJsonIntegerField(responseJson, "fan_speed", fanSpeed) ||
      !getJsonObjectField(responseJson, "dryer_status", dryerStatusJson) ||
      !getJsonStringField(dryerStatusJson, "status", dryerStatusText, sizeof(dryerStatusText)) ||
      !getJsonIntegerField(dryerStatusJson, "target_temp", targetTemperature) ||
      !getJsonIntegerField(dryerStatusJson, "duration", targetDuration) ||
      !getJsonIntegerField(dryerStatusJson, "remain_time", remainingTime)) {
    logger.println(F("AceProClient::getDryerStatus: ERROR: Response does not contain all complete dryer status fields."));
    return false;
  }

  if (strcmp(dryerStatusText, "drying") != 0 && strcmp(dryerStatusText, "stop") != 0) {
    logger.println(F("AceProClient::getDryerStatus: ERROR: ACE Pro returned an unknown dryer status."));
    return false;
  }
  if (currentTemperature < INT_MIN || currentTemperature > INT_MAX ||
      targetTemperature < INT_MIN || targetTemperature > INT_MAX ||
      fanSpeed < 0 || static_cast<unsigned long>(fanSpeed) > UINT_MAX ||
      targetDuration < 0 || static_cast<unsigned long>(targetDuration) > UINT_MAX ||
      remainingTime < 0 || static_cast<unsigned long>(remainingTime) > UINT_MAX) {
    logger.println(F("AceProClient::getDryerStatus: ERROR: Response contains out-of-range dryer status values."));
    return false;
  }

  dryerStatus.valid = true;
  dryerStatus.responseComplete = true;
  dryerStatus.currentTemperatureCelsius = static_cast<int>(currentTemperature);
  dryerStatus.isDrying = strcmp(dryerStatusText, "drying") == 0;
  dryerStatus.targetTemperatureCelsius = static_cast<int>(targetTemperature);
  dryerStatus.targetDurationMinutes = static_cast<unsigned int>(targetDuration);
  dryerStatus.remainingTimeSeconds = static_cast<unsigned int>(remainingTime);
  dryerStatus.fanSpeedRpm = static_cast<unsigned int>(fanSpeed);
  return true;
}

AceProClient::Error exchangeAceProCommand(Print& logger,
                                          const char* requestPayload,
                                          size_t requestPayloadLength,
                                          uint16_t requestId,
                                          const char* operation,
                                          const char* commandDescription,
                                          size_t& responseLength) {
  if (!usbLinkIsUp()) {
    logOperation(logger, operation, F("ERROR: ACE Pro is not connected."));
    return AceProClient::Error::NOT_CONNECTED;
  }

  uint8_t requestFrame[160];
  if (requestPayloadLength > sizeof(requestFrame) - 7) {
    logOperation(logger, operation, F("ERROR: Command payload exceeds the request buffer."));
    return AceProClient::Error::FAILED;
  }

  requestFrame[0] = 0xFF;
  requestFrame[1] = 0xAA;
  requestFrame[2] = static_cast<uint8_t>(requestPayloadLength & 0xFF);
  requestFrame[3] = static_cast<uint8_t>((requestPayloadLength >> 8) & 0xFF);
  memcpy(requestFrame + 4, requestPayload, requestPayloadLength);

  const uint16_t crc = calculateCrc16Mcrf4xx(
      reinterpret_cast<const uint8_t*>(requestPayload), requestPayloadLength);
  requestFrame[4 + requestPayloadLength] = static_cast<uint8_t>(crc & 0xFF);
  requestFrame[5 + requestPayloadLength] = static_cast<uint8_t>(crc >> 8);
  requestFrame[6 + requestPayloadLength] = 0xFE;
  const size_t requestFrameLength = requestPayloadLength + 7;

  setPhase("discard");
  const size_t discardedBytes = discardPendingUsbBytes();
  logger.print(F("AceProClient::"));
  logger.print(operation);
  logger.print(F(": Discarded "));
  logger.print(discardedBytes);
  logger.print(F(" stale USB byte(s). Link is "));
  logger.println(usbLinkIsUp() ? F("up.") : F("down."));
  logger.print(F("AceProClient::"));
  logger.print(operation);
  logger.print(F(": Sending "));
  logger.print(commandDescription);
  logger.print(F(" ("));
  logger.print(requestFrameLength);
  logger.println(F(" bytes)."));
  logger.flush();
  setPhase("write");
  const unsigned long writeStartedMs = millis();
  const size_t bytesWritten = aceProUsbSerial.write(requestFrame, requestFrameLength);
  logger.print(F("AceProClient::"));
  logger.print(operation);
  logger.print(F(": USB write finished after "));
  logger.print(millis() - writeStartedMs);
  logger.println(F(" ms."));
  logger.print(F("AceProClient::"));
  logger.print(operation);
  logger.print(F(": USB write returned "));
  logger.print(bytesWritten);
  logger.print(F(" of "));
  logger.print(requestFrameLength);
  logger.print(F(". Link is "));
  logger.println(usbLinkIsUp() ? F("up.") : F("down."));
  if (bytesWritten != requestFrameLength) {
    logOperation(logger, operation, F("ERROR: USB write did not accept the command."));
    return AceProClient::Error::NOT_CONNECTED;
  }
  logger.print(F("AceProClient::"));
  logger.print(operation);
  logger.println(F(": Flushing the USB write."));
  setPhase("flush");
  const unsigned long flushStartedMs = millis();
  aceProUsbSerial.flush();
  logger.print(F("AceProClient::"));
  logger.print(operation);
  logger.print(F(": USB flush finished after "));
  logger.print(millis() - flushStartedMs);
  logger.println(F(" ms."));
  logger.print(F("AceProClient::"));
  logger.print(operation);
  logger.print(F(": USB write flushed. Link is "));
  logger.println(usbLinkIsUp() ? F("up.") : F("down."));

  const ResponseReadResult readResult =
      readResponseFrame(responsePayload, MAX_RESPONSE_PAYLOAD_SIZE, responseLength, logger, operation);
  if (readResult == ResponseReadResult::DISCONNECTED) {
    logOperation(logger, operation, F("ERROR: ACE Pro disconnected before responding."));
    return AceProClient::Error::NOT_CONNECTED;
  }
  if (readResult == ResponseReadResult::TIMEOUT) {
    logOperation(logger, operation, F("ERROR: Timed out waiting for a response."));
    return AceProClient::Error::TIMEOUT;
  }
  if (readResult == ResponseReadResult::INVALID_FRAME) {
    logOperation(logger, operation, F("ERROR: Received an invalid ACE Pro response frame."));
    return AceProClient::Error::INVALID_RESPONSE;
  }

  responsePayload[responseLength] = '\0';
  const char* responseJson = reinterpret_cast<const char*>(responsePayload);
  long responseId = -1;
  long responseCode = 0;
  if (!getJsonIntegerField(responseJson, "id", responseId) || responseId != requestId) {
    logOperation(logger, operation, F("ERROR: Response ID did not match the command."));
    return AceProClient::Error::INVALID_RESPONSE;
  }
  if (getJsonIntegerField(responseJson, "code", responseCode) && responseCode != 0) {
    logOperation(logger, operation, F("ERROR: ACE Pro rejected the command."));
    return AceProClient::Error::REJECTED;
  }
  return AceProClient::Error::OK;
}

AceProClient::Error sendAceProMethod(Print& logger,
                                     const char* method,
                                     const char* paramsJson,
                                     const char* operation,
                                     const char* commandDescription,
                                     size_t& responseLength) {
  const uint16_t requestId = allocateAceRequestId();
  char requestPayload[128];
  const int payloadLength =
      paramsJson == nullptr
          ? snprintf(requestPayload, sizeof(requestPayload),
                     "{\"id\":%u,\"method\":\"%s\"}",
                     static_cast<unsigned int>(requestId), method)
          : snprintf(requestPayload, sizeof(requestPayload),
                     "{\"id\":%u,\"method\":\"%s\",\"params\":%s}",
                     static_cast<unsigned int>(requestId), method, paramsJson);
  if (payloadLength <= 0 || static_cast<size_t>(payloadLength) >= sizeof(requestPayload)) {
    logOperation(logger, operation, F("ERROR: Failed to create the request."));
    return AceProClient::Error::FAILED;
  }

  return exchangeAceProCommand(logger, requestPayload, static_cast<size_t>(payloadLength), requestId,
                               operation, commandDescription, responseLength);
}

}  // namespace

AceProClient::AceProClient(Print& logger) : _logger(logger) {}

AceProClient::Error AceProClient::connect(unsigned long timeoutMs) {
  _logger.println(F("AceProClient::connect: Connecting to ACE Pro..."));

  _logger.println(F("AceProClient::connect: Starting USB host..."));
  const bool hostWasRunning = aceUsbHost.ready();
  _logger.print("AceProClient::connect: USB host was ");
  _logger.print(hostWasRunning ? F("running.") : F("not running."));
  _logger.println();

  if (!startUsbHost(_logger)) {
    _logger.println(F("AceProClient::connect: Connecting to ACE Pro DONE. ERROR: Failed to start USB host."));
    return Error::HOST_FAILED;
  }

  _logger.println(F("AceProClient::connect: USB host started."));
  _logger.println(F("AceProClient::connect: Checking USB link status..."));

  if (usbLinkIsUp()) {
    noteLinkActivity();
    _logger.println(F("AceProClient::connect: Checking USB link status: USB link is up."));
    _logger.println(F("AceProClient::connect: Connecting to ACE Pro DONE. USB link is up."));
    return Error::OK;
  }

   _logger.println(F("AceProClient::connect: Checking USB link status: USB link is down."));

  _logger.println(F("AceProClient::connect: Reopening the CDC serial port..."));

  // The host was already up and the ACE Pro is gone, so reopen CDC before waiting.
  // The first connect only waits, which lets a device that is already on enumerate.
  if (hostWasRunning && !reopenCdcPort(_logger)) {
    _logger.println(F("AceProClient::connect: Connecting to ACE Pro DONE. ERROR: Failed to reopen the ACE Pro CDC serial port."));
    return Error::HOST_FAILED;
  }

  _logger.println(F("AceProClient::connect: CDC serial port reopened."));
  _logger.println(F("AceProClient::connect: Waiting for the ACE Pro CDC connection..."));

  if (waitForConnection(timeoutMs)) {
    _logger.println(F("AceProClient::connect: Connecting to ACE Pro DONE. ACE Pro connection is ready."));
    return Error::OK;
  }

  _logger.println(F("AceProClient::connect: Connecting to ACE Pro DONE. ACE Pro is not connected."));
  return Error::NOT_CONNECTED;
}

AceProClient::Error AceProClient::reopen(unsigned long timeoutMs) {
  setPhase("reopen");
  _logger.println(F("AceProClient::reopen: Restarting the USB host."));

  // Bouncing only the CDC stream calls setSerialBaudRate(), which clears the
  // port and submits SET_LINE_CODING. The following bulk OUT then waits inside
  // the host library and does not return. Stop the whole host and start it again.
  aceProConnected = false;
  aceProUsbSerial.end();
  aceUsbHost.end();

  if (!startUsbHost(_logger)) {
    _logger.println(F("AceProClient::reopen: ERROR: Failed to start USB host."));
    setPhase("idle");
    return Error::HOST_FAILED;
  }
  if (!waitForConnection(timeoutMs)) {
    _logger.println(F("AceProClient::reopen: ERROR: ACE Pro did not enumerate."));
    setPhase("idle");
    return Error::NOT_CONNECTED;
  }

  // The connect path submits SET_LINE_CODING. Wait until that control transfer
  // can finish before the next bulk OUT.
  setPhase("settle");
  _logger.println(F("AceProClient::reopen: Waiting for CDC line coding to finish."));
  delay(400);
  _logger.println(F("AceProClient::reopen: USB host restarted. ACE Pro CDC port is open."));
  return Error::OK;
}

void AceProClient::release() {
  if (!aceUsbHost.ready() && !aceProConnected) {
    return;
  }
  _logger.println(F("AceProClient::release: Releasing the ACE Pro USB host."));
  setPhase("idle");
  aceProConnected = false;
  aceProUsbSerial.end();
  aceUsbHost.end();
  _logger.println(F("AceProClient::release: ACE Pro USB host released."));
}

unsigned long AceProClient::millisecondsSinceActivity() const {
  return millis() - lastLinkActivityMs;
}

bool AceProClient::isConnected() const {
  return usbLinkIsUp();
}

const char* AceProClient::getCurrentPhase() const {
  return commandPhase;
}

unsigned long AceProClient::getPhaseElapsedMs() const {
  return millis() - commandPhaseStartedMs;
}

AceProClient::Error AceProClient::getDryerStatus(DryerStatus& dryerStatus) {
  dryerStatus = DryerStatus{};
  size_t responseLength = 0;
  const Error error = sendAceProMethod(_logger, "get_status", nullptr, "getDryerStatus",
                                       "get_status request.", responseLength);
  if (error != Error::OK) {
    return error;
  }
  if (!parseDryerStatusPayload(reinterpret_cast<char*>(responsePayload), dryerStatus, _logger)) {
    dryerStatus = DryerStatus{};
    return Error::INVALID_RESPONSE;
  }
  return Error::OK;
}

AceProClient::Error AceProClient::startDrying() {
  size_t responseLength = 0;
  return sendAceProMethod(_logger, "drying", "{\"temp\":45,\"duration\":3,\"fan_speed\":50}",
                          "startDrying", "drying command.", responseLength);
}

AceProClient::Error AceProClient::stopDrying() {
  size_t responseLength = 0;
  return sendAceProMethod(_logger, "drying_stop", "{}", "stopDrying", "drying_stop command.",
                          responseLength);
}
