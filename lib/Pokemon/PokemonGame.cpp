#include "PokemonGame.h"

#include <algorithm>
#include <array>
#include <cstdint>

#include "PokemonSpecies.h"

namespace pokemon {
namespace {

constexpr uint8_t OWNED_NEEDS_MASK = 0x3FU;

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
    return 8;
  }
  return species.captureRate;
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
  if (species.genderRate == 255) gender = Gender::Genderless;
  else if (species.genderRate == 0) gender = Gender::Male;
  else if (species.genderRate == 8) gender = Gender::Female;
  else {
    uint32_t roll = 0;
    if (!randomBelow(random, 8, roll)) return false;
    gender = roll < species.genderRate ? Gender::Female : Gender::Male;
  }
  return true;
}

void clearPending(PokemonState& state) {
  state.pending = {};
  state.dashboardNotice = DashboardNotice::None;
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
  mutation.record = record;
  mutation.kind = RecordMutationKind::Replace;
  return true;
}

bool finalizeEncounter(PokemonState& state, const uint16_t speciesId, const uint8_t bookProgressPercent,
                       const RandomSource& random) {
  uint8_t level = 0;
  if (!chooseEncounterLevel(bookProgressPercent, random, level)) return false;
  const SpeciesData* species = speciesData(speciesId);
  Gender gender = Gender::Unknown;
  if (species == nullptr || !chooseGender(*species, random, gender)) return false;

  state.pending.kind = PendingEventKind::Encounter;
  state.pending.speciesId = speciesId;
  state.pending.level = level;
  state.pending.gender = gender;
  state.dashboardNotice = DashboardNotice::NewPokemon;
  return markSpecies(state.seenSpecies, speciesId);
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

bool createItem(PokemonState& state, const OwnedEvolutionNeeds ownedEvolutionNeeds, const RandomSource& random) {
  std::array<uint8_t, EVOLUTION_ITEM_COUNT> candidates{};
  size_t candidateCount = 0;
  for (uint8_t index = 0; index < EVOLUTION_ITEM_COUNT; ++index) {
    if ((ownedEvolutionNeeds.mask & static_cast<uint8_t>(1U << index)) != 0) candidates[candidateCount++] = index;
  }
  if (candidateCount == 0) {
    for (uint8_t index = 0; index < EVOLUTION_ITEM_COUNT; ++index) candidates[candidateCount++] = index;
  }

  uint32_t selectedIndex = 0;
  if (candidateCount > 1 && !randomBelow(random, static_cast<uint32_t>(candidateCount), selectedIndex)) return false;
  const uint8_t itemIndex = candidates[selectedIndex];
  if (state.itemCounts[itemIndex] == UINT16_MAX) return false;
  ++state.itemCounts[itemIndex];
  state.pending.kind = PendingEventKind::Item;
  state.pending.item = static_cast<EvolutionItem>(itemIndex + 1U);
  state.dashboardNotice = DashboardNotice::ItemFound;
  return true;
}

bool processHourlyCheck(PokemonState& state, const uint8_t bookProgressPercent, const RandomSource& random,
                        const OwnedEvolutionNeeds ownedEvolutionNeeds, PendingEventKind& generatedEvent) {
  if (state.pending.kind != PendingEventKind::None) {
    incrementCapped(state.encounterMisses, 5);
    incrementCapped(state.itemMisses, 19);
    return true;
  }

  bool encounterTriggered = state.encounterMisses == 5;
  if (!encounterTriggered) {
    uint32_t encounterRoll = 0;
    if (!randomBelow(random, 4, encounterRoll)) return false;
    encounterTriggered = encounterRoll == 0;
  }
  if (encounterTriggered) {
    if (!createEncounter(state, bookProgressPercent, random)) return false;
    state.encounterMisses = 0;
    incrementCapped(state.itemMisses, 19);
    generatedEvent = PendingEventKind::Encounter;
    return true;
  }
  incrementCapped(state.encounterMisses, 5);

  bool itemTriggered = state.itemMisses == 19;
  if (!itemTriggered) {
    uint32_t itemRoll = 0;
    if (!randomBelow(random, 20, itemRoll)) return false;
    itemTriggered = itemRoll == 0;
  }
  if (itemTriggered) {
    if (!createItem(state, ownedEvolutionNeeds, random)) return false;
    state.itemMisses = 0;
    generatedEvent = PendingEventKind::Item;
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
  leaderCandidate.totalXp = std::min<uint32_t>(MAXIMUM_TOTAL_XP, leaderCandidate.totalXp + minutes);
  const uint32_t lifetimeHeadroom = UINT32_MAX - stateCandidate.lifetimeMinutes;
  stateCandidate.lifetimeMinutes += std::min<uint32_t>(minutes, lifetimeHeadroom);
  const uint32_t accumulatedMinutes = static_cast<uint32_t>(stateCandidate.readingMinuteRemainder) + minutes;
  const uint32_t hourlyChecks = accumulatedMinutes / 60U;
  stateCandidate.readingMinuteRemainder = static_cast<uint8_t>(accumulatedMinutes % 60U);

  result.currentLevel = levelForXp(leaderCandidate.totalXp);
  PendingEventKind generatedEvent = PendingEventKind::None;
  if (stateCandidate.pending.kind == PendingEventKind::None && result.currentLevel > result.previousLevel &&
      (leaderCandidate.flags & recordFlag(RecordFlag::EvolutionPromptsDisabled)) == 0) {
    for (const EvolutionRule& rule : evolutionsFor(leaderCandidate.speciesId)) {
      if (rule.trigger != EvolutionTrigger::Level || result.currentLevel < rule.minimumLevel) continue;
      stateCandidate.pending.kind = PendingEventKind::Evolution;
      stateCandidate.pending.recordId = leaderCandidate.recordId;
      stateCandidate.pending.speciesId = rule.targetSpeciesId;
      stateCandidate.dashboardNotice = DashboardNotice::WhatsThis;
      if (!markSpecies(stateCandidate.seenSpecies, rule.targetSpeciesId)) return result;
      generatedEvent = PendingEventKind::Evolution;
      break;
    }
  }
  for (uint32_t check = 0; check < hourlyChecks; ++check) {
    if (!processHourlyCheck(stateCandidate, bookProgressPercent, random, ownedEvolutionNeeds, generatedEvent)) {
      return result;
    }
  }

  result.creditedMinutes = minutes;
  result.generatedEvent = generatedEvent;
  result.status = CreditStatus::Applied;
  state = stateCandidate;
  leader = leaderCandidate;
  return result;
}

bool acknowledgeItem(PokemonState& state) {
  if (!validateState(state) || state.pending.kind != PendingEventKind::Item) return false;
  PokemonState candidate = state;
  clearPending(candidate);
  state = candidate;
  return true;
}

bool resolveEncounter(PokemonState& state, const EncounterChoice choice, const char* nickname,
                      RecordMutation& mutation) {
  if (!validateState(state) || state.pending.kind != PendingEventKind::Encounter ||
      mutation.kind != RecordMutationKind::None) {
    return false;
  }

  PokemonState stateCandidate = state;
  RecordMutation mutationCandidate = mutation;
  if (choice == EncounterChoice::Catch) {
    if (mutation.requestedRecordId == 0) return false;
    PokemonRecord caught{};
    caught.recordId = mutation.requestedRecordId;
    caught.totalXp = xpRequired(state.pending.level);
    caught.speciesId = state.pending.speciesId;
    caught.caughtLevel = state.pending.level;
    caught.gender = state.pending.gender;
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

  clearPending(stateCandidate);
  if (!validateState(stateCandidate)) return false;
  state = stateCandidate;
  mutation = mutationCandidate;
  return true;
}

bool resolveEvolution(PokemonState& state, PokemonRecord& record, const EvolutionChoice choice,
                      RecordMutation& mutation) {
  if (!validateState(state) || !validateRecord(record) || state.pending.kind != PendingEventKind::Evolution ||
      state.pending.recordId != record.recordId || mutation.kind != RecordMutationKind::None) {
    return false;
  }

  const EvolutionRule* rule = findEvolution(record, EvolutionTrigger::Level, EvolutionItem::None,
                                            state.pending.speciesId);
  if (rule == nullptr || levelForXp(record.totalXp) < rule->minimumLevel) return false;

  PokemonState stateCandidate = state;
  PokemonRecord recordCandidate = record;
  RecordMutation mutationCandidate = mutation;
  if (choice == EvolutionChoice::Evolve) {
    if (!evolveCandidate(stateCandidate, recordCandidate, rule->targetSpeciesId, mutationCandidate)) return false;
  } else if (choice != EvolutionChoice::Cancel) {
    return false;
  }

  clearPending(stateCandidate);
  state = stateCandidate;
  record = recordCandidate;
  mutation = mutationCandidate;
  return true;
}

bool useEvolutionItem(PokemonState& state, PokemonRecord& record, const EvolutionItem item,
                      RecordMutation& mutation) {
  if (!validateState(state) || !validateRecord(record) || state.pending.kind != PendingEventKind::None ||
      item < EvolutionItem::MoonStone || item > EvolutionItem::LinkCable ||
      mutation.kind != RecordMutationKind::None) {
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

}  // namespace pokemon
