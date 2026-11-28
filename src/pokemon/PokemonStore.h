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

// Fills in what a record gains when a save from before battles is upgraded: the moves a Pokemon
// of its species and level knows. Everything else a record gained starts out empty.
using RecordUpgradeFn = void (*)(PokemonRecord& record);

class PokemonStore {
 public:
  explicit PokemonStore(const RecordUpgradeFn upgradeRecord = nullptr) : upgradeRecord_(upgradeRecord) {}

  // Finds the newest valid snapshot. A snapshot from an older version is rewritten in the
  // current one before this returns, and the old file is kept as a backup.
  StoreBeginResult begin();
  bool isReady() const { return ready_; }
  uint32_t recordCount() const { return ready_ ? activeHeader_.recordCount : 0; }
  bool loadState(PokemonState& output) const;
  bool readRecord(uint32_t recordId, PokemonRecord& output) const;
  bool loadOwnedEvolutionNeeds(OwnedEvolutionNeeds& output) const;
  bool readPcPage(PcOrder order, size_t offset, std::span<PokemonRecord> output, size_t& count) const;
  bool commit(const PokemonState& state, const RecordMutation& mutation = {});
  // Saves the state and every record change in `mutations` as one snapshot: all of it is saved
  // or none of it. Takes up to RECORD_MUTATION_CAPACITY entries, at most one of them an Append,
  // and no record twice. Entries of kind None are skipped.
  bool commit(const PokemonState& state, std::span<const RecordMutation> mutations);
  bool reset();

 private:
  bool writeSnapshot(const PokemonState& state, std::span<const RecordMutation> mutations, bool discardRecords);
  bool writeInactiveSnapshot(const PokemonState& state, std::span<const RecordMutation> mutations, bool discardRecords,
                             SnapshotHeader& header);
  bool upgradeActiveSnapshot();

  RecordUpgradeFn upgradeRecord_ = nullptr;

  SnapshotHeader activeHeader_{};
  // Set by begin() when a single corrupt slot has no fallback (an interrupted
  // first save). reset() may then replace it; nothing else may write.
  const char* resettableCorruptPath_ = nullptr;
  bool activeIsA_ = false;
  bool ready_ = false;
  bool writable_ = false;
};

}  // namespace pokemon
