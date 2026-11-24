#if defined(CROSSINK_ENABLE_POKEMON)

#include "PokemonTracker.h"

#include <algorithm>
#include <limits>

namespace pokemon {

void PokemonTurnVerifier::request() { awaitingRender_.store(true, std::memory_order_release); }

bool PokemonTurnVerifier::renderSucceeded(const uint32_t renderedAtMs, const uint8_t bookProgressPercent) {
  if (!awaitingRender_.exchange(false, std::memory_order_acq_rel)) return false;
  renderedProgress_.store(std::min<uint8_t>(bookProgressPercent, 100), std::memory_order_relaxed);
  renderedAtMs_.store(renderedAtMs, std::memory_order_relaxed);
  ready_.store(true, std::memory_order_release);
  return true;
}

void PokemonTurnVerifier::reset() {
  awaitingRender_.store(false, std::memory_order_relaxed);
  ready_.store(false, std::memory_order_release);
}

bool PokemonTurnVerifier::consume(VerifiedTurn& output) {
  if (!ready_.exchange(false, std::memory_order_acq_rel)) return false;
  output.renderedAtMs = renderedAtMs_.load(std::memory_order_relaxed);
  output.bookProgressPercent = renderedProgress_.load(std::memory_order_relaxed);
  return true;
}

bool PokemonTracker::beginSession() {
  const uint32_t retrySeconds = ((creditedSeconds_ - committedSeconds_) / 60U) * 60U;
  const uint8_t retryProgressPercent = bookProgressPercent_;
  lastTickSeconds_ = 0;
  lastPageTurnSeconds_ = 0;
  creditedSeconds_ = retrySeconds;
  committedSeconds_ = 0;
  bookProgressPercent_ = retrySeconds == 0 ? 0 : retryProgressPercent;
  started_ = false;
  sawPageTurn_ = false;
  commitFailures_ = 0;
  retryAtSeconds_ = 0;
  if (retrySeconds != 0 && !commitWholeMinutes(1)) return false;
  bookProgressPercent_ = 0;
  return true;
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
  if (wholeMinutes < minimumMinutes) return true;
  if (creditMinutes_ == nullptr) return false;

  const uint16_t minutes =
      static_cast<uint16_t>(std::min<uint32_t>(wholeMinutes, std::numeric_limits<uint16_t>::max()));
  if (!creditMinutes_(context_, minutes, bookProgressPercent_)) return false;
  committedSeconds_ += static_cast<uint32_t>(minutes) * 60U;
  return true;
}

void PokemonTracker::recordCommitResult(const bool committed, const uint32_t nowSeconds) {
  if (committed) {
    commitFailures_ = 0;
    retryAtSeconds_ = 0;
    return;
  }
  if (commitFailures_ < RETRY_BACKOFF_STEPS) ++commitFailures_;
  retryAtSeconds_ = nowSeconds + RETRY_BACKOFF_SECONDS[commitFailures_ - 1U];
}

void PokemonTracker::checkpointIfDue(const uint32_t nowMs) {
  const uint32_t nowSeconds = nowMs / 1000U;
  tick(nowSeconds);
  // Wrap-safe comparison: millis() overflows after ~49 days of uptime.
  if (commitFailures_ != 0 && static_cast<int32_t>(nowSeconds - retryAtSeconds_) < 0) return;
  recordCommitResult(commitWholeMinutes(CHECKPOINT_MINUTES), nowSeconds);
}

bool PokemonTracker::flushOnExit(const uint32_t nowMs) {
  const uint32_t nowSeconds = nowMs / 1000U;
  tick(nowSeconds);
  const bool committed = commitWholeMinutes(1);
  recordCommitResult(committed, nowSeconds);
  return committed;
}

}  // namespace pokemon

#endif
