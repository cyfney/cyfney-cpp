#pragma once

#ifndef CYF_BYTE_BUFFER_H_
#define CYF_BYTE_BUFFER_H_

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

namespace cyf {
class ByteBuffer {
 public:
  explicit ByteBuffer(size_t size) noexcept : buffer_(reinterpret_cast<uint8_t*>(malloc(size))), size_(size) {
    assert(buffer_ != nullptr);
  }

  ~ByteBuffer() noexcept {
    if (buffer_ != nullptr) {
      free(buffer_);
    }
  }

  ByteBuffer(ByteBuffer&& other) noexcept : buffer_(other.buffer_), size_(other.size_) {
    other.buffer_ = nullptr;
    other.size_ = 0;
  }

  ByteBuffer& operator=(ByteBuffer&& other) noexcept {
    if (this != &other) {
      if (buffer_ != nullptr) {
        free(buffer_);
      }
      buffer_ = other.buffer_;
      size_ = other.size_;
      other.buffer_ = nullptr;
      other.size_ = 0;
    }
    return *this;
  }

  void Resize(size_t size) {
    buffer_ = reinterpret_cast<uint8_t*>(realloc(buffer_, size));
    assert(buffer_ != nullptr);
    size_ = size;
  }

  uint8_t* buffer() noexcept { return buffer_; }
  const uint8_t* buffer() const noexcept { return buffer_; }
  uint8_t* data() noexcept { return buffer_; }
  const uint8_t* data() const noexcept { return buffer_; }
  size_t size() const noexcept { return size_; }
  const uint8_t& operator[](size_t pos) const noexcept { return *(buffer_ + pos); }
  uint8_t& operator[](size_t pos) noexcept { return *(buffer_ + pos); }

 private:
  ByteBuffer(const ByteBuffer&) = delete;
  ByteBuffer& operator=(const ByteBuffer&) = delete;

  uint8_t* buffer_ = nullptr;
  size_t size_ = 0;
};
}  // namespace cyf
#endif
