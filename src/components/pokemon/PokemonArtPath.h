#pragma once

#include <PokemonTypes.h>

#include <cstddef>
#include <cstdint>

namespace pokemon {

const char* pokemonSpeciesArtPath(uint16_t speciesId, bool hero, char* output, size_t outputSize);
const char* pokemonItemArtPath(EvolutionItem item, bool hero, char* output, size_t outputSize);

}  // namespace pokemon
