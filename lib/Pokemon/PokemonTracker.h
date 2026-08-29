#pragma once

#include <cstdint>

namespace pokemon {

using CreditMinutesFn = bool (*)(void* context, uint16_t minutes, uint8_t bookProgressPercent);

// Credits elapsed time only while a successful manual page turn occurred in
// the trailing five minutes. This preserves Joshua Miller's companion timing
// semantics while keeping persistence behind a small callback boundary.
class PokemonTracker {
 public:
  static constexpr uint16_t ACTIVE_WINDOW_SECONDS = 300;
  static constexpr uint16_t CHECKPOINT_MINUTES = 5;

  PokemonTracker(CreditMinutesFn creditMinutes, void* context) : creditMinutes_(creditMinutes), context_(context) {}

  void beginSession();
  void setBookProgressPercent(uint8_t percent);
  void onSuccessfulPageTurn(uint32_t nowMs);
  void checkpointIfDue(uint32_t nowMs);
  void flushOnExit(uint32_t nowMs);

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
