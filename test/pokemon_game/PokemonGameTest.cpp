#include <cstdint>
#include <cstdio>
#include <iterator>
#include <string_view>

#include "Pokemon/PokemonGame.h"
#include "Pokemon/PokemonSpecies.h"

namespace {

int failures = 0;

#define CHECK(condition)                                                                \
  do {                                                                                  \
    if (!(condition)) {                                                                 \
      std::fprintf(stderr, "%s:%d check failed: %s\n", __FILE__, __LINE__, #condition); \
      ++failures;                                                                       \
    }                                                                                   \
  } while (false)

uint32_t chooseFirst(void*, uint32_t) { return 0; }

uint32_t chooseLast(void*, const uint32_t upperExclusive) { return upperExclusive - 1U; }

struct SequenceRandom {
  const uint32_t* values;
  size_t count;
  size_t index = 0;

  static uint32_t next(void* context, const uint32_t upperExclusive) {
    auto& sequence = *static_cast<SequenceRandom*>(context);
    if (sequence.index >= sequence.count) {
      std::fprintf(stderr, "unexpected random draw: upper=%u after %zu values\n", upperExclusive, sequence.count);
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

void collectionActionsExcludeOperationsThatCannotSucceed() {
  const pokemon::CollectionActionSet loneParty = pokemon::collectionActions(true, 1);
  CHECK(loneParty.count == 3);
  CHECK(loneParty.items[0] == pokemon::CollectionAction::Summary);
  CHECK(loneParty.items[1] == pokemon::CollectionAction::Rename);
  CHECK(loneParty.items[2] == pokemon::CollectionAction::EvolutionPrompts);

  const pokemon::CollectionActionSet reorderableParty = pokemon::collectionActions(true, 2);
  CHECK(reorderableParty.count == 5);
  CHECK(reorderableParty.items[0] == pokemon::CollectionAction::Summary);
  CHECK(reorderableParty.items[1] == pokemon::CollectionAction::Move);
  CHECK(reorderableParty.items[2] == pokemon::CollectionAction::Deposit);
  CHECK(reorderableParty.items[3] == pokemon::CollectionAction::Rename);
  CHECK(reorderableParty.items[4] == pokemon::CollectionAction::EvolutionPrompts);

  const pokemon::CollectionActionSet fullPartyPc = pokemon::collectionActions(false, pokemon::PARTY_SIZE);
  CHECK(fullPartyPc.count == 3);
  CHECK(fullPartyPc.items[0] == pokemon::CollectionAction::Summary);
  CHECK(fullPartyPc.items[1] == pokemon::CollectionAction::Rename);
  CHECK(fullPartyPc.items[2] == pokemon::CollectionAction::EvolutionPrompts);

  const pokemon::CollectionActionSet pcWithRoom = pokemon::collectionActions(false, pokemon::PARTY_SIZE - 1U);
  CHECK(pcWithRoom.count == 4);
  CHECK(pcWithRoom.items[0] == pokemon::CollectionAction::Summary);
  CHECK(pcWithRoom.items[1] == pokemon::CollectionAction::Withdraw);
  CHECK(pcWithRoom.items[2] == pokemon::CollectionAction::Rename);
  CHECK(pcWithRoom.items[3] == pokemon::CollectionAction::EvolutionPrompts);
}

pokemon::Gender validGenderForSpecies(const uint16_t speciesId) {
  const uint8_t genderRate = pokemon::speciesData(speciesId)->genderRate;
  if (genderRate == 255) return pokemon::Gender::Genderless;
  if (genderRate == 0) return pokemon::Gender::Male;
  return pokemon::Gender::Female;
}

void creditedMinutesAdvanceLeaderAndReadingCounters() {
  pokemon::PokemonRecord leader = leaderAtLevelFive();
  pokemon::PokemonState state = stateWithLeader(leader);
  pokemon::RandomSource random{nullptr, chooseLast};

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
  CHECK(state.encounterMisses == 1);
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
  const pokemon::CreditResult rejected =
      pokemon::applyCreditedMinutes(rejectedState, rejectedLeader, 10, 20, pokemon::OwnedEvolutionNeeds{}, random);
  CHECK(rejected.status == pokemon::CreditStatus::Rejected);
  CHECK(rejectedLeader == leaderBefore);
  CHECK(rejectedState == stateBefore);

  rejectedState = stateWithLeader(rejectedLeader);
  const pokemon::CreditResult noChange =
      pokemon::applyCreditedMinutes(rejectedState, rejectedLeader, 0, 20, pokemon::OwnedEvolutionNeeds{}, random);
  CHECK(noChange.status == pokemon::CreditStatus::NoChange);
  CHECK(rejectedLeader == leaderBefore);
  CHECK(rejectedState == stateWithLeader(rejectedLeader));
}

void fourthEncounterCheckIsForcedAndCreatesOnlyOneEvent() {
  pokemon::PokemonRecord leader = leaderAtLevelFive();
  pokemon::PokemonState state = stateWithLeader(leader);
  constexpr uint32_t draws[] = {
      2, 2, 2,     // Three encounter misses.
      1,           // Hourly item miss.
      1, 0, 0, 0,  // Legendary miss, first weighted species, minimum level, female roll.
  };
  SequenceRandom sequence{draws, std::size(draws)};
  pokemon::RandomSource random{&sequence, SequenceRandom::next};

  for (uint8_t check = 1; check <= 3; ++check) {
    const pokemon::CreditResult result =
        pokemon::applyCreditedMinutes(state, leader, 15, 10, pokemon::OwnedEvolutionNeeds{}, random);
    CHECK(result.status == pokemon::CreditStatus::Applied);
    CHECK(result.generatedEvent == pokemon::PendingEventKind::None);
    CHECK(state.encounterMisses == check);
    CHECK(state.itemMisses == 0);
  }

  const pokemon::CreditResult forced =
      pokemon::applyCreditedMinutes(state, leader, 15, 10, pokemon::OwnedEvolutionNeeds{}, random);
  CHECK(forced.status == pokemon::CreditStatus::Applied);
  CHECK(forced.generatedEvent == pokemon::PendingEventKind::Encounter);
  CHECK(state.pendingEvents[0].kind == pokemon::PendingEventKind::Encounter);
  CHECK(state.pendingEvents[0].speciesId == 1);
  CHECK(state.pendingEvents[0].level == 2);
  CHECK(state.pendingEvents[0].gender == pokemon::Gender::Female);
  CHECK(state.encounterMisses == 0);
  CHECK(state.itemMisses == 1);
  CHECK(state.readingMinuteRemainder == 0);
  CHECK(state.lifetimeMinutes == 60);
  CHECK(sequence.index == std::size(draws));
}

void fifteenMinuteCheckUsesFortyPercentEncounterChance() {
  pokemon::PokemonRecord leader = leaderAtLevelFive();
  pokemon::PokemonState state = stateWithLeader(leader);
  constexpr uint32_t draws[] = {
      1,  // Second successful value in a 2-in-5 encounter roll.
      1,  // Legendary miss.
      0,  // First weighted species.
      0,  // Minimum encounter level.
      0,  // Female gender roll.
  };
  SequenceRandom sequence{draws, std::size(draws)};
  pokemon::RandomSource random{&sequence, SequenceRandom::next};

  const pokemon::CreditResult result =
      pokemon::applyCreditedMinutes(state, leader, 15, 10, pokemon::OwnedEvolutionNeeds{}, random);

  CHECK(result.status == pokemon::CreditStatus::Applied);
  CHECK(result.generatedEvent == pokemon::PendingEventKind::Encounter);
  CHECK(state.pendingEvents[0].kind == pokemon::PendingEventKind::Encounter);
  CHECK(state.readingMinuteRemainder == 15);
  CHECK(sequence.index == std::size(draws));
}

void fullQueueCreditsReadingAndPrimesGuaranteesWithoutRolling() {
  pokemon::PokemonRecord leader = leaderAtLevelFive();
  pokemon::PokemonState state = stateWithLeader(leader);
  state.pendingEvents[0] = {
      0, 81, 24, pokemon::Gender::Genderless, pokemon::EvolutionItem::None, pokemon::PendingEventKind::Encounter};
  state.pendingEvents[1] = {
      0, 0, 0, pokemon::Gender::Unknown, pokemon::EvolutionItem::MoonStone, pokemon::PendingEventKind::Item};
  state.pendingEvents[2] = {leader.recordId,
                            26,
                            0,
                            pokemon::Gender::Unknown,
                            pokemon::EvolutionItem::None,
                            pokemon::PendingEventKind::Evolution};
  state.readingMinuteRemainder = 59;
  const auto pendingBefore = state.pendingEvents;
  const uint32_t xpBefore = leader.totalXp;
  pokemon::RandomSource random{nullptr, rejectUnexpectedDraw};

  const pokemon::CreditResult result =
      pokemon::applyCreditedMinutes(state, leader, 1, 50, pokemon::OwnedEvolutionNeeds{}, random);

  CHECK(result.status == pokemon::CreditStatus::Applied);
  CHECK(result.generatedEvent == pokemon::PendingEventKind::None);
  CHECK(state.pendingEvents == pendingBefore);
  CHECK(state.encounterMisses == 3);
  CHECK(state.itemMisses == 19);
  CHECK(state.readingMinuteRemainder == 0);
  CHECK(state.lifetimeMinutes == 1);
  CHECK(leader.totalXp == xpBefore + 1U);
}

void oneBoundaryQueuesItemEncounterAndEvolutionInOrder() {
  pokemon::PokemonRecord leader = leaderAtLevelFive();
  leader.speciesId = 1;
  leader.totalXp = pokemon::xpRequired(16) - 1U;
  pokemon::PokemonState state = stateWithLeader(leader);
  state.readingMinuteRemainder = 59;
  state.encounterMisses = 3;
  state.itemMisses = 19;
  constexpr uint32_t draws[] = {
      0,           // Select the first forced item.
      1, 0, 0, 0,  // Legendary miss, first species, minimum level, female roll.
  };
  SequenceRandom sequence{draws, std::size(draws)};
  pokemon::RandomSource random{&sequence, SequenceRandom::next};

  const pokemon::CreditResult result =
      pokemon::applyCreditedMinutes(state, leader, 1, 25, pokemon::OwnedEvolutionNeeds{}, random);

  CHECK(result.status == pokemon::CreditStatus::Applied);
  CHECK(result.generatedEvent == pokemon::PendingEventKind::Evolution);
  CHECK(pokemon::pendingEventCount(state) == 3);
  CHECK(state.pendingEvents[0].kind == pokemon::PendingEventKind::Item);
  CHECK(state.pendingEvents[1].kind == pokemon::PendingEventKind::Encounter);
  CHECK(state.pendingEvents[2].kind == pokemon::PendingEventKind::Evolution);
  CHECK(state.pendingEvents[2].recordId == leader.recordId);
  CHECK(state.pendingEvents[2].speciesId == 2);
  CHECK(state.dashboardNotice == pokemon::DashboardNotice::ItemFound);
  CHECK(sequence.index == std::size(draws));
}

void resolvingEventsPopsOnlyTheFrontAndRefreshesTheNotice() {
  pokemon::PokemonRecord leader = leaderAtLevelFive();
  leader.speciesId = 1;
  leader.totalXp = pokemon::xpRequired(16);
  pokemon::PokemonState state = stateWithLeader(leader);
  state.pendingEvents[0] = {
      0, 0, 0, pokemon::Gender::Unknown, pokemon::EvolutionItem::MoonStone, pokemon::PendingEventKind::Item};
  state.pendingEvents[1] = {
      0, 4, 6, pokemon::Gender::Female, pokemon::EvolutionItem::None, pokemon::PendingEventKind::Encounter};
  state.pendingEvents[2] = {leader.recordId,
                            2,
                            0,
                            pokemon::Gender::Unknown,
                            pokemon::EvolutionItem::None,
                            pokemon::PendingEventKind::Evolution};
  state.dashboardNotice = pokemon::DashboardNotice::ItemFound;

  CHECK(pokemon::acknowledgeItem(state, leader));
  CHECK(state.pendingEvents[0].kind == pokemon::PendingEventKind::Encounter);
  CHECK(state.dashboardNotice == pokemon::DashboardNotice::NewPokemon);

  pokemon::RecordMutation mutation{};
  CHECK(pokemon::resolveEncounter(state, leader, pokemon::EncounterChoice::Pass, nullptr, mutation));
  CHECK(state.pendingEvents[0].kind == pokemon::PendingEventKind::Evolution);
  CHECK(state.dashboardNotice == pokemon::DashboardNotice::WhatsThis);

  CHECK(pokemon::resolveEvolution(state, leader, pokemon::EvolutionChoice::Cancel, mutation));
  CHECK(pokemon::pendingEventCount(state) == 0);
  CHECK(state.dashboardNotice == pokemon::DashboardNotice::None);
}

void encounterDueWithLevelGainQueuesEncounterBeforeEvolution() {
  pokemon::PokemonRecord bulbasaur = leaderAtLevelFive();
  bulbasaur.speciesId = 1;
  bulbasaur.totalXp = pokemon::xpRequired(16) - 1U;
  pokemon::PokemonState state = stateWithLeader(bulbasaur);
  state.readingMinuteRemainder = 59;
  state.encounterMisses = 3;
  constexpr uint32_t draws[] = {1, 1, 0, 0, 0};
  SequenceRandom sequence{draws, std::size(draws)};
  pokemon::RandomSource random{&sequence, SequenceRandom::next};

  const pokemon::CreditResult result =
      pokemon::applyCreditedMinutes(state, bulbasaur, 1, 0, pokemon::OwnedEvolutionNeeds{}, random);

  CHECK(result.generatedEvent == pokemon::PendingEventKind::Evolution);
  CHECK(state.pendingEvents[0].kind == pokemon::PendingEventKind::Encounter);
  CHECK(state.pendingEvents[0].speciesId == 1);
  CHECK(state.pendingEvents[1].kind == pokemon::PendingEventKind::Evolution);
  CHECK(state.pendingEvents[1].speciesId == 2);
  CHECK(state.itemMisses == 1);
}

void forcedItemWithLevelGainQueuesItemBeforeEvolution() {
  pokemon::PokemonRecord bulbasaur = leaderAtLevelFive();
  bulbasaur.speciesId = 1;
  bulbasaur.totalXp = pokemon::xpRequired(16) - 1U;
  pokemon::PokemonState state = stateWithLeader(bulbasaur);
  state.readingMinuteRemainder = 59;
  state.itemMisses = 19;
  constexpr uint32_t draws[] = {
      0,  // Select the first forced item.
      2,  // Miss the encounter due at the hourly boundary.
  };
  SequenceRandom sequence{draws, std::size(draws)};
  pokemon::RandomSource random{&sequence, SequenceRandom::next};

  const pokemon::CreditResult result =
      pokemon::applyCreditedMinutes(state, bulbasaur, 1, 0, pokemon::OwnedEvolutionNeeds{}, random);

  CHECK(result.generatedEvent == pokemon::PendingEventKind::Evolution);
  CHECK(state.pendingEvents[0].kind == pokemon::PendingEventKind::Item);
  CHECK(state.pendingEvents[1].kind == pokemon::PendingEventKind::Evolution);
  CHECK(state.itemCounts[0] == 1);
  CHECK(sequence.index == std::size(draws));
}

void multiHourCreditUsesLifetimeAtEachHourlyBoundary() {
  pokemon::PokemonRecord leader = leaderAtLevelFive();
  pokemon::PokemonState state = stateWithLeader(leader);
  state.lifetimeMinutes = 1079;
  state.readingMinuteRemainder = 59;
  state.encounterMisses = 3;
  constexpr uint32_t draws[] = {
      1, 1, 0, 0, 0,  // 1080 minutes: item miss, then a regular encounter.
      2, 2, 2,        // Three encounter misses.
      1, 1, 0, 0, 0,  // 1140 minutes: item miss, then a regular encounter.
      2, 2, 2,        // Three more encounter misses.
      1, 0, 0, 0,     // 1200 minutes: item miss, Articuno, minimum level.
  };
  SequenceRandom sequence{draws, std::size(draws)};
  pokemon::RandomSource random{&sequence, SequenceRandom::next};

  const pokemon::CreditResult result =
      pokemon::applyCreditedMinutes(state, leader, 121, 75, pokemon::OwnedEvolutionNeeds{}, random);

  CHECK(result.status == pokemon::CreditStatus::Applied);
  CHECK(result.generatedEvent == pokemon::PendingEventKind::Encounter);
  CHECK(state.pendingEvents[0].speciesId == 1);
  CHECK(state.pendingEvents[0].level == 14);
  CHECK(state.pendingEvents[2].speciesId == 144);
  CHECK(state.pendingEvents[2].level == 14);
  CHECK(state.lifetimeMinutes == 1200);
  CHECK(sequence.index == std::size(draws));
}

pokemon::PendingEvent forceEncounterAtProgress(const uint8_t progress, const uint32_t* draws, const size_t drawCount) {
  pokemon::PokemonRecord leader = leaderAtLevelFive();
  pokemon::PokemonState state = stateWithLeader(leader);
  state.encounterMisses = 3;
  SequenceRandom sequence{draws, drawCount};
  pokemon::RandomSource random{&sequence, SequenceRandom::next};
  const pokemon::CreditResult result =
      pokemon::applyCreditedMinutes(state, leader, 15, progress, pokemon::OwnedEvolutionNeeds{}, random);
  CHECK(result.status == pokemon::CreditStatus::Applied);
  CHECK(result.generatedEvent == pokemon::PendingEventKind::Encounter);
  CHECK(sequence.index == drawCount);
  return state.pendingEvents[0];
}

void progressBandsGateStagesAndEncounterLevels() {
  constexpr uint32_t earlyDraws[] = {1, 1, 4, 0};
  const pokemon::PendingEvent early = forceEncounterAtProgress(0, earlyDraws, std::size(earlyDraws));
  CHECK(early.speciesId == 4);
  CHECK(early.level == 6);
  CHECK(early.gender == pokemon::Gender::Female);

  constexpr uint32_t middleDraws[] = {1, 1, 7, 7};
  const pokemon::PendingEvent middle = forceEncounterAtProgress(50, middleDraws, std::size(middleDraws));
  CHECK(middle.speciesId == 2);
  CHECK(middle.level == 16);
  CHECK(middle.gender == pokemon::Gender::Male);

  constexpr uint32_t finalDraws[] = {1, 2, 10, 0};
  const pokemon::PendingEvent finalStage = forceEncounterAtProgress(75, finalDraws, std::size(finalDraws));
  CHECK(finalStage.speciesId == 3);
  CHECK(finalStage.level == 24);
  CHECK(finalStage.gender == pokemon::Gender::Female);
}

void everyEligibleRegularEncounterWeightIntervalSelectsItsSpecies() {
  constexpr uint8_t progressBands[] = {0, 25, 50, 75, 95};
  for (const uint8_t progress : progressBands) {
    uint32_t cumulativeWeight = 0;
    for (uint16_t speciesId = 1; speciesId <= pokemon::KANTO_SPECIES_COUNT; ++speciesId) {
      const pokemon::SpeciesData& species = *pokemon::speciesData(speciesId);
      const bool eligible = species.acquisition == pokemon::Acquisition::Wild &&
                            (species.stage != pokemon::EvolutionStage::Middle || progress >= 50) &&
                            (species.stage != pokemon::EvolutionStage::Final || progress >= 75);
      if (!eligible) continue;

      const uint32_t draws[] = {1, cumulativeWeight, 0, 0};
      const size_t drawCount = species.genderRate == 0 || species.genderRate == 8 || species.genderRate == 255 ? 3 : 4;
      const pokemon::PendingEvent event = forceEncounterAtProgress(progress, draws, drawCount);
      CHECK(event.speciesId == speciesId);
      uint8_t expectedMinimumLevel = 2;
      if (progress >= 95)
        expectedMinimumLevel = 18;
      else if (progress >= 75)
        expectedMinimumLevel = 14;
      else if (progress >= 50)
        expectedMinimumLevel = 9;
      else if (progress >= 25)
        expectedMinimumLevel = 5;
      CHECK(event.level == expectedMinimumLevel);
      const bool speciallyRare = speciesId == 1 || speciesId == 4 || speciesId == 7 || speciesId == 25;
      uint8_t weight = 1;
      if (!speciallyRare && species.captureRate >= 200)
        weight = 4;
      else if (!speciallyRare && species.captureRate >= 120)
        weight = 3;
      else if (!speciallyRare && species.captureRate >= 60)
        weight = 2;
      cumulativeWeight += weight;
    }
  }
}

void itemRollAndPityPreferAnOwnedEvolutionNeed() {
  pokemon::PokemonRecord leader = leaderAtLevelFive();
  pokemon::PokemonState state = stateWithLeader(leader);
  state.readingMinuteRemainder = 59;
  constexpr uint32_t randomItemDraws[] = {
      0,  // Trigger the hourly item.
      2,  // Miss the encounter due at the same boundary.
  };
  SequenceRandom randomItemSequence{randomItemDraws, std::size(randomItemDraws)};
  pokemon::RandomSource randomItem{&randomItemSequence, SequenceRandom::next};
  const pokemon::OwnedEvolutionNeeds thunderNeeded{1U << 2U};

  pokemon::CreditResult result = pokemon::applyCreditedMinutes(state, leader, 1, 25, thunderNeeded, randomItem);
  CHECK(result.status == pokemon::CreditStatus::Applied);
  CHECK(result.generatedEvent == pokemon::PendingEventKind::Item);
  CHECK(state.pendingEvents[0].kind == pokemon::PendingEventKind::Item);
  CHECK(state.pendingEvents[0].item == pokemon::EvolutionItem::ThunderStone);
  CHECK(state.itemCounts[2] == 1);
  CHECK(state.itemMisses == 0);
  CHECK(state.encounterMisses == 1);
  CHECK(state.dashboardNotice == pokemon::DashboardNotice::ItemFound);

  CHECK(pokemon::acknowledgeItem(state, leader));
  CHECK(state.pendingEvents[0].kind == pokemon::PendingEventKind::None);
  CHECK(state.dashboardNotice == pokemon::DashboardNotice::None);

  state.itemMisses = 19;
  state.readingMinuteRemainder = 59;
  pokemon::RandomSource pityRandom{nullptr, chooseLast};
  result = pokemon::applyCreditedMinutes(state, leader, 1, 25, thunderNeeded, pityRandom);
  CHECK(result.generatedEvent == pokemon::PendingEventKind::Item);
  CHECK(state.pendingEvents[0].item == pokemon::EvolutionItem::ThunderStone);
  CHECK(state.itemCounts[2] == 2);
}

void saturatedItemsDoNotRejectReadingCredit() {
  pokemon::PokemonRecord leader = leaderAtLevelFive();
  pokemon::PokemonState state = stateWithLeader(leader);
  state.itemCounts.fill(UINT16_MAX);
  state.itemMisses = 19;
  state.readingMinuteRemainder = 59;
  constexpr uint32_t draws[] = {2};
  SequenceRandom sequence{draws, std::size(draws)};
  pokemon::RandomSource random{&sequence, SequenceRandom::next};

  const pokemon::CreditResult result =
      pokemon::applyCreditedMinutes(state, leader, 1, 25, pokemon::OwnedEvolutionNeeds{}, random);

  CHECK(result.status == pokemon::CreditStatus::Applied);
  CHECK(result.generatedEvent == pokemon::PendingEventKind::None);
  CHECK(leader.totalXp == 53);
  CHECK(state.lifetimeMinutes == 1);
  CHECK(state.itemMisses == 0);
  CHECK(state.pendingEvents[0].kind == pokemon::PendingEventKind::None);
  CHECK(sequence.index == 1);
  for (const uint16_t count : state.itemCounts) CHECK(count == UINT16_MAX);

  state = stateWithLeader(leader);
  state.itemCounts[2] = UINT16_MAX;
  state.itemMisses = 19;
  state.readingMinuteRemainder = 59;
  constexpr uint32_t fallbackDraws[] = {
      0,  // Select Moon Stone from the non-saturated fallback items.
      2,  // Miss the encounter due at the same hourly boundary.
  };
  SequenceRandom fallbackSequence{fallbackDraws, std::size(fallbackDraws)};
  pokemon::RandomSource fallbackRandom{&fallbackSequence, SequenceRandom::next};
  const pokemon::OwnedEvolutionNeeds thunderNeeded{1U << 2U};
  const pokemon::CreditResult fallback =
      pokemon::applyCreditedMinutes(state, leader, 1, 25, thunderNeeded, fallbackRandom);
  CHECK(fallback.status == pokemon::CreditStatus::Applied);
  CHECK(state.pendingEvents[0].item == pokemon::EvolutionItem::MoonStone);
  CHECK(state.itemCounts[0] == 1);
  CHECK(state.itemCounts[2] == UINT16_MAX);
}

void legendaryEligibilityAndMewOverrideRegularEncounters() {
  pokemon::PokemonRecord leader = leaderAtLevelFive();
  pokemon::PokemonState state = stateWithLeader(leader);
  state.encounterMisses = 3;
  state.readingMinuteRemainder = 59;
  state.lifetimeMinutes = 1199;
  constexpr uint32_t birdDraws[] = {1, 0, 0, 0};
  SequenceRandom birdSequence{birdDraws, std::size(birdDraws)};
  pokemon::RandomSource birdRandom{&birdSequence, SequenceRandom::next};

  pokemon::CreditResult result =
      pokemon::applyCreditedMinutes(state, leader, 1, 75, pokemon::OwnedEvolutionNeeds{}, birdRandom);
  CHECK(result.generatedEvent == pokemon::PendingEventKind::Encounter);
  CHECK(state.pendingEvents[0].speciesId == 144);
  CHECK(state.pendingEvents[0].level == 14);
  CHECK(state.pendingEvents[0].gender == pokemon::Gender::Genderless);

  state.pendingEvents[0] = {};
  state.dashboardNotice = pokemon::DashboardNotice::None;
  state.encounterMisses = 3;
  state.readingMinuteRemainder = 59;
  state.lifetimeMinutes = 2999;
  CHECK(pokemon::markSpecies(state.caughtSpecies, 144));
  CHECK(pokemon::markSpecies(state.seenSpecies, 144));
  CHECK(pokemon::markSpecies(state.caughtSpecies, 145));
  CHECK(pokemon::markSpecies(state.seenSpecies, 145));
  CHECK(pokemon::markSpecies(state.caughtSpecies, 146));
  CHECK(pokemon::markSpecies(state.seenSpecies, 146));
  constexpr uint32_t mewtwoDraws[] = {1, 0, 0};
  SequenceRandom mewtwoSequence{mewtwoDraws, std::size(mewtwoDraws)};
  pokemon::RandomSource mewtwoRandom{&mewtwoSequence, SequenceRandom::next};
  result = pokemon::applyCreditedMinutes(state, leader, 1, 95, pokemon::OwnedEvolutionNeeds{}, mewtwoRandom);
  CHECK(result.generatedEvent == pokemon::PendingEventKind::Encounter);
  CHECK(state.pendingEvents[0].speciesId == 150);
  CHECK(state.pendingEvents[0].level == 18);

  state.pendingEvents[0] = {};
  state.dashboardNotice = pokemon::DashboardNotice::None;
  state.encounterMisses = 3;
  state.readingMinuteRemainder = 59;
  CHECK(pokemon::markSpecies(state.caughtSpecies, 150));
  CHECK(pokemon::markSpecies(state.seenSpecies, 150));
  constexpr uint32_t duplicateDraws[] = {1, 0, 3, 0};
  SequenceRandom duplicateSequence{duplicateDraws, std::size(duplicateDraws)};
  pokemon::RandomSource duplicateRandom{&duplicateSequence, SequenceRandom::next};
  result = pokemon::applyCreditedMinutes(state, leader, 1, 95, pokemon::OwnedEvolutionNeeds{}, duplicateRandom);
  CHECK(result.generatedEvent == pokemon::PendingEventKind::Encounter);
  CHECK(state.pendingEvents[0].speciesId == 150);
  CHECK(duplicateSequence.index == std::size(duplicateDraws));

  state.pendingEvents[0] = {};
  state.dashboardNotice = pokemon::DashboardNotice::None;
  state.encounterMisses = 3;
  state.readingMinuteRemainder = 59;
  for (uint16_t speciesId = 1; speciesId <= 150; ++speciesId) {
    CHECK(pokemon::markSpecies(state.seenSpecies, speciesId));
    CHECK(pokemon::markSpecies(state.caughtSpecies, speciesId));
  }
  constexpr uint32_t mewDraws[] = {1, 0};
  SequenceRandom mewSequence{mewDraws, std::size(mewDraws)};
  pokemon::RandomSource mewRandom{&mewSequence, SequenceRandom::next};
  result = pokemon::applyCreditedMinutes(state, leader, 1, 95, pokemon::OwnedEvolutionNeeds{}, mewRandom);
  CHECK(result.generatedEvent == pokemon::PendingEventKind::Encounter);
  CHECK(state.pendingEvents[0].speciesId == 151);
  CHECK(state.pendingEvents[0].level == 18);
  CHECK(mewSequence.index == std::size(mewDraws));
}

pokemon::PokemonState stateWithEncounter(const pokemon::PokemonRecord& leader) {
  pokemon::PokemonState state = stateWithLeader(leader);
  state.pendingEvents[0].kind = pokemon::PendingEventKind::Encounter;
  state.pendingEvents[0].speciesId = 4;
  state.pendingEvents[0].level = 6;
  state.pendingEvents[0].gender = pokemon::Gender::Female;
  CHECK(pokemon::markSpecies(state.seenSpecies, 4));
  return state;
}

void catchCreatesOneAppendAndPassCreatesNone() {
  const pokemon::PokemonRecord leader = leaderAtLevelFive();
  pokemon::PokemonState state = stateWithEncounter(leader);
  pokemon::RecordMutation mutation{};
  mutation.requestedRecordId = 42;

  CHECK(pokemon::resolveEncounter(state, leader, pokemon::EncounterChoice::Catch, "Ember", mutation));
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
  CHECK(state.pendingEvents[0].kind == pokemon::PendingEventKind::None);

  state = stateWithEncounter(leader);
  const pokemon::PokemonState beforeInvalidNickname = state;
  mutation = {};
  mutation.requestedRecordId = 43;
  CHECK(!pokemon::resolveEncounter(state, leader, pokemon::EncounterChoice::Catch, "123456789012345678901234567890123",
                                   mutation));
  CHECK(state == beforeInvalidNickname);
  CHECK(mutation.kind == pokemon::RecordMutationKind::None);

  state = stateWithEncounter(leader);
  mutation = {};
  CHECK(pokemon::resolveEncounter(state, leader, pokemon::EncounterChoice::Pass, nullptr, mutation));
  CHECK(mutation.kind == pokemon::RecordMutationKind::None);
  CHECK(!pokemon::isSpeciesMarked(state.caughtSpecies, 4));
  CHECK(state.pendingEvents[0].kind == pokemon::PendingEventKind::None);
}

void fullPartyCatchLeavesTheNewRecordForPcStorage() {
  const pokemon::PokemonRecord leader = leaderAtLevelFive();
  pokemon::PokemonState state = stateWithEncounter(leader);
  state.partyRecordIds = {7, 8, 9, 10, 11, 12};
  const auto partyBefore = state.partyRecordIds;
  pokemon::RecordMutation mutation{};
  mutation.requestedRecordId = 99;
  CHECK(pokemon::resolveEncounter(state, leader, pokemon::EncounterChoice::Catch, nullptr, mutation));
  CHECK(state.partyRecordIds == partyBefore);
  CHECK(mutation.kind == pokemon::RecordMutationKind::Append);
  CHECK(mutation.record.recordId == 99);
}

void resolvingABlockingEncounterWaitsForTheNextLevel() {
  pokemon::PokemonRecord bulbasaur = leaderAtLevelFive();
  bulbasaur.speciesId = 1;
  bulbasaur.totalXp = pokemon::xpRequired(17) - 1U;
  pokemon::PokemonState state = stateWithEncounter(bulbasaur);
  pokemon::RecordMutation mutation{};

  CHECK(pokemon::resolveEncounter(state, bulbasaur, pokemon::EncounterChoice::Pass, nullptr, mutation));
  CHECK(state.pendingEvents[0].kind == pokemon::PendingEventKind::None);

  const pokemon::RandomSource random{nullptr, rejectUnexpectedDraw};
  const pokemon::CreditResult result =
      pokemon::applyCreditedMinutes(state, bulbasaur, 1, 10, pokemon::OwnedEvolutionNeeds{}, random);
  CHECK(result.generatedEvent == pokemon::PendingEventKind::Evolution);
  CHECK(state.pendingEvents[0].kind == pokemon::PendingEventKind::Evolution);
  CHECK(state.pendingEvents[0].speciesId == 2);
}

void acknowledgingABlockingItemWaitsForTheNextLevel() {
  pokemon::PokemonRecord bulbasaur = leaderAtLevelFive();
  bulbasaur.speciesId = 1;
  bulbasaur.totalXp = pokemon::xpRequired(17) - 1U;
  pokemon::PokemonState state = stateWithLeader(bulbasaur);
  state.pendingEvents[0].kind = pokemon::PendingEventKind::Item;
  state.pendingEvents[0].item = pokemon::EvolutionItem::MoonStone;
  state.dashboardNotice = pokemon::DashboardNotice::ItemFound;

  CHECK(pokemon::acknowledgeItem(state, bulbasaur));
  CHECK(state.pendingEvents[0].kind == pokemon::PendingEventKind::None);

  const pokemon::RandomSource random{nullptr, rejectUnexpectedDraw};
  const pokemon::CreditResult result =
      pokemon::applyCreditedMinutes(state, bulbasaur, 1, 10, pokemon::OwnedEvolutionNeeds{}, random);
  CHECK(result.generatedEvent == pokemon::PendingEventKind::Evolution);
  CHECK(state.pendingEvents[0].kind == pokemon::PendingEventKind::Evolution);
  CHECK(state.pendingEvents[0].speciesId == 2);
}

void levelEvolutionPromptsOnceAndCanBeConfirmedOrBlocked() {
  pokemon::PokemonRecord bulbasaur = leaderAtLevelFive();
  bulbasaur.speciesId = 1;
  bulbasaur.totalXp = 287;
  pokemon::PokemonState state = stateWithLeader(bulbasaur);
  pokemon::RandomSource random{nullptr, chooseLast};

  pokemon::CreditResult result =
      pokemon::applyCreditedMinutes(state, bulbasaur, 31, 10, pokemon::OwnedEvolutionNeeds{}, random);
  CHECK(result.generatedEvent == pokemon::PendingEventKind::Evolution);
  CHECK(state.pendingEvents[0].kind == pokemon::PendingEventKind::Evolution);
  CHECK(state.pendingEvents[0].recordId == bulbasaur.recordId);
  CHECK(state.pendingEvents[0].speciesId == 2);
  CHECK(state.dashboardNotice == pokemon::DashboardNotice::WhatsThis);

  pokemon::RecordMutation mutation{};
  CHECK(pokemon::resolveEvolution(state, bulbasaur, pokemon::EvolutionChoice::Evolve, mutation));
  CHECK(mutation.kind == pokemon::RecordMutationKind::Replace);
  CHECK(mutation.record.speciesId == 2);
  CHECK(bulbasaur.speciesId == 2);
  CHECK(pokemon::isSpeciesMarked(state.caughtSpecies, 2));
  CHECK(state.pendingEvents[0].kind == pokemon::PendingEventKind::None);

  bulbasaur = leaderAtLevelFive();
  bulbasaur.speciesId = 1;
  bulbasaur.totalXp = 287;
  bulbasaur.flags = pokemon::recordFlag(pokemon::RecordFlag::EvolutionPromptsDisabled);
  state = stateWithLeader(bulbasaur);
  result = pokemon::applyCreditedMinutes(state, bulbasaur, 31, 10, pokemon::OwnedEvolutionNeeds{}, random);
  CHECK(result.generatedEvent == pokemon::PendingEventKind::None);
  CHECK(state.pendingEvents[0].kind == pokemon::PendingEventKind::None);
}

void reenablingPromptsWaitsForTheNextLevel() {
  pokemon::PokemonRecord bulbasaur = leaderAtLevelFive();
  bulbasaur.speciesId = 1;
  bulbasaur.totalXp = pokemon::xpRequired(17) - 1U;
  bulbasaur.flags = pokemon::recordFlag(pokemon::RecordFlag::EvolutionPromptsDisabled);
  pokemon::PokemonState state = stateWithLeader(bulbasaur);
  pokemon::RecordMutation mutation{};

  CHECK(pokemon::setEvolutionPrompts(state, bulbasaur, true, mutation));
  CHECK((bulbasaur.flags & pokemon::recordFlag(pokemon::RecordFlag::EvolutionPromptsDisabled)) == 0);
  CHECK(mutation.requestedRecordId == bulbasaur.recordId);
  CHECK(mutation.kind == pokemon::RecordMutationKind::Replace);
  CHECK(state.pendingEvents[0].kind == pokemon::PendingEventKind::None);

  const pokemon::RandomSource random{nullptr, rejectUnexpectedDraw};
  const pokemon::CreditResult result =
      pokemon::applyCreditedMinutes(state, bulbasaur, 1, 10, pokemon::OwnedEvolutionNeeds{}, random);
  CHECK(result.generatedEvent == pokemon::PendingEventKind::Evolution);
  CHECK(state.pendingEvents[0].kind == pokemon::PendingEventKind::Evolution);
  CHECK(state.pendingEvents[0].speciesId == 2);

  mutation = {};
  CHECK(pokemon::setEvolutionPrompts(state, bulbasaur, false, mutation));
  CHECK((bulbasaur.flags & pokemon::recordFlag(pokemon::RecordFlag::EvolutionPromptsDisabled)) != 0);
  CHECK(mutation.kind == pokemon::RecordMutationKind::Replace);
  CHECK(state.pendingEvents[0].kind == pokemon::PendingEventKind::None);
}

void levelHundredDoesNotCatchUpOrChainLevelEvolutions() {
  pokemon::PokemonRecord bulbasaur = leaderAtLevelFive();
  bulbasaur.speciesId = 1;
  bulbasaur.totalXp = pokemon::MAXIMUM_TOTAL_XP;
  bulbasaur.flags = pokemon::recordFlag(pokemon::RecordFlag::EvolutionPromptsDisabled);
  pokemon::PokemonState state = stateWithLeader(bulbasaur);
  pokemon::RecordMutation mutation{};

  CHECK(pokemon::setEvolutionPrompts(state, bulbasaur, true, mutation));
  CHECK(state.pendingEvents[0].kind == pokemon::PendingEventKind::None);

  state.pendingEvents[0].kind = pokemon::PendingEventKind::Evolution;
  state.pendingEvents[0].recordId = bulbasaur.recordId;
  state.pendingEvents[0].speciesId = 2;
  state.dashboardNotice = pokemon::DashboardNotice::WhatsThis;
  mutation = {};
  CHECK(pokemon::resolveEvolution(state, bulbasaur, pokemon::EvolutionChoice::Evolve, mutation));
  CHECK(bulbasaur.speciesId == 2);
  CHECK(state.pendingEvents[0].kind == pokemon::PendingEventKind::None);

  const pokemon::RandomSource random{nullptr, rejectUnexpectedDraw};
  const pokemon::CreditResult result =
      pokemon::applyCreditedMinutes(state, bulbasaur, 1, 10, pokemon::OwnedEvolutionNeeds{}, random);
  CHECK(result.currentLevel == 100);
  CHECK(result.generatedEvent == pokemon::PendingEventKind::None);
  CHECK(state.pendingEvents[0].kind == pokemon::PendingEventKind::None);
}

void promptTogglePreservesAnUnrelatedPendingEvent() {
  pokemon::PokemonRecord leader = leaderAtLevelFive();
  leader.flags = pokemon::recordFlag(pokemon::RecordFlag::EvolutionPromptsDisabled);
  pokemon::PokemonState state = stateWithEncounter(leader);
  const pokemon::PendingEvent pendingBefore = state.pendingEvents[0];
  pokemon::RecordMutation mutation{};

  CHECK(pokemon::setEvolutionPrompts(state, leader, true, mutation));
  CHECK((leader.flags & pokemon::recordFlag(pokemon::RecordFlag::EvolutionPromptsDisabled)) == 0);
  CHECK(state.pendingEvents[0] == pendingBefore);

  mutation = {};
  CHECK(pokemon::setEvolutionPrompts(state, leader, false, mutation));
  CHECK((leader.flags & pokemon::recordFlag(pokemon::RecordFlag::EvolutionPromptsDisabled)) != 0);
  CHECK(state.pendingEvents[0] == pendingBefore);
}

void disablingPromptsRemovesOnlyMatchingQueuedEvolutions() {
  pokemon::PokemonRecord leader = leaderAtLevelFive();
  pokemon::PokemonState state = stateWithLeader(leader);
  const pokemon::PendingEvent encounter{
      0, 4, 6, pokemon::Gender::Female, pokemon::EvolutionItem::None, pokemon::PendingEventKind::Encounter};
  const pokemon::PendingEvent evolution{leader.recordId,
                                        26,
                                        0,
                                        pokemon::Gender::Unknown,
                                        pokemon::EvolutionItem::None,
                                        pokemon::PendingEventKind::Evolution};
  const pokemon::PendingEvent item{
      0, 0, 0, pokemon::Gender::Unknown, pokemon::EvolutionItem::ThunderStone, pokemon::PendingEventKind::Item};
  state.pendingEvents = {encounter, evolution, item};
  state.dashboardNotice = pokemon::DashboardNotice::NewPokemon;
  pokemon::RecordMutation mutation{};

  CHECK(pokemon::setEvolutionPrompts(state, leader, false, mutation));
  CHECK(pokemon::pendingEventCount(state) == 2);
  CHECK(state.pendingEvents[0] == encounter);
  CHECK(state.pendingEvents[1] == item);
  CHECK(state.dashboardNotice == pokemon::DashboardNotice::NewPokemon);
  CHECK(mutation.kind == pokemon::RecordMutationKind::Replace);
}

void rejectedPromptToggleDoesNotPartiallyMutate() {
  pokemon::PokemonRecord leader = leaderAtLevelFive();
  leader.flags = pokemon::recordFlag(pokemon::RecordFlag::EvolutionPromptsDisabled);
  pokemon::PokemonState state = stateWithLeader(leader);
  pokemon::RecordMutation mutation{};
  mutation.requestedRecordId = 99;
  mutation.record = leaderAtLevelFive();
  mutation.kind = pokemon::RecordMutationKind::Append;
  const pokemon::PokemonRecord leaderBefore = leader;
  const pokemon::PokemonState stateBefore = state;
  const pokemon::RecordMutation mutationBefore = mutation;

  CHECK(!pokemon::setEvolutionPrompts(state, leader, true, mutation));
  CHECK(leader == leaderBefore);
  CHECK(state == stateBefore);
  CHECK(mutation.requestedRecordId == mutationBefore.requestedRecordId);
  CHECK(mutation.record == mutationBefore.record);
  CHECK(mutation.kind == mutationBefore.kind);
}

void everyLevelEvolutionQueuesAtItsThreshold() {
  size_t checked = 0;
  for (uint16_t speciesId = 1; speciesId <= pokemon::KANTO_SPECIES_COUNT; ++speciesId) {
    for (const pokemon::EvolutionRule& rule : pokemon::evolutionsFor(speciesId)) {
      if (rule.trigger != pokemon::EvolutionTrigger::Level) continue;
      pokemon::PokemonRecord record = leaderAtLevelFive();
      record.speciesId = speciesId;
      record.totalXp = pokemon::xpRequired(rule.minimumLevel) - 1U;
      record.gender = validGenderForSpecies(speciesId);
      pokemon::PokemonState state = stateWithLeader(record);
      pokemon::RandomSource random{nullptr, rejectUnexpectedDraw};

      const pokemon::CreditResult result =
          pokemon::applyCreditedMinutes(state, record, 1, 0, pokemon::OwnedEvolutionNeeds{}, random);
      CHECK(result.status == pokemon::CreditStatus::Applied);
      CHECK(state.pendingEvents[0].kind == pokemon::PendingEventKind::Evolution);
      CHECK(state.pendingEvents[0].speciesId == rule.targetSpeciesId);
      pokemon::RecordMutation mutation{};
      CHECK(pokemon::resolveEvolution(state, record, pokemon::EvolutionChoice::Evolve, mutation));
      CHECK(record.speciesId == rule.targetSpeciesId);
      CHECK(mutation.kind == pokemon::RecordMutationKind::Replace);
      CHECK(mutation.requestedRecordId == record.recordId);
      CHECK(mutation.record.speciesId == rule.targetSpeciesId);
      CHECK(pokemon::isSpeciesMarked(state.seenSpecies, rule.targetSpeciesId));
      CHECK(pokemon::isSpeciesMarked(state.caughtSpecies, rule.targetSpeciesId));
      ++checked;
    }
  }
  CHECK(checked == 52);
}

void everyItemEvolutionConsumesExactlyOneItem() {
  struct ItemEvolutionCase {
    uint16_t source;
    uint16_t target;
    pokemon::EvolutionItem item;
  };
  constexpr ItemEvolutionCase cases[] = {
      {25, 26, pokemon::EvolutionItem::ThunderStone},   {30, 31, pokemon::EvolutionItem::MoonStone},
      {33, 34, pokemon::EvolutionItem::MoonStone},      {35, 36, pokemon::EvolutionItem::MoonStone},
      {37, 38, pokemon::EvolutionItem::FireStone},      {39, 40, pokemon::EvolutionItem::MoonStone},
      {44, 45, pokemon::EvolutionItem::LeafStone},      {58, 59, pokemon::EvolutionItem::FireStone},
      {61, 62, pokemon::EvolutionItem::WaterStone},     {64, 65, pokemon::EvolutionItem::LinkCable},
      {67, 68, pokemon::EvolutionItem::LinkCable},      {70, 71, pokemon::EvolutionItem::LeafStone},
      {75, 76, pokemon::EvolutionItem::LinkCable},      {90, 91, pokemon::EvolutionItem::WaterStone},
      {93, 94, pokemon::EvolutionItem::LinkCable},      {102, 103, pokemon::EvolutionItem::LeafStone},
      {120, 121, pokemon::EvolutionItem::WaterStone},   {133, 134, pokemon::EvolutionItem::WaterStone},
      {133, 135, pokemon::EvolutionItem::ThunderStone}, {133, 136, pokemon::EvolutionItem::FireStone},
  };

  for (const ItemEvolutionCase& itemCase : cases) {
    pokemon::PokemonRecord record = leaderAtLevelFive();
    record.speciesId = itemCase.source;
    record.gender = validGenderForSpecies(itemCase.source);
    pokemon::PokemonState state{};
    const size_t itemIndex = static_cast<size_t>(itemCase.item) - 1U;
    state.itemCounts[itemIndex] = 1;
    pokemon::RecordMutation mutation{};
    CHECK(pokemon::useEvolutionItem(state, record, itemCase.item, mutation));
    CHECK(record.speciesId == itemCase.target);
    CHECK(state.itemCounts[itemIndex] == 0);
    CHECK(mutation.kind == pokemon::RecordMutationKind::Replace);
    CHECK(mutation.requestedRecordId == record.recordId);
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
  CHECK(!pokemon::resolveEncounter(state, leader, static_cast<pokemon::EncounterChoice>(99), nullptr, mutation));
  CHECK(state == encounterBefore);
  CHECK(mutation.kind == pokemon::RecordMutationKind::None);

  leader.speciesId = 1;
  leader.totalXp = pokemon::xpRequired(16);
  state = stateWithLeader(leader);
  state.pendingEvents[0].kind = pokemon::PendingEventKind::Evolution;
  state.pendingEvents[0].recordId = leader.recordId;
  state.pendingEvents[0].speciesId = 2;
  state.dashboardNotice = pokemon::DashboardNotice::WhatsThis;
  CHECK(pokemon::resolveEvolution(state, leader, pokemon::EvolutionChoice::Cancel, mutation));
  CHECK(leader.speciesId == 1);
  CHECK(state.pendingEvents[0].kind == pokemon::PendingEventKind::None);
  CHECK(mutation.kind == pokemon::RecordMutationKind::None);
}

}  // namespace

int main() {
  collectionActionsExcludeOperationsThatCannotSucceed();
  creditedMinutesAdvanceLeaderAndReadingCounters();
  creditClampsAndRejectsInvalidCallsWithoutMutation();
  fifteenMinuteCheckUsesFortyPercentEncounterChance();
  fourthEncounterCheckIsForcedAndCreatesOnlyOneEvent();
  fullQueueCreditsReadingAndPrimesGuaranteesWithoutRolling();
  oneBoundaryQueuesItemEncounterAndEvolutionInOrder();
  resolvingEventsPopsOnlyTheFrontAndRefreshesTheNotice();
  encounterDueWithLevelGainQueuesEncounterBeforeEvolution();
  forcedItemWithLevelGainQueuesItemBeforeEvolution();
  multiHourCreditUsesLifetimeAtEachHourlyBoundary();
  progressBandsGateStagesAndEncounterLevels();
  everyEligibleRegularEncounterWeightIntervalSelectsItsSpecies();
  itemRollAndPityPreferAnOwnedEvolutionNeed();
  saturatedItemsDoNotRejectReadingCredit();
  legendaryEligibilityAndMewOverrideRegularEncounters();
  catchCreatesOneAppendAndPassCreatesNone();
  fullPartyCatchLeavesTheNewRecordForPcStorage();
  resolvingABlockingEncounterWaitsForTheNextLevel();
  acknowledgingABlockingItemWaitsForTheNextLevel();
  levelEvolutionPromptsOnceAndCanBeConfirmedOrBlocked();
  reenablingPromptsWaitsForTheNextLevel();
  levelHundredDoesNotCatchUpOrChainLevelEvolutions();
  promptTogglePreservesAnUnrelatedPendingEvent();
  disablingPromptsRemovesOnlyMatchingQueuedEvolutions();
  rejectedPromptToggleDoesNotPartiallyMutate();
  everyLevelEvolutionQueuesAtItsThreshold();
  everyItemEvolutionConsumesExactlyOneItem();
  rejectedInputsAndCancelledEvolutionDoNotPartiallyMutate();
  return failures == 0 ? 0 : 1;
}
