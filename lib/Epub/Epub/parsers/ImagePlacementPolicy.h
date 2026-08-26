#pragma once

namespace ImagePlacementPolicy {

constexpr int clampTopMarginToViewport(const int currentY, const int requestedMarginTop, const int imageHeight,
                                       const int viewportHeight) {
  if (currentY + requestedMarginTop + imageHeight <= viewportHeight) {
    return requestedMarginTop;
  }
  const int availableMargin = viewportHeight - imageHeight - currentY;
  return availableMargin > 0 ? availableMargin : 0;
}

}  // namespace ImagePlacementPolicy
