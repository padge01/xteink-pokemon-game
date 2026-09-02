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

DashboardLayout pokemonDashboardLayout(int width, int height);

}  // namespace pokemon
