#pragma once

#ifndef CYF_ROBUST_FRAME_H_
#define CYF_ROBUST_FRAME_H_

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#if defined(ARDUINO)
#include <Stream.h>
#endif

#include "buffer_view.h"
#include "crc8.h"
#include "fixed_buffer.h"

namespace cyf {
namespace robust_frame {
/**
 * 编码后的最大字节数（帧头 + 转义后的载荷与 CRC + 帧尾）。
 *
 * 每个字节最多转义为 2 字节，因此这是最坏情况下的长度上界。
 */
constexpr inline size_t MaxEncodedLength(size_t data_length) noexcept {
  return 1                    // header
         + (data_length + 1)  // payload + crc, 每个字节最多转义为 2 字节
               * 2 +
         1  // footer
      ;
}

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
}  // namespace detail

#if defined(ARDUINO)
inline void Encode(const uint8_t* data, size_t length, Stream& stream) noexcept {
  const uint8_t crc = crc8::Calculate(data, length);
  stream.write(detail::kFrameHeader);
  detail::Escape(data, length, stream);
  detail::Escape(&crc, 1, stream);
  stream.write(detail::kFrameFooter);
}
#endif

/**
 * 编码到调用者提供的缓冲区。
 *
 * @param data         载荷起始地址
 * @param length       载荷字节数
 * @param out_buffer   输出缓冲区，可用字节数不得小于 MaxEncodedLength(length)
 * @param out_capacity 输出缓冲区可用字节数
 * @return 实际写入的帧长度；data/out_buffer 为空或容量不足时返回 0
 */
inline size_t Encode(const uint8_t* data, size_t length, uint8_t* out_buffer, size_t out_capacity) noexcept {
  if ((data == nullptr) || (out_buffer == nullptr) || (out_capacity < MaxEncodedLength(length))) {
    return 0;
  }

  size_t offset = 0;
  const uint8_t crc = crc8::Calculate(data, length);

  out_buffer[offset++] = detail::kFrameHeader;
  detail::Escape(data, length, out_buffer, &offset);
  detail::Escape(&crc, 1, out_buffer, &offset);
  out_buffer[offset++] = detail::kFrameFooter;
  return offset;
}

/**
 * 编码到定长缓冲，缓冲随返回值分配在栈上，无动态内存。
 *
 * 返回值的 size() 即实际帧长度；data 为空指针或 length 超过 MaxPayloadSize
 * 时返回 size() == 0 的空缓冲。
 *
 * @tparam MaxPayloadSize 允许的最大载荷字节数
 */
template <size_t MaxPayloadSize>
inline FixedBuffer<uint8_t, MaxEncodedLength(MaxPayloadSize)> Encode(const uint8_t* data, size_t length) noexcept {
  static_assert(MaxPayloadSize > 0, "Encode requires MaxPayloadSize > 0");
  FixedBuffer<uint8_t, MaxEncodedLength(MaxPayloadSize)> buffer;
  if (length > MaxPayloadSize) {
    return buffer;
  }
  buffer.Resize(Encode(data, length, buffer.data(), MaxEncodedLength(MaxPayloadSize)));
  return buffer;
}

/**
 * robust_frame 帧解码器：按字节喂入串行字节流，还原转义、校验 CRC，
 * 收齐一帧时返回载荷视图。
 *
 * @tparam MaxPayloadSize 最大载荷字节数（缓冲区实际为 MaxPayloadSize + 1，
 *         额外 1 字节存放 CRC）。
 */
template <size_t MaxPayloadSize>
class Decoder {
  static_assert(MaxPayloadSize > 0, "Decoder requires MaxPayloadSize > 0");

 public:
  Decoder() noexcept = default;

  void Reset() noexcept {
    buffer_.Clear();
    state_ = State::kSyncHeader;
  }

  BufferView Decode(uint8_t byte) noexcept {
    switch (state_) {
      case State::kSyncHeader: {
        if (!buffer_.empty()) {
          Reset();
        }
        if (byte == detail::kFrameHeader) {
          state_ = State::kCollectData;
        }
        break;
      }
      case State::kCollectData: {
        if (byte == detail::kFrameFooter) {
          constexpr size_t kMinUnescapedSize = 2;
          if (buffer_.size() >= kMinUnescapedSize) {
            const uint8_t received_crc = buffer_[buffer_.size() - 1];
            const uint8_t calculated_crc = crc8::Calculate(buffer_.data(), buffer_.size() - 1);
            if (received_crc == calculated_crc) {
              buffer_.Resize(buffer_.size() - 1);
              state_ = State::kFrameParsed;
              return {buffer_.data(), buffer_.size()};
            }
          }
          Reset();
        } else if (byte == detail::kFrameHeader || buffer_.full()) {
          Reset();
          Decode(byte);
        } else if (byte == detail::kEscapeChar) {
          state_ = State::kUnescape;
        } else {
          buffer_.PushBack(byte);
        }
        break;
      }
      case State::kUnescape: {
        const uint8_t unescaped_byte = static_cast<uint8_t>(byte ^ detail::kXorEscape);
        if (unescaped_byte != detail::kFrameHeader && unescaped_byte != detail::kFrameFooter &&
            unescaped_byte != detail::kEscapeChar) {
          Reset();
          Decode(byte);
        } else if (!buffer_.PushBack(unescaped_byte)) {
          Reset();
          Decode(byte);
        } else {
          state_ = State::kCollectData;
        }
        break;
      }
      case State::kFrameParsed: {
        Reset();
        Decode(byte);
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

  static constexpr size_t kDataSize = MaxPayloadSize + 1;  // +1 for CRC

  Decoder(const Decoder&) = delete;
  Decoder& operator=(const Decoder&) = delete;

  State state_ = State::kSyncHeader;
  FixedBuffer<uint8_t, kDataSize> buffer_;
};

}  // namespace robust_frame
}  // namespace cyf
#endif
