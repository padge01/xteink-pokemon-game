#if defined(CROSSINK_ENABLE_POKEMON)

#include "PokemonArt.h"

#include <Bitmap.h>
#include <HalStorage.h>
#include <Logging.h>

#include "PokemonArtPath.h"
#include "fontIds.h"

namespace pokemon {
namespace {

void drawFallback(const GfxRenderer& renderer, const Rect bounds) {
  const char* mark = "?";
  const int width = renderer.getTextWidth(UI_12_FONT_ID, mark);
  const int height = renderer.getLineHeight(UI_12_FONT_ID);
  renderer.drawText(UI_12_FONT_ID, bounds.x + (bounds.width - width) / 2,
                    bounds.y + (bounds.height - height) / 2, mark);
}

bool drawPath(const GfxRenderer& renderer, const char* path, const Rect bounds, const bool fallback) {
  if (path == nullptr || bounds.width <= 0 || bounds.height <= 0) return false;
  FsFile file;
  if (!Storage.openFileForRead("PKART", path, file)) {
    LOG_ERR("PKART", "Missing Pokemon art: %s", path);
    if (fallback) drawFallback(renderer, bounds);
    return false;
  }
  Bitmap bitmap(file);
  const bool valid = bitmap.parseHeaders() == BmpReaderError::Ok;
  if (valid) {
    renderer.drawBitmap(bitmap, bounds.x, bounds.y, bounds.width, bounds.height);
  } else {
    LOG_ERR("PKART", "Invalid Pokemon art: %s", path);
    if (fallback) drawFallback(renderer, bounds);
  }
  file.close();
  return valid;
}

}  // namespace

bool drawPokemonSpeciesArt(const GfxRenderer& renderer, const uint16_t speciesId, const bool hero,
                           const Rect bounds, const bool fallback) {
  char path[64]{};
  return drawPath(renderer, pokemonSpeciesArtPath(speciesId, hero, path, sizeof(path)), bounds, fallback);
}

bool drawPokemonItemArt(const GfxRenderer& renderer, const EvolutionItem item, const bool hero,
                        const Rect bounds, const bool fallback) {
  char path[80]{};
  const char* pathValue = pokemonItemArtPath(item, hero, path, sizeof(path));
  if (pathValue == nullptr && item == EvolutionItem::LinkCable) return false;
  return drawPath(renderer, pathValue, bounds, fallback);
}

}  // namespace pokemon

#endif
