#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace pokemon {

constexpr size_t POKEMON_RECORD_BYTES = 64;
// Record size in snapshot versions 1 and 2, from before a Pokemon carried moves and health.
constexpr size_t POKEMON_LEGACY_RECORD_BYTES = 48;
constexpr size_t POKEMON_NICKNAME_BYTES = 33;
constexpr uint16_t KANTO_SPECIES_COUNT = 151;
// Trainer classes met while reading - see TrainerClass in PokemonTrainers.h.
constexpr uint8_t TRAINER_CLASS_COUNT = 12;
constexpr uint32_t MAXIMUM_TOTAL_XP = 8340;
constexpr size_t POKEDEX_BYTES = 19;
constexpr size_t PARTY_SIZE = 6;
constexpr size_t EVOLUTION_ITEM_COUNT = 6;
constexpr size_t PENDING_EVENT_CAPACITY = 8;

// Sizes of the generated battle data tables. PokemonBattleTypes.h sits above
// this header, so these are pinned here and checked by a static_assert next to
// each table (PokemonMoveData.cpp, PokemonItemData.cpp, PokemonGymData.cpp).
constexpr uint16_t POKEMON_MOVE_ID_MAX = 165;
constexpr size_t POKEMON_BAG_SLOT_COUNT = 77;
constexpr uint8_t POKEMON_ITEM_ID_MAX = 95;
constexpr size_t POKEMON_BATTLE_BOOST_ITEM_COUNT = 6;
constexpr size_t POKEMON_VITAMIN_ITEM_COUNT = 5;
constexpr uint16_t POKEMON_GYM_PROGRESS_BITS = 13;  // 8 gyms + 4 Elite Four + the Champion
constexpr size_t POKEMON_MOVE_SLOTS = 4;
constexpr uint8_t POKEMON_STATUS_MAX = 6;  // the highest Ailment a Pokemon can carry: Confusion

// The bag holds every item except the six evolution items, which keep their own counts in
// PokemonState::itemCounts. bagCounts[0] is item id POKEMON_FIRST_BAG_ITEM_ID.
constexpr uint8_t POKEMON_FIRST_BAG_ITEM_ID = EVOLUTION_ITEM_COUNT + 1;
constexpr size_t POKEMON_BAG_ITEM_COUNT = POKEMON_ITEM_ID_MAX - EVOLUTION_ITEM_COUNT;
constexpr uint8_t POKEMON_BAG_STACK_MAX = 99;
constexpr uint8_t POKEMON_ITEM_POKE_BALL = 7;
constexpr uint8_t POKEMON_ITEM_POTION = 11;
// What a new game starts with, and what a save from before battles is given when it is upgraded.
constexpr uint8_t POKEMON_STARTING_POKE_BALLS = 10;
constexpr uint8_t POKEMON_STARTING_POTIONS = 1;

enum class Gender : uint8_t {
  Unknown = 0,
  Male = 1,
  Female = 2,
  Genderless = 3,
};

enum class Origin : uint8_t {
  Unknown = 0,
  Caught = 1,
  Starter = 2,
};

enum class RecordFlag : uint8_t {
  EvolutionPromptsDisabled = 1U << 0U,
};

enum class EvolutionItem : uint8_t {
  None = 0,
  MoonStone = 1,
  FireStone = 2,
  ThunderStone = 3,
  WaterStone = 4,
  LeafStone = 5,
  LinkCable = 6,
};

enum class PendingEventKind : uint8_t {
  None = 0,
  Encounter = 1,
  Item = 2,
  Evolution = 3,
};

enum class DashboardNotice : uint8_t {
  None = 0,
  NewPokemon = 1,
  ItemFound = 2,
  WhatsThis = 3,
};

constexpr uint8_t recordFlag(const RecordFlag flag) { return static_cast<uint8_t>(flag); }

struct PokemonRecord {
  uint32_t recordId = 0;
  uint32_t totalXp = 0;
  uint16_t speciesId = 0;
  uint8_t caughtLevel = 0;
  Gender gender = Gender::Unknown;
  Origin origin = Origin::Unknown;
  uint8_t flags = 0;
  std::array<char, POKEMON_NICKNAME_BYTES> nickname{};
  // Battle condition, carried from one battle to the next. Health and PP are stored as what has
  // been lost, so zero always means full: a record needs no stat table to be valid, and a level
  // gained keeps the damage rather than the remaining health.
  std::array<uint8_t, POKEMON_MOVE_SLOTS> moves{};   // move ids; 0 is an empty slot
  std::array<uint8_t, POKEMON_MOVE_SLOTS> ppUsed{};  // PP spent per move slot
  uint16_t hpLost = 0;                               // at or above maximum HP means fainted
  uint8_t status = 0;                                // an Ailment value, up to POKEMON_STATUS_MAX
  uint8_t statusTurns = 0;                           // turns left of sleep or confusion
  uint8_t ppUps = 0;                                 // PP Ups used, two bits per move slot

  bool operator==(const PokemonRecord&) const = default;
};

struct LevelXpProgress {
  uint8_t level = 1;
  uint32_t earned = 0;
  uint32_t required = 0;
};

using RecordBytes = std::array<uint8_t, POKEMON_RECORD_BYTES>;
using LegacyRecordBytes = std::array<uint8_t, POKEMON_LEGACY_RECORD_BYTES>;

