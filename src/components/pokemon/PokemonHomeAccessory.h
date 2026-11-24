#pragma once

#if defined(CROSSINK_ENABLE_POKEMON)

#include "GfxRenderer.h"
#include "components/themes/BaseTheme.h"
#include "pokemon/PokemonService.h"

namespace pokemon {

bool pokemonHomeAccessorySupported(uint8_t theme);
// Refreshes the Home snapshot, leaving it empty when the accessory is disabled
// or the current theme cannot place it (Cover Grid).
void loadPokemonHomeDashboard(PokemonDashboardSnapshot& output);
bool pokemonHomeAccessoryShown(const PokemonDashboardSnapshot& snapshot);
void drawPokemonHomeAccessory(const GfxRenderer& renderer, const PokemonDashboardSnapshot& snapshot, Rect bounds);

}  // namespace pokemon

#endif
