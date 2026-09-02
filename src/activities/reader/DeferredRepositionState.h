#pragma once

#include <cstdint>
#include <limits>
#include <optional>

struct DeferredRepositionState {
  int spineIndex = 0;
  int chapterPageNumber = 0;
  int chapterTotalPageCount = 0;
  int chapterPageWatermark = 0;
  std::optional<uint32_t> visibleTextOffset;
  bool pending = false;
  uint16_t paragraphIndex = std::numeric_limits<uint16_t>::max();
  uint16_t paragraphOffset = 0;
  uint16_t paragraphSpan = 0;

  constexpr void clear() {
    spineIndex = 0;
    chapterPageNumber = 0;
    chapterTotalPageCount = 0;
    chapterPageWatermark = 0;
    visibleTextOffset.reset();
    pending = false;
    paragraphIndex = std::numeric_limits<uint16_t>::max();
    paragraphOffset = 0;
    paragraphSpan = 0;
  }
};
