#pragma once

#include <PokemonTracker.h>

#include <array>
#include <string_view>

#include "PokemonStore.h"

namespace pokemon {

enum class ServiceStatus : uint8_t {
  Ok,
  Empty,
  AlreadyStarted,
  Invalid,
  NotFound,
  PartyFull,
  LastPokemon,
  NotApplicable,
  StorageError,
};

struct PokemonSnapshot {
  PokemonState state{};
  std::array<PokemonRecord, PARTY_SIZE> party{};
  uint32_t ownedCount = 0;
  uint8_t partyCount = 0;
};

struct PokemonDashboardSnapshot {
  PokemonRecord leader{};
  PendingEvent pending{};
  DashboardNotice notice = DashboardNotice::None;
};

class PokemonService {
 public:
  PokemonService(PokemonStore& store, RandomSource random)
      : store_(store), random_(random), tracker_(&PokemonService::creditFromTracker, this) {}

  bool beginReadingSession();
  void setBookProgressPercent(uint8_t percent);
  void onSuccessfulPageTurn(uint32_t nowMs);
  void checkpointIfDue(uint32_t nowMs);
  void flushOnExit(uint32_t nowMs);

  bool creditMinutes(uint16_t minutes, uint8_t bookProgressPercent);

  ServiceStatus loadSnapshot(PokemonSnapshot& output);
  ServiceStatus createStarter(uint16_t speciesId, Gender gender, std::string_view nickname);
  ServiceStatus readRecord(uint32_t recordId, PokemonRecord& output);
  ServiceStatus renamePokemon(uint32_t recordId, std::string_view nickname);
  ServiceStatus movePartyMember(uint8_t fromSlot, uint8_t toSlot);
  ServiceStatus depositPokemon(uint32_t recordId);
  ServiceStatus withdrawPokemon(uint32_t recordId);
  ServiceStatus loadDashboardSnapshot(PokemonDashboardSnapshot& output);
  ServiceStatus readPcPage(PcOrder order, size_t offset, std::span<PokemonRecord> output, size_t& count);
  ServiceStatus resolveEncounter(EncounterChoice choice, uint32_t& caughtRecordId);
  ServiceStatus acknowledgeItem();
  ServiceStatus resolveEvolution(EvolutionChoice choice);
  ServiceStatus setEvolutionPrompts(uint32_t recordId, bool enabled);
  ServiceStatus useEvolutionItem(uint32_t recordId, EvolutionItem item);
  ServiceStatus reset();

 private:
  ServiceStatus prepareStore();
  ServiceStatus loadReadyState(PokemonState& output);
  static bool creditFromTracker(void* context, uint16_t minutes, uint8_t bookProgressPercent);

  PokemonStore& store_;
  RandomSource random_{};
  PokemonTracker tracker_;
  bool readingSessionActive_ = false;
};

PokemonService& devicePokemonService();

}  // namespace pokemon
