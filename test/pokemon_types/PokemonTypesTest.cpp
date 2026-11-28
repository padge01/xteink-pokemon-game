#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string_view>

#include "Pokemon/PokemonPromptContext.h"
#include "Pokemon/PokemonSpecies.h"
#include "Pokemon/PokemonTypes.h"

namespace {

using pokemon::Gender;
using pokemon::Origin;
using pokemon::PendingEvent;
using pokemon::PendingEventKind;
using pokemon::PokemonRecord;
using pokemon::PokemonState;
using pokemon::RecordBytes;
using pokemon::RecordFlag;

int failures = 0;

#define CHECK(condition)                                                                \
  do {                                                                                  \
    if (!(condition)) {                                                                 \
      std::fprintf(stderr, "%s:%d check failed: %s\n", __FILE__, __LINE__, #condition); \
      ++failures;                                                                       \
    }                                                                                   \
  } while (false)

PokemonRecord validRecord() {
  PokemonRecord record{};
  record.recordId = 0x12345678U;
  record.totalXp = 52;
  record.speciesId = 25;
  record.caughtLevel = 5;
  record.gender = Gender::Female;
  record.origin = Origin::Starter;
  record.flags = pokemon::recordFlag(RecordFlag::EvolutionPromptsDisabled);
  CHECK(pokemon::setNickname(record, "Sparky"));
  return record;
}

void recordEncodingIsExplicitAndRoundTrips() {
  static_assert(std::tuple_size_v<RecordBytes> == 64);
  PokemonRecord source = validRecord();
  source.moves = {84, 45, 98, 0};
  source.ppUsed = {3, 0, 30, 0};
  source.hpLost = 0x0102;
  source.status = 5;
  source.statusTurns = 2;
  source.ppUps = 0x1B;
  RecordBytes bytes{};

  CHECK(pokemon::encodeRecord(source, bytes));
  CHECK(bytes[0] == 0x78 && bytes[1] == 0x56 && bytes[2] == 0x34 && bytes[3] == 0x12);
  CHECK(bytes[4] == 52 && bytes[5] == 0 && bytes[6] == 0 && bytes[7] == 0);
  CHECK(bytes[8] == 25 && bytes[9] == 0);
  CHECK(bytes[10] == 5);
  CHECK(bytes[11] == static_cast<uint8_t>(Gender::Female));
  CHECK(bytes[12] == static_cast<uint8_t>(Origin::Starter));
  CHECK(bytes[13] == pokemon::recordFlag(RecordFlag::EvolutionPromptsDisabled));
  CHECK(std::memcmp(bytes.data() + 14, "Sparky", 6) == 0);
  CHECK(bytes[20] == 0);
  CHECK(bytes[47] == 5);
  CHECK(bytes[48] == 84 && bytes[49] == 45 && bytes[50] == 98 && bytes[51] == 0);
  CHECK(bytes[52] == 3 && bytes[53] == 0 && bytes[54] == 30 && bytes[55] == 0);
  CHECK(bytes[56] == 0x02 && bytes[57] == 0x01);
  CHECK(bytes[58] == 0x1B);
  CHECK(bytes[59] == 2);
  CHECK(bytes[60] == 0 && bytes[61] == 0 && bytes[62] == 0 && bytes[63] == 0);

  PokemonRecord decoded{};
  CHECK(pokemon::decodeRecord(bytes, decoded));
  CHECK(decoded == source);

  // The spare bytes must stay empty.
  bytes[63] = 1;
  CHECK(!pokemon::decodeRecord(bytes, decoded));
}

void legacyRecordsDecodeWithAnEmptyBattleCondition() {
  static_assert(std::tuple_size_v<pokemon::LegacyRecordBytes> == 48);
  const PokemonRecord source = validRecord();
  RecordBytes current{};
  CHECK(pokemon::encodeRecord(source, current));
  // A legacy record is the first 48 bytes of the current one, ending in a zero byte.
  pokemon::LegacyRecordBytes legacy{};
  std::copy_n(current.begin(), legacy.size(), legacy.begin());

  PokemonRecord decoded{};
  CHECK(pokemon::decodeLegacyRecord(legacy, decoded));
  CHECK(decoded == source);
  CHECK(decoded.moves == (std::array<uint8_t, pokemon::POKEMON_MOVE_SLOTS>{}));
  CHECK(decoded.hpLost == 0);

  legacy[47] = 1;
  const PokemonRecord before = decoded;
  CHECK(!pokemon::decodeLegacyRecord(legacy, decoded));
  CHECK(decoded == before);
}

void battleConditionValidationRejectsImpossibleMovesAndStatus() {
  PokemonRecord record = validRecord();
  record.moves = {84, 45, 0, 0};
  record.ppUsed = {5, 0, 0, 0};
  CHECK(pokemon::validateRecord(record));

  // A Pokemon may have lost any amount of health or PP: what that means depends on its stats.
  record.hpLost = 999;
  record.ppUsed = {64, 64, 0, 0};
  CHECK(pokemon::validateRecord(record));

  PokemonRecord invalid = record;
  invalid.moves[2] = pokemon::POKEMON_MOVE_ID_MAX;  // Struggle is never learned
  CHECK(!pokemon::validateRecord(invalid));
  invalid = record;
  invalid.moves[2] = 84;  // the same move twice
  CHECK(!pokemon::validateRecord(invalid));
  invalid = record;
  invalid.ppUsed[3] = 1;  // PP spent on an empty slot
  CHECK(!pokemon::validateRecord(invalid));
  invalid = record;
  invalid.status = pokemon::POKEMON_STATUS_MAX + 1U;
  CHECK(!pokemon::validateRecord(invalid));
}

void nicknameValidationEnforcesTheStoredBoundary() {
  PokemonRecord record = validRecord();
  CHECK(pokemon::setNickname(record, ""));
  CHECK(pokemon::setNickname(record, "12345678901234567890123456789012"));
  const PokemonRecord before = record;
  CHECK(!pokemon::setNickname(record, "123456789012345678901234567890123"));
  CHECK(record == before);

  const char embeddedNull[] = {'A', '\0', 'B'};
  CHECK(!pokemon::setNickname(record, std::string_view(embeddedNull, sizeof(embeddedNull))));
  const char malformedUtf8[] = {static_cast<char>(0xC3), static_cast<char>(0x28)};
  CHECK(!pokemon::setNickname(record, std::string_view(malformedUtf8, sizeof(malformedUtf8))));
}

void nicknamePromptContextTracksThePokemonBeingNamed() {
  constexpr auto starter = pokemon::PokemonPromptContext::forStarter(1);
  static_assert(starter.speciesId == 1);
  static_assert(starter.recordId == 0);
  static_assert(starter.isStarter());

  constexpr auto caught = pokemon::PokemonPromptContext::forCaught(133, 42);
  static_assert(caught.speciesId == 133);
  static_assert(caught.recordId == 42);
  static_assert(!caught.isStarter());
}

void invalidRecordsDoNotMutateOutputs() {
  const PokemonRecord sentinel = validRecord();
  RecordBytes unchanged{};
  unchanged.fill(0xA5);

  PokemonRecord invalid = validRecord();
  invalid.speciesId = 0;
  CHECK(!pokemon::encodeRecord(invalid, unchanged));
  RecordBytes expectedUnchanged{};
  expectedUnchanged.fill(0xA5);
  CHECK(unchanged == expectedUnchanged);

  RecordBytes corrupt{};
  CHECK(pokemon::encodeRecord(sentinel, corrupt));
  corrupt[13] = 0x80;
  PokemonRecord decoded = sentinel;
  decoded.recordId = 99;
  const PokemonRecord decodedBefore = decoded;
  CHECK(!pokemon::decodeRecord(corrupt, decoded));
  CHECK(decoded == decodedBefore);

  CHECK(pokemon::encodeRecord(sentinel, corrupt));
  corrupt[47] = pokemon::POKEMON_STATUS_MAX + 1U;  // not a status
  CHECK(!pokemon::decodeRecord(corrupt, decoded));
  CHECK(decoded == decodedBefore);

  for (const auto mutateInvalidRecord : {
           +[](PokemonRecord& record) { record.totalXp = pokemon::MAXIMUM_TOTAL_XP + 1U; },
           +[](PokemonRecord& record) { record.caughtLevel = 0; },
           +[](PokemonRecord& record) { record.caughtLevel = 101; },
           +[](PokemonRecord& record) { record.gender = Gender::Unknown; },
           +[](PokemonRecord& record) { record.origin = Origin::Unknown; },
       }) {
    invalid = validRecord();
    mutateInvalidRecord(invalid);
    unchanged.fill(0xA5);
    CHECK(!pokemon::encodeRecord(invalid, unchanged));
    CHECK(unchanged == expectedUnchanged);
  }

  CHECK(pokemon::encodeRecord(sentinel, corrupt));
  std::fill(corrupt.begin() + 14, corrupt.begin() + 47, static_cast<uint8_t>('A'));
  CHECK(!pokemon::decodeRecord(corrupt, decoded));
  CHECK(decoded == decodedBefore);
}

void recordValidationRejectsImpossibleLevelAndGender() {
  PokemonRecord record = validRecord();
  record.totalXp = pokemon::xpRequired(record.caughtLevel) - 1U;
  CHECK(!pokemon::validateRecord(record));

  record = validRecord();
  record.gender = Gender::Genderless;
  CHECK(!pokemon::validateRecord(record));

  record = validRecord();
  record.speciesId = 81;
  record.gender = Gender::Genderless;
  CHECK(pokemon::validateRecord(record));
  record.gender = Gender::Female;
  CHECK(!pokemon::validateRecord(record));

  record = validRecord();
  record.speciesId = 29;
  record.gender = Gender::Male;
  CHECK(!pokemon::validateRecord(record));
  record.speciesId = 32;
  record.gender = Gender::Female;
  CHECK(!pokemon::validateRecord(record));
}

void xpBoundariesStartAtZeroAndClampAtLevelOneHundred() {
  CHECK(pokemon::xpRequired(0) == 0);
  CHECK(pokemon::xpRequired(1) == 0);
  CHECK(pokemon::xpRequired(5) == 52);
  CHECK(pokemon::xpRequired(6) == 68);
  CHECK(pokemon::xpRequired(16) == 318);
  CHECK(pokemon::xpRequired(100) == 8340);
  CHECK(pokemon::xpRequired(255) == 8340);

  CHECK(pokemon::levelForXp(0) == 1);
  CHECK(pokemon::levelForXp(51) == 4);
  CHECK(pokemon::levelForXp(52) == 5);
  CHECK(pokemon::levelForXp(8340) == 100);
  CHECK(pokemon::levelForXp(UINT32_MAX) == 100);

  for (uint8_t level = 1; level <= 100; ++level) {
    CHECK(pokemon::levelForXp(pokemon::xpRequired(level)) == level);
    if (level < 100) {
      CHECK(pokemon::levelForXp(pokemon::xpRequired(static_cast<uint8_t>(level + 1U)) - 1U) == level);
    }
  }
}

void levelProgressIsRelativeToTheCurrentLevel() {
  const pokemon::LevelXpProgress freshStarter = pokemon::levelXpProgress(52);
  CHECK(freshStarter.level == 5);
  CHECK(freshStarter.earned == 0);
  CHECK(freshStarter.required == 16);

  const pokemon::LevelXpProgress oneMinute = pokemon::levelXpProgress(53);
  CHECK(oneMinute.level == 5);
  CHECK(oneMinute.earned == 1);
  CHECK(oneMinute.required == 16);

  const pokemon::LevelXpProgress levelSix = pokemon::levelXpProgress(68);
  CHECK(levelSix.level == 6);
  CHECK(levelSix.earned == 0);
  CHECK(levelSix.required == 19);

  const pokemon::LevelXpProgress maximum = pokemon::levelXpProgress(pokemon::MAXIMUM_TOTAL_XP);
  CHECK(maximum.level == 100);
  CHECK(maximum.earned == 0);
  CHECK(maximum.required == 0);
}

PokemonState validState() {
  PokemonState state{};
  state.partyRecordIds = {1, 2, 0, 0, 0, 0};
  state.readingMinuteRemainder = 17;
  state.encounterMisses = 3;
  state.itemMisses = 8;
  state.lifetimeMinutes = 1234;
  state.sequence = 9;
  CHECK(pokemon::markSpecies(state.seenSpecies, 1));
  CHECK(pokemon::markSpecies(state.caughtSpecies, 1));
  CHECK(pokemon::markSpecies(state.seenSpecies, 25));
  return state;
}

void bagHoldsEveryItemExceptEvolutionItems() {
  static_assert(pokemon::POKEMON_FIRST_BAG_ITEM_ID == 7);
  static_assert(pokemon::POKEMON_BAG_ITEM_COUNT == 89);
  PokemonState state = validState();
  CHECK(pokemon::bagCount(state, pokemon::POKEMON_ITEM_POKE_BALL) == 0);
  CHECK(pokemon::addBagItem(state, pokemon::POKEMON_ITEM_POKE_BALL, 3));
  CHECK(pokemon::addBagItem(state, pokemon::POKEMON_ITEM_ID_MAX, 1));
  CHECK(pokemon::bagCount(state, pokemon::POKEMON_ITEM_POKE_BALL) == 3);
  CHECK(pokemon::bagCount(state, pokemon::POKEMON_ITEM_ID_MAX) == 1);
  CHECK(state.bagCounts.front() == 3 && state.bagCounts.back() == 1);

  // A stack stops at its limit instead of wrapping.
  CHECK(pokemon::addBagItem(state, pokemon::POKEMON_ITEM_POKE_BALL, 250));
  CHECK(pokemon::bagCount(state, pokemon::POKEMON_ITEM_POKE_BALL) == pokemon::POKEMON_BAG_STACK_MAX);
  CHECK(pokemon::validateState(state));

  // Evolution items and unknown ids are not in the bag.
  const PokemonState before = state;
  CHECK(!pokemon::addBagItem(state, pokemon::POKEMON_FIRST_BAG_ITEM_ID - 1U, 1));
  CHECK(!pokemon::addBagItem(state, pokemon::POKEMON_ITEM_ID_MAX + 1U, 1));
  CHECK(state == before);
  CHECK(pokemon::bagCount(state, 1) == 0);

  state.bagCounts[4] = pokemon::POKEMON_BAG_STACK_MAX + 1U;
  CHECK(!pokemon::validateState(state));

  PokemonState fresh = validState();
  pokemon::grantStartingItems(fresh);
  CHECK(pokemon::bagCount(fresh, pokemon::POKEMON_ITEM_POKE_BALL) == pokemon::POKEMON_STARTING_POKE_BALLS);
  CHECK(pokemon::bagCount(fresh, pokemon::POKEMON_ITEM_POTION) == pokemon::POKEMON_STARTING_POTIONS);
}

void leagueProgressAndHallOfFameAreValidated() {
  PokemonState state = validState();
  state.battleProgress = (1U << pokemon::POKEMON_GYM_PROGRESS_BITS) - 1U;
  CHECK(pokemon::validateState(state));
  state.battleProgress = 1U << pokemon::POKEMON_GYM_PROGRESS_BITS;
  CHECK(!pokemon::validateState(state));

  state = validState();
  state.hallOfFame.team[0] = {6, 62};
  state.hallOfFame.team[1] = {25, 58};
  state.hallOfFame.lifetimeMinutes = 6000;
  CHECK(pokemon::validateState(state));

  PokemonState invalid = state;
  invalid.hallOfFame.team[3] = {9, 60};  // a gap in the team
  CHECK(!pokemon::validateState(invalid));
  invalid = state;
  invalid.hallOfFame.team[1].level = 101;
  CHECK(!pokemon::validateState(invalid));
  invalid = state;
  invalid.hallOfFame.team[1].speciesId = pokemon::KANTO_SPECIES_COUNT + 1U;
  CHECK(!pokemon::validateState(invalid));
  invalid = validState();
  invalid.hallOfFame.lifetimeMinutes = 6000;  // a time without a team
  CHECK(!pokemon::validateState(invalid));
}

void savedBattleMustMatchThePartyAndTheWaitingList() {
  const PendingEvent encounter{0, 16, 4, Gender::Male, pokemon::EvolutionItem::None, PendingEventKind::Encounter};
  PokemonState state = validState();
  state.pendingEvents[0] = encounter;
  state.savedBattle.kind = pokemon::SavedBattleKind::Wild;
  state.savedBattle.playerSlot = 1;
  state.savedBattle.foes[0] = {7, 5};
  CHECK(pokemon::validateState(state));

  PokemonState invalid = state;
  invalid.pendingEvents[0] = {};  // nothing left to fight
  CHECK(!pokemon::validateState(invalid));
  invalid = state;
  invalid.savedBattle.playerSlot = 2;  // an empty party slot
  CHECK(!pokemon::validateState(invalid));
  invalid = state;
  invalid.savedBattle.foes[1].hpLost = 1;  // a wild Pokemon is alone
  CHECK(!pokemon::validateState(invalid));
  invalid = state;
  invalid.savedBattle.leagueIndex = 1;
  CHECK(!pokemon::validateState(invalid));
  invalid = state;
  invalid.savedBattle.foes[0].status = pokemon::POKEMON_STATUS_MAX + 1U;
  CHECK(!pokemon::validateState(invalid));

  PokemonState league = validState();
  league.savedBattle.kind = pokemon::SavedBattleKind::League;
  league.savedBattle.leagueIndex = pokemon::POKEMON_GYM_PROGRESS_BITS;
  league.savedBattle.foeSlot = 5;
  league.savedBattle.foes[4] = {300, 0};
  CHECK(pokemon::validateState(league));
  invalid = league;
  invalid.savedBattle.leagueIndex = 0;
  CHECK(!pokemon::validateState(invalid));
  invalid = league;
  invalid.savedBattle.leagueIndex = pokemon::POKEMON_GYM_PROGRESS_BITS + 1U;
  CHECK(!pokemon::validateState(invalid));
  invalid = league;
  invalid.savedBattle.foeSlot = 6;
  CHECK(!pokemon::validateState(invalid));

  // With no battle saved, nothing else may be left behind.
  invalid = validState();
  invalid.savedBattle.foes[0].hpLost = 1;
  CHECK(!pokemon::validateState(invalid));
}

void stateValidationRejectsPartyAndPokedexCorruption() {
  const PokemonState valid = validState();
  CHECK(pokemon::validateState(valid));
  CHECK(pokemon::isSpeciesMarked(valid.seenSpecies, 1));
  CHECK(pokemon::isSpeciesMarked(valid.seenSpecies, 25));
  CHECK(!pokemon::isSpeciesMarked(valid.caughtSpecies, 25));
  auto invalidMark = valid.seenSpecies;
  CHECK(!pokemon::markSpecies(invalidMark, 0));
  CHECK(invalidMark == valid.seenSpecies);

  PokemonState hole = valid;
  hole.partyRecordIds = {1, 0, 2, 0, 0, 0};
  CHECK(!pokemon::validateState(hole));

  PokemonState duplicate = valid;
  duplicate.partyRecordIds = {1, 1, 0, 0, 0, 0};
  CHECK(!pokemon::validateState(duplicate));

  PokemonState caughtWithoutSeen = valid;
  CHECK(pokemon::markSpecies(caughtWithoutSeen.caughtSpecies, 7));
  CHECK(!pokemon::validateState(caughtWithoutSeen));

  PokemonState paddingBit = valid;
  paddingBit.seenSpecies.back() |= 0x80U;
  CHECK(!pokemon::validateState(paddingBit));

  PokemonState badCounters = valid;
  badCounters.readingMinuteRemainder = 60;
  CHECK(!pokemon::validateState(badCounters));
  badCounters = valid;
  badCounters.encounterMisses = 6;
  CHECK(!pokemon::validateState(badCounters));
  badCounters = valid;
  badCounters.itemMisses = 20;
  CHECK(!pokemon::validateState(badCounters));
}

void pendingEventValidationFollowsItsTag() {
  PokemonState state = validState();
  state.pendingEvents[0].kind = PendingEventKind::Encounter;
  state.pendingEvents[0].speciesId = 81;
  state.pendingEvents[0].level = 24;
  state.pendingEvents[0].gender = Gender::Genderless;
  CHECK(pokemon::validateState(state));

  state.pendingEvents[0].gender = Gender::Female;
  CHECK(!pokemon::validateState(state));
  state.pendingEvents[0].speciesId = 1;
  state.pendingEvents[0].gender = Gender::Genderless;
  CHECK(!pokemon::validateState(state));

  state.pendingEvents[0].speciesId = 81;
  state.pendingEvents[0].gender = Gender::Genderless;
  state.pendingEvents[0].item = pokemon::EvolutionItem::MoonStone;
  CHECK(!pokemon::validateState(state));

  state = validState();
  state.pendingEvents[0].kind = PendingEventKind::Item;
  state.pendingEvents[0].item = pokemon::EvolutionItem::LeafStone;
  CHECK(pokemon::validateState(state));

  state.pendingEvents[0].recordId = 3;
  CHECK(!pokemon::validateState(state));

  state = validState();
  state.pendingEvents[0].kind = PendingEventKind::Evolution;
  state.pendingEvents[0].recordId = 1;
  state.pendingEvents[0].speciesId = 2;
  CHECK(pokemon::validateState(state));

  state.pendingEvents[0].level = 16;
  CHECK(!pokemon::validateState(state));
}

void pendingEventQueueIsFixedFifoAndCompacted() {
  PokemonState state = validState();
  const PendingEvent encounter{0, 25, 5, Gender::Female, pokemon::EvolutionItem::None, PendingEventKind::Encounter};
  const PendingEvent item{0, 0, 0, Gender::Unknown, pokemon::EvolutionItem::ThunderStone, PendingEventKind::Item};
  const PendingEvent evolution{1, 2, 0, Gender::Unknown, pokemon::EvolutionItem::None, PendingEventKind::Evolution};
  const PendingEvent fourth{2, 5, 0, Gender::Unknown, pokemon::EvolutionItem::None, PendingEventKind::Evolution};

  CHECK(pokemon::pendingEventCount(state) == 0);
  CHECK(pokemon::pendingEventFront(state) == nullptr);
  CHECK(pokemon::enqueuePendingEvent(state, encounter));
  CHECK(pokemon::enqueuePendingEvent(state, item));
  CHECK(pokemon::enqueuePendingEvent(state, evolution));
  CHECK(pokemon::pendingEventCount(state) == 3);

  // Once every slot is taken the queue refuses another event and is left as it was.
  PokemonState full = state;
  while (pokemon::pendingEventCount(full) < pokemon::PENDING_EVENT_CAPACITY) {
    CHECK(pokemon::enqueuePendingEvent(full, item));
  }
  const PokemonState fullBefore = full;
  CHECK(!pokemon::enqueuePendingEvent(full, fourth));
  CHECK(full == fullBefore);
  CHECK(pokemon::pendingEventFront(state) != nullptr);
  CHECK(*pokemon::pendingEventFront(state) == encounter);

  CHECK(pokemon::dequeuePendingEvent(state));
  CHECK(*pokemon::pendingEventFront(state) == item);
  CHECK(pokemon::removePendingEvolutionsForRecord(state, evolution.recordId) == 1);
  CHECK(pokemon::pendingEventCount(state) == 1);
  CHECK(*pokemon::pendingEventFront(state) == item);
  CHECK(pokemon::dequeuePendingEvent(state));
  CHECK(!pokemon::dequeuePendingEvent(state));
  CHECK(pokemon::pendingEventFront(state) == nullptr);

  state = validState();
  state.pendingEvents[1] = encounter;
  CHECK(!pokemon::validateState(state));
}

struct ExpectedEvolution {
  uint16_t source;
  uint16_t target;
  pokemon::EvolutionTrigger trigger;
  uint8_t level;
  pokemon::EvolutionItem item;
};

constexpr ExpectedEvolution EXPECTED_EVOLUTIONS[] = {
    {1, 2, pokemon::EvolutionTrigger::Level, 16, pokemon::EvolutionItem::None},
    {2, 3, pokemon::EvolutionTrigger::Level, 32, pokemon::EvolutionItem::None},
    {4, 5, pokemon::EvolutionTrigger::Level, 16, pokemon::EvolutionItem::None},
    {5, 6, pokemon::EvolutionTrigger::Level, 36, pokemon::EvolutionItem::None},
    {7, 8, pokemon::EvolutionTrigger::Level, 16, pokemon::EvolutionItem::None},
    {8, 9, pokemon::EvolutionTrigger::Level, 36, pokemon::EvolutionItem::None},
    {10, 11, pokemon::EvolutionTrigger::Level, 7, pokemon::EvolutionItem::None},
    {11, 12, pokemon::EvolutionTrigger::Level, 10, pokemon::EvolutionItem::None},
    {13, 14, pokemon::EvolutionTrigger::Level, 7, pokemon::EvolutionItem::None},
    {14, 15, pokemon::EvolutionTrigger::Level, 10, pokemon::EvolutionItem::None},
    {16, 17, pokemon::EvolutionTrigger::Level, 18, pokemon::EvolutionItem::None},
    {17, 18, pokemon::EvolutionTrigger::Level, 36, pokemon::EvolutionItem::None},
    {19, 20, pokemon::EvolutionTrigger::Level, 20, pokemon::EvolutionItem::None},
    {21, 22, pokemon::EvolutionTrigger::Level, 20, pokemon::EvolutionItem::None},
    {23, 24, pokemon::EvolutionTrigger::Level, 22, pokemon::EvolutionItem::None},
    {25, 26, pokemon::EvolutionTrigger::Item, 0, pokemon::EvolutionItem::ThunderStone},
    {27, 28, pokemon::EvolutionTrigger::Level, 22, pokemon::EvolutionItem::None},
    {29, 30, pokemon::EvolutionTrigger::Level, 16, pokemon::EvolutionItem::None},
    {30, 31, pokemon::EvolutionTrigger::Item, 0, pokemon::EvolutionItem::MoonStone},
    {32, 33, pokemon::EvolutionTrigger::Level, 16, pokemon::EvolutionItem::None},
    {33, 34, pokemon::EvolutionTrigger::Item, 0, pokemon::EvolutionItem::MoonStone},
    {35, 36, pokemon::EvolutionTrigger::Item, 0, pokemon::EvolutionItem::MoonStone},
    {37, 38, pokemon::EvolutionTrigger::Item, 0, pokemon::EvolutionItem::FireStone},
    {39, 40, pokemon::EvolutionTrigger::Item, 0, pokemon::EvolutionItem::MoonStone},
    {41, 42, pokemon::EvolutionTrigger::Level, 22, pokemon::EvolutionItem::None},
    {43, 44, pokemon::EvolutionTrigger::Level, 21, pokemon::EvolutionItem::None},
    {44, 45, pokemon::EvolutionTrigger::Item, 0, pokemon::EvolutionItem::LeafStone},
    {46, 47, pokemon::EvolutionTrigger::Level, 24, pokemon::EvolutionItem::None},
    {48, 49, pokemon::EvolutionTrigger::Level, 31, pokemon::EvolutionItem::None},
    {50, 51, pokemon::EvolutionTrigger::Level, 26, pokemon::EvolutionItem::None},
    {52, 53, pokemon::EvolutionTrigger::Level, 28, pokemon::EvolutionItem::None},
    {54, 55, pokemon::EvolutionTrigger::Level, 33, pokemon::EvolutionItem::None},
    {56, 57, pokemon::EvolutionTrigger::Level, 28, pokemon::EvolutionItem::None},
    {58, 59, pokemon::EvolutionTrigger::Item, 0, pokemon::EvolutionItem::FireStone},
    {60, 61, pokemon::EvolutionTrigger::Level, 25, pokemon::EvolutionItem::None},
    {61, 62, pokemon::EvolutionTrigger::Item, 0, pokemon::EvolutionItem::WaterStone},
    {63, 64, pokemon::EvolutionTrigger::Level, 16, pokemon::EvolutionItem::None},
    {64, 65, pokemon::EvolutionTrigger::Item, 0, pokemon::EvolutionItem::LinkCable},
    {66, 67, pokemon::EvolutionTrigger::Level, 28, pokemon::EvolutionItem::None},
    {67, 68, pokemon::EvolutionTrigger::Item, 0, pokemon::EvolutionItem::LinkCable},
    {69, 70, pokemon::EvolutionTrigger::Level, 21, pokemon::EvolutionItem::None},
    {70, 71, pokemon::EvolutionTrigger::Item, 0, pokemon::EvolutionItem::LeafStone},
    {72, 73, pokemon::EvolutionTrigger::Level, 30, pokemon::EvolutionItem::None},
    {74, 75, pokemon::EvolutionTrigger::Level, 25, pokemon::EvolutionItem::None},
    {75, 76, pokemon::EvolutionTrigger::Item, 0, pokemon::EvolutionItem::LinkCable},
    {77, 78, pokemon::EvolutionTrigger::Level, 40, pokemon::EvolutionItem::None},
    {79, 80, pokemon::EvolutionTrigger::Level, 37, pokemon::EvolutionItem::None},
    {81, 82, pokemon::EvolutionTrigger::Level, 30, pokemon::EvolutionItem::None},
    {84, 85, pokemon::EvolutionTrigger::Level, 31, pokemon::EvolutionItem::None},
    {86, 87, pokemon::EvolutionTrigger::Level, 34, pokemon::EvolutionItem::None},
    {88, 89, pokemon::EvolutionTrigger::Level, 38, pokemon::EvolutionItem::None},
    {90, 91, pokemon::EvolutionTrigger::Item, 0, pokemon::EvolutionItem::WaterStone},
    {92, 93, pokemon::EvolutionTrigger::Level, 25, pokemon::EvolutionItem::None},
    {93, 94, pokemon::EvolutionTrigger::Item, 0, pokemon::EvolutionItem::LinkCable},
    {96, 97, pokemon::EvolutionTrigger::Level, 26, pokemon::EvolutionItem::None},
    {98, 99, pokemon::EvolutionTrigger::Level, 28, pokemon::EvolutionItem::None},
    {100, 101, pokemon::EvolutionTrigger::Level, 30, pokemon::EvolutionItem::None},
    {102, 103, pokemon::EvolutionTrigger::Item, 0, pokemon::EvolutionItem::LeafStone},
    {104, 105, pokemon::EvolutionTrigger::Level, 28, pokemon::EvolutionItem::None},
    {109, 110, pokemon::EvolutionTrigger::Level, 35, pokemon::EvolutionItem::None},
    {111, 112, pokemon::EvolutionTrigger::Level, 42, pokemon::EvolutionItem::None},
    {116, 117, pokemon::EvolutionTrigger::Level, 32, pokemon::EvolutionItem::None},
    {118, 119, pokemon::EvolutionTrigger::Level, 33, pokemon::EvolutionItem::None},
    {120, 121, pokemon::EvolutionTrigger::Item, 0, pokemon::EvolutionItem::WaterStone},
    {129, 130, pokemon::EvolutionTrigger::Level, 20, pokemon::EvolutionItem::None},
    {133, 134, pokemon::EvolutionTrigger::Item, 0, pokemon::EvolutionItem::WaterStone},
    {133, 135, pokemon::EvolutionTrigger::Item, 0, pokemon::EvolutionItem::ThunderStone},
    {133, 136, pokemon::EvolutionTrigger::Item, 0, pokemon::EvolutionItem::FireStone},
    {138, 139, pokemon::EvolutionTrigger::Level, 40, pokemon::EvolutionItem::None},
    {140, 141, pokemon::EvolutionTrigger::Level, 40, pokemon::EvolutionItem::None},
    {147, 148, pokemon::EvolutionTrigger::Level, 30, pokemon::EvolutionItem::None},
    {148, 149, pokemon::EvolutionTrigger::Level, 55, pokemon::EvolutionItem::None},
};

void speciesTableCoversKantoAndPinnedMetadata() {
  CHECK(pokemon::speciesData(0) == nullptr);
  CHECK(pokemon::speciesData(152) == nullptr);
  for (uint16_t speciesId = 1; speciesId <= 151; ++speciesId) {
    const pokemon::SpeciesData* species = pokemon::speciesData(speciesId);
    CHECK(species != nullptr);
    if (species == nullptr) continue;
    CHECK(species->speciesId == speciesId);
    CHECK(species->name != nullptr && species->name[0] != '\0');
    CHECK(species->primaryType != pokemon::PokemonType::None);
    CHECK(species->genderRate <= 8 || species->genderRate == 255);
    CHECK(species->captureRate >= 1);
  }

  const auto* bulbasaur = pokemon::speciesData(1);
  CHECK(bulbasaur->primaryType == pokemon::PokemonType::Grass);
  CHECK(bulbasaur->secondaryType == pokemon::PokemonType::Poison);
  CHECK(bulbasaur->genderRate == 1);
  CHECK(pokemon::speciesData(29)->genderRate == 8);
  CHECK(pokemon::speciesData(32)->genderRate == 0);
  CHECK(pokemon::speciesData(81)->genderRate == 255);
  CHECK(pokemon::speciesData(81)->secondaryType == pokemon::PokemonType::Steel);
  CHECK(pokemon::speciesData(35)->primaryType == pokemon::PokemonType::Fairy);
  CHECK(pokemon::speciesData(149)->secondaryType == pokemon::PokemonType::Flying);
  CHECK(pokemon::speciesData(25)->stage == pokemon::EvolutionStage::Base);
  CHECK(pokemon::speciesData(2)->stage == pokemon::EvolutionStage::Middle);
  CHECK(pokemon::speciesData(3)->stage == pokemon::EvolutionStage::Final);
}

void acquisitionRulesKeepItemsNecessaryAndMewSpecial() {
  constexpr uint16_t evolutionOnly[] = {26, 31, 34, 36, 38, 40,  45,  59,  62,  65,
                                        68, 71, 76, 91, 94, 103, 121, 134, 135, 136};
  size_t actualEvolutionOnly = 0;
  for (uint16_t speciesId = 1; speciesId <= 151; ++speciesId) {
    if (pokemon::speciesData(speciesId)->acquisition == pokemon::Acquisition::EvolutionOnly) {
      ++actualEvolutionOnly;
    }
  }
  CHECK(actualEvolutionOnly == std::size(evolutionOnly));
  for (const uint16_t speciesId : evolutionOnly) {
    CHECK(pokemon::speciesData(speciesId)->acquisition == pokemon::Acquisition::EvolutionOnly);
  }
  size_t legendaryCount = 0;
  size_t specialCount = 0;
  for (uint16_t speciesId = 1; speciesId <= 151; ++speciesId) {
    legendaryCount += pokemon::speciesData(speciesId)->acquisition == pokemon::Acquisition::Legendary ? 1U : 0U;
    specialCount += pokemon::speciesData(speciesId)->acquisition == pokemon::Acquisition::Special ? 1U : 0U;
  }
  CHECK(legendaryCount == 4);
  CHECK(specialCount == 1);
  CHECK(pokemon::speciesData(144)->acquisition == pokemon::Acquisition::Legendary);
  CHECK(pokemon::speciesData(145)->acquisition == pokemon::Acquisition::Legendary);
  CHECK(pokemon::speciesData(146)->acquisition == pokemon::Acquisition::Legendary);
  CHECK(pokemon::speciesData(150)->acquisition == pokemon::Acquisition::Legendary);
  CHECK(pokemon::speciesData(151)->acquisition == pokemon::Acquisition::Special);
}

void everyKantoEvolutionMatchesTheCanonicalList() {
  size_t actualCount = 0;
  for (uint16_t source = 1; source <= 151; ++source) {
    const auto actual = pokemon::evolutionsFor(source);
    actualCount += actual.size();
    for (const pokemon::EvolutionRule& rule : actual) {
      bool found = false;
      for (const ExpectedEvolution& expected : EXPECTED_EVOLUTIONS) {
        if (expected.source == source && expected.target == rule.targetSpeciesId && expected.trigger == rule.trigger &&
            expected.level == rule.minimumLevel && expected.item == rule.item) {
          found = true;
          break;
        }
      }
      CHECK(found);
    }
  }
  CHECK(actualCount == std::size(EXPECTED_EVOLUTIONS));
  for (const ExpectedEvolution& expected : EXPECTED_EVOLUTIONS) {
    size_t matches = 0;
    for (const pokemon::EvolutionRule& rule : pokemon::evolutionsFor(expected.source)) {
      if (expected.target == rule.targetSpeciesId && expected.trigger == rule.trigger &&
          expected.level == rule.minimumLevel && expected.item == rule.item) {
        ++matches;
      }
    }
    CHECK(matches == 1);
  }
}

}  // namespace

int main() {
  recordEncodingIsExplicitAndRoundTrips();
  legacyRecordsDecodeWithAnEmptyBattleCondition();
  battleConditionValidationRejectsImpossibleMovesAndStatus();
  nicknameValidationEnforcesTheStoredBoundary();
  nicknamePromptContextTracksThePokemonBeingNamed();
  invalidRecordsDoNotMutateOutputs();
  recordValidationRejectsImpossibleLevelAndGender();
  xpBoundariesStartAtZeroAndClampAtLevelOneHundred();
  levelProgressIsRelativeToTheCurrentLevel();
  bagHoldsEveryItemExceptEvolutionItems();
  leagueProgressAndHallOfFameAreValidated();
  savedBattleMustMatchThePartyAndTheWaitingList();
  stateValidationRejectsPartyAndPokedexCorruption();
  pendingEventValidationFollowsItsTag();
  pendingEventQueueIsFixedFifoAndCompacted();
  speciesTableCoversKantoAndPinnedMetadata();
  acquisitionRulesKeepItemsNecessaryAndMewSpecial();
  everyKantoEvolutionMatchesTheCanonicalList();
  return failures == 0 ? 0 : 1;
}
