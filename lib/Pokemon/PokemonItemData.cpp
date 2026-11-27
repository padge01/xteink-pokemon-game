#if defined(CROSSINK_ENABLE_POKEMON)

#include "PokemonBattleTypes.h"
#include "PokemonItems.generated.h"

namespace pokemon {

// PokemonTypes.h pins its own copies of these bounds (POKEMON_BAG_SLOT_COUNT,
// the non-stone item count, and POKEMON_ITEM_ID_MAX) since it cannot include
// this battle layer without inverting the dependency direction; keep them
// in sync.
// +1 is PP Up (id 84, PP_UP_ITEM_ID); +BATTLE_BOOST_ITEM_COUNT is the 6
// battle-boost items (ids 85-90), and VITAMIN_ITEM_COUNT the 5 EV vitamins
// (ids 91-95) - all tracked outside bagCounts.
static_assert(POKEMON_BAG_SLOT_COUNT + 6U + 1U + BATTLE_BOOST_ITEM_COUNT + VITAMIN_ITEM_COUNT == ITEM_COUNT);
static_assert(POKEMON_ITEM_ID_MAX == ITEM_COUNT);
static_assert(POKEMON_BATTLE_BOOST_ITEM_COUNT == BATTLE_BOOST_ITEM_COUNT);
static_assert(POKEMON_VITAMIN_ITEM_COUNT == VITAMIN_ITEM_COUNT);

const ItemData* itemData(const uint8_t itemId) {
  if (itemId == 0 || itemId > ITEM_COUNT) return nullptr;
  return &generated::ITEMS[itemId - 1U];
}

}  // namespace pokemon

#endif
