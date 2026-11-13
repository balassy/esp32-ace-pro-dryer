#include "LogRouter.h"

LogRouter::LogRouter(Print& output) : _output(&output) {}

void LogRouter::setOutput(Print& output) {
  _output = &output;
}

void LogRouter::flush() {
  _output->flush();
}

size_t LogRouter::write(uint8_t value) {
  return _output->write(value);
}

size_t LogRouter::write(const uint8_t* buffer, size_t size) {
  return _output->write(buffer, size);
}
