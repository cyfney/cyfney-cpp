#pragma once

#ifndef CYF_ROBUST_FRAME_H
#define CYF_ROBUST_FRAME_H

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#if defined(ARDUINO)
#include <Stream.h>
#endif

#include "buffer_view.h"
#include "byte_buffer.h"
#include "crc8.h"

namespace cyf {
namespace robust_frame {
namespace detail {
/**
 * 帧头
 */
constexpr uint8_t kFrameHeader = 0xAA;

/**
 * 帧尾
 */
constexpr uint8_t kFrameFooter = 0x55;

/**
 * 转义字符
 */
constexpr uint8_t kEscapeChar = 0xDB;

/**
 * 转义运算符
 */
constexpr uint8_t kXorEscape = 0x20;

inline void Escape(const uint8_t* data, size_t data_length, uint8_t* buffer, size_t* offset) {
  for (size_t i = 0; i < data_length; ++i) {
    if (data[i] == kFrameHeader || data[i] == kFrameFooter || data[i] == kEscapeChar) {
      buffer[(*offset)++] = kEscapeChar;
      buffer[(*offset)++] = static_cast<uint8_t>(data[i] ^ kXorEscape);
    } else {
      buffer[(*offset)++] = data[i];
    }
  }
}

#if defined(ARDUINO)
inline void Escape(const uint8_t* data, size_t data_length, Stream& stream) {
  for (size_t i = 0; i < data_length; ++i) {
    if (data[i] == kFrameHeader || data[i] == kFrameFooter || data[i] == kEscapeChar) {
      stream.write(kEscapeChar);
      stream.write(static_cast<uint8_t>(data[i] ^ kXorEscape));
    } else {
      stream.write(data[i]);
    }
  }
}
#endif

inline size_t MaxSerializedLength(size_t data_length) noexcept {
  return 1                    // header
         + (data_length + 1)  // payload + crc, 每个字节最多转义为 2 字节
               * 2 +
         1  // footer
      ;
}
}  // namespace detail

#if defined(ARDUINO)
inline void Serialize(const uint8_t* data, size_t length, Stream& stream) noexcept {
  using namespace detail;
  const uint8_t crc = crc8::Calculate(data, length);
  stream.write(kFrameHeader);
  Escape(data, length, stream);
  Escape(&crc, 1, stream);
  stream.write(kFrameFooter);
}
#endif

inline ByteBuffer Serialize(const uint8_t* data, size_t length) noexcept {
  using namespace detail;
  ByteBuffer buffer(MaxSerializedLength(length));
  size_t offset = 0;
  const uint8_t crc = crc8::Calculate(data, length);

  buffer[offset++] = kFrameHeader;
  Escape(data, length, buffer.data(), &offset);
  Escape(&crc, 1, buffer.data(), &offset);
  buffer[offset++] = kFrameFooter;
  buffer.Resize(offset);
  return buffer;
}

class Parser {
 public:
  explicit Parser(size_t max_payload_size) noexcept
      : max_payload_size_(max_payload_size),
        data_(new uint8_t[max_payload_size + 1]  // +1 for CRC
        ) {}

  ~Parser() noexcept { delete[] data_; }

  void Reset() noexcept {
    data_length_ = 0;
    state_ = State::kSyncHeader;
  }

  BufferView Process(uint8_t byte) noexcept {
    using namespace detail;
    switch (state_) {
      case State::kSyncHeader: {
        if (data_length_ != 0) {
          Reset();
        }
        if (byte == kFrameHeader) {
          state_ = State::kCollectData;
        }
        break;
      }
      case State::kCollectData: {
        if (byte == kFrameFooter) {
          constexpr size_t kMinUnescapedSize = 2;
          if (data_length_ >= kMinUnescapedSize) {
            const uint8_t received_crc = data_[data_length_ - 1];
            const uint8_t calculated_crc = crc8::Calculate(data_, data_length_ - 1);
            if (received_crc == calculated_crc) {
              --data_length_;
              state_ = State::kFrameParsed;
              return {data_, data_length_};
            }
          }
          Reset();
        } else if (byte == kFrameHeader || data_length_ == max_payload_size_ + 1) {
          Reset();
          Process(byte);
        } else if (byte == kEscapeChar) {
          state_ = State::kUnescape;
        } else {
          data_[data_length_++] = byte;
        }
        break;
      }
      case State::kUnescape: {
        const uint8_t unescaped_byte = static_cast<uint8_t>(byte ^ kXorEscape);
        if (unescaped_byte != kFrameHeader && unescaped_byte != kFrameFooter && unescaped_byte != kEscapeChar) {
          Reset();
          Process(byte);
        } else {
          data_[data_length_++] = unescaped_byte;
          state_ = State::kCollectData;
        }
        break;
      }
      case State::kFrameParsed: {
        Reset();
        Process(byte);
        break;
      }
      default: {
        break;
      }
    }
    return {nullptr, 0};
  }

 private:
  enum class State {
    kSyncHeader,
    kCollectData,
    kUnescape,
    kFrameParsed,
  };

  Parser(const Parser&) = delete;
  Parser& operator=(const Parser&) = delete;

  const size_t max_payload_size_ = 0;
  State state_ = State::kSyncHeader;
  uint8_t* const data_ = nullptr;
  size_t data_length_ = 0;
};

}  // namespace robust_frame
}  // namespace cyf
#endif
