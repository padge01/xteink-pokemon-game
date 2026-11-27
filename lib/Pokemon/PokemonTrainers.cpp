#if defined(CROSSINK_ENABLE_POKEMON)

#include "PokemonTrainers.h"

#include <algorithm>
#include <span>

#include "PokemonBattle.h"
#include "PokemonSpecies.h"

namespace pokemon {
namespace {

// Each class's Pokemon, as in Pokemon Red/Blue's route trainers.
constexpr uint16_t YOUNGSTER[] = {19, 20, 21, 22, 23, 24, 27, 28, 52, 53};
constexpr uint16_t BUG_CATCHER[] = {10, 11, 12, 13, 14, 15, 46, 47, 48, 49, 123, 127};
constexpr uint16_t LASS[] = {16, 19, 29, 30, 35, 36, 39, 40, 43, 44, 69, 70};
constexpr uint16_t HIKER[] = {66, 67, 74, 75, 76, 95, 104, 105, 111, 112};
constexpr uint16_t SWIMMER[] = {60, 61, 72, 73, 86, 87, 90, 91, 116, 117, 118, 119, 120, 121};
constexpr uint16_t SUPER_NERD[] = {37, 58, 77, 78, 81, 82, 88, 89, 100, 101, 109, 110};
constexpr uint16_t BIRD_KEEPER[] = {16, 17, 18, 21, 22, 83, 84, 85};
constexpr uint16_t PSYCHIC[] = {63, 64, 79, 80, 96, 97, 122, 124};
constexpr uint16_t CHANNELER[] = {41, 42, 92, 93, 94, 104, 105};
constexpr uint16_t BLACKBELT[] = {56, 57, 66, 67, 68, 106, 107};
constexpr uint16_t FISHERMAN[] = {54, 55, 60, 61, 98, 99, 118, 119, 129, 130};
constexpr uint16_t BEAUTY[] = {35, 37, 38, 43, 44, 45, 52, 53, 69, 70, 71, 102, 103};

std::span<const uint16_t> poolFor(const uint16_t classId) {
  switch (static_cast<TrainerClass>(classId)) {
    case TrainerClass::Youngster:
      return YOUNGSTER;
    case TrainerClass::BugCatcher:
      return BUG_CATCHER;
    case TrainerClass::Lass:
      return LASS;
    case TrainerClass::Hiker:
      return HIKER;
    case TrainerClass::Swimmer:
      return SWIMMER;
    case TrainerClass::SuperNerd:
      return SUPER_NERD;
    case TrainerClass::BirdKeeper:
      return BIRD_KEEPER;
    case TrainerClass::Psychic:
      return PSYCHIC;
    case TrainerClass::Channeler:
      return CHANNELER;
    case TrainerClass::Blackbelt:
      return BLACKBELT;
    case TrainerClass::Fisherman:
      return FISHERMAN;
    case TrainerClass::Beauty:
      return BEAUTY;
  }
  return {};
}

// Level from which a species can be on a trainer's team: 1 for a species that
// evolves from nothing, otherwise the level it evolves at (30 for a stone or
// trade evolution, which have none).
uint8_t minimumTrainerLevel(const uint16_t speciesId) {
  for (uint16_t candidate = 1; candidate <= KANTO_SPECIES_COUNT; ++candidate) {
    for (const EvolutionRule& rule : evolutionsFor(candidate)) {
      if (rule.targetSpeciesId != speciesId) continue;
      return rule.trigger == EvolutionTrigger::Level && rule.minimumLevel > 0 ? rule.minimumLevel : 30;
    }
  }
  return 1;
}

uint32_t nextRandom(uint32_t& state) {
  state ^= state << 13;
  state ^= state >> 17;
  state ^= state << 5;
  return state;
}

}  // namespace

bool isValidTrainerClass(const uint16_t classId) { return classId >= 1 && classId <= TRAINER_CLASS_COUNT; }

uint8_t trainerLevelFor(const uint8_t bookProgressPercent, const uint8_t partyAverageLevel) {
  const int average = std::clamp<int>(partyAverageLevel, 1, 100);
  const int low = std::max(2, average - 4);
  const int high = std::clamp(average + 2, low, 100);
  const int progress = std::min<int>(bookProgressPercent, 100);
  return static_cast<uint8_t>(low + (high - low) * progress / 100);
}

bool buildTrainerTeam(const uint16_t classId, const uint8_t level, const uint32_t seed, TrainerTeam& team) {
  team = TrainerTeam{};
  const std::span<const uint16_t> pool = poolFor(classId);
  if (pool.empty() || level == 0 || level > 100) return false;
  uint32_t random = seed ^ 0x9E3779B9U;
  if (random == 0) random = 1;
  nextRandom(random);

  size_t size = 1;
  if (level >= 20) {
    size = 2 + nextRandom(random) % 2;
  } else if (level >= 10) {
    size = 1 + nextRandom(random) % 2;
  }
  team.count = size;
  for (size_t i = 0; i < size; ++i) {
    // The last member is the strongest, at the trainer's level; the others a
    // little below it.
    const size_t fromLast = size - 1 - i;
    const uint8_t memberLevel =
        static_cast<uint8_t>(std::max<int>(2, level - static_cast<int>(fromLast) * (1 + nextRandom(random) % 2)));
    std::array<uint16_t, 16> eligible{};
    size_t eligibleCount = 0;
    for (const uint16_t speciesId : pool) {
      if (eligibleCount < eligible.size() && memberLevel >= minimumTrainerLevel(speciesId)) {
        eligible[eligibleCount++] = speciesId;
      }
    }
    if (eligibleCount == 0) {
      team = TrainerTeam{};
      return false;
    }
    // Prefer a species not already on the team.
    uint16_t speciesId = eligible[nextRandom(random) % eligibleCount];
    for (size_t attempt = 0; attempt < 4; ++attempt) {
      const bool duplicate = std::any_of(team.members.begin(), team.members.begin() + i,
                                         [speciesId](const GymTeamMember& m) { return m.speciesId == speciesId; });
      if (!duplicate) break;
      speciesId = eligible[nextRandom(random) % eligibleCount];
    }
    GymTeamMember& member = team.members[i];
    member.speciesId = speciesId;
    member.level = memberLevel;
    std::array<uint8_t, BATTLE_MOVE_SLOTS> moves{};
    std::array<uint8_t, BATTLE_MOVE_SLOTS> pp{};
    defaultMovesetForLevel(speciesId, memberLevel, moves, pp);
    for (size_t slot = 0; slot < member.moves.size() && slot < moves.size(); ++slot) member.moves[slot] = moves[slot];
  }
  return true;
}

}  // namespace pokemon

#endif
