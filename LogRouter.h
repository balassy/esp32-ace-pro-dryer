#pragma once

#include <Arduino.h>

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

class LogRouter : public Print {
 public:
  explicit LogRouter(Print& output);

  void setOutput(Print& output);
  void flush() override;

  size_t write(uint8_t value) override;
  size_t write(const uint8_t* buffer, size_t size) override;

  const char* getServerAddress() const;

 private:
  void _beginLine();
  void _endLine();

  Print* _output;
  bool _lineStart = true;
  SemaphoreHandle_t _mutex = nullptr;
  TaskHandle_t _lineTask = nullptr;
};

extern LogRouter logRouter;
