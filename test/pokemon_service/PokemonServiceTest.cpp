#include <gtest/gtest.h>

#include <HalStorage.h>

#include "pokemon/PokemonService.h"

namespace {

uint32_t zeroRandom(void*, uint32_t) { return 0; }

uint32_t itemEventRandom(void*, const uint32_t upperExclusive) {
  return upperExclusive == 4 ? 1U : 0U;
}

pokemon::PokemonRecord starterPikachu() {
  pokemon::PokemonRecord record{};
  record.recordId = 1;
  record.totalXp = pokemon::xpRequired(5);
  record.speciesId = 25;
  record.caughtLevel = 5;
  record.gender = pokemon::Gender::Female;
  record.origin = pokemon::Origin::Starter;
  return record;
}

void seedStarter(pokemon::PokemonStore& store) {
  ASSERT_EQ(store.begin(), pokemon::StoreBeginResult::Empty);
  const pokemon::PokemonRecord starter = starterPikachu();
  pokemon::PokemonState state{};
  state.partyRecordIds[0] = starter.recordId;
  ASSERT_TRUE(pokemon::markSpecies(state.seenSpecies, starter.speciesId));
  ASSERT_TRUE(pokemon::markSpecies(state.caughtSpecies, starter.speciesId));
  const pokemon::RecordMutation mutation{starter.recordId, starter, pokemon::RecordMutationKind::Append};
  ASSERT_TRUE(store.commit(state, mutation));
}

pokemon::PokemonRecord caughtPokemon(const uint32_t recordId, const uint16_t speciesId) {
  pokemon::PokemonRecord record{};
  record.recordId = recordId;
  record.totalXp = pokemon::xpRequired(5);
  record.speciesId = speciesId;
  record.caughtLevel = 5;
  record.gender = pokemon::Gender::Male;
  record.origin = pokemon::Origin::Caught;
  return record;
}

void appendOwnedPokemon(pokemon::PokemonStore& store, const pokemon::PokemonRecord& record,
                        const bool addToParty) {
  pokemon::PokemonState state{};
  ASSERT_TRUE(store.loadState(state));
  ASSERT_TRUE(pokemon::markSpecies(state.seenSpecies, record.speciesId));
  ASSERT_TRUE(pokemon::markSpecies(state.caughtSpecies, record.speciesId));
  if (addToParty) {
    size_t slot = 0;
    while (slot < pokemon::PARTY_SIZE && state.partyRecordIds[slot] != 0) ++slot;
    ASSERT_LT(slot, pokemon::PARTY_SIZE);
    state.partyRecordIds[slot] = record.recordId;
  }
  const pokemon::RecordMutation mutation{record.recordId, record, pokemon::RecordMutationKind::Append};
  ASSERT_TRUE(store.commit(state, mutation));
}

TEST(PokemonService, VerifiedCheckpointDurablyCreditsStateAndLeader) {
  Storage.clear();
  pokemon::PokemonStore store;
  seedStarter(store);
  pokemon::PokemonService service(store, {nullptr, zeroRandom});

  ASSERT_TRUE(service.beginReadingSession());
  service.setBookProgressPercent(64);
  service.onSuccessfulPageTurn(0);
  service.checkpointIfDue(300000);

  pokemon::PokemonStore reopened;
  ASSERT_EQ(reopened.begin(), pokemon::StoreBeginResult::Ready);
  pokemon::PokemonState state{};
  pokemon::PokemonRecord leader{};
  ASSERT_TRUE(reopened.loadState(state));
  ASSERT_TRUE(reopened.readRecord(1, leader));
  EXPECT_EQ(state.lifetimeMinutes, 5U);
  EXPECT_EQ(leader.totalXp, pokemon::xpRequired(5) + 5U);
}

TEST(PokemonService, FailedSnapshotWriteAdvancesNeitherStateNorLeader) {
  Storage.clear();
  pokemon::PokemonStore store;
  seedStarter(store);
  pokemon::PokemonService service(store, {nullptr, zeroRandom});
  ASSERT_TRUE(service.beginReadingSession());

  Storage.setFailWritableOpen(true);
  EXPECT_FALSE(service.creditMinutes(5, 64));
  Storage.setFailWritableOpen(false);

  pokemon::PokemonStore reopened;
  ASSERT_EQ(reopened.begin(), pokemon::StoreBeginResult::Ready);
  pokemon::PokemonState state{};
  pokemon::PokemonRecord leader{};
  ASSERT_TRUE(reopened.loadState(state));
  ASSERT_TRUE(reopened.readRecord(1, leader));
  EXPECT_EQ(state.lifetimeMinutes, 0U);
  EXPECT_EQ(leader.totalXp, pokemon::xpRequired(5));
}

TEST(PokemonService, FailedSyncRetryCannotDoubleCreditOnTheNextReaderSession) {
  Storage.clear();
  pokemon::PokemonStore store;
  seedStarter(store);
  pokemon::PokemonService service(store, {nullptr, zeroRandom});
  ASSERT_TRUE(service.beginReadingSession());
  service.setBookProgressPercent(64);
  service.onSuccessfulPageTurn(0);

  Storage.setFailSync(true);
  service.flushOnExit(300000);
  Storage.setFailSync(false);

  ASSERT_TRUE(service.beginReadingSession());
  pokemon::PokemonState state{};
  pokemon::PokemonRecord leader{};
  ASSERT_TRUE(store.loadState(state));
  ASSERT_TRUE(store.readRecord(1, leader));
  EXPECT_EQ(state.lifetimeMinutes, 5U);
  EXPECT_EQ(leader.totalXp, pokemon::xpRequired(5) + 5U);
}

TEST(PokemonService, ReadingSessionDoesNotStartBeforeStarterExists) {
  Storage.clear();
  pokemon::PokemonStore store;
  ASSERT_EQ(store.begin(), pokemon::StoreBeginResult::Empty);
  ASSERT_TRUE(store.commit({}));
  pokemon::PokemonService service(store, {nullptr, zeroRandom});

  EXPECT_FALSE(service.beginReadingSession());
}

TEST(PokemonService, CreatesOneDurableStarterWithChosenIdentity) {
  Storage.clear();
  pokemon::PokemonStore store;
  pokemon::PokemonService service(store, {nullptr, zeroRandom});

  EXPECT_EQ(service.createStarter(25, pokemon::Gender::Female, "CinderVolt"),
            pokemon::ServiceStatus::Ok);

  pokemon::PokemonSnapshot snapshot{};
  ASSERT_EQ(service.loadSnapshot(snapshot), pokemon::ServiceStatus::Ok);
  ASSERT_EQ(snapshot.partyCount, 1U);
  ASSERT_EQ(snapshot.ownedCount, 1U);
  EXPECT_EQ(snapshot.party[0].recordId, 1U);
  EXPECT_EQ(snapshot.party[0].speciesId, 25U);
  EXPECT_EQ(snapshot.party[0].gender, pokemon::Gender::Female);
  EXPECT_EQ(snapshot.party[0].origin, pokemon::Origin::Starter);
  EXPECT_EQ(snapshot.party[0].totalXp, pokemon::xpRequired(5));
  EXPECT_STREQ(snapshot.party[0].nickname.data(), "CinderVolt");
  EXPECT_TRUE(pokemon::isSpeciesMarked(snapshot.state.seenSpecies, 25));
  EXPECT_TRUE(pokemon::isSpeciesMarked(snapshot.state.caughtSpecies, 25));

  pokemon::PokemonStore reopened;
  ASSERT_EQ(reopened.begin(), pokemon::StoreBeginResult::Ready);
  pokemon::PokemonRecord durable{};
  ASSERT_TRUE(reopened.readRecord(1, durable));
  EXPECT_EQ(durable, snapshot.party[0]);
}

TEST(PokemonService, RejectsSecondStarterWithoutChangingTheSave) {
  Storage.clear();
  pokemon::PokemonStore store;
  pokemon::PokemonService service(store, {nullptr, zeroRandom});
  ASSERT_EQ(service.createStarter(1, pokemon::Gender::Male, ""), pokemon::ServiceStatus::Ok);

  EXPECT_EQ(service.createStarter(4, pokemon::Gender::Female, ""),
            pokemon::ServiceStatus::AlreadyStarted);

  pokemon::PokemonSnapshot snapshot{};
  ASSERT_EQ(service.loadSnapshot(snapshot), pokemon::ServiceStatus::Ok);
  ASSERT_EQ(snapshot.partyCount, 1U);
  EXPECT_EQ(snapshot.party[0].speciesId, 1U);
  EXPECT_EQ(snapshot.ownedCount, 1U);
}

TEST(PokemonService, RenamesDurablyAndLeavesTheOldNameWhenSavingFails) {
  Storage.clear();
  pokemon::PokemonStore store;
  seedStarter(store);
  pokemon::PokemonService service(store, {nullptr, zeroRandom});

  ASSERT_EQ(service.renamePokemon(1, "Sparky"), pokemon::ServiceStatus::Ok);
  pokemon::PokemonRecord renamed{};
  ASSERT_EQ(service.readRecord(1, renamed), pokemon::ServiceStatus::Ok);
  EXPECT_STREQ(renamed.nickname.data(), "Sparky");

  Storage.setFailWritableOpen(true);
  EXPECT_EQ(service.renamePokemon(1, "Static"), pokemon::ServiceStatus::StorageError);
  Storage.setFailWritableOpen(false);

  pokemon::PokemonStore reopened;
  ASSERT_EQ(reopened.begin(), pokemon::StoreBeginResult::Ready);
  ASSERT_TRUE(reopened.readRecord(1, renamed));
  EXPECT_STREQ(renamed.nickname.data(), "Sparky");
}

TEST(PokemonService, RejectsMovingOnlyMemberIntoAnEmptySlotWithoutWriting) {
  Storage.clear();
  pokemon::PokemonStore store;
  seedStarter(store);
  pokemon::PokemonService service(store, {nullptr, zeroRandom});

  EXPECT_EQ(service.movePartyMember(0, 5), pokemon::ServiceStatus::Invalid);

  pokemon::PokemonSnapshot snapshot{};
  ASSERT_EQ(service.loadSnapshot(snapshot), pokemon::ServiceStatus::Ok);
  ASSERT_EQ(snapshot.partyCount, 1U);
  EXPECT_EQ(snapshot.state.partyRecordIds[0], 1U);
  for (size_t slot = 1; slot < pokemon::PARTY_SIZE; ++slot) {
    EXPECT_EQ(snapshot.state.partyRecordIds[slot], 0U);
  }
}

TEST(PokemonService, ReordersOnlyOccupiedPartySlotsAndChangesTheLeader) {
  Storage.clear();
  pokemon::PokemonStore store;
  seedStarter(store);
  appendOwnedPokemon(store, caughtPokemon(2, 4), true);
  appendOwnedPokemon(store, caughtPokemon(3, 7), true);
  pokemon::PokemonService service(store, {nullptr, zeroRandom});

  ASSERT_EQ(service.movePartyMember(2, 0), pokemon::ServiceStatus::Ok);

  pokemon::PokemonSnapshot snapshot{};
  ASSERT_EQ(service.loadSnapshot(snapshot), pokemon::ServiceStatus::Ok);
  ASSERT_EQ(snapshot.partyCount, 3U);
  EXPECT_EQ(snapshot.state.partyRecordIds[0], 3U);
  EXPECT_EQ(snapshot.state.partyRecordIds[1], 1U);
  EXPECT_EQ(snapshot.state.partyRecordIds[2], 2U);
  EXPECT_EQ(snapshot.party[0].speciesId, 7U);
}

TEST(PokemonService, ProtectsTheLastPartyMemberAndSupportsDepositWithdraw) {
  Storage.clear();
  pokemon::PokemonStore store;
  seedStarter(store);
  pokemon::PokemonService service(store, {nullptr, zeroRandom});
  EXPECT_EQ(service.depositPokemon(1), pokemon::ServiceStatus::LastPokemon);

  appendOwnedPokemon(store, caughtPokemon(2, 4), true);
  ASSERT_EQ(service.depositPokemon(1), pokemon::ServiceStatus::Ok);
  pokemon::PokemonSnapshot snapshot{};
  ASSERT_EQ(service.loadSnapshot(snapshot), pokemon::ServiceStatus::Ok);
  ASSERT_EQ(snapshot.partyCount, 1U);
  EXPECT_EQ(snapshot.state.partyRecordIds[0], 2U);

  ASSERT_EQ(service.withdrawPokemon(1), pokemon::ServiceStatus::Ok);
  ASSERT_EQ(service.loadSnapshot(snapshot), pokemon::ServiceStatus::Ok);
  ASSERT_EQ(snapshot.partyCount, 2U);
  EXPECT_EQ(snapshot.state.partyRecordIds[1], 1U);
}

TEST(PokemonService, RejectsWithdrawalWhenThePartyIsFull) {
  Storage.clear();
  pokemon::PokemonStore store;
  seedStarter(store);
  for (uint32_t id = 2; id <= 7; ++id) {
    appendOwnedPokemon(store, caughtPokemon(id, static_cast<uint16_t>(id + 3)), id <= 6);
  }
  pokemon::PokemonService service(store, {nullptr, zeroRandom});

  EXPECT_EQ(service.withdrawPokemon(7), pokemon::ServiceStatus::PartyFull);

  pokemon::PokemonSnapshot snapshot{};
  ASSERT_EQ(service.loadSnapshot(snapshot), pokemon::ServiceStatus::Ok);
  EXPECT_EQ(snapshot.partyCount, pokemon::PARTY_SIZE);
}

TEST(PokemonService, ResolvesEncounterCatchThenAllowsNickname) {
  Storage.clear();
  pokemon::PokemonStore store;
  seedStarter(store);
  pokemon::PokemonState state{};
  ASSERT_TRUE(store.loadState(state));
  state.pending.kind = pokemon::PendingEventKind::Encounter;
  state.pending.speciesId = 133;
  state.pending.level = 12;
  state.pending.gender = pokemon::Gender::Female;
  state.dashboardNotice = pokemon::DashboardNotice::NewPokemon;
  ASSERT_TRUE(store.commit(state));
  pokemon::PokemonService service(store, {nullptr, zeroRandom});

  uint32_t caughtRecordId = 0;
  ASSERT_EQ(service.resolveEncounter(pokemon::EncounterChoice::Catch, caughtRecordId),
            pokemon::ServiceStatus::Ok);
  EXPECT_EQ(caughtRecordId, 2U);
  ASSERT_EQ(service.renamePokemon(caughtRecordId, "Nova"), pokemon::ServiceStatus::Ok);

  pokemon::PokemonSnapshot snapshot{};
  ASSERT_EQ(service.loadSnapshot(snapshot), pokemon::ServiceStatus::Ok);
  ASSERT_EQ(snapshot.partyCount, 2U);
  EXPECT_EQ(snapshot.state.pending.kind, pokemon::PendingEventKind::None);
  EXPECT_EQ(snapshot.state.dashboardNotice, pokemon::DashboardNotice::None);
  EXPECT_EQ(snapshot.party[1].speciesId, 133U);
  EXPECT_STREQ(snapshot.party[1].nickname.data(), "Nova");
}

TEST(PokemonService, ResolvesEncounterPassWithoutCreatingARecord) {
  Storage.clear();
  pokemon::PokemonStore store;
  seedStarter(store);
  pokemon::PokemonState state{};
  ASSERT_TRUE(store.loadState(state));
  state.pending.kind = pokemon::PendingEventKind::Encounter;
  state.pending.speciesId = 4;
  state.pending.level = 9;
  state.pending.gender = pokemon::Gender::Male;
  state.dashboardNotice = pokemon::DashboardNotice::NewPokemon;
  ASSERT_TRUE(store.commit(state));
  pokemon::PokemonService service(store, {nullptr, zeroRandom});

  uint32_t caughtRecordId = 99;
  ASSERT_EQ(service.resolveEncounter(pokemon::EncounterChoice::Pass, caughtRecordId),
            pokemon::ServiceStatus::Ok);
  EXPECT_EQ(caughtRecordId, 0U);
  pokemon::PokemonSnapshot snapshot{};
  ASSERT_EQ(service.loadSnapshot(snapshot), pokemon::ServiceStatus::Ok);
  EXPECT_EQ(snapshot.ownedCount, 1U);
  EXPECT_EQ(snapshot.state.pending.kind, pokemon::PendingEventKind::None);
}

TEST(PokemonService, AcknowledgesItemAndPublishesBoundedDashboardSnapshot) {
  Storage.clear();
  pokemon::PokemonStore store;
  seedStarter(store);
  pokemon::PokemonState state{};
  ASSERT_TRUE(store.loadState(state));
  state.pending.kind = pokemon::PendingEventKind::Item;
  state.pending.item = pokemon::EvolutionItem::ThunderStone;
  state.itemCounts[2] = 1;
  state.dashboardNotice = pokemon::DashboardNotice::ItemFound;
  ASSERT_TRUE(store.commit(state));
  pokemon::PokemonService service(store, {nullptr, zeroRandom});

  pokemon::PokemonDashboardSnapshot dashboard{};
  ASSERT_EQ(service.loadDashboardSnapshot(dashboard), pokemon::ServiceStatus::Ok);
  EXPECT_EQ(dashboard.leader.speciesId, 25U);
  EXPECT_EQ(dashboard.notice, pokemon::DashboardNotice::ItemFound);
  EXPECT_EQ(dashboard.pending.kind, pokemon::PendingEventKind::Item);

  ASSERT_EQ(service.acknowledgeItem(), pokemon::ServiceStatus::Ok);
  pokemon::PokemonSnapshot snapshot{};
  ASSERT_EQ(service.loadSnapshot(snapshot), pokemon::ServiceStatus::Ok);
  EXPECT_EQ(snapshot.state.pending.kind, pokemon::PendingEventKind::None);
  EXPECT_EQ(snapshot.state.itemCounts[2], 1U);
}

TEST(PokemonService, EvolvesByLevelAndCanDisableFuturePrompts) {
  Storage.clear();
  pokemon::PokemonStore store;
  ASSERT_EQ(store.begin(), pokemon::StoreBeginResult::Empty);
  pokemon::PokemonRecord bulbasaur = caughtPokemon(1, 1);
  bulbasaur.origin = pokemon::Origin::Starter;
  bulbasaur.totalXp = pokemon::xpRequired(16);
  pokemon::PokemonState state{};
  state.partyRecordIds[0] = 1;
  state.pending.kind = pokemon::PendingEventKind::Evolution;
  state.pending.recordId = 1;
  state.pending.speciesId = 2;
  state.dashboardNotice = pokemon::DashboardNotice::WhatsThis;
  ASSERT_TRUE(pokemon::markSpecies(state.seenSpecies, 1));
  ASSERT_TRUE(pokemon::markSpecies(state.caughtSpecies, 1));
  ASSERT_TRUE(store.commit(state, {1, bulbasaur, pokemon::RecordMutationKind::Append}));
  pokemon::PokemonService service(store, {nullptr, zeroRandom});

  ASSERT_EQ(service.resolveEvolution(pokemon::EvolutionChoice::Evolve), pokemon::ServiceStatus::Ok);
  pokemon::PokemonRecord evolved{};
  ASSERT_EQ(service.readRecord(1, evolved), pokemon::ServiceStatus::Ok);
  EXPECT_EQ(evolved.speciesId, 2U);
  ASSERT_EQ(service.setEvolutionPrompts(1, false), pokemon::ServiceStatus::Ok);
  ASSERT_EQ(service.readRecord(1, evolved), pokemon::ServiceStatus::Ok);
  EXPECT_NE(evolved.flags & pokemon::recordFlag(pokemon::RecordFlag::EvolutionPromptsDisabled), 0U);
}

TEST(PokemonService, ConsumesStoneOnlyForApplicableEvolution) {
  Storage.clear();
  pokemon::PokemonStore store;
  seedStarter(store);
  pokemon::PokemonState state{};
  ASSERT_TRUE(store.loadState(state));
  state.itemCounts[2] = 1;
  ASSERT_TRUE(store.commit(state));
  pokemon::PokemonService service(store, {nullptr, zeroRandom});

  EXPECT_EQ(service.useEvolutionItem(1, pokemon::EvolutionItem::WaterStone),
            pokemon::ServiceStatus::NotApplicable);
  ASSERT_EQ(service.useEvolutionItem(1, pokemon::EvolutionItem::ThunderStone),
            pokemon::ServiceStatus::Ok);

  pokemon::PokemonSnapshot snapshot{};
  ASSERT_EQ(service.loadSnapshot(snapshot), pokemon::ServiceStatus::Ok);
  EXPECT_EQ(snapshot.party[0].speciesId, 26U);
  EXPECT_EQ(snapshot.state.itemCounts[2], 0U);
}

TEST(PokemonService, ReadsBoundedPcPagesAndResetsToEmpty) {
  Storage.clear();
  pokemon::PokemonStore store;
  seedStarter(store);
  appendOwnedPokemon(store, caughtPokemon(2, 7), false);
  appendOwnedPokemon(store, caughtPokemon(3, 4), false);
  pokemon::PokemonService service(store, {nullptr, zeroRandom});

  std::array<pokemon::PokemonRecord, 6> page{};
  size_t count = 0;
  ASSERT_EQ(service.readPcPage(pokemon::PcOrder::PokedexNumber, 0, page, count),
            pokemon::ServiceStatus::Ok);
  ASSERT_EQ(count, 2U);
  EXPECT_EQ(page[0].speciesId, 4U);
  EXPECT_EQ(page[1].speciesId, 7U);

  ASSERT_EQ(service.reset(), pokemon::ServiceStatus::Ok);
  pokemon::PokemonSnapshot snapshot{};
  EXPECT_EQ(service.loadSnapshot(snapshot), pokemon::ServiceStatus::Empty);
}

TEST(PokemonService, HourlyItemDropPrefersAnOwnedPokemonsEvolutionNeed) {
  Storage.clear();
  pokemon::PokemonStore store;
  seedStarter(store);
  pokemon::PokemonService service(store, {nullptr, itemEventRandom});
  ASSERT_TRUE(service.beginReadingSession());

  ASSERT_TRUE(service.creditMinutes(60, 64));

  pokemon::PokemonState state{};
  ASSERT_TRUE(store.loadState(state));
  EXPECT_EQ(state.pending.kind, pokemon::PendingEventKind::Item);
  EXPECT_EQ(state.pending.item, pokemon::EvolutionItem::ThunderStone);
  EXPECT_EQ(state.itemCounts[2], 1U);
}

}  // namespace
