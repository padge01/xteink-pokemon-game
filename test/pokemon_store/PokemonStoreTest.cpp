#include <HalStorage.h>

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "pokemon/PokemonStore.h"

namespace {

int failures = 0;

#define CHECK(condition)                                                                \
  do {                                                                                  \
    if (!(condition)) {                                                                 \
      std::fprintf(stderr, "%s:%d check failed: %s\n", __FILE__, __LINE__, #condition); \
      ++failures;                                                                       \
    }                                                                                   \
  } while (false)

void write16(uint8_t* bytes, const size_t offset, const uint16_t value) {
  bytes[offset] = static_cast<uint8_t>(value);
  bytes[offset + 1] = static_cast<uint8_t>(value >> 8U);
}

void write32(uint8_t* bytes, const size_t offset, const uint32_t value) {
  bytes[offset] = static_cast<uint8_t>(value);
  bytes[offset + 1] = static_cast<uint8_t>(value >> 8U);
  bytes[offset + 2] = static_cast<uint8_t>(value >> 16U);
  bytes[offset + 3] = static_cast<uint8_t>(value >> 24U);
}

bool encodeLegacyState(const pokemon::PokemonState& state,
                       std::array<uint8_t, pokemon::POKEMON_STATE_V1_BYTES>& output) {
  if (!pokemon::validateState(state) || pokemon::pendingEventCount(state) > 1) return false;
  output = {};
  for (size_t slot = 0; slot < pokemon::PARTY_SIZE; ++slot) write32(output.data(), slot * 4U, state.partyRecordIds[slot]);
  const pokemon::PendingEvent* pending = pokemon::pendingEventFront(state);
  if (pending != nullptr) {
    write32(output.data(), 24, pending->recordId);
    write16(output.data(), 28, pending->speciesId);
    output[30] = pending->level;
    output[31] = static_cast<uint8_t>(pending->gender);
    output[32] = static_cast<uint8_t>(pending->item);
    output[33] = static_cast<uint8_t>(pending->kind);
  }
  for (size_t index = 0; index < pokemon::EVOLUTION_ITEM_COUNT; ++index) {
    write16(output.data(), 34U + index * 2U, state.itemCounts[index]);
  }
  std::memcpy(output.data() + 46, state.seenSpecies.data(), state.seenSpecies.size());
  std::memcpy(output.data() + 65, state.caughtSpecies.data(), state.caughtSpecies.size());
  write32(output.data(), 84, state.lifetimeMinutes);
  write32(output.data(), 88, state.sequence);
  output[92] = state.readingMinuteRemainder;
  output[93] = state.encounterMisses;
  output[94] = state.itemMisses;
  output[95] = static_cast<uint8_t>(state.dashboardNotice);
  return true;
}

bool writeLegacySnapshot(const char* path, const pokemon::PokemonState& state,
                         const pokemon::PokemonRecord& record) {
  pokemon::HeaderBytes headerBytes{};
  const pokemon::SnapshotHeader header{pokemon::POKEMON_SNAPSHOT_VERSION_V1, state.sequence, 1};
  std::array<uint8_t, pokemon::POKEMON_STATE_V1_BYTES> stateBytes{};
  pokemon::RecordBytes recordBytes{};
  if (!pokemon::encodeSnapshotHeader(header, headerBytes) || !encodeLegacyState(state, stateBytes) ||
      !pokemon::encodeRecord(record, recordBytes)) {
    return false;
  }
  uint32_t crc = pokemon::updateSnapshotCrc32(pokemon::POKEMON_SNAPSHOT_CRC32_INITIAL, headerBytes.data(),
                                               headerBytes.size());
  crc = pokemon::updateSnapshotCrc32(crc, stateBytes.data(), stateBytes.size());
  crc = pokemon::updateSnapshotCrc32(crc, recordBytes.data(), recordBytes.size());
  uint8_t crcBytes[pokemon::POKEMON_SNAPSHOT_CRC_BYTES]{};
  write32(crcBytes, 0, pokemon::finishSnapshotCrc32(crc));

  FsFile file = Storage.open(path, O_WRONLY | O_CREAT | O_TRUNC);
  if (!file) return false;
  const bool written = file.write(headerBytes.data(), headerBytes.size()) == headerBytes.size() &&
                       file.write(stateBytes.data(), stateBytes.size()) == stateBytes.size() &&
                       file.write(recordBytes.data(), recordBytes.size()) == recordBytes.size() &&
                       file.write(crcBytes, sizeof(crcBytes)) == sizeof(crcBytes);
  return written && file.close();
}

bool readStoredHeader(const char* path, pokemon::SnapshotHeader& output) {
  FsFile file = Storage.open(path, O_RDONLY);
  pokemon::HeaderBytes bytes{};
  if (!file || file.read(bytes.data(), bytes.size()) != static_cast<int>(bytes.size())) {
    file.close();
    return false;
  }
  file.close();
  return pokemon::decodeSnapshotHeader(bytes, output) == pokemon::HeaderDecodeResult::Ready;
}

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
  size_t count = 0;
  CHECK(store.readPcPage(pokemon::PcOrder::CatchDate, 0, page, count));
  CHECK(count == 2);
  CHECK(page[0] == abra);
  CHECK(page[1] == bulbasaur);
  CHECK(store.readPcPage(pokemon::PcOrder::PokedexNumber, 0, page, count));
  CHECK(count == 2);
  CHECK(page[0] == bulbasaur);
  CHECK(page[1] == abra);
  CHECK(store.readPcPage(pokemon::PcOrder::PokedexNumber, 1, second, count));
  CHECK(count == 1);
  CHECK(second[0] == abra);
  CHECK(store.readPcPage(pokemon::PcOrder::Alphabetical, 0, page, count));
  CHECK(count == 2);
  CHECK(page[0] == abra);
  CHECK(page[1] == bulbasaur);
  CHECK(store.readPcPage(pokemon::PcOrder::Alphabetical, 1, second, count));
  CHECK(count == 1);
  CHECK(second[0] == bulbasaur);
  CHECK(store.readPcPage(pokemon::PcOrder::CatchDate, 1, second, count));
  CHECK(count == 1);
  CHECK(second[0] == bulbasaur);
}

void pcReadFailureIsDistinctFromAValidEmptyPage() {
  Storage.clear();
  pokemon::PokemonStore store;
  CHECK(store.begin() == pokemon::StoreBeginResult::Empty);
  const pokemon::PokemonRecord pikachu = starterPikachu();
  pokemon::PokemonState state{};
  state.partyRecordIds[0] = pikachu.recordId;
  CHECK(pokemon::markSpecies(state.seenSpecies, pikachu.speciesId));
  CHECK(pokemon::markSpecies(state.caughtSpecies, pikachu.speciesId));
  CHECK(store.commit(state, {pikachu.recordId, pikachu, pokemon::RecordMutationKind::Append}));

  std::array<pokemon::PokemonRecord, 2> page{};
  size_t count = 99;
  CHECK(store.readPcPage(pokemon::PcOrder::CatchDate, 0, page, count));
  CHECK(count == 0);

  Storage.setFailRead(true);
  count = 99;
  CHECK(!store.readPcPage(pokemon::PcOrder::CatchDate, 0, page, count));
  CHECK(count == 0);
  Storage.setFailRead(false);
}

void resetCommitsANewerEmptySnapshot() {
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
  state.lifetimeMinutes = 1;
  CHECK(store.commit(state));

  CHECK(store.reset());
  CHECK(Storage.exists("/.crosspoint/pokemon-v2-a.bin"));
  CHECK(Storage.exists("/.crosspoint/pokemon-v2-b.bin"));
  CHECK(store.isReady());
  CHECK(store.recordCount() == 0);
  pokemon::PokemonState loaded{};
  CHECK(store.loadState(loaded));
  CHECK(loaded.sequence == 3);
  CHECK(loaded.partyRecordIds[0] == 0);
  CHECK(loaded.lifetimeMinutes == 0);
  CHECK(!pokemon::isSpeciesMarked(loaded.caughtSpecies, pikachu.speciesId));

  pokemon::PokemonStore reopened;
  CHECK(reopened.begin() == pokemon::StoreBeginResult::Ready);
  CHECK(reopened.recordCount() == 0);
  pokemon::PokemonState reopenedState{};
  CHECK(reopened.loadState(reopenedState));
  CHECK(reopenedState == loaded);
}

void everyInterruptedResetByteLeavesThePreviousSnapshotBootable() {
  constexpr size_t emptySnapshotBytes =
      pokemon::POKEMON_SNAPSHOT_HEADER_BYTES + pokemon::POKEMON_STATE_BYTES + pokemon::POKEMON_SNAPSHOT_CRC_BYTES;
  for (size_t cut = 0; cut < emptySnapshotBytes; ++cut) {
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

    Storage.setWriteLimit(cut);
    CHECK(!store.reset());
    Storage.clearWriteLimit();

    pokemon::PokemonStore reopened;
    CHECK(reopened.begin() == pokemon::StoreBeginResult::Ready);
    CHECK(reopened.recordCount() == 1);
    pokemon::PokemonState loaded{};
    CHECK(reopened.loadState(loaded));
    CHECK(loaded.partyRecordIds[0] == pikachu.recordId);
    CHECK(pokemon::isSpeciesMarked(loaded.caughtSpecies, pikachu.speciesId));
    pokemon::PokemonRecord loadedRecord{};
    CHECK(reopened.readRecord(pikachu.recordId, loadedRecord));
    CHECK(loadedRecord == pikachu);
  }
}

void everyInterruptedWriteByteLeavesThePreviousSnapshotBootable() {
  constexpr size_t oneRecordSnapshotBytes = pokemon::POKEMON_SNAPSHOT_HEADER_BYTES + pokemon::POKEMON_STATE_BYTES +
                                            pokemon::POKEMON_RECORD_BYTES + pokemon::POKEMON_SNAPSHOT_CRC_BYTES;
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
  state.pendingEvents[0].kind = pokemon::PendingEventKind::Encounter;
  state.pendingEvents[0].speciesId = bulbasaur.speciesId;
  state.pendingEvents[0].level = bulbasaur.caughtLevel;
  state.pendingEvents[0].gender = bulbasaur.gender;
  CHECK(pokemon::markSpecies(state.seenSpecies, bulbasaur.speciesId));
  const pokemon::RecordMutation starter{pikachu.recordId, pikachu, pokemon::RecordMutationKind::Append};
  CHECK(store.commit(state, starter));

  pokemon::PokemonState caught = state;
  caught.pendingEvents = {};
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
  CHECK(loaded.pendingEvents == state.pendingEvents);
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
  state.pendingEvents[0].kind = pokemon::PendingEventKind::Encounter;
  state.pendingEvents[0].speciesId = bulbasaur.speciesId;
  state.pendingEvents[0].level = bulbasaur.caughtLevel;
  state.pendingEvents[0].gender = bulbasaur.gender;
  CHECK(pokemon::markSpecies(state.seenSpecies, bulbasaur.speciesId));
  CHECK(store.commit(state, {pikachu.recordId, pikachu, pokemon::RecordMutationKind::Append}));

  pokemon::PokemonState caught = state;
  caught.pendingEvents = {};
  caught.partyRecordIds[1] = bulbasaur.recordId;
  CHECK(pokemon::markSpecies(caught.caughtSpecies, bulbasaur.speciesId));
  Storage.setWriteLimit(200);  // Twelve bytes into the appended record.
  CHECK(!store.commit(caught, {bulbasaur.recordId, bulbasaur, pokemon::RecordMutationKind::Append}));
  Storage.clearWriteLimit();

  pokemon::PokemonStore reopened;
  CHECK(reopened.begin() == pokemon::StoreBeginResult::Ready);
  pokemon::PokemonState loaded{};
  CHECK(reopened.loadState(loaded));
  CHECK(loaded.pendingEvents == state.pendingEvents);
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

void legacySnapshotMigratesToV2AndRemainsTheCorruptionFallback() {
  Storage.clear();
  const pokemon::PokemonRecord pikachu = starterPikachu();
  pokemon::PokemonState legacy{};
  legacy.partyRecordIds[0] = pikachu.recordId;
  legacy.sequence = 7;
  legacy.lifetimeMinutes = 123;
  legacy.pendingEvents[0] = {0, 1, 8, pokemon::Gender::Female, pokemon::EvolutionItem::None,
                             pokemon::PendingEventKind::Encounter};
  legacy.dashboardNotice = pokemon::DashboardNotice::NewPokemon;
  CHECK(pokemon::markSpecies(legacy.seenSpecies, pikachu.speciesId));
  CHECK(pokemon::markSpecies(legacy.caughtSpecies, pikachu.speciesId));
  CHECK(pokemon::markSpecies(legacy.seenSpecies, 1));
  CHECK(writeLegacySnapshot("/.crosspoint/pokemon-v2-a.bin", legacy, pikachu));

  pokemon::PokemonStore store;
  CHECK(store.begin() == pokemon::StoreBeginResult::Ready);
  pokemon::PokemonState loaded{};
  CHECK(store.loadState(loaded));
  CHECK(loaded == legacy);
  pokemon::PokemonRecord loadedRecord{};
  CHECK(store.readRecord(pikachu.recordId, loadedRecord));
  CHECK(loadedRecord == pikachu);

  CHECK(store.commit(loaded));
  pokemon::SnapshotHeader migratedHeader{};
  CHECK(readStoredHeader("/.crosspoint/pokemon-v2-b.bin", migratedHeader));
  CHECK(migratedHeader.version == pokemon::POKEMON_SNAPSHOT_VERSION);
  CHECK(migratedHeader.sequence == 8);

  pokemon::PokemonStore migrated;
  CHECK(migrated.begin() == pokemon::StoreBeginResult::Ready);
  CHECK(migrated.loadState(loaded));
  CHECK(loaded.sequence == 8);
  CHECK(loaded.lifetimeMinutes == legacy.lifetimeMinutes);
  CHECK(loaded.pendingEvents == legacy.pendingEvents);
  CHECK(migrated.readRecord(pikachu.recordId, loadedRecord));
  CHECK(loadedRecord == pikachu);

  Storage.setByte("/.crosspoint/pokemon-v2-b.bin", pokemon::POKEMON_SNAPSHOT_HEADER_BYTES + 104, 0xFF);
  pokemon::PokemonStore fallback;
  CHECK(fallback.begin() == pokemon::StoreBeginResult::Ready);
  CHECK(fallback.loadState(loaded));
  CHECK(loaded == legacy);
  CHECK(fallback.readRecord(pikachu.recordId, loadedRecord));
  CHECK(loadedRecord == pikachu);
}

void startupClassifiesCorruptAndUnsupportedSnapshotsAndFallsBackToValidOlderData() {
  Storage.clear();
  pokemon::PokemonStore store;
  CHECK(store.begin() == pokemon::StoreBeginResult::Empty);
  pokemon::PokemonState state{};
  state.lifetimeMinutes = 10;
  CHECK(store.commit(state));
  Storage.setByte("/.crosspoint/pokemon-v2-a.bin", 24 + 104, 0xFF);
  pokemon::PokemonStore corrupt;
  CHECK(corrupt.begin() == pokemon::StoreBeginResult::Corrupt);

  Storage.clear();
  CHECK(store.begin() == pokemon::StoreBeginResult::Empty);
  CHECK(store.commit(state));
  Storage.setByte("/.crosspoint/pokemon-v2-a.bin", 4, 3);
  pokemon::PokemonStore unsupported;
  CHECK(unsupported.begin() == pokemon::StoreBeginResult::Unsupported);

  Storage.clear();
  CHECK(store.begin() == pokemon::StoreBeginResult::Empty);
  CHECK(store.commit(state));
  state.lifetimeMinutes = 20;
  CHECK(store.commit(state));
  Storage.setByte("/.crosspoint/pokemon-v2-b.bin", 24 + 104, 0xFF);
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
  Storage.setByte("/.crosspoint/pokemon-v2-b.bin", 4, 3);

  pokemon::PokemonStore downgraded;
  CHECK(downgraded.begin() == pokemon::StoreBeginResult::Unsupported);
  CHECK(!downgraded.commit(state));

  Storage.setByte("/.crosspoint/pokemon-v2-a.bin", 24 + 104, 0xFF);
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
  Storage.setByte("/.crosspoint/pokemon-v2-a.bin", 4, 3);

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
  pcReadFailureIsDistinctFromAValidEmptyPage();
  resetCommitsANewerEmptySnapshot();
  everyInterruptedResetByteLeavesThePreviousSnapshotBootable();
  everyInterruptedWriteByteLeavesThePreviousSnapshotBootable();
  unwritableAppendPreservesThePendingEncounterAndRoster();
  interruptedAppendPreservesThePendingEncounterAndRoster();
  failedSyncKeepsTheLiveStoreOldButAllowsACompleteSnapshotAfterRestart();
  legacySnapshotMigratesToV2AndRemainsTheCorruptionFallback();
  startupClassifiesCorruptAndUnsupportedSnapshotsAndFallsBackToValidOlderData();
  startupBlocksDowngradesAndMixedUnsupportedCorruption();
  failedResetDoesNotBypassProtectedStoreGating();
  return failures == 0 ? 0 : 1;
}