struct PendingEvent {
  uint32_t recordId = 0;
  uint16_t speciesId = 0;
  uint8_t level = 0;
  Gender gender = Gender::Unknown;
  EvolutionItem item = EvolutionItem::None;
  PendingEventKind kind = PendingEventKind::None;

  bool operator==(const PendingEvent&) const = default;
};

using PokedexBits = std::array<uint8_t, POKEDEX_BYTES>;

// Where the player is on the map. `area` counts areas along the main path from the first town
// (0), `steps` is how far into that area they have walked, and `destination` is the area they
// are walking to.
struct MapPosition {
  uint8_t area = 0;
  uint8_t destination = 0;
  uint16_t steps = 0;

  bool operator==(const MapPosition&) const = default;
};

struct HallOfFameMember {
  uint16_t speciesId = 0;
  uint8_t level = 0;

  bool operator==(const HallOfFameMember&) const = default;
};

// The party that beat the Champion, and the reading time it took.
struct HallOfFame {
  std::array<HallOfFameMember, PARTY_SIZE> team{};
  uint32_t lifetimeMinutes = 0;

  bool operator==(const HallOfFame&) const = default;
};

enum class SavedBattleKind : uint8_t {
  None = 0,
  Wild = 1,    // against the encounter at the front of the waiting list
  League = 2,  // against the gym leader, Elite Four member or Champion in `leagueIndex`
};

// One of the opponent's Pokemon in a battle that was left unfinished.
struct SavedBattleFoe {
  uint16_t hpLost = 0;
  uint8_t status = 0;

  bool operator==(const SavedBattleFoe&) const = default;
};

// A battle that was interrupted by sleep or by leaving the game, so it can pick up where it
// stopped. The player's side is already in the party records; this holds who was on the field
// and the opponent's condition. The opponent's team itself is rebuilt from the encounter or the
// league roster, and effects that last only for a battle (stat changes, PP the opponent spent)
// start over.
struct SavedBattle {
  SavedBattleKind kind = SavedBattleKind::None;
  uint8_t leagueIndex = 0;  // 1 to POKEMON_GYM_PROGRESS_BITS for a League battle, otherwise 0
  uint8_t playerSlot = 0;   // party slot on the field
  uint8_t foeSlot = 0;      // opponent team member on the field
  std::array<SavedBattleFoe, PARTY_SIZE> foes{};

  bool operator==(const SavedBattle&) const = default;
};

struct PokemonState {
  std::array<uint32_t, PARTY_SIZE> partyRecordIds{};
  std::array<PendingEvent, PENDING_EVENT_CAPACITY> pendingEvents{};
  std::array<uint16_t, EVOLUTION_ITEM_COUNT> itemCounts{};
  std::array<uint8_t, POKEMON_BAG_ITEM_COUNT> bagCounts{};
  PokedexBits seenSpecies{};
  PokedexBits caughtSpecies{};
  HallOfFame hallOfFame{};
  SavedBattle savedBattle{};
  MapPosition map{};
  uint32_t lifetimeMinutes = 0;
  uint32_t sequence = 0;
  // One bit per league battle won, in challenge order: bit 0 is the first gym.
  uint16_t battleProgress = 0;
  uint8_t readingMinuteRemainder = 0;
  uint8_t encounterMisses = 0;
  uint8_t itemMisses = 0;
  DashboardNotice dashboardNotice = DashboardNotice::None;

  bool operator==(const PokemonState&) const = default;
};

bool validateNickname(std::string_view nickname);
bool setNickname(PokemonRecord& record, std::string_view nickname);
bool validateRecord(const PokemonRecord& record);
bool encodeRecord(const PokemonRecord& record, RecordBytes& output);
bool decodeRecord(const RecordBytes& bytes, PokemonRecord& output);
// Reads a record from a version 1 or 2 snapshot. Its battle condition comes out empty: no moves,
// full health.
bool decodeLegacyRecord(const LegacyRecordBytes& bytes, PokemonRecord& output);
uint32_t xpRequired(uint8_t level);
uint8_t levelForXp(uint32_t totalXp);
LevelXpProgress levelXpProgress(uint32_t totalXp);
bool markSpecies(PokedexBits& bits, uint16_t speciesId);
bool isSpeciesMarked(const PokedexBits& bits, uint16_t speciesId);
size_t pendingEventCount(const PokemonState& state);
const PendingEvent* pendingEventFront(const PokemonState& state);
PendingEvent* pendingEventFront(PokemonState& state);
bool enqueuePendingEvent(PokemonState& state, const PendingEvent& event);
bool dequeuePendingEvent(PokemonState& state);
size_t removePendingEvolutionsForRecord(PokemonState& state, uint32_t recordId);
bool validateState(const PokemonState& state);
// How many of an item the bag holds; 0 for an id outside the bag.
uint8_t bagCount(const PokemonState& state, uint8_t itemId);
// Adds `count` of an item, stopping at POKEMON_BAG_STACK_MAX. False for an id outside the bag.
bool addBagItem(PokemonState& state, uint8_t itemId, uint8_t count);
// The Poke Balls and Potion every player starts with.
void grantStartingItems(PokemonState& state);

}  // namespace pokemon
