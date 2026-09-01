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

void stateCodecUsesTheCanonical116ByteLayout() {
  static_assert(pokemon::POKEMON_STATE_BYTES == 116);
  static_assert(pokemon::POKEMON_STATE_V1_BYTES == 96);
  pokemon::PokemonState state{};
  state.partyRecordIds[0] = 7;
  state.partyRecordIds[1] = 9;
  state.pendingEvents[0] = {0, 25, 5, pokemon::Gender::Female, pokemon::EvolutionItem::None,
                            pokemon::PendingEventKind::Encounter};
  state.pendingEvents[1] = {0, 0, 0, pokemon::Gender::Unknown, pokemon::EvolutionItem::LinkCable,
                            pokemon::PendingEventKind::Item};
  state.pendingEvents[2] = {7, 26, 0, pokemon::Gender::Unknown, pokemon::EvolutionItem::None,
                            pokemon::PendingEventKind::Evolution};
  state.itemCounts = {1, 2, 3, 4, 5, 6};
  state.seenSpecies[0] = 0x05;
  state.caughtSpecies[0] = 0x01;
  state.lifetimeMinutes = 0x11223344U;
  state.sequence = 0x55667788U;
  state.readingMinuteRemainder = 59;
  state.encounterMisses = 5;
  state.itemMisses = 19;
  state.dashboardNotice = pokemon::DashboardNotice::ItemFound;

  pokemon::StateBytes bytes{};
  CHECK(pokemon::encodeState(state, bytes));
  CHECK(read32(bytes.data(), 0) == 7);
  CHECK(read32(bytes.data(), 4) == 9);
  CHECK(bytes[33] == static_cast<uint8_t>(pokemon::PendingEventKind::Encounter));
  CHECK(bytes[42] == static_cast<uint8_t>(pokemon::EvolutionItem::LinkCable));
  CHECK(bytes[43] == static_cast<uint8_t>(pokemon::PendingEventKind::Item));
  CHECK(bytes[53] == static_cast<uint8_t>(pokemon::PendingEventKind::Evolution));
  CHECK(bytes[54] == 1 && bytes[55] == 0);
  CHECK(bytes[66] == 0x05);
  CHECK(bytes[85] == 0x01);
  CHECK(read32(bytes.data(), 104) == 0x11223344U);
  CHECK(read32(bytes.data(), 108) == 0x55667788U);
  CHECK(bytes[112] == 59);
  CHECK(bytes[113] == 5);
  CHECK(bytes[114] == 19);
  CHECK(bytes[115] == static_cast<uint8_t>(pokemon::DashboardNotice::ItemFound));

  pokemon::PokemonState decoded{};
  CHECK(pokemon::decodeState(bytes.data(), bytes.size(), pokemon::POKEMON_SNAPSHOT_VERSION, decoded));
  CHECK(decoded == state);
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
  bytes[112] = 60;
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
  CHECK(readHeader16(bytes, 12) == 116);
  CHECK(readHeader16(bytes, 14) == 48);
  CHECK(readHeader32(bytes, 16) == 3);
  CHECK(readHeader32(bytes, 20) == 260);
  CHECK(pokemon::snapshotFileBytes(header) == 288);
  CHECK(pokemon::snapshotStateBytes(pokemon::POKEMON_SNAPSHOT_VERSION_V1) == 96);
  CHECK(pokemon::snapshotStateBytes(pokemon::POKEMON_SNAPSHOT_VERSION) == 116);
  CHECK(pokemon::snapshotStateBytes(3) == 0);

  pokemon::SnapshotHeader decoded{};
  CHECK(pokemon::decodeSnapshotHeader(bytes, decoded) == pokemon::HeaderDecodeResult::Ready);
  CHECK(decoded == header);
}

void unknownSnapshotVersionIsUnsupportedWithoutMutation() {
  pokemon::HeaderBytes bytes{};
  CHECK(pokemon::encodeSnapshotHeader({pokemon::POKEMON_SNAPSHOT_VERSION, 1, 0}, bytes));
  bytes[4] = 3;
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
    if (variant == 3) bytes[12] = 115;
    if (variant == 4) bytes[14] = 47;
    if (variant == 5) writeHeader32(bytes, 20, 259);
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
  stateCodecUsesTheCanonical116ByteLayout();
  legacyStateDecodesItsPendingEventIntoTheQueue();
  invalidStateDoesNotMutateEncodedOutput();
  invalidStateBytesDoNotMutateDecodedOutput();
  snapshotHeaderUsesCanonical24ByteLayout();
  unknownSnapshotVersionIsUnsupportedWithoutMutation();
  malformedSupportedHeaderIsCorruptWithoutMutation();
  unencodableHeaderDoesNotMutateOutput();
  crc32MatchesTheStandardVectorAcrossChunks();
  return failures == 0 ? 0 : 1;
}
