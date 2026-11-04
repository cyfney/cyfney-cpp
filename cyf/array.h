#pragma once

#ifndef CYF_ARRAY_H_
#define CYF_ARRAY_H_

#include <assert.h>
#include <stddef.h>

namespace cyf {
/**
 * 固定长度数组容器。
 *
 * 元素作为成员数组随对象本身分配（栈上或静态存储期），因此可以按值跨函数
 * 返回和传递。接口与 std::array 保持一致，便于在没有 <array> 的环境
 * （如 Arduino AVR）中替代使用。
 *
 * 保持聚合类型：无自定义构造函数，支持 Array<T, N> a = {1, 2, 3}; 的初始化。
 *
 * @tparam T 元素类型
 * @tparam N 元素个数，必须大于 0
 */
template <typename T, size_t N>
class Array {
  static_assert(N > 0, "Array requires N > 0");

 public:
  typedef T value_type;
  typedef T* iterator;
  typedef const T* const_iterator;
  typedef T& reference;
  typedef const T& const_reference;

  constexpr size_t size() const noexcept { return N; }

  T* data() noexcept { return data_; }
  const T* data() const noexcept { return data_; }

  iterator begin() noexcept { return data_; }
  iterator end() noexcept { return data_ + N; }
  const_iterator begin() const noexcept { return data_; }
  const_iterator end() const noexcept { return data_ + N; }

  T& operator[](size_t pos) noexcept { return data_[pos]; }
  const T& operator[](size_t pos) const noexcept { return data_[pos]; }

  T& at(size_t pos) noexcept {
    assert(pos < N);
    return data_[pos];
  }

  const T& at(size_t pos) const noexcept {
    assert(pos < N);
    return data_[pos];
  }

  T& front() noexcept { return data_[0]; }
  const T& front() const noexcept { return data_[0]; }
  T& back() noexcept { return data_[N - 1]; }
  const T& back() const noexcept { return data_[N - 1]; }

  void fill(const T& value) noexcept {
    for (size_t i = 0; i < N; ++i) {
      data_[i] = value;
    }
  }

  bool operator==(const Array& other) const noexcept {
    for (size_t i = 0; i < N; ++i) {
      if (!(data_[i] == other.data_[i])) {
        return false;
      }
    }
    return true;
  }

  bool operator!=(const Array& other) const noexcept { return !(*this == other); }

  T data_[N];
};
}  // namespace cyf
#endif
