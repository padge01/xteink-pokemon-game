#include "PokemonTracker.h"

#include <algorithm>
#include <limits>

namespace pokemon {

void PokemonTracker::beginSession() {
  lastTickSeconds_ = 0;
  lastPageTurnSeconds_ = 0;
  creditedSeconds_ = 0;
  committedSeconds_ = 0;
  bookProgressPercent_ = 0;
  started_ = false;
  sawPageTurn_ = false;
}

void PokemonTracker::setBookProgressPercent(const uint8_t percent) {
  bookProgressPercent_ = std::min<uint8_t>(percent, 100);
}

void PokemonTracker::onSuccessfulPageTurn(const uint32_t nowMs) {
  const uint32_t nowSeconds = nowMs / 1000U;
  if (!started_) {
    started_ = true;
    lastTickSeconds_ = nowSeconds;
  }
  lastPageTurnSeconds_ = nowSeconds;
  sawPageTurn_ = true;
}

void PokemonTracker::tick(const uint32_t nowSeconds) {
  if (!started_) {
    started_ = true;
    lastTickSeconds_ = nowSeconds;
    return;
  }
  if (nowSeconds < lastTickSeconds_) {
    lastTickSeconds_ = nowSeconds;
    return;
  }

  uint32_t delta = nowSeconds - lastTickSeconds_;
  lastTickSeconds_ = nowSeconds;
  if (!sawPageTurn_ || nowSeconds - lastPageTurnSeconds_ > ACTIVE_WINDOW_SECONDS) return;

  delta = std::min<uint32_t>(delta, ACTIVE_WINDOW_SECONDS);
  creditedSeconds_ = creditedSeconds_ > UINT32_MAX - delta ? UINT32_MAX : creditedSeconds_ + delta;
}

bool PokemonTracker::commitWholeMinutes(const uint16_t minimumMinutes) {
  const uint32_t pendingSeconds = creditedSeconds_ - committedSeconds_;
  const uint32_t wholeMinutes = pendingSeconds / 60U;
  if (wholeMinutes < minimumMinutes || creditMinutes_ == nullptr) return false;

  const uint16_t minutes = static_cast<uint16_t>(
      std::min<uint32_t>(wholeMinutes, std::numeric_limits<uint16_t>::max()));
  if (!creditMinutes_(context_, minutes, bookProgressPercent_)) return false;
  committedSeconds_ += static_cast<uint32_t>(minutes) * 60U;
  return true;
}

void PokemonTracker::checkpointIfDue(const uint32_t nowMs) {
  tick(nowMs / 1000U);
  commitWholeMinutes(CHECKPOINT_MINUTES);
}

void PokemonTracker::flushOnExit(const uint32_t nowMs) {
  tick(nowMs / 1000U);
  commitWholeMinutes(1);
}

}  // namespace pokemon
