#pragma once

#ifndef CYF_LOG_H
#define CYF_LOG_H

#define CYF_LOG_SEVERITY_VERBOSE (1)
#define CYF_LOG_SEVERITY_DEBUG (2)
#define CYF_LOG_SEVERITY_INFO (3)
#define CYF_LOG_SEVERITY_WARN (4)
#define CYF_LOG_SEVERITY_ERROR (5)
#define CYF_LOG_SEVERITY_NONE (6)

#ifndef CYF_LOG_SEVERITY
#define CYF_LOG_SEVERITY (CYF_LOG_SEVERITY_INFO)
#endif

#if defined(ARDUINO) && !defined(ARDUINO_ARCH_ESP32)
#include <Arduino.h>
#endif

#if defined(ARDUINO_ARCH_AVR)
#include <assert.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#else
#include <cassert>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#endif

#if defined(ARDUINO_ARCH_ESP32)
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#endif

static_assert(__cplusplus >= 201103L, "This project requires C++11 standard");

namespace cyf {
namespace log {
template <typename T, size_t size>
constexpr inline size_t ExtractFileNameOffset(const T (&file_path)[size], size_t i = size) noexcept {
  return (i == 0) ? 0 : (file_path[i - 1] == '/' || file_path[i - 1] == '\\') ? i : ExtractFileNameOffset(file_path, i - 1);
}

using MillisSource = uint32_t (*)();

inline MillisSource& millis_source() noexcept {
  static MillisSource s_millis_source = nullptr;
  return s_millis_source;
}

inline void set_millis_source(MillisSource source) noexcept {
  assert(millis_source() == nullptr);  // Ensure it's set only once
  millis_source() = source;
}

using LogSink = void (*)(const char* message, size_t length);

inline LogSink& log_sink() noexcept {
  static LogSink s_log_sink = nullptr;
  return s_log_sink;
}

inline void set_log_sink(LogSink sink) noexcept {
  assert(log_sink() == nullptr);  // Ensure it's set only once
  log_sink() = sink;
}

inline size_t& format_buffer_size() noexcept {
#if defined(ARDUINO_ARCH_AVR)
  static size_t s_format_buffer_size = 128;  // Default buffer size
#else
  static size_t s_format_buffer_size = 512;  // Default buffer size
#endif
  return s_format_buffer_size;
}

inline void set_format_buffer_size(size_t size) noexcept {
  assert(size > 0);  // Ensure the buffer size is within a reasonable range
  format_buffer_size() = size;
}

inline void FormatTimestamp(char* buffer) noexcept {
  constexpr int kShiftBits = 28;
  constexpr uint32_t kTimeMask = (uint32_t{1} << kShiftBits) - 1;

  const uint32_t raw_millis = millis_source() ? millis_source()() :
#if defined(ARDUINO_ARCH_ESP32)
                                              pdTICKS_TO_MS(xTaskGetTickCount())
#elif defined(ARDUINO)
                                              millis()
#else
                                              0
#endif
      ;

  const uint32_t milliseconds_in_range = raw_millis & kTimeMask;

  const uint32_t milliseconds = milliseconds_in_range % 1000;
  const uint32_t total_seconds = milliseconds_in_range / 1000;

  const uint32_t seconds = total_seconds % 60;
  const uint32_t total_minutes = total_seconds / 60;

  const uint32_t minutes = total_minutes % 60;
  const uint32_t hours = total_minutes / 60;

  buffer[0] = '0' + hours / 10;
  buffer[1] = '0' + hours % 10;
  buffer[2] = '.';

  buffer[3] = '0' + minutes / 10;
  buffer[4] = '0' + minutes % 10;
  buffer[5] = '.';

  buffer[6] = '0' + seconds / 10;
  buffer[7] = '0' + seconds % 10;
  buffer[8] = '.';

  buffer[9] = '0' + milliseconds / 100;
  buffer[10] = '0' + (milliseconds / 10) % 10;
  buffer[11] = '0' + milliseconds % 10;
}

inline void Log(const char* fmt, ...) noexcept {
  constexpr size_t kTimestampSize = 12;
  char buffer[format_buffer_size()];
  FormatTimestamp(buffer);

  va_list args;
  va_start(args, fmt);
  auto length = vsnprintf(buffer + kTimestampSize, sizeof(buffer) - kTimestampSize, fmt, args) + kTimestampSize;
  va_end(args);

  if (length >= sizeof(buffer)) {
    length = sizeof(buffer);
    buffer[format_buffer_size() - 1] = '\n';
  }

  if (log_sink()) {
    log_sink()(buffer, length);
  } else {
#if defined(ARDUINO_ARCH_ESP32)
    fwrite(buffer, 1, length, stdout);
#elif defined(ARDUINO)
    Serial.write(buffer, length);
#else
    fwrite(buffer, 1, length, stdout);
#endif
  }
}
}  // namespace log
}  // namespace cyf

#if CYF_LOG_SEVERITY <= CYF_LOG_SEVERITY_VERBOSE
#define CLOGV(fmt, ...)                                                                                                 \
  cyf::log::Log(" V %s:%d %s] " fmt "\n", __FILE__ + cyf::log::ExtractFileNameOffset(__FILE__), __LINE__, __FUNCTION__, \
                ##__VA_ARGS__)
#else
#define CLOGV(fmt, ...) (void(0))
#endif

#if CYF_LOG_SEVERITY <= CYF_LOG_SEVERITY_DEBUG
#define CLOGD(fmt, ...)                                                                                                 \
  cyf::log::Log(" D %s:%d %s] " fmt "\n", __FILE__ + cyf::log::ExtractFileNameOffset(__FILE__), __LINE__, __FUNCTION__, \
                ##__VA_ARGS__)
#else
#define CLOGD(fmt, ...) (void(0))
#endif

#if CYF_LOG_SEVERITY <= CYF_LOG_SEVERITY_INFO
#define CLOGI(fmt, ...)                                                                                                 \
  cyf::log::Log(" I %s:%d %s] " fmt "\n", __FILE__ + cyf::log::ExtractFileNameOffset(__FILE__), __LINE__, __FUNCTION__, \
                ##__VA_ARGS__)
#else
#define CLOGI(fmt, ...) (void(0))
#endif

#if CYF_LOG_SEVERITY <= CYF_LOG_SEVERITY_WARN
#define CLOGW(fmt, ...)                                                                                                 \
  cyf::log::Log(" W %s:%d %s] " fmt "\n", __FILE__ + cyf::log::ExtractFileNameOffset(__FILE__), __LINE__, __FUNCTION__, \
                ##__VA_ARGS__)
#else
#define CLOGW(fmt, ...) (void(0))
#endif

#if CYF_LOG_SEVERITY <= CYF_LOG_SEVERITY_ERROR
#define CLOGE(fmt, ...)                                                                                                 \
  cyf::log::Log(" E %s:%d %s] " fmt "\n", __FILE__ + cyf::log::ExtractFileNameOffset(__FILE__), __LINE__, __FUNCTION__, \
                ##__VA_ARGS__)
#else
#define CLOGE(fmt, ...) (void(0))
#endif

#endif