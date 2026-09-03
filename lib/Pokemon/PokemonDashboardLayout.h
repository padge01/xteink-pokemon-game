#pragma once

namespace pokemon {

struct DashboardBox {
  int x = 0;
  int y = 0;
  int width = 0;
  int height = 0;
};

struct DashboardLayout {
  DashboardBox sprite{};
  DashboardBox identity{};
  DashboardBox level{};
  DashboardBox gender{};
  DashboardBox xp{};
  DashboardBox notice{};
  bool singleRow = false;
  bool valid = false;
};

struct HomeAccessorySizing {
  int coverHeight = 0;
  int accessoryHeight = 0;
  int followingContentOffset = 0;
};

constexpr int kPokemonHomeAccessoryHeight = 68;
constexpr int kPokemonHomeAccessoryFollowingOffset = 72;

DashboardLayout pokemonDashboardLayout(int width, int height);
HomeAccessorySizing classicHomeAccessorySizing(int baseCoverHeight, bool visible);
bool pokemonHomeAccessoryVisible(bool enabled, bool themeSupported, bool hasLeader);

}  // namespace pokemon
