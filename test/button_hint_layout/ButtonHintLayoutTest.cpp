#include "components/themes/ButtonHintLayout.h"
#include "EpdFont/builtinFonts/inter_10_regular.h"
#include "EpdFont/builtinFonts/inter_8_regular.h"

using ButtonHintLayout::Alignment;

static_assert(ButtonHintLayout::maxTextWidth(106, 4) == 98);
static_assert(ButtonHintLayout::overflowMaxTextWidth(106, 4, Alignment::Center) == 97);
static_assert(ButtonHintLayout::overflowMaxTextWidth(80, 4, Alignment::Center) == 71);

// Preserve Base's existing centered placement: x + (width - 1 - textWidth) / 2.
static_assert(ButtonHintLayout::textX(38, 106, 31, Alignment::Center, 4) == 75);

// Preserve RoundedRaff's 16-pixel inset from the aligned edge.
static_assert(ButtonHintLayout::textX(20, 107, 30, Alignment::Left, 16) == 36);
static_assert(ButtonHintLayout::textX(127, 108, 30, Alignment::Right, 16) == 189);
static_assert(ButtonHintLayout::overflowMaxTextWidth(107, 16, Alignment::Left) == 75);
static_assert(ButtonHintLayout::overflowMaxTextWidth(108, 16, Alignment::Right) == 76);

constexpr int pixelHeight(const EpdFontData& font) { return font.ascender - font.descender; }

// The actual built-in font bounds cannot safely fit two rows in any current
// front-button guide, so overflow must use a single ellipsized line.
static_assert(ButtonHintLayout::maxVisibleLines(40, pixelHeight(inter_10_regular), 2) == 1);
static_assert(ButtonHintLayout::maxVisibleLines(40, pixelHeight(inter_8_regular), 2) == 1);
static_assert(ButtonHintLayout::maxVisibleLines(30, pixelHeight(inter_8_regular), 2) == 1);

// A future guide tall enough for two complete SMALL rows, the gap, and one
// pixel of vertical padding on each edge may wrap.
static_assert(ButtonHintLayout::maxVisibleLines(48, pixelHeight(inter_8_regular), 2) == 2);

int main() { return 0; }
