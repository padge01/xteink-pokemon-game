#pragma once

#include <cstdint>

namespace ButtonHintLayout {

enum class Alignment : uint8_t { Left, Center, Right };

constexpr int maxTextWidth(const int width, const int horizontalPadding) {
  const int available = width - horizontalPadding * 2;
  return available > 0 ? available : 0;
}

constexpr int overflowMaxTextWidth(const int width, const int horizontalPadding, const Alignment alignment) {
  const int available = maxTextWidth(width, horizontalPadding);
  // Legacy centered placement uses (width - 1 - textWidth) / 2. Tighten only
  // generated overflow lines by one pixel so both requested insets remain.
  return alignment == Alignment::Center && available > 0 ? available - 1 : available;
}

constexpr int textX(const int x, const int width, const int textWidth, const Alignment alignment,
                    const int horizontalPadding) {
  switch (alignment) {
    case Alignment::Left:
      return x + horizontalPadding;
    case Alignment::Right:
      return x + width - horizontalPadding - textWidth;
    case Alignment::Center:
      return x + (width - 1 - textWidth) / 2;
  }
  return x;
}

constexpr int maxVisibleLines(const int height, const int pixelHeight, const int requestedLines, const int lineGap = 2,
                              const int verticalPadding = 1) {
  if (requestedLines <= 0) return 0;
  if (height <= 0 || pixelHeight <= 0) return 1;

  const int availableHeight = height - verticalPadding * 2;
  if (availableHeight <= pixelHeight) return 1;
  const int fittingLines = (availableHeight + lineGap) / (pixelHeight + lineGap);
  return fittingLines < requestedLines ? fittingLines : requestedLines;
}

constexpr int wrappedTextTop(const int top, const int height, const int pixelHeight, const int lineCount,
                             const int lineGap = 2, const int verticalPadding = 1) {
  if (lineCount <= 0) return top;
  const int blockHeight = lineCount * pixelHeight + (lineCount - 1) * lineGap;
  const int availableHeight = height - verticalPadding * 2;
  const int centeredInset = availableHeight > blockHeight ? (availableHeight - blockHeight) / 2 : 0;
  return top + verticalPadding + centeredInset;
}

}  // namespace ButtonHintLayout
