#pragma once

#include <atomic>
#include <cstdint>

namespace pokemon {

using CreditMinutesFn = bool (*)(void* context, uint16_t minutes, uint8_t bookProgressPercent);

struct VerifiedTurn {
  uint32_t renderedAtMs = 0;
  uint8_t bookProgressPercent = 0;
};

// Bridges the reader's input task and render task without counting a requested
// page change until that page has actually rendered. Book progress is sampled
// from the rendered page, so a turn that crosses a chapter boundary reports the
// newly laid-out position rather than a transient pre-layout value.
class PokemonTurnVerifier {
 public:
  void request();
  // Returns true when this render confirmed a pending turn. Re-renders with no
  // pending turn (menus, bookmarks, overlays) are ignored.
  bool renderSucceeded(uint32_t renderedAtMs, uint8_t bookProgressPercent);
  bool consume(VerifiedTurn& output);
  void reset();

 private:
  std::atomic<uint32_t> renderedAtMs_{0};
  std::atomic<uint8_t> renderedProgress_{0};
  std::atomic<bool> awaitingRender_{false};
  std::atomic<bool> ready_{false};
};

// Credits elapsed time only while a successful manual page turn occurred in
// the trailing five minutes. This preserves Joshua Miller's companion timing
// semantics while keeping persistence behind a small callback boundary.
class PokemonTracker {
 public:
  static constexpr uint16_t ACTIVE_WINDOW_SECONDS = 300;
  static constexpr uint16_t CHECKPOINT_MINUTES = 5;
  // Delays between checkpoint retries after a failed save, so a failing SD
  // card is not rewritten on every reader loop tick. Exit flushes ignore it.
  static constexpr uint16_t RETRY_BACKOFF_SECONDS[] = {30, 60, 120, 300, 600};
  static constexpr uint8_t RETRY_BACKOFF_STEPS = sizeof(RETRY_BACKOFF_SECONDS) / sizeof(RETRY_BACKOFF_SECONDS[0]);

  PokemonTracker(CreditMinutesFn creditMinutes, void* context) : creditMinutes_(creditMinutes), context_(context) {}

  bool beginSession();
  void setBookProgressPercent(uint8_t percent);
  void onSuccessfulPageTurn(uint32_t nowMs);
  void checkpointIfDue(uint32_t nowMs);
  bool flushOnExit(uint32_t nowMs);

  uint32_t creditedSeconds() const { return creditedSeconds_; }
  uint8_t consecutiveCommitFailures() const { return commitFailures_; }

 private:
  void tick(uint32_t nowSeconds);
  bool commitWholeMinutes(uint16_t minimumMinutes);
  void recordCommitResult(bool committed, uint32_t nowSeconds);

  CreditMinutesFn creditMinutes_ = nullptr;
  void* context_ = nullptr;
  uint32_t lastTickSeconds_ = 0;
  uint32_t lastPageTurnSeconds_ = 0;
  uint32_t creditedSeconds_ = 0;
  uint32_t committedSeconds_ = 0;
  uint32_t retryAtSeconds_ = 0;
  uint8_t bookProgressPercent_ = 0;
  uint8_t commitFailures_ = 0;
  bool started_ = false;
  bool sawPageTurn_ = false;
};

}  // namespace pokemon
