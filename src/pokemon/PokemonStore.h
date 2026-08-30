#pragma once

#include <PokemonGame.h>
#include <PokemonStoreCodec.h>

#include <cstddef>
#include <cstdint>
#include <span>

namespace pokemon {

enum class StoreBeginResult : uint8_t {
  Empty,
  Ready,
  Corrupt,
  Unsupported,
};

enum class PcOrder : uint8_t {
  CatchDate,
  PokedexNumber,
  Alphabetical,
};

class PokemonStore {
 public:
  StoreBeginResult begin();
  bool isReady() const { return ready_; }
  uint32_t recordCount() const { return ready_ ? activeHeader_.recordCount : 0; }
  bool loadState(PokemonState& output) const;
  bool readRecord(uint32_t recordId, PokemonRecord& output) const;
  bool loadOwnedEvolutionNeeds(OwnedEvolutionNeeds& output) const;
  size_t readPcPage(PcOrder order, size_t offset, std::span<PokemonRecord> output) const;
  bool commit(const PokemonState& state, const RecordMutation& mutation = {});
  bool reset();

 private:
  SnapshotHeader activeHeader_{};
  bool activeIsA_ = false;
  bool ready_ = false;
  bool writable_ = false;
};

}  // namespace pokemon
