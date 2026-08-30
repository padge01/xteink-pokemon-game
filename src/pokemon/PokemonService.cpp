#if defined(CROSSINK_ENABLE_POKEMON)

#include "PokemonService.h"

#include <Logging.h>
#include <PokemonSpecies.h>

#if !defined(POKEMON_SERVICE_HOST_TEST)
#include <Arduino.h>
#endif

namespace pokemon {
namespace {

#if !defined(POKEMON_SERVICE_HOST_TEST)
uint32_t deviceRandomBelow(void*, const uint32_t upperExclusive) {
  if (upperExclusive == 0) return 0;
  return static_cast<uint32_t>(random(static_cast<long>(upperExclusive)));
}
#endif

}  // namespace

ServiceStatus PokemonService::prepareStore() {
  if (store_.isReady()) return ServiceStatus::Ok;
  switch (store_.begin()) {
    case StoreBeginResult::Ready:
      return ServiceStatus::Ok;
    case StoreBeginResult::Empty:
      return ServiceStatus::Empty;
    case StoreBeginResult::Corrupt:
    case StoreBeginResult::Unsupported:
      return ServiceStatus::StorageError;
  }
  return ServiceStatus::StorageError;
}

ServiceStatus PokemonService::loadSnapshot(PokemonSnapshot& output) {
  output = {};
  const ServiceStatus prepared = prepareStore();
  if (prepared != ServiceStatus::Ok) return prepared;
  if (!store_.loadState(output.state)) {
    LOG_ERR("PokemonService", "Failed to load Pokemon snapshot state");
    return ServiceStatus::StorageError;
  }
  for (size_t slot = 0; slot < PARTY_SIZE && output.state.partyRecordIds[slot] != 0; ++slot) {
    if (!store_.readRecord(output.state.partyRecordIds[slot], output.party[slot])) {
      LOG_ERR("PokemonService", "Failed to load Pokemon snapshot party");
      output = {};
      return ServiceStatus::StorageError;
    }
    ++output.partyCount;
  }
  output.ownedCount = store_.recordCount();
  return ServiceStatus::Ok;
}

ServiceStatus PokemonService::loadReadyState(PokemonState& output) {
  const ServiceStatus prepared = prepareStore();
  if (prepared != ServiceStatus::Ok) return prepared;
  if (!store_.loadState(output)) {
    LOG_ERR("PokemonService", "Failed to load Pokemon state");
    return ServiceStatus::StorageError;
  }
  return ServiceStatus::Ok;
}

ServiceStatus PokemonService::createStarter(const uint16_t speciesId, const Gender gender,
                                            const std::string_view nickname) {
  const ServiceStatus prepared = prepareStore();
  if (prepared == ServiceStatus::Ok) return ServiceStatus::AlreadyStarted;
  if (prepared != ServiceStatus::Empty) return prepared;
  if ((speciesId != 1 && speciesId != 4 && speciesId != 7 && speciesId != 25) ||
      (gender != Gender::Male && gender != Gender::Female)) {
    return ServiceStatus::Invalid;
  }

  PokemonRecord starter{};
  starter.recordId = 1;
  starter.totalXp = xpRequired(5);
  starter.speciesId = speciesId;
  starter.caughtLevel = 5;
  starter.gender = gender;
  starter.origin = Origin::Starter;
  if (!setNickname(starter, nickname) || !validateRecord(starter)) return ServiceStatus::Invalid;

  PokemonState state{};
  state.partyRecordIds[0] = starter.recordId;
  if (!markSpecies(state.seenSpecies, speciesId) || !markSpecies(state.caughtSpecies, speciesId)) {
    return ServiceStatus::Invalid;
  }
  const RecordMutation mutation{starter.recordId, starter, RecordMutationKind::Append};
  if (!store_.commit(state, mutation)) {
    LOG_ERR("PokemonService", "Failed to create starter");
    return ServiceStatus::StorageError;
  }
  return ServiceStatus::Ok;
}

ServiceStatus PokemonService::readRecord(const uint32_t recordId, PokemonRecord& output) {
  const ServiceStatus prepared = prepareStore();
  if (prepared != ServiceStatus::Ok) return prepared;
  return store_.readRecord(recordId, output) ? ServiceStatus::Ok : ServiceStatus::NotFound;
}

ServiceStatus PokemonService::renamePokemon(const uint32_t recordId, const std::string_view nickname) {
  PokemonRecord record{};
  const ServiceStatus readStatus = readRecord(recordId, record);
  if (readStatus != ServiceStatus::Ok) return readStatus;
  if (!setNickname(record, nickname)) return ServiceStatus::Invalid;
  PokemonState state{};
  const ServiceStatus stateStatus = loadReadyState(state);
  if (stateStatus != ServiceStatus::Ok) return stateStatus;
  const RecordMutation mutation{record.recordId, record, RecordMutationKind::Replace};
  if (!store_.commit(state, mutation)) {
    LOG_ERR("PokemonService", "Failed to rename Pokemon");
    return ServiceStatus::StorageError;
  }
  return ServiceStatus::Ok;
}

ServiceStatus PokemonService::movePartyMember(const uint8_t fromSlot, const uint8_t toSlot) {
  PokemonState state{};
  const ServiceStatus stateStatus = loadReadyState(state);
  if (stateStatus != ServiceStatus::Ok) return stateStatus;
  uint8_t partyCount = 0;
  while (partyCount < PARTY_SIZE && state.partyRecordIds[partyCount] != 0) ++partyCount;
  if (fromSlot >= partyCount || toSlot >= partyCount) return ServiceStatus::Invalid;
  if (fromSlot == toSlot) return ServiceStatus::Ok;

  const uint32_t moved = state.partyRecordIds[fromSlot];
  if (fromSlot > toSlot) {
    for (uint8_t slot = fromSlot; slot > toSlot; --slot) {
      state.partyRecordIds[slot] = state.partyRecordIds[slot - 1U];
    }
  } else {
    for (uint8_t slot = fromSlot; slot < toSlot; ++slot) {
      state.partyRecordIds[slot] = state.partyRecordIds[slot + 1U];
    }
  }
  state.partyRecordIds[toSlot] = moved;
  if (!store_.commit(state)) {
    LOG_ERR("PokemonService", "Failed to reorder party");
    return ServiceStatus::StorageError;
  }
  return ServiceStatus::Ok;
}

ServiceStatus PokemonService::depositPokemon(const uint32_t recordId) {
  PokemonState state{};
  const ServiceStatus stateStatus = loadReadyState(state);
  if (stateStatus != ServiceStatus::Ok) return stateStatus;
  uint8_t partyCount = 0;
  while (partyCount < PARTY_SIZE && state.partyRecordIds[partyCount] != 0) ++partyCount;
  uint8_t slot = 0;
  while (slot < partyCount && state.partyRecordIds[slot] != recordId) ++slot;
  if (slot == partyCount) return ServiceStatus::NotFound;
  if (partyCount == 1) return ServiceStatus::LastPokemon;
  for (; slot + 1U < partyCount; ++slot) state.partyRecordIds[slot] = state.partyRecordIds[slot + 1U];
  state.partyRecordIds[partyCount - 1U] = 0;
  if (!store_.commit(state)) {
    LOG_ERR("PokemonService", "Failed to deposit Pokemon");
    return ServiceStatus::StorageError;
  }
  return ServiceStatus::Ok;
}

ServiceStatus PokemonService::withdrawPokemon(const uint32_t recordId) {
  PokemonState state{};
  const ServiceStatus stateStatus = loadReadyState(state);
  if (stateStatus != ServiceStatus::Ok) return stateStatus;
  uint8_t partyCount = 0;
  while (partyCount < PARTY_SIZE && state.partyRecordIds[partyCount] != 0) {
    if (state.partyRecordIds[partyCount] == recordId) return ServiceStatus::Invalid;
    ++partyCount;
  }
  if (partyCount == PARTY_SIZE) return ServiceStatus::PartyFull;
  PokemonRecord record{};
  if (!store_.readRecord(recordId, record)) return ServiceStatus::NotFound;
  state.partyRecordIds[partyCount] = recordId;
  if (!store_.commit(state)) {
    LOG_ERR("PokemonService", "Failed to withdraw Pokemon");
    return ServiceStatus::StorageError;
  }
  return ServiceStatus::Ok;
}

ServiceStatus PokemonService::loadDashboardSnapshot(PokemonDashboardSnapshot& output) {
  output = {};
  PokemonState state{};
  const ServiceStatus stateStatus = loadReadyState(state);
  if (stateStatus != ServiceStatus::Ok) return stateStatus;
  if (state.partyRecordIds[0] == 0 || !store_.readRecord(state.partyRecordIds[0], output.leader)) {
    LOG_ERR("PokemonService", "Failed to load dashboard leader");
    output = {};
    return ServiceStatus::StorageError;
  }
  output.pending = state.pending;
  output.notice = state.dashboardNotice;
  return ServiceStatus::Ok;
}

ServiceStatus PokemonService::readPcPage(const PcOrder order, const size_t offset,
                                         const std::span<PokemonRecord> output, size_t& count) {
  count = 0;
  const ServiceStatus prepared = prepareStore();
  if (prepared != ServiceStatus::Ok) return prepared;
  count = store_.readPcPage(order, offset, output);
  return ServiceStatus::Ok;
}

ServiceStatus PokemonService::resolveEncounter(const EncounterChoice choice, uint32_t& caughtRecordId) {
  caughtRecordId = 0;
  PokemonState state{};
  const ServiceStatus stateStatus = loadReadyState(state);
  if (stateStatus != ServiceStatus::Ok) return stateStatus;
  PokemonRecord leader{};
  if (state.partyRecordIds[0] == 0 || !store_.readRecord(state.partyRecordIds[0], leader)) {
    return ServiceStatus::StorageError;
  }
  RecordMutation mutation{};
  if (choice == EncounterChoice::Catch) {
    if (store_.recordCount() >= 1024U) return ServiceStatus::StorageError;
    mutation.requestedRecordId = store_.recordCount() + 1U;
  }
  if (!pokemon::resolveEncounter(state, leader, choice, "", mutation)) return ServiceStatus::NotApplicable;
  if (!store_.commit(state, mutation)) {
    LOG_ERR("PokemonService", "Failed to resolve encounter");
    return ServiceStatus::StorageError;
  }
  if (mutation.kind == RecordMutationKind::Append) caughtRecordId = mutation.record.recordId;
  return ServiceStatus::Ok;
}

ServiceStatus PokemonService::acknowledgeItem() {
  PokemonState state{};
  const ServiceStatus stateStatus = loadReadyState(state);
  if (stateStatus != ServiceStatus::Ok) return stateStatus;
  PokemonRecord leader{};
  if (state.partyRecordIds[0] == 0 || !store_.readRecord(state.partyRecordIds[0], leader)) {
    return ServiceStatus::StorageError;
  }
  if (!pokemon::acknowledgeItem(state, leader)) return ServiceStatus::NotApplicable;
  if (!store_.commit(state)) {
    LOG_ERR("PokemonService", "Failed to acknowledge item");
    return ServiceStatus::StorageError;
  }
  return ServiceStatus::Ok;
}

ServiceStatus PokemonService::resolveEvolution(const EvolutionChoice choice) {
  PokemonState state{};
  const ServiceStatus stateStatus = loadReadyState(state);
  if (stateStatus != ServiceStatus::Ok) return stateStatus;
  if (state.pending.kind != PendingEventKind::Evolution || state.pending.recordId == 0) {
    return ServiceStatus::NotApplicable;
  }
  PokemonRecord record{};
  if (!store_.readRecord(state.pending.recordId, record)) return ServiceStatus::StorageError;
  RecordMutation mutation{};
  if (!pokemon::resolveEvolution(state, record, choice, mutation)) return ServiceStatus::NotApplicable;
  if (!store_.commit(state, mutation)) {
    LOG_ERR("PokemonService", "Failed to resolve evolution");
    return ServiceStatus::StorageError;
  }
  return ServiceStatus::Ok;
}

ServiceStatus PokemonService::setEvolutionPrompts(const uint32_t recordId, const bool enabled) {
  PokemonState state{};
  const ServiceStatus stateStatus = loadReadyState(state);
  if (stateStatus != ServiceStatus::Ok) return stateStatus;
  PokemonRecord record{};
  if (!store_.readRecord(recordId, record)) return ServiceStatus::NotFound;
  RecordMutation mutation{};
  if (!pokemon::setEvolutionPrompts(state, record, enabled, mutation)) return ServiceStatus::NotApplicable;
  if (!store_.commit(state, mutation)) {
    LOG_ERR("PokemonService", "Failed to set evolution prompts");
    return ServiceStatus::StorageError;
  }
  return ServiceStatus::Ok;
}

ServiceStatus PokemonService::useEvolutionItem(const uint32_t recordId, const EvolutionItem item) {
  PokemonState state{};
  const ServiceStatus stateStatus = loadReadyState(state);
  if (stateStatus != ServiceStatus::Ok) return stateStatus;
  PokemonRecord record{};
  if (!store_.readRecord(recordId, record)) return ServiceStatus::NotFound;
  RecordMutation mutation{};
  if (!pokemon::useEvolutionItem(state, record, item, mutation)) return ServiceStatus::NotApplicable;
  if (!store_.commit(state, mutation)) {
    LOG_ERR("PokemonService", "Failed to use evolution item");
    return ServiceStatus::StorageError;
  }
  return ServiceStatus::Ok;
}

ServiceStatus PokemonService::reset() {
  readingSessionActive_ = false;
  if (!store_.reset()) {
    LOG_ERR("PokemonService", "Failed to reset Pokemon save");
    return ServiceStatus::StorageError;
  }
  return ServiceStatus::Ok;
}

bool PokemonService::beginReadingSession() {
  readingSessionActive_ = false;
  if (!store_.isReady() && store_.begin() != StoreBeginResult::Ready) return false;

  PokemonState state{};
  PokemonRecord leader{};
  if (!store_.loadState(state) || state.partyRecordIds[0] == 0 ||
      !store_.readRecord(state.partyRecordIds[0], leader)) {
    return false;
  }

  if (!tracker_.beginSession()) {
    LOG_ERR("PokemonService", "Failed to retry unsaved reading credit");
    return false;
  }
  readingSessionActive_ = true;
  return true;
}

void PokemonService::setBookProgressPercent(const uint8_t percent) {
  if (readingSessionActive_) tracker_.setBookProgressPercent(percent);
}

void PokemonService::onSuccessfulPageTurn(const uint32_t nowMs) {
  if (readingSessionActive_) tracker_.onSuccessfulPageTurn(nowMs);
}

void PokemonService::checkpointIfDue(const uint32_t nowMs) {
  if (readingSessionActive_) tracker_.checkpointIfDue(nowMs);
}

void PokemonService::flushOnExit(const uint32_t nowMs) {
  if (!readingSessionActive_) return;
  if (!tracker_.flushOnExit(nowMs)) {
    LOG_ERR("PokemonService", "Retaining unsaved reading credit for the next session");
  }
  readingSessionActive_ = false;
}

bool PokemonService::creditFromTracker(void* context, const uint16_t minutes,
                                       const uint8_t bookProgressPercent) {
  return static_cast<PokemonService*>(context)->creditMinutes(minutes, bookProgressPercent);
}

bool PokemonService::creditMinutes(const uint16_t minutes, const uint8_t bookProgressPercent) {
  PokemonState state{};
  if (minutes == 0 || !store_.loadState(state) || state.partyRecordIds[0] == 0) return false;

  PokemonRecord leader{};
  if (!store_.readRecord(state.partyRecordIds[0], leader)) {
    LOG_ERR("PokemonService", "Failed to load party leader");
    return false;
  }

  OwnedEvolutionNeeds ownedEvolutionNeeds{};
  if (static_cast<uint32_t>(state.readingMinuteRemainder) + minutes >= 60U &&
      !store_.loadOwnedEvolutionNeeds(ownedEvolutionNeeds)) {
    LOG_ERR("PokemonService", "Failed to load owned evolution needs");
    return false;
  }

  const uint32_t originalLeaderXp = leader.totalXp;
  const CreditResult result =
      applyCreditedMinutes(state, leader, minutes, bookProgressPercent, ownedEvolutionNeeds, random_);
  if (result.status != CreditStatus::Applied) return false;

  RecordMutation mutation{};
  if (leader.totalXp != originalLeaderXp) {
    mutation.requestedRecordId = leader.recordId;
    mutation.record = leader;
    mutation.kind = RecordMutationKind::Replace;
  }
  if (!store_.commit(state, mutation)) {
    LOG_ERR("PokemonService", "Failed to commit credited reading");
    return false;
  }
  return true;
}

#if !defined(POKEMON_SERVICE_HOST_TEST)
PokemonService& devicePokemonService() {
  static PokemonStore store;
  static PokemonService service(store, {nullptr, deviceRandomBelow});
  return service;
}
#endif

}  // namespace pokemon

#endif
