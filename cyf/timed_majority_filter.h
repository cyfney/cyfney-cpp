#pragma once

#ifndef CYF_TIMED_MAJORITY_FILTER_H_
#define CYF_TIMED_MAJORITY_FILTER_H_

#include <stdint.h>

#include "fixed_buffer.h"

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
    for (size_t i = 0; i < entries_.size(); ++i) {
      if (entries_[i].frame == frame) {
        entries_[i].count++;
        return;
      }
    }

    if (entries_.PushBack({frame, 1}) && entries_.size() == 1) {
      last_time_ = getter_();
    }
  }

  // Writes the winning value to out; returns false when no decision was reached.
  bool Flush(FrameType& out) { return Flush(getter_(), out); }

  bool Flush(TimeType now, FrameType& out) {
    if (entries_.empty()) {
      return false;
    }

    if (Elapsed(now, last_time_) < timeout_) {
      return false;
    }

    uint16_t max_count = 0;
    uint16_t second_max = 0;
    bool has_best = false;
    FrameType best{};

    for (size_t i = 0; i < entries_.size(); ++i) {
      const uint16_t c = entries_[i].count;
      if (c > max_count) {
        second_max = max_count;
        max_count = c;
        best = entries_[i].frame;
        has_best = true;
      } else if (c > second_max) {
        second_max = c;
      }
    }

    entries_.Clear();

    if (has_best && (max_count >= min_count_) && (max_count > second_max)) {
      out = best;
      return true;
    }

    return false;
  }

  void Reset() { entries_.Clear(); }

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

  TimeType last_time_ = TimeType{0};
  FixedBuffer<Entry, MaxDistinct> entries_;
};
}  // namespace cyf

#endif  // CYF_TIMED_MAJORITY_FILTER_H_