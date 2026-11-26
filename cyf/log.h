#pragma once

#ifndef CYF_LOG_H_
#define CYF_LOG_H_

// Lightweight printf-style logging with millisecond timestamps.
//
// Output routing (first match wins):
//   1. user sink installed via set_log_sink()
//   2. platform default: ESP32 -> stdout, Arduino -> Serial,
//      bare metal -> write() (i.e. the project's _write() retarget)
//
// Time source: set_millis_source() on bare metal, millis() on Arduino,
// pdTICKS_TO_MS(xTaskGetTickCount()) on ESP32. Timestamps cover up to
// 2^28 ms (~74.9 h) and render as "HH.MM.SS.mmm".
//
// Verbosity is compile-time: CYF_LOG_LEVEL selects the highest enabled
// level; CLOGx macros below it expand to nothing.

#define CYF_LOG_LEVEL_NONE (0)
#define CYF_LOG_LEVEL_ERROR (1)
#define CYF_LOG_LEVEL_WARN (2)
#define CYF_LOG_LEVEL_INFO (3)
#define CYF_LOG_LEVEL_DEBUG (4)
#define CYF_LOG_LEVEL_VERBOSE (5)

// CYF_LOG_LEVEL = highest verbosity compiled into the binary.
// NONE (0) disables all logging; higher values enable more CLOGx macros.
#ifndef CYF_LOG_LEVEL
#define CYF_LOG_LEVEL (CYF_LOG_LEVEL_INFO)
#endif

#if defined(ARDUINO) && !defined(ARDUINO_ARCH_ESP32)
#include <Arduino.h>
#endif

#if defined(ARDUINO_ARCH_AVR)
#include <avr/pgmspace.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#else
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#endif

#if !defined(ARDUINO)
#include <unistd.h>  // write() for the bare-metal default sink
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
  millis_source() = source;
}

using LogSink = void (*)(const char* message, size_t length);

inline LogSink& log_sink() noexcept {
  static LogSink s_log_sink = nullptr;
  return s_log_sink;
}

inline void set_log_sink(LogSink sink) noexcept {
  log_sink() = sink;
}

inline size_t& format_buffer_size() noexcept {
#if defined(ARDUINO_ARCH_AVR)
  static size_t s_format_buffer_size = 256;  // RAM-constrained default
#else
  static size_t s_format_buffer_size = 512;
#endif
  return s_format_buffer_size;
}

inline void set_format_buffer_size(size_t size) noexcept {
  format_buffer_size() = size;
}

inline void FormatTimestamp(char* buffer) noexcept {
  constexpr int kShiftBits = 28;
  // Two-digit hours require uptime < 2^28 ms; kShiftBits must not exceed 28.
  static_assert(kShiftBits <= 28, "two-digit hours requires kShiftBits <= 28");
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

  // Constant-multiplication division: floor(x/d) == (x * ceil(2^32/d)) >> 32,
  // exact over the entire masked input range. Remainders are derived by
  // subtracting the scaled quotient, so no runtime division or modulo is
  // needed — relevant on cores without a hardware divider.
  const uint32_t x = raw_millis & kTimeMask;

  const uint32_t total_seconds = static_cast<uint32_t>(((uint64_t)(x >> 3) * 34359739ULL) >> 32);       // x/1000 = (x/8)/125
  const uint32_t total_minutes = static_cast<uint32_t>(((uint64_t)total_seconds * 71582789ULL) >> 32);  // /60
  const uint32_t hours = static_cast<uint32_t>(((uint64_t)total_minutes * 71582789ULL) >> 32);          // /60

  const uint32_t milliseconds = x - 1000 * total_seconds;
  const uint32_t seconds = total_seconds - 60 * total_minutes;
  const uint32_t minutes = total_minutes - 60 * hours;

  // Two-digit split (n < 100): tens = (n * 205) >> 11 == n / 10.
  const auto write_two = [buffer](uint32_t pos, uint32_t n) {
    const uint32_t tens = (n * 205) >> 11;
    buffer[pos] = static_cast<char>('0' + tens);
    buffer[pos + 1] = static_cast<char>('0' + n - 10 * tens);
  };
  write_two(0, hours);
  buffer[2] = '.';
  write_two(3, minutes);
  buffer[5] = '.';
  write_two(6, seconds);
  buffer[8] = '.';

  const uint32_t ms_tens = (milliseconds * 205) >> 11;      // ms / 10
  const uint32_t ms_hundreds = (ms_tens * 205) >> 11;       // ms / 100
  buffer[9] = static_cast<char>('0' + ms_hundreds);
  buffer[10] = static_cast<char>('0' + ms_tens - 10 * ms_hundreds);
  buffer[11] = static_cast<char>('0' + milliseconds - 10 * ms_tens);
}

