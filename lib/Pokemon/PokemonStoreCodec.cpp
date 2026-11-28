#if defined(CROSSINK_ENABLE_POKEMON)

#include "PokemonStoreCodec.h"

#include <cstring>
#include <limits>

namespace pokemon {
namespace {

constexpr size_t PENDING_EVENT_BYTES = 10;
constexpr size_t HALL_OF_FAME_MEMBER_BYTES = 3;
constexpr size_t SAVED_BATTLE_FOE_BYTES = 3;

// Layout of the current state, in file order.
constexpr size_t STATE_PARTY_OFFSET = 0;
constexpr size_t STATE_PENDING_OFFSET = STATE_PARTY_OFFSET + PARTY_SIZE * 4U;
constexpr size_t STATE_ITEM_COUNTS_OFFSET = STATE_PENDING_OFFSET + PENDING_EVENT_CAPACITY * PENDING_EVENT_BYTES;
constexpr size_t STATE_SEEN_OFFSET = STATE_ITEM_COUNTS_OFFSET + EVOLUTION_ITEM_COUNT * 2U;
constexpr size_t STATE_CAUGHT_OFFSET = STATE_SEEN_OFFSET + POKEDEX_BYTES;
constexpr size_t STATE_LIFETIME_MINUTES_OFFSET = STATE_CAUGHT_OFFSET + POKEDEX_BYTES;
constexpr size_t STATE_COUNTERS_OFFSET = POKEMON_STATE_SEQUENCE_OFFSET + 4U;
constexpr size_t STATE_BATTLE_PROGRESS_OFFSET = STATE_COUNTERS_OFFSET + 4U;
constexpr size_t STATE_MAP_OFFSET = STATE_BATTLE_PROGRESS_OFFSET + 2U;
constexpr size_t STATE_BAG_OFFSET = STATE_MAP_OFFSET + 4U;
constexpr size_t STATE_HALL_OF_FAME_OFFSET = STATE_BAG_OFFSET + POKEMON_BAG_ITEM_COUNT;
constexpr size_t STATE_HALL_OF_FAME_MINUTES_OFFSET = STATE_HALL_OF_FAME_OFFSET + PARTY_SIZE * HALL_OF_FAME_MEMBER_BYTES;
constexpr size_t STATE_SAVED_BATTLE_OFFSET = STATE_HALL_OF_FAME_MINUTES_OFFSET + 4U;
constexpr size_t STATE_SAVED_BATTLE_FOES_OFFSET = STATE_SAVED_BATTLE_OFFSET + 4U;
// The rest is spare and must be zero.
constexpr size_t STATE_RESERVED_OFFSET = STATE_SAVED_BATTLE_FOES_OFFSET + PARTY_SIZE * SAVED_BATTLE_FOE_BYTES;
static_assert(STATE_LIFETIME_MINUTES_OFFSET + 4U == POKEMON_STATE_SEQUENCE_OFFSET);
static_assert(STATE_RESERVED_OFFSET <= POKEMON_STATE_BYTES);

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

// Fills everything after the party from a current-version state. False if the spare bytes are used.
bool decodeCurrentState(const uint8_t* bytes, PokemonState& state) {
  for (size_t index = STATE_RESERVED_OFFSET; index < POKEMON_STATE_BYTES; ++index) {
    if (bytes[index] != 0) return false;
  }
  for (size_t index = 0; index < PENDING_EVENT_CAPACITY; ++index) {
    state.pendingEvents[index] = decodePendingEvent(bytes + STATE_PENDING_OFFSET + index * PENDING_EVENT_BYTES);
  }
  for (size_t index = 0; index < EVOLUTION_ITEM_COUNT; ++index) {
    state.itemCounts[index] = read16(bytes, STATE_ITEM_COUNTS_OFFSET + index * 2U);
  }
  std::memcpy(state.seenSpecies.data(), bytes + STATE_SEEN_OFFSET, state.seenSpecies.size());
  std::memcpy(state.caughtSpecies.data(), bytes + STATE_CAUGHT_OFFSET, state.caughtSpecies.size());
  state.lifetimeMinutes = read32(bytes, STATE_LIFETIME_MINUTES_OFFSET);
  state.sequence = read32(bytes, POKEMON_STATE_SEQUENCE_OFFSET);
  state.readingMinuteRemainder = bytes[STATE_COUNTERS_OFFSET];
  state.encounterMisses = bytes[STATE_COUNTERS_OFFSET + 1U];
  state.itemMisses = bytes[STATE_COUNTERS_OFFSET + 2U];
  state.dashboardNotice = static_cast<DashboardNotice>(bytes[STATE_COUNTERS_OFFSET + 3U]);
  state.battleProgress = read16(bytes, STATE_BATTLE_PROGRESS_OFFSET);
  state.map.area = bytes[STATE_MAP_OFFSET];
  state.map.destination = bytes[STATE_MAP_OFFSET + 1U];
  state.map.steps = read16(bytes, STATE_MAP_OFFSET + 2U);
  std::memcpy(state.bagCounts.data(), bytes + STATE_BAG_OFFSET, state.bagCounts.size());
  for (size_t slot = 0; slot < PARTY_SIZE; ++slot) {
    const uint8_t* member = bytes + STATE_HALL_OF_FAME_OFFSET + slot * HALL_OF_FAME_MEMBER_BYTES;
    state.hallOfFame.team[slot].speciesId = read16(member, 0);
    state.hallOfFame.team[slot].level = member[2];
  }
  state.hallOfFame.lifetimeMinutes = read32(bytes, STATE_HALL_OF_FAME_MINUTES_OFFSET);
  state.savedBattle.kind = static_cast<SavedBattleKind>(bytes[STATE_SAVED_BATTLE_OFFSET]);
  state.savedBattle.leagueIndex = bytes[STATE_SAVED_BATTLE_OFFSET + 1U];
  state.savedBattle.playerSlot = bytes[STATE_SAVED_BATTLE_OFFSET + 2U];
  state.savedBattle.foeSlot = bytes[STATE_SAVED_BATTLE_OFFSET + 3U];
  for (size_t slot = 0; slot < PARTY_SIZE; ++slot) {
    const uint8_t* foe = bytes + STATE_SAVED_BATTLE_FOES_OFFSET + slot * SAVED_BATTLE_FOE_BYTES;
    state.savedBattle.foes[slot].hpLost = read16(foe, 0);
    state.savedBattle.foes[slot].status = foe[2];
  }
  return true;
}

}  // namespace

size_t snapshotStateBytes(const uint16_t version) {
  if (version == POKEMON_SNAPSHOT_VERSION_V1) return POKEMON_STATE_V1_BYTES;
  if (version == POKEMON_SNAPSHOT_VERSION_V2) return POKEMON_STATE_V2_BYTES;
  if (version == POKEMON_SNAPSHOT_VERSION) return POKEMON_STATE_BYTES;
  return 0;
}

size_t snapshotRecordBytes(const uint16_t version) {
  if (version == POKEMON_SNAPSHOT_VERSION_V1 || version == POKEMON_SNAPSHOT_VERSION_V2) {
    return POKEMON_LEGACY_RECORD_BYTES;
  }
  if (version == POKEMON_SNAPSHOT_VERSION) return POKEMON_RECORD_BYTES;
  return 0;
}

uint64_t snapshotFileBytes(const SnapshotHeader& header) {
  const size_t stateBytes = snapshotStateBytes(header.version);
  if (stateBytes == 0) return 0;
  return POKEMON_SNAPSHOT_HEADER_BYTES + stateBytes +
         static_cast<uint64_t>(header.recordCount) * snapshotRecordBytes(header.version) + POKEMON_SNAPSHOT_CRC_BYTES;
}

bool encodeSnapshotHeader(const SnapshotHeader& header, HeaderBytes& output) {
  const size_t stateBytes = snapshotStateBytes(header.version);
  const size_t recordBytes = snapshotRecordBytes(header.version);
  const uint64_t payloadBytes = stateBytes + static_cast<uint64_t>(header.recordCount) * recordBytes;
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
  write16(candidate.data(), 14, static_cast<uint16_t>(recordBytes));
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
  const size_t recordBytes = snapshotRecordBytes(candidate.version);
  candidate.sequence = read32(bytes.data(), 8);
  candidate.recordCount = read32(bytes.data(), 16);
  const uint64_t payloadBytes = stateBytes + static_cast<uint64_t>(candidate.recordCount) * recordBytes;
  if (candidate.sequence == 0 || read16(bytes.data(), 6) != POKEMON_SNAPSHOT_HEADER_BYTES ||
      read16(bytes.data(), 12) != stateBytes || read16(bytes.data(), 14) != recordBytes ||
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
  uint8_t* bytes = candidate.data();
  for (size_t slot = 0; slot < PARTY_SIZE; ++slot)
    write32(bytes, STATE_PARTY_OFFSET + slot * 4U, state.partyRecordIds[slot]);
  for (size_t index = 0; index < PENDING_EVENT_CAPACITY; ++index) {
    encodePendingEvent(state.pendingEvents[index], bytes + STATE_PENDING_OFFSET + index * PENDING_EVENT_BYTES);
  }
  for (size_t index = 0; index < EVOLUTION_ITEM_COUNT; ++index) {
    write16(bytes, STATE_ITEM_COUNTS_OFFSET + index * 2U, state.itemCounts[index]);
  }
  std::memcpy(bytes + STATE_SEEN_OFFSET, state.seenSpecies.data(), state.seenSpecies.size());
  std::memcpy(bytes + STATE_CAUGHT_OFFSET, state.caughtSpecies.data(), state.caughtSpecies.size());
  write32(bytes, STATE_LIFETIME_MINUTES_OFFSET, state.lifetimeMinutes);
  write32(bytes, POKEMON_STATE_SEQUENCE_OFFSET, state.sequence);
  bytes[STATE_COUNTERS_OFFSET] = state.readingMinuteRemainder;
  bytes[STATE_COUNTERS_OFFSET + 1U] = state.encounterMisses;
  bytes[STATE_COUNTERS_OFFSET + 2U] = state.itemMisses;
  bytes[STATE_COUNTERS_OFFSET + 3U] = static_cast<uint8_t>(state.dashboardNotice);
  write16(bytes, STATE_BATTLE_PROGRESS_OFFSET, state.battleProgress);
  bytes[STATE_MAP_OFFSET] = state.map.area;
  bytes[STATE_MAP_OFFSET + 1U] = state.map.destination;
  write16(bytes, STATE_MAP_OFFSET + 2U, state.map.steps);
  std::memcpy(bytes + STATE_BAG_OFFSET, state.bagCounts.data(), state.bagCounts.size());
  for (size_t slot = 0; slot < PARTY_SIZE; ++slot) {
    uint8_t* member = bytes + STATE_HALL_OF_FAME_OFFSET + slot * HALL_OF_FAME_MEMBER_BYTES;
    write16(member, 0, state.hallOfFame.team[slot].speciesId);
    member[2] = state.hallOfFame.team[slot].level;
  }
  write32(bytes, STATE_HALL_OF_FAME_MINUTES_OFFSET, state.hallOfFame.lifetimeMinutes);
  bytes[STATE_SAVED_BATTLE_OFFSET] = static_cast<uint8_t>(state.savedBattle.kind);
  bytes[STATE_SAVED_BATTLE_OFFSET + 1U] = state.savedBattle.leagueIndex;
  bytes[STATE_SAVED_BATTLE_OFFSET + 2U] = state.savedBattle.playerSlot;
  bytes[STATE_SAVED_BATTLE_OFFSET + 3U] = state.savedBattle.foeSlot;
  for (size_t slot = 0; slot < PARTY_SIZE; ++slot) {
    uint8_t* foe = bytes + STATE_SAVED_BATTLE_FOES_OFFSET + slot * SAVED_BATTLE_FOE_BYTES;
    write16(foe, 0, state.savedBattle.foes[slot].hpLost);
    foe[2] = state.savedBattle.foes[slot].status;
  }
  output = candidate;
  return true;
}

bool decodeState(const uint8_t* bytes, const size_t size, const uint16_t version, PokemonState& output) {
  if (bytes == nullptr || size == 0 || size != snapshotStateBytes(version)) return false;
  PokemonState candidate{};
  for (size_t slot = 0; slot < PARTY_SIZE; ++slot) candidate.partyRecordIds[slot] = read32(bytes, slot * 4U);
  if (version == POKEMON_SNAPSHOT_VERSION) {
    if (!decodeCurrentState(bytes, candidate)) return false;
  } else {
    // Versions 1 and 2 share one layout, apart from how many events can wait.
    const size_t eventCount = version == POKEMON_SNAPSHOT_VERSION_V1 ? 1U : 3U;
    const uint8_t* rest = bytes + STATE_PENDING_OFFSET + eventCount * PENDING_EVENT_BYTES;
    for (size_t index = 0; index < eventCount; ++index) {
      candidate.pendingEvents[index] = decodePendingEvent(bytes + STATE_PENDING_OFFSET + index * PENDING_EVENT_BYTES);
    }
    for (size_t index = 0; index < EVOLUTION_ITEM_COUNT; ++index) {
      candidate.itemCounts[index] = read16(rest, index * 2U);
    }
    std::memcpy(candidate.seenSpecies.data(), rest + 12, candidate.seenSpecies.size());
    std::memcpy(candidate.caughtSpecies.data(), rest + 31, candidate.caughtSpecies.size());
    candidate.lifetimeMinutes = read32(rest, 50);
    candidate.sequence = read32(rest, 54);
    candidate.readingMinuteRemainder = rest[58];
    candidate.encounterMisses = rest[59];
    candidate.itemMisses = rest[60];
    candidate.dashboardNotice = static_cast<DashboardNotice>(rest[61]);
  }
  if (!validateState(candidate)) return false;
  output = candidate;
  return true;
}

void upgradeState(PokemonState& state, const uint16_t fromVersion) {
  // 1 -> 2 only made room for more waiting events.
  // 2 -> 3: battles need something to catch with.
  if (fromVersion < POKEMON_SNAPSHOT_VERSION) grantStartingItems(state);
}

}  // namespace pokemon

#endif
