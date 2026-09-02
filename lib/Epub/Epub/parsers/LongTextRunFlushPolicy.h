#pragma once

#include <cstddef>
#include <cstdint>

namespace LongTextRunFlushPolicy {

constexpr bool shouldFlush(const bool force, const bool inRuby, const size_t wordCount, const size_t wordLimit,
                           const uint16_t textRunBytes, const uint16_t byteLimit) {
  if (inRuby) return false;
  return force || wordCount > wordLimit || textRunBytes > byteLimit;
}

}  // namespace LongTextRunFlushPolicy
