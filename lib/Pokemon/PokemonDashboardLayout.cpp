#if defined(CROSSINK_ENABLE_POKEMON)

#include "PokemonDashboardLayout.h"

namespace pokemon {

DashboardLayout pokemonDashboardLayout(const int width, const int height) {
  DashboardLayout layout{};
  if (width < 240 || height < 60) return layout;

  layout.sprite = {0, 0, 104, height};
  constexpr int textX = 112;
  const int textWidth = width - textX - 6;
  layout.singleRow = width >= 620;
  if (layout.singleRow) {
    const int identityWidth = textWidth * 25 / 100;
    const int levelWidth = textWidth * 15 / 100;
    const int genderWidth = textWidth * 15 / 100;
    const int xpWidth = textWidth * 25 / 100;
    layout.identity = {textX, 0, identityWidth, height};
    layout.level = {textX + identityWidth, 0, levelWidth, height};
    layout.gender = {layout.level.x + levelWidth, 0, genderWidth, height};
    layout.xp = {layout.gender.x + genderWidth, 0, xpWidth, height};
    layout.notice = {layout.xp.x + xpWidth, 0,
                     textWidth - identityWidth - levelWidth - genderWidth - xpWidth, height};
  } else {
    const int topHeight = height / 2;
    const int identityWidth = textWidth * 45 / 100;
    const int levelWidth = textWidth * 25 / 100;
    const int xpWidth = textWidth * 55 / 100;
    layout.identity = {textX, 0, identityWidth, topHeight};
    layout.level = {layout.identity.x + identityWidth, 0, levelWidth, topHeight};
    layout.gender = {layout.level.x + levelWidth, 0, textWidth - identityWidth - levelWidth, topHeight};
    layout.xp = {textX, topHeight, xpWidth, height - topHeight};
    layout.notice = {layout.xp.x + xpWidth, topHeight, textWidth - xpWidth, height - topHeight};
  }
  layout.valid = true;
  return layout;
}

}  // namespace pokemon

#endif
