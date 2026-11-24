#pragma once

#include <cstdint>

namespace pokemon {

struct PokemonPromptContext {
  uint16_t speciesId = 1;
  uint32_t recordId = 0;

  static constexpr PokemonPromptContext forStarter(const uint16_t speciesId) { return {speciesId, 0}; }

  static constexpr PokemonPromptContext forCaught(const uint16_t speciesId, const uint32_t recordId) {
    return {speciesId, recordId};
  }

  constexpr bool isStarter() const { return recordId == 0; }
};

}  // namespace pokemon
