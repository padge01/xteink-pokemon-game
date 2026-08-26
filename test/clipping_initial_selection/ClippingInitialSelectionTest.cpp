#include <array>
#include <cstdint>

#include "src/clippings/ClippingInitialSelection.h"

using ClippingInitialSelection::WordWindow;
using ClippingInitialSelection::centeredWordWindow;
using ClippingInitialSelection::initialCursorIndex;
using ClippingInitialSelection::leadingWordWindow;
using ClippingInitialSelection::wordWindowForPage;

constexpr WordRef wordAt(const int y, const int pageIdx) {
  WordRef word{};
  word.y = y;
  word.pageIdx = pageIdx;
  return word;
}

static_assert(centeredWordWindow(500, 240) == WordWindow{130, 240});
static_assert(centeredWordWindow(240, 240) == WordWindow{0, 240});
static_assert(centeredWordWindow(80, 240) == WordWindow{0, 80});
static_assert(centeredWordWindow(80, 0) == WordWindow{40, 0});
static_assert(leadingWordWindow(500, 240) == WordWindow{0, 240});
static_assert(centeredWordWindow(500, 240).contains(130));
static_assert(centeredWordWindow(500, 240).contains(369));
static_assert(!centeredWordWindow(500, 240).contains(129));
static_assert(!centeredWordWindow(500, 240).contains(370));
static_assert(wordWindowForPage(0, 500, 240) == WordWindow{130, 240});
static_assert(wordWindowForPage(1, 500, 240) == WordWindow{0, 240});

constexpr std::array<WordRef, 9> WORDS = {
    wordAt(10, 0), wordAt(10, 0), wordAt(20, 0), wordAt(20, 0), wordAt(20, 0),
    wordAt(30, 0), wordAt(30, 0), wordAt(5, 1),  wordAt(5, 1),
};
constexpr std::array<uint16_t, 9> READING_ORDER = {0, 1, 2, 3, 4, 5, 6, 7, 8};

// Three rows on page zero: choose the middle word of the middle row. Words on
// the next preloaded page must not influence the initial cursor.
static_assert(initialCursorIndex(WORDS.data(), READING_ORDER.data(), READING_ORDER.size()) == 3);

constexpr std::array<WordRef, 8> EVEN_ROW_WORDS = {
    wordAt(10, 0), wordAt(20, 0), wordAt(30, 0), wordAt(30, 0),
    wordAt(40, 0), wordAt(5, 1),  wordAt(5, 1),  wordAt(5, 1),
};
constexpr std::array<uint16_t, 8> EVEN_ROW_ORDER = {0, 1, 2, 3, 4, 5, 6, 7};

// Match the upstream choice for an even row count: the first row in the lower
// half, then the middle word in that row.
static_assert(initialCursorIndex(EVEN_ROW_WORDS.data(), EVEN_ROW_ORDER.data(), EVEN_ROW_ORDER.size()) == 3);
static_assert(initialCursorIndex(nullptr, nullptr, 0) == 0);

int main() { return 0; }
