#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "PokemonSpecies.h"

namespace pokemon {

constexpr uint8_t MOVE_COUNT = 165;
// 83 original items + PP Up (id 84) + 6 battle-boost items (ids 85-90).
constexpr uint8_t ITEM_COUNT = 95;
constexpr uint8_t GYM_COUNT = 13;  // 8 gyms + 4 Elite Four + the Champion (Blue), in challenge order
// The Champion is always the last entry - see gymProgressFor()'s Champion
// branch and PokemonActivity's dynamic final-slot substitution.
constexpr uint8_t CHAMPION_GYM_INDEX = GYM_COUNT;
constexpr uint8_t MAX_GYM_TEAM_SIZE = 6;  // full real Pokemon Red teams: Giovanni/Lorelei/Bruno/Agatha/Lance
                                          // have 5, the Champion (Blue) has 6
// Mirrors PokemonBattle.h's BATTLE_MOVE_SLOTS (this header can't include
// that one - PokemonBattle.h includes this header, not the reverse). Kept
// in sync via the static_assert alongside BATTLE_MOVE_SLOTS's definition.
constexpr uint8_t GYM_MOVE_SLOTS = 4;

enum class MoveCategory : uint8_t {
  Physical = 0,
  Special = 1,
  Status = 2,
};

enum class Ailment : uint8_t {
  None = 0,
  Paralysis = 1,
  Sleep = 2,
  Freeze = 3,
  Burn = 4,
  Poison = 5,
  Confusion = 6,
  All = 7,  // Full Heal / Full Restore: cures whichever ailment is active
};

struct MoveData {
  uint8_t moveId;
  const char* name;
  PokemonType type;
  uint8_t power;
  uint8_t accuracy;
  uint8_t pp;
  MoveCategory category;
  Ailment ailment;
  uint8_t ailmentChance;
};

// Generation I base stats. Special stands in for the single Gen I "Special"
// stat (PokeAPI's special-attack). All Gen I base stats fit uint8_t (max 255).
// Shared stat order for BaseStats and every IV/EV array in this project: HP,
// Attack, Defense, Special, Speed.
constexpr size_t STAT_COUNT = 5;

struct BaseStats {
  uint8_t hp;
  uint8_t attack;
  uint8_t defense;
  uint8_t special;
  uint8_t speed;
  // EV yield per stat (0-3 in practice) - how much of that stat's Effort
  // Value a Pokemon of this species grants when defeated. Not a real Gen 1
  // mechanic (Gen 1 accumulated "Stat Experience" directly from a defeated
  // Pokemon's base stats, no per-species yield table existed yet) - this
  // project's own simplified EV model deliberately borrows the modern
  // games' small per-species yield table instead of Gen 1's own formula, the
  // same way it already rejected Gen 1's real battle-XP formula as
  // oversized for this game's pace.
  uint8_t evHp;
  uint8_t evAttack;
  uint8_t evDefense;
  uint8_t evSpecial;
  uint8_t evSpeed;
};

struct LearnsetEntry {
  uint8_t level;
  uint8_t moveId;
};

// Offset/count indirection into a flat per-species table (learnset entries or
// TM/HM-compatible move IDs), the same pattern SpeciesData uses for
// evolutions. uint16_t offset because the TM/HM table alone has 3000+ rows.
struct MoveListRef {
  uint16_t offset;
  uint8_t count;
};

enum class ItemCategory : uint8_t {
  Stone = 0,  // ids 1-6, pinned to match EvolutionItem exactly
  Ball = 1,
  Medicine = 2,
  StatusCure = 3,
  Candy = 4,
  PPRestore = 5,
  Machine = 6,  // TM/HM
  PpUp = 7,     // id 84, PP_UP_ITEM_ID - the one exception tracked in PokemonState::ppUpCount, not bagCounts
  // ids 85-90 (BATTLE_BOOST_ITEM_ID_FIRST..LAST) - X Attack/X Defense/X Speed/
  // X Special/Guard Spec./Dire Hit. Same exception as PpUp above: tracked in
  // PokemonState::battleBoostCounts, not bagCounts (which is already a full,
  // already-shipped fixed array). Only meaningful mid-battle - applied
  // directly to the live BattleCombatant via applyBattleBoostItem()
  // (PokemonBattle.h/.cpp), never persisted to a BattleRecordEntry.
  BattleBoost = 8,
  // ids 91-95 (VITAMIN_ITEM_ID_FIRST..LAST) - HP Up/Protein/Iron/Calcium/
  // Carbos. Same "outside bagCounts" exception as PpUp/BattleBoost: tracked in
  // PokemonState::vitaminCounts (save v7). Each use adds VITAMIN_EV_PER_USE to
  // one stat's EV while it is still below VITAMIN_EV_CAP (see
  // PokemonService::useVitamin()).
  Vitamin = 9,
};

// The one and only PP Up item id - deliberately outside the
// [EVOLUTION_ITEM_COUNT+1, EVOLUTION_ITEM_COUNT+POKEMON_BAG_SLOT_COUNT] range
// every other non-evolution-stone item lives in (see PokemonTypes.h), since
// its count is tracked in its own PokemonState::ppUpCount field rather than
// growing bagCounts (which would shift every byte after it in every
// already-shipped save - see the v5 note on PokemonState).
constexpr uint8_t PP_UP_ITEM_ID = 84;

// The 6 battle-boost item ids, same "outside bagCounts" exception as PP Up
// above (see PokemonState::battleBoostCounts, v6). Real Gen 1 items: each of
// the 4 X items raises one stat by 1 stage for the rest of the battle; Guard
// Spec. blocks the opponent from lowering the user's stats for the rest of
// the battle (simplified from the real 5-turn timer - see
// applyBattleBoostItem()'s doc comment); Dire Hit raises the user's own
// critical-hit ratio for the rest of the battle.
constexpr uint8_t ITEM_X_ATTACK = 85;
constexpr uint8_t ITEM_X_DEFENSE = 86;
constexpr uint8_t ITEM_X_SPEED = 87;
constexpr uint8_t ITEM_X_SPECIAL = 88;
constexpr uint8_t ITEM_GUARD_SPEC = 89;
constexpr uint8_t ITEM_DIRE_HIT = 90;
constexpr uint8_t BATTLE_BOOST_ITEM_ID_FIRST = ITEM_X_ATTACK;
constexpr uint8_t BATTLE_BOOST_ITEM_ID_LAST = ITEM_DIRE_HIT;
constexpr size_t BATTLE_BOOST_ITEM_COUNT = BATTLE_BOOST_ITEM_ID_LAST - BATTLE_BOOST_ITEM_ID_FIRST + 1U;

// The 5 EV vitamins, same "outside bagCounts" exception as above (see
// PokemonState::vitaminCounts, v7). In Gen 1 order of the stat each raises:
// HP Up -> HP, Protein -> Attack, Iron -> Defense, Calcium -> Special,
// Carbos -> Speed. Real Gen 1 gives +2560 stat exp per vitamin up to 25600 of a
// 65535 cap; this project's EVs are a simplified 0-255 scale, so the same
// 1:10 ratio is +10 EV per use, usable while that EV is still below 100.
constexpr uint8_t ITEM_HP_UP = 91;
constexpr uint8_t ITEM_PROTEIN = 92;
constexpr uint8_t ITEM_IRON = 93;
constexpr uint8_t ITEM_CALCIUM = 94;
constexpr uint8_t ITEM_CARBOS = 95;
constexpr uint8_t VITAMIN_ITEM_ID_FIRST = ITEM_HP_UP;
constexpr uint8_t VITAMIN_ITEM_ID_LAST = ITEM_CARBOS;
constexpr size_t VITAMIN_ITEM_COUNT = VITAMIN_ITEM_ID_LAST - VITAMIN_ITEM_ID_FIRST + 1U;
constexpr uint8_t VITAMIN_EV_PER_USE = 10;
constexpr uint8_t VITAMIN_EV_CAP = 100;

struct ItemData {
  uint8_t itemId;
  const char* name;
  ItemCategory category;
  uint8_t effectValue;
  Ailment curesAilment;
  uint8_t teachesMoveId;  // Machine category only, else 0
  uint8_t dropWeight;
};

struct GymTeamMember {
  uint16_t speciesId;
  uint8_t level;
  // Fixed moveset matching the real Pokemon Red trainer data (0 = empty
  // slot) - unlike a wild encounter or a player's own Pokemon, a trainer's
  // Pokemon in the real games never had its moves derived from the
  // learnset-by-level table, so this is authored explicitly per member
  // rather than computed via defaultMovesetForLevel().
  std::array<uint8_t, GYM_MOVE_SLOTS> moves{};
};

struct GymData {
  const char* leaderName;
  const char* badgeName;  // empty for Elite Four entries
  PokemonType specialty;
  uint8_t teamOffset;
  uint8_t teamCount;
};

// Combined type-effectiveness multiplier as a percent (0/25/50/100/200/400),
// folding in the defender's second type when present. 100 = neutral.
uint16_t typeEffectivenessPercent(PokemonType attackerType, PokemonType defenderPrimary, PokemonType defenderSecondary);

const MoveData* moveData(uint8_t moveId);
const BaseStats* baseStatsFor(uint16_t speciesId);
std::span<const LearnsetEntry> learnsetFor(uint16_t speciesId);
bool canLearnViaMachine(uint16_t speciesId, uint8_t moveId);
const ItemData* itemData(uint8_t itemId);
const GymData* gymData(uint8_t gymIndex);

enum class GymProgress : uint8_t {
  Locked,
  Available,
  Defeated,
};

// Pure read of a battleProgress bitfield (PokemonState::battleProgress):
// gym N (1-8) requires gyms 1..N-1 already defeated; an Elite Four member
// (9-12) requires all 8 gym bits set; the Champion (CHAMPION_GYM_INDEX)
// additionally requires all 4 Elite Four bits set. Shared by
// PokemonService::markGymDefeated (the mutating check) and the UI's gym
// list display (read-only) so the unlock rule lives in exactly one place.
GymProgress gymProgressFor(uint16_t battleProgress, uint8_t gymIndex);
std::span<const GymTeamMember> gymTeamFor(uint8_t gymIndex);

// The Champion's team (gymTeamFor(CHAMPION_GYM_INDEX)) bakes in a fixed
// species for its final (6th) slot in the generated data, but the real
// games send out the evolution that counters the player's own starter -
// Charizard/Blastoise/Venusaur depending on whether the player started with
// Bulbasaur/Charmander/Squirtle. Returns the correct substitute for that
// slot given the player's starter species (any species not in one of those
// three lines - including this project's added Pikachu starter, which the
// original games have no rival response for - falls back to Charizard).
GymTeamMember championFinalSlotFor(uint16_t starterSpeciesId);

}  // namespace pokemon
