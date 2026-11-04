#pragma once

#ifndef CYF_BUFFER_VIEW_H_
#define CYF_BUFFER_VIEW_H_

#include <stddef.h>
#include <stdint.h>

namespace cyf {
class BufferView {
 public:
  constexpr BufferView(const uint8_t* data, size_t length) noexcept : data_(data), length_(length) {}
  const uint8_t* data() const noexcept { return data_; }
  size_t length() const noexcept { return length_; }
  size_t size() const noexcept { return length_; }

 private:
  const uint8_t* data_;
  const size_t length_;
};
}  // namespace cyf
#endif
