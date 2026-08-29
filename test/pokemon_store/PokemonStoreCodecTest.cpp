#include <cstdint>
#include <cstdio>

#include "Pokemon/PokemonStoreCodec.h"

namespace {

int failures = 0;

#define CHECK(condition)                                                                  \
  do {                                                                                    \
    if (!(condition)) {                                                                   \
      std::fprintf(stderr, "%s:%d check failed: %s\n", __FILE__, __LINE__, #condition); \
      ++failures;                                                                         \
    }                                                                                     \
  } while (false)

uint32_t read32(const pokemon::StateBytes& bytes, const size_t offset) {
  return static_cast<uint32_t>(bytes[offset]) | (static_cast<uint32_t>(bytes[offset + 1]) << 8U) |
         (static_cast<uint32_t>(bytes[offset + 2]) << 16U) |
         (static_cast<uint32_t>(bytes[offset + 3]) << 24U);
}

uint16_t readHeader16(const pokemon::HeaderBytes& bytes, const size_t offset) {
  return static_cast<uint16_t>(bytes[offset]) |
         static_cast<uint16_t>(static_cast<uint16_t>(bytes[offset + 1]) << 8U);
}

uint32_t readHeader32(const pokemon::HeaderBytes& bytes, const size_t offset) {
  return static_cast<uint32_t>(bytes[offset]) | (static_cast<uint32_t>(bytes[offset + 1]) << 8U) |
         (static_cast<uint32_t>(bytes[offset + 2]) << 16U) |
         (static_cast<uint32_t>(bytes[offset + 3]) << 24U);
}

void writeHeader32(pokemon::HeaderBytes& bytes, const size_t offset, const uint32_t value) {
  bytes[offset] = static_cast<uint8_t>(value);
  bytes[offset + 1] = static_cast<uint8_t>(value >> 8U);
  bytes[offset + 2] = static_cast<uint8_t>(value >> 16U);
  bytes[offset + 3] = static_cast<uint8_t>(value >> 24U);
}

void stateCodecUsesTheCanonical96ByteLayout() {
  static_assert(pokemon::POKEMON_STATE_BYTES == 96);
  pokemon::PokemonState state{};
  state.partyRecordIds[0] = 7;
  state.partyRecordIds[1] = 9;
  state.pending.kind = pokemon::PendingEventKind::Item;
  state.pending.item = pokemon::EvolutionItem::LinkCable;
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
  CHECK(read32(bytes, 0) == 7);
  CHECK(read32(bytes, 4) == 9);
  CHECK(bytes[32] == static_cast<uint8_t>(pokemon::EvolutionItem::LinkCable));
  CHECK(bytes[33] == static_cast<uint8_t>(pokemon::PendingEventKind::Item));
  CHECK(bytes[34] == 1 && bytes[35] == 0);
  CHECK(bytes[46] == 0x05);
  CHECK(bytes[65] == 0x01);
  CHECK(read32(bytes, 84) == 0x11223344U);
  CHECK(read32(bytes, 88) == 0x55667788U);
  CHECK(bytes[92] == 59);
  CHECK(bytes[93] == 5);
  CHECK(bytes[94] == 19);
  CHECK(bytes[95] == static_cast<uint8_t>(pokemon::DashboardNotice::ItemFound));

  pokemon::PokemonState decoded{};
  CHECK(pokemon::decodeState(bytes, decoded));
  CHECK(decoded == state);
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
  bytes[92] = 60;
  pokemon::PokemonState output{};
  output.lifetimeMinutes = 77;
  const pokemon::PokemonState before = output;

  CHECK(!pokemon::decodeState(bytes, output));
  CHECK(output == before);
}

void snapshotHeaderUsesCanonical24ByteLayout() {
  static_assert(pokemon::POKEMON_SNAPSHOT_HEADER_BYTES == 24);
  const pokemon::SnapshotHeader header{0x01020304U, 3};
  pokemon::HeaderBytes bytes{};

  CHECK(pokemon::encodeSnapshotHeader(header, bytes));
  CHECK(bytes[0] == 'P' && bytes[1] == 'K' && bytes[2] == 'V' && bytes[3] == '2');
  CHECK(readHeader16(bytes, 4) == 1);
  CHECK(readHeader16(bytes, 6) == 24);
  CHECK(readHeader32(bytes, 8) == 0x01020304U);
  CHECK(readHeader16(bytes, 12) == 96);
  CHECK(readHeader16(bytes, 14) == 48);
  CHECK(readHeader32(bytes, 16) == 3);
  CHECK(readHeader32(bytes, 20) == 240);
  CHECK(pokemon::snapshotFileBytes(header) == 268);

  pokemon::SnapshotHeader decoded{};
  CHECK(pokemon::decodeSnapshotHeader(bytes, decoded) == pokemon::HeaderDecodeResult::Ready);
  CHECK(decoded == header);
}

void unknownSnapshotVersionIsUnsupportedWithoutMutation() {
  pokemon::HeaderBytes bytes{};
  CHECK(pokemon::encodeSnapshotHeader({1, 0}, bytes));
  bytes[4] = 2;
  pokemon::SnapshotHeader output{77, 88};
  const pokemon::SnapshotHeader before = output;

  CHECK(pokemon::decodeSnapshotHeader(bytes, output) == pokemon::HeaderDecodeResult::Unsupported);
  CHECK(output == before);
}

void malformedSupportedHeaderIsCorruptWithoutMutation() {
  pokemon::HeaderBytes canonical{};
  CHECK(pokemon::encodeSnapshotHeader({1, 3}, canonical));
  for (uint8_t variant = 0; variant < 6; ++variant) {
    pokemon::HeaderBytes bytes = canonical;
    if (variant == 0) bytes[0] = 'X';
    if (variant == 1) writeHeader32(bytes, 8, 0);
    if (variant == 2) bytes[6] = 23;
    if (variant == 3) bytes[12] = 95;
    if (variant == 4) bytes[14] = 47;
    if (variant == 5) writeHeader32(bytes, 20, 239);
    pokemon::SnapshotHeader output{77, 88};
    const pokemon::SnapshotHeader before = output;

    CHECK(pokemon::decodeSnapshotHeader(bytes, output) == pokemon::HeaderDecodeResult::Corrupt);
    CHECK(output == before);
  }
}

void unencodableHeaderDoesNotMutateOutput() {
  pokemon::HeaderBytes output{};
  output.fill(0xA5);
  const pokemon::HeaderBytes before = output;

  CHECK(!pokemon::encodeSnapshotHeader({0, 0}, output));
  CHECK(output == before);
  CHECK(!pokemon::encodeSnapshotHeader({1, UINT32_MAX}, output));
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
  stateCodecUsesTheCanonical96ByteLayout();
  invalidStateDoesNotMutateEncodedOutput();
  invalidStateBytesDoNotMutateDecodedOutput();
  snapshotHeaderUsesCanonical24ByteLayout();
  unknownSnapshotVersionIsUnsupportedWithoutMutation();
  malformedSupportedHeaderIsCorruptWithoutMutation();
  unencodableHeaderDoesNotMutateOutput();
  crc32MatchesTheStandardVectorAcrossChunks();
  return failures == 0 ? 0 : 1;
}
