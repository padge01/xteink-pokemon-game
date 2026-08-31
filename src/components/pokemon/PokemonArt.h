#pragma once

#include <PokemonTypes.h>

#include "GfxRenderer.h"
#include "components/themes/BaseTheme.h"

namespace pokemon {

bool drawPokemonSpeciesArt(const GfxRenderer& renderer, uint16_t speciesId, bool hero, Rect bounds,
                           bool drawFallback = true);
bool drawPokemonPokedexArt(const GfxRenderer& renderer, uint16_t speciesId, const char* speciesName, Rect bounds,
                           bool drawFallback = true);
bool drawPokemonItemArt(const GfxRenderer& renderer, EvolutionItem item, bool hero, Rect bounds,
                        bool drawFallback = true);

}  // namespace pokemon