inline void Log(const char* fmt, ...) noexcept {
  constexpr size_t kTimestampSize = 12;
  char buffer[format_buffer_size()];
  FormatTimestamp(buffer);

  va_list args;
  va_start(args, fmt);
#if defined(ARDUINO_ARCH_AVR)
  // On AVR fmt lives in flash (the CLOGx macros pass it via PSTR); %s
  // arguments still point to RAM. This halves RAM use: literals would
  // otherwise be copied to RAM at startup.
  auto length = vsnprintf_P(buffer + kTimestampSize, sizeof(buffer) - kTimestampSize, fmt, args) + kTimestampSize;
#else
  auto length = vsnprintf(buffer + kTimestampSize, sizeof(buffer) - kTimestampSize, fmt, args) + kTimestampSize;
#endif
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
    // Bare metal: write() goes straight to the _write() retarget, bypassing
    // the newlib FILE layer (fwrite pulls in extra code, stack usage and a
    // malloc'ed stdout buffer).
    write(STDOUT_FILENO, buffer, length);
#endif
  }
}
}  // namespace log
}  // namespace cyf

// Shared implementation for the CLOGx macros. On AVR the whole format string
// (prefix + user fmt) goes to flash via PSTR, and __FILE__ too; otherwise
// plain RAM literals are used. Note: vsnprintf_P only reads the format string
// from flash — %s arguments must still point to RAM — so the flash-resident
// file name is passed with %S (avr-libc: string located in program memory).
#if defined(ARDUINO_ARCH_AVR)
#define CYF_LOG_IMPL(prefix, fmt, ...)                                                                                         \
  cyf::log::Log(PSTR(" " prefix " %S:%d %s] " fmt "\n"), PSTR(__FILE__) + cyf::log::ExtractFileNameOffset(__FILE__), __LINE__, \
                __FUNCTION__, ##__VA_ARGS__)
#else
#define CYF_LOG_IMPL(prefix, fmt, ...)                                                                             \
  cyf::log::Log(" " prefix " %s:%d %s] " fmt "\n", __FILE__ + cyf::log::ExtractFileNameOffset(__FILE__), __LINE__, \
                __FUNCTION__, ##__VA_ARGS__)
#endif

#if CYF_LOG_LEVEL_ERROR <= CYF_LOG_LEVEL
#define CLOGE(fmt, ...) CYF_LOG_IMPL("E", fmt, ##__VA_ARGS__)
#else
#define CLOGE(fmt, ...) (void(0))
#endif

#if CYF_LOG_LEVEL_WARN <= CYF_LOG_LEVEL
#define CLOGW(fmt, ...) CYF_LOG_IMPL("W", fmt, ##__VA_ARGS__)
#else
#define CLOGW(fmt, ...) (void(0))
#endif

#if CYF_LOG_LEVEL_INFO <= CYF_LOG_LEVEL
#define CLOGI(fmt, ...) CYF_LOG_IMPL("I", fmt, ##__VA_ARGS__)
#else
#define CLOGI(fmt, ...) (void(0))
#endif

#if CYF_LOG_LEVEL_DEBUG <= CYF_LOG_LEVEL
#define CLOGD(fmt, ...) CYF_LOG_IMPL("D", fmt, ##__VA_ARGS__)
#else
#define CLOGD(fmt, ...) (void(0))
#endif

#if CYF_LOG_LEVEL_VERBOSE <= CYF_LOG_LEVEL
#define CLOGV(fmt, ...) CYF_LOG_IMPL("V", fmt, ##__VA_ARGS__)
#else
#define CLOGV(fmt, ...) (void(0))
#endif

#endif