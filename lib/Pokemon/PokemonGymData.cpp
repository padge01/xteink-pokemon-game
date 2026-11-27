#if defined(CROSSINK_ENABLE_POKEMON)

#include "PokemonBattleTypes.h"
#include "PokemonGyms.generated.h"

namespace pokemon {

// PokemonTypes.h pins its own copy of this bound (POKEMON_GYM_PROGRESS_BITS)
// since it cannot include this battle layer without inverting the
// dependency direction; keep the two in sync.
static_assert(POKEMON_GYM_PROGRESS_BITS == GYM_COUNT);

const GymData* gymData(const uint8_t gymIndex) {
  if (gymIndex == 0 || gymIndex > GYM_COUNT) return nullptr;
  return &generated::GYMS[gymIndex - 1U];
}

std::span<const GymTeamMember> gymTeamFor(const uint8_t gymIndex) {
  if (gymIndex == 0 || gymIndex > GYM_COUNT) return {};
  const GymData& gym = generated::GYMS[gymIndex - 1U];
  return {generated::GYM_TEAM_MEMBERS + gym.teamOffset, gym.teamCount};
}

GymProgress gymProgressFor(const uint16_t battleProgress, const uint8_t gymIndex) {
  if (gymIndex == 0 || gymIndex > GYM_COUNT) return GymProgress::Locked;
  const uint16_t bit = static_cast<uint16_t>(1U << (gymIndex - 1U));
  if ((battleProgress & bit) != 0) return GymProgress::Defeated;
  if (gymIndex <= 8U) {
    const uint16_t requiredMask = static_cast<uint16_t>(bit - 1U);  // every earlier gym bit
    return (battleProgress & requiredMask) == requiredMask ? GymProgress::Available : GymProgress::Locked;
  }
  constexpr uint16_t ALL_GYMS_MASK = 0x00FFU;  // bits 0-7: all 8 gyms
  if (gymIndex < CHAMPION_GYM_INDEX) {
    return (battleProgress & ALL_GYMS_MASK) == ALL_GYMS_MASK ? GymProgress::Available : GymProgress::Locked;
  }
  constexpr uint16_t ALL_ELITE_FOUR_MASK = 0x0F00U;  // bits 8-11: all 4 Elite Four members
  constexpr uint16_t CHAMPION_REQUIRED_MASK = static_cast<uint16_t>(ALL_GYMS_MASK | ALL_ELITE_FOUR_MASK);
  return (battleProgress & CHAMPION_REQUIRED_MASK) == CHAMPION_REQUIRED_MASK ? GymProgress::Available
                                                                             : GymProgress::Locked;
}

GymTeamMember championFinalSlotFor(const uint16_t starterSpeciesId) {
  // Bulbasaur/Ivysaur/Venusaur -> Charizard; Charmander/Charmeleon/Charizard
  // -> Blastoise; Squirtle/Wartortle/Blastoise -> Venusaur - matching the
  // real games' type-triangle rival response, independent of how far that
  // starter has evolved by now. Everything else (including this project's
  // Pikachu starter, which the original games never had a rival response
  // for) falls back to Charizard, the single most common default.
  if (starterSpeciesId >= 4U && starterSpeciesId <= 6U) {
    return GymTeamMember{9U, 65U, {56U, 59U, 44U, 110U}};  // Blastoise: Hydro Pump/Blizzard/Bite/Withdraw
  }
  if (starterSpeciesId >= 7U && starterSpeciesId <= 9U) {
    return GymTeamMember{3U, 65U, {74U, 72U, 75U, 76U}};  // Venusaur: Growth/Mega Drain/Razor Leaf/Solar Beam
  }
  return GymTeamMember{6U, 65U, {126U, 99U, 163U, 83U}};  // Charizard: Fire Blast/Rage/Slash/Fire Spin
}

}  // namespace pokemon

#endif
