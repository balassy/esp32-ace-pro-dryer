#pragma once

#include <Arduino.h>

class LogRouter : public Print {
 public:
  explicit LogRouter(Print& output);

  void setOutput(Print& output);
  void flush() override;

  size_t write(uint8_t value) override;
  size_t write(const uint8_t* buffer, size_t size) override;

 private:
  Print* _output;
  bool _lineStart = true;
};

extern LogRouter logRouter;
