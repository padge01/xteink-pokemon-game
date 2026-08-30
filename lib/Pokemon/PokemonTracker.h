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
// page change until that page has actually rendered.
class PokemonTurnVerifier {
 public:
  void request(uint8_t bookProgressPercent);
  void renderSucceeded(uint32_t renderedAtMs);
  bool consume(VerifiedTurn& output);

 private:
  std::atomic<uint32_t> renderedAtMs_{0};
  std::atomic<uint8_t> requestedProgress_{0};
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

  PokemonTracker(CreditMinutesFn creditMinutes, void* context) : creditMinutes_(creditMinutes), context_(context) {}

  bool beginSession();
  void setBookProgressPercent(uint8_t percent);
  void onSuccessfulPageTurn(uint32_t nowMs);
  void checkpointIfDue(uint32_t nowMs);
  bool flushOnExit(uint32_t nowMs);

  uint32_t creditedSeconds() const { return creditedSeconds_; }

 private:
  void tick(uint32_t nowSeconds);
  bool commitWholeMinutes(uint16_t minimumMinutes);

  CreditMinutesFn creditMinutes_ = nullptr;
  void* context_ = nullptr;
  uint32_t lastTickSeconds_ = 0;
  uint32_t lastPageTurnSeconds_ = 0;
  uint32_t creditedSeconds_ = 0;
  uint32_t committedSeconds_ = 0;
  uint8_t bookProgressPercent_ = 0;
  bool started_ = false;
  bool sawPageTurn_ = false;
};

}  // namespace pokemon
