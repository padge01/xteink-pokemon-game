#pragma once

#include <cstddef>
#include <cstdint>

namespace SleepWakePolicy {

enum class Resume : uint8_t {
  Splash,
  Silent,
  Network,
  QuickResume,
};

constexpr Resume resolveResume(const bool networkResume, const bool silentReboot, const bool sleepWake,
                               const bool showBootScreen) {
  if (networkResume) {
    return Resume::Network;
  }
  if (silentReboot) {
    return Resume::Silent;
  }
  if (sleepWake && !showBootScreen) {
    return Resume::QuickResume;
  }
  return Resume::Splash;
}

constexpr bool hasValidSavedFrame(const bool exists, const size_t actualSize, const size_t expectedSize) {
  return exists && actualSize == expectedSize;
}

// A UC8279 X3 can skip its initial resync only after a saved Quick Resume
// frame has been verified. Other resume paths retain their existing behavior.
constexpr bool shouldInitializeSeamlessly(const Resume resume, const bool isUc8279X3, const bool hasValidFrame) {
  return resume != Resume::Splash && !(resume == Resume::QuickResume && isUc8279X3 && !hasValidFrame);
}

}  // namespace SleepWakePolicy
