#include <cstdint>
#include <cstdio>

#include "Pokemon/PokemonStoreCodec.h"

namespace {

int failures = 0;

#define CHECK(condition)                                                                \
  do {                                                                                  \
    if (!(condition)) {                                                                 \
      std::fprintf(stderr, "%s:%d check failed: %s\n", __FILE__, __LINE__, #condition); \
      ++failures;                                                                       \
    }                                                                                   \
  } while (false)

uint32_t read32(const uint8_t* bytes, const size_t offset) {
  return static_cast<uint32_t>(bytes[offset]) | (static_cast<uint32_t>(bytes[offset + 1]) << 8U) |
         (static_cast<uint32_t>(bytes[offset + 2]) << 16U) | (static_cast<uint32_t>(bytes[offset + 3]) << 24U);
}

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

uint16_t readHeader16(const pokemon::HeaderBytes& bytes, const size_t offset) {
  return static_cast<uint16_t>(bytes[offset]) | static_cast<uint16_t>(static_cast<uint16_t>(bytes[offset + 1]) << 8U);
}

uint32_t readHeader32(const pokemon::HeaderBytes& bytes, const size_t offset) {
  return static_cast<uint32_t>(bytes[offset]) | (static_cast<uint32_t>(bytes[offset + 1]) << 8U) |
         (static_cast<uint32_t>(bytes[offset + 2]) << 16U) | (static_cast<uint32_t>(bytes[offset + 3]) << 24U);
}

void writeHeader32(pokemon::HeaderBytes& bytes, const size_t offset, const uint32_t value) {
  bytes[offset] = static_cast<uint8_t>(value);
  bytes[offset + 1] = static_cast<uint8_t>(value >> 8U);
  bytes[offset + 2] = static_cast<uint8_t>(value >> 16U);
  bytes[offset + 3] = static_cast<uint8_t>(value >> 24U);
}

void stateCodecUsesTheCanonical320ByteLayout() {
  static_assert(pokemon::POKEMON_STATE_BYTES == 320);
  static_assert(pokemon::POKEMON_STATE_V2_BYTES == 116);
  static_assert(pokemon::POKEMON_STATE_V1_BYTES == 96);
  static_assert(pokemon::POKEMON_STATE_SEQUENCE_OFFSET == 158);
  pokemon::PokemonState state{};
  state.partyRecordIds[0] = 7;
  state.partyRecordIds[1] = 9;
  state.pendingEvents[0] = {
      0, 25, 5, pokemon::Gender::Female, pokemon::EvolutionItem::None, pokemon::PendingEventKind::Encounter};
  state.pendingEvents[1] = {
      0, 0, 0, pokemon::Gender::Unknown, pokemon::EvolutionItem::LinkCable, pokemon::PendingEventKind::Item};
  state.pendingEvents[2] = {
      7, 26, 0, pokemon::Gender::Unknown, pokemon::EvolutionItem::None, pokemon::PendingEventKind::Evolution};
  for (size_t index = 3; index < state.pendingEvents.size(); ++index)
    state.pendingEvents[index] = state.pendingEvents[1];
  state.itemCounts = {1, 2, 3, 4, 5, 6};
  state.seenSpecies[0] = 0x05;
  state.caughtSpecies[0] = 0x01;
  state.lifetimeMinutes = 0x11223344U;
  state.sequence = 0x55667788U;
  state.readingMinuteRemainder = 59;
  state.encounterMisses = 5;
  state.itemMisses = 19;
  state.dashboardNotice = pokemon::DashboardNotice::ItemFound;
  state.battleProgress = 0x1003;
  state.map = {4, 9, 0x0203};
  state.bagCounts.front() = 10;
  state.bagCounts.back() = 99;
  state.hallOfFame.team[0] = {6, 62};
  state.hallOfFame.team[5] = {143, 55};
  for (size_t slot = 1; slot < 5; ++slot) state.hallOfFame.team[slot] = {25, 50};
  state.hallOfFame.lifetimeMinutes = 0x0A0B0C0DU;
  state.savedBattle.kind = pokemon::SavedBattleKind::Wild;
  state.savedBattle.playerSlot = 1;
  state.savedBattle.foes[0] = {0x0111, 4};

  pokemon::StateBytes bytes{};
  CHECK(pokemon::encodeState(state, bytes));
  CHECK(read32(bytes.data(), 0) == 7);
  CHECK(read32(bytes.data(), 4) == 9);
  CHECK(bytes[33] == static_cast<uint8_t>(pokemon::PendingEventKind::Encounter));
  CHECK(bytes[42] == static_cast<uint8_t>(pokemon::EvolutionItem::LinkCable));
  CHECK(bytes[43] == static_cast<uint8_t>(pokemon::PendingEventKind::Item));
  CHECK(bytes[53] == static_cast<uint8_t>(pokemon::PendingEventKind::Evolution));
  CHECK(bytes[103] == static_cast<uint8_t>(pokemon::PendingEventKind::Item));
  CHECK(bytes[104] == 1 && bytes[105] == 0);
  CHECK(bytes[116] == 0x05);
  CHECK(bytes[135] == 0x01);
  CHECK(read32(bytes.data(), 154) == 0x11223344U);
  CHECK(read32(bytes.data(), 158) == 0x55667788U);
  CHECK(bytes[162] == 59);
  CHECK(bytes[163] == 5);
  CHECK(bytes[164] == 19);
  CHECK(bytes[165] == static_cast<uint8_t>(pokemon::DashboardNotice::ItemFound));
  CHECK(bytes[166] == 0x03 && bytes[167] == 0x10);
  CHECK(bytes[168] == 4 && bytes[169] == 9 && bytes[170] == 0x03 && bytes[171] == 0x02);
  CHECK(bytes[172] == 10);
  CHECK(bytes[260] == 99);
  CHECK(bytes[261] == 6 && bytes[262] == 0 && bytes[263] == 62);
  CHECK(bytes[276] == 143 && bytes[278] == 55);
  CHECK(read32(bytes.data(), 279) == 0x0A0B0C0DU);
  CHECK(bytes[283] == static_cast<uint8_t>(pokemon::SavedBattleKind::Wild));
  CHECK(bytes[284] == 0 && bytes[285] == 1 && bytes[286] == 0);
  CHECK(bytes[287] == 0x11 && bytes[288] == 0x01 && bytes[289] == 4);
  for (size_t index = 305; index < bytes.size(); ++index) CHECK(bytes[index] == 0);

  pokemon::PokemonState decoded{};
  CHECK(pokemon::decodeState(bytes.data(), bytes.size(), pokemon::POKEMON_SNAPSHOT_VERSION, decoded));
  CHECK(decoded == state);

  // The spare bytes must stay empty.
  bytes[319] = 1;
  const pokemon::PokemonState before = decoded;
  CHECK(!pokemon::decodeState(bytes.data(), bytes.size(), pokemon::POKEMON_SNAPSHOT_VERSION, decoded));
  CHECK(decoded == before);
}

void legacyStateDecodesItsPendingEventIntoTheQueue() {
  std::array<uint8_t, pokemon::POKEMON_STATE_V1_BYTES> bytes{};
  write32(bytes.data(), 0, 7);
  write16(bytes.data(), 28, 133);
  bytes[30] = 12;
  bytes[31] = static_cast<uint8_t>(pokemon::Gender::Female);
  bytes[33] = static_cast<uint8_t>(pokemon::PendingEventKind::Encounter);
  bytes[46] = 0x01;
  bytes[65] = 0x01;
  write32(bytes.data(), 84, 60);
  write32(bytes.data(), 88, 9);
  bytes[95] = static_cast<uint8_t>(pokemon::DashboardNotice::NewPokemon);

  pokemon::PokemonState decoded{};
  CHECK(pokemon::decodeState(bytes.data(), bytes.size(), pokemon::POKEMON_SNAPSHOT_VERSION_V1, decoded));
  CHECK(decoded.partyRecordIds[0] == 7);
  CHECK(decoded.pendingEvents[0].kind == pokemon::PendingEventKind::Encounter);
  CHECK(decoded.pendingEvents[0].speciesId == 133);
  CHECK(decoded.pendingEvents[1].kind == pokemon::PendingEventKind::None);
  CHECK(decoded.pendingEvents[2].kind == pokemon::PendingEventKind::None);
  CHECK(decoded.sequence == 9);
}

void versionTwoStateDecodesIntoTheCurrentShape() {
  std::array<uint8_t, pokemon::POKEMON_STATE_V2_BYTES> bytes{};
  write32(bytes.data(), 0, 7);
  write16(bytes.data(), 28, 133);
  bytes[30] = 12;
  bytes[31] = static_cast<uint8_t>(pokemon::Gender::Female);
  bytes[33] = static_cast<uint8_t>(pokemon::PendingEventKind::Encounter);
  bytes[42] = static_cast<uint8_t>(pokemon::EvolutionItem::LinkCable);
  bytes[43] = static_cast<uint8_t>(pokemon::PendingEventKind::Item);
  write32(bytes.data(), 44, 7);
  write16(bytes.data(), 48, 26);
  bytes[53] = static_cast<uint8_t>(pokemon::PendingEventKind::Evolution);
  write16(bytes.data(), 64, 2);
  bytes[66] = 0x01;
  bytes[85] = 0x01;
  write32(bytes.data(), 104, 600);
  write32(bytes.data(), 108, 41);
  bytes[112] = 30;
  bytes[113] = 2;
  bytes[114] = 11;
  bytes[115] = static_cast<uint8_t>(pokemon::DashboardNotice::NewPokemon);

  pokemon::PokemonState decoded{};
  CHECK(pokemon::decodeState(bytes.data(), bytes.size(), pokemon::POKEMON_SNAPSHOT_VERSION_V2, decoded));
  CHECK(decoded.partyRecordIds[0] == 7);
  CHECK(decoded.pendingEvents[0].speciesId == 133);
  CHECK(decoded.pendingEvents[1].item == pokemon::EvolutionItem::LinkCable);
  CHECK(decoded.pendingEvents[2].kind == pokemon::PendingEventKind::Evolution);
  CHECK(decoded.pendingEvents[2].recordId == 7);
  CHECK(decoded.pendingEvents[3].kind == pokemon::PendingEventKind::None);
  CHECK(decoded.itemCounts[5] == 2);
  CHECK(decoded.lifetimeMinutes == 600);
  CHECK(decoded.sequence == 41);
  CHECK(decoded.readingMinuteRemainder == 30);
  CHECK(decoded.encounterMisses == 2);
  CHECK(decoded.itemMisses == 11);
  CHECK(decoded.dashboardNotice == pokemon::DashboardNotice::NewPokemon);

  // Everything version 3 added starts out empty: no bag, no badges, at the first town.
  pokemon::PokemonState expectedNewFields{};
  CHECK(decoded.bagCounts == expectedNewFields.bagCounts);
  CHECK(decoded.battleProgress == 0);
  CHECK(decoded.map == pokemon::MapPosition{});
  CHECK(decoded.hallOfFame == pokemon::HallOfFame{});
  CHECK(decoded.savedBattle == pokemon::SavedBattle{});

  // The state size has to match the version it claims.
  CHECK(!pokemon::decodeState(bytes.data(), bytes.size(), pokemon::POKEMON_SNAPSHOT_VERSION, decoded));
  CHECK(!pokemon::decodeState(bytes.data(), bytes.size(), pokemon::POKEMON_SNAPSHOT_VERSION_V1, decoded));
}

void upgradingAnOlderStateGrantsTheStartingItemsOnce() {
  for (const uint16_t olderVersion : {pokemon::POKEMON_SNAPSHOT_VERSION_V1, pokemon::POKEMON_SNAPSHOT_VERSION_V2}) {
    pokemon::PokemonState state{};
    pokemon::upgradeState(state, olderVersion);
    CHECK(pokemon::bagCount(state, pokemon::POKEMON_ITEM_POKE_BALL) == pokemon::POKEMON_STARTING_POKE_BALLS);
    CHECK(pokemon::bagCount(state, pokemon::POKEMON_ITEM_POTION) == pokemon::POKEMON_STARTING_POTIONS);
    CHECK(pokemon::validateState(state));
  }

  // A state that is already current is left alone.
  pokemon::PokemonState current{};
  pokemon::upgradeState(current, pokemon::POKEMON_SNAPSHOT_VERSION);
  CHECK(current == pokemon::PokemonState{});
}

void invalidStateDoesNotMutateEncodedOutput() {
  pokemon::PokemonState state{};
  state.readingMinuteRemainder = 60;
  pokemon::StateBytes output{};
  output.fill(0xA5);
  const pokemon::StateBytes before = output;

  CHECK(!pokemon::encodeState(state, output));
  CHECK(output == before);
}

void invalidStateBytesDoNotMutateDecodedOutput() {
  pokemon::PokemonState valid{};
  pokemon::StateBytes bytes{};
  CHECK(pokemon::encodeState(valid, bytes));
  bytes[162] = 60;  // the reading minute remainder
  pokemon::PokemonState output{};
  output.lifetimeMinutes = 77;
  const pokemon::PokemonState before = output;

  CHECK(!pokemon::decodeState(bytes.data(), bytes.size(), pokemon::POKEMON_SNAPSHOT_VERSION, output));
  CHECK(output == before);
}

void snapshotHeaderUsesCanonical24ByteLayout() {
  static_assert(pokemon::POKEMON_SNAPSHOT_HEADER_BYTES == 24);
  const pokemon::SnapshotHeader header{pokemon::POKEMON_SNAPSHOT_VERSION, 0x01020304U, 3};
  pokemon::HeaderBytes bytes{};

  CHECK(pokemon::encodeSnapshotHeader(header, bytes));
  CHECK(bytes[0] == 'P' && bytes[1] == 'K' && bytes[2] == 'V' && bytes[3] == '2');
  CHECK(readHeader16(bytes, 4) == pokemon::POKEMON_SNAPSHOT_VERSION);
  CHECK(readHeader16(bytes, 6) == 24);
  CHECK(readHeader32(bytes, 8) == 0x01020304U);
  CHECK(readHeader16(bytes, 12) == 320);
  CHECK(readHeader16(bytes, 14) == 64);
  CHECK(readHeader32(bytes, 16) == 3);
  CHECK(readHeader32(bytes, 20) == 512);
  CHECK(pokemon::snapshotFileBytes(header) == 540);
  CHECK(pokemon::snapshotStateBytes(pokemon::POKEMON_SNAPSHOT_VERSION_V1) == 96);
  CHECK(pokemon::snapshotStateBytes(pokemon::POKEMON_SNAPSHOT_VERSION_V2) == 116);
  CHECK(pokemon::snapshotStateBytes(pokemon::POKEMON_SNAPSHOT_VERSION) == 320);
  CHECK(pokemon::snapshotStateBytes(4) == 0);
  CHECK(pokemon::snapshotRecordBytes(pokemon::POKEMON_SNAPSHOT_VERSION_V1) == 48);
  CHECK(pokemon::snapshotRecordBytes(pokemon::POKEMON_SNAPSHOT_VERSION_V2) == 48);
  CHECK(pokemon::snapshotRecordBytes(pokemon::POKEMON_SNAPSHOT_VERSION) == 64);
  CHECK(pokemon::snapshotRecordBytes(4) == 0);

  pokemon::SnapshotHeader decoded{};
  CHECK(pokemon::decodeSnapshotHeader(bytes, decoded) == pokemon::HeaderDecodeResult::Ready);
  CHECK(decoded == header);
}

void olderSnapshotHeadersKeepTheirOwnSizes() {
  const pokemon::SnapshotHeader header{pokemon::POKEMON_SNAPSHOT_VERSION_V2, 12, 3};
  pokemon::HeaderBytes bytes{};

  CHECK(pokemon::encodeSnapshotHeader(header, bytes));
  CHECK(readHeader16(bytes, 4) == pokemon::POKEMON_SNAPSHOT_VERSION_V2);
  CHECK(readHeader16(bytes, 12) == 116);
  CHECK(readHeader16(bytes, 14) == 48);
  CHECK(readHeader32(bytes, 20) == 260);
  CHECK(pokemon::snapshotFileBytes(header) == 288);

  pokemon::SnapshotHeader decoded{};
  CHECK(pokemon::decodeSnapshotHeader(bytes, decoded) == pokemon::HeaderDecodeResult::Ready);
  CHECK(decoded == header);

  // A version 2 header that claims the current record size does not describe a real file.
  bytes[14] = 64;
  CHECK(pokemon::decodeSnapshotHeader(bytes, decoded) == pokemon::HeaderDecodeResult::Corrupt);
}

void unknownSnapshotVersionIsUnsupportedWithoutMutation() {
  pokemon::HeaderBytes bytes{};
  CHECK(pokemon::encodeSnapshotHeader({pokemon::POKEMON_SNAPSHOT_VERSION, 1, 0}, bytes));
  bytes[4] = 4;
  pokemon::SnapshotHeader output{pokemon::POKEMON_SNAPSHOT_VERSION, 77, 88};
  const pokemon::SnapshotHeader before = output;

  CHECK(pokemon::decodeSnapshotHeader(bytes, output) == pokemon::HeaderDecodeResult::Unsupported);
  CHECK(output == before);
}

void malformedSupportedHeaderIsCorruptWithoutMutation() {
  pokemon::HeaderBytes canonical{};
  CHECK(pokemon::encodeSnapshotHeader({pokemon::POKEMON_SNAPSHOT_VERSION, 1, 3}, canonical));
  for (uint8_t variant = 0; variant < 6; ++variant) {
    pokemon::HeaderBytes bytes = canonical;
    if (variant == 0) bytes[0] = 'X';
    if (variant == 1) writeHeader32(bytes, 8, 0);
    if (variant == 2) bytes[6] = 23;
    if (variant == 3) bytes[12] = 0x3F;  // a state one byte short
    if (variant == 4) bytes[14] = 63;
    if (variant == 5) writeHeader32(bytes, 20, 511);
    pokemon::SnapshotHeader output{pokemon::POKEMON_SNAPSHOT_VERSION, 77, 88};
    const pokemon::SnapshotHeader before = output;

    CHECK(pokemon::decodeSnapshotHeader(bytes, output) == pokemon::HeaderDecodeResult::Corrupt);
    CHECK(output == before);
  }
}

void unencodableHeaderDoesNotMutateOutput() {
  pokemon::HeaderBytes output{};
  output.fill(0xA5);
  const pokemon::HeaderBytes before = output;

  CHECK(!pokemon::encodeSnapshotHeader({pokemon::POKEMON_SNAPSHOT_VERSION, 0, 0}, output));
  CHECK(output == before);
  CHECK(!pokemon::encodeSnapshotHeader({pokemon::POKEMON_SNAPSHOT_VERSION, 1, UINT32_MAX}, output));
  CHECK(output == before);
}

void crc32MatchesTheStandardVectorAcrossChunks() {
  constexpr uint8_t input[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
  const uint32_t oneShot = pokemon::finishSnapshotCrc32(
      pokemon::updateSnapshotCrc32(pokemon::POKEMON_SNAPSHOT_CRC32_INITIAL, input, sizeof(input)));
  uint32_t chunked = pokemon::updateSnapshotCrc32(pokemon::POKEMON_SNAPSHOT_CRC32_INITIAL, input, 4);
  chunked = pokemon::updateSnapshotCrc32(chunked, input + 4, 5);
  chunked = pokemon::finishSnapshotCrc32(chunked);

  CHECK(oneShot == 0xCBF43926U);
  CHECK(chunked == oneShot);
}

}  // namespace

int main() {
  stateCodecUsesTheCanonical320ByteLayout();
  legacyStateDecodesItsPendingEventIntoTheQueue();
  versionTwoStateDecodesIntoTheCurrentShape();
  upgradingAnOlderStateGrantsTheStartingItemsOnce();
  invalidStateDoesNotMutateEncodedOutput();
  invalidStateBytesDoNotMutateDecodedOutput();
  snapshotHeaderUsesCanonical24ByteLayout();
  olderSnapshotHeadersKeepTheirOwnSizes();
  unknownSnapshotVersionIsUnsupportedWithoutMutation();
  malformedSupportedHeaderIsCorruptWithoutMutation();
  unencodableHeaderDoesNotMutateOutput();
  crc32MatchesTheStandardVectorAcrossChunks();
  return failures == 0 ? 0 : 1;
}
