#include <HalStorage.h>

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <span>

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

constexpr const char* PATH_A = "/.crosspoint/pokemon-a.bin";
constexpr const char* PATH_B = "/.crosspoint/pokemon-b.bin";
constexpr const char* BACKUP_PATH = "/.crosspoint/pokemon-before-upgrade.bin";
// A snapshot version newer than this build knows.
constexpr uint8_t UNKNOWN_VERSION = pokemon::POKEMON_SNAPSHOT_VERSION + 1U;

// Encodes a state the way snapshot version 1 or 2 stored it: one or three waiting events, and
// none of what version 3 added.
bool encodeOlderState(const uint16_t version, const pokemon::PokemonState& state,
                      std::array<uint8_t, pokemon::POKEMON_STATE_V2_BYTES>& output, size_t& size) {
  const size_t eventCount = version == pokemon::POKEMON_SNAPSHOT_VERSION_V1 ? 1U : 3U;
  if (!pokemon::validateState(state) || pokemon::pendingEventCount(state) > eventCount) return false;
  output = {};
  for (size_t slot = 0; slot < pokemon::PARTY_SIZE; ++slot)
    write32(output.data(), slot * 4U, state.partyRecordIds[slot]);
  for (size_t index = 0; index < eventCount; ++index) {
    const pokemon::PendingEvent& pending = state.pendingEvents[index];
    uint8_t* bytes = output.data() + 24U + index * 10U;
    write32(bytes, 0, pending.recordId);
    write16(bytes, 4, pending.speciesId);
    bytes[6] = pending.level;
    bytes[7] = static_cast<uint8_t>(pending.gender);
    bytes[8] = static_cast<uint8_t>(pending.item);
    bytes[9] = static_cast<uint8_t>(pending.kind);
  }
  uint8_t* rest = output.data() + 24U + eventCount * 10U;
  for (size_t index = 0; index < pokemon::EVOLUTION_ITEM_COUNT; ++index) {
    write16(rest, index * 2U, state.itemCounts[index]);
  }
  std::memcpy(rest + 12, state.seenSpecies.data(), state.seenSpecies.size());
  std::memcpy(rest + 31, state.caughtSpecies.data(), state.caughtSpecies.size());
  write32(rest, 50, state.lifetimeMinutes);
  write32(rest, 54, state.sequence);
  rest[58] = state.readingMinuteRemainder;
  rest[59] = state.encounterMisses;
  rest[60] = state.itemMisses;
  rest[61] = static_cast<uint8_t>(state.dashboardNotice);
  size = pokemon::snapshotStateBytes(version);
  return true;
}

// Writes a complete snapshot as version 1 or 2 did, with its 48-byte records.
bool writeOlderSnapshot(const char* path, const uint16_t version, const pokemon::PokemonState& state,
                        const std::span<const pokemon::PokemonRecord> records) {
  pokemon::HeaderBytes headerBytes{};
  const pokemon::SnapshotHeader header{version, state.sequence, static_cast<uint32_t>(records.size())};
  std::array<uint8_t, pokemon::POKEMON_STATE_V2_BYTES> stateBytes{};
  size_t stateSize = 0;
  if (!pokemon::encodeSnapshotHeader(header, headerBytes) || !encodeOlderState(version, state, stateBytes, stateSize)) {
    return false;
  }
  FsFile file = Storage.open(path, O_WRONLY | O_CREAT | O_TRUNC);
  if (!file) return false;
  uint32_t crc =
      pokemon::updateSnapshotCrc32(pokemon::POKEMON_SNAPSHOT_CRC32_INITIAL, headerBytes.data(), headerBytes.size());
  crc = pokemon::updateSnapshotCrc32(crc, stateBytes.data(), stateSize);
  bool written = file.write(headerBytes.data(), headerBytes.size()) == headerBytes.size() &&
                 file.write(stateBytes.data(), stateSize) == stateSize;
  for (const pokemon::PokemonRecord& record : records) {
    // An older record is the first 48 bytes of the current one.
    pokemon::RecordBytes recordBytes{};
    written =
        written && pokemon::encodeRecord(record, recordBytes) &&
        file.write(recordBytes.data(), pokemon::POKEMON_LEGACY_RECORD_BYTES) == pokemon::POKEMON_LEGACY_RECORD_BYTES;
    crc = pokemon::updateSnapshotCrc32(crc, recordBytes.data(), pokemon::POKEMON_LEGACY_RECORD_BYTES);
  }
  uint8_t crcBytes[pokemon::POKEMON_SNAPSHOT_CRC_BYTES]{};
  write32(crcBytes, 0, pokemon::finishSnapshotCrc32(crc));
  written = written && file.write(crcBytes, sizeof(crcBytes)) == sizeof(crcBytes);
  return file.close() && written;
}

