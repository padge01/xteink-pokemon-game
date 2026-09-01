#if defined(CROSSINK_ENABLE_POKEMON)

#include "PokemonStore.h"

#include <HalStorage.h>
#include <Logging.h>
#include <PokemonSpecies.h>

#include <cstring>

namespace pokemon {
namespace {

constexpr const char* STORE_DIRECTORY = "/.crosspoint";
constexpr const char* STORE_PATH_A = "/.crosspoint/pokemon-v2-a.bin";
constexpr const char* STORE_PATH_B = "/.crosspoint/pokemon-v2-b.bin";
constexpr size_t RECORDS_OFFSET = POKEMON_SNAPSHOT_HEADER_BYTES + POKEMON_STATE_BYTES;

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
  if (!readExact(file, stateBytes.data(), stateBytes.size())) {
    LOG_ERR("PokemonStore", "Short payload in %s", path);
    file.close();
    return InspectionResult::Corrupt;
  }
  PokemonState state{};
  if (!decodeState(stateBytes, state) || state.sequence != header.sequence) {
    LOG_ERR("PokemonStore", "Invalid state in %s", path);
    file.close();
    return InspectionResult::Corrupt;
  }
  uint32_t crc = updateSnapshotCrc32(POKEMON_SNAPSHOT_CRC32_INITIAL, headerBytes.data(), headerBytes.size());
  crc = updateSnapshotCrc32(crc, stateBytes.data(), stateBytes.size());
  uint32_t previousRecordId = 0;
  uint8_t foundPartySlots = 0;
  bool foundPendingRecord = state.pending.kind != PendingEventKind::Evolution;
  for (uint32_t index = 0; index < header.recordCount; ++index) {
    RecordBytes recordBytes{};
    PokemonRecord record{};
    if (!readExact(file, recordBytes.data(), recordBytes.size()) || !decodeRecord(recordBytes, record) ||
        record.recordId <= previousRecordId || !isSpeciesMarked(state.caughtSpecies, record.speciesId)) {
      LOG_ERR("PokemonStore", "Invalid record in %s", path);
      file.close();
      return InspectionResult::Corrupt;
    }
    crc = updateSnapshotCrc32(crc, recordBytes.data(), recordBytes.size());
    previousRecordId = record.recordId;
    for (size_t slot = 0; slot < PARTY_SIZE; ++slot) {
      if (state.partyRecordIds[slot] == record.recordId) foundPartySlots |= static_cast<uint8_t>(1U << slot);
    }
    if (state.pending.kind == PendingEventKind::Evolution && state.pending.recordId == record.recordId) {
      foundPendingRecord = true;
    }
  }

