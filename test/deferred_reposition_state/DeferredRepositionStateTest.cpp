#include <cstdint>
#include <limits>

#include "src/activities/reader/DeferredRepositionState.h"

constexpr bool clearsEveryCachedAnchor() {
  DeferredRepositionState state;
  state.spineIndex = 8;
  state.chapterPageNumber = 13;
  state.chapterTotalPageCount = 34;
  state.chapterPageWatermark = 21;
  state.visibleTextOffset = 9876U;
  state.pending = true;
  state.paragraphIndex = 55;
  state.paragraphOffset = 2;
  state.paragraphSpan = 4;

  state.clear();

  return state.spineIndex == 0 && state.chapterPageNumber == 0 && state.chapterTotalPageCount == 0 &&
         state.chapterPageWatermark == 0 && !state.visibleTextOffset.has_value() && !state.pending &&
         state.paragraphIndex == std::numeric_limits<uint16_t>::max() && state.paragraphOffset == 0 &&
         state.paragraphSpan == 0;
}

static_assert(clearsEveryCachedAnchor());

int main() { return 0; }
