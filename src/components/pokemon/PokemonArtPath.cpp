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

const char* pokemonPokedexArtPath(const uint16_t speciesId, const char* speciesName, char* output,
                                  const size_t outputSize) {
  if (output == nullptr || outputSize == 0) return nullptr;
  output[0] = '\0';
  if (speciesId == 0 || speciesId > KANTO_SPECIES_COUNT || speciesName == nullptr || speciesName[0] == '\0') {
    return nullptr;
  }

  const char* specialSlug = nullptr;
  switch (speciesId) {
    case 29:
      specialSlug = "nidoran-f";
      break;
    case 32:
      specialSlug = "nidoran-m";
      break;
    case 83:
      specialSlug = "farfetchd";
      break;
    case 122:
      specialSlug = "mr-mime";
      break;
    default:
      break;
  }

  char slug[32]{};
  if (specialSlug != nullptr) {
    std::snprintf(slug, sizeof(slug), "%s", specialSlug);
  } else {
    size_t written = 0;
    bool lastWasDash = false;
    for (size_t index = 0; speciesName[index] != '\0' && written + 1 < sizeof(slug); ++index) {
      const unsigned char value = static_cast<unsigned char>(speciesName[index]);
      if (value >= 'A' && value <= 'Z') {
        slug[written++] = static_cast<char>(value - 'A' + 'a');
        lastWasDash = false;
      } else if ((value >= 'a' && value <= 'z') || (value >= '0' && value <= '9')) {
        slug[written++] = static_cast<char>(value);
        lastWasDash = false;
      } else if (value == ' ' || value == '-') {
        if (written != 0 && !lastWasDash) {
          slug[written++] = '-';
          lastWasDash = true;
        }
      }
    }
    if (written != 0 && slug[written - 1] == '-') slug[written - 1] = '\0';
  }
  return finishPath(output, outputSize,
                    std::snprintf(output, outputSize, "/sleep/%03u-%s.bmp", static_cast<unsigned>(speciesId), slug));
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
