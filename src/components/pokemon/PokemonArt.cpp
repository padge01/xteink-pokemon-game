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

bool drawPath(const GfxRenderer& renderer, const char* path, const Rect bounds, const bool fallback,
              const GfxRenderer::BitmapBwPolicy bwPolicy) {
  if (path == nullptr || bounds.width <= 0 || bounds.height <= 0) return false;
  FsFile file;
  if (!Storage.openFileForRead("PKART", path, file)) {
    LOG_ERR("PKART", "Missing Pokemon art: %s", path);
    if (fallback) drawFallback(renderer, bounds);
    return false;
  }
  Bitmap bitmap(file);
  const BmpReaderError parseResult = bitmap.parseHeaders();
  if (parseResult != BmpReaderError::Ok) {
    LOG_ERR("PKART", "Invalid Pokemon art: %s", path);
    if (fallback) drawFallback(renderer, bounds);
    file.close();
    return false;
  }

  const bool rendered = renderer.drawBitmap(bitmap, bounds.x, bounds.y, bounds.width, bounds.height, 0.0f, 0.0f,
                                            bwPolicy);
  if (!rendered) {
    LOG_ERR("PKART", "Could not render Pokemon art: %s", path);
    if (fallback) drawFallback(renderer, bounds);
  }
  file.close();
  return rendered;
}

}  // namespace

bool drawPokemonSpeciesArt(const GfxRenderer& renderer, const uint16_t speciesId, const bool hero,
                           const Rect bounds, const bool fallback) {
  char path[64]{};
  return drawPath(renderer, pokemonSpeciesArtPath(speciesId, hero, path, sizeof(path)), bounds, fallback,
                  GfxRenderer::BitmapBwPolicy::ExistingThreshold);
}

bool drawPokemonPokedexArt(const GfxRenderer& renderer, const uint16_t speciesId, const char* speciesName,
                           const Rect bounds, const bool fallback) {
  char path[64]{};
  return drawPath(renderer, pokemonPokedexArtPath(speciesId, speciesName, path, sizeof(path)), bounds, fallback,
                  GfxRenderer::BitmapBwPolicy::DitherNativeGray);
}

bool drawPokemonItemArt(const GfxRenderer& renderer, const EvolutionItem item, const bool hero,
                        const Rect bounds, const bool fallback) {
  char path[80]{};
  const char* pathValue = pokemonItemArtPath(item, hero, path, sizeof(path));
  if (pathValue == nullptr && item == EvolutionItem::LinkCable) return false;
  return drawPath(renderer, pathValue, bounds, fallback, GfxRenderer::BitmapBwPolicy::ExistingThreshold);
}

}  // namespace pokemon

#endif
