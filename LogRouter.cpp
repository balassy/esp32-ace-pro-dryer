#include "LogRouter.h"

#include <cstdio>

LogRouter::LogRouter(Print& output) : _output(&output) {}

void LogRouter::setOutput(Print& output) {
  _output = &output;
}

void LogRouter::flush() {
  _output->flush();
}

size_t LogRouter::write(uint8_t value) {
  return write(&value, 1);
}

size_t LogRouter::write(const uint8_t* buffer, size_t size) {
  if (buffer == nullptr || size == 0 || _output == nullptr) {
    return 0;
  }

  size_t accepted = 0;
  size_t offset = 0;
  while (offset < size) {
    if (_lineStart) {
      while (offset < size && buffer[offset] == '\r') {
        if (_output->write(buffer[offset]) != 1) {
          return accepted;
        }
        ++accepted;
        ++offset;
      }
      if (offset >= size) {
        break;
      }

      const unsigned long totalSeconds = millis() / 1000UL;
      char timestamp[16];
      const int timestampLength =
          snprintf(timestamp, sizeof(timestamp), "%02lu:%02lu ", totalSeconds / 60UL, totalSeconds % 60UL);
      if (timestampLength > 0) {
        _output->write(reinterpret_cast<const uint8_t*>(timestamp), static_cast<size_t>(timestampLength));
      }
      _lineStart = false;
    }

    size_t end = offset;
    while (end < size && buffer[end] != '\n') {
      ++end;
    }
    if (end < size) {
      ++end;
    }
    const size_t chunkLength = end - offset;
    const size_t written = _output->write(buffer + offset, chunkLength);
    accepted += written;
    if (written != chunkLength) {
      return accepted;
    }
    _lineStart = buffer[end - 1] == '\n';
    offset = end;
  }
  return accepted;
}
