#pragma once

#if defined(CROSSINK_ENABLE_POKEMON)

#include "GfxRenderer.h"
#include "components/themes/BaseTheme.h"
#include "pokemon/PokemonService.h"

namespace pokemon {

bool pokemonHomeAccessorySupported(uint8_t theme);
void drawPokemonHomeAccessory(const GfxRenderer& renderer, const PokemonDashboardSnapshot& snapshot, Rect bounds);

}  // namespace pokemon

#endif
