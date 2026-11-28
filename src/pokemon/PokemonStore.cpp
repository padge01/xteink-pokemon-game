#if defined(CROSSINK_ENABLE_POKEMON)

#include "PokemonStore.h"

#include <HalStorage.h>
#include <Logging.h>
#include <PokemonSpecies.h>

#include <algorithm>
#include <cstring>

namespace pokemon {
namespace {

constexpr const char* STORE_DIRECTORY = "/.crosspoint";
constexpr const char* STORE_PATH_A = "/.crosspoint/pokemon-a.bin";
constexpr const char* STORE_PATH_B = "/.crosspoint/pokemon-b.bin";
constexpr const char* LEGACY_STORE_PATH_A = "/.crosspoint/pokemon-v2-a.bin";
constexpr const char* LEGACY_STORE_PATH_B = "/.crosspoint/pokemon-v2-b.bin";
// The last snapshot an older version wrote, set aside when the save is upgraded so the player
// can still go back to that version.
constexpr const char* UPGRADE_BACKUP_PATH = "/.crosspoint/pokemon-before-upgrade.bin";

enum class InspectionResult : uint8_t {
  Missing,
  Ready,
  Corrupt,
  Unsupported,
};

bool readExact(FsFile& file, void* output, const size_t size) {
  return file.read(output, size) == static_cast<int>(size);
}

bool writeExact(FsFile& file, const void* input, const size_t size) { return file.write(input, size) == size; }

uint32_t read32(const uint8_t* bytes) {
  return static_cast<uint32_t>(bytes[0]) | (static_cast<uint32_t>(bytes[1]) << 8U) |
         (static_cast<uint32_t>(bytes[2]) << 16U) | (static_cast<uint32_t>(bytes[3]) << 24U);
}

void write32(uint8_t* bytes, const uint32_t value) {
  bytes[0] = static_cast<uint8_t>(value);
  bytes[1] = static_cast<uint8_t>(value >> 8U);
  bytes[2] = static_cast<uint8_t>(value >> 16U);
  bytes[3] = static_cast<uint8_t>(value >> 24U);
}

size_t recordsOffset(const SnapshotHeader& header) {
  return POKEMON_SNAPSHOT_HEADER_BYTES + snapshotStateBytes(header.version);
}

bool isOlderVersion(const SnapshotHeader& header) { return header.version != POKEMON_SNAPSHOT_VERSION; }

// Decodes the `size` bytes of a record as `version` wrote them.
bool decodeStoredRecord(const RecordBytes& bytes, const uint16_t version, PokemonRecord& output) {
  if (version == POKEMON_SNAPSHOT_VERSION) return decodeRecord(bytes, output);
  LegacyRecordBytes legacy{};
  std::memcpy(legacy.data(), bytes.data(), legacy.size());
  return decodeLegacyRecord(legacy, output);
}

// Moves the snapshot an older version wrote out of its slot, replacing any earlier backup.
void setAsideOlderSnapshot(const char* path) {
  if (Storage.exists(UPGRADE_BACKUP_PATH) && !Storage.remove(UPGRADE_BACKUP_PATH)) {
    LOG_ERR("PokemonStore", "Failed to remove the previous upgrade backup");
    return;
  }
  if (!Storage.rename(path, UPGRADE_BACKUP_PATH)) {
    LOG_ERR("PokemonStore", "Failed to keep %s as the upgrade backup", path);
    return;
  }
  LOG_INF("PokemonStore", "Kept the pre-upgrade save as %s", UPGRADE_BACKUP_PATH);
}

InspectionResult inspectSnapshot(const char* path, SnapshotHeader& outputHeader) {
  if (!Storage.exists(path)) return InspectionResult::Missing;
  FsFile file = Storage.open(path, O_RDONLY);
  if (!file) {
    LOG_ERR("PokemonStore", "Failed to open %s", path);
    return InspectionResult::Corrupt;
  }

  HeaderBytes headerBytes{};
  if (!readExact(file, headerBytes.data(), headerBytes.size())) {
    LOG_ERR("PokemonStore", "Short header in %s", path);
    file.close();
    return InspectionResult::Corrupt;
  }
  SnapshotHeader header{};
  const HeaderDecodeResult headerResult = decodeSnapshotHeader(headerBytes, header);
  if (headerResult != HeaderDecodeResult::Ready) {
    file.close();
    return headerResult == HeaderDecodeResult::Unsupported ? InspectionResult::Unsupported : InspectionResult::Corrupt;
  }
  if (file.fileSize64() != snapshotFileBytes(header)) {
    LOG_ERR("PokemonStore", "Invalid snapshot size in %s", path);
    file.close();
    return InspectionResult::Corrupt;
  }

  StateBytes stateBytes{};
  const size_t stateSize = snapshotStateBytes(header.version);
  if (stateSize == 0 || !readExact(file, stateBytes.data(), stateSize)) {
    LOG_ERR("PokemonStore", "Short payload in %s", path);
    file.close();
    return InspectionResult::Corrupt;
  }
  PokemonState state{};
  if (!decodeState(stateBytes.data(), stateSize, header.version, state) || state.sequence != header.sequence) {
    LOG_ERR("PokemonStore", "Invalid state in %s", path);
    file.close();
    return InspectionResult::Corrupt;
  }
  uint32_t crc = updateSnapshotCrc32(POKEMON_SNAPSHOT_CRC32_INITIAL, headerBytes.data(), headerBytes.size());
  crc = updateSnapshotCrc32(crc, stateBytes.data(), stateSize);
  uint32_t previousRecordId = 0;
  uint8_t foundPartySlots = 0;
  uint8_t requiredPendingRecords = 0;
  uint8_t foundPendingRecords = 0;
  for (size_t eventIndex = 0; eventIndex < state.pendingEvents.size(); ++eventIndex) {
    if (state.pendingEvents[eventIndex].kind == PendingEventKind::Evolution) {
      requiredPendingRecords |= static_cast<uint8_t>(1U << eventIndex);
    }
  }
  const size_t recordSize = snapshotRecordBytes(header.version);
  for (uint32_t index = 0; index < header.recordCount; ++index) {
    RecordBytes recordBytes{};
    PokemonRecord record{};
    if (!readExact(file, recordBytes.data(), recordSize) || !decodeStoredRecord(recordBytes, header.version, record) ||
        record.recordId <= previousRecordId || !isSpeciesMarked(state.caughtSpecies, record.speciesId)) {
      LOG_ERR("PokemonStore", "Invalid record in %s", path);
      file.close();
      return InspectionResult::Corrupt;
    }
    crc = updateSnapshotCrc32(crc, recordBytes.data(), recordSize);
    previousRecordId = record.recordId;
    for (size_t slot = 0; slot < PARTY_SIZE; ++slot) {
      if (state.partyRecordIds[slot] == record.recordId) foundPartySlots |= static_cast<uint8_t>(1U << slot);
    }
    for (size_t eventIndex = 0; eventIndex < state.pendingEvents.size(); ++eventIndex) {
      if (state.pendingEvents[eventIndex].kind == PendingEventKind::Evolution &&
          state.pendingEvents[eventIndex].recordId == record.recordId) {
        foundPendingRecords |= static_cast<uint8_t>(1U << eventIndex);
      }
    }
  }

  uint8_t requiredPartySlots = 0;
  for (size_t slot = 0; slot < PARTY_SIZE && state.partyRecordIds[slot] != 0; ++slot) {
    requiredPartySlots |= static_cast<uint8_t>(1U << slot);
  }
  uint8_t crcBytes[POKEMON_SNAPSHOT_CRC_BYTES]{};
  if (foundPartySlots != requiredPartySlots || foundPendingRecords != requiredPendingRecords ||
      !readExact(file, crcBytes, sizeof(crcBytes))) {
    LOG_ERR("PokemonStore", "Unresolved state reference in %s", path);
    file.close();
    return InspectionResult::Corrupt;
  }
  file.close();
  if (finishSnapshotCrc32(crc) != read32(crcBytes)) {
    LOG_ERR("PokemonStore", "CRC mismatch in %s", path);
    return InspectionResult::Corrupt;
  }
  outputHeader = header;
  return InspectionResult::Ready;
}

bool sequenceIsNewer(const uint32_t candidate, const uint32_t current) {
  return candidate != current && candidate - current < 0x80000000U;
}

bool recordIsInParty(const PokemonState& state, const uint32_t recordId) {
  return std::any_of(state.partyRecordIds.begin(), state.partyRecordIds.end(),
                     [recordId](const uint32_t partyRecordId) { return partyRecordId == recordId; });
}

// Checks a batch of record changes before anything is written, and reports the one record it
// adds (if any) and how many it replaces. Entries of kind None are skipped.
bool planMutations(const std::span<const RecordMutation> mutations, const RecordMutation*& append,
                   size_t& replacementCount) {
  append = nullptr;
  replacementCount = 0;
  if (mutations.size() > RECORD_MUTATION_CAPACITY) return false;
  for (size_t index = 0; index < mutations.size(); ++index) {
    const RecordMutation& mutation = mutations[index];
    if (mutation.kind == RecordMutationKind::None) continue;
    if ((mutation.kind != RecordMutationKind::Append && mutation.kind != RecordMutationKind::Replace) ||
        mutation.requestedRecordId != mutation.record.recordId || !validateRecord(mutation.record)) {
      return false;
    }
    for (size_t prior = 0; prior < index; ++prior) {
      if (mutations[prior].kind != RecordMutationKind::None &&
          mutations[prior].record.recordId == mutation.record.recordId) {
        return false;
      }
    }
    if (mutation.kind == RecordMutationKind::Replace) {
      ++replacementCount;
    } else if (append == nullptr) {
      append = &mutation;
    } else {
      return false;
    }
  }
  return true;
}

const RecordMutation* findReplacement(const std::span<const RecordMutation> mutations, const uint32_t recordId) {
  for (const RecordMutation& mutation : mutations) {
    if (mutation.kind == RecordMutationKind::Replace && mutation.record.recordId == recordId) return &mutation;
  }
  return nullptr;
}

}  // namespace

StoreBeginResult PokemonStore::begin() {
  ready_ = false;
  writable_ = false;
  resettableCorruptPath_ = nullptr;
  activeHeader_ = {};
  activeIsA_ = false;
  if (!Storage.ensureDirectoryExists(STORE_DIRECTORY)) {
    LOG_ERR("PokemonStore", "Failed to prepare data directory");
    return StoreBeginResult::Corrupt;
  }

  SnapshotHeader headerA{};
  SnapshotHeader headerB{};
  InspectionResult resultA = inspectSnapshot(STORE_PATH_A, headerA);
  InspectionResult resultB = inspectSnapshot(STORE_PATH_B, headerB);
  if (resultA == InspectionResult::Missing && resultB == InspectionResult::Missing) {
    SnapshotHeader legacyHeaderA{};
    SnapshotHeader legacyHeaderB{};
    const InspectionResult legacyResultA = inspectSnapshot(LEGACY_STORE_PATH_A, legacyHeaderA);
    const InspectionResult legacyResultB = inspectSnapshot(LEGACY_STORE_PATH_B, legacyHeaderB);
    if (legacyResultA == InspectionResult::Unsupported || legacyResultB == InspectionResult::Unsupported) {
      if (legacyResultA == InspectionResult::Corrupt || legacyResultB == InspectionResult::Corrupt) {
        return StoreBeginResult::Corrupt;
      }
      return StoreBeginResult::Unsupported;
    }
    if (legacyResultA == InspectionResult::Ready || legacyResultB == InspectionResult::Ready) {
      const bool legacyIsA = legacyResultA == InspectionResult::Ready &&
                             (legacyResultB != InspectionResult::Ready ||
                              !sequenceIsNewer(legacyHeaderB.sequence, legacyHeaderA.sequence));
      const char* legacyPath = legacyIsA ? LEGACY_STORE_PATH_A : LEGACY_STORE_PATH_B;
      const SnapshotHeader legacyHeader = legacyIsA ? legacyHeaderA : legacyHeaderB;
      if (!Storage.rename(legacyPath, STORE_PATH_A)) {
        LOG_ERR("PokemonStore", "Failed to migrate legacy save filename");
        return StoreBeginResult::Corrupt;
      }
      resultA = inspectSnapshot(STORE_PATH_A, headerA);
      if (resultA != InspectionResult::Ready || headerA != legacyHeader) {
        LOG_ERR("PokemonStore", "Migrated legacy save failed verification");
        return StoreBeginResult::Corrupt;
      }
      LOG_INF("PokemonStore", "Migrated legacy save filename");
    } else if (legacyResultA != InspectionResult::Missing || legacyResultB != InspectionResult::Missing) {
      return StoreBeginResult::Corrupt;
    }
  }
  if (resultA == InspectionResult::Unsupported || resultB == InspectionResult::Unsupported) {
    if (resultA == InspectionResult::Corrupt || resultB == InspectionResult::Corrupt) {
      return StoreBeginResult::Corrupt;
    }
    // A downgraded build must not overwrite the slot owned by a newer format.
    return StoreBeginResult::Unsupported;
  }
  if (resultA == InspectionResult::Ready || resultB == InspectionResult::Ready) {
    activeIsA_ = resultA == InspectionResult::Ready &&
                 (resultB != InspectionResult::Ready || !sequenceIsNewer(headerB.sequence, headerA.sequence));
    activeHeader_ = activeIsA_ ? headerA : headerB;
    ready_ = true;
    writable_ = true;
    const bool otherIsReady = (activeIsA_ ? resultB : resultA) == InspectionResult::Ready;
    const SnapshotHeader& otherHeader = activeIsA_ ? headerB : headerA;
    if (isOlderVersion(activeHeader_)) {
      if (!upgradeActiveSnapshot()) {
        ready_ = false;
        writable_ = false;
        return StoreBeginResult::Corrupt;
      }
    } else if (otherIsReady && isOlderVersion(otherHeader)) {
      // An upgrade that was cut off after the new snapshot was written but before the old one
      // was set aside. Finish it, or the next save would overwrite the old snapshot.
      setAsideOlderSnapshot(activeIsA_ ? STORE_PATH_B : STORE_PATH_A);
    }
    return StoreBeginResult::Ready;
  }
  if (resultA == InspectionResult::Missing && resultB == InspectionResult::Missing) {
    writable_ = true;
    return StoreBeginResult::Empty;
  }
  // One corrupt slot with no other snapshot means the very first save was cut
  // short. No valid data remains, so an explicit reset() may replace it rather
  // than leaving the game locked behind a save error forever.
  if (resultA == InspectionResult::Corrupt && resultB == InspectionResult::Missing) {
    resettableCorruptPath_ = STORE_PATH_A;
  } else if (resultA == InspectionResult::Missing && resultB == InspectionResult::Corrupt) {
    resettableCorruptPath_ = STORE_PATH_B;
  }
  return StoreBeginResult::Corrupt;
}

bool PokemonStore::loadState(PokemonState& output) const {
  if (!ready_) return false;
  const char* path = activeIsA_ ? STORE_PATH_A : STORE_PATH_B;
  FsFile file = Storage.open(path, O_RDONLY);
  if (!file || !file.seek(POKEMON_SNAPSHOT_HEADER_BYTES)) {
    LOG_ERR("PokemonStore", "Failed to open active state");
    file.close();
    return false;
  }
  StateBytes bytes{};
  const size_t stateSize = snapshotStateBytes(activeHeader_.version);
  const bool readOk = stateSize != 0 && readExact(file, bytes.data(), stateSize);
  file.close();
  if (!readOk || !decodeState(bytes.data(), stateSize, activeHeader_.version, output)) {
    LOG_ERR("PokemonStore", "Failed to decode active state");
    return false;
  }
  upgradeState(output, activeHeader_.version);
  return true;
}

bool PokemonStore::upgradeActiveSnapshot() {
  const char* olderPath = activeIsA_ ? STORE_PATH_A : STORE_PATH_B;
  PokemonState state{};
  // writeSnapshot() converts each record as it copies it across.
  if (!loadState(state) || !writeSnapshot(state, {}, false)) {
    LOG_ERR("PokemonStore", "Failed to upgrade the save in %s", olderPath);
    return false;
  }
  LOG_INF("PokemonStore", "Upgraded the save to version %u", static_cast<unsigned>(POKEMON_SNAPSHOT_VERSION));
  setAsideOlderSnapshot(olderPath);
  return true;
}

bool PokemonStore::commit(const PokemonState& state, const RecordMutation& mutation) {
  return writeSnapshot(state, std::span<const RecordMutation>(&mutation, 1), false);
}

bool PokemonStore::commit(const PokemonState& state, const std::span<const RecordMutation> mutations) {
  return writeSnapshot(state, mutations, false);
}

bool PokemonStore::writeSnapshot(const PokemonState& state, const std::span<const RecordMutation> mutations,
                                 const bool discardRecords) {
  SnapshotHeader header{};
  if (!writeInactiveSnapshot(state, mutations, discardRecords, header)) return false;

  const bool destinationIsA = !ready_ || !activeIsA_;
  SnapshotHeader verified{};
  if (inspectSnapshot(destinationIsA ? STORE_PATH_A : STORE_PATH_B, verified) != InspectionResult::Ready ||
      verified != header) {
    LOG_ERR("PokemonStore", "Inactive snapshot verification failed");
    return false;
  }
  activeHeader_ = verified;
  activeIsA_ = destinationIsA;
  ready_ = true;
  return true;
}

// Kept out of line so its buffers are off the stack again before the snapshot is read back and
// checked: the two steps each hold a full copy of the state.
[[gnu::noinline]] bool PokemonStore::writeInactiveSnapshot(const PokemonState& state,
                                                           const std::span<const RecordMutation> mutations,
                                                           const bool discardRecords, SnapshotHeader& header) {
  const RecordMutation* append = nullptr;
  size_t replacementCount = 0;
  if (!writable_ || !validateState(state) || !planMutations(mutations, append, replacementCount) ||
      (discardRecords && (append != nullptr || replacementCount != 0)) || (replacementCount != 0 && !ready_)) {
    return false;
  }

  const bool preserveRecords = ready_ && !discardRecords;
  const uint32_t currentRecordCount = preserveRecords ? activeHeader_.recordCount : 0;
  if (append != nullptr && currentRecordCount == UINT32_MAX) return false;

  const uint32_t nextSequence =
      !ready_ ? 1U : (activeHeader_.sequence == UINT32_MAX ? 1U : activeHeader_.sequence + 1U);
  StateBytes stateBytes{};
  header = SnapshotHeader{nextSequence, currentRecordCount + (append != nullptr ? 1U : 0U)};
  HeaderBytes headerBytes{};
  if (!encodeState(state, stateBytes) || !encodeSnapshotHeader(header, headerBytes)) return false;
  write32(stateBytes.data() + POKEMON_STATE_SEQUENCE_OFFSET, nextSequence);

  const bool destinationIsA = !ready_ || !activeIsA_;
  const char* destinationPath = destinationIsA ? STORE_PATH_A : STORE_PATH_B;
  FsFile source;
  if (preserveRecords) {
    const char* sourcePath = activeIsA_ ? STORE_PATH_A : STORE_PATH_B;
    source = Storage.open(sourcePath, O_RDONLY);
    if (!source || !source.seek(recordsOffset(activeHeader_))) {
      LOG_ERR("PokemonStore", "Failed to open active snapshot records");
      source.close();
      return false;
    }
  }
  FsFile destination = Storage.open(destinationPath, O_WRONLY | O_CREAT | O_TRUNC);
  if (!destination) {
    LOG_ERR("PokemonStore", "Failed to open inactive snapshot");
    if (preserveRecords) source.close();
    return false;
  }

  uint32_t crc = updateSnapshotCrc32(POKEMON_SNAPSHOT_CRC32_INITIAL, headerBytes.data(), headerBytes.size());
  crc = updateSnapshotCrc32(crc, stateBytes.data(), stateBytes.size());
  bool writeOk = writeExact(destination, headerBytes.data(), headerBytes.size()) &&
                 writeExact(destination, stateBytes.data(), stateBytes.size());
  uint32_t lastRecordId = 0;
  size_t replacementsWritten = 0;
  const uint16_t sourceVersion = activeHeader_.version;
  const size_t sourceRecordSize = preserveRecords ? snapshotRecordBytes(sourceVersion) : 0;
  for (uint32_t index = 0; writeOk && index < currentRecordCount; ++index) {
    RecordBytes recordBytes{};
    writeOk = readExact(source, recordBytes.data(), sourceRecordSize);
    if (writeOk && sourceVersion != POKEMON_SNAPSHOT_VERSION) {
      // The source is a snapshot from an older version: bring each record forward.
      PokemonRecord upgraded{};
      writeOk = decodeStoredRecord(recordBytes, sourceVersion, upgraded);
      if (writeOk && upgradeRecord_ != nullptr) upgradeRecord_(upgraded);
      writeOk = writeOk && encodeRecord(upgraded, recordBytes);
    }
    if (writeOk) {
      const uint32_t recordId = read32(recordBytes.data());
      writeOk = recordId > lastRecordId;
      lastRecordId = recordId;
      if (const RecordMutation* replacement = findReplacement(mutations, recordId); replacement != nullptr) {
        writeOk = writeOk && encodeRecord(replacement->record, recordBytes);
        if (writeOk) ++replacementsWritten;
      }
      writeOk = writeOk && writeExact(destination, recordBytes.data(), recordBytes.size());
      if (writeOk) crc = updateSnapshotCrc32(crc, recordBytes.data(), recordBytes.size());
    }
  }
  if (append != nullptr) {
    RecordBytes appendBytes{};
    writeOk = writeOk && append->record.recordId > lastRecordId && encodeRecord(append->record, appendBytes) &&
              writeExact(destination, appendBytes.data(), appendBytes.size());
    if (writeOk) crc = updateSnapshotCrc32(crc, appendBytes.data(), appendBytes.size());
  }
  // A replacement whose record is not in the save would otherwise be dropped silently.
  writeOk = writeOk && replacementsWritten == replacementCount;
  const bool sourceCloseOk = !preserveRecords || source.close();
  uint8_t crcBytes[POKEMON_SNAPSHOT_CRC_BYTES]{};
  write32(crcBytes, finishSnapshotCrc32(crc));
  writeOk = writeOk && writeExact(destination, crcBytes, sizeof(crcBytes)) && destination.sync();
  const bool closeOk = destination.close();
  if (!writeOk || !sourceCloseOk || !closeOk) {
    LOG_ERR("PokemonStore", "Failed to write inactive snapshot");
    return false;
  }
  return true;
}

bool PokemonStore::readRecord(const uint32_t recordId, PokemonRecord& output) const {
  if (!ready_ || recordId == 0) return false;
  const char* path = activeIsA_ ? STORE_PATH_A : STORE_PATH_B;
  FsFile file = Storage.open(path, O_RDONLY);
  if (!file || !file.seek(recordsOffset(activeHeader_))) {
    LOG_ERR("PokemonStore", "Failed to open active records");
    file.close();
    return false;
  }
  for (uint32_t index = 0; index < activeHeader_.recordCount; ++index) {
    RecordBytes bytes{};
    PokemonRecord candidate{};
    if (!readExact(file, bytes.data(), bytes.size()) || !decodeRecord(bytes, candidate)) {
      LOG_ERR("PokemonStore", "Failed to decode active record");
      file.close();
      return false;
    }
    if (candidate.recordId == recordId) {
      file.close();
      output = candidate;
      return true;
    }
    if (candidate.recordId > recordId) break;
  }
  file.close();
  return false;
}

bool PokemonStore::loadOwnedEvolutionNeeds(OwnedEvolutionNeeds& output) const {
  output = {};
  if (!ready_) return false;

  const char* path = activeIsA_ ? STORE_PATH_A : STORE_PATH_B;
  FsFile file = Storage.open(path, O_RDONLY);
  if (!file || !file.seek(recordsOffset(activeHeader_))) {
    LOG_ERR("PokemonStore", "Failed to open owned records");
    file.close();
    return false;
  }

  for (uint32_t index = 0; index < activeHeader_.recordCount; ++index) {
    RecordBytes bytes{};
    PokemonRecord record{};
    if (!readExact(file, bytes.data(), bytes.size()) || !decodeRecord(bytes, record)) {
      LOG_ERR("PokemonStore", "Failed to scan owned evolution needs");
      file.close();
      return false;
    }
    for (const EvolutionRule& rule : evolutionsFor(record.speciesId)) {
      const uint8_t item = static_cast<uint8_t>(rule.item);
      if (rule.trigger == EvolutionTrigger::Item && item >= 1 && item <= EVOLUTION_ITEM_COUNT) {
        output.mask |= static_cast<uint8_t>(1U << (item - 1U));
      }
    }
  }
  return file.close();
}

bool PokemonStore::readPcPage(const PcOrder order, size_t offset, const std::span<PokemonRecord> output,
                              size_t& count) const {
  count = 0;
  if (!ready_ || output.empty() || order > PcOrder::Alphabetical) return false;
  PokemonState state{};
  if (!loadState(state)) return false;
  const char* path = activeIsA_ ? STORE_PATH_A : STORE_PATH_B;
  FsFile file = Storage.open(path, O_RDONLY);
  const size_t activeRecordsOffset = recordsOffset(activeHeader_);
  if (!file || !file.seek(activeRecordsOffset)) {
    LOG_ERR("PokemonStore", "Failed to open PC records");
    file.close();
    return false;
  }

  size_t written = 0;
  if (order == PcOrder::CatchDate) {
    for (uint32_t index = 0; index < activeHeader_.recordCount; ++index) {
      RecordBytes bytes{};
      PokemonRecord record{};
      if (!readExact(file, bytes.data(), bytes.size()) || !decodeRecord(bytes, record)) {
        LOG_ERR("PokemonStore", "Failed to read PC capture order");
        file.close();
        return false;
      }
      if (recordIsInParty(state, record.recordId)) continue;
      if (offset != 0) {
        --offset;
        continue;
      }
      output[written++] = record;
      if (written == output.size()) break;
    }
    if (!file.close()) {
      LOG_ERR("PokemonStore", "Failed to close PC capture order");
      return false;
    }
    count = written;
    return true;
  }

  PokedexBits pcSpecies{};
  for (uint32_t index = 0; index < activeHeader_.recordCount; ++index) {
    RecordBytes bytes{};
    PokemonRecord record{};
    if (!readExact(file, bytes.data(), bytes.size()) || !decodeRecord(bytes, record) ||
        (!recordIsInParty(state, record.recordId) && !markSpecies(pcSpecies, record.speciesId))) {
      LOG_ERR("PokemonStore", "Failed to scan PC species");
      file.close();
      return false;
    }
  }

  const auto appendSpecies = [&](const uint16_t speciesId) {
    if (!file.seek(activeRecordsOffset)) return false;
    for (uint32_t index = 0; index < activeHeader_.recordCount; ++index) {
      RecordBytes bytes{};
      PokemonRecord record{};
      if (!readExact(file, bytes.data(), bytes.size()) || !decodeRecord(bytes, record)) return false;
      if (record.speciesId != speciesId || recordIsInParty(state, record.recordId)) continue;
      if (offset != 0) {
        --offset;
        continue;
      }
      output[written++] = record;
      if (written == output.size()) return true;
    }
    return true;
  };

  if (order == PcOrder::PokedexNumber) {
    for (uint16_t speciesId = 1; speciesId <= KANTO_SPECIES_COUNT && written < output.size(); ++speciesId) {
      if (isSpeciesMarked(pcSpecies, speciesId) && !appendSpecies(speciesId)) {
        LOG_ERR("PokemonStore", "Failed to order PC by Pokedex number");
        file.close();
        return false;
      }
    }
  } else {
    uint16_t previousSpeciesId = 0;
    while (written < output.size()) {
      uint16_t nextSpeciesId = 0;
      for (uint16_t speciesId = 1; speciesId <= KANTO_SPECIES_COUNT; ++speciesId) {
        if (!isSpeciesMarked(pcSpecies, speciesId)) continue;
        const char* name = speciesData(speciesId)->name;
        if (previousSpeciesId != 0 && std::strcmp(name, speciesData(previousSpeciesId)->name) <= 0) continue;
        if (nextSpeciesId == 0 || std::strcmp(name, speciesData(nextSpeciesId)->name) < 0) nextSpeciesId = speciesId;
      }
      if (nextSpeciesId == 0) break;
      if (!appendSpecies(nextSpeciesId)) {
        LOG_ERR("PokemonStore", "Failed to order PC alphabetically");
        file.close();
        return false;
      }
      previousSpeciesId = nextSpeciesId;
    }
  }
  if (!file.close()) {
    LOG_ERR("PokemonStore", "Failed to close ordered PC page");
    return false;
  }
  count = written;
  return true;
}

bool PokemonStore::reset() {
  if (!writable_ && resettableCorruptPath_ == nullptr) return false;
  if (!writable_) {
    if (!Storage.remove(resettableCorruptPath_)) {
      LOG_ERR("PokemonStore", "Failed to remove corrupt save %s", resettableCorruptPath_);
      return false;
    }
    LOG_INF("PokemonStore", "Removed unrecoverable save %s", resettableCorruptPath_);
    resettableCorruptPath_ = nullptr;
    writable_ = true;
  }
  const PokemonState empty{};
  if (!writeSnapshot(empty, {}, true)) {
    LOG_ERR("PokemonStore", "Failed to commit empty reset snapshot");
    return false;
  }
  return true;
}

}  // namespace pokemon

#endif
