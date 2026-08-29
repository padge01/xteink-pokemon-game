#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace pokemon {

constexpr size_t POKEMON_RECORD_BYTES = 48;
constexpr size_t POKEMON_NICKNAME_BYTES = 33;
constexpr uint16_t KANTO_SPECIES_COUNT = 151;
constexpr uint32_t MAXIMUM_TOTAL_XP = 8340;
constexpr size_t POKEDEX_BYTES = 19;
constexpr size_t PARTY_SIZE = 6;
constexpr size_t EVOLUTION_ITEM_COUNT = 6;

enum class Gender : uint8_t {
  Unknown = 0,
  Male = 1,
  Female = 2,
  Genderless = 3,
};

enum class Origin : uint8_t {
  Unknown = 0,
  Caught = 1,
  Starter = 2,
};

enum class RecordFlag : uint8_t {
  EvolutionPromptsDisabled = 1U << 0U,
};

enum class EvolutionItem : uint8_t {
  None = 0,
  MoonStone = 1,
  FireStone = 2,
  ThunderStone = 3,
  WaterStone = 4,
  LeafStone = 5,
  LinkCable = 6,
};

enum class PendingEventKind : uint8_t {
  None = 0,
  Encounter = 1,
  Item = 2,
  Evolution = 3,
};

enum class DashboardNotice : uint8_t {
  None = 0,
  NewPokemon = 1,
  ItemFound = 2,
  WhatsThis = 3,
};

constexpr uint8_t recordFlag(const RecordFlag flag) { return static_cast<uint8_t>(flag); }

struct PokemonRecord {
  uint32_t recordId = 0;
  uint32_t totalXp = 0;
  uint16_t speciesId = 0;
  uint8_t caughtLevel = 0;
  Gender gender = Gender::Unknown;
  Origin origin = Origin::Unknown;
  uint8_t flags = 0;
  std::array<char, POKEMON_NICKNAME_BYTES> nickname{};

  bool operator==(const PokemonRecord&) const = default;
};

using RecordBytes = std::array<uint8_t, POKEMON_RECORD_BYTES>;

struct PendingEvent {
  uint32_t recordId = 0;
  uint16_t speciesId = 0;
  uint8_t level = 0;
  Gender gender = Gender::Unknown;
  EvolutionItem item = EvolutionItem::None;
  PendingEventKind kind = PendingEventKind::None;

  bool operator==(const PendingEvent&) const = default;
};

using PokedexBits = std::array<uint8_t, POKEDEX_BYTES>;

struct PokemonState {
  std::array<uint32_t, PARTY_SIZE> partyRecordIds{};
  PendingEvent pending{};
  std::array<uint16_t, EVOLUTION_ITEM_COUNT> itemCounts{};
  PokedexBits seenSpecies{};
  PokedexBits caughtSpecies{};
  uint32_t lifetimeMinutes = 0;
  uint32_t sequence = 0;
  uint8_t readingMinuteRemainder = 0;
  uint8_t encounterMisses = 0;
  uint8_t itemMisses = 0;
  DashboardNotice dashboardNotice = DashboardNotice::None;

  bool operator==(const PokemonState&) const = default;
};

bool validateNickname(std::string_view nickname);
bool setNickname(PokemonRecord& record, std::string_view nickname);
bool validateRecord(const PokemonRecord& record);
bool encodeRecord(const PokemonRecord& record, RecordBytes& output);
bool decodeRecord(const RecordBytes& bytes, PokemonRecord& output);
uint32_t xpRequired(uint8_t level);
uint8_t levelForXp(uint32_t totalXp);
bool markSpecies(PokedexBits& bits, uint16_t speciesId);
bool isSpeciesMarked(const PokedexBits& bits, uint16_t speciesId);
bool validateState(const PokemonState& state);

}  // namespace pokemon
