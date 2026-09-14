#pragma once

#ifndef CYF_RING_BUFFER_H
#define CYF_RING_BUFFER_H

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstring>
#include <type_traits>

namespace cyf {
template <typename T>
class RingBufferCore {
  static_assert(std::is_trivially_copyable<T>::value, "RingBufferCore requires trivially copyable type");

 public:
  constexpr explicit RingBufferCore(T* buffer, size_t buffer_size) noexcept : buffer_(buffer), max_size_(buffer_size - 1) {
    assert(buffer != nullptr);
    assert(buffer_size >= 4);
    assert((buffer_size & (buffer_size - 1)) == 0);  // buffer_size must be a power of two
  }

  size_t Write(const T& data, bool overwrite = false) noexcept { return Write(&data, 1, overwrite); }

  size_t Write(const T* data, size_t count, bool overwrite = false) noexcept {
    if (data == nullptr || count == 0) {
      return 0;
    }

    const size_t free = this->free();

    if (!overwrite) {
      const size_t write_count = (count > free) ? free : count;
      return write_count > 0 ? WriteBlock(data, write_count) : 0;
    } else {
      // If writing more data than the total buffer size,
      // discard all old data and keep only the last (N - 1) elements.
      if (count > max_size_) {
        const size_t start_index = count - max_size_;
        read_pos_ = 0;
        write_pos_ = 0;
        return WriteBlock(data + start_index, max_size_);
      } else {
        if (count > free) {
          read_pos_ = (read_pos_ + (count - free)) & max_size_;
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
    const size_t first_chunk = std::min(read_count, max_size_ + 1 - read_pos_);
    std::memcpy(dest, buffer_ + read_pos_, first_chunk * sizeof(T));
    if (read_count > first_chunk) {
      std::memcpy(dest + first_chunk, buffer_, (read_count - first_chunk) * sizeof(T));
    }

    read_pos_ = (read_pos_ + read_count) & max_size_;
    return read_count;
  }

  T Read() noexcept {
    T value = buffer_[read_pos_];
    read_pos_ = (read_pos_ + 1) & max_size_;
    return value;
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
    const size_t first_chunk = std::min(read_count, max_size_ + 1 - read_pos_);
    std::memcpy(dest, buffer_ + read_pos_, first_chunk * sizeof(T));
    if (read_count > first_chunk) {
      std::memcpy(dest + first_chunk, buffer_, (read_count - first_chunk) * sizeof(T));
    }

    return read_count;
  }

  const T& Peek(size_t index = 0) const noexcept { return buffer_[(read_pos_ + index) & max_size_]; }

  T& Peek(size_t index = 0) noexcept { return buffer_[(read_pos_ + index) & max_size_]; }

  size_t Push(const T& data, bool overwrite = false) noexcept { return Write(data, overwrite); }

  size_t Pop(T* dest, size_t count) noexcept { return Read(dest, count); }

  T Pop() noexcept { return Read(); }

  T& front() noexcept { return buffer_[read_pos_]; }

  const T& front() const noexcept { return buffer_[read_pos_]; }

  T& operator[](size_t index) noexcept { return buffer_[(read_pos_ + index) & max_size_]; }

  const T& operator[](size_t index) const noexcept { return buffer_[(read_pos_ + index) & max_size_]; }

  void Consume(size_t count = 1) noexcept { read_pos_ = (read_pos_ + count) & max_size_; }

  void Reset() noexcept { write_pos_ = read_pos_; }

  bool empty() const noexcept { return read_pos_ == write_pos_; }

  bool full() const noexcept { return count() == max_size_; }

  size_t count() const noexcept { return (write_pos_ - read_pos_) & max_size_; }

  size_t size() const noexcept { return count(); }

  /**
   * The maximum number of elements that can be stored in the buffer
   * @note The underlying buffer size is `max_size() + 1`, which must be a power of two.
   *       One slot is left unused to distinguish between full and empty states.
   */
  size_t max_size() const noexcept { return max_size_; }

  size_t free() const noexcept { return max_size_ - count(); }

 private:
  RingBufferCore(const RingBufferCore&) = delete;
  RingBufferCore& operator=(const RingBufferCore&) = delete;

  size_t WriteBlock(const T* data, size_t count) noexcept {
    if (count == 0) {
      return 0;
    }

    const size_t first_chunk = std::min(count, max_size_ + 1 - write_pos_);
    std::memcpy(buffer_ + write_pos_, data, first_chunk * sizeof(T));
    if (count > first_chunk) {
      std::memcpy(buffer_, data + first_chunk, (count - first_chunk) * sizeof(T));
    }

    write_pos_ = (write_pos_ + count) & max_size_;
    return count;
  }

  T* const buffer_ = nullptr;
  const size_t max_size_ = 0;
  size_t read_pos_ = 0;
  size_t write_pos_ = 0;
};

template <typename T, size_t N>
class RingBuffer : public RingBufferCore<T> {
 public:
  static_assert(N >= 4 && (N & (N - 1)) == 0, "RingBuffer<N, T>: N must be a power of two and >= 4");
  constexpr RingBuffer() noexcept : RingBufferCore<T>(buffer_, N) {}

 private:
  T buffer_[N];
};
}  // namespace cyf
#endif