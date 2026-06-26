#pragma once

#ifndef CFYNEY_CORE_RING_BUFFER_H
#define CFYNEY_CORE_RING_BUFFER_H

#include <cassert>
#include <cstddef>
#include <cstring>
#include <type_traits>

#if __cplusplus >= 202002L
#include <bit>
#endif

namespace cfyney {
namespace core {
template <typename T>
class RingBuffer {
  static_assert(std::is_trivially_copyable_v<T>, "RingBuffer requires trivially copyable type");

 public:
  constexpr explicit RingBuffer(const size_t capacity) noexcept : capacity_(RoundUpToPowerOfTwo(capacity + 1) - 1) {
    assert(capacity_ > 0);
  }

  ~RingBuffer() noexcept { delete[] buffer_; }

  void Init() noexcept {
    assert(buffer_ == nullptr);
    buffer_ = new T[capacity_ + 1];
    assert(buffer_ != nullptr);
  }

  bool empty() const noexcept { return read_pos_ == write_pos_; }

  bool full() const noexcept { return count() == capacity_; }

  size_t Write(const T& data, bool overwrite = false) noexcept { return Write(&data, 1, overwrite); }

  size_t Write(const T* data, size_t count, bool overwrite = false) noexcept {
    if (data == nullptr || count == 0) {
      return 0;
    }

    const size_t free = free_space();

    if (!overwrite) {
      const size_t write_count = (count > free) ? free : count;
      return write_count > 0 ? WriteBlock(data, write_count) : 0;
    } else {
      if (count >= capacity_ + 1) {
        const size_t start_index = count - capacity_;
        read_pos_ = 0;
        write_pos_ = 0;
        return WriteBlock(data + start_index, capacity_);
      } else {
        if (count > free) {
          read_pos_ = (read_pos_ + (count - free)) & capacity_;
        }
        return WriteBlock(data, count);
      }
    }
  }

  size_t Read(T* dest, size_t count) noexcept {
    if (dest == nullptr || count == 0) {
      return 0;
    }

    const size_t available = this->count();

    if (available == 0) {
      return 0;
    }

    const size_t read_count = std::min(count, available);
    const size_t first_chunk = std::min(read_count, capacity_ + 1 - read_pos_);
    std::memcpy(dest, buffer_ + read_pos_, first_chunk * sizeof(T));
    if (read_count > first_chunk) {
      std::memcpy(dest + first_chunk, buffer_, (read_count - first_chunk) * sizeof(T));
    }

    read_pos_ = (read_pos_ + read_count) & capacity_;
    return read_count;
  }

  size_t Peek(T* dest, size_t count) const noexcept {
    if (dest == nullptr || count == 0) {
      return 0;
    }

    const size_t available = this->count();

    if (available == 0) {
      return 0;
    }

    const size_t read_count = std::min(count, available);
    const size_t first_chunk = std::min(read_count, capacity_ + 1 - read_pos_);
    std::memcpy(dest, buffer_ + read_pos_, first_chunk * sizeof(T));
    if (read_count > first_chunk) {
      std::memcpy(dest + first_chunk, buffer_, (read_count - first_chunk) * sizeof(T));
    }

    return read_count;
  }

  T& front() noexcept { return buffer_[read_pos_]; }

  const T& front() const noexcept { return buffer_[read_pos_]; }

  T& operator[](size_t index) noexcept { return buffer_[(read_pos_ + index) & capacity_]; }

  const T& operator[](size_t index) const noexcept { return buffer_[(read_pos_ + index) & capacity_]; }

  void Advance(size_t count = 1) noexcept { read_pos_ = (read_pos_ + count) & capacity_; }

  T Read() noexcept {
    T value = buffer_[read_pos_];
    read_pos_ = (read_pos_ + 1) & capacity_;
    return value;
  }

  void Clear() noexcept { write_pos_ = read_pos_; }

  size_t count() const noexcept { return (write_pos_ - read_pos_) & capacity_; }

  size_t capacity() const noexcept { return capacity_; }

  size_t free_space() const noexcept { return capacity_ - count(); }

 private:
  RingBuffer(const RingBuffer&) = delete;
  RingBuffer& operator=(const RingBuffer&) = delete;

  constexpr static size_t RoundUpToPowerOfTwo(size_t v) noexcept {
#if __cplusplus >= 202002L
    return std::bit_ceil<size_t>(v);
#else
    return v <= 1 ? 1
                  : ((v - 1) | ((v - 1) >> 1) | ((v - 1) >> 2) | ((v - 1) >> 4) | ((v - 1) >> 8)
#if SIZE_MAX >= 0xFFFFFFFF
                     | ((v - 1) >> 16)
#if SIZE_MAX >= 0xFFFFFFFFFFFFFFFF
                     | ((v - 1) >> 32)
#endif
#endif
                         ) +
                        1;
#endif
  }

  size_t WriteBlock(const T* data, size_t count) noexcept {
    if (count == 0) {
      return 0;
    }

    const size_t first_chunk = std::min(count, capacity_ + 1 - write_pos_);
    std::memcpy(buffer_ + write_pos_, data, first_chunk * sizeof(T));
    if (count > first_chunk) {
      std::memcpy(buffer_, data + first_chunk, (count - first_chunk) * sizeof(T));
    }

    write_pos_ = (write_pos_ + count) & capacity_;
    return count;
  }

  size_t capacity_ = 0;
  T* buffer_ = nullptr;
  volatile size_t read_pos_ = 0;
  volatile size_t write_pos_ = 0;
};

}  // namespace core
}  // namespace cfyney
#endif