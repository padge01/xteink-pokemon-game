#include "PokemonStoreCodec.h"

#include <cstring>
#include <limits>

namespace pokemon {
namespace {

constexpr uint16_t SNAPSHOT_VERSION = 1;

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

uint16_t read16(const uint8_t* bytes, const size_t offset) {
  return static_cast<uint16_t>(bytes[offset]) |
         static_cast<uint16_t>(static_cast<uint16_t>(bytes[offset + 1]) << 8U);
}

uint32_t read32(const uint8_t* bytes, const size_t offset) {
  return static_cast<uint32_t>(bytes[offset]) |
         (static_cast<uint32_t>(bytes[offset + 1]) << 8U) |
         (static_cast<uint32_t>(bytes[offset + 2]) << 16U) |
         (static_cast<uint32_t>(bytes[offset + 3]) << 24U);
}

}  // namespace

uint64_t snapshotFileBytes(const SnapshotHeader& header) {
  return POKEMON_SNAPSHOT_HEADER_BYTES + POKEMON_STATE_BYTES +
         static_cast<uint64_t>(header.recordCount) * POKEMON_RECORD_BYTES + POKEMON_SNAPSHOT_CRC_BYTES;
}

bool encodeSnapshotHeader(const SnapshotHeader& header, HeaderBytes& output) {
  const uint64_t payloadBytes =
      POKEMON_STATE_BYTES + static_cast<uint64_t>(header.recordCount) * POKEMON_RECORD_BYTES;
  if (header.sequence == 0 || payloadBytes > std::numeric_limits<uint32_t>::max()) return false;

  HeaderBytes candidate{};
  candidate[0] = 'P';
  candidate[1] = 'K';
  candidate[2] = 'V';
  candidate[3] = '2';
  write16(candidate.data(), 4, SNAPSHOT_VERSION);
  write16(candidate.data(), 6, POKEMON_SNAPSHOT_HEADER_BYTES);
  write32(candidate.data(), 8, header.sequence);
  write16(candidate.data(), 12, POKEMON_STATE_BYTES);
  write16(candidate.data(), 14, POKEMON_RECORD_BYTES);
  write32(candidate.data(), 16, header.recordCount);
  write32(candidate.data(), 20, static_cast<uint32_t>(payloadBytes));
  output = candidate;
  return true;
}

HeaderDecodeResult decodeSnapshotHeader(const HeaderBytes& bytes, SnapshotHeader& output) {
  if (bytes[0] != 'P' || bytes[1] != 'K' || bytes[2] != 'V' || bytes[3] != '2') {
    return HeaderDecodeResult::Corrupt;
  }
  if (read16(bytes.data(), 4) != SNAPSHOT_VERSION) return HeaderDecodeResult::Unsupported;
  SnapshotHeader candidate{};
  candidate.sequence = read32(bytes.data(), 8);
  candidate.recordCount = read32(bytes.data(), 16);
  const uint64_t payloadBytes = POKEMON_STATE_BYTES + static_cast<uint64_t>(candidate.recordCount) * POKEMON_RECORD_BYTES;
  if (candidate.sequence == 0 || read16(bytes.data(), 6) != POKEMON_SNAPSHOT_HEADER_BYTES ||
      read16(bytes.data(), 12) != POKEMON_STATE_BYTES || read16(bytes.data(), 14) != POKEMON_RECORD_BYTES ||
      payloadBytes > std::numeric_limits<uint32_t>::max() || read32(bytes.data(), 20) != payloadBytes) {
    return HeaderDecodeResult::Corrupt;
  }
  output = candidate;
  return HeaderDecodeResult::Ready;
}

uint32_t updateSnapshotCrc32(uint32_t crc, const uint8_t* data, const size_t size) {
  for (size_t index = 0; index < size; ++index) {
    crc ^= data[index];
    for (uint8_t bit = 0; bit < 8; ++bit) {
      crc = (crc >> 1U) ^ (0xEDB88320U & static_cast<uint32_t>(0U - (crc & 1U)));
    }
  }
  return crc;
}

bool encodeState(const PokemonState& state, StateBytes& output) {
  if (!validateState(state)) return false;

  StateBytes candidate{};
  for (size_t slot = 0; slot < PARTY_SIZE; ++slot) write32(candidate.data(), slot * 4U, state.partyRecordIds[slot]);
  write32(candidate.data(), 24, state.pending.recordId);
  write16(candidate.data(), 28, state.pending.speciesId);
  candidate[30] = state.pending.level;
  candidate[31] = static_cast<uint8_t>(state.pending.gender);
  candidate[32] = static_cast<uint8_t>(state.pending.item);
  candidate[33] = static_cast<uint8_t>(state.pending.kind);
  for (size_t index = 0; index < EVOLUTION_ITEM_COUNT; ++index) {
    write16(candidate.data(), 34U + index * 2U, state.itemCounts[index]);
  }
  std::memcpy(candidate.data() + 46, state.seenSpecies.data(), state.seenSpecies.size());
  std::memcpy(candidate.data() + 65, state.caughtSpecies.data(), state.caughtSpecies.size());
  write32(candidate.data(), 84, state.lifetimeMinutes);
  write32(candidate.data(), 88, state.sequence);
  candidate[92] = state.readingMinuteRemainder;
  candidate[93] = state.encounterMisses;
  candidate[94] = state.itemMisses;
  candidate[95] = static_cast<uint8_t>(state.dashboardNotice);
  output = candidate;
  return true;
}

bool decodeState(const StateBytes& bytes, PokemonState& output) {
  PokemonState candidate{};
  for (size_t slot = 0; slot < PARTY_SIZE; ++slot) candidate.partyRecordIds[slot] = read32(bytes.data(), slot * 4U);
  candidate.pending.recordId = read32(bytes.data(), 24);
  candidate.pending.speciesId = read16(bytes.data(), 28);
  candidate.pending.level = bytes[30];
  candidate.pending.gender = static_cast<Gender>(bytes[31]);
  candidate.pending.item = static_cast<EvolutionItem>(bytes[32]);
  candidate.pending.kind = static_cast<PendingEventKind>(bytes[33]);
  for (size_t index = 0; index < EVOLUTION_ITEM_COUNT; ++index) {
    candidate.itemCounts[index] = read16(bytes.data(), 34U + index * 2U);
  }
  std::memcpy(candidate.seenSpecies.data(), bytes.data() + 46, candidate.seenSpecies.size());
  std::memcpy(candidate.caughtSpecies.data(), bytes.data() + 65, candidate.caughtSpecies.size());
  candidate.lifetimeMinutes = read32(bytes.data(), 84);
  candidate.sequence = read32(bytes.data(), 88);
  candidate.readingMinuteRemainder = bytes[92];
  candidate.encounterMisses = bytes[93];
  candidate.itemMisses = bytes[94];
  candidate.dashboardNotice = static_cast<DashboardNotice>(bytes[95]);
  if (!validateState(candidate)) return false;
  output = candidate;
  return true;
}

}  // namespace pokemon
