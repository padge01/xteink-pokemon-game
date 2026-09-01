#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "PokemonTypes.h"

namespace pokemon {

constexpr size_t POKEMON_STATE_BYTES = 96;
using StateBytes = std::array<uint8_t, POKEMON_STATE_BYTES>;

constexpr size_t POKEMON_SNAPSHOT_HEADER_BYTES = 24;
constexpr size_t POKEMON_SNAPSHOT_CRC_BYTES = 4;
constexpr uint32_t POKEMON_SNAPSHOT_CRC32_INITIAL = 0xFFFFFFFFU;
using HeaderBytes = std::array<uint8_t, POKEMON_SNAPSHOT_HEADER_BYTES>;

struct SnapshotHeader {
  uint32_t sequence = 0;
  uint32_t recordCount = 0;

  bool operator==(const SnapshotHeader&) const = default;
};

enum class HeaderDecodeResult : uint8_t {
  Ready,
  Corrupt,
  Unsupported,
};

bool encodeState(const PokemonState& state, StateBytes& output);
bool decodeState(const StateBytes& bytes, PokemonState& output);
bool encodeSnapshotHeader(const SnapshotHeader& header, HeaderBytes& output);
HeaderDecodeResult decodeSnapshotHeader(const HeaderBytes& bytes, SnapshotHeader& output);
uint64_t snapshotFileBytes(const SnapshotHeader& header);
uint32_t updateSnapshotCrc32(uint32_t crc, const uint8_t* data, size_t size);
constexpr uint32_t finishSnapshotCrc32(const uint32_t crc) { return crc ^ 0xFFFFFFFFU; }

}  // namespace pokemon
