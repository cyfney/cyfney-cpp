#pragma once

#ifndef CYF_BUFFER_VIEW_H
#define CYF_BUFFER_VIEW_H

#include <stdint.h>

#if defined(ARDUINO)
#include <Print.h>
#endif

namespace cyf {
class BufferView {
 public:
  constexpr BufferView(const uint8_t* data, size_t length) noexcept : data_(data), length_(length) {}
  const uint8_t* data() const noexcept { return data_; }
  size_t length() const noexcept { return length_; }
  size_t size() const noexcept { return length_; }
#if defined(ARDUINO)
  size_t WriteTo(Print& print) const noexcept { return print.write(data_, length_); }
#endif

 private:
  const uint8_t* data_;
  const size_t length_;
};
}  // namespace cyf
#endif