  uint8_t requiredPartySlots = 0;
  for (size_t slot = 0; slot < PARTY_SIZE && state.partyRecordIds[slot] != 0; ++slot) {
    requiredPartySlots |= static_cast<uint8_t>(1U << slot);
  }
  uint8_t crcBytes[POKEMON_SNAPSHOT_CRC_BYTES]{};
  if (foundPartySlots != requiredPartySlots || !foundPendingRecord || !readExact(file, crcBytes, sizeof(crcBytes))) {
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
  for (const uint32_t partyRecordId : state.partyRecordIds) {
    if (partyRecordId == recordId) return true;
  }
  return false;
}

}  // namespace

StoreBeginResult PokemonStore::begin() {
  ready_ = false;
  writable_ = false;
  activeHeader_ = {};
  activeIsA_ = false;
  if (!Storage.ensureDirectoryExists(STORE_DIRECTORY)) {
    LOG_ERR("PokemonStore", "Failed to prepare data directory");
    return StoreBeginResult::Corrupt;
  }

  SnapshotHeader headerA{};
  SnapshotHeader headerB{};
  const InspectionResult resultA = inspectSnapshot(STORE_PATH_A, headerA);
  const InspectionResult resultB = inspectSnapshot(STORE_PATH_B, headerB);
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
    return StoreBeginResult::Ready;
  }
  if (resultA == InspectionResult::Missing && resultB == InspectionResult::Missing) {
    writable_ = true;
    return StoreBeginResult::Empty;
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
  const bool readOk = readExact(file, bytes.data(), bytes.size());
  file.close();
  if (!readOk || !decodeState(bytes, output)) {
    LOG_ERR("PokemonStore", "Failed to decode active state");
    return false;
  }
  return true;
}

bool PokemonStore::commit(const PokemonState& state, const RecordMutation& mutation) {
  return writeSnapshot(state, mutation, false);
}

bool PokemonStore::writeSnapshot(const PokemonState& state, const RecordMutation& mutation, const bool discardRecords) {
  const bool appending = mutation.kind == RecordMutationKind::Append;
  const bool replacing = mutation.kind == RecordMutationKind::Replace;
  if (!writable_ || !validateState(state) || (discardRecords && mutation.kind != RecordMutationKind::None) ||
      ((appending || replacing) &&
       (mutation.requestedRecordId != mutation.record.recordId || !validateRecord(mutation.record))) ||
      (replacing && (!ready_ || discardRecords)) ||
      (!appending && !replacing && mutation.kind != RecordMutationKind::None)) {
    return false;
  }

  const bool preserveRecords = ready_ && !discardRecords;
  const uint32_t currentRecordCount = preserveRecords ? activeHeader_.recordCount : 0;
  if (appending && currentRecordCount == UINT32_MAX) return false;

  const uint32_t nextSequence =
      !ready_ ? 1U : (activeHeader_.sequence == UINT32_MAX ? 1U : activeHeader_.sequence + 1U);
  StateBytes stateBytes{};
  const SnapshotHeader header{nextSequence, currentRecordCount + (appending ? 1U : 0U)};
  HeaderBytes headerBytes{};
  if (!encodeState(state, stateBytes) || !encodeSnapshotHeader(header, headerBytes)) return false;
  write32(stateBytes.data() + 88, nextSequence);
  RecordBytes mutationRecordBytes{};
  if ((appending || replacing) && !encodeRecord(mutation.record, mutationRecordBytes)) return false;

  const bool destinationIsA = !ready_ || !activeIsA_;
  const char* destinationPath = destinationIsA ? STORE_PATH_A : STORE_PATH_B;
  FsFile source;
  if (preserveRecords) {
    const char* sourcePath = activeIsA_ ? STORE_PATH_A : STORE_PATH_B;
    source = Storage.open(sourcePath, O_RDONLY);
    if (!source || !source.seek(POKEMON_SNAPSHOT_HEADER_BYTES + POKEMON_STATE_BYTES)) {
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
  bool replacementFound = !replacing;
  for (uint32_t index = 0; writeOk && index < currentRecordCount; ++index) {
    RecordBytes recordBytes{};
    writeOk = readExact(source, recordBytes.data(), recordBytes.size());
    if (writeOk) {
      const uint32_t recordId = read32(recordBytes.data());
      writeOk = recordId > lastRecordId;
      lastRecordId = recordId;
      const bool useReplacement = replacing && recordId == mutation.requestedRecordId;
      const RecordBytes& outputBytes = useReplacement ? mutationRecordBytes : recordBytes;
      writeOk = writeOk && writeExact(destination, outputBytes.data(), outputBytes.size());
      if (writeOk) crc = updateSnapshotCrc32(crc, outputBytes.data(), outputBytes.size());
      if (writeOk && useReplacement) replacementFound = true;
    }
  }
  if (appending) {
    writeOk = writeOk && mutation.record.recordId > lastRecordId &&
              writeExact(destination, mutationRecordBytes.data(), mutationRecordBytes.size());
    if (writeOk) crc = updateSnapshotCrc32(crc, mutationRecordBytes.data(), mutationRecordBytes.size());
  }
  writeOk = writeOk && replacementFound;
  const bool sourceCloseOk = !preserveRecords || source.close();
  uint8_t crcBytes[POKEMON_SNAPSHOT_CRC_BYTES]{};
  write32(crcBytes, finishSnapshotCrc32(crc));
  writeOk = writeOk && writeExact(destination, crcBytes, sizeof(crcBytes)) && destination.sync();
  const bool closeOk = destination.close();
  if (!writeOk || !sourceCloseOk || !closeOk) {
    LOG_ERR("PokemonStore", "Failed to write inactive snapshot");
    return false;
  }

  SnapshotHeader verified{};
  if (inspectSnapshot(destinationPath, verified) != InspectionResult::Ready || verified != header) {
    LOG_ERR("PokemonStore", "Inactive snapshot verification failed");
    return false;
  }
  activeHeader_ = verified;
  activeIsA_ = destinationIsA;
  ready_ = true;
  return true;
}

bool PokemonStore::readRecord(const uint32_t recordId, PokemonRecord& output) const {
  if (!ready_ || recordId == 0) return false;
  const char* path = activeIsA_ ? STORE_PATH_A : STORE_PATH_B;
  FsFile file = Storage.open(path, O_RDONLY);
  if (!file || !file.seek(POKEMON_SNAPSHOT_HEADER_BYTES + POKEMON_STATE_BYTES)) {
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
  if (!file || !file.seek(RECORDS_OFFSET)) {
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
  if (!file || !file.seek(RECORDS_OFFSET)) {
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
    if (!file.seek(RECORDS_OFFSET)) return false;
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
  if (!writable_) return false;
  const PokemonState empty{};
  if (!writeSnapshot(empty, {}, true)) {
    LOG_ERR("PokemonStore", "Failed to commit empty reset snapshot");
    return false;
  }
  return true;
}

}  // namespace pokemon

#endif
