#include "lib/Epub/Epub/parsers/ImagePlacementPolicy.h"

using ImagePlacementPolicy::clampTopMarginToViewport;

static_assert(clampTopMarginToViewport(0, 24, 430, 430) == 0,
              "A full-height image must not retain a top margin");
static_assert(clampTopMarginToViewport(100, 20, 300, 430) == 20,
              "A fitting image must retain its requested top margin");
static_assert(clampTopMarginToViewport(100, 50, 300, 430) == 30,
              "An overflowing margin must shrink to the remaining viewport room");
static_assert(clampTopMarginToViewport(200, 20, 300, 430) == 0,
              "An image taller than the remaining viewport must not receive extra top margin");
static_assert(clampTopMarginToViewport(20, -10, 100, 430) == -10,
              "A fitting publisher negative margin must retain its layout meaning");

int main() { return 0; }
