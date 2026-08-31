#include <gtest/gtest.h>

#include "components/pokemon/PokemonArtPath.h"

TEST(PokemonArtPath, BuildsApprovedKantoIconAndHeroPaths) {
  char path[64]{};
  ASSERT_NE(pokemon::pokemonSpeciesArtPath(1, false, path, sizeof(path)), nullptr);
  EXPECT_STREQ(path, "/.crosspoint/pokemon/sprites/001.bmp");
  ASSERT_NE(pokemon::pokemonSpeciesArtPath(151, true, path, sizeof(path)), nullptr);
  EXPECT_STREQ(path, "/.crosspoint/pokemon/heroes/151.bmp");
}

TEST(PokemonArtPath, RejectsSpeciesOutsideTheOriginal151) {
  char path[64] = "unchanged";
  EXPECT_EQ(pokemon::pokemonSpeciesArtPath(0, false, path, sizeof(path)), nullptr);
  EXPECT_STREQ(path, "");
  EXPECT_EQ(pokemon::pokemonSpeciesArtPath(172, false, path, sizeof(path)), nullptr);
  EXPECT_STREQ(path, "");
}

TEST(PokemonArtPath, BuildsExistingSleepCoverPokedexPaths) {
  char path[64]{};
  ASSERT_NE(pokemon::pokemonPokedexArtPath(1, "Bulbasaur", path, sizeof(path)), nullptr);
  EXPECT_STREQ(path, "/sleep/001-bulbasaur.bmp");
  ASSERT_NE(pokemon::pokemonPokedexArtPath(29, "Nidoran♀", path, sizeof(path)), nullptr);
  EXPECT_STREQ(path, "/sleep/029-nidoran-f.bmp");
  ASSERT_NE(pokemon::pokemonPokedexArtPath(83, "Farfetch'd", path, sizeof(path)), nullptr);
  EXPECT_STREQ(path, "/sleep/083-farfetchd.bmp");
  ASSERT_NE(pokemon::pokemonPokedexArtPath(122, "Mr. Mime", path, sizeof(path)), nullptr);
  EXPECT_STREQ(path, "/sleep/122-mr-mime.bmp");
  ASSERT_NE(pokemon::pokemonPokedexArtPath(151, "Mew", path, sizeof(path)), nullptr);
  EXPECT_STREQ(path, "/sleep/151-mew.bmp");
}

TEST(PokemonArtPath, BuildsStonePathsButLeavesLinkCableBlank) {
  char path[80]{};
  ASSERT_NE(pokemon::pokemonItemArtPath(pokemon::EvolutionItem::ThunderStone, false, path, sizeof(path)), nullptr);
  EXPECT_STREQ(path, "/.crosspoint/pokemon/items/thunder-stone.bmp");
  ASSERT_NE(pokemon::pokemonItemArtPath(pokemon::EvolutionItem::MoonStone, true, path, sizeof(path)), nullptr);
  EXPECT_STREQ(path, "/.crosspoint/pokemon/heroes/items/moon-stone.bmp");
  EXPECT_EQ(pokemon::pokemonItemArtPath(pokemon::EvolutionItem::LinkCable, false, path, sizeof(path)), nullptr);
  EXPECT_STREQ(path, "");
}
