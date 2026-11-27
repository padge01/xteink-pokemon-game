#if defined(CROSSINK_ENABLE_POKEMON)

#include "PokemonBattleTypes.h"
#include "PokemonLearnsets.generated.h"

namespace pokemon {

std::span<const LearnsetEntry> learnsetFor(const uint16_t speciesId) {
  if (speciesId == 0 || speciesId > KANTO_SPECIES_COUNT) return {};
  const MoveListRef& ref = generated::SPECIES_LEARNSET[speciesId - 1U];
  return {generated::LEARN_ENTRIES + ref.offset, ref.count};
}

bool canLearnViaMachine(const uint16_t speciesId, const uint8_t moveId) {
  if (speciesId == 0 || speciesId > KANTO_SPECIES_COUNT || moveId == 0) return false;
  const MoveListRef& ref = generated::SPECIES_TMHM[speciesId - 1U];
  for (uint16_t index = 0; index < ref.count; ++index) {
    if (generated::TMHM_MOVE_IDS[ref.offset + index] == moveId) return true;
  }
  return false;
}

}  // namespace pokemon

#endif
