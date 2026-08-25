#pragma once

#include <cstdint>

namespace section_cache {

inline constexpr uint32_t MAGIC = 0x535843FF;  // bytes: 0xFF, "CXS"
inline constexpr uint8_t FINALIZED_VERSION = 61;
inline constexpr uint8_t PARTIAL_VERSION = 0xF8;

constexpr bool isSupportedVersion(const uint8_t version) {
  return version == FINALIZED_VERSION || version == PARTIAL_VERSION;
}

constexpr bool isPartialVersion(const uint8_t version) { return version == PARTIAL_VERSION; }

}  // namespace section_cache
