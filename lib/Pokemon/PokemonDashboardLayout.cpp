#if defined(CROSSINK_ENABLE_POKEMON)

#include "PokemonDashboardLayout.h"

namespace pokemon {

DashboardLayout pokemonDashboardLayout(const int width, const int height) {
  DashboardLayout layout{};
  if (width < 240 || height < 60) return layout;

  layout.sprite = {4, (height - 60) / 2, 80, 60};
  constexpr int textX = 92;
  const int textWidth = width - textX - 6;
  layout.singleRow = width >= 620;
  if (layout.singleRow) {
    const int identityWidth = textWidth * 28 / 100;
    const int levelWidth = textWidth * 20 / 100;
    const int xpWidth = textWidth * 30 / 100;
    layout.identity = {textX, 0, identityWidth, height};
    layout.level = {textX + identityWidth, 0, levelWidth, height};
    layout.xp = {textX + identityWidth + levelWidth, 0, xpWidth, height};
    layout.notice = {layout.xp.x + xpWidth, 0, textWidth - identityWidth - levelWidth - xpWidth, height};
  } else {
    const int topHeight = height / 2;
    const int halfWidth = textWidth / 2;
    layout.identity = {textX, 0, halfWidth, topHeight};
    layout.level = {textX + halfWidth, 0, textWidth - halfWidth, topHeight};
    layout.xp = {textX, topHeight, halfWidth, height - topHeight};
    layout.notice = {textX + halfWidth, topHeight, textWidth - halfWidth, height - topHeight};
  }
  layout.valid = true;
  return layout;
}

}  // namespace pokemon

#endif
