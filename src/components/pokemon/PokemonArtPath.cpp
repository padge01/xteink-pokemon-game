#include "PokemonArtPath.h"

#include <cstdio>

namespace pokemon {
namespace {

const char* finishPath(char* output, const size_t outputSize, const int written) {
  if (written < 0 || static_cast<size_t>(written) >= outputSize) {
    if (output != nullptr && outputSize != 0) output[0] = '\0';
    return nullptr;
  }
  return output;
}

const char* itemSlug(const EvolutionItem item) {
  switch (item) {
    case EvolutionItem::MoonStone:
      return "moon-stone";
    case EvolutionItem::FireStone:
      return "fire-stone";
    case EvolutionItem::ThunderStone:
      return "thunder-stone";
    case EvolutionItem::WaterStone:
      return "water-stone";
    case EvolutionItem::LeafStone:
      return "leaf-stone";
    case EvolutionItem::None:
    case EvolutionItem::LinkCable:
      return nullptr;
  }
  return nullptr;
}

}  // namespace

const char* pokemonSpeciesArtPath(const uint16_t speciesId, const bool hero, char* output,
                                  const size_t outputSize) {
  if (output == nullptr || outputSize == 0) return nullptr;
  output[0] = '\0';
  if (speciesId == 0 || speciesId > KANTO_SPECIES_COUNT) return nullptr;
  return finishPath(output, outputSize,
                    std::snprintf(output, outputSize, hero ? "/.crosspoint/pokemon/heroes/%03u.bmp"
                                                          : "/.crosspoint/pokemon/sprites/%03u.bmp",
                                  static_cast<unsigned>(speciesId)));
}

const char* pokemonItemArtPath(const EvolutionItem item, const bool hero, char* output,
                               const size_t outputSize) {
  if (output == nullptr || outputSize == 0) return nullptr;
  output[0] = '\0';
  const char* slug = itemSlug(item);
  if (slug == nullptr) return nullptr;
  return finishPath(output, outputSize,
                    std::snprintf(output, outputSize, hero ? "/.crosspoint/pokemon/heroes/items/%s.bmp"
                                                          : "/.crosspoint/pokemon/items/%s.bmp",
                                  slug));
}

}  // namespace pokemon
