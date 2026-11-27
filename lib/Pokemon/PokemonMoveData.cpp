#if defined(CROSSINK_ENABLE_POKEMON)

#include "PokemonBattleTypes.h"
#include "PokemonMoves.generated.h"

namespace pokemon {

// PokemonTypes.h pins its own copy of this bound (POKEMON_MOVE_ID_MAX) since
// it cannot include this battle layer without inverting the dependency
// direction; keep the two in sync.
static_assert(POKEMON_MOVE_ID_MAX == MOVE_COUNT);

const MoveData* moveData(const uint8_t moveId) {
  if (moveId == 0 || moveId > MOVE_COUNT) return nullptr;
  return &generated::MOVES[moveId - 1U];
}

}  // namespace pokemon

#endif
