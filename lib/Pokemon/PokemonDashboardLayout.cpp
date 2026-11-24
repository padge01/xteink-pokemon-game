#if defined(CROSSINK_ENABLE_POKEMON)

#include "PokemonDashboardLayout.h"

#include <algorithm>

namespace pokemon {

DashboardLayout pokemonDashboardLayout(const int width, const int height) {
  DashboardLayout layout{};
  if (width < 240 || height < 60) return layout;

  layout.sprite = {0, 0, 104, height};
  constexpr int textX = 112;
  const int textWidth = width - textX - 6;
  layout.singleRow = width >= 620;
  constexpr int noticeWidth = 28;
  const int usefulWidth = textWidth - noticeWidth;
  const int identityWidth = usefulWidth * 55 / 100;
  const int statsWidth = usefulWidth - identityWidth;
  const int statsX = textX + identityWidth;
  const int halfHeight = height / 2;
  layout.identity = {textX, 0, identityWidth, height};
  layout.level = {statsX, 0, statsWidth, halfHeight};
  layout.gender = {width, 0, 0, 0};
  layout.xp = {statsX, halfHeight, statsWidth, height - halfHeight};
  layout.notice = {statsX + statsWidth, 0, noticeWidth, height};
  layout.valid = true;
  return layout;
}

HomeAccessorySizing classicHomeAccessorySizing(const int baseCoverHeight, const bool visible) {
  const int safeCoverHeight = std::max(0, baseCoverHeight);
  if (!visible || safeCoverHeight < kPokemonHomeAccessoryFollowingOffset) {
    return {safeCoverHeight, 0, 0};
  }

  return {safeCoverHeight - kPokemonHomeAccessoryFollowingOffset, kPokemonHomeAccessoryHeight,
          kPokemonHomeAccessoryFollowingOffset};
}

bool pokemonHomeAccessoryVisible(const bool enabled, const bool themeSupported, const bool hasLeader) {
  return enabled && themeSupported && hasLeader;
}

}  // namespace pokemon

#endif
