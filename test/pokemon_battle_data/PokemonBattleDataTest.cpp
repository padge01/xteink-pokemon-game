#include <cstdint>
#include <cstdio>
#include <string_view>

#include "Pokemon/PokemonBattleTypes.h"
#include "Pokemon/PokemonSpecies.h"

namespace {

using pokemon::Ailment;
using pokemon::ItemCategory;
using pokemon::MoveCategory;
using pokemon::MoveData;

int failures = 0;

#define CHECK(condition)                                                                \
  do {                                                                                  \
    if (!(condition)) {                                                                 \
      std::fprintf(stderr, "%s:%d check failed: %s\n", __FILE__, __LINE__, #condition); \
      ++failures;                                                                       \
    }                                                                                   \
  } while (false)

void movesCoverTheGenerationIRangeWithSaneFields() {
  CHECK(pokemon::moveData(0) == nullptr);
  CHECK(pokemon::moveData(pokemon::MOVE_COUNT + 1) == nullptr);
  for (uint8_t moveId = 1; moveId <= pokemon::MOVE_COUNT; ++moveId) {
    const MoveData* move = pokemon::moveData(moveId);
    CHECK(move != nullptr);
    if (move == nullptr) continue;
    CHECK(move->moveId == moveId);
    CHECK(move->name != nullptr && move->name[0] != '\0');
    CHECK(move->accuracy <= 100);
    CHECK(move->pp <= 40);
    CHECK(move->ailmentChance <= 100);
  }
  // Tackle: physical Normal move. Multi-ailment moves (e.g. Tri Attack, which
  // randomly freezes/burns/paralyzes) surface as Ailment::None here because
  // this model only tracks a single status per move; that's an accepted
  // Generation I-lite simplification, not a data bug.
  const MoveData* tackle = pokemon::moveData(33);
  CHECK(tackle != nullptr);
  if (tackle != nullptr) {
    CHECK(tackle->category == MoveCategory::Physical);
    CHECK(tackle->power == 40);
  }
}

void baseStatsCoverAllKantoSpeciesWithPositiveValues() {
  CHECK(pokemon::baseStatsFor(0) == nullptr);
  CHECK(pokemon::baseStatsFor(pokemon::KANTO_SPECIES_COUNT + 1) == nullptr);
  for (uint16_t speciesId = 1; speciesId <= pokemon::KANTO_SPECIES_COUNT; ++speciesId) {
    const pokemon::BaseStats* stats = pokemon::baseStatsFor(speciesId);
    CHECK(stats != nullptr);
    if (stats == nullptr) continue;
    CHECK(stats->hp > 0);
    CHECK(stats->attack > 0);
    CHECK(stats->defense > 0);
    CHECK(stats->special > 0);
    CHECK(stats->speed > 0);
  }
  // Chansey carries the highest base HP in the original 151 (250).
  const pokemon::BaseStats* chansey = pokemon::baseStatsFor(113);
  CHECK(chansey != nullptr);
  if (chansey != nullptr) CHECK(chansey->hp == 250);
}

void learnsetsAreOrderedByLevelAndSpeciesTotalMatchesTheSourceRowCount() {
  CHECK(pokemon::learnsetFor(0).empty());
  CHECK(pokemon::learnsetFor(pokemon::KANTO_SPECIES_COUNT + 1).empty());

  // Bulbasaur's Red/Blue learnset starts 1:Growl(45)/1:Tackle(33) style pairs;
  // the pinned CSV lists Tackle(33) then Growl(45) at level 1.
  const std::span<const pokemon::LearnsetEntry> bulbasaur = pokemon::learnsetFor(1);
  CHECK(bulbasaur.size() == 9);
  if (bulbasaur.size() >= 2) {
    CHECK(bulbasaur[0].level == 1 && bulbasaur[0].moveId == 33);
    CHECK(bulbasaur[1].level == 1 && bulbasaur[1].moveId == 45);
  }

  size_t total = 0;
  uint8_t previousLevel = 0;
  for (uint16_t speciesId = 1; speciesId <= pokemon::KANTO_SPECIES_COUNT; ++speciesId) {
    const std::span<const pokemon::LearnsetEntry> entries = pokemon::learnsetFor(speciesId);
    total += entries.size();
    previousLevel = 0;
    for (const pokemon::LearnsetEntry& entry : entries) {
      CHECK(entry.level >= previousLevel);
      CHECK(entry.moveId >= 1 && entry.moveId <= pokemon::MOVE_COUNT);
      previousLevel = entry.level;
    }
  }
  CHECK(total == 989);
}

void machineCompatibilityMatchesTheSourceRowCount() {
  CHECK(!pokemon::canLearnViaMachine(0, 1));
  CHECK(!pokemon::canLearnViaMachine(1, 0));
  CHECK(pokemon::canLearnViaMachine(1, 14));
  CHECK(!pokemon::canLearnViaMachine(1, 33));  // Tackle is a level-up move, not a TM/HM for Bulbasaur

  size_t total = 0;
  for (uint16_t speciesId = 1; speciesId <= pokemon::KANTO_SPECIES_COUNT; ++speciesId) {
    for (uint8_t moveId = 1; moveId <= pokemon::MOVE_COUNT; ++moveId) {
      if (pokemon::canLearnViaMachine(speciesId, moveId)) ++total;
    }
  }
  CHECK(total == 3037);
}

void itemsPinTheFirstSixIdsToTheExistingEvolutionItemOrder() {
  CHECK(pokemon::itemData(0) == nullptr);
  CHECK(pokemon::itemData(pokemon::ITEM_COUNT + 1) == nullptr);

  struct {
    uint8_t id;
    const char* name;
  } stones[] = {
      {1, "Moon Stone"},  {2, "Fire Stone"}, {3, "Thunder Stone"},
      {4, "Water Stone"}, {5, "Leaf Stone"}, {6, "Link Cable"},
  };
  for (const auto& stone : stones) {
    const pokemon::ItemData* item = pokemon::itemData(stone.id);
    CHECK(item != nullptr);
    if (item == nullptr) continue;
    CHECK(item->category == ItemCategory::Stone);
    CHECK(std::string_view(item->name) == stone.name);
  }

  const pokemon::ItemData* pokeBall = pokemon::itemData(7);
  CHECK(pokeBall != nullptr);
  if (pokeBall != nullptr) {
    CHECK(pokeBall->category == ItemCategory::Ball);
    CHECK(std::string_view(pokeBall->name) == "Poke Ball");
    CHECK(pokeBall->effectValue == 10);
  }

  // The starting items are named by id in PokemonTypes.h, which cannot see this table.
  const pokemon::ItemData* startingBall = pokemon::itemData(pokemon::POKEMON_ITEM_POKE_BALL);
  const pokemon::ItemData* startingPotion = pokemon::itemData(pokemon::POKEMON_ITEM_POTION);
  CHECK(startingBall != nullptr && std::string_view(startingBall->name) == "Poke Ball");
  CHECK(startingPotion != nullptr && std::string_view(startingPotion->name) == "Potion");

  const pokemon::ItemData* masterBall = pokemon::itemData(10);
  CHECK(masterBall != nullptr);
  if (masterBall != nullptr) CHECK(masterBall->effectValue == 255);

  // TM/HM section: ids 29-78 are TM01-50, 79-83 are HM01-05; every one must
  // name a real move and carry the Machine category.
  for (uint8_t itemId = 29; itemId <= 83; ++itemId) {
    const pokemon::ItemData* machine = pokemon::itemData(itemId);
    CHECK(machine != nullptr);
    if (machine == nullptr) continue;
    CHECK(machine->category == ItemCategory::Machine);
    CHECK(machine->teachesMoveId >= 1 && machine->teachesMoveId <= pokemon::MOVE_COUNT);
    CHECK(pokemon::moveData(machine->teachesMoveId) != nullptr);
  }
  const pokemon::ItemData* hm01 = pokemon::itemData(79);
  CHECK(hm01 != nullptr);
  if (hm01 != nullptr) CHECK(std::string_view(hm01->name) == "HM01");
}

void gymsAreOrderedAndEliteFourCarriesNoBadge() {
  CHECK(pokemon::gymData(0) == nullptr);
  CHECK(pokemon::gymData(pokemon::GYM_COUNT + 1) == nullptr);

  const pokemon::GymData* brock = pokemon::gymData(1);
  CHECK(brock != nullptr);
  if (brock != nullptr) {
    CHECK(std::string_view(brock->leaderName) == "Brock");
    CHECK(std::string_view(brock->badgeName) == "Boulder Badge");
  }
  const std::span<const pokemon::GymTeamMember> brockTeam = pokemon::gymTeamFor(1);
  CHECK(brockTeam.size() == 2);
  if (brockTeam.size() == 2) {
    CHECK(brockTeam[0].speciesId == 74 && brockTeam[0].level == 12);
    CHECK(brockTeam[0].moves[0] == 33 && brockTeam[0].moves[1] == 111);  // Geodude: Tackle, Defense Curl
    CHECK(brockTeam[1].speciesId == 95 && brockTeam[1].level == 14);
    CHECK(brockTeam[1].moves[0] == 33 && brockTeam[1].moves[1] == 103 &&
          brockTeam[1].moves[2] == 117);  // Onix: Tackle, Screech, Bide
  }

  for (uint8_t gymIndex = 1; gymIndex <= 8; ++gymIndex) {
    const pokemon::GymData* gym = pokemon::gymData(gymIndex);
    CHECK(gym != nullptr);
    if (gym != nullptr) CHECK(gym->badgeName[0] != '\0');
  }
  for (uint8_t gymIndex = 9; gymIndex <= pokemon::GYM_COUNT; ++gymIndex) {
    const pokemon::GymData* eliteFour = pokemon::gymData(gymIndex);
    CHECK(eliteFour != nullptr);
    if (eliteFour != nullptr) CHECK(eliteFour->badgeName[0] == '\0');
  }

  // Stage 14: full real Pokemon Red teams restored (species duplicates and
  // all), not the earlier 2-3-member e-ink-refresh trim.
  CHECK(pokemon::gymTeamFor(8).size() == 5);   // Giovanni
  CHECK(pokemon::gymTeamFor(9).size() == 5);   // Lorelei
  CHECK(pokemon::gymTeamFor(10).size() == 5);  // Bruno
  CHECK(pokemon::gymTeamFor(11).size() == 5);  // Agatha
  CHECK(pokemon::gymTeamFor(12).size() == 5);  // Lance

  // The Champion (Blue) - added after the Elite Four, always the last entry.
  CHECK(pokemon::CHAMPION_GYM_INDEX == pokemon::GYM_COUNT);
  const pokemon::GymData* champion = pokemon::gymData(pokemon::CHAMPION_GYM_INDEX);
  CHECK(champion != nullptr);
  if (champion != nullptr) {
    CHECK(std::string_view(champion->leaderName) == "Blue");
    CHECK(champion->badgeName[0] == '\0');  // no badge for the Champion, same as Elite Four
  }
  const std::span<const pokemon::GymTeamMember> championTeam = pokemon::gymTeamFor(pokemon::CHAMPION_GYM_INDEX);
  CHECK(championTeam.size() == 6);
  if (championTeam.size() == 6) {
    CHECK(championTeam[0].speciesId == 18 && championTeam[0].level == 61);   // Pidgeot
    CHECK(championTeam[4].speciesId == 130 && championTeam[4].level == 63);  // Gyarados
  }

  // championFinalSlotFor(): the real games send out the evolution that
  // counters the player's own starter for the Champion's final slot.
  CHECK(pokemon::championFinalSlotFor(1).speciesId == 6);   // Bulbasaur -> Charizard
  CHECK(pokemon::championFinalSlotFor(2).speciesId == 6);   // Ivysaur -> Charizard
  CHECK(pokemon::championFinalSlotFor(3).speciesId == 6);   // Venusaur -> Charizard
  CHECK(pokemon::championFinalSlotFor(4).speciesId == 9);   // Charmander -> Blastoise
  CHECK(pokemon::championFinalSlotFor(5).speciesId == 9);   // Charmeleon -> Blastoise
  CHECK(pokemon::championFinalSlotFor(6).speciesId == 9);   // Charizard -> Blastoise
  CHECK(pokemon::championFinalSlotFor(7).speciesId == 3);   // Squirtle -> Venusaur
  CHECK(pokemon::championFinalSlotFor(8).speciesId == 3);   // Wartortle -> Venusaur
  CHECK(pokemon::championFinalSlotFor(9).speciesId == 3);   // Blastoise -> Venusaur
  CHECK(pokemon::championFinalSlotFor(25).speciesId == 6);  // Pikachu (no rival response) -> falls back Charizard
  for (uint8_t starter : {static_cast<uint8_t>(1), static_cast<uint8_t>(4), static_cast<uint8_t>(7)}) {
    const pokemon::GymTeamMember slot = pokemon::championFinalSlotFor(starter);
    CHECK(slot.level == 65);
    for (const uint8_t moveId : slot.moves) CHECK(moveId >= 1 && moveId <= pokemon::MOVE_COUNT);
  }

  for (uint8_t gymIndex = 1; gymIndex <= pokemon::GYM_COUNT; ++gymIndex) {
    const std::span<const pokemon::GymTeamMember> team = pokemon::gymTeamFor(gymIndex);
    CHECK(team.size() >= 1 && team.size() <= pokemon::MAX_GYM_TEAM_SIZE);
    for (const pokemon::GymTeamMember& member : team) {
      CHECK(member.speciesId >= 1 && member.speciesId <= pokemon::KANTO_SPECIES_COUNT);
      CHECK(member.level >= 1 && member.level <= 100);
      // Every trainer Pokemon has a real, fixed moveset (Stage 12) - never
      // derived from the learnset-by-level table like a wild encounter.
      CHECK(member.moves[0] != 0);
      for (const uint8_t moveId : member.moves) {
        CHECK(moveId <= pokemon::MOVE_COUNT);
      }
    }
  }
}

}  // namespace

int main() {
  movesCoverTheGenerationIRangeWithSaneFields();
  baseStatsCoverAllKantoSpeciesWithPositiveValues();
  learnsetsAreOrderedByLevelAndSpeciesTotalMatchesTheSourceRowCount();
  machineCompatibilityMatchesTheSourceRowCount();
  itemsPinTheFirstSixIdsToTheExistingEvolutionItemOrder();
  gymsAreOrderedAndEliteFourCarriesNoBadge();
  return failures == 0 ? 0 : 1;
}
