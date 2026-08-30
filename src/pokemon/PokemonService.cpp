#if defined(CROSSINK_ENABLE_POKEMON)

#include "PokemonService.h"

#include <Logging.h>

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
