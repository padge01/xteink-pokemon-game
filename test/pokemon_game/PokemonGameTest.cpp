#include <cstdint>
#include <cstdio>
#include <iterator>
#include <string_view>

#include "Pokemon/PokemonGame.h"
#include "Pokemon/PokemonSpecies.h"

namespace {

int failures = 0;

#define CHECK(condition)                                                                  \
  do {                                                                                    \
    if (!(condition)) {                                                                   \
      std::fprintf(stderr, "%s:%d check failed: %s\n", __FILE__, __LINE__, #condition); \
      ++failures;                                                                         \
    }                                                                                     \
  } while (false)

uint32_t chooseFirst(void*, uint32_t) { return 0; }

struct SequenceRandom {
  const uint32_t* values;
  size_t count;
  size_t index = 0;

  static uint32_t next(void* context, const uint32_t upperExclusive) {
    auto& sequence = *static_cast<SequenceRandom*>(context);
    if (sequence.index >= sequence.count) {
      CHECK(false);
      return 0;
    }
    const uint32_t value = sequence.values[sequence.index++];
    CHECK(value < upperExclusive);
    return value;
  }
};

uint32_t rejectUnexpectedDraw(void*, uint32_t) {
  CHECK(false);
  return 0;
}

pokemon::PokemonRecord leaderAtLevelFive() {
  pokemon::PokemonRecord leader{};
  leader.recordId = 7;
  leader.totalXp = 52;
  leader.speciesId = 25;
  leader.caughtLevel = 5;
  leader.gender = pokemon::Gender::Female;
  leader.origin = pokemon::Origin::Starter;
  return leader;
}

pokemon::PokemonState stateWithLeader(const pokemon::PokemonRecord& leader) {
  pokemon::PokemonState state{};
  state.partyRecordIds[0] = leader.recordId;
  return state;
}

void creditedMinutesAdvanceLeaderAndReadingCounters() {
  pokemon::PokemonRecord leader = leaderAtLevelFive();
  pokemon::PokemonState state = stateWithLeader(leader);
  pokemon::RandomSource random{nullptr, chooseFirst};

  const pokemon::CreditResult result =
      pokemon::applyCreditedMinutes(state, leader, 15, 10, pokemon::OwnedEvolutionNeeds{}, random);

  CHECK(result.status == pokemon::CreditStatus::Applied);
  CHECK(result.creditedMinutes == 15);
  CHECK(result.previousLevel == 5);
  CHECK(result.currentLevel == 5);
  CHECK(result.generatedEvent == pokemon::PendingEventKind::None);
  CHECK(leader.totalXp == 67);
  CHECK(state.lifetimeMinutes == 15);
  CHECK(state.readingMinuteRemainder == 15);
}

void creditClampsAndRejectsInvalidCallsWithoutMutation() {
  pokemon::PokemonRecord leader = leaderAtLevelFive();
  leader.totalXp = 8339;
  pokemon::PokemonState state = stateWithLeader(leader);
  state.lifetimeMinutes = UINT32_MAX - 2U;
  pokemon::RandomSource random{nullptr, chooseFirst};

  const pokemon::CreditResult clamped =
      pokemon::applyCreditedMinutes(state, leader, 10, 20, pokemon::OwnedEvolutionNeeds{}, random);
  CHECK(clamped.status == pokemon::CreditStatus::Applied);
  CHECK(clamped.previousLevel == 99);
  CHECK(clamped.currentLevel == 100);
  CHECK(leader.totalXp == pokemon::MAXIMUM_TOTAL_XP);
  CHECK(state.lifetimeMinutes == UINT32_MAX);

  pokemon::PokemonRecord rejectedLeader = leaderAtLevelFive();
  pokemon::PokemonState rejectedState = stateWithLeader(rejectedLeader);
  rejectedState.partyRecordIds[0] = 99;
  const pokemon::PokemonRecord leaderBefore = rejectedLeader;
  const pokemon::PokemonState stateBefore = rejectedState;
  const pokemon::CreditResult rejected = pokemon::applyCreditedMinutes(
      rejectedState, rejectedLeader, 10, 20, pokemon::OwnedEvolutionNeeds{}, random);
  CHECK(rejected.status == pokemon::CreditStatus::Rejected);
  CHECK(rejectedLeader == leaderBefore);
  CHECK(rejectedState == stateBefore);

  rejectedState = stateWithLeader(rejectedLeader);
  const pokemon::CreditResult noChange = pokemon::applyCreditedMinutes(
      rejectedState, rejectedLeader, 0, 20, pokemon::OwnedEvolutionNeeds{}, random);
  CHECK(noChange.status == pokemon::CreditStatus::NoChange);
  CHECK(rejectedLeader == leaderBefore);
  CHECK(rejectedState == stateWithLeader(rejectedLeader));
}

void sixthEncounterCheckIsForcedAndCreatesOnlyOneEvent() {
  pokemon::PokemonRecord leader = leaderAtLevelFive();
  pokemon::PokemonState state = stateWithLeader(leader);
  constexpr uint32_t draws[] = {
      1, 1,  // encounter miss, item miss
      1, 1,
      1, 1,
      1, 1,
      1, 1,
      1, 0, 0, 0,  // legendary miss, first weighted species, minimum level, female roll
  };
  SequenceRandom sequence{draws, std::size(draws)};
  pokemon::RandomSource random{&sequence, SequenceRandom::next};

  for (uint8_t check = 1; check <= 5; ++check) {
    const pokemon::CreditResult result =
        pokemon::applyCreditedMinutes(state, leader, 60, 10, pokemon::OwnedEvolutionNeeds{}, random);
    CHECK(result.status == pokemon::CreditStatus::Applied);
    CHECK(result.generatedEvent == pokemon::PendingEventKind::None);
    CHECK(state.encounterMisses == check);
    CHECK(state.itemMisses == check);
  }

  const pokemon::CreditResult forced =
      pokemon::applyCreditedMinutes(state, leader, 60, 10, pokemon::OwnedEvolutionNeeds{}, random);
  CHECK(forced.status == pokemon::CreditStatus::Applied);
  CHECK(forced.generatedEvent == pokemon::PendingEventKind::Encounter);
  CHECK(state.pending.kind == pokemon::PendingEventKind::Encounter);
  CHECK(state.pending.speciesId == 1);
  CHECK(state.pending.level == 2);
  CHECK(state.pending.gender == pokemon::Gender::Female);
  CHECK(state.encounterMisses == 0);
  CHECK(state.itemMisses == 6);
  CHECK(state.readingMinuteRemainder == 0);
  CHECK(state.lifetimeMinutes == 360);
  CHECK(sequence.index == std::size(draws));
}

void pendingEventBlocksRollsButStillAdvancesPity() {
  pokemon::PokemonRecord leader = leaderAtLevelFive();
  pokemon::PokemonState state = stateWithLeader(leader);
  state.pending.kind = pokemon::PendingEventKind::Encounter;
  state.pending.speciesId = 81;
  state.pending.level = 24;
  state.pending.gender = pokemon::Gender::Genderless;
  const pokemon::PendingEvent pendingBefore = state.pending;
  pokemon::RandomSource random{nullptr, rejectUnexpectedDraw};

  const pokemon::CreditResult result =
      pokemon::applyCreditedMinutes(state, leader, 125, 50, pokemon::OwnedEvolutionNeeds{}, random);

  CHECK(result.status == pokemon::CreditStatus::Applied);
  CHECK(result.generatedEvent == pokemon::PendingEventKind::None);
  CHECK(state.pending == pendingBefore);
  CHECK(state.encounterMisses == 2);
  CHECK(state.itemMisses == 2);
  CHECK(state.readingMinuteRemainder == 5);
  CHECK(state.lifetimeMinutes == 125);
  CHECK(leader.totalXp == 177);
}

pokemon::PendingEvent forceEncounterAtProgress(const uint8_t progress, const uint32_t* draws,
                                               const size_t drawCount) {
  pokemon::PokemonRecord leader = leaderAtLevelFive();
  pokemon::PokemonState state = stateWithLeader(leader);
  state.encounterMisses = 5;
  SequenceRandom sequence{draws, drawCount};
  pokemon::RandomSource random{&sequence, SequenceRandom::next};
  const pokemon::CreditResult result =
      pokemon::applyCreditedMinutes(state, leader, 60, progress, pokemon::OwnedEvolutionNeeds{}, random);
  CHECK(result.status == pokemon::CreditStatus::Applied);
  CHECK(result.generatedEvent == pokemon::PendingEventKind::Encounter);
  CHECK(sequence.index == drawCount);
  return state.pending;
}

void progressBandsGateStagesAndEncounterLevels() {
  constexpr uint32_t earlyDraws[] = {1, 8, 4, 0};
  const pokemon::PendingEvent early = forceEncounterAtProgress(0, earlyDraws, std::size(earlyDraws));
  CHECK(early.speciesId == 4);
  CHECK(early.level == 6);
  CHECK(early.gender == pokemon::Gender::Female);

  constexpr uint32_t middleDraws[] = {1, 8, 7, 7};
  const pokemon::PendingEvent middle = forceEncounterAtProgress(50, middleDraws, std::size(middleDraws));
  CHECK(middle.speciesId == 2);
  CHECK(middle.level == 16);
  CHECK(middle.gender == pokemon::Gender::Male);

  constexpr uint32_t finalDraws[] = {1, 53, 10, 0};
  const pokemon::PendingEvent finalStage = forceEncounterAtProgress(75, finalDraws, std::size(finalDraws));
  CHECK(finalStage.speciesId == 3);
  CHECK(finalStage.level == 24);
  CHECK(finalStage.gender == pokemon::Gender::Female);
}

void itemRollAndPityPreferAnOwnedEvolutionNeed() {
  pokemon::PokemonRecord leader = leaderAtLevelFive();
  pokemon::PokemonState state = stateWithLeader(leader);
  constexpr uint32_t randomItemDraws[] = {1, 0};
  SequenceRandom randomItemSequence{randomItemDraws, std::size(randomItemDraws)};
  pokemon::RandomSource randomItem{&randomItemSequence, SequenceRandom::next};
  const pokemon::OwnedEvolutionNeeds thunderNeeded{1U << 2U};

  pokemon::CreditResult result =
      pokemon::applyCreditedMinutes(state, leader, 60, 25, thunderNeeded, randomItem);
  CHECK(result.status == pokemon::CreditStatus::Applied);
  CHECK(result.generatedEvent == pokemon::PendingEventKind::Item);
  CHECK(state.pending.kind == pokemon::PendingEventKind::Item);
  CHECK(state.pending.item == pokemon::EvolutionItem::ThunderStone);
  CHECK(state.itemCounts[2] == 1);
  CHECK(state.itemMisses == 0);
  CHECK(state.encounterMisses == 1);
  CHECK(state.dashboardNotice == pokemon::DashboardNotice::ItemFound);

  CHECK(pokemon::acknowledgeItem(state));
  CHECK(state.pending.kind == pokemon::PendingEventKind::None);
  CHECK(state.dashboardNotice == pokemon::DashboardNotice::None);

  state.itemMisses = 19;
  constexpr uint32_t pityDraws[] = {1};
  SequenceRandom pitySequence{pityDraws, std::size(pityDraws)};
  pokemon::RandomSource pityRandom{&pitySequence, SequenceRandom::next};
  result = pokemon::applyCreditedMinutes(state, leader, 60, 25, thunderNeeded, pityRandom);
  CHECK(result.generatedEvent == pokemon::PendingEventKind::Item);
  CHECK(state.pending.item == pokemon::EvolutionItem::ThunderStone);
  CHECK(state.itemCounts[2] == 2);
  CHECK(pitySequence.index == std::size(pityDraws));
}

void legendaryEligibilityAndMewOverrideRegularEncounters() {
  pokemon::PokemonRecord leader = leaderAtLevelFive();
  pokemon::PokemonState state = stateWithLeader(leader);
  state.encounterMisses = 5;
  state.readingMinuteRemainder = 59;
  state.lifetimeMinutes = 1199;
  constexpr uint32_t birdDraws[] = {0, 0, 0};
  SequenceRandom birdSequence{birdDraws, std::size(birdDraws)};
  pokemon::RandomSource birdRandom{&birdSequence, SequenceRandom::next};

  pokemon::CreditResult result =
      pokemon::applyCreditedMinutes(state, leader, 1, 75, pokemon::OwnedEvolutionNeeds{}, birdRandom);
  CHECK(result.generatedEvent == pokemon::PendingEventKind::Encounter);
  CHECK(state.pending.speciesId == 144);
  CHECK(state.pending.level == 14);
  CHECK(state.pending.gender == pokemon::Gender::Genderless);

  state.pending = {};
  state.dashboardNotice = pokemon::DashboardNotice::None;
  state.encounterMisses = 5;
  state.readingMinuteRemainder = 59;
  state.lifetimeMinutes = 2999;
  CHECK(pokemon::markSpecies(state.caughtSpecies, 144));
  CHECK(pokemon::markSpecies(state.seenSpecies, 144));
  CHECK(pokemon::markSpecies(state.caughtSpecies, 145));
  CHECK(pokemon::markSpecies(state.seenSpecies, 145));
  CHECK(pokemon::markSpecies(state.caughtSpecies, 146));
  CHECK(pokemon::markSpecies(state.seenSpecies, 146));
  constexpr uint32_t mewtwoDraws[] = {0, 0};
  SequenceRandom mewtwoSequence{mewtwoDraws, std::size(mewtwoDraws)};
  pokemon::RandomSource mewtwoRandom{&mewtwoSequence, SequenceRandom::next};
  result = pokemon::applyCreditedMinutes(state, leader, 1, 95, pokemon::OwnedEvolutionNeeds{}, mewtwoRandom);
  CHECK(result.generatedEvent == pokemon::PendingEventKind::Encounter);
  CHECK(state.pending.speciesId == 150);
  CHECK(state.pending.level == 18);

  state.pending = {};
  state.dashboardNotice = pokemon::DashboardNotice::None;
  state.encounterMisses = 5;
  state.readingMinuteRemainder = 59;
  for (uint16_t speciesId = 1; speciesId <= 150; ++speciesId) {
    CHECK(pokemon::markSpecies(state.seenSpecies, speciesId));
    CHECK(pokemon::markSpecies(state.caughtSpecies, speciesId));
  }
  constexpr uint32_t mewDraws[] = {0};
  SequenceRandom mewSequence{mewDraws, std::size(mewDraws)};
  pokemon::RandomSource mewRandom{&mewSequence, SequenceRandom::next};
  result = pokemon::applyCreditedMinutes(state, leader, 1, 95, pokemon::OwnedEvolutionNeeds{}, mewRandom);
  CHECK(result.generatedEvent == pokemon::PendingEventKind::Encounter);
  CHECK(state.pending.speciesId == 151);
  CHECK(state.pending.level == 18);
  CHECK(mewSequence.index == std::size(mewDraws));
}

pokemon::PokemonState stateWithEncounter(const pokemon::PokemonRecord& leader) {
  pokemon::PokemonState state = stateWithLeader(leader);
  state.pending.kind = pokemon::PendingEventKind::Encounter;
  state.pending.speciesId = 4;
  state.pending.level = 6;
  state.pending.gender = pokemon::Gender::Female;
  CHECK(pokemon::markSpecies(state.seenSpecies, 4));
  return state;
}

void catchCreatesOneAppendAndPassCreatesNone() {
  const pokemon::PokemonRecord leader = leaderAtLevelFive();
  pokemon::PokemonState state = stateWithEncounter(leader);
  pokemon::RecordMutation mutation{};
  mutation.requestedRecordId = 42;

  CHECK(pokemon::resolveEncounter(state, pokemon::EncounterChoice::Catch, "Ember", mutation));
  CHECK(mutation.kind == pokemon::RecordMutationKind::Append);
  CHECK(mutation.record.recordId == 42);
  CHECK(mutation.record.speciesId == 4);
  CHECK(mutation.record.totalXp == 68);
  CHECK(mutation.record.caughtLevel == 6);
  CHECK(mutation.record.gender == pokemon::Gender::Female);
  CHECK(mutation.record.origin == pokemon::Origin::Caught);
  CHECK(std::string_view(mutation.record.nickname.data()) == "Ember");
  CHECK(state.partyRecordIds[1] == 42);
  CHECK(pokemon::isSpeciesMarked(state.caughtSpecies, 4));
  CHECK(state.pending.kind == pokemon::PendingEventKind::None);

  state = stateWithEncounter(leader);
  const pokemon::PokemonState beforeInvalidNickname = state;
  mutation = {};
  mutation.requestedRecordId = 43;
  CHECK(!pokemon::resolveEncounter(
      state, pokemon::EncounterChoice::Catch, "123456789012345678901234567890123", mutation));
  CHECK(state == beforeInvalidNickname);
  CHECK(mutation.kind == pokemon::RecordMutationKind::None);

  state = stateWithEncounter(leader);
  mutation = {};
  CHECK(pokemon::resolveEncounter(state, pokemon::EncounterChoice::Pass, nullptr, mutation));
  CHECK(mutation.kind == pokemon::RecordMutationKind::None);
  CHECK(!pokemon::isSpeciesMarked(state.caughtSpecies, 4));
  CHECK(state.pending.kind == pokemon::PendingEventKind::None);
}

void fullPartyCatchLeavesTheNewRecordForPcStorage() {
  const pokemon::PokemonRecord leader = leaderAtLevelFive();
  pokemon::PokemonState state = stateWithEncounter(leader);
  state.partyRecordIds = {7, 8, 9, 10, 11, 12};
  const auto partyBefore = state.partyRecordIds;
  pokemon::RecordMutation mutation{};
  mutation.requestedRecordId = 99;
  CHECK(pokemon::resolveEncounter(state, pokemon::EncounterChoice::Catch, nullptr, mutation));
  CHECK(state.partyRecordIds == partyBefore);
  CHECK(mutation.kind == pokemon::RecordMutationKind::Append);
  CHECK(mutation.record.recordId == 99);
}

void levelEvolutionPromptsOnceAndCanBeConfirmedOrBlocked() {
  pokemon::PokemonRecord bulbasaur = leaderAtLevelFive();
  bulbasaur.speciesId = 1;
  bulbasaur.totalXp = 287;
  pokemon::PokemonState state = stateWithLeader(bulbasaur);
  pokemon::RandomSource random{nullptr, rejectUnexpectedDraw};

  pokemon::CreditResult result =
      pokemon::applyCreditedMinutes(state, bulbasaur, 31, 10, pokemon::OwnedEvolutionNeeds{}, random);
  CHECK(result.generatedEvent == pokemon::PendingEventKind::Evolution);
  CHECK(state.pending.kind == pokemon::PendingEventKind::Evolution);
  CHECK(state.pending.recordId == bulbasaur.recordId);
  CHECK(state.pending.speciesId == 2);
  CHECK(state.dashboardNotice == pokemon::DashboardNotice::WhatsThis);

  pokemon::RecordMutation mutation{};
  CHECK(pokemon::resolveEvolution(state, bulbasaur, pokemon::EvolutionChoice::Evolve, mutation));
  CHECK(mutation.kind == pokemon::RecordMutationKind::Replace);
  CHECK(mutation.record.speciesId == 2);
  CHECK(bulbasaur.speciesId == 2);
  CHECK(pokemon::isSpeciesMarked(state.caughtSpecies, 2));
  CHECK(state.pending.kind == pokemon::PendingEventKind::None);

  bulbasaur = leaderAtLevelFive();
  bulbasaur.speciesId = 1;
  bulbasaur.totalXp = 287;
  bulbasaur.flags = pokemon::recordFlag(pokemon::RecordFlag::EvolutionPromptsDisabled);
  state = stateWithLeader(bulbasaur);
  result = pokemon::applyCreditedMinutes(state, bulbasaur, 31, 10, pokemon::OwnedEvolutionNeeds{}, random);
  CHECK(result.generatedEvent == pokemon::PendingEventKind::None);
  CHECK(state.pending.kind == pokemon::PendingEventKind::None);
}

void everyItemEvolutionConsumesExactlyOneItem() {
  struct ItemEvolutionCase {
    uint16_t source;
    uint16_t target;
    pokemon::EvolutionItem item;
  };
  constexpr ItemEvolutionCase cases[] = {
      {25, 26, pokemon::EvolutionItem::ThunderStone}, {30, 31, pokemon::EvolutionItem::MoonStone},
      {33, 34, pokemon::EvolutionItem::MoonStone},    {35, 36, pokemon::EvolutionItem::MoonStone},
      {37, 38, pokemon::EvolutionItem::FireStone},   {39, 40, pokemon::EvolutionItem::MoonStone},
      {44, 45, pokemon::EvolutionItem::LeafStone},   {58, 59, pokemon::EvolutionItem::FireStone},
      {61, 62, pokemon::EvolutionItem::WaterStone},  {64, 65, pokemon::EvolutionItem::LinkCable},
      {67, 68, pokemon::EvolutionItem::LinkCable},   {70, 71, pokemon::EvolutionItem::LeafStone},
      {75, 76, pokemon::EvolutionItem::LinkCable},   {90, 91, pokemon::EvolutionItem::WaterStone},
      {93, 94, pokemon::EvolutionItem::LinkCable},   {102, 103, pokemon::EvolutionItem::LeafStone},
      {120, 121, pokemon::EvolutionItem::WaterStone},{133, 134, pokemon::EvolutionItem::WaterStone},
      {133, 135, pokemon::EvolutionItem::ThunderStone},{133, 136, pokemon::EvolutionItem::FireStone},
  };

  for (const ItemEvolutionCase& itemCase : cases) {
    pokemon::PokemonRecord record = leaderAtLevelFive();
    record.speciesId = itemCase.source;
    const uint8_t genderRate = pokemon::speciesData(itemCase.source)->genderRate;
    record.gender = genderRate == 255 ? pokemon::Gender::Genderless
                    : genderRate == 0 ? pokemon::Gender::Male
                                      : pokemon::Gender::Female;
    pokemon::PokemonState state{};
    const size_t itemIndex = static_cast<size_t>(itemCase.item) - 1U;
    state.itemCounts[itemIndex] = 1;
    pokemon::RecordMutation mutation{};
    CHECK(pokemon::useEvolutionItem(state, record, itemCase.item, mutation));
    CHECK(record.speciesId == itemCase.target);
    CHECK(state.itemCounts[itemIndex] == 0);
    CHECK(mutation.kind == pokemon::RecordMutationKind::Replace);
    CHECK(mutation.record.speciesId == itemCase.target);
    CHECK(pokemon::isSpeciesMarked(state.caughtSpecies, itemCase.target));
  }
}

void rejectedInputsAndCancelledEvolutionDoNotPartiallyMutate() {
  pokemon::PokemonRecord leader = leaderAtLevelFive();
  pokemon::PokemonState state = stateWithLeader(leader);
  state.readingMinuteRemainder = 59;
  const pokemon::PokemonRecord leaderBefore = leader;
  const pokemon::PokemonState stateBefore = state;
  const pokemon::RandomSource unavailableRandom{};
  const pokemon::CreditResult rejected =
      pokemon::applyCreditedMinutes(state, leader, 1, 10, pokemon::OwnedEvolutionNeeds{}, unavailableRandom);
  CHECK(rejected.status == pokemon::CreditStatus::Rejected);
  CHECK(leader == leaderBefore);
  CHECK(state == stateBefore);

  state = stateWithEncounter(leader);
  const pokemon::PokemonState encounterBefore = state;
  pokemon::RecordMutation mutation{};
  CHECK(!pokemon::resolveEncounter(state, static_cast<pokemon::EncounterChoice>(99), nullptr, mutation));
  CHECK(state == encounterBefore);
  CHECK(mutation.kind == pokemon::RecordMutationKind::None);

  leader.speciesId = 1;
  leader.totalXp = pokemon::xpRequired(16);
  state = stateWithLeader(leader);
  state.pending.kind = pokemon::PendingEventKind::Evolution;
  state.pending.recordId = leader.recordId;
  state.pending.speciesId = 2;
  state.dashboardNotice = pokemon::DashboardNotice::WhatsThis;
  CHECK(pokemon::resolveEvolution(state, leader, pokemon::EvolutionChoice::Cancel, mutation));
  CHECK(leader.speciesId == 1);
  CHECK(state.pending.kind == pokemon::PendingEventKind::None);
  CHECK(mutation.kind == pokemon::RecordMutationKind::None);
}

}  // namespace

int main() {
  creditedMinutesAdvanceLeaderAndReadingCounters();
  creditClampsAndRejectsInvalidCallsWithoutMutation();
  sixthEncounterCheckIsForcedAndCreatesOnlyOneEvent();
  pendingEventBlocksRollsButStillAdvancesPity();
  progressBandsGateStagesAndEncounterLevels();
  itemRollAndPityPreferAnOwnedEvolutionNeed();
  legendaryEligibilityAndMewOverrideRegularEncounters();
  catchCreatesOneAppendAndPassCreatesNone();
  fullPartyCatchLeavesTheNewRecordForPcStorage();
  levelEvolutionPromptsOnceAndCanBeConfirmedOrBlocked();
  everyItemEvolutionConsumesExactlyOneItem();
  rejectedInputsAndCancelledEvolutionDoNotPartiallyMutate();
  return failures == 0 ? 0 : 1;
}
