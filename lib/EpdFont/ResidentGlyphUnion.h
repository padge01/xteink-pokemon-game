#pragma once

#include <cstdint>

#include "EpdFontData.h"

namespace ResidentGlyphUnion {

enum class MergeResult : uint8_t { Success, CapacityExceeded };

// Merges the sorted, non-overlapping resident intervals with a sorted unique
// request. The caller owns the output buffer so this hot-path helper allocates
// no memory and can stop before an oversized union drops unseen tail glyphs.
constexpr MergeResult mergeSorted(const EpdUnicodeInterval* residentIntervals, const uint32_t residentIntervalCount,
                                  const uint32_t* requested, const uint32_t requestedCount, uint32_t* output,
                                  const uint32_t outputCapacity, uint32_t& outputCount) {
  outputCount = 0;
  uint32_t intervalIndex = 0;
  uint32_t residentCodepoint = residentIntervalCount > 0 ? residentIntervals[0].first : 0;
  bool residentActive = residentIntervalCount > 0;
  uint32_t requestIndex = 0;

  while (residentActive || requestIndex < requestedCount) {
    if (outputCount >= outputCapacity) return MergeResult::CapacityExceeded;

    uint32_t next = 0;
    if (residentActive && (requestIndex >= requestedCount || residentCodepoint <= requested[requestIndex])) {
      next = residentCodepoint;
      if (requestIndex < requestedCount && requested[requestIndex] == residentCodepoint) ++requestIndex;

      if (residentCodepoint < residentIntervals[intervalIndex].last) {
        ++residentCodepoint;
      } else if (++intervalIndex < residentIntervalCount) {
        residentCodepoint = residentIntervals[intervalIndex].first;
      } else {
        residentActive = false;
      }
    } else {
      next = requested[requestIndex++];
    }
    output[outputCount++] = next;
  }

  return MergeResult::Success;
}

}  // namespace ResidentGlyphUnion
