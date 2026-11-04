#pragma once

#ifndef CYF_FIXED_BUFFER_H_
#define CYF_FIXED_BUFFER_H_

#include <assert.h>
#include <stddef.h>

namespace cyf {
/**
 * 固定容量缓冲容器。
 *
 * 底层数组作为成员随对象分配（栈上或静态存储期），容量由模板参数在编译期
 * 确定，不涉及动态内存分配。有效长度由 size() 给出，取值不超过 capacity()。
 *
 * @tparam T        元素类型
 * @tparam Capacity 容量（元素个数），必须大于 0
 */
template <typename T, size_t Capacity>
class FixedBuffer {
  static_assert(Capacity > 0, "FixedBuffer requires Capacity > 0");

 public:
  typedef T value_type;
  typedef T* iterator;
  typedef const T* const_iterator;

  FixedBuffer() noexcept : size_(0) {}

  static constexpr size_t capacity() noexcept { return Capacity; }

  T* data() noexcept { return data_; }
  const T* data() const noexcept { return data_; }

  size_t size() const noexcept { return size_; }
  bool empty() const noexcept { return size_ == 0; }
  bool full() const noexcept { return size_ == Capacity; }

  iterator begin() noexcept { return data_; }
  iterator end() noexcept { return data_ + size_; }
  const_iterator begin() const noexcept { return data_; }
  const_iterator end() const noexcept { return data_ + size_; }

  T& operator[](size_t pos) noexcept { return data_[pos]; }
  const T& operator[](size_t pos) const noexcept { return data_[pos]; }

  T& at(size_t pos) noexcept {
    assert(pos < size_);
    return data_[pos];
  }

  const T& at(size_t pos) const noexcept {
    assert(pos < size_);
    return data_[pos];
  }

  /** 设置有效长度，超过容量时返回 false 且不改变状态。 */
  bool Resize(size_t size) noexcept {
    if (size > Capacity) {
      return false;
    }
    size_ = size;
    return true;
  }

  /** 追加单个元素，容量已满时返回 false。 */
  bool PushBack(const T& value) noexcept {
    if (size_ >= Capacity) {
      return false;
    }
    data_[size_++] = value;
    return true;
  }

  /** 追加一段数据，容量不足时整体失败（不写入任何元素）并返回 false。 */
  bool Append(const T* data, size_t count) noexcept {
    if ((data == nullptr) || (count > (Capacity - size_))) {
      return false;
    }
    for (size_t i = 0; i < count; ++i) {
      data_[size_ + i] = data[i];
    }
    size_ += count;
    return true;
  }

  void Clear() noexcept { size_ = 0; }

 private:
  T data_[Capacity];
  size_t size_;
};
}  // namespace cyf
#endif
