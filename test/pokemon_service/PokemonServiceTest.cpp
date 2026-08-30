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
