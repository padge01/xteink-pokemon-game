#if defined(CROSSINK_ENABLE_POKEMON)

#include "PokemonTypes.h"

#include <algorithm>
#include <cstring>

#include "PokemonSpecies.h"

namespace pokemon {
namespace {

constexpr uint8_t ALLOWED_RECORD_FLAGS = recordFlag(RecordFlag::EvolutionPromptsDisabled);

bool isContinuation(const uint8_t byte) { return (byte & 0xC0U) == 0x80U; }

bool isValidUtf8(const std::string_view value) {
  size_t index = 0;
  while (index < value.size()) {
    const uint8_t first = static_cast<uint8_t>(value[index]);
    if (first <= 0x7FU) {
      ++index;
      continue;
    }
    if (first >= 0xC2U && first <= 0xDFU) {
      if (index + 1 >= value.size() || !isContinuation(static_cast<uint8_t>(value[index + 1]))) return false;
      index += 2;
      continue;
    }
    if (first >= 0xE0U && first <= 0xEFU) {
      if (index + 2 >= value.size()) return false;
      const uint8_t second = static_cast<uint8_t>(value[index + 1]);
      const uint8_t third = static_cast<uint8_t>(value[index + 2]);
      if (!isContinuation(third)) return false;
      if ((first == 0xE0U && (second < 0xA0U || second > 0xBFU)) ||
          (first == 0xEDU && (second < 0x80U || second > 0x9FU)) ||
          (first != 0xE0U && first != 0xEDU && !isContinuation(second))) {
        return false;
      }
      index += 3;
      continue;
    }
    if (first >= 0xF0U && first <= 0xF4U) {
      if (index + 3 >= value.size()) return false;
      const uint8_t second = static_cast<uint8_t>(value[index + 1]);
      if ((first == 0xF0U && (second < 0x90U || second > 0xBFU)) ||
          (first == 0xF4U && (second < 0x80U || second > 0x8FU)) ||
          (first != 0xF0U && first != 0xF4U && !isContinuation(second)) ||
          !isContinuation(static_cast<uint8_t>(value[index + 2])) ||
          !isContinuation(static_cast<uint8_t>(value[index + 3]))) {
        return false;
      }
      index += 4;
      continue;
    }
    return false;
  }
  return true;
}

void write16(uint8_t* bytes, const size_t offset, const uint16_t value) {
  bytes[offset] = static_cast<uint8_t>(value);
  bytes[offset + 1] = static_cast<uint8_t>(value >> 8U);
}

void write32(uint8_t* bytes, const size_t offset, const uint32_t value) {
  bytes[offset] = static_cast<uint8_t>(value);
  bytes[offset + 1] = static_cast<uint8_t>(value >> 8U);
  bytes[offset + 2] = static_cast<uint8_t>(value >> 16U);
  bytes[offset + 3] = static_cast<uint8_t>(value >> 24U);
}

uint16_t read16(const uint8_t* bytes, const size_t offset) {
  return static_cast<uint16_t>(bytes[offset]) | static_cast<uint16_t>(static_cast<uint16_t>(bytes[offset + 1]) << 8U);
}

uint32_t read32(const uint8_t* bytes, const size_t offset) {
  return static_cast<uint32_t>(bytes[offset]) | (static_cast<uint32_t>(bytes[offset + 1]) << 8U) |
         (static_cast<uint32_t>(bytes[offset + 2]) << 16U) | (static_cast<uint32_t>(bytes[offset + 3]) << 24U);
}

// Record layout. Bytes 0-46 are the same in every snapshot version; a legacy record ends with one
// zero byte at 47, where the current record carries the Pokemon's status.
constexpr size_t RECORD_NICKNAME_OFFSET = 14;
constexpr size_t RECORD_STATUS_OFFSET = 47;
constexpr size_t RECORD_MOVES_OFFSET = 48;
constexpr size_t RECORD_PP_USED_OFFSET = 52;
constexpr size_t RECORD_HP_LOST_OFFSET = 56;
constexpr size_t RECORD_PP_UPS_OFFSET = 58;
constexpr size_t RECORD_STATUS_TURNS_OFFSET = 59;
constexpr size_t RECORD_RESERVED_OFFSET = 60;
static_assert(RECORD_NICKNAME_OFFSET + POKEMON_NICKNAME_BYTES == RECORD_STATUS_OFFSET);
static_assert(RECORD_STATUS_OFFSET + 1U == POKEMON_LEGACY_RECORD_BYTES);

void writeSharedRecordFields(const PokemonRecord& record, uint8_t* bytes) {
  write32(bytes, 0, record.recordId);
  write32(bytes, 4, record.totalXp);
  write16(bytes, 8, record.speciesId);
  bytes[10] = record.caughtLevel;
  bytes[11] = static_cast<uint8_t>(record.gender);
  bytes[12] = static_cast<uint8_t>(record.origin);
  bytes[13] = record.flags;
  std::memcpy(bytes + RECORD_NICKNAME_OFFSET, record.nickname.data(), record.nickname.size());
}

void readSharedRecordFields(const uint8_t* bytes, PokemonRecord& record) {
  record.recordId = read32(bytes, 0);
  record.totalXp = read32(bytes, 4);
  record.speciesId = read16(bytes, 8);
  record.caughtLevel = bytes[10];
  record.gender = static_cast<Gender>(bytes[11]);
  record.origin = static_cast<Origin>(bytes[12]);
  record.flags = bytes[13];
  std::memcpy(record.nickname.data(), bytes + RECORD_NICKNAME_OFFSET, record.nickname.size());
}

bool validateBattleCondition(const PokemonRecord& record) {
  if (record.status > POKEMON_STATUS_MAX) return false;
  for (size_t slot = 0; slot < POKEMON_MOVE_SLOTS; ++slot) {
    const uint8_t moveId = record.moves[slot];
    if (moveId == 0) {
      if (record.ppUsed[slot] != 0) return false;
      continue;
    }
    // The last move id is Struggle, which is never learned.
    if (moveId >= POKEMON_MOVE_ID_MAX) return false;
    for (size_t prior = 0; prior < slot; ++prior) {
      if (record.moves[prior] == moveId) return false;
    }
  }
  return true;
}

bool validateHallOfFame(const HallOfFame& hallOfFame) {
  bool foundEmptyMember = false;
  for (const HallOfFameMember& member : hallOfFame.team) {
    if (member.speciesId == 0) {
      if (member.level != 0) return false;
      foundEmptyMember = true;
      continue;
    }
    if (foundEmptyMember || member.speciesId > KANTO_SPECIES_COUNT || member.level == 0 || member.level > 100) {
      return false;
    }
  }
  return hallOfFame.team[0].speciesId != 0 || hallOfFame.lifetimeMinutes == 0;
}

bool validateSavedBattle(const PokemonState& state) {
  const SavedBattle& battle = state.savedBattle;
  if (battle.kind == SavedBattleKind::None) return battle == SavedBattle{};
  if (battle.playerSlot >= PARTY_SIZE || state.partyRecordIds[battle.playerSlot] == 0 ||
      battle.foeSlot >= battle.foes.size()) {
    return false;
  }
  for (const SavedBattleFoe& foe : battle.foes) {
    if (foe.status > POKEMON_STATUS_MAX) return false;
  }
  switch (battle.kind) {
    case SavedBattleKind::None:
      return false;
    case SavedBattleKind::Wild:
      // A wild battle is against the one Pokemon at the front of the waiting list.
      if (battle.leagueIndex != 0 || battle.foeSlot != 0 ||
          state.pendingEvents[0].kind != PendingEventKind::Encounter) {
        return false;
      }
      for (size_t slot = 1; slot < battle.foes.size(); ++slot) {
        if (battle.foes[slot] != SavedBattleFoe{}) return false;
      }
      return true;
    case SavedBattleKind::League:
      return battle.leagueIndex >= 1 && battle.leagueIndex <= POKEMON_GYM_PROGRESS_BITS;
  }
  return false;
}

bool canonicalNickname(const std::array<char, POKEMON_NICKNAME_BYTES>& nickname) {
  const auto terminator = std::find(nickname.begin(), nickname.end(), '\0');
  if (terminator == nickname.end()) return false;
  if (!std::all_of(terminator, nickname.end(), [](const char value) { return value == '\0'; })) return false;
  return validateNickname(std::string_view(nickname.data(), static_cast<size_t>(terminator - nickname.begin())));
}

bool genderMatchesSpecies(const uint16_t speciesId, const Gender gender) {
  const SpeciesData* species = speciesData(speciesId);
  if (species == nullptr) return false;
  if (species->genderRate == 255) return gender == Gender::Genderless;
  if (species->genderRate == 0) return gender == Gender::Male;
  if (species->genderRate == 8) return gender == Gender::Female;
  return gender == Gender::Male || gender == Gender::Female;
}

bool validatePendingEvent(const PendingEvent& event) {
  switch (event.kind) {
    case PendingEventKind::None:
      return event.recordId == 0 && event.speciesId == 0 && event.level == 0 && event.gender == Gender::Unknown &&
             event.item == EvolutionItem::None;
    case PendingEventKind::Encounter:
      return event.recordId == 0 && event.speciesId >= 1 && event.speciesId <= KANTO_SPECIES_COUNT &&
             event.level >= 1 && event.level <= 100 && genderMatchesSpecies(event.speciesId, event.gender) &&
             event.item == EvolutionItem::None;
    case PendingEventKind::Item:
      return event.recordId == 0 && event.speciesId == 0 && event.level == 0 && event.gender == Gender::Unknown &&
             event.item >= EvolutionItem::MoonStone && event.item <= EvolutionItem::LinkCable;
    case PendingEventKind::Evolution:
      return event.recordId != 0 && event.speciesId >= 1 && event.speciesId <= KANTO_SPECIES_COUNT &&
             event.level == 0 && event.gender == Gender::Unknown && event.item == EvolutionItem::None;
  }
  return false;
}

}  // namespace

bool validateNickname(const std::string_view nickname) {
  return nickname.size() < POKEMON_NICKNAME_BYTES && nickname.find('\0') == std::string_view::npos &&
         isValidUtf8(nickname);
}

bool setNickname(PokemonRecord& record, const std::string_view nickname) {
  if (!validateNickname(nickname)) return false;
  std::array<char, POKEMON_NICKNAME_BYTES> candidate{};
  std::copy(nickname.begin(), nickname.end(), candidate.begin());
  record.nickname = candidate;
  return true;
}

bool validateRecord(const PokemonRecord& record) {
  if (record.recordId == 0 || record.speciesId == 0 || record.speciesId > KANTO_SPECIES_COUNT ||
      record.totalXp > MAXIMUM_TOTAL_XP || record.caughtLevel == 0 || record.caughtLevel > 100 ||
      record.totalXp < xpRequired(record.caughtLevel) ||
      (record.flags & static_cast<uint8_t>(~ALLOWED_RECORD_FLAGS)) != 0 || !canonicalNickname(record.nickname) ||
      !validateBattleCondition(record)) {
    return false;
  }
  return genderMatchesSpecies(record.speciesId, record.gender) &&
         (record.origin == Origin::Caught || record.origin == Origin::Starter);
}

bool encodeRecord(const PokemonRecord& record, RecordBytes& output) {
  if (!validateRecord(record)) return false;
  RecordBytes candidate{};
  writeSharedRecordFields(record, candidate.data());
  candidate[RECORD_STATUS_OFFSET] = record.status;
  std::memcpy(candidate.data() + RECORD_MOVES_OFFSET, record.moves.data(), record.moves.size());
  std::memcpy(candidate.data() + RECORD_PP_USED_OFFSET, record.ppUsed.data(), record.ppUsed.size());
  write16(candidate.data(), RECORD_HP_LOST_OFFSET, record.hpLost);
  candidate[RECORD_PP_UPS_OFFSET] = record.ppUps;
  candidate[RECORD_STATUS_TURNS_OFFSET] = record.statusTurns;
  output = candidate;
  return true;
}

bool decodeRecord(const RecordBytes& bytes, PokemonRecord& output) {
  for (size_t index = RECORD_RESERVED_OFFSET; index < bytes.size(); ++index) {
    if (bytes[index] != 0) return false;
  }
  PokemonRecord candidate{};
  readSharedRecordFields(bytes.data(), candidate);
  candidate.status = bytes[RECORD_STATUS_OFFSET];
  std::memcpy(candidate.moves.data(), bytes.data() + RECORD_MOVES_OFFSET, candidate.moves.size());
  std::memcpy(candidate.ppUsed.data(), bytes.data() + RECORD_PP_USED_OFFSET, candidate.ppUsed.size());
  candidate.hpLost = read16(bytes.data(), RECORD_HP_LOST_OFFSET);
  candidate.ppUps = bytes[RECORD_PP_UPS_OFFSET];
  candidate.statusTurns = bytes[RECORD_STATUS_TURNS_OFFSET];
  if (!validateRecord(candidate)) return false;
  output = candidate;
  return true;
}

bool decodeLegacyRecord(const LegacyRecordBytes& bytes, PokemonRecord& output) {
  if (bytes[RECORD_STATUS_OFFSET] != 0) return false;
  PokemonRecord candidate{};
  readSharedRecordFields(bytes.data(), candidate);
  if (!validateRecord(candidate)) return false;
  output = candidate;
  return true;
}

uint32_t xpRequired(const uint8_t level) {
  const uint32_t clamped = std::clamp<uint32_t>(level, 1U, 100U);
  const uint32_t n = clamped - 1U;
  return 10U * n + (3U * n * n) / 4U;
}

uint8_t levelForXp(const uint32_t totalXp) {
  uint8_t low = 1;
  uint8_t high = 100;
  while (low < high) {
    const uint8_t middle = static_cast<uint8_t>(low + (high - low + 1U) / 2U);
    if (xpRequired(middle) <= totalXp) {
      low = middle;
    } else {
      high = static_cast<uint8_t>(middle - 1U);
    }
  }
  return low;
}

LevelXpProgress levelXpProgress(const uint32_t totalXp) {
  const uint8_t level = levelForXp(totalXp);
  if (level >= 100) return LevelXpProgress{100, 0, 0};
  const uint32_t levelStart = xpRequired(level);
  const uint32_t nextLevel = xpRequired(static_cast<uint8_t>(level + 1U));
  return LevelXpProgress{level, totalXp - levelStart, nextLevel - levelStart};
}

bool markSpecies(PokedexBits& bits, const uint16_t speciesId) {
  if (speciesId == 0 || speciesId > KANTO_SPECIES_COUNT) return false;
  const uint16_t zeroBased = static_cast<uint16_t>(speciesId - 1U);
  bits[zeroBased / 8U] |= static_cast<uint8_t>(1U << (zeroBased % 8U));
  return true;
}

bool isSpeciesMarked(const PokedexBits& bits, const uint16_t speciesId) {
  if (speciesId == 0 || speciesId > KANTO_SPECIES_COUNT) return false;
  const uint16_t zeroBased = static_cast<uint16_t>(speciesId - 1U);
  return (bits[zeroBased / 8U] & static_cast<uint8_t>(1U << (zeroBased % 8U))) != 0;
}

size_t pendingEventCount(const PokemonState& state) {
  size_t count = 0;
  while (count < state.pendingEvents.size() && state.pendingEvents[count].kind != PendingEventKind::None) ++count;
  return count;
}

const PendingEvent* pendingEventFront(const PokemonState& state) {
  return state.pendingEvents[0].kind == PendingEventKind::None ? nullptr : &state.pendingEvents[0];
}

PendingEvent* pendingEventFront(PokemonState& state) {
  return state.pendingEvents[0].kind == PendingEventKind::None ? nullptr : &state.pendingEvents[0];
}

bool enqueuePendingEvent(PokemonState& state, const PendingEvent& event) {
  if (event.kind == PendingEventKind::None || !validatePendingEvent(event)) return false;
  const size_t count = pendingEventCount(state);
  if (count == state.pendingEvents.size()) return false;
  for (size_t index = count; index < state.pendingEvents.size(); ++index) {
    if (state.pendingEvents[index].kind != PendingEventKind::None) return false;
  }
  state.pendingEvents[count] = event;
  return true;
}

bool dequeuePendingEvent(PokemonState& state) {
  if (state.pendingEvents[0].kind == PendingEventKind::None) return false;
  for (size_t index = 1; index < state.pendingEvents.size(); ++index) {
    state.pendingEvents[index - 1] = state.pendingEvents[index];
  }
  state.pendingEvents.back() = {};
  return true;
}

size_t removePendingEvolutionsForRecord(PokemonState& state, const uint32_t recordId) {
  if (recordId == 0) return 0;
  size_t writeIndex = 0;
  size_t removed = 0;
  for (const PendingEvent& event : state.pendingEvents) {
    if (event.kind == PendingEventKind::Evolution && event.recordId == recordId) {
      ++removed;
      continue;
    }
    if (event.kind != PendingEventKind::None) state.pendingEvents[writeIndex++] = event;
  }
  while (writeIndex < state.pendingEvents.size()) state.pendingEvents[writeIndex++] = {};
  return removed;
}

bool validateState(const PokemonState& state) {
  bool foundEmptyPartySlot = false;
  for (size_t slot = 0; slot < state.partyRecordIds.size(); ++slot) {
    const uint32_t recordId = state.partyRecordIds[slot];
    if (recordId == 0) {
      foundEmptyPartySlot = true;
      continue;
    }
    if (foundEmptyPartySlot) return false;
    for (size_t prior = 0; prior < slot; ++prior) {
      if (state.partyRecordIds[prior] == recordId) return false;
    }
  }

  if ((state.seenSpecies.back() & 0x80U) != 0 || (state.caughtSpecies.back() & 0x80U) != 0) return false;
  for (size_t index = 0; index < POKEDEX_BYTES; ++index) {
    if ((state.caughtSpecies[index] & static_cast<uint8_t>(~state.seenSpecies[index])) != 0) return false;
  }
  if (state.readingMinuteRemainder >= 60 || state.encounterMisses > 5 || state.itemMisses > 19 ||
      state.dashboardNotice > DashboardNotice::WhatsThis || (state.battleProgress >> POKEMON_GYM_PROGRESS_BITS) != 0 ||
      !validateHallOfFame(state.hallOfFame)) {
    return false;
  }
  for (const uint8_t count : state.bagCounts) {
    if (count > POKEMON_BAG_STACK_MAX) return false;
  }

  bool foundEmptyEvent = false;
  for (const PendingEvent& event : state.pendingEvents) {
    if (!validatePendingEvent(event)) return false;
    if (event.kind == PendingEventKind::None) {
      foundEmptyEvent = true;
    } else if (foundEmptyEvent) {
      return false;
    }
  }
  return validateSavedBattle(state);
}

uint8_t bagCount(const PokemonState& state, const uint8_t itemId) {
  if (itemId < POKEMON_FIRST_BAG_ITEM_ID || itemId > POKEMON_ITEM_ID_MAX) return 0;
  return state.bagCounts[itemId - POKEMON_FIRST_BAG_ITEM_ID];
}

bool addBagItem(PokemonState& state, const uint8_t itemId, const uint8_t count) {
  if (itemId < POKEMON_FIRST_BAG_ITEM_ID || itemId > POKEMON_ITEM_ID_MAX) return false;
  uint8_t& held = state.bagCounts[itemId - POKEMON_FIRST_BAG_ITEM_ID];
  held = static_cast<uint8_t>(std::min<uint16_t>(POKEMON_BAG_STACK_MAX, static_cast<uint16_t>(held + count)));
  return true;
}

void grantStartingItems(PokemonState& state) {
  addBagItem(state, POKEMON_ITEM_POKE_BALL, POKEMON_STARTING_POKE_BALLS);
  addBagItem(state, POKEMON_ITEM_POTION, POKEMON_STARTING_POTIONS);
}

}  // namespace pokemon

#endif
