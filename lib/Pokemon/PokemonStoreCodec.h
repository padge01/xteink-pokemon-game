#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "PokemonTypes.h"

namespace pokemon {

// Snapshot versions. Each one is read by its own decoder and brought forward one step at a time
// (see upgradeState()), so a new version only ever has to describe what it changed.
//   1: one waiting event.
//   2: three waiting events.
//   3: battles. Records grow from 48 to 64 bytes to carry moves, health and status; the state
//      gains the bag, league progress, the map position, the Hall of Fame, an unfinished battle
//      and eight waiting events.
constexpr uint16_t POKEMON_SNAPSHOT_VERSION_V1 = 1;
constexpr uint16_t POKEMON_SNAPSHOT_VERSION_V2 = 2;
constexpr uint16_t POKEMON_SNAPSHOT_VERSION = 3;
constexpr size_t POKEMON_STATE_V1_BYTES = 96;
constexpr size_t POKEMON_STATE_V2_BYTES = 116;
constexpr size_t POKEMON_STATE_BYTES = 320;
// Where the state repeats the snapshot's sequence number.
constexpr size_t POKEMON_STATE_SEQUENCE_OFFSET = 158;
using StateBytes = std::array<uint8_t, POKEMON_STATE_BYTES>;

constexpr size_t POKEMON_SNAPSHOT_HEADER_BYTES = 24;
constexpr size_t POKEMON_SNAPSHOT_CRC_BYTES = 4;
constexpr uint32_t POKEMON_SNAPSHOT_CRC32_INITIAL = 0xFFFFFFFFU;
using HeaderBytes = std::array<uint8_t, POKEMON_SNAPSHOT_HEADER_BYTES>;

struct SnapshotHeader {
  uint16_t version = POKEMON_SNAPSHOT_VERSION;
  uint32_t sequence = 0;
  uint32_t recordCount = 0;

  constexpr SnapshotHeader() = default;
  constexpr SnapshotHeader(const uint32_t sequenceValue, const uint32_t recordCountValue)
      : sequence(sequenceValue), recordCount(recordCountValue) {}
  constexpr SnapshotHeader(const uint16_t versionValue, const uint32_t sequenceValue, const uint32_t recordCountValue)
      : version(versionValue), sequence(sequenceValue), recordCount(recordCountValue) {}
  bool operator==(const SnapshotHeader&) const = default;
};

enum class HeaderDecodeResult : uint8_t {
  Ready,
  Corrupt,
  Unsupported,
};

bool encodeState(const PokemonState& state, StateBytes& output);
// Reads a state written by any supported version. What an older version did not store comes out
// empty; upgradeState() then applies what the newer rules add.
bool decodeState(const uint8_t* bytes, size_t size, uint16_t version, PokemonState& output);
// Brings a state decoded from an older snapshot up to the current rules, one version at a time.
void upgradeState(PokemonState& state, uint16_t fromVersion);
inline bool decodeState(const StateBytes& bytes, PokemonState& output) {
  return decodeState(bytes.data(), bytes.size(), POKEMON_SNAPSHOT_VERSION, output);
}
bool encodeSnapshotHeader(const SnapshotHeader& header, HeaderBytes& output);
HeaderDecodeResult decodeSnapshotHeader(const HeaderBytes& bytes, SnapshotHeader& output);
size_t snapshotStateBytes(uint16_t version);
size_t snapshotRecordBytes(uint16_t version);
uint64_t snapshotFileBytes(const SnapshotHeader& header);
uint32_t updateSnapshotCrc32(uint32_t crc, const uint8_t* data, size_t size);
constexpr uint32_t finishSnapshotCrc32(const uint32_t crc) { return crc ^ 0xFFFFFFFFU; }

}  // namespace pokemon
