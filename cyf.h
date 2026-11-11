#pragma once

#include <stdint.h>

namespace cyf {
// 库版本号，与 git tag 保持一致
constexpr uint32_t kVersionMajor = 1;
constexpr uint32_t kVersionMinor = 4;
constexpr uint32_t kVersionPatch = 2;

// 数值形式，便于比较：0xMMmmpp
constexpr uint32_t kVersion = (kVersionMajor << 16) | (kVersionMinor << 8) | kVersionPatch;

// 字符串形式，便于打印
constexpr const char* kVersionString = "1.4.2";
}  // namespace cyf
