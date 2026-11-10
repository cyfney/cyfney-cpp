#pragma once

#ifndef CYF_ARRAY_H_
#define CYF_ARRAY_H_

#include <assert.h>
#include <stddef.h>

namespace cyf {
template <typename T, size_t N>
struct Array {
  static_assert(N > 0, "Array requires N > 0");

  using value_type = T;
  using iterator = T*;
  using const_iterator = const T*;
  using reference = T&;
  using const_reference = const T&;

  T data[N];

  constexpr size_t size() const noexcept { return N; }

  iterator begin() noexcept { return data; }
  iterator end() noexcept { return data + N; }
  const_iterator begin() const noexcept { return data; }
  const_iterator end() const noexcept { return data + N; }

  T& operator[](size_t pos) noexcept { return data[pos]; }
  const T& operator[](size_t pos) const noexcept { return data[pos]; }

  T& at(size_t pos) noexcept {
    assert(pos < N);
    return data[pos];
  }

  const T& at(size_t pos) const noexcept {
    assert(pos < N);
    return data[pos];
  }

  T& front() noexcept { return data[0]; }
  const T& front() const noexcept { return data[0]; }
  T& back() noexcept { return data[N - 1]; }
  const T& back() const noexcept { return data[N - 1]; }

  void Fill(const T& value) noexcept {
    for (size_t i = 0; i < N; ++i) {
      data[i] = value;
    }
  }
};
}  // namespace cyf
#endif
