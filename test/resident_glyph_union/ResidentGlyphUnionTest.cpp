#include <array>
#include <cstddef>
#include <cstdint>

#include "lib/EpdFont/ResidentGlyphUnion.h"

using ResidentGlyphUnion::MergeResult;
using ResidentGlyphUnion::mergeSorted;

namespace {

constexpr bool mergesIntervalsAndRequestsWithoutDuplicates() {
  constexpr std::array<EpdUnicodeInterval, 2> RESIDENT{{{0x41, 0x43, 0}, {0x48, 0x49, 3}}};
  constexpr std::array<uint32_t, 5> REQUESTED{{0x40, 0x42, 0x46, 0x49, 0x4A}};
  constexpr std::array<uint32_t, 8> EXPECTED{{0x40, 0x41, 0x42, 0x43, 0x46, 0x48, 0x49, 0x4A}};
  std::array<uint32_t, EXPECTED.size()> merged{};
  uint32_t mergedCount = 0;

  if (mergeSorted(RESIDENT.data(), RESIDENT.size(), REQUESTED.data(), REQUESTED.size(), merged.data(), merged.size(),
                  mergedCount) != MergeResult::Success) {
    return false;
  }
  if (mergedCount != EXPECTED.size()) return false;
  for (std::size_t i = 0; i < EXPECTED.size(); ++i) {
    if (merged[i] != EXPECTED[i]) return false;
  }
  return true;
}

constexpr bool reportsCapacityExceededBeforeDroppingTailGlyphs() {
  constexpr std::array<EpdUnicodeInterval, 1> RESIDENT{{{1, 4, 0}}};
  constexpr std::array<uint32_t, 2> REQUESTED{{5, 6}};
  std::array<uint32_t, 5> merged{};
  uint32_t mergedCount = 0;

  return mergeSorted(RESIDENT.data(), RESIDENT.size(), REQUESTED.data(), REQUESTED.size(), merged.data(), merged.size(),
                     mergedCount) == MergeResult::CapacityExceeded &&
         mergedCount == merged.size();
}

constexpr bool handlesAnEmptyResidentSet() {
  constexpr std::array<uint32_t, 3> REQUESTED{{7, 11, 13}};
  std::array<uint32_t, REQUESTED.size()> merged{};
  uint32_t mergedCount = 0;

  return mergeSorted(nullptr, 0, REQUESTED.data(), REQUESTED.size(), merged.data(), merged.size(), mergedCount) ==
             MergeResult::Success &&
         mergedCount == REQUESTED.size() && merged == REQUESTED;
}

static_assert(mergesIntervalsAndRequestsWithoutDuplicates());
static_assert(reportsCapacityExceededBeforeDroppingTailGlyphs());
static_assert(handlesAnEmptyResidentSet());

}  // namespace

int main() { return 0; }
