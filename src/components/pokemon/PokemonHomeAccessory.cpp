#if defined(CROSSINK_ENABLE_POKEMON)

#include "PokemonHomeAccessory.h"

#include <I18n.h>
#include <PokemonDashboardLayout.h>
#include <PokemonSpecies.h>
#include <Utf8.h>

#include <cstdio>
#include <cstring>

#include "CrossPointSettings.h"
#include "PokemonArt.h"
#include "fontIds.h"

namespace pokemon {
namespace {

const char* noticeText(const DashboardNotice notice) {
  switch (notice) {
    case DashboardNotice::NewPokemon:
      return tr(STR_POKEMON_NEW);
    case DashboardNotice::ItemFound:
      return tr(STR_POKEMON_ITEM_FOUND);
    case DashboardNotice::WhatsThis:
      return tr(STR_POKEMON_WHATS_THIS);
    default:
      return "";
  }
}

const char* genderText(const Gender gender) {
  if (gender == Gender::Male) return "♂";
  if (gender == Gender::Female) return "♀";
  if (gender == Gender::Genderless) return "—";
  return "";
}

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
  return selected == CrossPointSettings::UI_THEME::DASHBOARD || selected == CrossPointSettings::UI_THEME::LYRA ||
         selected == CrossPointSettings::UI_THEME::LYRA_3_COVERS ||
         selected == CrossPointSettings::UI_THEME::ROUNDEDRAFF;
}

void drawPokemonHomeAccessory(const GfxRenderer& renderer, const PokemonDashboardSnapshot& snapshot,
                               const Rect bounds) {
  const DashboardLayout layout = pokemonDashboardLayout(bounds.width, bounds.height);
  if (snapshot.leader.recordId == 0 || !layout.valid) return;
  renderer.fillRect(bounds.x, bounds.y, bounds.width, bounds.height, false);
  drawPokemonSpeciesArt(renderer, snapshot.leader.speciesId, true,
                        Rect{bounds.x + layout.sprite.x, bounds.y + layout.sprite.y, layout.sprite.width,
                             layout.sprite.height});

  const SpeciesData* species = speciesData(snapshot.leader.speciesId);
  const char* identity = snapshot.leader.nickname[0] == '\0'
                             ? (species == nullptr ? "???" : species->name)
                             : snapshot.leader.nickname.data();
  const uint8_t level = levelForXp(snapshot.leader.totalXp);
  const uint32_t nextXp = level >= 100 ? MAXIMUM_TOTAL_XP : xpRequired(level + 1);
  char levelText[32];
  char xpText[40];
  snprintf(levelText, sizeof(levelText), "%s %u  %s", tr(STR_POKEMON_LEVEL), level, genderText(snapshot.leader.gender));
  snprintf(xpText, sizeof(xpText), "%s %lu / %lu", tr(STR_POKEMON_EXP_POINTS),
           static_cast<unsigned long>(snapshot.leader.totalXp), static_cast<unsigned long>(nextXp));

  const char* notice = noticeText(snapshot.notice);
  if (layout.singleRow) {
    const int y = bounds.y + (bounds.height - renderer.getLineHeight(UI_10_FONT_ID)) / 2;
    char identityFit[40], levelFit[32], xpFit[40], noticeFit[32];
    renderer.drawText(UI_12_FONT_ID, bounds.x + layout.identity.x, y,
                      fit(renderer, UI_12_FONT_ID, identity, layout.identity.width, identityFit,
                          EpdFontFamily::BOLD),
                      true, EpdFontFamily::BOLD);
    renderer.drawText(UI_10_FONT_ID, bounds.x + layout.level.x, y,
                      fit(renderer, UI_10_FONT_ID, levelText, layout.level.width, levelFit));
    renderer.drawText(UI_10_FONT_ID, bounds.x + layout.xp.x, y,
                      fit(renderer, UI_10_FONT_ID, xpText, layout.xp.width, xpFit));
    renderer.drawText(UI_10_FONT_ID, bounds.x + layout.notice.x, y,
                      fit(renderer, UI_10_FONT_ID, notice, layout.notice.width, noticeFit), true,
                      EpdFontFamily::BOLD);
    return;
  }

  const int top = bounds.y + layout.identity.y + 8;
  const int bottom = bounds.y + layout.xp.y + 1;
  char identityFit[40], levelFit[32], xpFit[40], noticeFit[32];
  renderer.drawText(UI_12_FONT_ID, bounds.x + layout.identity.x, top,
                    fit(renderer, UI_12_FONT_ID, identity, layout.identity.width, identityFit,
                        EpdFontFamily::BOLD),
                    true, EpdFontFamily::BOLD);
  renderer.drawText(UI_10_FONT_ID, bounds.x + layout.level.x, top,
                    fit(renderer, UI_10_FONT_ID, levelText, layout.level.width, levelFit));
  renderer.drawText(UI_10_FONT_ID, bounds.x + layout.xp.x, bottom,
                    fit(renderer, UI_10_FONT_ID, xpText, layout.xp.width, xpFit));
  renderer.drawText(UI_10_FONT_ID, bounds.x + layout.notice.x, bottom,
                    fit(renderer, UI_10_FONT_ID, notice, layout.notice.width, noticeFit), true,
                    EpdFontFamily::BOLD);
}

}  // namespace pokemon

#endif