bool writeLegacySnapshot(const char* path, const pokemon::PokemonState& state, const pokemon::PokemonRecord& record) {
  return writeOlderSnapshot(path, pokemon::POKEMON_SNAPSHOT_VERSION_V1, state,
                            std::span<const pokemon::PokemonRecord>(&record, 1));
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
  CHECK(Storage.exists("/.crosspoint/pokemon-a.bin"));
  CHECK(!Storage.exists("/.crosspoint/pokemon-b.bin"));

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
  CHECK(Storage.exists("/.crosspoint/pokemon-a.bin"));
  CHECK(Storage.exists("/.crosspoint/pokemon-b.bin"));

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

// A full party: record ids 1-6, all of them in the party. Returns the state that goes with it.
pokemon::PokemonState commitFullParty(pokemon::PokemonStore& store,
                                      std::array<pokemon::PokemonRecord, pokemon::PARTY_SIZE>& party) {
  pokemon::PokemonState state{};
  for (size_t slot = 0; slot < party.size(); ++slot) {
    const uint32_t recordId = static_cast<uint32_t>(slot + 1U);
    party[slot] = slot == 0 ? starterPikachu() : (slot % 2U == 1U ? caughtBulbasaur(recordId) : caughtAbra(recordId));
    state.partyRecordIds[slot] = recordId;
    CHECK(pokemon::markSpecies(state.seenSpecies, party[slot].speciesId));
    CHECK(pokemon::markSpecies(state.caughtSpecies, party[slot].speciesId));
    const pokemon::RecordMutation append{recordId, party[slot], pokemon::RecordMutationKind::Append};
    CHECK(store.commit(state, append));
  }
  return state;
}

void oneCommitReplacesEveryPartyMemberAndAppendsACatch() {
  Storage.clear();
  pokemon::PokemonStore store;
  CHECK(store.begin() == pokemon::StoreBeginResult::Empty);
  std::array<pokemon::PokemonRecord, pokemon::PARTY_SIZE> party{};
  pokemon::PokemonState state = commitFullParty(store, party);

  std::array<pokemon::RecordMutation, pokemon::RECORD_MUTATION_CAPACITY> batch{};
  for (size_t slot = 0; slot < party.size(); ++slot) {
    party[slot].totalXp += static_cast<uint32_t>(slot + 1U);
    batch[slot] = {party[slot].recordId, party[slot], pokemon::RecordMutationKind::Replace};
  }
  const pokemon::PokemonRecord caught = caughtAbra(7);
  batch[pokemon::PARTY_SIZE] = {caught.recordId, caught, pokemon::RecordMutationKind::Append};
  state.lifetimeMinutes = 30;
  CHECK(store.commit(state, batch));

  pokemon::PokemonStore reopened;
  CHECK(reopened.begin() == pokemon::StoreBeginResult::Ready);
  CHECK(reopened.recordCount() == 7);
  pokemon::PokemonRecord loaded{};
  for (const pokemon::PokemonRecord& member : party) {
    CHECK(reopened.readRecord(member.recordId, loaded));
    CHECK(loaded == member);
  }
  CHECK(reopened.readRecord(caught.recordId, loaded));
  CHECK(loaded == caught);
  pokemon::PokemonState loadedState{};
  CHECK(reopened.loadState(loadedState));
  // Six appends, then one save for the whole batch.
  CHECK(loadedState.sequence == 7);
  CHECK(loadedState.lifetimeMinutes == 30);
}

void batchSkipsUnusedEntries() {
  Storage.clear();
  pokemon::PokemonStore store;
  CHECK(store.begin() == pokemon::StoreBeginResult::Empty);
  std::array<pokemon::PokemonRecord, pokemon::PARTY_SIZE> party{};
  const pokemon::PokemonState state = commitFullParty(store, party);

  std::array<pokemon::RecordMutation, pokemon::RECORD_MUTATION_CAPACITY> batch{};
  party[3].totalXp += 9;
  batch[4] = {party[3].recordId, party[3], pokemon::RecordMutationKind::Replace};
  CHECK(store.commit(state, batch));

  pokemon::PokemonRecord loaded{};
  for (const pokemon::PokemonRecord& member : party) {
    CHECK(store.readRecord(member.recordId, loaded));
    CHECK(loaded == member);
  }
}

void rejectedBatchSavesNothing() {
  Storage.clear();
  pokemon::PokemonStore store;
  CHECK(store.begin() == pokemon::StoreBeginResult::Empty);
  std::array<pokemon::PokemonRecord, pokemon::PARTY_SIZE> party{};
  pokemon::PokemonState state = commitFullParty(store, party);
  state.lifetimeMinutes = 45;

  pokemon::PokemonRecord changed = party[0];
  changed.totalXp += 5;
  const pokemon::RecordMutation replaceFirst{changed.recordId, changed, pokemon::RecordMutationKind::Replace};

  // A record that is not in the save.
  const pokemon::PokemonRecord stranger = caughtAbra(40);
  std::array<pokemon::RecordMutation, 2> unknownRecord{
      replaceFirst, pokemon::RecordMutation{stranger.recordId, stranger, pokemon::RecordMutationKind::Replace}};
  CHECK(!store.commit(state, unknownRecord));

  // The same record twice.
  std::array<pokemon::RecordMutation, 2> sameRecordTwice{replaceFirst, replaceFirst};
  CHECK(!store.commit(state, sameRecordTwice));

  // Two new records.
  const pokemon::PokemonRecord seventh = caughtAbra(7);
  const pokemon::PokemonRecord eighth = caughtAbra(8);
  std::array<pokemon::RecordMutation, 2> twoAppends{
      pokemon::RecordMutation{seventh.recordId, seventh, pokemon::RecordMutationKind::Append},
      pokemon::RecordMutation{eighth.recordId, eighth, pokemon::RecordMutationKind::Append}};
  CHECK(!store.commit(state, twoAppends));

  // More entries than one save takes.
  std::array<pokemon::RecordMutation, pokemon::RECORD_MUTATION_CAPACITY + 1U> tooMany{};
  tooMany[0] = replaceFirst;
  CHECK(!store.commit(state, tooMany));

  pokemon::PokemonStore reopened;
  CHECK(reopened.begin() == pokemon::StoreBeginResult::Ready);
  CHECK(reopened.recordCount() == pokemon::PARTY_SIZE);
  pokemon::PokemonRecord loaded{};
  CHECK(reopened.readRecord(party[0].recordId, loaded));
  CHECK(loaded == party[0]);
  pokemon::PokemonState loadedState{};
  CHECK(reopened.loadState(loadedState));
  CHECK(loadedState.sequence == pokemon::PARTY_SIZE);
  CHECK(loadedState.lifetimeMinutes == 0);
}

void everyInterruptedBatchByteLeavesThePreviousSnapshotBootable() {
  constexpr size_t batchSnapshotBytes = pokemon::POKEMON_SNAPSHOT_HEADER_BYTES + pokemon::POKEMON_STATE_BYTES +
                                        (pokemon::PARTY_SIZE + 1U) * pokemon::POKEMON_RECORD_BYTES +
                                        pokemon::POKEMON_SNAPSHOT_CRC_BYTES;
  for (size_t cut = 0; cut < batchSnapshotBytes; ++cut) {
    Storage.clear();
    pokemon::PokemonStore store;
    CHECK(store.begin() == pokemon::StoreBeginResult::Empty);
    std::array<pokemon::PokemonRecord, pokemon::PARTY_SIZE> party{};
    pokemon::PokemonState state = commitFullParty(store, party);

    std::array<pokemon::RecordMutation, pokemon::RECORD_MUTATION_CAPACITY> batch{};
    for (size_t slot = 0; slot < party.size(); ++slot) {
      pokemon::PokemonRecord changed = party[slot];
      changed.totalXp += 10;
      batch[slot] = {changed.recordId, changed, pokemon::RecordMutationKind::Replace};
    }
    const pokemon::PokemonRecord caught = caughtAbra(7);
    batch[pokemon::PARTY_SIZE] = {caught.recordId, caught, pokemon::RecordMutationKind::Append};
    state.lifetimeMinutes = 30;
    Storage.setWriteLimit(cut);
    CHECK(!store.commit(state, batch));
    Storage.clearWriteLimit();

    // None of the batch is visible: not the new record, not one changed party member.
    pokemon::PokemonStore reopened;
    CHECK(reopened.begin() == pokemon::StoreBeginResult::Ready);
    CHECK(reopened.recordCount() == pokemon::PARTY_SIZE);
    pokemon::PokemonState loadedState{};
    CHECK(reopened.loadState(loadedState));
    CHECK(loadedState.lifetimeMinutes == 0);
    pokemon::PokemonRecord loaded{};
    for (const pokemon::PokemonRecord& member : party) {
      CHECK(reopened.readRecord(member.recordId, loaded));
      CHECK(loaded == member);
    }
    CHECK(!reopened.readRecord(caught.recordId, loaded));
  }
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
  CHECK(Storage.exists("/.crosspoint/pokemon-a.bin"));
  CHECK(Storage.exists("/.crosspoint/pokemon-b.bin"));
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

bool writeGarbage(const char* path) {
  FsFile file = Storage.open(path, O_WRONLY | O_CREAT | O_TRUNC);
  if (!file) return false;
  constexpr uint8_t garbage[] = {'P', 'K', 'M', 0x00, 0x13, 0x37};
  const bool written = file.write(garbage, sizeof(garbage)) == sizeof(garbage);
  return file.close() && written;
}

// What a state from an older version looks like once it has been upgraded and saved.
pokemon::PokemonState upgraded(pokemon::PokemonState older) {
  pokemon::grantStartingItems(older);
  ++older.sequence;
  return older;
}

// A version 2 save as v0.1.1 leaves it: the newer snapshot in slot A, the one before it in slot B.
struct VersionTwoSave {
  std::array<pokemon::PokemonRecord, 3> records{starterPikachu(), caughtBulbasaur(2), caughtAbra(3)};
  pokemon::PokemonState state{};
};

VersionTwoSave writeVersionTwoSave() {
  VersionTwoSave save{};
  save.state.partyRecordIds = {1, 2, 0, 0, 0, 0};
  for (const pokemon::PokemonRecord& record : save.records) {
    CHECK(pokemon::markSpecies(save.state.seenSpecies, record.speciesId));
    CHECK(pokemon::markSpecies(save.state.caughtSpecies, record.speciesId));
  }
  CHECK(pokemon::markSpecies(save.state.seenSpecies, 16));
  save.state.pendingEvents[0] = {
      0, 16, 4, pokemon::Gender::Male, pokemon::EvolutionItem::None, pokemon::PendingEventKind::Encounter};
  save.state.pendingEvents[1] = {
      0, 0, 0, pokemon::Gender::Unknown, pokemon::EvolutionItem::MoonStone, pokemon::PendingEventKind::Item};
  save.state.pendingEvents[2] = {
      1, 26, 0, pokemon::Gender::Unknown, pokemon::EvolutionItem::None, pokemon::PendingEventKind::Evolution};
  save.state.itemCounts[0] = 1;
  save.state.dashboardNotice = pokemon::DashboardNotice::NewPokemon;
  save.state.lifetimeMinutes = 480;
  save.state.readingMinuteRemainder = 12;
  pokemon::PokemonState previous = save.state;
  previous.sequence = 20;
  previous.lifetimeMinutes = 475;
  save.state.sequence = 21;
  CHECK(writeOlderSnapshot(PATH_B, pokemon::POKEMON_SNAPSHOT_VERSION_V2, previous, save.records));
  CHECK(writeOlderSnapshot(PATH_A, pokemon::POKEMON_SNAPSHOT_VERSION_V2, save.state, save.records));
  return save;
}

void firstVersionSaveIsUpgradedOnStartupAndKeptAsABackup() {
  Storage.clear();
  const pokemon::PokemonRecord pikachu = starterPikachu();
  pokemon::PokemonState legacy{};
  legacy.partyRecordIds[0] = pikachu.recordId;
  legacy.sequence = 7;
  legacy.lifetimeMinutes = 123;
  legacy.pendingEvents[0] = {
      0, 1, 8, pokemon::Gender::Female, pokemon::EvolutionItem::None, pokemon::PendingEventKind::Encounter};
  legacy.dashboardNotice = pokemon::DashboardNotice::NewPokemon;
  CHECK(pokemon::markSpecies(legacy.seenSpecies, pikachu.speciesId));
  CHECK(pokemon::markSpecies(legacy.caughtSpecies, pikachu.speciesId));
  CHECK(pokemon::markSpecies(legacy.seenSpecies, 1));
  CHECK(writeLegacySnapshot(PATH_A, legacy, pikachu));

  pokemon::PokemonStore store;
  CHECK(store.begin() == pokemon::StoreBeginResult::Ready);
  pokemon::PokemonState loaded{};
  CHECK(store.loadState(loaded));
  CHECK(loaded == upgraded(legacy));
  pokemon::PokemonRecord loadedRecord{};
  CHECK(store.readRecord(pikachu.recordId, loadedRecord));
  CHECK(loadedRecord == pikachu);

  // The upgraded snapshot went to the free slot; the old one moved out of the way, untouched.
  pokemon::SnapshotHeader header{};
  CHECK(readStoredHeader(PATH_B, header));
  CHECK(header.version == pokemon::POKEMON_SNAPSHOT_VERSION);
  CHECK(header.sequence == 8);
  CHECK(!Storage.exists(PATH_A));
  CHECK(readStoredHeader(BACKUP_PATH, header));
  CHECK(header.version == pokemon::POKEMON_SNAPSHOT_VERSION_V1);
  CHECK(header.sequence == 7);

  // Starting again finds the upgraded save and does not upgrade it a second time.
  pokemon::PokemonStore reopened;
  CHECK(reopened.begin() == pokemon::StoreBeginResult::Ready);
  CHECK(reopened.loadState(loaded));
  CHECK(loaded == upgraded(legacy));

  // Saving carries on from there.
  loaded.lifetimeMinutes = 124;
  CHECK(reopened.commit(loaded));
  CHECK(readStoredHeader(PATH_A, header));
  CHECK(header.version == pokemon::POKEMON_SNAPSHOT_VERSION);
  CHECK(header.sequence == 9);
}

void versionTwoSaveKeepsEveryPokemonAndWaitingEvent() {
  Storage.clear();
  const VersionTwoSave save = writeVersionTwoSave();

  pokemon::PokemonStore store;
  CHECK(store.begin() == pokemon::StoreBeginResult::Ready);
  CHECK(store.recordCount() == save.records.size());
  pokemon::PokemonState loaded{};
  CHECK(store.loadState(loaded));
  CHECK(loaded == upgraded(save.state));
  CHECK(pokemon::bagCount(loaded, pokemon::POKEMON_ITEM_POKE_BALL) == pokemon::POKEMON_STARTING_POKE_BALLS);
  CHECK(pokemon::bagCount(loaded, pokemon::POKEMON_ITEM_POTION) == pokemon::POKEMON_STARTING_POTIONS);
  for (const pokemon::PokemonRecord& record : save.records) {
    pokemon::PokemonRecord loadedRecord{};
    CHECK(store.readRecord(record.recordId, loadedRecord));
    CHECK(loadedRecord == record);
  }

  // The newer of the two old snapshots is the one kept.
  pokemon::SnapshotHeader header{};
  CHECK(readStoredHeader(BACKUP_PATH, header));
  CHECK(header.version == pokemon::POKEMON_SNAPSHOT_VERSION_V2);
  CHECK(header.sequence == 21);
  CHECK(!Storage.exists(PATH_A));
  CHECK(readStoredHeader(PATH_B, header));
  CHECK(header.version == pokemon::POKEMON_SNAPSHOT_VERSION);
  CHECK(header.sequence == 22);
}

void upgradeGivesEveryPokemonItsMoves() {
  Storage.clear();
  const VersionTwoSave save = writeVersionTwoSave();

  // Stands in for the game's rule: here, the species id is the first move.
  pokemon::PokemonStore store(
      +[](pokemon::PokemonRecord& record) { record.moves = {static_cast<uint8_t>(record.speciesId), 33, 0, 0}; });
  CHECK(store.begin() == pokemon::StoreBeginResult::Ready);
  for (const pokemon::PokemonRecord& record : save.records) {
    pokemon::PokemonRecord expected = record;
    expected.moves = {static_cast<uint8_t>(record.speciesId), 33, 0, 0};
    pokemon::PokemonRecord loadedRecord{};
    CHECK(store.readRecord(record.recordId, loadedRecord));
    CHECK(loadedRecord == expected);
    CHECK(loadedRecord.hpLost == 0 && loadedRecord.status == 0);
  }

  // A record the upgrade leaves invalid stops the upgrade; the old save stays where it was.
  Storage.clear();
  writeVersionTwoSave();
  pokemon::PokemonStore broken(+[](pokemon::PokemonRecord& record) { record.moves = {33, 33, 0, 0}; });
  CHECK(broken.begin() == pokemon::StoreBeginResult::Corrupt);
  CHECK(!broken.commit(save.state));
  pokemon::SnapshotHeader header{};
  CHECK(readStoredHeader(PATH_A, header));
  CHECK(header.version == pokemon::POKEMON_SNAPSHOT_VERSION_V2);
  CHECK(!Storage.exists(BACKUP_PATH));
}

void everyInterruptedUpgradeByteLeavesTheOlderSaveBootable() {
  constexpr size_t upgradedSnapshotBytes = pokemon::POKEMON_SNAPSHOT_HEADER_BYTES + pokemon::POKEMON_STATE_BYTES +
                                           3U * pokemon::POKEMON_RECORD_BYTES + pokemon::POKEMON_SNAPSHOT_CRC_BYTES;
  for (size_t cut = 0; cut < upgradedSnapshotBytes; ++cut) {
    Storage.clear();
    const VersionTwoSave save = writeVersionTwoSave();

    Storage.setWriteLimit(cut);
    pokemon::PokemonStore interrupted;
    CHECK(interrupted.begin() == pokemon::StoreBeginResult::Corrupt);
    CHECK(!interrupted.commit(save.state));
    Storage.clearWriteLimit();

    // The old snapshot is still in its slot, and the next start upgrades it in full.
    pokemon::SnapshotHeader header{};
    CHECK(readStoredHeader(PATH_A, header));
    CHECK(header.version == pokemon::POKEMON_SNAPSHOT_VERSION_V2);
    CHECK(!Storage.exists(BACKUP_PATH));
    pokemon::PokemonStore retried;
    CHECK(retried.begin() == pokemon::StoreBeginResult::Ready);
    pokemon::PokemonState loaded{};
    CHECK(retried.loadState(loaded));
    CHECK(loaded == upgraded(save.state));
    CHECK(retried.recordCount() == save.records.size());
    CHECK(Storage.exists(BACKUP_PATH));
  }
}

void backupLeftUnfinishedIsCompletedOnTheNextStartup() {
  Storage.clear();
  const VersionTwoSave save = writeVersionTwoSave();

  // The upgrade itself succeeds, but the old snapshot cannot be moved aside.
  Storage.setFailRename(true);
  pokemon::PokemonStore store;
  CHECK(store.begin() == pokemon::StoreBeginResult::Ready);
  Storage.setFailRename(false);
  CHECK(!Storage.exists(BACKUP_PATH));
  pokemon::SnapshotHeader header{};
  CHECK(readStoredHeader(PATH_A, header));
  CHECK(header.version == pokemon::POKEMON_SNAPSHOT_VERSION_V2);

  pokemon::PokemonStore reopened;
  CHECK(reopened.begin() == pokemon::StoreBeginResult::Ready);
  CHECK(!Storage.exists(PATH_A));
  CHECK(readStoredHeader(BACKUP_PATH, header));
  CHECK(header.version == pokemon::POKEMON_SNAPSHOT_VERSION_V2);
  CHECK(header.sequence == 21);
  pokemon::PokemonState loaded{};
  CHECK(reopened.loadState(loaded));
  CHECK(loaded == upgraded(save.state));
}

void upgradeReplacesAnEarlierBackup() {
  Storage.clear();
  writeVersionTwoSave();
  CHECK(writeGarbage(BACKUP_PATH));

  pokemon::PokemonStore store;
  CHECK(store.begin() == pokemon::StoreBeginResult::Ready);
  pokemon::SnapshotHeader header{};
  CHECK(readStoredHeader(BACKUP_PATH, header));
  CHECK(header.sequence == 21);
}

void startupClassifiesCorruptAndUnsupportedSnapshotsAndFallsBackToValidOlderData() {
  Storage.clear();
  pokemon::PokemonStore store;
  CHECK(store.begin() == pokemon::StoreBeginResult::Empty);
  pokemon::PokemonState state{};
  state.lifetimeMinutes = 10;
  CHECK(store.commit(state));
  Storage.setByte("/.crosspoint/pokemon-a.bin", 24 + 104, 0xFF);
  pokemon::PokemonStore corrupt;
  CHECK(corrupt.begin() == pokemon::StoreBeginResult::Corrupt);

  Storage.clear();
  CHECK(store.begin() == pokemon::StoreBeginResult::Empty);
  CHECK(store.commit(state));
  Storage.setByte("/.crosspoint/pokemon-a.bin", 4, UNKNOWN_VERSION);
  pokemon::PokemonStore unsupported;
  CHECK(unsupported.begin() == pokemon::StoreBeginResult::Unsupported);

  Storage.clear();
  CHECK(store.begin() == pokemon::StoreBeginResult::Empty);
  CHECK(store.commit(state));
  state.lifetimeMinutes = 20;
  CHECK(store.commit(state));
  Storage.setByte("/.crosspoint/pokemon-b.bin", 24 + 104, 0xFF);
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
  Storage.setByte("/.crosspoint/pokemon-b.bin", 4, UNKNOWN_VERSION);

  pokemon::PokemonStore downgraded;
  CHECK(downgraded.begin() == pokemon::StoreBeginResult::Unsupported);
  CHECK(!downgraded.commit(state));

  Storage.setByte("/.crosspoint/pokemon-a.bin", 24 + 104, 0xFF);
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
  Storage.setByte("/.crosspoint/pokemon-a.bin", 4, UNKNOWN_VERSION);

  pokemon::PokemonStore protectedStore;
  CHECK(protectedStore.begin() == pokemon::StoreBeginResult::Unsupported);
  Storage.setFailRemove(true);
  CHECK(!protectedStore.reset());
  Storage.setFailRemove(false);
  CHECK(!protectedStore.commit(state));
  CHECK(Storage.exists("/.crosspoint/pokemon-a.bin"));
}

void legacyFilenamesMigrateTheNewestValidSnapshot() {
  Storage.clear();
  const pokemon::PokemonRecord pikachu = starterPikachu();
  pokemon::PokemonState older{};
  older.partyRecordIds[0] = pikachu.recordId;
  older.sequence = 7;
  older.lifetimeMinutes = 120;
  CHECK(pokemon::markSpecies(older.seenSpecies, pikachu.speciesId));
  CHECK(pokemon::markSpecies(older.caughtSpecies, pikachu.speciesId));
  pokemon::PokemonState newer = older;
  newer.sequence = 8;
  newer.lifetimeMinutes = 135;
  CHECK(writeLegacySnapshot("/.crosspoint/pokemon-v2-a.bin", older, pikachu));
  CHECK(writeLegacySnapshot("/.crosspoint/pokemon-v2-b.bin", newer, pikachu));

  pokemon::PokemonStore store;
  CHECK(store.begin() == pokemon::StoreBeginResult::Ready);
  pokemon::PokemonState loaded{};
  CHECK(store.loadState(loaded));
  CHECK(loaded == upgraded(newer));
  // The newer file took slot A under its new name, was upgraded into slot B, and is now the backup.
  CHECK(Storage.exists(PATH_B));
  CHECK(!Storage.exists(PATH_A));
  pokemon::SnapshotHeader header{};
  CHECK(readStoredHeader(BACKUP_PATH, header));
  CHECK(header.sequence == 8);
  CHECK(!Storage.exists("/.crosspoint/pokemon-v2-b.bin"));
  CHECK(Storage.exists("/.crosspoint/pokemon-v2-a.bin"));
}

void failedLegacyFilenameMigrationKeepsTheOriginalSave() {
  Storage.clear();
  const pokemon::PokemonRecord pikachu = starterPikachu();
  pokemon::PokemonState state{};
  state.partyRecordIds[0] = pikachu.recordId;
  state.sequence = 4;
  CHECK(pokemon::markSpecies(state.seenSpecies, pikachu.speciesId));
  CHECK(pokemon::markSpecies(state.caughtSpecies, pikachu.speciesId));
  CHECK(writeLegacySnapshot("/.crosspoint/pokemon-v2-a.bin", state, pikachu));
  Storage.setFailRename(true);

  pokemon::PokemonStore store;
  CHECK(store.begin() == pokemon::StoreBeginResult::Corrupt);
  CHECK(Storage.exists("/.crosspoint/pokemon-v2-a.bin"));
  CHECK(!Storage.exists("/.crosspoint/pokemon-a.bin"));
}

void corruptFirstSaveWithoutFallbackCanBeReset() {
  for (const char* corruptPath : {"/.crosspoint/pokemon-a.bin", "/.crosspoint/pokemon-b.bin"}) {
    Storage.clear();
    CHECK(writeGarbage(corruptPath));

    pokemon::PokemonStore store;
    CHECK(store.begin() == pokemon::StoreBeginResult::Corrupt);
    CHECK(!store.isReady());
    pokemon::PokemonState state{};
    // Only an explicit reset may replace the unrecoverable slot.
    CHECK(!store.commit(state));
    CHECK(store.reset());
    CHECK(store.isReady());
    CHECK(store.recordCount() == 0);
    CHECK(Storage.exists("/.crosspoint/pokemon-a.bin"));
    CHECK(!Storage.exists("/.crosspoint/pokemon-b.bin"));
    pokemon::PokemonState loaded{};
    CHECK(store.loadState(loaded));
    CHECK(loaded.sequence == 1);
    CHECK(loaded.partyRecordIds[0] == 0);

    pokemon::PokemonStore reopened;
    CHECK(reopened.begin() == pokemon::StoreBeginResult::Ready);
    CHECK(reopened.recordCount() == 0);
  }
}

void failedCorruptSlotRemovalKeepsResetBlocked() {
  Storage.clear();
  CHECK(writeGarbage("/.crosspoint/pokemon-a.bin"));
  pokemon::PokemonStore store;
  CHECK(store.begin() == pokemon::StoreBeginResult::Corrupt);
  Storage.setFailRemove(true);
  CHECK(!store.reset());
  Storage.setFailRemove(false);
  CHECK(!store.isReady());
  CHECK(!store.commit(pokemon::PokemonState{}));
  CHECK(store.reset());
  CHECK(store.isReady());
}

void corruptSlotBesideOtherDamageIsNotResettable() {
  // Two corrupt slots, or corruption next to a newer-format save, still block
  // writes; only the interrupted-first-save case is recoverable by reset().
  Storage.clear();
  CHECK(writeGarbage("/.crosspoint/pokemon-a.bin"));
  CHECK(writeGarbage("/.crosspoint/pokemon-b.bin"));
  pokemon::PokemonStore bothCorrupt;
  CHECK(bothCorrupt.begin() == pokemon::StoreBeginResult::Corrupt);
  CHECK(!bothCorrupt.reset());
  CHECK(Storage.exists("/.crosspoint/pokemon-a.bin"));
  CHECK(Storage.exists("/.crosspoint/pokemon-b.bin"));

  Storage.clear();
  pokemon::PokemonStore store;
  CHECK(store.begin() == pokemon::StoreBeginResult::Empty);
  CHECK(store.commit(pokemon::PokemonState{}));
  Storage.setByte("/.crosspoint/pokemon-a.bin", 4, UNKNOWN_VERSION);
  CHECK(writeGarbage("/.crosspoint/pokemon-b.bin"));
  pokemon::PokemonStore mixed;
  CHECK(mixed.begin() == pokemon::StoreBeginResult::Corrupt);
  CHECK(!mixed.reset());
  CHECK(Storage.exists("/.crosspoint/pokemon-b.bin"));
}

}  // namespace

int main() {
  emptyStoreCommitsAndReloadsSequenceOne();
  secondCommitUsesSlotBAndWinsStartupSelection();
  initialAppendPersistsAReadableRecord();
  laterAppendStreamsExistingRecordsIntoTheInactiveSlot();
  replacementChangesOnlyTheRequestedRecord();
  oneCommitReplacesEveryPartyMemberAndAppendsACatch();
  batchSkipsUnusedEntries();
  rejectedBatchSavesNothing();
  everyInterruptedBatchByteLeavesThePreviousSnapshotBootable();
  pcPagesExcludeThePartyAndSupportAllThreeOrders();
  pcReadFailureIsDistinctFromAValidEmptyPage();
  resetCommitsANewerEmptySnapshot();
  everyInterruptedResetByteLeavesThePreviousSnapshotBootable();
  everyInterruptedWriteByteLeavesThePreviousSnapshotBootable();
  unwritableAppendPreservesThePendingEncounterAndRoster();
  interruptedAppendPreservesThePendingEncounterAndRoster();
  failedSyncKeepsTheLiveStoreOldButAllowsACompleteSnapshotAfterRestart();
  firstVersionSaveIsUpgradedOnStartupAndKeptAsABackup();
  versionTwoSaveKeepsEveryPokemonAndWaitingEvent();
  upgradeGivesEveryPokemonItsMoves();
  everyInterruptedUpgradeByteLeavesTheOlderSaveBootable();
  backupLeftUnfinishedIsCompletedOnTheNextStartup();
  upgradeReplacesAnEarlierBackup();
  startupClassifiesCorruptAndUnsupportedSnapshotsAndFallsBackToValidOlderData();
  startupBlocksDowngradesAndMixedUnsupportedCorruption();
  failedResetDoesNotBypassProtectedStoreGating();
  legacyFilenamesMigrateTheNewestValidSnapshot();
  failedLegacyFilenameMigrationKeepsTheOriginalSave();
  corruptFirstSaveWithoutFallbackCanBeReset();
  failedCorruptSlotRemovalKeepsResetBlocked();
  corruptSlotBesideOtherDamageIsNotResettable();
  return failures == 0 ? 0 : 1;
}
