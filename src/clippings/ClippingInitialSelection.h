#pragma once

#include <activities/reader/WordRef.h>

#include <cstddef>
#include <cstdint>

namespace ClippingInitialSelection {

struct WordWindow {
  size_t first = 0;
  size_t count = 0;

  constexpr size_t end() const { return first + count; }
  constexpr bool contains(const size_t index) const { return index >= first && index < end(); }
  constexpr bool operator==(const WordWindow& other) const { return first == other.first && count == other.count; }
};

constexpr WordWindow leadingWordWindow(const size_t totalWords, const size_t capacity) {
  return {0, totalWords < capacity ? totalWords : capacity};
}

constexpr WordWindow centeredWordWindow(const size_t totalWords, const size_t capacity) {
  const size_t retainedWords = totalWords < capacity ? totalWords : capacity;
  return {(totalWords - retainedWords) / 2, retainedWords};
}

constexpr WordWindow wordWindowForPage(const size_t pageIndex, const size_t totalWords, const size_t capacity) {
  return pageIndex == 0 ? centeredWordWindow(totalWords, capacity) : leadingWordWindow(totalWords, capacity);
}

constexpr size_t initialCursorIndex(const WordRef* words, const uint16_t* readingOrder, const size_t readingOrderSize) {
  if (!words || !readingOrder || readingOrderSize == 0) return 0;

  size_t pageEnd = 0;
  while (pageEnd < readingOrderSize && words[readingOrder[pageEnd]].pageIdx == 0) {
    ++pageEnd;
  }
  if (pageEnd == 0) return 0;

  size_t rowCount = 0;
  for (size_t rowStart = 0; rowStart < pageEnd;) {
    const int y = words[readingOrder[rowStart]].y;
    size_t rowEnd = rowStart + 1;
    while (rowEnd < pageEnd && words[readingOrder[rowEnd]].y == y) ++rowEnd;
    ++rowCount;
    rowStart = rowEnd;
  }

  const size_t targetRow = rowCount / 2;
  size_t rowIndex = 0;
  for (size_t rowStart = 0; rowStart < pageEnd;) {
    const int y = words[readingOrder[rowStart]].y;
    size_t rowEnd = rowStart + 1;
    while (rowEnd < pageEnd && words[readingOrder[rowEnd]].y == y) ++rowEnd;
    if (rowIndex == targetRow) return rowStart + (rowEnd - rowStart) / 2;
    ++rowIndex;
    rowStart = rowEnd;
  }

  return 0;
}

}  // namespace ClippingInitialSelection
