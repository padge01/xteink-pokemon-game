#include "PokemonTypes.h"

#include <algorithm>
#include <cstring>

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

void write16(RecordBytes& bytes, const size_t offset, const uint16_t value) {
  bytes[offset] = static_cast<uint8_t>(value);
  bytes[offset + 1] = static_cast<uint8_t>(value >> 8U);
}

void write32(RecordBytes& bytes, const size_t offset, const uint32_t value) {
  bytes[offset] = static_cast<uint8_t>(value);
  bytes[offset + 1] = static_cast<uint8_t>(value >> 8U);
  bytes[offset + 2] = static_cast<uint8_t>(value >> 16U);
  bytes[offset + 3] = static_cast<uint8_t>(value >> 24U);
}

uint16_t read16(const RecordBytes& bytes, const size_t offset) {
  return static_cast<uint16_t>(bytes[offset]) |
         static_cast<uint16_t>(static_cast<uint16_t>(bytes[offset + 1]) << 8U);
}

uint32_t read32(const RecordBytes& bytes, const size_t offset) {
  return static_cast<uint32_t>(bytes[offset]) |
         (static_cast<uint32_t>(bytes[offset + 1]) << 8U) |
         (static_cast<uint32_t>(bytes[offset + 2]) << 16U) |
         (static_cast<uint32_t>(bytes[offset + 3]) << 24U);
}

bool canonicalNickname(const std::array<char, POKEMON_NICKNAME_BYTES>& nickname) {
  const auto terminator = std::find(nickname.begin(), nickname.end(), '\0');
  if (terminator == nickname.end()) return false;
  if (!std::all_of(terminator, nickname.end(), [](const char value) { return value == '\0'; })) return false;
  return validateNickname(std::string_view(nickname.data(), static_cast<size_t>(terminator - nickname.begin())));
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
      (record.flags & static_cast<uint8_t>(~ALLOWED_RECORD_FLAGS)) != 0 || !canonicalNickname(record.nickname)) {
    return false;
  }
  if (record.gender != Gender::Male && record.gender != Gender::Female && record.gender != Gender::Genderless) {
    return false;
  }
  return record.origin == Origin::Caught || record.origin == Origin::Starter;
}

bool encodeRecord(const PokemonRecord& record, RecordBytes& output) {
  if (!validateRecord(record)) return false;
  RecordBytes candidate{};
  write32(candidate, 0, record.recordId);
  write32(candidate, 4, record.totalXp);
  write16(candidate, 8, record.speciesId);
  candidate[10] = record.caughtLevel;
  candidate[11] = static_cast<uint8_t>(record.gender);
  candidate[12] = static_cast<uint8_t>(record.origin);
  candidate[13] = record.flags;
  std::memcpy(candidate.data() + 14, record.nickname.data(), record.nickname.size());
  output = candidate;
  return true;
}

bool decodeRecord(const RecordBytes& bytes, PokemonRecord& output) {
  if (bytes[47] != 0) return false;
  PokemonRecord candidate{};
  candidate.recordId = read32(bytes, 0);
  candidate.totalXp = read32(bytes, 4);
  candidate.speciesId = read16(bytes, 8);
  candidate.caughtLevel = bytes[10];
  candidate.gender = static_cast<Gender>(bytes[11]);
  candidate.origin = static_cast<Origin>(bytes[12]);
  candidate.flags = bytes[13];
  std::memcpy(candidate.nickname.data(), bytes.data() + 14, candidate.nickname.size());
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
      state.dashboardNotice > DashboardNotice::WhatsThis) {
    return false;
  }

  const PendingEvent& event = state.pending;
  switch (event.kind) {
    case PendingEventKind::None:
      return event.recordId == 0 && event.speciesId == 0 && event.level == 0 && event.gender == Gender::Unknown &&
             event.item == EvolutionItem::None;
    case PendingEventKind::Encounter:
      return event.recordId == 0 && event.speciesId >= 1 && event.speciesId <= KANTO_SPECIES_COUNT &&
             event.level >= 1 && event.level <= 100 &&
             (event.gender == Gender::Male || event.gender == Gender::Female || event.gender == Gender::Genderless) &&
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

}  // namespace pokemon
