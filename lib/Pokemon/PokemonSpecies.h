#pragma once

#include <cstdint>
#include <span>

#include "PokemonTypes.h"

namespace pokemon {

enum class PokemonType : uint8_t {
  None = 0,
  Normal,
  Fire,
  Water,
  Electric,
  Grass,
  Ice,
  Fighting,
  Poison,
  Ground,
  Flying,
  Psychic,
  Bug,
  Rock,
  Ghost,
  Dragon,
  Dark,
  Steel,
  Fairy,
};

enum class EvolutionStage : uint8_t {
  Base = 0,
  Middle = 1,
  Final = 2,
};

enum class Acquisition : uint8_t {
  Wild = 0,
  EvolutionOnly = 1,
  Legendary = 2,
  Special = 3,
};

enum class EvolutionTrigger : uint8_t {
  Level = 0,
  Item = 1,
};

struct EvolutionRule {
  uint16_t targetSpeciesId;
  EvolutionTrigger trigger;
  uint8_t minimumLevel;
  EvolutionItem item;

  bool operator==(const EvolutionRule&) const = default;
};

struct SpeciesData {
  uint16_t speciesId;
  const char* name;
  PokemonType primaryType;
  PokemonType secondaryType;
  uint8_t genderRate;
  uint8_t captureRate;
  EvolutionStage stage;
  Acquisition acquisition;
  uint8_t evolutionOffset;
  uint8_t evolutionCount;
};

const SpeciesData* speciesData(uint16_t speciesId);
std::span<const EvolutionRule> evolutionsFor(uint16_t speciesId);

}  // namespace pokemon
