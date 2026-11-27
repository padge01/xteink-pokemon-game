#pragma once

// Trainers met while reading (the road trainers of the real games, as opposed
// to the fixed Gym Leader / Elite Four / Champion rosters in PokemonGymData).
// Ported from the xteink-pokemon-crosslingua fork.
//
// A trainer is queued as a PendingEventKind::Trainer event that holds only
// three numbers: its class (speciesId), its level (level) and a seed
// (recordId). Its team is rebuilt from those when the battle starts, so the
// save never stores a team. How often one shows up and what a win leaves
// behind live in PokemonGame.h (rollRoadTrainer(), grantTrainerReward()).

#include <array>
#include <cstddef>
#include <cstdint>

#include "PokemonBattleTypes.h"
#include "PokemonTypes.h"

namespace pokemon {

enum class TrainerClass : uint8_t {
  Youngster = 1,
  BugCatcher,
  Lass,
  Hiker,
  Swimmer,
  SuperNerd,
  BirdKeeper,
  Psychic,
  Channeler,
  Blackbelt,
  Fisherman,
  Beauty,
};
static_assert(static_cast<uint8_t>(TrainerClass::Beauty) == TRAINER_CLASS_COUNT);

constexpr size_t MAX_TRAINER_TEAM_SIZE = 3;
// XP for beating a road trainer, whatever its level (docs/development/pokemon-pace-plan.md).
constexpr uint32_t ROAD_TRAINER_VICTORY_XP = 10;

struct TrainerTeam {
  std::array<GymTeamMember, MAX_TRAINER_TEAM_SIZE> members{};
  size_t count = 0;
};

bool isValidTrainerClass(uint16_t classId);

// Level of a trainer met at `bookProgressPercent` through the current book,
// for a party whose average level is `partyAverageLevel`: it stays in a
// window around the party (4 levels below to 2 above), and the book's
// progress picks where in that window - the start of a book brings easier
// trainers, its end harder ones.
uint8_t trainerLevelFor(uint8_t bookProgressPercent, uint8_t partyAverageLevel);

// The trainer's team: 1 Pokemon below level 10, 1-2 below level 20, 2-3 from
// there, drawn from the class's species (an evolved species only once the
// member's level reaches the level it evolves at; 30 for stone or trade
// evolutions), the strongest last at `level`, with their level-up moves.
// Deterministic for the same arguments. Returns false for an invalid class
// or level.
bool buildTrainerTeam(uint16_t classId, uint8_t level, uint32_t seed, TrainerTeam& team);

}  // namespace pokemon
