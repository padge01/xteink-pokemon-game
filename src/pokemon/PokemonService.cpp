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
  if (store_.begin() != StoreBeginResult::Ready) return false;

  PokemonState state{};
  PokemonRecord leader{};
  if (!store_.loadState(state) || state.partyRecordIds[0] == 0 ||
      !store_.readRecord(state.partyRecordIds[0], leader)) {
    return false;
  }

  tracker_.beginSession();
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
  tracker_.flushOnExit(nowMs);
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
    LOG_ERR("Pokemon service: failed to load party leader");
    return false;
  }

  OwnedEvolutionNeeds ownedEvolutionNeeds{};
  if (static_cast<uint32_t>(state.readingMinuteRemainder) + minutes >= 60U &&
      !store_.loadOwnedEvolutionNeeds(ownedEvolutionNeeds)) {
    LOG_ERR("Pokemon service: failed to load owned evolution needs");
    return false;
  }

  PokemonState stateCandidate = state;
  PokemonRecord leaderCandidate = leader;
  const CreditResult result = applyCreditedMinutes(stateCandidate, leaderCandidate, minutes,
                                                    bookProgressPercent, ownedEvolutionNeeds, random_);
  if (result.status != CreditStatus::Applied) return false;

  RecordMutation mutation{};
  if (leaderCandidate != leader) {
    mutation.requestedRecordId = leaderCandidate.recordId;
    mutation.record = leaderCandidate;
    mutation.kind = RecordMutationKind::Replace;
  }
  if (!store_.commit(stateCandidate, mutation)) {
    LOG_ERR("Pokemon service: failed to commit credited reading");
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
