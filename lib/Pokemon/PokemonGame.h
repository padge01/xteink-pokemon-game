#pragma once

#include <array>
#include <cstdint>

#include "PokemonTypes.h"

namespace pokemon {

enum class CreditStatus : uint8_t {
  Rejected = 0,
  NoChange = 1,
  Applied = 2,
};

struct CreditResult {
  uint16_t creditedMinutes = 0;
  uint8_t previousLevel = 0;
  uint8_t currentLevel = 0;
  PendingEventKind generatedEvent = PendingEventKind::None;
  CreditStatus status = CreditStatus::Rejected;
};

struct OwnedEvolutionNeeds {
  uint8_t mask = 0;
};

using RandomBelowFn = uint32_t (*)(void* context, uint32_t upperExclusive);

struct RandomSource {
  void* context = nullptr;
  RandomBelowFn below = nullptr;
};

enum class EncounterChoice : uint8_t {
  Catch = 0,
  Pass = 1,
};

enum class EvolutionChoice : uint8_t {
  Evolve = 0,
  Cancel = 1,
};

enum class CollectionAction : uint8_t {
  Summary = 0,
  Move = 1,
  Deposit = 2,
  Withdraw = 3,
  Rename = 4,
  EvolutionPrompts = 5,
};

struct CollectionActionSet {
  std::array<CollectionAction, 5> items{};
  uint8_t count = 0;
};

enum class RecordMutationKind : uint8_t {
  None = 0,
  Append = 1,
  Replace = 2,
};

struct RecordMutation {
  uint32_t requestedRecordId = 0;
  PokemonRecord record{};
  RecordMutationKind kind = RecordMutationKind::None;
};

CreditResult applyCreditedMinutes(PokemonState& state, PokemonRecord& leader, uint16_t minutes,
                                  uint8_t bookProgressPercent, OwnedEvolutionNeeds ownedEvolutionNeeds,
                                  const RandomSource& random);
bool acknowledgeItem(PokemonState& state, const PokemonRecord& leader);
bool setEvolutionPrompts(PokemonState& state, PokemonRecord& record, bool enabled, RecordMutation& mutation);
bool resolveEncounter(PokemonState& state, const PokemonRecord& leader, EncounterChoice choice, const char* nickname,
                      RecordMutation& mutation);
bool resolveEvolution(PokemonState& state, PokemonRecord& record, EvolutionChoice choice, RecordMutation& mutation);
bool useEvolutionItem(PokemonState& state, PokemonRecord& record, EvolutionItem item, RecordMutation& mutation);
CollectionActionSet collectionActions(bool party, uint8_t partyCount);

}  // namespace pokemon
