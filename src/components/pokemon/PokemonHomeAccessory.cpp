#if defined(CROSSINK_ENABLE_POKEMON)

#include "PokemonHomeAccessory.h"

#include <I18n.h>
#include <PokemonDashboardLayout.h>
#include <PokemonSpecies.h>
#include <Utf8.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "CrossPointSettings.h"
#include "PokemonArt.h"
#include "fontIds.h"

namespace pokemon {
namespace {

template <size_t Size>
const char* fit(const GfxRenderer& renderer, const int font, const char* source, const int width, char (&out)[Size],
                const EpdFontFamily::Style style = EpdFontFamily::REGULAR) {
  snprintf(out, Size, "%s", source == nullptr ? "" : source);
  int length = utf8SafeTruncateBuffer(out, static_cast<int>(strlen(out)));
  out[length] = '\0';
  while (length > 0 && renderer.getTextWidth(font, out, style) > width) {
    length = utf8SafeTruncateBuffer(out, length - 1);
    out[length] = '\0';
  }
  return out;
}

}  // namespace

bool pokemonHomeAccessorySupported(const uint8_t theme) {
  const auto selected = static_cast<CrossPointSettings::UI_THEME>(theme);
  return selected == CrossPointSettings::UI_THEME::CLASSIC || selected == CrossPointSettings::UI_THEME::MINIMAL ||
         selected == CrossPointSettings::UI_THEME::DASHBOARD ||
         selected == CrossPointSettings::UI_THEME::LYRA ||
         selected == CrossPointSettings::UI_THEME::LYRA_3_COVERS ||
         selected == CrossPointSettings::UI_THEME::LYRA_CAROUSEL ||
         selected == CrossPointSettings::UI_THEME::ROUNDEDRAFF;
}

void drawPokemonHomeAccessory(const GfxRenderer& renderer, const PokemonDashboardSnapshot& snapshot,
                              const Rect bounds) {
  const DashboardLayout layout = pokemonDashboardLayout(bounds.width, bounds.height);
  if (snapshot.leader.recordId == 0 || !layout.valid) return;
  renderer.fillRect(bounds.x, bounds.y, bounds.width, bounds.height, false);
  const int spriteWidth = std::min(layout.sprite.width, layout.sprite.height * 4 / 3);
  const int spriteX = layout.sprite.x + (layout.sprite.width - spriteWidth) / 2;
  drawPokemonSpeciesArt(renderer, snapshot.leader.speciesId, true,
                        Rect{bounds.x + spriteX, bounds.y + layout.sprite.y, spriteWidth, layout.sprite.height});

  const SpeciesData* species = speciesData(snapshot.leader.speciesId);
  const char* speciesLabel = species == nullptr ? "???" : species->name;
  const LevelXpProgress progress = levelXpProgress(snapshot.leader.totalXp);
  char levelText[24];
  char xpText[40];
  snprintf(levelText, sizeof(levelText), "%s %u", tr(STR_POKEMON_LEVEL), progress.level);
  if (progress.required == 0) {
    snprintf(xpText, sizeof(xpText), "%s %s", tr(STR_POKEMON_EXP_SHORT), tr(STR_POKEMON_MAX));
  } else {
    snprintf(xpText, sizeof(xpText), "%s %lu / %lu", tr(STR_POKEMON_EXP_SHORT),
             static_cast<unsigned long>(progress.earned), static_cast<unsigned long>(progress.required));
  }

  const int top = bounds.y + 5;
  const int bottom = bounds.y + bounds.height / 2 + 1;
  char speciesFit[40];
  char levelFit[24];
  char xpFit[40];
  renderer.drawText(UI_12_FONT_ID, bounds.x + layout.identity.x, top,
                    fit(renderer, UI_12_FONT_ID, speciesLabel, layout.identity.width, speciesFit, EpdFontFamily::BOLD),
                    true, EpdFontFamily::BOLD);
  if (snapshot.leader.nickname[0] != '\0') {
    char nicknameFit[40];
    renderer.drawText(
        UI_10_FONT_ID, bounds.x + layout.identity.x, bottom,
        fit(renderer, UI_10_FONT_ID, snapshot.leader.nickname.data(), layout.identity.width, nicknameFit));
  }
  renderer.drawText(UI_10_FONT_ID, bounds.x + layout.level.x, top,
                    fit(renderer, UI_10_FONT_ID, levelText, layout.level.width, levelFit));
  renderer.drawText(UI_10_FONT_ID, bounds.x + layout.xp.x, bottom,
                    fit(renderer, UI_10_FONT_ID, xpText, layout.xp.width, xpFit));
  if (snapshot.notice != DashboardNotice::None) {
    constexpr const char* indicator = "!";
    const int noticeX =
        bounds.x + layout.notice.x +
        (layout.notice.width - renderer.getTextWidth(UI_12_FONT_ID, indicator, EpdFontFamily::BOLD)) / 2;
    const int noticeY = bounds.y + (bounds.height - renderer.getLineHeight(UI_12_FONT_ID)) / 2;
    renderer.drawText(UI_12_FONT_ID, noticeX, noticeY, indicator, true, EpdFontFamily::BOLD);
  }
}

}  // namespace pokemon

#endif
