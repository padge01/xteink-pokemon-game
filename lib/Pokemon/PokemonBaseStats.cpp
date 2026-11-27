#if defined(CROSSINK_ENABLE_POKEMON)

#include "PokemonBattleTypes.h"
#include "PokemonStats.generated.h"

namespace pokemon {

const BaseStats* baseStatsFor(const uint16_t speciesId) {
  if (speciesId == 0 || speciesId > KANTO_SPECIES_COUNT) return nullptr;
  return &generated::KANTO_BASE_STATS[speciesId - 1U];
}

}  // namespace pokemon

#endif
