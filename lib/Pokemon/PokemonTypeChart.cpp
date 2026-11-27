#if defined(CROSSINK_ENABLE_POKEMON)

#include <array>

#include "PokemonBattleTypes.h"

namespace pokemon {
namespace {

// Hand-authored: modern (post-Gen 6) type effectiveness chart, not sourced
// from a CSV/generator like the other Pokemon/* data because it never
// changes independently of this code. Row = attacking type, column =
// defending type. Values are percent multipliers (0/50/100/200 =
// immune/not very effective/neutral/super effective). Index 0 = Normal
// .. 17 = Fairy, i.e. static_cast<uint8_t>(PokemonType) - 1.
constexpr std::array<std::array<uint8_t, 18>, 18> TYPE_CHART = {{
    {{100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 50, 0, 100, 100, 50, 100}},   // Normal
    {{100, 50, 50, 100, 200, 200, 100, 100, 100, 100, 100, 200, 50, 100, 50, 100, 200, 100}},   // Fire
    {{100, 200, 50, 100, 50, 100, 100, 100, 200, 100, 100, 100, 200, 100, 50, 100, 100, 100}},  // Water
    {{100, 100, 200, 50, 50, 100, 100, 100, 0, 200, 100, 100, 100, 100, 50, 100, 100, 100}},    // Electric
    {{100, 50, 200, 100, 50, 100, 100, 50, 200, 50, 100, 50, 200, 100, 50, 100, 50, 100}},      // Grass
    {{100, 50, 50, 100, 200, 50, 100, 100, 200, 200, 100, 100, 100, 100, 200, 100, 50, 100}},   // Ice
    {{200, 100, 100, 100, 100, 200, 100, 50, 100, 50, 50, 50, 200, 0, 100, 200, 200, 50}},      // Fighting
    {{100, 100, 100, 100, 200, 100, 100, 50, 50, 100, 100, 100, 50, 50, 100, 100, 0, 200}},     // Poison
    {{100, 200, 100, 200, 50, 100, 100, 200, 100, 0, 100, 50, 200, 100, 100, 100, 200, 100}},   // Ground
    {{100, 100, 100, 50, 200, 100, 200, 100, 100, 100, 100, 200, 50, 100, 100, 100, 50, 100}},  // Flying
    {{100, 100, 100, 100, 100, 100, 200, 200, 100, 100, 50, 100, 100, 100, 100, 0, 50, 100}},   // Psychic
    {{100, 50, 100, 100, 200, 100, 50, 50, 100, 50, 200, 100, 100, 50, 100, 200, 50, 50}},      // Bug
    {{100, 200, 100, 100, 100, 200, 50, 100, 50, 200, 100, 200, 100, 100, 100, 100, 50, 100}},  // Rock
    {{0, 100, 100, 100, 100, 100, 100, 100, 100, 100, 200, 100, 100, 200, 100, 50, 100, 100}},  // Ghost
    {{100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 200, 100, 50, 0}},  // Dragon
    {{100, 100, 100, 100, 100, 100, 50, 100, 100, 100, 200, 100, 100, 200, 100, 50, 100, 50}},  // Dark
    {{100, 50, 50, 50, 100, 200, 100, 100, 100, 100, 100, 100, 200, 100, 100, 100, 50, 200}},   // Steel
    {{100, 50, 100, 100, 100, 100, 200, 50, 100, 100, 100, 100, 100, 100, 200, 200, 50, 100}},  // Fairy
}};

uint16_t singleTypeEffectivenessPercent(const PokemonType attackerType, const PokemonType defenderType) {
  if (defenderType == PokemonType::None) return 100;
  const size_t attackerIndex = static_cast<size_t>(attackerType) - 1U;
  const size_t defenderIndex = static_cast<size_t>(defenderType) - 1U;
  if (attackerIndex >= TYPE_CHART.size() || defenderIndex >= TYPE_CHART.front().size()) return 100;
  return TYPE_CHART[attackerIndex][defenderIndex];
}

}  // namespace

uint16_t typeEffectivenessPercent(const PokemonType attackerType, const PokemonType defenderPrimary,
                                  const PokemonType defenderSecondary) {
  const uint16_t primaryPercent = singleTypeEffectivenessPercent(attackerType, defenderPrimary);
  if (defenderSecondary == PokemonType::None || defenderSecondary == defenderPrimary) return primaryPercent;
  const uint16_t secondaryPercent = singleTypeEffectivenessPercent(attackerType, defenderSecondary);
  return static_cast<uint16_t>(primaryPercent * secondaryPercent / 100U);
}

}  // namespace pokemon

#endif
