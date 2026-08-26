#include "lib/Epub/Epub/parsers/LongTextRunFlushPolicy.h"

using LongTextRunFlushPolicy::shouldFlush;

static_assert(!shouldFlush(/*force=*/false, /*inRuby=*/false, 100, 100, 200, 200));
static_assert(shouldFlush(/*force=*/false, /*inRuby=*/false, 101, 100, 200, 200));
static_assert(shouldFlush(/*force=*/false, /*inRuby=*/false, 100, 100, 201, 200));
static_assert(shouldFlush(/*force=*/true, /*inRuby=*/false, 0, 100, 0, 200));

static_assert(!shouldFlush(/*force=*/false, /*inRuby=*/true, 101, 100, 201, 200));
static_assert(!shouldFlush(/*force=*/true, /*inRuby=*/true, 101, 100, 201, 200));

int main() { return 0; }
