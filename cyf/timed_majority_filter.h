#pragma once

#ifndef CYF_TIMED_MAJORITY_FILTER_H
#define CYF_TIMED_MAJORITY_FILTER_H

#include <stdint.h>

namespace cyf {
// Accumulates distinct FrameType values (counted by equality) and, once at
// least `timeout` time units have elapsed since the first sample of the
// current window, yields the unique most frequent value via Flush().
//
// Notes:
// - The window is anchored when the buffer transitions from empty to
//   non-empty; a Flush() before the timeout does not restart the timer.
// - Once the timeout has elapsed, the next Flush() decides over all
//   accumulated samples, including recently pushed ones.
// - When more than MaxDistinct distinct values coexist, new distinct values
//   are silently dropped.
// - FrameType must be default-constructible and provide operator==.
template <typename FrameType, typename TimeType, typename TimeGetter, uint8_t MaxDistinct = 8>
class TimedMajorityFilter {
 public:
  constexpr TimedMajorityFilter(TimeType timeout, TimeGetter getter, uint16_t min_count = 1)
      : timeout_(timeout), getter_(getter), min_count_(min_count) {}

  void Push(const FrameType& frame) {
    for (uint8_t i = 0; i < distinct_; ++i) {
      if (entries_[i].frame == frame) {
        entries_[i].count++;
        return;
      }
    }

    if (distinct_ < MaxDistinct) {
      entries_[distinct_].frame = frame;
      entries_[distinct_].count = 1;
      if (distinct_ == 0) {
        last_time_ = getter_();
      }
      distinct_++;
    }
  }

  bool Flush(FrameType* out) { return Flush(getter_(), out); }

  bool Flush(TimeType now, FrameType* out) {
    if (out == nullptr) {
      return false;
    }

    if (distinct_ == 0) {
      return false;
    }

    if (Elapsed(now, last_time_) < timeout_) {
      return false;
    }

    uint16_t max_count = 0;
    uint16_t second_max = 0;
    uint8_t best_idx = 0xFF;

    for (uint8_t i = 0; i < distinct_; ++i) {
      uint16_t c = entries_[i].count;
      if (c > max_count) {
        second_max = max_count;
        max_count = c;
        best_idx = i;
      } else if (c > second_max) {
        second_max = c;
      }
    }

    distinct_ = 0;

    if ((best_idx != 0xFF) && (max_count >= min_count_) && (max_count > second_max)) {
      *out = entries_[best_idx].frame;
      return true;
    }

    return false;
  }

  void Reset() { distinct_ = 0; }

 private:
  TimedMajorityFilter(const TimedMajorityFilter&) = delete;
  TimedMajorityFilter& operator=(const TimedMajorityFilter&) = delete;

  static TimeType Elapsed(TimeType now, TimeType last) { return now - last; }

  struct Entry {
    FrameType frame;
    uint16_t count;
  };

  const TimeType timeout_;
  const TimeGetter getter_;
  const uint16_t min_count_;

  uint8_t distinct_ = 0;
  TimeType last_time_ = TimeType{0};
  Entry entries_[MaxDistinct];
};
}  // namespace cyf

#endif  // CYF_TIMED_MAJORITY_FILTER_H