#if defined(CROSSINK_ENABLE_POKEMON)

#include "PokemonGame.h"

#include <array>
#include <cstdint>

#include "PokemonSpecies.h"

namespace pokemon {
namespace {

constexpr uint8_t OWNED_NEEDS_MASK = 0x3FU;
constexpr uint8_t ENCOUNTER_CHECK_MINUTES = 15;
constexpr uint8_t ENCOUNTER_CHANCE_DENOMINATOR = 5;
constexpr uint8_t ENCOUNTER_CHANCE_SUCCESSES = 2;
constexpr uint8_t ENCOUNTER_MISSES_BEFORE_GUARANTEE = 3;

bool randomBelow(const RandomSource& random, const uint32_t upperExclusive, uint32_t& output) {
  if (random.below == nullptr || upperExclusive == 0) return false;
  const uint32_t candidate = random.below(random.context, upperExclusive);
  if (candidate >= upperExclusive) return false;
  output = candidate;
  return true;
}

void incrementCapped(uint8_t& value, const uint8_t maximum) {
  if (value < maximum) ++value;
}

bool regularEncounterEligible(const SpeciesData& species, const uint8_t bookProgressPercent) {
  if (species.acquisition != Acquisition::Wild) return false;
  if (species.stage == EvolutionStage::Middle && bookProgressPercent < 50) return false;
  if (species.stage == EvolutionStage::Final && bookProgressPercent < 75) return false;
  return true;
}

uint8_t encounterWeight(const SpeciesData& species) {
  if (species.speciesId == 1 || species.speciesId == 4 || species.speciesId == 7 || species.speciesId == 25) {
    return 1;
  }
  if (species.captureRate >= 200) return 4;
  if (species.captureRate >= 120) return 3;
  if (species.captureRate >= 60) return 2;
  return 1;
}

bool chooseRegularSpecies(const uint8_t bookProgressPercent, const RandomSource& random, uint16_t& speciesId) {
  uint32_t totalWeight = 0;
  for (uint16_t candidateId = 1; candidateId <= KANTO_SPECIES_COUNT; ++candidateId) {
    const SpeciesData* candidate = speciesData(candidateId);
    if (candidate != nullptr && regularEncounterEligible(*candidate, bookProgressPercent)) {
      totalWeight += encounterWeight(*candidate);
    }
  }

  uint32_t selectedWeight = 0;
  if (!randomBelow(random, totalWeight, selectedWeight)) return false;
  for (uint16_t candidateId = 1; candidateId <= KANTO_SPECIES_COUNT; ++candidateId) {
    const SpeciesData* candidate = speciesData(candidateId);
    if (candidate == nullptr || !regularEncounterEligible(*candidate, bookProgressPercent)) continue;
    const uint8_t weight = encounterWeight(*candidate);
    if (selectedWeight < weight) {
      speciesId = candidateId;
      return true;
    }
    selectedWeight -= weight;
  }
  return false;
}

bool chooseEncounterLevel(const uint8_t bookProgressPercent, const RandomSource& random, uint8_t& level) {
  struct LevelBand {
    uint8_t progress;
    uint8_t minimum;
    uint8_t maximum;
  };
  static constexpr LevelBand BANDS[] = {{95, 18, 30}, {75, 14, 24}, {50, 9, 16}, {25, 5, 10}, {0, 2, 6}};
  const LevelBand* band = &BANDS[4];
  for (const LevelBand& candidate : BANDS) {
    if (bookProgressPercent >= candidate.progress) {
      band = &candidate;
      break;
    }
  }
  uint32_t offset = 0;
  if (!randomBelow(random, static_cast<uint32_t>(band->maximum - band->minimum + 1U), offset)) return false;
  level = static_cast<uint8_t>(band->minimum + offset);
  return true;
}

bool chooseGender(const SpeciesData& species, const RandomSource& random, Gender& gender) {
  if (species.genderRate == 255)
    gender = Gender::Genderless;
  else if (species.genderRate == 0)
    gender = Gender::Male;
  else if (species.genderRate == 8)
    gender = Gender::Female;
  else {
    uint32_t roll = 0;
    if (!randomBelow(random, 8, roll)) return false;
    gender = roll < species.genderRate ? Gender::Female : Gender::Male;
  }
  return true;
}

DashboardNotice noticeForEvent(const PendingEvent* event) {
  if (event == nullptr) return DashboardNotice::None;
  switch (event->kind) {
    case PendingEventKind::Encounter:
      return DashboardNotice::NewPokemon;
    case PendingEventKind::Item:
      return DashboardNotice::ItemFound;
    case PendingEventKind::Evolution:
      return DashboardNotice::WhatsThis;
    case PendingEventKind::None:
      return DashboardNotice::None;
  }
  return DashboardNotice::None;
}

void refreshDashboardNotice(PokemonState& state) { state.dashboardNotice = noticeForEvent(pendingEventFront(state)); }

bool popPendingEvent(PokemonState& state) {
  if (!dequeuePendingEvent(state)) return false;
  refreshDashboardNotice(state);
  return true;
}

const EvolutionRule* findEvolution(const PokemonRecord& record, const EvolutionTrigger trigger,
                                   const EvolutionItem item, const uint16_t targetSpeciesId = 0) {
  for (const EvolutionRule& rule : evolutionsFor(record.speciesId)) {
    if (rule.trigger == trigger && (item == EvolutionItem::None || rule.item == item) &&
        (targetSpeciesId == 0 || rule.targetSpeciesId == targetSpeciesId)) {
      return &rule;
    }
  }
  return nullptr;
}

bool evolveCandidate(PokemonState& state, PokemonRecord& record, const uint16_t targetSpeciesId,
                     RecordMutation& mutation) {
  record.speciesId = targetSpeciesId;
  if (!validateRecord(record) || !markSpecies(state.seenSpecies, targetSpeciesId) ||
      !markSpecies(state.caughtSpecies, targetSpeciesId)) {
    return false;
  }
  mutation.requestedRecordId = record.recordId;
  mutation.record = record;
  mutation.kind = RecordMutationKind::Replace;
  return true;
}

bool queueEvolutionAfterLevelGain(PokemonState& state, const PokemonRecord& record, bool& queued) {
  queued = false;
  if ((record.flags & recordFlag(RecordFlag::EvolutionPromptsDisabled)) != 0) {
    return true;
  }
  for (const PendingEvent& event : state.pendingEvents) {
    if (event.kind == PendingEventKind::Evolution && event.recordId == record.recordId) return true;
  }
  if (pendingEventCount(state) == PENDING_EVENT_CAPACITY) return true;
  const uint8_t level = levelForXp(record.totalXp);
  for (const EvolutionRule& rule : evolutionsFor(record.speciesId)) {
    if (rule.trigger != EvolutionTrigger::Level || level < rule.minimumLevel) continue;
    const PendingEvent event{record.recordId, rule.targetSpeciesId, 0,
                             Gender::Unknown, EvolutionItem::None,  PendingEventKind::Evolution};
    if (!enqueuePendingEvent(state, event)) return false;
    if (!markSpecies(state.seenSpecies, rule.targetSpeciesId)) return false;
    refreshDashboardNotice(state);
    queued = true;
    return true;
  }
  return true;
}

bool finalizeEncounter(PokemonState& state, const uint16_t speciesId, const uint8_t bookProgressPercent,
                       const RandomSource& random) {
  uint8_t level = 0;
  if (!chooseEncounterLevel(bookProgressPercent, random, level)) return false;
  const SpeciesData* species = speciesData(speciesId);
  Gender gender = Gender::Unknown;
  if (species == nullptr || !chooseGender(*species, random, gender)) return false;

  const PendingEvent event{0, speciesId, level, gender, EvolutionItem::None, PendingEventKind::Encounter};
  if (!enqueuePendingEvent(state, event) || !markSpecies(state.seenSpecies, speciesId)) return false;
  refreshDashboardNotice(state);
  return true;
}

bool mewIsReady(const PokemonState& state) {
  if (isSpeciesMarked(state.caughtSpecies, 151)) return false;
  for (uint16_t speciesId = 1; speciesId <= 150; ++speciesId) {
    if (!isSpeciesMarked(state.caughtSpecies, speciesId)) return false;
  }
  return true;
}

bool chooseLegendary(const PokemonState& state, const uint8_t bookProgressPercent, const RandomSource& random,
                     uint16_t& selectedSpeciesId) {
  std::array<uint16_t, 4> eligible{};
  size_t eligibleCount = 0;
  if (state.lifetimeMinutes >= 1200U && bookProgressPercent >= 75U) {
    eligible[eligibleCount++] = 144;
    eligible[eligibleCount++] = 145;
    eligible[eligibleCount++] = 146;
  }
  if (state.lifetimeMinutes >= 3000U && bookProgressPercent >= 95U) eligible[eligibleCount++] = 150;
  if (eligibleCount == 0) return false;

  std::array<uint16_t, 4> uncaught{};
  size_t uncaughtCount = 0;
  for (size_t index = 0; index < eligibleCount; ++index) {
    if (!isSpeciesMarked(state.caughtSpecies, eligible[index])) uncaught[uncaughtCount++] = eligible[index];
  }
  const uint16_t* candidates = uncaughtCount == 0 ? eligible.data() : uncaught.data();
  const size_t candidateCount = uncaughtCount == 0 ? eligibleCount : uncaughtCount;
  uint32_t selectedIndex = 0;
  if (candidateCount > 1 && !randomBelow(random, static_cast<uint32_t>(candidateCount), selectedIndex)) return false;
  selectedSpeciesId = candidates[selectedIndex];
  return true;
}

bool createEncounter(PokemonState& state, const uint8_t bookProgressPercent, const RandomSource& random) {
  if (mewIsReady(state)) return finalizeEncounter(state, 151, bookProgressPercent, random);

  uint32_t legendaryRoll = 0;
  if (!randomBelow(random, 20, legendaryRoll)) return false;
  uint16_t speciesId = 0;
  if (legendaryRoll == 0 && chooseLegendary(state, bookProgressPercent, random, speciesId)) {
    return finalizeEncounter(state, speciesId, bookProgressPercent, random);
  }
  if (!chooseRegularSpecies(bookProgressPercent, random, speciesId)) return false;
  return finalizeEncounter(state, speciesId, bookProgressPercent, random);
}

bool createItem(PokemonState& state, const OwnedEvolutionNeeds ownedEvolutionNeeds, const RandomSource& random,
                bool& created) {
  created = false;
  std::array<uint8_t, EVOLUTION_ITEM_COUNT> candidates{};
  size_t candidateCount = 0;
  for (uint8_t index = 0; index < EVOLUTION_ITEM_COUNT; ++index) {
    if (state.itemCounts[index] != UINT16_MAX && (ownedEvolutionNeeds.mask & static_cast<uint8_t>(1U << index)) != 0) {
      candidates[candidateCount++] = index;
    }
  }
  if (candidateCount == 0) {
    for (uint8_t index = 0; index < EVOLUTION_ITEM_COUNT; ++index) {
      if (state.itemCounts[index] != UINT16_MAX) candidates[candidateCount++] = index;
    }
  }
  if (candidateCount == 0) return true;

  uint32_t selectedIndex = 0;
  if (candidateCount > 1 && !randomBelow(random, static_cast<uint32_t>(candidateCount), selectedIndex)) return false;
  const uint8_t itemIndex = candidates[selectedIndex];
  ++state.itemCounts[itemIndex];
  const PendingEvent event{
      0, 0, 0, Gender::Unknown, static_cast<EvolutionItem>(itemIndex + 1U), PendingEventKind::Item};
  if (!enqueuePendingEvent(state, event)) return false;
  refreshDashboardNotice(state);
  created = true;
  return true;
}

bool processEncounterCheck(PokemonState& state, const uint8_t bookProgressPercent, const RandomSource& random,
                           PendingEventKind& generatedEvent) {
  if (pendingEventCount(state) == PENDING_EVENT_CAPACITY) {
    state.encounterMisses = ENCOUNTER_MISSES_BEFORE_GUARANTEE;
    return true;
  }

  bool encounterTriggered = state.encounterMisses >= ENCOUNTER_MISSES_BEFORE_GUARANTEE;
  if (!encounterTriggered) {
    uint32_t encounterRoll = 0;
    if (!randomBelow(random, ENCOUNTER_CHANCE_DENOMINATOR, encounterRoll)) return false;
    encounterTriggered = encounterRoll < ENCOUNTER_CHANCE_SUCCESSES;
  }
  if (encounterTriggered) {
    if (!createEncounter(state, bookProgressPercent, random)) return false;
    state.encounterMisses = 0;
    generatedEvent = PendingEventKind::Encounter;
    return true;
  }
  incrementCapped(state.encounterMisses, ENCOUNTER_MISSES_BEFORE_GUARANTEE);

  return true;
}

bool processHourlyItem(PokemonState& state, const RandomSource& random, const OwnedEvolutionNeeds ownedEvolutionNeeds,
                       PendingEventKind& generatedEvent) {
  if (pendingEventCount(state) == PENDING_EVENT_CAPACITY) {
    state.itemMisses = 19;
    return true;
  }
  bool itemTriggered = state.itemMisses == 19;
  if (!itemTriggered) {
    uint32_t itemRoll = 0;
    if (!randomBelow(random, 20, itemRoll)) return false;
    itemTriggered = itemRoll == 0;
  }
  if (itemTriggered) {
    bool itemCreated = false;
    if (!createItem(state, ownedEvolutionNeeds, random, itemCreated)) return false;
    state.itemMisses = 0;
    if (itemCreated) generatedEvent = PendingEventKind::Item;
    return true;
  }
  incrementCapped(state.itemMisses, 19);
  return true;
}

}  // namespace

CreditResult applyCreditedMinutes(PokemonState& state, PokemonRecord& leader, const uint16_t minutes,
                                  const uint8_t bookProgressPercent, const OwnedEvolutionNeeds ownedEvolutionNeeds,
                                  const RandomSource& random) {
  CreditResult result{};
  if (!validateState(state) || !validateRecord(leader) || state.partyRecordIds[0] != leader.recordId ||
      bookProgressPercent > 100 || (ownedEvolutionNeeds.mask & static_cast<uint8_t>(~OWNED_NEEDS_MASK)) != 0) {
    return result;
  }

  result.previousLevel = levelForXp(leader.totalXp);
  result.currentLevel = result.previousLevel;
  if (minutes == 0) {
    result.status = CreditStatus::NoChange;
    return result;
  }

  PokemonState stateCandidate = state;
  PokemonRecord leaderCandidate = leader;
  PendingEventKind generatedEvent = PendingEventKind::None;
  for (uint32_t minute = 0; minute < minutes; ++minute) {
    if (leaderCandidate.totalXp < MAXIMUM_TOTAL_XP) ++leaderCandidate.totalXp;
    if (stateCandidate.lifetimeMinutes < UINT32_MAX) ++stateCandidate.lifetimeMinutes;
    ++stateCandidate.readingMinuteRemainder;

    const uint8_t minuteLevel = levelForXp(leaderCandidate.totalXp);
    const bool gainedLevel = minuteLevel > result.currentLevel;
    result.currentLevel = minuteLevel;

    const bool hourlyBoundary = stateCandidate.readingMinuteRemainder == 60;
    if (hourlyBoundary) {
      stateCandidate.readingMinuteRemainder = 0;
      if (!processHourlyItem(stateCandidate, random, ownedEvolutionNeeds, generatedEvent)) return result;
    }

    if (hourlyBoundary || stateCandidate.readingMinuteRemainder % ENCOUNTER_CHECK_MINUTES == 0) {
      if (!processEncounterCheck(stateCandidate, bookProgressPercent, random, generatedEvent)) {
        return result;
      }
    }

    if (gainedLevel) {
      bool queued = false;
      if (!queueEvolutionAfterLevelGain(stateCandidate, leaderCandidate, queued)) return result;
      if (queued) generatedEvent = PendingEventKind::Evolution;
    }
  }

  result.creditedMinutes = minutes;
  result.generatedEvent = generatedEvent;
  result.status = CreditStatus::Applied;
  state = stateCandidate;
  leader = leaderCandidate;
  return result;
}

bool acknowledgeItem(PokemonState& state, const PokemonRecord& leader) {
  const PendingEvent* pending = pendingEventFront(state);
  if (!validateState(state) || !validateRecord(leader) || state.partyRecordIds[0] != leader.recordId ||
      pending == nullptr || pending->kind != PendingEventKind::Item) {
    return false;
  }
  PokemonState candidate = state;
  if (!popPendingEvent(candidate)) return false;
  state = candidate;
  return true;
}

bool setEvolutionPrompts(PokemonState& state, PokemonRecord& record, const bool enabled, RecordMutation& mutation) {
  if (!validateState(state) || !validateRecord(record) || mutation.kind != RecordMutationKind::None) {
    return false;
  }

  PokemonState stateCandidate = state;
  PokemonRecord recordCandidate = record;
  if (enabled) {
    recordCandidate.flags &= static_cast<uint8_t>(~recordFlag(RecordFlag::EvolutionPromptsDisabled));
  } else {
    recordCandidate.flags |= recordFlag(RecordFlag::EvolutionPromptsDisabled);
    removePendingEvolutionsForRecord(stateCandidate, record.recordId);
    refreshDashboardNotice(stateCandidate);
  }
  if (!validateRecord(recordCandidate)) return false;

  RecordMutation mutationCandidate = mutation;
  mutationCandidate.requestedRecordId = recordCandidate.recordId;
  mutationCandidate.record = recordCandidate;
  mutationCandidate.kind = RecordMutationKind::Replace;
  state = stateCandidate;
  record = recordCandidate;
  mutation = mutationCandidate;
  return true;
}

bool resolveEncounter(PokemonState& state, const PokemonRecord& leader, const EncounterChoice choice,
                      const char* nickname, RecordMutation& mutation) {
  const PendingEvent* front = pendingEventFront(state);
  if (!validateState(state) || !validateRecord(leader) || state.partyRecordIds[0] != leader.recordId ||
      front == nullptr || front->kind != PendingEventKind::Encounter || mutation.kind != RecordMutationKind::None) {
    return false;
  }
  const PendingEvent pending = *front;

  PokemonState stateCandidate = state;
  RecordMutation mutationCandidate = mutation;
  if (choice == EncounterChoice::Catch) {
    if (mutation.requestedRecordId == 0) return false;
    PokemonRecord caught{};
    caught.recordId = mutation.requestedRecordId;
    caught.totalXp = xpRequired(pending.level);
    caught.speciesId = pending.speciesId;
    caught.caughtLevel = pending.level;
    caught.gender = pending.gender;
    caught.origin = Origin::Caught;
    if (!setNickname(caught, nickname == nullptr ? "" : nickname) || !validateRecord(caught) ||
        !markSpecies(stateCandidate.seenSpecies, caught.speciesId) ||
        !markSpecies(stateCandidate.caughtSpecies, caught.speciesId)) {
      return false;
    }
    for (uint32_t& partyRecordId : stateCandidate.partyRecordIds) {
      if (partyRecordId == 0) {
        partyRecordId = caught.recordId;
        break;
      }
    }
    mutationCandidate.record = caught;
    mutationCandidate.kind = RecordMutationKind::Append;
  } else if (choice != EncounterChoice::Pass) {
    return false;
  }

  if (!popPendingEvent(stateCandidate)) return false;
  if (!validateState(stateCandidate)) return false;
  state = stateCandidate;
  mutation = mutationCandidate;
  return true;
}

bool resolveEvolution(PokemonState& state, PokemonRecord& record, const EvolutionChoice choice,
                      RecordMutation& mutation) {
  const PendingEvent* front = pendingEventFront(state);
  if (!validateState(state) || !validateRecord(record) || front == nullptr ||
      front->kind != PendingEventKind::Evolution || front->recordId != record.recordId ||
      mutation.kind != RecordMutationKind::None) {
    return false;
  }
  const PendingEvent pending = *front;

  const EvolutionRule* rule = findEvolution(record, EvolutionTrigger::Level, EvolutionItem::None, pending.speciesId);
  if (rule == nullptr || levelForXp(record.totalXp) < rule->minimumLevel) return false;

  PokemonState stateCandidate = state;
  PokemonRecord recordCandidate = record;
  RecordMutation mutationCandidate = mutation;
  if (choice == EvolutionChoice::Evolve) {
    if (!evolveCandidate(stateCandidate, recordCandidate, rule->targetSpeciesId, mutationCandidate)) return false;
  } else if (choice != EvolutionChoice::Cancel) {
    return false;
  }

  if (!popPendingEvent(stateCandidate)) return false;
  state = stateCandidate;
  record = recordCandidate;
  mutation = mutationCandidate;
  return true;
}

bool useEvolutionItem(PokemonState& state, PokemonRecord& record, const EvolutionItem item, RecordMutation& mutation) {
  if (!validateState(state) || !validateRecord(record) || pendingEventFront(state) != nullptr ||
      item < EvolutionItem::MoonStone || item > EvolutionItem::LinkCable || mutation.kind != RecordMutationKind::None) {
    return false;
  }

  const EvolutionRule* rule = findEvolution(record, EvolutionTrigger::Item, item);
  if (rule == nullptr) return false;
  const size_t itemIndex = static_cast<size_t>(item) - 1U;
  if (state.itemCounts[itemIndex] == 0) return false;

  PokemonState stateCandidate = state;
  PokemonRecord recordCandidate = record;
  RecordMutation mutationCandidate = mutation;
  if (!evolveCandidate(stateCandidate, recordCandidate, rule->targetSpeciesId, mutationCandidate)) return false;
  --stateCandidate.itemCounts[itemIndex];
  state = stateCandidate;
  record = recordCandidate;
  mutation = mutationCandidate;
  return true;
}

CollectionActionSet collectionActions(const bool party, const uint8_t partyCount) {
  CollectionActionSet actions{};
  const auto append = [&actions](const CollectionAction action) { actions.items[actions.count++] = action; };

  append(CollectionAction::Summary);
  if (party) {
    if (partyCount > 1) {
      append(CollectionAction::Move);
      append(CollectionAction::Deposit);
    }
  } else if (partyCount < PARTY_SIZE) {
    append(CollectionAction::Withdraw);
  }
  append(CollectionAction::Rename);
  append(CollectionAction::EvolutionPrompts);
  return actions;
}

}  // namespace pokemon

#endif
