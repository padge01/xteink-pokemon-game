#pragma once

#include <FreeInkUICore.h>

namespace pokemon {

struct PokemonUiRect {
  int x = 0;
  int y = 0;
  int width = 0;
  int height = 0;
};

struct PokemonListPresentation {
  freeink::ui::StyleSet rowStyles{};
  int sidePadding = 16;
  int markerInset = 0;
};

int pokemonRowsPerPage(int screenHeight, int contentTop, int bottomReserve, int rowHeight, int maximumRows);
int pokemonPageStart(int selectedIndex, int rowsPerPage);
int pokemonCenteredOffset(int containerExtent, int contentExtent);
int pokemonRightAlignedX(int rightEdge, int contentWidth);
PokemonListPresentation pokemonListPresentation(bool hasArtwork);
PokemonUiRect pokemonPokedexCardBounds(int screenWidth, int screenHeight, int marginTop, int marginBottom,
                                       int footerHeight, bool landscape);
bool pokemonNeedsCleanRefresh(bool previousWasDetail, bool nextIsDetail, bool firstRender);

}  // namespace pokemon
