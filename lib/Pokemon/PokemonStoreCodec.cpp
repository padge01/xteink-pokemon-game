#if defined(CROSSINK_ENABLE_POKEMON)

#include "PokemonStoreCodec.h"

#include <cstring>
#include <limits>

namespace pokemon {
namespace {

constexpr size_t PENDING_EVENT_BYTES = 10;

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
  return static_cast<uint16_t>(bytes[offset]) | static_cast<uint16_t>(static_cast<uint16_t>(bytes[offset + 1]) << 8U);
}

uint32_t read32(const uint8_t* bytes, const size_t offset) {
  return static_cast<uint32_t>(bytes[offset]) | (static_cast<uint32_t>(bytes[offset + 1]) << 8U) |
         (static_cast<uint32_t>(bytes[offset + 2]) << 16U) | (static_cast<uint32_t>(bytes[offset + 3]) << 24U);
}

void encodePendingEvent(const PendingEvent& event, uint8_t* bytes) {
  write32(bytes, 0, event.recordId);
  write16(bytes, 4, event.speciesId);
  bytes[6] = event.level;
  bytes[7] = static_cast<uint8_t>(event.gender);
  bytes[8] = static_cast<uint8_t>(event.item);
  bytes[9] = static_cast<uint8_t>(event.kind);
}

PendingEvent decodePendingEvent(const uint8_t* bytes) {
  PendingEvent event{};
  event.recordId = read32(bytes, 0);
  event.speciesId = read16(bytes, 4);
  event.level = bytes[6];
  event.gender = static_cast<Gender>(bytes[7]);
  event.item = static_cast<EvolutionItem>(bytes[8]);
  event.kind = static_cast<PendingEventKind>(bytes[9]);
  return event;
}

}  // namespace

size_t snapshotStateBytes(const uint16_t version) {
  if (version == POKEMON_SNAPSHOT_VERSION_V1) return POKEMON_STATE_V1_BYTES;
  if (version == POKEMON_SNAPSHOT_VERSION) return POKEMON_STATE_BYTES;
  return 0;
}

uint64_t snapshotFileBytes(const SnapshotHeader& header) {
  const size_t stateBytes = snapshotStateBytes(header.version);
  if (stateBytes == 0) return 0;
  return POKEMON_SNAPSHOT_HEADER_BYTES + stateBytes +
         static_cast<uint64_t>(header.recordCount) * POKEMON_RECORD_BYTES + POKEMON_SNAPSHOT_CRC_BYTES;
}

bool encodeSnapshotHeader(const SnapshotHeader& header, HeaderBytes& output) {
  const size_t stateBytes = snapshotStateBytes(header.version);
  const uint64_t payloadBytes = stateBytes + static_cast<uint64_t>(header.recordCount) * POKEMON_RECORD_BYTES;
  if (stateBytes == 0 || header.sequence == 0 || payloadBytes > std::numeric_limits<uint32_t>::max()) return false;

  HeaderBytes candidate{};
  candidate[0] = 'P';
  candidate[1] = 'K';
  candidate[2] = 'V';
  candidate[3] = '2';
  write16(candidate.data(), 4, header.version);
  write16(candidate.data(), 6, POKEMON_SNAPSHOT_HEADER_BYTES);
  write32(candidate.data(), 8, header.sequence);
  write16(candidate.data(), 12, static_cast<uint16_t>(stateBytes));
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
  SnapshotHeader candidate{};
  candidate.version = read16(bytes.data(), 4);
  const size_t stateBytes = snapshotStateBytes(candidate.version);
  if (stateBytes == 0) return HeaderDecodeResult::Unsupported;
  candidate.sequence = read32(bytes.data(), 8);
  candidate.recordCount = read32(bytes.data(), 16);
  const uint64_t payloadBytes = stateBytes + static_cast<uint64_t>(candidate.recordCount) * POKEMON_RECORD_BYTES;
  if (candidate.sequence == 0 || read16(bytes.data(), 6) != POKEMON_SNAPSHOT_HEADER_BYTES ||
      read16(bytes.data(), 12) != stateBytes || read16(bytes.data(), 14) != POKEMON_RECORD_BYTES ||
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
  for (size_t index = 0; index < PENDING_EVENT_CAPACITY; ++index) {
    encodePendingEvent(state.pendingEvents[index], candidate.data() + 24U + index * PENDING_EVENT_BYTES);
  }
  for (size_t index = 0; index < EVOLUTION_ITEM_COUNT; ++index) {
    write16(candidate.data(), 54U + index * 2U, state.itemCounts[index]);
  }
  std::memcpy(candidate.data() + 66, state.seenSpecies.data(), state.seenSpecies.size());
  std::memcpy(candidate.data() + 85, state.caughtSpecies.data(), state.caughtSpecies.size());
  write32(candidate.data(), 104, state.lifetimeMinutes);
  write32(candidate.data(), 108, state.sequence);
  candidate[112] = state.readingMinuteRemainder;
  candidate[113] = state.encounterMisses;
  candidate[114] = state.itemMisses;
  candidate[115] = static_cast<uint8_t>(state.dashboardNotice);
  output = candidate;
  return true;
}

bool decodeState(const uint8_t* bytes, const size_t size, const uint16_t version, PokemonState& output) {
  if (bytes == nullptr || size != snapshotStateBytes(version)) return false;
  PokemonState candidate{};
  for (size_t slot = 0; slot < PARTY_SIZE; ++slot) candidate.partyRecordIds[slot] = read32(bytes, slot * 4U);
  if (version == POKEMON_SNAPSHOT_VERSION_V1) {
    candidate.pendingEvents[0] = decodePendingEvent(bytes + 24);
    for (size_t index = 0; index < EVOLUTION_ITEM_COUNT; ++index) {
      candidate.itemCounts[index] = read16(bytes, 34U + index * 2U);
    }
    std::memcpy(candidate.seenSpecies.data(), bytes + 46, candidate.seenSpecies.size());
    std::memcpy(candidate.caughtSpecies.data(), bytes + 65, candidate.caughtSpecies.size());
    candidate.lifetimeMinutes = read32(bytes, 84);
    candidate.sequence = read32(bytes, 88);
    candidate.readingMinuteRemainder = bytes[92];
    candidate.encounterMisses = bytes[93];
    candidate.itemMisses = bytes[94];
    candidate.dashboardNotice = static_cast<DashboardNotice>(bytes[95]);
  } else {
    for (size_t index = 0; index < PENDING_EVENT_CAPACITY; ++index) {
      candidate.pendingEvents[index] = decodePendingEvent(bytes + 24U + index * PENDING_EVENT_BYTES);
    }
    for (size_t index = 0; index < EVOLUTION_ITEM_COUNT; ++index) {
      candidate.itemCounts[index] = read16(bytes, 54U + index * 2U);
    }
    std::memcpy(candidate.seenSpecies.data(), bytes + 66, candidate.seenSpecies.size());
    std::memcpy(candidate.caughtSpecies.data(), bytes + 85, candidate.caughtSpecies.size());
    candidate.lifetimeMinutes = read32(bytes, 104);
    candidate.sequence = read32(bytes, 108);
    candidate.readingMinuteRemainder = bytes[112];
    candidate.encounterMisses = bytes[113];
    candidate.itemMisses = bytes[114];
    candidate.dashboardNotice = static_cast<DashboardNotice>(bytes[115]);
  }
  if (!validateState(candidate)) return false;
  output = candidate;
  return true;
}

}  // namespace pokemon

#endif
