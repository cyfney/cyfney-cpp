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
class RingBufferView {
  static_assert(std::is_trivially_copyable_v<T>, "RingBufferView requires trivially copyable type");

 public:
  constexpr explicit RingBufferView(T* buffer, size_t capacity) noexcept : buffer_(buffer), capacity_mask_(capacity - 1) {
    assert(buffer != nullptr);
    assert(capacity >= 4);
    assert((capacity & (capacity - 1)) == 0);  // capacity must be a power of two
  }

  bool empty() const noexcept { return read_pos_ == write_pos_; }

  bool full() const noexcept { return count() == capacity_mask_; }

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
      // If writing more data than the total buffer size,
      // discard all old data and keep only the last (N - 1) elements.
      if (count >= buffer_size()) {
        const size_t start_index = count - capacity_mask_;
        read_pos_ = 0;
        write_pos_ = 0;
        return WriteBlock(data + start_index, capacity_mask_);
      } else {
        if (count > free) {
          read_pos_ = (read_pos_ + (count - free)) & capacity_mask_;
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
    const size_t first_chunk = std::min(read_count, capacity_mask_ + 1 - read_pos_);
    std::memcpy(dest, buffer_ + read_pos_, first_chunk * sizeof(T));
    if (read_count > first_chunk) {
      std::memcpy(dest + first_chunk, buffer_, (read_count - first_chunk) * sizeof(T));
    }

    read_pos_ = (read_pos_ + read_count) & capacity_mask_;
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
    const size_t first_chunk = std::min(read_count, capacity_mask_ + 1 - read_pos_);
    std::memcpy(dest, buffer_ + read_pos_, first_chunk * sizeof(T));
    if (read_count > first_chunk) {
      std::memcpy(dest + first_chunk, buffer_, (read_count - first_chunk) * sizeof(T));
    }

    return read_count;
  }

  T& front() noexcept { return buffer_[read_pos_]; }

  const T& front() const noexcept { return buffer_[read_pos_]; }

  T& operator[](size_t index) noexcept { return buffer_[(read_pos_ + index) & capacity_mask_]; }

  const T& operator[](size_t index) const noexcept { return buffer_[(read_pos_ + index) & capacity_mask_]; }

  void Advance(size_t count = 1) noexcept { read_pos_ = (read_pos_ + count) & capacity_mask_; }

  T Read() noexcept {
    T value = buffer_[read_pos_];
    read_pos_ = (read_pos_ + 1) & capacity_mask_;
    return value;
  }

  void Clear() noexcept { write_pos_ = read_pos_; }

  size_t count() const noexcept { return (write_pos_ - read_pos_) & capacity_mask_; }

  size_t size() const noexcept { return count(); }

  /// @return Maximum number of elements that can be stored (N - 1)
  size_t capacity() const noexcept { return capacity_mask_; }

  size_t buffer_size() const noexcept { return capacity_mask_ + 1; }

  size_t free_space() const noexcept { return capacity_mask_ - count(); }

 private:
  RingBufferView(const RingBufferView&) = delete;
  RingBufferView& operator=(const RingBufferView&) = delete;

  size_t WriteBlock(const T* data, size_t count) noexcept {
    if (count == 0) {
      return 0;
    }

    const size_t first_chunk = std::min(count, capacity_mask_ + 1 - write_pos_);
    std::memcpy(buffer_ + write_pos_, data, first_chunk * sizeof(T));
    if (count > first_chunk) {
      std::memcpy(buffer_, data + first_chunk, (count - first_chunk) * sizeof(T));
    }

    write_pos_ = (write_pos_ + count) & capacity_mask_;
    return count;
  }

  T* const buffer_ = nullptr;
  const size_t capacity_mask_ = 0;
  size_t read_pos_ = 0;
  size_t write_pos_ = 0;
};

template <typename T, size_t N>
class RingBuffer : public RingBufferView<T> {
 public:
  static_assert(N >= 4 && (N & (N - 1)) == 0, "RingBuffer<N, T>: N must be a power of two and >= 4");
  constexpr RingBuffer() noexcept : RingBufferView<T>(buffer_, N) {}

 private:
  T buffer_[N];
};
}  // namespace cyf
#endif