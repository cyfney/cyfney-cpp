#pragma once

#ifndef CYF_HYSTERESIS_QUANTIZER_H_
#define CYF_HYSTERESIS_QUANTIZER_H_

#include <assert.h>
#include <stdint.h>

namespace cyf {

template <typename T>
class HysteresisQuantizer {
 public:
  HysteresisQuantizer(T min_value, T max_value, uint32_t levels, T divisor) noexcept
      : min_value_(min_value), max_value_(max_value), levels_(levels) {
    assert(max_value > min_value);
    assert(levels > 0);
    assert(divisor > 0);

    T step_size = (max_value - min_value) / static_cast<T>(levels);
    hysteresis_ = step_size / divisor;

    if (hysteresis_ == static_cast<T>(0)) {
      hysteresis_ = static_cast<T>(1);
    }
  }

  uint32_t Update(T value) noexcept {
    if (value < min_value_) {
      value = min_value_;
    }

    if (value > max_value_) {
      value = max_value_;
    }

    const T range = max_value_ - min_value_;
    uint32_t raw_level = static_cast<uint32_t>((value - min_value_) * levels_ / range);
    if (raw_level >= levels_) {
      raw_level = levels_ - 1;
    }

    if (raw_level > level_) {
      const T threshold = min_value_ + (range * level_ / levels_) + hysteresis_;
      if (value >= threshold) {
        level_ = raw_level;
      }
    } else if (raw_level < level_) {
      const T threshold = min_value_ + (range * (level_ - 1) / levels_) - hysteresis_;
      if (value <= threshold) {
        level_ = raw_level;
      }
    }

    return level_;
  }

  void Reset() noexcept { level_ = 0; }
  uint32_t level() const noexcept { return level_; }
  T value() const noexcept { return min_value_ + (max_value_ - min_value_) * level_ / levels_; }

 private:
  const T min_value_;
  const T max_value_;
  const uint32_t levels_;
  T hysteresis_;
  uint32_t level_ = 0;
};
}  // namespace cyf
#endif