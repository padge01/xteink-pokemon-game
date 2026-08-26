#include <cstdint>
#include <limits>

#include "src/activities/reader/ManualPageTurnGuard.h"

using ReaderPageTurnGuard::shouldDrop;

static_assert(shouldDrop(true, 1000U, 0U));
static_assert(shouldDrop(false, 1199U, 1000U));
static_assert(!shouldDrop(false, 1200U, 1000U));

constexpr uint32_t nearRollover = std::numeric_limits<uint32_t>::max() - 99U;
static_assert(shouldDrop(false, 99U, nearRollover));
static_assert(!shouldDrop(false, 100U, nearRollover));

int main() { return 0; }
