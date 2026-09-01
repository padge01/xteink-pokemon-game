#include <PokemonUiLayout.h>
#include <gtest/gtest.h>

TEST(PokemonUiLayoutTest, UsesAvailableHeightInsteadOfADeviceSpecificPageSize) {
  EXPECT_EQ(pokemon::pokemonRowsPerPage(792, 72, 48, 64, 10), 10);
  EXPECT_EQ(pokemon::pokemonRowsPerPage(528, 72, 48, 64, 10), 6);
  EXPECT_EQ(pokemon::pokemonRowsPerPage(480, 80, 48, 64, 10), 5);
}

TEST(PokemonUiLayoutTest, KeepsAtLeastOneRowAndNeverExceedsFixedStorage) {
  EXPECT_EQ(pokemon::pokemonRowsPerPage(100, 80, 48, 64, 10), 1);
  EXPECT_EQ(pokemon::pokemonRowsPerPage(2000, 0, 0, 64, 10), 10);
}

TEST(PokemonUiLayoutTest, StartsPagesUsingTheCalculatedCapacity) {
  EXPECT_EQ(pokemon::pokemonPageStart(9, 10), 0);
  EXPECT_EQ(pokemon::pokemonPageStart(10, 10), 10);
  EXPECT_EQ(pokemon::pokemonPageStart(150, 10), 150);
  EXPECT_EQ(pokemon::pokemonPageStart(6, 6), 6);
}

TEST(PokemonUiLayoutTest, CentersNativeItemArtWithinItsRow) {
  EXPECT_EQ(pokemon::pokemonCenteredOffset(64, 32), 16);
  EXPECT_EQ(pokemon::pokemonCenteredOffset(80, 32), 24);
  EXPECT_EQ(pokemon::pokemonCenteredOffset(30, 40), 0);
}

TEST(PokemonUiLayoutTest, UsesTheSameWhitePaperCursorForTextAndArtworkLists) {
  for (const bool hasArtwork : {false, true}) {
    const auto presentation = pokemon::pokemonListPresentation(hasArtwork);
    for (const auto& style : {presentation.rowStyles.normal, presentation.rowStyles.selected,
                              presentation.rowStyles.focused, presentation.rowStyles.active}) {
      EXPECT_EQ(style.background.kind, freeink::ui::PaintKind::Solid);
      EXPECT_EQ(style.background.color, freeink::ui::Color::White);
      EXPECT_EQ(style.foreground.kind, freeink::ui::PaintKind::Solid);
      EXPECT_EQ(style.foreground.color, freeink::ui::Color::Black);
    }
  }

  const auto textOnly = pokemon::pokemonListPresentation(false);
  EXPECT_EQ(textOnly.sidePadding, 16);
  EXPECT_EQ(textOnly.markerInset, 0);

  const auto artwork = pokemon::pokemonListPresentation(true);
  EXPECT_EQ(artwork.sidePadding, 104);
  EXPECT_EQ(artwork.markerInset, 86);
}

TEST(PokemonUiLayoutTest, RightAlignsSummaryValuesToTheContentEdge) {
  EXPECT_EQ(pokemon::pokemonRightAlignedX(500, 42), 458);
  EXPECT_EQ(pokemon::pokemonRightAlignedX(500, 0), 500);
}

TEST(PokemonUiLayoutTest, CentersNativePokedexCardsWithoutUpscaling) {
  const auto portrait = pokemon::pokemonPokedexCardBounds(528, 792, 0, 0, 40, false);
  EXPECT_EQ(portrait.x, 28);
  EXPECT_EQ(portrait.y, 22);
  EXPECT_EQ(portrait.width, 472);
  EXPECT_EQ(portrait.height, 708);

  const auto landscape = pokemon::pokemonPokedexCardBounds(792, 528, 0, 0, 40, true);
  EXPECT_EQ(landscape.x, 252);
  EXPECT_EQ(landscape.y, 28);
  EXPECT_EQ(landscape.width, 288);
  EXPECT_EQ(landscape.height, 432);
}

TEST(PokemonUiLayoutTest, UsesCleanRefreshOnlyAtActivityAndDetailBoundaries) {
  EXPECT_TRUE(pokemon::pokemonNeedsCleanRefresh(false, false, true));
  EXPECT_TRUE(pokemon::pokemonNeedsCleanRefresh(false, true, false));
  EXPECT_TRUE(pokemon::pokemonNeedsCleanRefresh(true, false, false));
  EXPECT_FALSE(pokemon::pokemonNeedsCleanRefresh(false, false, false));
  EXPECT_FALSE(pokemon::pokemonNeedsCleanRefresh(true, true, false));
}
