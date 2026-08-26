#include "src/clippings/ClippingHighlightGeometry.h"

using ClippingHighlightGeometry::GapRect;
using ClippingHighlightGeometry::WordRect;
using ClippingHighlightGeometry::gapBetweenAdjacentWords;

constexpr bool bridgesHiddenLayoutSpace() {
  constexpr WordRect wordBeforeEllipsis{12, 100, 200, 32, 20};
  constexpr WordRect ellipsis{13, 140, 200, 10, 20};
  GapRect gap{};
  return gapBetweenAdjacentWords(wordBeforeEllipsis, ellipsis, gap) && gap.x == 132 && gap.y == 200 &&
         gap.width == 8 && gap.height == 20;
}

constexpr bool bridgesRightToLeftWords() {
  constexpr WordRect wordBeforeEllipsis{20, 140, 200, 10, 20};
  constexpr WordRect ellipsis{21, 100, 200, 32, 20};
  GapRect gap{};
  return gapBetweenAdjacentWords(wordBeforeEllipsis, ellipsis, gap) && gap.x == 132 && gap.y == 200 &&
         gap.width == 8 && gap.height == 20;
}

constexpr bool rejectsUnsafeBridges() {
  constexpr WordRect first{12, 100, 200, 32, 20};
  constexpr WordRect last{UINT16_MAX, 100, 200, 32, 20};
  GapRect gap{};
  return !gapBetweenAdjacentWords(first, {13, 140, 220, 10, 20}, gap) &&
         !gapBetweenAdjacentWords(first, {14, 140, 200, 10, 20}, gap) &&
         !gapBetweenAdjacentWords(first, {13, 130, 200, 10, 20}, gap) &&
         !gapBetweenAdjacentWords(first, {13, 140, 200, 10, 0}, gap) &&
         !gapBetweenAdjacentWords(last, {0, 140, 200, 10, 20}, gap);
}

static_assert(bridgesHiddenLayoutSpace());
static_assert(bridgesRightToLeftWords());
static_assert(rejectsUnsafeBridges());

int main() { return 0; }
