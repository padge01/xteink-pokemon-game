#include "PokemonSpecies.h"

#include "PokemonSpecies.generated.h"

namespace pokemon {

const SpeciesData* speciesData(const uint16_t speciesId) {
  if (speciesId == 0 || speciesId > KANTO_SPECIES_COUNT) return nullptr;
  return &generated::KANTO_SPECIES[speciesId - 1U];
}

std::span<const EvolutionRule> evolutionsFor(const uint16_t speciesId) {
  const SpeciesData* species = speciesData(speciesId);
  if (species == nullptr || species->evolutionCount == 0) return {};
  return {generated::EVOLUTION_RULES + species->evolutionOffset, species->evolutionCount};
}

}  // namespace pokemon
