#include <gtest/gtest.h>

#include <PokemonDashboardLayout.h>

namespace {

bool inside(const pokemon::DashboardBox& box, const int width, const int height) {
  return box.x >= 0 && box.y >= 0 && box.x + box.width <= width && box.y + box.height <= height;
}

bool overlaps(const pokemon::DashboardBox& left, const pokemon::DashboardBox& right) {
  return left.x < right.x + right.width && left.x + left.width > right.x && left.y < right.y + right.height &&
         left.y + left.height > right.y;
}

void expectValid(const int width) {
  constexpr int height = 68;
  const auto layout = pokemon::pokemonDashboardLayout(width, height);
  ASSERT_TRUE(layout.valid);
  EXPECT_EQ(layout.sprite.width, 80);
  EXPECT_EQ(layout.sprite.height, 60);
  for (const auto box : {layout.sprite, layout.identity, layout.level, layout.xp, layout.notice}) {
    EXPECT_TRUE(inside(box, width, height));
  }
  EXPECT_FALSE(overlaps(layout.sprite, layout.identity));
  EXPECT_FALSE(overlaps(layout.sprite, layout.level));
  EXPECT_FALSE(overlaps(layout.sprite, layout.xp));
  EXPECT_FALSE(overlaps(layout.sprite, layout.notice));
  EXPECT_FALSE(overlaps(layout.identity, layout.level));
  EXPECT_FALSE(overlaps(layout.identity, layout.xp));
  EXPECT_FALSE(overlaps(layout.identity, layout.notice));
  EXPECT_FALSE(overlaps(layout.level, layout.xp));
  EXPECT_FALSE(overlaps(layout.level, layout.notice));
  EXPECT_FALSE(overlaps(layout.xp, layout.notice));
}

}  // namespace

TEST(PokemonDashboardLayoutTest, PortraitBandDoesNotOverlap) { expectValid(480); }

TEST(PokemonDashboardLayoutTest, LandscapeBandDoesNotOverlap) { expectValid(800); }

TEST(PokemonDashboardLayoutTest, RejectsTooSmallBounds) {
  EXPECT_FALSE(pokemon::pokemonDashboardLayout(239, 68).valid);
  EXPECT_FALSE(pokemon::pokemonDashboardLayout(480, 59).valid);
}
