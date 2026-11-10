#pragma once

#ifndef CYF_FIFO_H_
#define CYF_FIFO_H_

#if defined(__AVR__)
#include <assert.h>
#include <stddef.h>
#include <string.h>
#else
#include <cassert>
#include <cstddef>
#include <cstring>
#include <type_traits>
#endif

namespace cyf {

// Single-threaded FIFO. The core API (Push/Pop/Clear) mirrors
// cyf::lockfree::spsc::Fifo; extras (overwrite, peek, random access) are
// additions on top of it.
template <typename T>
class FifoCore {
#if !defined(__AVR__)
  static_assert(std::is_trivially_copyable<T>::value, "FifoCore requires trivially copyable type");
#endif

 public:
  constexpr explicit FifoCore(T* buffer, size_t buffer_size) noexcept : buffer_(buffer), max_size_(buffer_size - 1) {
#if defined(__cplusplus) && __cplusplus >= 202002L
    assert(buffer != nullptr);
    assert(buffer_size >= 2);
    assert((buffer_size & (buffer_size - 1)) == 0);  // buffer_size must be a power of two
#endif
  }

  // ------------------------------------------------------------------
  // Core API — signatures match cyf::lockfree::spsc::Fifo
  // ------------------------------------------------------------------

  bool Push(const T& data) noexcept { return 1 == Push(&data, 1); }

  size_t Push(const T* data, size_t count) noexcept {
    if (data == nullptr || count == 0) {
      return 0;
    }

    const size_t free_count = free();
    if (count > free_count) {
      count = free_count;
    }
    return count > 0 ? WriteBlock(data, count) : 0;
  }

  bool Pop(T& data) noexcept { return 1 == Pop(&data, 1); }

  size_t Pop(T* dest, size_t count) noexcept {
    if (dest == nullptr || count == 0) {
      return 0;
    }

    const size_t available = this->count();
    if (count > available) {
      count = available;
    }
    if (count == 0) {
      return 0;
    }

    ReadBlock(dest, count);
    read_pos_ = (read_pos_ + count) & max_size_;
    return count;
  }

  // Drops every item not yet consumed (same semantics as spsc Fifo::Clear).
  void Clear() noexcept { write_pos_ = read_pos_; }

  // ------------------------------------------------------------------
  // Extras beyond the spsc API
  // ------------------------------------------------------------------

  bool Push(const T& data, bool overwrite) noexcept { return 1 == Push(&data, 1, overwrite); }

  size_t Push(const T* data, size_t count, bool overwrite) noexcept {
    if (data == nullptr || count == 0) {
      return 0;
    }

    if (!overwrite) {
      return Push(data, count);
    }

    // If writing more data than the total buffer size,
    // discard all old data and keep only the last max_size_ elements.
    if (count > max_size_) {
      read_pos_ = 0;
      write_pos_ = 0;
      return WriteBlock(data + (count - max_size_), max_size_);
    }

    const size_t free_count = free();
    if (count > free_count) {
      read_pos_ = (read_pos_ + (count - free_count)) & max_size_;
    }
    return WriteBlock(data, count);
  }

  // Reads up to count elements without consuming them.
  size_t Peek(T* dest, size_t count) const noexcept {
    if (dest == nullptr || count == 0) {
      return 0;
    }

    const size_t available = this->count();
    if (count > available) {
      count = available;
    }
    return count > 0 ? ReadBlock(dest, count) : 0;
  }

  const T& Peek(size_t index = 0) const noexcept { return buffer_[(read_pos_ + index) & max_size_]; }

  T& Peek(size_t index = 0) noexcept { return buffer_[(read_pos_ + index) & max_size_]; }

  T& front() noexcept { return buffer_[read_pos_]; }

  const T& front() const noexcept { return buffer_[read_pos_]; }

  T& operator[](size_t index) noexcept { return buffer_[(read_pos_ + index) & max_size_]; }

  const T& operator[](size_t index) const noexcept { return buffer_[(read_pos_ + index) & max_size_]; }

  // Moves the read pointer forward without copying (useful for DMA).
  void Consume(size_t count = 1) noexcept { read_pos_ = (read_pos_ + count) & max_size_; }

  // ------------------------------------------------------------------
  // Queries
  // ------------------------------------------------------------------

  bool empty() const noexcept { return read_pos_ == write_pos_; }

  bool full() const noexcept { return count() == max_size_; }

  size_t count() const noexcept { return (write_pos_ - read_pos_) & max_size_; }

  /**
   * The maximum number of elements that can be stored in the buffer
   * @note The underlying buffer size is `max_size() + 1`, which must be a power of two.
   *       One slot is left unused to distinguish between full and empty states.
   */
  size_t max_size() const noexcept { return max_size_; }

  size_t free() const noexcept { return max_size_ - count(); }

 private:
  FifoCore(const FifoCore&) = delete;
  FifoCore& operator=(const FifoCore&) = delete;

  size_t WriteBlock(const T* data, size_t count) noexcept {
    if (count == 0) {
      return 0;
    }

    const size_t to_end = max_size_ + 1 - write_pos_;
    const size_t first_chunk = (count < to_end) ? count : to_end;
    memcpy(buffer_ + write_pos_, data, first_chunk * sizeof(T));
    if (count > first_chunk) {
      memcpy(buffer_, data + first_chunk, (count - first_chunk) * sizeof(T));
    }

    write_pos_ = (write_pos_ + count) & max_size_;
    return count;
  }

  // Copies count elements starting at read_pos_ without advancing it.
  size_t ReadBlock(T* dest, size_t count) const noexcept {
    const size_t to_end = max_size_ + 1 - read_pos_;
    const size_t first_chunk = (count < to_end) ? count : to_end;
    memcpy(dest, buffer_ + read_pos_, first_chunk * sizeof(T));
    if (count > first_chunk) {
      memcpy(dest + first_chunk, buffer_, (count - first_chunk) * sizeof(T));
    }
    return count;
  }

  T* const buffer_ = nullptr;
  const size_t max_size_ = 0;
  size_t read_pos_ = 0;
  size_t write_pos_ = 0;
};

template <typename T, size_t N>
class Fifo : public FifoCore<T> {
 public:
  static_assert(N >= 2 && (N & (N - 1)) == 0, "Fifo<T, N>: N must be a power of two and >= 2");
  constexpr Fifo() noexcept : FifoCore<T>(buffer_, N) {}

 private:
  T buffer_[N];
};
}  // namespace cyf
#endif
