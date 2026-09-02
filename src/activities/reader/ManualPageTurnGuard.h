#pragma once

#include <cstdint>

namespace ReaderPageTurnGuard {

inline constexpr uint32_t MIN_MANUAL_TURN_GAP_MS = 200U;

constexpr bool shouldDrop(const bool renderBusy, const uint32_t nowMs, const uint32_t lastTurnMs) {
  return renderBusy || (nowMs - lastTurnMs) < MIN_MANUAL_TURN_GAP_MS;
}

}  // namespace ReaderPageTurnGuard
