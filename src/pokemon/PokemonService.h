#pragma once

#include <Pokemon/PokemonTracker.h>

#include "PokemonStore.h"

namespace pokemon {

class PokemonService {
 public:
  PokemonService(PokemonStore& store, RandomSource random)
      : store_(store), random_(random), tracker_(&PokemonService::creditFromTracker, this) {}

  bool beginReadingSession();
  void setBookProgressPercent(uint8_t percent);
  void onSuccessfulPageTurn(uint32_t nowMs);
  void checkpointIfDue(uint32_t nowMs);
  void flushOnExit(uint32_t nowMs);

  bool creditMinutes(uint16_t minutes, uint8_t bookProgressPercent);

 private:
  static bool creditFromTracker(void* context, uint16_t minutes, uint8_t bookProgressPercent);

  PokemonStore& store_;
  RandomSource random_{};
  PokemonTracker tracker_;
  bool readingSessionActive_ = false;
};

PokemonService& devicePokemonService();

}  // namespace pokemon
