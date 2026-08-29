#include <cstdint>
#include <cstdio>

#include <HalStorage.h>

#include "pokemon/PokemonStore.h"

namespace {

int failures = 0;

#define CHECK(condition)                                                                  \
  do {                                                                                    \
    if (!(condition)) {                                                                   \
      std::fprintf(stderr, "%s:%d check failed: %s\n", __FILE__, __LINE__, #condition); \
      ++failures;                                                                         \
    }                                                                                     \
  } while (false)

pokemon::PokemonRecord starterPikachu() {
  pokemon::PokemonRecord record{};
  record.recordId = 1;
  record.totalXp = 52;
  record.speciesId = 25;
  record.caughtLevel = 5;
  record.gender = pokemon::Gender::Female;
  record.origin = pokemon::Origin::Starter;
  return record;
}

pokemon::PokemonRecord caughtBulbasaur(const uint32_t recordId = 2) {
  pokemon::PokemonRecord record{};
  record.recordId = recordId;
  record.totalXp = 52;
  record.speciesId = 1;
  record.caughtLevel = 5;
  record.gender = pokemon::Gender::Female;
  record.origin = pokemon::Origin::Caught;
  return record;
}

pokemon::PokemonRecord caughtAbra(const uint32_t recordId) {
  pokemon::PokemonRecord record{};
  record.recordId = recordId;
  record.totalXp = 52;
  record.speciesId = 63;
  record.caughtLevel = 5;
  record.gender = pokemon::Gender::Female;
  record.origin = pokemon::Origin::Caught;
  return record;
}

void emptyStoreCommitsAndReloadsSequenceOne() {
  Storage.clear();
  pokemon::PokemonStore store;
  CHECK(store.begin() == pokemon::StoreBeginResult::Empty);
  pokemon::PokemonState state{};
  state.lifetimeMinutes = 42;

  CHECK(store.commit(state));
  CHECK(Storage.exists("/.crosspoint/pokemon-v2-a.bin"));
  CHECK(!Storage.exists("/.crosspoint/pokemon-v2-b.bin"));

  pokemon::PokemonStore reopened;
  CHECK(reopened.begin() == pokemon::StoreBeginResult::Ready);
  pokemon::PokemonState loaded{};
  CHECK(reopened.loadState(loaded));
  state.sequence = 1;
  CHECK(loaded == state);
}

void secondCommitUsesSlotBAndWinsStartupSelection() {
  Storage.clear();
  pokemon::PokemonStore store;
  CHECK(store.begin() == pokemon::StoreBeginResult::Empty);
  pokemon::PokemonState state{};
  state.lifetimeMinutes = 10;
  CHECK(store.commit(state));
  state.lifetimeMinutes = 20;
  CHECK(store.commit(state));
  CHECK(Storage.exists("/.crosspoint/pokemon-v2-a.bin"));
  CHECK(Storage.exists("/.crosspoint/pokemon-v2-b.bin"));

  pokemon::PokemonStore reopened;
  CHECK(reopened.begin() == pokemon::StoreBeginResult::Ready);
  pokemon::PokemonState loaded{};
  CHECK(reopened.loadState(loaded));
  CHECK(loaded.sequence == 2);
  CHECK(loaded.lifetimeMinutes == 20);
}

void initialAppendPersistsAReadableRecord() {
  Storage.clear();
  pokemon::PokemonStore store;
  CHECK(store.begin() == pokemon::StoreBeginResult::Empty);
  const pokemon::PokemonRecord pikachu = starterPikachu();
  pokemon::PokemonState state{};
  state.partyRecordIds[0] = pikachu.recordId;
  CHECK(pokemon::markSpecies(state.seenSpecies, pikachu.speciesId));
  CHECK(pokemon::markSpecies(state.caughtSpecies, pikachu.speciesId));
  pokemon::RecordMutation mutation{};
  mutation.requestedRecordId = pikachu.recordId;
  mutation.record = pikachu;
  mutation.kind = pokemon::RecordMutationKind::Append;

  CHECK(store.commit(state, mutation));
  pokemon::PokemonStore reopened;
  CHECK(reopened.begin() == pokemon::StoreBeginResult::Ready);
  pokemon::PokemonRecord loaded{};
  CHECK(reopened.readRecord(pikachu.recordId, loaded));
  CHECK(loaded == pikachu);
}

void laterAppendStreamsExistingRecordsIntoTheInactiveSlot() {
  Storage.clear();
  pokemon::PokemonStore store;
  CHECK(store.begin() == pokemon::StoreBeginResult::Empty);
  const pokemon::PokemonRecord pikachu = starterPikachu();
  pokemon::PokemonState state{};
  state.partyRecordIds[0] = pikachu.recordId;
  CHECK(pokemon::markSpecies(state.seenSpecies, pikachu.speciesId));
  CHECK(pokemon::markSpecies(state.caughtSpecies, pikachu.speciesId));
  pokemon::RecordMutation mutation{pikachu.recordId, pikachu, pokemon::RecordMutationKind::Append};
  CHECK(store.commit(state, mutation));

  const pokemon::PokemonRecord bulbasaur = caughtBulbasaur();
  state.partyRecordIds[1] = bulbasaur.recordId;
  CHECK(pokemon::markSpecies(state.seenSpecies, bulbasaur.speciesId));
  CHECK(pokemon::markSpecies(state.caughtSpecies, bulbasaur.speciesId));
  mutation = {bulbasaur.recordId, bulbasaur, pokemon::RecordMutationKind::Append};
  CHECK(store.commit(state, mutation));

  pokemon::PokemonStore reopened;
  CHECK(reopened.begin() == pokemon::StoreBeginResult::Ready);
  pokemon::PokemonRecord loaded{};
  CHECK(reopened.readRecord(pikachu.recordId, loaded));
  CHECK(loaded == pikachu);
  CHECK(reopened.readRecord(bulbasaur.recordId, loaded));
  CHECK(loaded == bulbasaur);
  pokemon::PokemonState loadedState{};
  CHECK(reopened.loadState(loadedState));
  CHECK(loadedState.sequence == 2);
}

void replacementChangesOnlyTheRequestedRecord() {
  Storage.clear();
  pokemon::PokemonStore store;
  CHECK(store.begin() == pokemon::StoreBeginResult::Empty);
  pokemon::PokemonRecord pikachu = starterPikachu();
  const pokemon::PokemonRecord bulbasaur = caughtBulbasaur();
  pokemon::PokemonState state{};
  state.partyRecordIds[0] = pikachu.recordId;
  CHECK(pokemon::markSpecies(state.seenSpecies, pikachu.speciesId));
  CHECK(pokemon::markSpecies(state.caughtSpecies, pikachu.speciesId));
  pokemon::RecordMutation mutation{pikachu.recordId, pikachu, pokemon::RecordMutationKind::Append};
  CHECK(store.commit(state, mutation));
  state.partyRecordIds[1] = bulbasaur.recordId;
  CHECK(pokemon::markSpecies(state.seenSpecies, bulbasaur.speciesId));
  CHECK(pokemon::markSpecies(state.caughtSpecies, bulbasaur.speciesId));
  mutation = {bulbasaur.recordId, bulbasaur, pokemon::RecordMutationKind::Append};
  CHECK(store.commit(state, mutation));

  pikachu.totalXp = 67;
  CHECK(pokemon::setNickname(pikachu, "Sparky"));
  state.lifetimeMinutes = 15;
  mutation = {pikachu.recordId, pikachu, pokemon::RecordMutationKind::Replace};
  CHECK(store.commit(state, mutation));

  pokemon::PokemonStore reopened;
  CHECK(reopened.begin() == pokemon::StoreBeginResult::Ready);
  pokemon::PokemonRecord loaded{};
  CHECK(reopened.readRecord(pikachu.recordId, loaded));
  CHECK(loaded == pikachu);
  CHECK(reopened.readRecord(bulbasaur.recordId, loaded));
  CHECK(loaded == bulbasaur);
  pokemon::PokemonState loadedState{};
  CHECK(reopened.loadState(loadedState));
  CHECK(loadedState.sequence == 3);
  CHECK(loadedState.lifetimeMinutes == 15);
}

void pcPagesExcludeThePartyAndSupportAllThreeOrders() {
  Storage.clear();
  pokemon::PokemonStore store;
  CHECK(store.begin() == pokemon::StoreBeginResult::Empty);
  const pokemon::PokemonRecord pikachu = starterPikachu();
  const pokemon::PokemonRecord abra = caughtAbra(2);
  const pokemon::PokemonRecord bulbasaur = caughtBulbasaur(3);
  pokemon::PokemonState state{};
  state.partyRecordIds[0] = pikachu.recordId;
  for (const pokemon::PokemonRecord& record : {pikachu, abra, bulbasaur}) {
    CHECK(pokemon::markSpecies(state.seenSpecies, record.speciesId));
    CHECK(pokemon::markSpecies(state.caughtSpecies, record.speciesId));
    pokemon::RecordMutation mutation{record.recordId, record, pokemon::RecordMutationKind::Append};
    CHECK(store.commit(state, mutation));
  }

  std::array<pokemon::PokemonRecord, 2> page{};
  std::array<pokemon::PokemonRecord, 1> second{};
  CHECK(store.readPcPage(pokemon::PcOrder::CatchDate, 0, page) == 2);
  CHECK(page[0] == abra);
  CHECK(page[1] == bulbasaur);
  CHECK(store.readPcPage(pokemon::PcOrder::PokedexNumber, 0, page) == 2);
  CHECK(page[0] == bulbasaur);
  CHECK(page[1] == abra);
  CHECK(store.readPcPage(pokemon::PcOrder::PokedexNumber, 1, second) == 1);
  CHECK(second[0] == abra);
  CHECK(store.readPcPage(pokemon::PcOrder::Alphabetical, 0, page) == 2);
  CHECK(page[0] == abra);
  CHECK(page[1] == bulbasaur);
  CHECK(store.readPcPage(pokemon::PcOrder::Alphabetical, 1, second) == 1);
  CHECK(second[0] == bulbasaur);
  CHECK(store.readPcPage(pokemon::PcOrder::CatchDate, 1, second) == 1);
  CHECK(second[0] == bulbasaur);
}

void resetRemovesBothSnapshotsAndClearsReadiness() {
  Storage.clear();
  pokemon::PokemonStore store;
  CHECK(store.begin() == pokemon::StoreBeginResult::Empty);
  pokemon::PokemonState state{};
  CHECK(store.commit(state));
  state.lifetimeMinutes = 1;
  CHECK(store.commit(state));
  CHECK(store.reset());
  CHECK(!Storage.exists("/.crosspoint/pokemon-v2-a.bin"));
  CHECK(!Storage.exists("/.crosspoint/pokemon-v2-b.bin"));
  pokemon::PokemonState loaded{};
  CHECK(!store.loadState(loaded));
  pokemon::PokemonStore reopened;
  CHECK(reopened.begin() == pokemon::StoreBeginResult::Empty);
}

void everyInterruptedWriteByteLeavesThePreviousSnapshotBootable() {
  constexpr size_t oneRecordSnapshotBytes = 172;
  for (size_t cut = 0; cut < oneRecordSnapshotBytes; ++cut) {
    Storage.clear();
    pokemon::PokemonStore store;
    CHECK(store.begin() == pokemon::StoreBeginResult::Empty);
    const pokemon::PokemonRecord pikachu = starterPikachu();
    pokemon::PokemonState state{};
    state.partyRecordIds[0] = pikachu.recordId;
    CHECK(pokemon::markSpecies(state.seenSpecies, pikachu.speciesId));
    CHECK(pokemon::markSpecies(state.caughtSpecies, pikachu.speciesId));
    const pokemon::RecordMutation append{pikachu.recordId, pikachu, pokemon::RecordMutationKind::Append};
    CHECK(store.commit(state, append));

    pokemon::PokemonState updated = state;
    updated.lifetimeMinutes = 99;
    Storage.setWriteLimit(cut);
    CHECK(!store.commit(updated));
    Storage.clearWriteLimit();

    pokemon::PokemonStore reopened;
    CHECK(reopened.begin() == pokemon::StoreBeginResult::Ready);
    pokemon::PokemonState loaded{};
    CHECK(reopened.loadState(loaded));
    CHECK(loaded.sequence == 1);
    CHECK(loaded.lifetimeMinutes == 0);
    pokemon::PokemonRecord loadedRecord{};
    CHECK(reopened.readRecord(pikachu.recordId, loadedRecord));
    CHECK(loadedRecord == pikachu);
  }
}

void unwritableAppendPreservesThePendingEncounterAndRoster() {
  Storage.clear();
  pokemon::PokemonStore store;
  CHECK(store.begin() == pokemon::StoreBeginResult::Empty);
  const pokemon::PokemonRecord pikachu = starterPikachu();
  const pokemon::PokemonRecord bulbasaur = caughtBulbasaur();
  pokemon::PokemonState state{};
  state.partyRecordIds[0] = pikachu.recordId;
  CHECK(pokemon::markSpecies(state.seenSpecies, pikachu.speciesId));
  CHECK(pokemon::markSpecies(state.caughtSpecies, pikachu.speciesId));
  state.pending.kind = pokemon::PendingEventKind::Encounter;
  state.pending.speciesId = bulbasaur.speciesId;
  state.pending.level = bulbasaur.caughtLevel;
  state.pending.gender = bulbasaur.gender;
  CHECK(pokemon::markSpecies(state.seenSpecies, bulbasaur.speciesId));
  const pokemon::RecordMutation starter{pikachu.recordId, pikachu, pokemon::RecordMutationKind::Append};
  CHECK(store.commit(state, starter));

  pokemon::PokemonState caught = state;
  caught.pending = {};
  caught.dashboardNotice = pokemon::DashboardNotice::None;
  caught.partyRecordIds[1] = bulbasaur.recordId;
  CHECK(pokemon::markSpecies(caught.caughtSpecies, bulbasaur.speciesId));
  const pokemon::RecordMutation append{bulbasaur.recordId, bulbasaur, pokemon::RecordMutationKind::Append};
  Storage.setFailWritableOpen(true);
  CHECK(!store.commit(caught, append));
  Storage.setFailWritableOpen(false);

  pokemon::PokemonStore reopened;
  CHECK(reopened.begin() == pokemon::StoreBeginResult::Ready);
  pokemon::PokemonState loaded{};
  CHECK(reopened.loadState(loaded));
  CHECK(loaded.pending == state.pending);
  CHECK(loaded.partyRecordIds[1] == 0);
  pokemon::PokemonRecord missing{};
  CHECK(!reopened.readRecord(bulbasaur.recordId, missing));
}

void interruptedAppendPreservesThePendingEncounterAndRoster() {
  Storage.clear();
  pokemon::PokemonStore store;
  CHECK(store.begin() == pokemon::StoreBeginResult::Empty);
  const pokemon::PokemonRecord pikachu = starterPikachu();
  const pokemon::PokemonRecord bulbasaur = caughtBulbasaur();
  pokemon::PokemonState state{};
  state.partyRecordIds[0] = pikachu.recordId;
  CHECK(pokemon::markSpecies(state.seenSpecies, pikachu.speciesId));
  CHECK(pokemon::markSpecies(state.caughtSpecies, pikachu.speciesId));
  state.pending.kind = pokemon::PendingEventKind::Encounter;
  state.pending.speciesId = bulbasaur.speciesId;
  state.pending.level = bulbasaur.caughtLevel;
  state.pending.gender = bulbasaur.gender;
  CHECK(pokemon::markSpecies(state.seenSpecies, bulbasaur.speciesId));
  CHECK(store.commit(state, {pikachu.recordId, pikachu, pokemon::RecordMutationKind::Append}));

  pokemon::PokemonState caught = state;
  caught.pending = {};
  caught.partyRecordIds[1] = bulbasaur.recordId;
  CHECK(pokemon::markSpecies(caught.caughtSpecies, bulbasaur.speciesId));
  Storage.setWriteLimit(180);  // Ten bytes into the appended record.
  CHECK(!store.commit(caught, {bulbasaur.recordId, bulbasaur, pokemon::RecordMutationKind::Append}));
  Storage.clearWriteLimit();

  pokemon::PokemonStore reopened;
  CHECK(reopened.begin() == pokemon::StoreBeginResult::Ready);
  pokemon::PokemonState loaded{};
  CHECK(reopened.loadState(loaded));
  CHECK(loaded.pending == state.pending);
  CHECK(loaded.partyRecordIds[1] == 0);
  pokemon::PokemonRecord missing{};
  CHECK(!reopened.readRecord(bulbasaur.recordId, missing));
}

void failedSyncKeepsTheLiveStoreOldButAllowsACompleteSnapshotAfterRestart() {
  Storage.clear();
  pokemon::PokemonStore store;
  CHECK(store.begin() == pokemon::StoreBeginResult::Empty);
  pokemon::PokemonState state{};
  state.lifetimeMinutes = 10;
  CHECK(store.commit(state));

  state.lifetimeMinutes = 20;
  Storage.setFailSync(true);
  CHECK(!store.commit(state));
  Storage.setFailSync(false);

  pokemon::PokemonState loaded{};
  CHECK(store.loadState(loaded));
  CHECK(loaded.sequence == 1);
  CHECK(loaded.lifetimeMinutes == 10);

  pokemon::PokemonStore reopened;
  CHECK(reopened.begin() == pokemon::StoreBeginResult::Ready);
  CHECK(reopened.loadState(loaded));
  CHECK(loaded.sequence == 2);
  CHECK(loaded.lifetimeMinutes == 20);
}

void startupClassifiesCorruptAndUnsupportedSnapshotsAndFallsBackToValidOlderData() {
  Storage.clear();
  pokemon::PokemonStore store;
  CHECK(store.begin() == pokemon::StoreBeginResult::Empty);
  pokemon::PokemonState state{};
  state.lifetimeMinutes = 10;
  CHECK(store.commit(state));
  Storage.setByte("/.crosspoint/pokemon-v2-a.bin", 24 + 84, 0xFF);
  pokemon::PokemonStore corrupt;
  CHECK(corrupt.begin() == pokemon::StoreBeginResult::Corrupt);

  Storage.clear();
  CHECK(store.begin() == pokemon::StoreBeginResult::Empty);
  CHECK(store.commit(state));
  Storage.setByte("/.crosspoint/pokemon-v2-a.bin", 4, 2);
  pokemon::PokemonStore unsupported;
  CHECK(unsupported.begin() == pokemon::StoreBeginResult::Unsupported);

  Storage.clear();
  CHECK(store.begin() == pokemon::StoreBeginResult::Empty);
  CHECK(store.commit(state));
  state.lifetimeMinutes = 20;
  CHECK(store.commit(state));
  Storage.setByte("/.crosspoint/pokemon-v2-b.bin", 24 + 84, 0xFF);
  pokemon::PokemonStore fallback;
  CHECK(fallback.begin() == pokemon::StoreBeginResult::Ready);
  pokemon::PokemonState loaded{};
  CHECK(fallback.loadState(loaded));
  CHECK(loaded.sequence == 1);
  CHECK(loaded.lifetimeMinutes == 10);
}

void startupBlocksDowngradesAndMixedUnsupportedCorruption() {
  Storage.clear();
  pokemon::PokemonStore store;
  CHECK(store.begin() == pokemon::StoreBeginResult::Empty);
  pokemon::PokemonState state{};
  state.lifetimeMinutes = 10;
  CHECK(store.commit(state));
  state.lifetimeMinutes = 20;
  CHECK(store.commit(state));
  Storage.setByte("/.crosspoint/pokemon-v2-b.bin", 4, 2);

  pokemon::PokemonStore downgraded;
  CHECK(downgraded.begin() == pokemon::StoreBeginResult::Unsupported);
  CHECK(!downgraded.commit(state));

  Storage.setByte("/.crosspoint/pokemon-v2-a.bin", 24 + 84, 0xFF);
  pokemon::PokemonStore mixed;
  CHECK(mixed.begin() == pokemon::StoreBeginResult::Corrupt);
  CHECK(!mixed.commit(state));
}

void failedResetDoesNotBypassProtectedStoreGating() {
  Storage.clear();
  pokemon::PokemonStore store;
  CHECK(store.begin() == pokemon::StoreBeginResult::Empty);
  pokemon::PokemonState state{};
  CHECK(store.commit(state));
  Storage.setByte("/.crosspoint/pokemon-v2-a.bin", 4, 2);

  pokemon::PokemonStore protectedStore;
  CHECK(protectedStore.begin() == pokemon::StoreBeginResult::Unsupported);
  Storage.setFailRemove(true);
  CHECK(!protectedStore.reset());
  Storage.setFailRemove(false);
  CHECK(!protectedStore.commit(state));
  CHECK(Storage.exists("/.crosspoint/pokemon-v2-a.bin"));
}

}  // namespace

int main() {
  emptyStoreCommitsAndReloadsSequenceOne();
  secondCommitUsesSlotBAndWinsStartupSelection();
  initialAppendPersistsAReadableRecord();
  laterAppendStreamsExistingRecordsIntoTheInactiveSlot();
  replacementChangesOnlyTheRequestedRecord();
  pcPagesExcludeThePartyAndSupportAllThreeOrders();
  resetRemovesBothSnapshotsAndClearsReadiness();
  everyInterruptedWriteByteLeavesThePreviousSnapshotBootable();
  unwritableAppendPreservesThePendingEncounterAndRoster();
  interruptedAppendPreservesThePendingEncounterAndRoster();
  failedSyncKeepsTheLiveStoreOldButAllowsACompleteSnapshotAfterRestart();
  startupClassifiesCorruptAndUnsupportedSnapshotsAndFallsBackToValidOlderData();
  startupBlocksDowngradesAndMixedUnsupportedCorruption();
  failedResetDoesNotBypassProtectedStoreGating();
  return failures == 0 ? 0 : 1;
}
