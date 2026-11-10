#pragma once

#ifndef CYF_LOCKFREE_SPSC_FIFO_H_
#define CYF_LOCKFREE_SPSC_FIFO_H_

#if defined(__AVR__)
#include <assert.h>
#include <stddef.h>
#include <string.h>
#else
#include <atomic>
#include <cassert>
#include <cstddef>
#include <cstring>
#include <type_traits>
#endif

namespace cyf {
namespace lockfree {
namespace spsc {

// Index type: byte-sized on AVR (the only atomic width there, and the avr-gcc
// 7.3 toolchain cannot even link 4-byte __atomic ops); 32-bit elsewhere.
#if defined(__AVR__)
using FifoIndex = uint8_t;
#else
using FifoIndex = uint32_t;
#endif

template <typename T>
class FifoCore {
#if !defined(__AVR__)
  static_assert(std::is_trivially_copyable<T>::value, "FifoCore requires trivially copyable type");
#endif

 public:
  constexpr explicit FifoCore(T* buffer, FifoIndex buffer_size) noexcept
      : buffer_(buffer), max_size_(buffer_size - 1) {
#if defined(__cplusplus) && __cplusplus >= 202002L
    assert(buffer != nullptr);
    assert(buffer_size >= 2);
    assert((buffer_size & (buffer_size - 1)) == 0);  // buffer_size must be a power of two
#endif
  }

  bool Push(const T& data) noexcept { return 1 == Push(&data, 1); }

  size_t Push(const T* data, size_t count) noexcept {
    if (data == nullptr || count == 0) {
      return 0;
    }

    const FifoIndex free = max_size_ - (in_ - ReadOnce(out_));

    if (count > free) {
      count = static_cast<size_t>(free);
    }
    if (count == 0) {
      return 0;
    }

    const FifoIndex pos = in_ & max_size_;
    const FifoIndex to_end = max_size_ + 1 - pos;
    const FifoIndex first = (to_end < count) ? to_end : static_cast<FifoIndex>(count);

    memcpy(buffer_ + pos, data, first * sizeof(T));
    if (count > first) {
      memcpy(buffer_, data + first, (count - first) * sizeof(T));
    }

    // Release store in WriteOnce publishes the data above.
    WriteOnce(in_, static_cast<FifoIndex>(in_ + count));
    return count;
  }

  bool Pop(T& data) noexcept { return 1 == Pop(&data, 1); }

  size_t Pop(T* dest, size_t count) noexcept {
    if (dest == nullptr || count == 0) {
      return 0;
    }

    const FifoIndex available = ReadOnce(in_) - out_;

    if (count > available) {
      count = static_cast<size_t>(available);
    }
    if (count == 0) {
      return 0;
    }

    const FifoIndex pos = out_ & max_size_;
    const FifoIndex to_end = max_size_ + 1 - pos;
    const FifoIndex first = (to_end < count) ? to_end : static_cast<FifoIndex>(count);

    memcpy(dest, buffer_ + pos, first * sizeof(T));
    if (count > first) {
      memcpy(dest + first, buffer_, (count - first) * sizeof(T));
    }

    // Release store in WriteOnce publishes the consumed data above.
    WriteOnce(out_, static_cast<FifoIndex>(out_ + count));
    return count;
  }

  // Drops every item not yet consumed. MUST be called from the consumer task
  // (the only writer of out_): out_ is reset to the current in_, which always
  // keeps available >= 0, so it is safe against concurrent producer Push.
  void Clear() noexcept { WriteOnce(out_, ReadOnce(in_)); }

  // Next element to be popped. Consumer-side only (like Clear/Pop).
  T& front() noexcept { return buffer_[out_ & max_size_]; }

  const T& front() const noexcept { return buffer_[out_ & max_size_]; }

  bool empty() const noexcept { return ReadOnce(in_) == out_; }

 private:
  FifoCore(const FifoCore&) = delete;
  FifoCore& operator=(const FifoCore&) = delete;

  // Index accessors with acquire/release semantics: the release store publishes
  // data written just before it, the acquire load keeps data read just after it.
  // Non-AVR targets use std::atomic (portable, works on MSVC too); AVR has no
  // usable <atomic>, so use the GCC builtins directly (inline for 1-byte index).
  // std::atomic<U> is transparent for integral U of machine-word size, so
  // reinterpreting the plain index member is safe (std::atomic_ref semantics).
  // Note: we only use load/store acq/rel, which stay inline even under
  // -mdisable-hardware-atomics; is_always_lock_free would also judge the RMW
  // ops we never use, so it is intentionally not asserted here.
#if !defined(__AVR__)
  static_assert(sizeof(std::atomic<FifoIndex>) == sizeof(FifoIndex), "FifoCore: std::atomic<FifoIndex> must not add storage");
#endif
  template <typename U>
  static inline U ReadOnce(const U& x) noexcept {
#if defined(__AVR__)
    return __atomic_load_n(&x, __ATOMIC_ACQUIRE);
#else
    return reinterpret_cast<const std::atomic<U>&>(x).load(std::memory_order_acquire);
#endif
  }

  template <typename U>
  static inline void WriteOnce(U& x, U value) noexcept {
#if defined(__AVR__)
    __atomic_store_n(&x, value, __ATOMIC_RELEASE);
#else
    reinterpret_cast<std::atomic<U>&>(x).store(value, std::memory_order_release);
#endif
  }

  T* const buffer_ = nullptr;
  const FifoIndex max_size_ = 0;
  FifoIndex in_ = 0;
  FifoIndex out_ = 0;
};

template <typename T, size_t N>
class Fifo : public FifoCore<T> {
 public:
  static_assert(N >= 2 && (N & (N - 1)) == 0, "Fifo<T, N>: N must be a power of two and >= 2");
  static_assert(static_cast<unsigned long long>(N) <= static_cast<unsigned long long>(FifoIndex(-1)) / 2 + 1ULL,
                "Fifo<T, N>: N is too large for the index type (max 128 on AVR, 2^31 elsewhere)");
  constexpr Fifo() noexcept : FifoCore<T>(buffer_, N) {}

 private:
  T buffer_[N];
};
}  // namespace spsc
}  // namespace lockfree
}  // namespace cyf
#endif