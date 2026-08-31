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
  const int identityWidth = textWidth * 45 / 100;
  const int statsWidth = textWidth * 25 / 100;
  const int statsX = textX + identityWidth;
  const int halfHeight = height / 2;
  layout.identity = {textX, 0, identityWidth, height};
  layout.level = {statsX, 0, statsWidth, halfHeight};
  layout.gender = {width, 0, 0, 0};
  layout.xp = {statsX, halfHeight, statsWidth, height - halfHeight};
  layout.notice = {statsX + statsWidth, 0, textWidth - identityWidth - statsWidth, height};
  layout.valid = true;
  return layout;
}

}  // namespace pokemon

#endif
