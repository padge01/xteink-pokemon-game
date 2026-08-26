#include <cstddef>

#include "src/util/SleepWakePolicy.h"

using SleepWakePolicy::Resume;
using SleepWakePolicy::hasValidSavedFrame;
using SleepWakePolicy::resolveResume;
using SleepWakePolicy::shouldInitializeSeamlessly;

constexpr size_t EXPECTED_FRAME_BYTES = 52272U;

static_assert(!shouldInitializeSeamlessly(Resume::QuickResume, true, false),
              "UC8279 X3 wake without a valid frame must rebuild the display baseline");
static_assert(resolveResume(false, false, false, false) == Resume::Splash,
              "A stale Quick Resume flag on cold boot must not suppress normal initialization");
static_assert(resolveResume(false, false, true, false) == Resume::QuickResume,
              "A real sleep wake with the Quick Resume flag must restore the saved frame");
static_assert(resolveResume(true, false, true, false) == Resume::Network,
              "A network restart must take precedence over the persisted sleep flag");
static_assert(resolveResume(false, true, true, false) == Resume::Silent,
              "A silent restart must take precedence over the persisted sleep flag");
static_assert(shouldInitializeSeamlessly(Resume::QuickResume, true,
                                         hasValidSavedFrame(true, EXPECTED_FRAME_BYTES, EXPECTED_FRAME_BYTES)),
              "UC8279 X3 Quick Resume with a valid frame must remain seamless");
static_assert(!shouldInitializeSeamlessly(Resume::Splash, true, true), "Cold boot must initialize normally");
static_assert(shouldInitializeSeamlessly(Resume::Silent, true, false),
              "Silent restart must retain its existing seamless behavior");
static_assert(shouldInitializeSeamlessly(Resume::Network, true, false),
              "Network restart must retain its existing seamless behavior");
static_assert(shouldInitializeSeamlessly(Resume::QuickResume, false, false),
              "Other display profiles must retain their existing Quick Resume behavior");
static_assert(!hasValidSavedFrame(false, EXPECTED_FRAME_BYTES, EXPECTED_FRAME_BYTES),
              "A missing saved frame must be invalid");
static_assert(!hasValidSavedFrame(true, EXPECTED_FRAME_BYTES - 1U, EXPECTED_FRAME_BYTES),
              "A truncated saved frame must be invalid");
static_assert(!hasValidSavedFrame(true, EXPECTED_FRAME_BYTES + 1U, EXPECTED_FRAME_BYTES),
              "An oversized saved frame must be invalid");

int main() { return 0; }
