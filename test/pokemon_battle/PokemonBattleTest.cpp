#include <algorithm>
#include <cstdio>

#include "Pokemon/PokemonBattle.h"
#include "Pokemon/PokemonSpecies.h"

namespace {

using pokemon::Ailment;
using pokemon::BallKind;
using pokemon::BattleCombatant;
using pokemon::BattleLogEvent;
using pokemon::BattleMoveSlot;
using pokemon::BattleOutcome;
using pokemon::MoveCategory;
using pokemon::PokemonType;
using pokemon::RandomSource;
using pokemon::StatKind;

int failures = 0;

#define CHECK(condition)                                                                \
  do {                                                                                  \
    if (!(condition)) {                                                                 \
      std::fprintf(stderr, "%s:%d check failed: %s\n", __FILE__, __LINE__, #condition); \
      ++failures;                                                                       \
    }                                                                                   \
  } while (false)

uint32_t alwaysZero(void*, const uint32_t) { return 0; }
uint32_t alwaysMax(void*, const uint32_t upperExclusive) { return upperExclusive == 0 ? 0 : upperExclusive - 1U; }
uint32_t fixedRoll(void* context, const uint32_t upperExclusive) {
  const uint32_t value = *static_cast<const uint32_t*>(context);
  return upperExclusive == 0 ? 0 : std::min(value, upperExclusive - 1U);
}

const RandomSource ZERO_RANDOM{nullptr, alwaysZero};
const RandomSource MAX_RANDOM{nullptr, alwaysMax};

// Species ids: 1 Bulbasaur (Grass/Poison), 4 Charmander (Fire), 7 Squirtle (Water).
// Move ids: 33 Tackle (Normal/Physical/40pow/100acc), 55 Water Gun
// (Water/Special/40pow/100acc), 52 Ember (Fire/Special/40pow/100acc/10% Burn),
// 77 Poison Powder (Poison/Status/0pow/75acc/Poison, ailment_chance=0 meaning
// "always" per the PokeAPI convention this engine special-cases).

BattleCombatant makeCombatant(const uint16_t speciesId, const uint8_t level,
                              const std::initializer_list<uint8_t> moveIds) {
  BattleCombatant combatant{};
  combatant.speciesId = speciesId;
  combatant.level = level;
  const pokemon::BaseStats* stats = pokemon::baseStatsFor(speciesId);
  combatant.maxHp = stats == nullptr ? 1 : pokemon::battleMaxHp(stats->hp, level);
  combatant.currentHp = combatant.maxHp;
  uint8_t slot = 0;
  for (const uint8_t moveId : moveIds) {
    if (slot >= pokemon::BATTLE_MOVE_SLOTS) break;
    const pokemon::MoveData* move = pokemon::moveData(moveId);
    combatant.moves[slot] = BattleMoveSlot{moveId, move == nullptr ? static_cast<uint8_t>(0) : move->pp};
    ++slot;
  }
  return combatant;
}

void statFormulasScaleWithLevel() {
  CHECK(pokemon::battleMaxHp(45, 5) > 10);  // at least the level/base floor terms
  CHECK(pokemon::battleMaxHp(45, 50) > pokemon::battleMaxHp(45, 5));
  CHECK(pokemon::battleWorkingStat(49, 50) > pokemon::battleWorkingStat(49, 5));
}

void damagingMoveReducesDefenderHpAndReportsSuperEffective() {
  // Squirtle (Water) uses Water Gun on Charmander (Fire): 200% effectiveness.
  // Give Squirtle a much higher level so it's guaranteed to act first
  // regardless of base Speed (same technique the next test uses) - ZERO_RANDOM
  // now also guarantees a critical hit on every attack (see rollCriticalHit:
  // a roll of 0 is always below any positive threshold), so without this,
  // Charmander's own move could go first and faint the low-level Squirtle
  // before Water Gun ever gets a turn.
  BattleCombatant squirtle = makeCombatant(7, 30, {55});
  BattleCombatant charmander = makeCombatant(4, 5, {33});
  const uint16_t hpBefore = charmander.currentHp;

  const pokemon::BattleTurnResult result = pokemon::stepBattle(squirtle, charmander, 0, ZERO_RANDOM);
  CHECK(result.player.event == BattleLogEvent::MoveSuperEffective);
  CHECK(charmander.currentHp < hpBefore);
}

void fireMoveIsNotVeryEffectiveAgainstWaterAndCanBurn() {
  // Charmander (Fire, Ember) attacking Squirtle (Water): 50% effectiveness.
  // Use MAX_RANDOM so the 10% secondary burn chance does not fire, keeping
  // the assertion purely about type effectiveness.
  BattleCombatant charmander = makeCombatant(4, 20, {52});
  BattleCombatant squirtle = makeCombatant(7, 5, {33});  // slower dummy target
  // Force Charmander to act first regardless of speed by giving it a much
  // higher level; the assertion only cares about the Ember side's event.
  const pokemon::BattleTurnResult result = pokemon::stepBattle(charmander, squirtle, 0, MAX_RANDOM);
  CHECK(result.player.event == BattleLogEvent::MoveNotVeryEffective);
  CHECK(squirtle.status == Ailment::None);
}

void statusMoveWithZeroAilmentChanceAlwaysAppliesItsAilment() {
  // Poison Powder has ailment_chance == 0 in the source data, which this
  // engine must treat as "always poisons on hit" per the PokeAPI convention
  // (see the comment in PokemonBattle.cpp), not "never poisons."
  BattleCombatant bulbasaur = makeCombatant(1, 30, {77});  // fast enough to act first
  BattleCombatant charmander = makeCombatant(4, 5, {33});
  const pokemon::BattleTurnResult result = pokemon::stepBattle(bulbasaur, charmander, 0, ZERO_RANDOM);
  CHECK(result.player.event == BattleLogEvent::InflictedStatus);
  CHECK(charmander.status == Ailment::Poison);
}

void paralysisCanPreventAnActionAndSpeedIsHalved() {
  BattleCombatant player = makeCombatant(4, 50, {33});
  player.status = Ailment::Paralysis;
  BattleCombatant opponent = makeCombatant(7, 5, {33});
  // ZERO_RANDOM rolls 0, which is < PARALYSIS_FAIL_CHANCE_PERCENT (25), so
  // the paralyzed side must fail to act this turn.
  const pokemon::BattleTurnResult result = pokemon::stepBattle(player, opponent, 0, ZERO_RANDOM);
  CHECK(result.player.event == BattleLogEvent::StatusPreventedMove);
  CHECK(opponent.currentHp == opponent.maxHp);  // player never got to attack
  CHECK(player.currentHp < player.maxHp);       // but the non-paralyzed opponent still got its hit in
}

void poisonAndBurnTickAtEndOfTurn() {
  BattleCombatant player = makeCombatant(4, 50, {45});  // Growl: 0-power status move, no side effect on target
  player.status = Ailment::Poison;
  BattleCombatant opponent = makeCombatant(7, 50, {45});
  const uint16_t hpBefore = player.currentHp;
  pokemon::stepBattle(player, opponent, 0, MAX_RANDOM);
  CHECK(player.currentHp < hpBefore);
}

void aiVariesItsMoveChoiceAcrossRandomSeedsInsteadOfAlwaysTheSameSlot() {
  // Stage 12: a Gym Leader's Pokemon with several equally-effective moves
  // (Tackle and Scratch are both Normal/40 power - identical effectiveness
  // against a Fire-type target) must not always throw the same one turn
  // after turn just because it happens to sit in the lowest slot index.
  bool sawTackle = false;
  bool sawScratch = false;
  for (uint32_t seed = 0; seed < 12 && !(sawTackle && sawScratch); ++seed) {
    BattleCombatant player = makeCombatant(4, 50, {45});        // Charmander, Growl: harmless filler, never KOs
    BattleCombatant opponent = makeCombatant(4, 50, {33, 10});  // Tackle + Scratch, tied effectiveness
    const RandomSource seeded{&seed, fixedRoll};
    pokemon::stepBattle(player, opponent, 0, seeded);
    if (opponent.moves[0].currentPp < pokemon::moveData(33)->pp) sawTackle = true;
    if (opponent.moves[1].currentPp < pokemon::moveData(10)->pp) sawScratch = true;
  }
  CHECK(sawTackle);
  CHECK(sawScratch);
}

void faintingEndsTheBattleImmediatelyWithoutARetaliation() {
  BattleCombatant strong = makeCombatant(1, 100, {33});
  BattleCombatant weak = makeCombatant(4, 2, {33});
  weak.currentHp = 1;
  weak.maxHp = 1;
  const pokemon::BattleTurnResult result = pokemon::stepBattle(strong, weak, 0, ZERO_RANDOM);
  CHECK(weak.currentHp == 0);
  CHECK(result.outcome == BattleOutcome::PlayerWon);
  CHECK(result.opponent.event == BattleLogEvent::Fainted);
}

void alreadyFaintedCombatantsShortCircuitToAnOutcome() {
  BattleCombatant player = makeCombatant(1, 10, {33});
  BattleCombatant opponent = makeCombatant(4, 10, {33});
  player.currentHp = 0;
  const pokemon::BattleTurnResult result = pokemon::stepBattle(player, opponent, 0, ZERO_RANDOM);
  CHECK(result.outcome == BattleOutcome::OpponentWon);
  CHECK(!result.player.acted);
  CHECK(!result.opponent.acted);
}

void opponentOnlyTurnActsRegardlessOfSpeedSinceThePlayerAlreadySpentTheTurn() {
  // Switching or using an item mid-battle costs the whole turn (Gen 1), so
  // stepOpponentOnlyTurn() never compares Speed - it's called only for the
  // opponent's own action, full stop. Charmander (4, faster) as "player"
  // proves this: if Speed were compared, the faster side would go first and
  // nothing would touch it, but here only the opponent's Squirtle acts.
  BattleCombatant charmander = makeCombatant(4, 20, {33});
  BattleCombatant squirtle = makeCombatant(7, 5, {55});
  const uint16_t playerHpBefore = charmander.currentHp;
  const pokemon::BattleTurnResult result = pokemon::stepOpponentOnlyTurn(charmander, squirtle, ZERO_RANDOM);
  CHECK(result.outcome == BattleOutcome::InProgress);
  CHECK(!result.player.acted);
  CHECK(result.opponent.acted);
  CHECK(charmander.currentHp < playerHpBefore);
  CHECK(squirtle.currentHp == squirtle.maxHp);  // player's side never acted, so it's untouched
}

void opponentOnlyTurnCanFaintThePlayer() {
  BattleCombatant player = makeCombatant(1, 5, {33});
  BattleCombatant opponent = makeCombatant(4, 60, {52});  // Ember, way overleveled
  const pokemon::BattleTurnResult result = pokemon::stepOpponentOnlyTurn(player, opponent, ZERO_RANDOM);
  CHECK(result.outcome == BattleOutcome::OpponentWon);
  CHECK(result.player.event == BattleLogEvent::Fainted);
}

void opponentOnlyTurnStillAppliesEndOfTurnStatusDamage() {
  BattleCombatant player = makeCombatant(1, 20, {33});
  BattleCombatant opponent = makeCombatant(4, 20, {33});
  opponent.status = Ailment::Poison;
  const uint16_t opponentHpBefore = opponent.currentHp;
  const pokemon::BattleTurnResult result = pokemon::stepOpponentOnlyTurn(player, opponent, ZERO_RANDOM);
  CHECK(result.outcome == BattleOutcome::InProgress);
  CHECK(opponent.currentHp < opponentHpBefore);  // poison ticked even though only the opponent acted this turn
}

void opponentOnlyTurnShortCircuitsWhenAlreadyFainted() {
  BattleCombatant player = makeCombatant(1, 10, {33});
  BattleCombatant opponent = makeCombatant(4, 10, {33});
  player.currentHp = 0;
  const pokemon::BattleTurnResult result = pokemon::stepOpponentOnlyTurn(player, opponent, ZERO_RANDOM);
  CHECK(result.outcome == BattleOutcome::OpponentWon);
  CHECK(!result.player.acted);
  CHECK(!result.opponent.acted);
}

void playerOnlyTurnActsRegardlessOfSpeedSinceTheOpponentAlreadySpentTheTurn() {
  // Mirror image of opponentOnlyTurnActsRegardlessOfSpeedSinceThePlayerAlreadySpentTheTurn():
  // a trainer AI healing or switching mid-battle costs its whole turn, so
  // stepPlayerOnlyTurn() never compares Speed either - only the player's
  // own chosen move resolves. Squirtle (7, slower) as "opponent" proves
  // this: if Speed were compared, the faster Charmander "player" would
  // still go first anyway here, so use the slower side as player instead -
  // Squirtle (7, faster than Bulbasaur) as opponent never acts even though
  // it would normally win a Speed comparison against Bulbasaur.
  BattleCombatant bulbasaur = makeCombatant(1, 20, {33});  // Tackle
  BattleCombatant squirtle = makeCombatant(7, 50, {55});
  const uint16_t opponentHpBefore = squirtle.currentHp;
  const pokemon::BattleTurnResult result = pokemon::stepPlayerOnlyTurn(bulbasaur, squirtle, 0, ZERO_RANDOM);
  CHECK(result.outcome == BattleOutcome::InProgress);
  CHECK(result.player.acted);
  CHECK(!result.opponent.acted);
  CHECK(squirtle.currentHp < opponentHpBefore);
  CHECK(bulbasaur.currentHp == bulbasaur.maxHp);  // opponent's side never acted, so it's untouched
}

void playerOnlyTurnCanFaintTheOpponent() {
  BattleCombatant player = makeCombatant(4, 60, {52});  // Ember, way overleveled
  BattleCombatant opponent = makeCombatant(1, 5, {33});
  const pokemon::BattleTurnResult result = pokemon::stepPlayerOnlyTurn(player, opponent, 0, ZERO_RANDOM);
  CHECK(result.outcome == BattleOutcome::PlayerWon);
  CHECK(result.opponent.event == BattleLogEvent::Fainted);
}

void playerOnlyTurnStillAppliesEndOfTurnStatusDamage() {
  BattleCombatant player = makeCombatant(1, 20, {33});
  player.status = Ailment::Poison;
  BattleCombatant opponent = makeCombatant(4, 20, {33});
  const uint16_t playerHpBefore = player.currentHp;
  const pokemon::BattleTurnResult result = pokemon::stepPlayerOnlyTurn(player, opponent, 0, ZERO_RANDOM);
  CHECK(result.outcome == BattleOutcome::InProgress);
  CHECK(player.currentHp < playerHpBefore);
}

void playerOnlyTurnShortCircuitsWhenAlreadyFainted() {
  BattleCombatant player = makeCombatant(1, 10, {33});
  BattleCombatant opponent = makeCombatant(4, 10, {33});
  opponent.currentHp = 0;
  const pokemon::BattleTurnResult result = pokemon::stepPlayerOnlyTurn(player, opponent, 0, ZERO_RANDOM);
  CHECK(result.outcome == BattleOutcome::PlayerWon);
  CHECK(!result.player.acted);
  CHECK(!result.opponent.acted);
}

void masterBallAlwaysCatchesRegardlessOfRandomness() {
  BattleCombatant wild = makeCombatant(1, 50, {});
  CHECK(pokemon::attemptCatch(wild, BallKind::Master, MAX_RANDOM));
}

void pokeBallCatchOddsScaleWithHpAndCaptureRate() {
  // Real Gen 1's two-roll algorithm (see attemptCatch()'s doc comment):
  // ZERO_RANDOM rolls 0 for both R1 and R2 - R1=0 never exceeds Bulbasaur's
  // own captureRate (45), and R2=0 never exceeds the HP factor (capped at
  // 255 for a full-HP target with the deterministic ZERO_RANDOM species/
  // level here) - so it always catches, same as every other ZERO_RANDOM
  // "best case" convention this test suite already relies on elsewhere.
  // MAX_RANDOM's R1 (255 for a Poke Ball's 0-255 range) exceeds 45
  // immediately - an instant breakout before R2 is even relevant.
  BattleCombatant fullHp = makeCombatant(1, 20, {});
  CHECK(pokemon::attemptCatch(fullHp, BallKind::Poke, ZERO_RANDOM));
  CHECK(!pokemon::attemptCatch(fullHp, BallKind::Poke, MAX_RANDOM));

  // A fixed roll of 50 (used for both R1 and R2, since fixedRoll always
  // returns the same context value): against the healthy, unstatused
  // target, R*=50 already exceeds Bulbasaur's captureRate (45) - an
  // instant breakout, regardless of R2. Against a badly wounded (HP/10),
  // paralyzed target, the same R1=50 minus Paralysis's status bonus (12)
  // gives R*=38, which clears the captureRate check - and the wounded
  // target's much smaller current-HP quarter pushes its HP factor to the
  // 255 cap, so R2=50 comfortably catches. Demonstrates the intended
  // "easier to catch when hurt/statused" effect end to end.
  uint32_t roll = 50;
  const RandomSource midRandom{&roll, fixedRoll};
  CHECK(!pokemon::attemptCatch(fullHp, BallKind::Poke, midRandom));

  BattleCombatant wounded = makeCombatant(1, 20, {});
  wounded.currentHp = wounded.maxHp / 10;
  wounded.status = Ailment::Paralysis;
  CHECK(pokemon::attemptCatch(wounded, BallKind::Poke, midRandom));
}

void criticalHitExactlyDoublesDamageForAHighCritRatioMove() {
  // Bulbasaur (base Speed 45) uses Slash (move 163, a real Gen 1 high-crit
  // move: threshold min(45*8,511)=360) on Charmander. A fixedRoll context of
  // 15 saturates the 16-wide damage-variance roll to its max (100%) *and*
  // lands under the 360 crit threshold (guaranteed crit); a context of 400
  // still saturates variance to 100% (same upperExclusive-1 either way) but
  // clears the crit threshold (guaranteed no crit) - isolating the crit
  // multiplier as the only difference between the two calls.
  uint32_t critContext = 15;
  uint32_t noCritContext = 400;
  const RandomSource critRandom{&critContext, fixedRoll};
  const RandomSource noCritRandom{&noCritContext, fixedRoll};

  BattleCombatant attackerCrit = makeCombatant(1, 20, {163});
  BattleCombatant defenderCrit = makeCombatant(4, 20, {33});
  BattleCombatant attackerNoCrit = makeCombatant(1, 20, {163});
  BattleCombatant defenderNoCrit = makeCombatant(4, 20, {33});

  const pokemon::BattleTurnResult critResult = pokemon::stepBattle(attackerCrit, defenderCrit, 0, critRandom);
  const pokemon::BattleTurnResult noCritResult = pokemon::stepBattle(attackerNoCrit, defenderNoCrit, 0, noCritRandom);

  CHECK(critResult.player.critical);
  CHECK(!noCritResult.player.critical);
  const uint16_t critDamage = defenderCrit.maxHp - defenderCrit.currentHp;
  const uint16_t noCritDamage = defenderNoCrit.maxHp - defenderNoCrit.currentHp;
  CHECK(critDamage == noCritDamage * 2U);

  // A move outside the high-crit list uses the plain baseSpeed/512 threshold
  // (45 for Bulbasaur) rather than baseSpeed/64 (360) - a roll of 100 sits
  // between the two, demonstrating the high-crit list genuinely lowers the
  // bar rather than every move sharing one threshold.
  uint32_t midContext = 100;
  const RandomSource midRandom{&midContext, fixedRoll};
  BattleCombatant tackleUser = makeCombatant(1, 20, {33});
  BattleCombatant tackleTarget = makeCombatant(4, 20, {33});
  const pokemon::BattleTurnResult tackleResult = pokemon::stepBattle(tackleUser, tackleTarget, 0, midRandom);
  CHECK(!tackleResult.player.critical);  // 100 >= Tackle's threshold of 45
}

void applyStatStageMultiplierMatchesGen1Table() {
  CHECK(pokemon::applyStatStage(100, 0) == 100);
  CHECK(pokemon::applyStatStage(100, 1) == 150);
  CHECK(pokemon::applyStatStage(100, 2) == 200);
  CHECK(pokemon::applyStatStage(100, 6) == 400);
  CHECK(pokemon::applyStatStage(100, -1) == 66);
  CHECK(pokemon::applyStatStage(100, -6) == 25);
  // Out-of-range stages are clamped to +-6 rather than trusted verbatim.
  CHECK(pokemon::applyStatStage(100, 10) == pokemon::applyStatStage(100, 6));
  CHECK(pokemon::applyStatStage(100, -10) == pokemon::applyStatStage(100, -6));
}

void applyAccuracyEvasionStageMultiplierMatchesGen1Table() {
  CHECK(pokemon::applyAccuracyEvasionStage(100, 0) == 100);
  CHECK(pokemon::applyAccuracyEvasionStage(100, 1) == 133);
  CHECK(pokemon::applyAccuracyEvasionStage(100, 6) == 300);
  CHECK(pokemon::applyAccuracyEvasionStage(100, -1) == 75);
  CHECK(pokemon::applyAccuracyEvasionStage(100, -6) == 33);
}

void statChangeForMoveIdentifiesSelfBuffsAndOpponentDebuffs() {
  const pokemon::StatChangeEffect* swordsDance = pokemon::statChangeForMove(14);
  CHECK(swordsDance != nullptr);
  CHECK(swordsDance->targetsSelf);
  CHECK(swordsDance->stat == StatKind::Attack);
  CHECK(swordsDance->stages == 2);

  const pokemon::StatChangeEffect* growl = pokemon::statChangeForMove(45);
  CHECK(growl != nullptr);
  CHECK(!growl->targetsSelf);
  CHECK(growl->stat == StatKind::Attack);
  CHECK(growl->stages == -1);

  CHECK(pokemon::statChangeForMove(114) == nullptr);  // Haze is handled separately, not table-driven
  CHECK(pokemon::statChangeForMove(33) == nullptr);   // Tackle: not a stat-changing move at all
}

void selfBuffMoveRaisesAttackStageAndReportsEvent() {
  BattleCombatant attacker = makeCombatant(1, 20, {14});  // Swords Dance
  BattleCombatant defender = makeCombatant(4, 20, {33});
  const pokemon::BattleTurnResult result = pokemon::stepBattle(attacker, defender, 0, ZERO_RANDOM);
  CHECK(result.player.event == BattleLogEvent::StatRaised);
  CHECK(attacker.attackStage == 2);
  CHECK(defender.attackStage == 0);
}

void opponentDebuffMoveLowersDefendersStageNotTheUsers() {
  BattleCombatant attacker = makeCombatant(1, 20, {45});  // Growl
  BattleCombatant defender = makeCombatant(4, 20, {33});
  const pokemon::BattleTurnResult result = pokemon::stepBattle(attacker, defender, 0, ZERO_RANDOM);
  CHECK(result.player.event == BattleLogEvent::StatLowered);
  CHECK(defender.attackStage == -1);
  CHECK(attacker.attackStage == 0);
}

void statChangeAtCapReportsFailureInsteadOfExceedingBounds() {
  BattleCombatant attacker = makeCombatant(1, 20, {14});  // Swords Dance, +2 Attack
  attacker.attackStage = 6;                               // already at the cap
  BattleCombatant defender = makeCombatant(4, 20, {33});
  const pokemon::BattleTurnResult result = pokemon::stepBattle(attacker, defender, 0, ZERO_RANDOM);
  CHECK(result.player.event == BattleLogEvent::StatChangeFailed);
  CHECK(attacker.attackStage == 6);
}

void hazeResetsBothSidesStatStages() {
  BattleCombatant attacker = makeCombatant(1, 20, {114});  // Haze
  attacker.attackStage = 2;
  BattleCombatant defender = makeCombatant(4, 20, {33});
  defender.defenseStage = -3;
  const pokemon::BattleTurnResult result = pokemon::stepBattle(attacker, defender, 0, ZERO_RANDOM);
  CHECK(result.player.event == BattleLogEvent::StatsReset);
  CHECK(attacker.attackStage == 0);
  CHECK(defender.defenseStage == 0);
}

void applyBattleBoostItemRaisesEachXStatByOneStageUpToTheCap() {
  BattleCombatant combatant{};
  CHECK(pokemon::applyBattleBoostItem(combatant, pokemon::ITEM_X_ATTACK));
  CHECK(combatant.attackStage == 1);
  CHECK(pokemon::applyBattleBoostItem(combatant, pokemon::ITEM_X_DEFENSE));
  CHECK(combatant.defenseStage == 1);
  CHECK(pokemon::applyBattleBoostItem(combatant, pokemon::ITEM_X_SPEED));
  CHECK(combatant.speedStage == 1);
  CHECK(pokemon::applyBattleBoostItem(combatant, pokemon::ITEM_X_SPECIAL));
  CHECK(combatant.specialStage == 1);

  combatant.attackStage = 6;
  CHECK(!pokemon::applyBattleBoostItem(combatant, pokemon::ITEM_X_ATTACK));  // already at the +6 cap
  CHECK(combatant.attackStage == 6);
}

void guardSpecItemActivatesOnceAndBlocksOpponentStatLoweringMoves() {
  BattleCombatant combatant{};
  CHECK(pokemon::applyBattleBoostItem(combatant, pokemon::ITEM_GUARD_SPEC));
  CHECK(combatant.guardSpecActive);
  CHECK(!pokemon::applyBattleBoostItem(combatant, pokemon::ITEM_GUARD_SPEC));  // already active this battle

  // Growl (move 45) lowers the target's Attack by 1 stage - Guard Spec
  // should block that entirely, leaving the stage untouched, rather than
  // just capping it the way an already-at-6 stage would.
  BattleCombatant attacker = makeCombatant(4, 20, {45});
  BattleCombatant defender = makeCombatant(1, 20, {33});
  defender.guardSpecActive = true;
  const pokemon::BattleTurnResult result = pokemon::stepBattle(attacker, defender, 0, ZERO_RANDOM);
  CHECK(defender.attackStage == 0);
  CHECK(result.player.event == BattleLogEvent::StatChangeFailed);
}

void guardSpecDoesNotBlockSelfBuffingStatMoves() {
  // Guard Spec only blocks the OPPONENT from lowering this combatant's own
  // stats - it must not interfere with the combatant's own self-buffs.
  BattleCombatant attacker = makeCombatant(1, 20, {14});  // Swords Dance, self-targeting +2 Attack
  attacker.guardSpecActive = true;
  BattleCombatant defender = makeCombatant(4, 20, {33});
  const pokemon::BattleTurnResult result = pokemon::stepBattle(attacker, defender, 0, ZERO_RANDOM);
  CHECK(result.player.event == BattleLogEvent::StatRaised);
  CHECK(attacker.attackStage == 2);
}

void direHitItemActivatesOnceAndRaisesCritRatioToTheHighTier() {
  BattleCombatant combatant{};
  CHECK(pokemon::applyBattleBoostItem(combatant, pokemon::ITEM_DIRE_HIT));
  CHECK(combatant.direHitActive);
  CHECK(!pokemon::applyBattleBoostItem(combatant, pokemon::ITEM_DIRE_HIT));  // already active this battle

  // Same roll=100 Tackle scenario as criticalHitExactlyDoublesDamageForAHighCritRatioMove's
  // last check (100 sits between Bulbasaur's plain 45 threshold and the
  // high-crit 360 one) - Dire Hit should push a plain Tackle into the
  // high-crit tier exactly the way a move like Slash already is.
  uint32_t midContext = 100;
  const RandomSource midRandom{&midContext, fixedRoll};
  BattleCombatant tackleUser = makeCombatant(1, 20, {33});
  tackleUser.direHitActive = true;
  BattleCombatant tackleTarget = makeCombatant(4, 20, {33});
  const pokemon::BattleTurnResult result = pokemon::stepBattle(tackleUser, tackleTarget, 0, midRandom);
  CHECK(result.player.critical);
}

void applyBattleBoostItemRejectsAnUnknownItemId() {
  BattleCombatant combatant{};
  CHECK(!pokemon::applyBattleBoostItem(combatant, 11));  // Potion - not a battle-boost item at all
}

void battleBoostItemWouldApplyMatchesApplyWithoutMutating() {
  // battleBoostItemWouldApply() exists so a caller can confirm an item
  // would do something BEFORE spending it (round 3 audit bug 2.5) - it
  // must agree with applyBattleBoostItem()'s own applicability check in
  // every case, and must never itself change the combatant.
  BattleCombatant combatant{};
  CHECK(pokemon::battleBoostItemWouldApply(combatant, pokemon::ITEM_X_ATTACK));
  CHECK(combatant.attackStage == 0);  // unmutated - only checked, not applied
  CHECK(pokemon::applyBattleBoostItem(combatant, pokemon::ITEM_X_ATTACK));
  CHECK(combatant.attackStage == 1);

  combatant.attackStage = 6;
  CHECK(!pokemon::battleBoostItemWouldApply(combatant, pokemon::ITEM_X_ATTACK));  // already at the +6 cap
  CHECK(!pokemon::applyBattleBoostItem(combatant, pokemon::ITEM_X_ATTACK));

  BattleCombatant guardSpecUser{};
  CHECK(pokemon::battleBoostItemWouldApply(guardSpecUser, pokemon::ITEM_GUARD_SPEC));
  CHECK(!guardSpecUser.guardSpecActive);  // unmutated
  guardSpecUser.guardSpecActive = true;
  CHECK(!pokemon::battleBoostItemWouldApply(guardSpecUser, pokemon::ITEM_GUARD_SPEC));  // already active

  BattleCombatant direHitUser{};
  CHECK(pokemon::battleBoostItemWouldApply(direHitUser, pokemon::ITEM_DIRE_HIT));
  direHitUser.direHitActive = true;
  CHECK(!pokemon::battleBoostItemWouldApply(direHitUser, pokemon::ITEM_DIRE_HIT));  // already active

  CHECK(!pokemon::battleBoostItemWouldApply(combatant, 11));  // Potion - not a battle-boost item at all
}

void speedStageCanFlipWhichSideActsFirst() {
  // Bulbasaur (base Speed 45) is normally slower than Charmander (65) at the
  // same level, so Charmander acts first and its Ember faints a 1-HP
  // Bulbasaur before it ever gets a turn.
  BattleCombatant bulbasaur = makeCombatant(1, 20, {33});
  bulbasaur.currentHp = 1;
  bulbasaur.maxHp = 1;
  BattleCombatant charmander = makeCombatant(4, 20, {52});
  const pokemon::BattleTurnResult baseline = pokemon::stepBattle(bulbasaur, charmander, 0, ZERO_RANDOM);
  CHECK(!baseline.player.acted);

  // +6 Speed stages (45 * 4 = 180) comfortably overtakes Charmander's 65 -
  // now Bulbasaur must act before Charmander gets a turn.
  BattleCombatant fastBulbasaur = makeCombatant(1, 20, {33});
  fastBulbasaur.currentHp = 1;
  fastBulbasaur.maxHp = 1;
  fastBulbasaur.speedStage = 6;
  BattleCombatant slowCharmander = makeCombatant(4, 20, {52});
  const pokemon::BattleTurnResult boosted = pokemon::stepBattle(fastBulbasaur, slowCharmander, 0, ZERO_RANDOM);
  CHECK(boosted.player.acted);
}

void accuracyStageLoweringCanCauseAMissThatWouldOtherwiseHit() {
  uint32_t roll = 50;  // between the staged (33) and unstaged (100) accuracy thresholds
  const RandomSource midRandom{&roll, fixedRoll};

  BattleCombatant plainAttacker = makeCombatant(1, 20, {33});
  BattleCombatant plainDefender = makeCombatant(4, 20, {33});
  CHECK(pokemon::stepBattle(plainAttacker, plainDefender, 0, midRandom).player.event != BattleLogEvent::MoveMissed);

  BattleCombatant debuffedAttacker = makeCombatant(1, 20, {33});
  debuffedAttacker.accuracyStage = -6;
  BattleCombatant defender = makeCombatant(4, 20, {33});
  CHECK(pokemon::stepBattle(debuffedAttacker, defender, 0, midRandom).player.event == BattleLogEvent::MoveMissed);
}

void evasionStageRaisingCanCauseAMissThatWouldOtherwiseHit() {
  uint32_t roll = 50;  // between the staged (33) and unstaged (100) accuracy thresholds
  const RandomSource midRandom{&roll, fixedRoll};

  BattleCombatant attacker = makeCombatant(1, 20, {33});
  BattleCombatant evasiveDefender = makeCombatant(4, 20, {33});
  evasiveDefender.evasionStage = 6;
  CHECK(pokemon::stepBattle(attacker, evasiveDefender, 0, midRandom).player.event == BattleLogEvent::MoveMissed);
}

void criticalHitIgnoresAttackersUnfavorableNegativeAttackStage() {
  // A crit uses whichever of {unstaged, staged} Attack is higher, so a -1
  // Attack stage has zero effect on a guaranteed crit - the real Gen 1 rule.
  uint32_t critRoll = 15;  // guarantees both max variance and a crit on Slash
  const RandomSource critRandom{&critRoll, fixedRoll};

  BattleCombatant debuffed = makeCombatant(1, 20, {163});  // Slash, high-crit move
  debuffed.attackStage = -1;
  BattleCombatant defenderA = makeCombatant(4, 20, {33});
  pokemon::stepBattle(debuffed, defenderA, 0, critRandom);

  BattleCombatant normal = makeCombatant(1, 20, {163});
  BattleCombatant defenderB = makeCombatant(4, 20, {33});
  pokemon::stepBattle(normal, defenderB, 0, critRandom);

  const uint16_t debuffedDamage = defenderA.maxHp - defenderA.currentHp;
  const uint16_t normalDamage = defenderB.maxHp - defenderB.currentHp;
  CHECK(debuffedDamage == normalDamage);
}

void criticalHitIgnoresDefendersUnfavorablePositiveDefenseStage() {
  uint32_t critRoll = 15;
  const RandomSource critRandom{&critRoll, fixedRoll};

  BattleCombatant attackerA = makeCombatant(1, 20, {163});
  BattleCombatant buffedDefender = makeCombatant(4, 20, {33});
  buffedDefender.defenseStage = 2;
  pokemon::stepBattle(attackerA, buffedDefender, 0, critRandom);

  BattleCombatant attackerB = makeCombatant(1, 20, {163});
  BattleCombatant plainDefender = makeCombatant(4, 20, {33});
  pokemon::stepBattle(attackerB, plainDefender, 0, critRandom);

  const uint16_t buffedDamage = buffedDefender.maxHp - buffedDefender.currentHp;
  const uint16_t plainDamage = plainDefender.maxHp - plainDefender.currentHp;
  CHECK(buffedDamage == plainDamage);
}

void recoilMoveDamagesTheAttackerAfterDealingDamage() {
  // Take Down (move 36): a real Gen 1 recoil move, 1/4 of damage dealt back
  // to the user. A fixedRoll context of 70 lands under Take Down's 85
  // accuracy (hit), at/above Charmander's 65 base-Speed crit threshold (no
  // crit, keeping the damage/recoil math simple to verify exactly), and
  // saturates the 16-wide damage-variance roll to its max (100%).
  uint32_t context = 70;
  const RandomSource fixedRandom{&context, fixedRoll};
  BattleCombatant attacker = makeCombatant(4, 20, {36});  // Charmander, Take Down
  // Same level as the attacker - Charmander's higher base Speed (65 vs 45)
  // keeps it acting first regardless - with HP overridden well above
  // anything Take Down could deal in one hit, so the measured damage/recoil
  // never gets skewed by HP clamping at 0.
  BattleCombatant defender = makeCombatant(1, 20, {33});
  defender.maxHp = 200;
  defender.currentHp = 200;
  // Asleep so it never counter-attacks - stepBattle() resolves a full
  // two-sided turn, and an awake defender's own hit back would inflate the
  // measured "recoil" with unrelated damage from its counter-attack.
  defender.status = Ailment::Sleep;
  defender.statusTurns = 5;
  const uint16_t attackerHpBefore = attacker.currentHp;
  const uint16_t defenderHpBefore = defender.currentHp;
  const pokemon::BattleTurnResult result = pokemon::stepBattle(attacker, defender, 0, fixedRandom);
  CHECK(defender.currentHp < defenderHpBefore);
  CHECK(result.player.recoilApplied);
  CHECK(attacker.currentHp < attackerHpBefore);
  const uint16_t damageDealt = defenderHpBefore - defender.currentHp;
  const uint16_t recoilTaken = attackerHpBefore - attacker.currentHp;
  CHECK(recoilTaken == std::max<uint16_t>(1, static_cast<uint16_t>(damageDealt / 4U)));
}

void nonRecoilMoveNeverAppliesRecoil() {
  BattleCombatant attacker = makeCombatant(4, 20, {33});  // Tackle: not a recoil move
  BattleCombatant defender = makeCombatant(1, 20, {33});
  const pokemon::BattleTurnResult result = pokemon::stepBattle(attacker, defender, 0, MAX_RANDOM);
  CHECK(!result.player.recoilApplied);
}

void multiHitMoveConnectsMultipleTimesAndReportsHitCount() {
  // Comet Punch (move 4): a real Gen 1 multi-hit move with a random 2/3/4/5
  // hit count. A fixedRoll context of 7 forces every 8-wide roll in
  // rollMultiHitCount() to its max (upperExclusive-1 = 7), landing in the
  // top 1/8 bucket - always exactly 5 hits.
  uint32_t context = 7;
  const RandomSource fixedRandom{&context, fixedRoll};
  BattleCombatant attacker = makeCombatant(4, 30, {4});  // Charmander, Comet Punch
  // Same level as the attacker - Charmander's higher base Speed keeps it
  // acting first - with HP overridden well above what 5 hits could deal.
  BattleCombatant defender = makeCombatant(1, 30, {33});  // Bulbasaur
  defender.maxHp = 300;
  defender.currentHp = 300;
  const pokemon::BattleTurnResult result = pokemon::stepBattle(attacker, defender, 0, fixedRandom);
  CHECK(result.player.hitCount == 5);
}

void twineedleAlwaysHitsExactlyTwice() {
  // Twineedle (move 41) is the one multi-hit move with a fixed count rather
  // than a rolled one - always exactly 2 hits, never 3-5.
  BattleCombatant attacker = makeCombatant(1, 30, {41});  // Bulbasaur, Twineedle
  BattleCombatant defender = makeCombatant(4, 60, {33});
  const pokemon::BattleTurnResult result = pokemon::stepBattle(attacker, defender, 0, MAX_RANDOM);
  CHECK(result.player.hitCount == 2);
}

void nonMultiHitMoveReportsZeroHitCount() {
  BattleCombatant attacker = makeCombatant(4, 20, {33});  // Tackle: single-hit
  BattleCombatant defender = makeCombatant(1, 20, {33});
  const pokemon::BattleTurnResult result = pokemon::stepBattle(attacker, defender, 0, MAX_RANDOM);
  CHECK(result.player.hitCount == 0);
}

void multiHitMoveStopsEarlyIfTheDefenderFaintsPartway() {
  uint32_t context = 7;  // forces 5 intended hits, same as the test above
  const RandomSource fixedRandom{&context, fixedRoll};
  BattleCombatant attacker = makeCombatant(4, 50, {4});  // strong Charmander, Comet Punch
  BattleCombatant defender = makeCombatant(1, 5, {33});  // weak, 1-HP Bulbasaur
  defender.currentHp = 1;
  defender.maxHp = 1;
  const pokemon::BattleTurnResult result = pokemon::stepBattle(attacker, defender, 0, fixedRandom);
  CHECK(defender.currentHp == 0);
  CHECK(result.player.hitCount == 1);  // fainted on the first hit - the rest never got attempted
}

void multiHitMoveStopsAfterBreakingASubstituteInsteadOfHittingRealHp() {
  // Regression test: a multi-hit move that breaks a Substitute partway
  // through used to keep landing its remaining hits on the defender's real
  // HP instead of stopping there, like the real games do. Twineedle (move
  // 41) always hits exactly twice - give the defender a 1-HP Substitute so
  // the first hit alone breaks it, then assert the second hit never touched
  // real HP.
  BattleCombatant attacker = makeCombatant(1, 30, {41});  // Bulbasaur, Twineedle
  BattleCombatant defender = makeCombatant(4, 30, {33});  // Charmander
  defender.substituteHp = 1;
  const uint16_t hpBefore = defender.currentHp;
  const pokemon::BattleTurnResult result = pokemon::stepBattle(attacker, defender, 0, MAX_RANDOM);
  CHECK(defender.substituteHp == 0);
  CHECK(defender.currentHp == hpBefore);
  CHECK(result.player.hitCount == 1);  // stopped after breaking the substitute, not 2
}

void forcedStruggleSentinelDealsDamageAndRecoilsWithNoLearnedMove() {
  // pokemon::BATTLE_MOVE_SLOTS itself (or any value >= it) as the move slot
  // forces a real Struggle turn - no learned move needed, no PP touched.
  BattleCombatant attacker = makeCombatant(4, 30, {});  // Charmander, no moves at all
  // HP overridden well above what one Struggle hit could deal, so the
  // measured damage/recoil never gets skewed by HP clamping at 0.
  BattleCombatant defender = makeCombatant(7, 20, {33});
  defender.maxHp = 200;
  defender.currentHp = 200;
  // Asleep so it never counter-attacks (see the same note in the Take Down
  // recoil test above).
  defender.status = Ailment::Sleep;
  defender.statusTurns = 5;
  const uint16_t attackerHpBefore = attacker.currentHp;
  const uint16_t defenderHpBefore = defender.currentHp;
  const pokemon::BattleTurnResult result =
      pokemon::stepBattle(attacker, defender, pokemon::BATTLE_MOVE_SLOTS, MAX_RANDOM);
  CHECK(result.player.acted);
  CHECK(result.player.moveSlot == pokemon::BATTLE_MOVE_SLOTS);
  CHECK(defender.currentHp < defenderHpBefore);
  CHECK(result.player.recoilApplied);
  CHECK(attacker.currentHp < attackerHpBefore);
  const uint16_t damageDealt = defenderHpBefore - defender.currentHp;
  const uint16_t recoilTaken = attackerHpBefore - attacker.currentHp;
  CHECK(recoilTaken == std::max<uint16_t>(1, static_cast<uint16_t>(damageDealt / 2U)));
}

void opponentStrugglesWhenAllOfItsLearnedMovesAreOutOfPp() {
  BattleCombatant player = makeCombatant(4, 20, {45});  // Growl: harmless filler, never KOs
  BattleCombatant opponent = makeCombatant(7, 20, {33, 55});
  opponent.moves[0].currentPp = 0;
  opponent.moves[1].currentPp = 0;
  const pokemon::BattleTurnResult result = pokemon::stepBattle(player, opponent, 0, MAX_RANDOM);
  CHECK(result.opponent.acted);
  CHECK(result.opponent.moveSlot == pokemon::BATTLE_MOVE_SLOTS);
  CHECK(result.opponent.event != BattleLogEvent::MoveHadNoPp);
  CHECK(result.opponent.recoilApplied);
}

void ivZeroEvZeroReproducesTheOriginalFormulaExactly() {
  // The backward-compat invariant: every caller that hasn't been taught a
  // Pokemon's real IV/EV yet can pass 0/0 (or rely on the defaults) and see
  // no behavior change from before IV/EV existed.
  CHECK(pokemon::battleMaxHp(45, 50) == static_cast<uint16_t>((2U * 45U * 50U) / 100U + 50U + 10U));
  CHECK(pokemon::battleWorkingStat(49, 50) == static_cast<uint16_t>((2U * 49U * 50U) / 100U + 5U));
  CHECK(pokemon::battleMaxHp(45, 50, 0, 0) == pokemon::battleMaxHp(45, 50));
  CHECK(pokemon::battleWorkingStat(49, 50, 0, 0) == pokemon::battleWorkingStat(49, 50));
}

void ivAndEvRaiseStatsAboveTheZeroBaseline() {
  const uint16_t baseline = pokemon::battleWorkingStat(49, 50);
  CHECK(pokemon::battleWorkingStat(49, 50, 15, 0) > baseline);   // max IV alone helps
  CHECK(pokemon::battleWorkingStat(49, 50, 0, 255) > baseline);  // max EV alone helps
  CHECK(pokemon::battleWorkingStat(49, 50, 15, 255) > pokemon::battleWorkingStat(49, 50, 15, 0));

  const uint16_t hpBaseline = pokemon::battleMaxHp(45, 50);
  CHECK(pokemon::battleMaxHp(45, 50, 15, 0) > hpBaseline);
  CHECK(pokemon::battleMaxHp(45, 50, 0, 255) > hpBaseline);
}

void evBonusMatchesTheFlatDivideByFourFormula() {
  // ev/4 exactly, not the real Gen 1 sqrt(ev)/4 curve. 100/4=25, 255/4=63 (both floored).
  CHECK(pokemon::battleWorkingStat(50, 100, 0, 100) == static_cast<uint16_t>((2U * 50U + 25U) * 100U / 100U + 5U));
  CHECK(pokemon::battleWorkingStat(50, 100, 0, 255) == static_cast<uint16_t>((2U * 50U + 63U) * 100U / 100U + 5U));
}

void outOfRangeIvIsClampedToTheRealGen1Ceiling() {
  // A caller passing an IV above the real 0-15 ceiling (shouldn't happen -
  // IvEvStoreCodec's own validateIvEvEntry() already rejects it before it's
  // ever persisted - but the formula itself still guards independently)
  // must not get more bonus than a real IV of 15 would give.
  CHECK(pokemon::battleWorkingStat(50, 100, 200, 0) == pokemon::battleWorkingStat(50, 100, 15, 0));
}

void rollIvSetProducesValuesInTheRealGen1Range() {
  uint32_t context = 15;  // saturates a 16-wide roll to its max, 15
  const RandomSource maxIvRandom{&context, fixedRoll};
  std::array<uint8_t, pokemon::STAT_COUNT> iv{};
  pokemon::rollIvSet(maxIvRandom, iv);
  for (const uint8_t value : iv) CHECK(value == 15);

  pokemon::rollIvSet(ZERO_RANDOM, iv);
  for (const uint8_t value : iv) CHECK(value == 0);
}

void maxPpForMatchesTheRealGen1PpUpProgression() {
  // A real Gen 1 example: a 40-PP move goes 40 -> 48 -> 56 -> 64 across the
  // 3 real PP Up uses (each adds floor(basePp/5) = 8).
  CHECK(pokemon::maxPpFor(40, 0) == 40);
  CHECK(pokemon::maxPpFor(40, 1) == 48);
  CHECK(pokemon::maxPpFor(40, 2) == 56);
  CHECK(pokemon::maxPpFor(40, 3) == 64);
  // A caller passing more than the real 3-use cap (shouldn't happen -
  // PokemonService::applyPpUp() refuses a 4th use - but the formula itself
  // still guards independently) must not exceed the 3-use result.
  CHECK(pokemon::maxPpFor(40, 5) == pokemon::maxPpFor(40, 3));
  // A move whose base PP isn't a multiple of 5 still floors the bonus per
  // use rather than accumulating fractional PP.
  CHECK(pokemon::maxPpFor(30, 1) == 36);  // floor(30/5)=6
  CHECK(pokemon::maxPpFor(17, 1) == 20);  // floor(17/5)=3
}

void battleVictoryXpScalesWithTheLevelGap() {
  // Lo * ((2*Lo + 10) / (Lo + Lp + 10))^2.5, rounded, at least 1.
  CHECK(pokemon::battleVictoryXp(5, 5) == 5);  // same level: the opponent's level
  CHECK(pokemon::battleVictoryXp(65, 65) == 65);
  CHECK(pokemon::battleVictoryXp(14, 24) == 8);   // 10 levels above: x0.56
  CHECK(pokemon::battleVictoryXp(50, 40) == 63);  // 10 levels below: x1.27
  CHECK(pokemon::battleVictoryXp(65, 95) == 40);
  CHECK(pokemon::battleVictoryXp(2, 100) == 1);  // never 0 for a real win
  CHECK(pokemon::battleVictoryXp(0, 50) == 0);
}

// Move ids for the fixed-damage moves below (PokeAPI's own data stores
// power=0 for every one of these - see FIXED_DAMAGE_TABLE's doc comment in
// PokemonBattle.cpp): 12 Guillotine, 32 Horn Drill, 90 Fissure (OHKO);
// 69 Seismic Toss, 101 Night Shade (damage = user's level); 82 Dragon Rage
// (flat 40), 49 Sonic Boom (flat 20); 149 Psywave (random, scaled to level);
// 162 Super Fang (halves target's current HP); 68 Counter (reflects 2x the
// last physical damage taken this turn); 67 Low Kick (simplified fixed
// power, since this project has no per-species weight data).

void ohkoMoveInstantlyFaintsWhenAttackerLevelIsAtLeastDefenders() {
  // Guillotine at equal level: real Gen 1 hit chance is exactly the move's
  // listed accuracy (30) plus the level difference (0 here) - ZERO_RANDOM
  // (always rolls 0, below any positive threshold) connects and instantly
  // faints the target regardless of its remaining HP.
  BattleCombatant charmander = makeCombatant(4, 20, {12});
  BattleCombatant squirtle = makeCombatant(7, 20, {33});
  const pokemon::BattleTurnResult result = pokemon::stepBattle(charmander, squirtle, 0, ZERO_RANDOM);
  CHECK(result.player.event == BattleLogEvent::OneHitKo);
  CHECK(squirtle.currentHp == 0);
}

void ohkoMoveNeverConnectsWhenAttackerIsALowerLevel() {
  // A real Gen 1 rule: an OHKO move always misses if the user's level is
  // below the target's, no matter how favorable the random roll is. Keep the
  // level gap small so Squirtle's own (normal, non-OHKO) Tackle can't just
  // faint Charmander outright before Guillotine gets a chance to whiff.
  BattleCombatant charmander = makeCombatant(4, 20, {12});
  BattleCombatant squirtle = makeCombatant(7, 25, {33});
  const uint16_t hpBefore = squirtle.currentHp;
  const pokemon::BattleTurnResult result = pokemon::stepBattle(charmander, squirtle, 0, ZERO_RANDOM);
  CHECK(result.player.event == BattleLogEvent::MoveMissed);
  CHECK(squirtle.currentHp == hpBefore);
}

void ohkoMoveHasNoEffectOnAnImmuneType() {
  // Guillotine/Horn Drill are Normal-type - Gastly (Ghost/Poison) is immune
  // to Normal, and that immunity applies to an OHKO move exactly like any
  // other move.
  BattleCombatant charmander = makeCombatant(4, 50, {12});
  BattleCombatant gastly = makeCombatant(92, 5, {33});
  const uint16_t hpBefore = gastly.currentHp;
  const pokemon::BattleTurnResult result = pokemon::stepBattle(charmander, gastly, 0, ZERO_RANDOM);
  CHECK(result.player.event == BattleLogEvent::MoveNoEffect);
  CHECK(gastly.currentHp == hpBefore);
}

void seismicTossAndNightShadeDealDamageEqualToUsersLevel() {
  // Charmander (base Speed 65) is faster than Bulbasaur (base Speed 45) at
  // the same level, so its fixed-damage move always resolves this turn
  // regardless of the random source.
  BattleCombatant charmander = makeCombatant(4, 20, {69});  // Seismic Toss
  BattleCombatant bulbasaur = makeCombatant(1, 20, {33});
  const uint16_t hpBefore = bulbasaur.currentHp;
  const pokemon::BattleTurnResult result = pokemon::stepBattle(charmander, bulbasaur, 0, ZERO_RANDOM);
  CHECK(hpBefore - bulbasaur.currentHp == 20);
  CHECK(result.player.event == BattleLogEvent::MoveHit);

  BattleCombatant charmander2 = makeCombatant(4, 20, {101});  // Night Shade
  BattleCombatant bulbasaur2 = makeCombatant(1, 20, {33});
  const uint16_t hpBefore2 = bulbasaur2.currentHp;
  pokemon::stepBattle(charmander2, bulbasaur2, 0, ZERO_RANDOM);
  CHECK(hpBefore2 - bulbasaur2.currentHp == 20);
}

void dragonRageAndSonicBoomDealFixedFlatDamage() {
  BattleCombatant charmander = makeCombatant(4, 30, {82});  // Dragon Rage
  BattleCombatant bulbasaur = makeCombatant(1, 30, {33});
  const uint16_t hpBefore = bulbasaur.currentHp;
  pokemon::stepBattle(charmander, bulbasaur, 0, ZERO_RANDOM);
  CHECK(hpBefore - bulbasaur.currentHp == 40);

  BattleCombatant charmander2 = makeCombatant(4, 30, {49});  // Sonic Boom
  BattleCombatant bulbasaur2 = makeCombatant(1, 30, {33});
  const uint16_t hpBefore2 = bulbasaur2.currentHp;
  pokemon::stepBattle(charmander2, bulbasaur2, 0, ZERO_RANDOM);
  CHECK(hpBefore2 - bulbasaur2.currentHp == 20);
}

void psywaveDamageIsBoundedBetweenOneAndOnePointFiveTimesLevel() {
  // Real Gen 1 Psywave: random damage from 1 up to 1.5x the user's level.
  BattleCombatant charmander = makeCombatant(4, 20, {149});
  BattleCombatant bulbasaur = makeCombatant(1, 20, {33});
  const uint16_t hpBefore = bulbasaur.currentHp;
  pokemon::stepBattle(charmander, bulbasaur, 0, ZERO_RANDOM);
  CHECK(hpBefore - bulbasaur.currentHp == 1);

  BattleCombatant charmander2 = makeCombatant(4, 20, {149});
  BattleCombatant bulbasaur2 = makeCombatant(1, 20, {33});
  const uint16_t hpBefore2 = bulbasaur2.currentHp;
  pokemon::stepBattle(charmander2, bulbasaur2, 0, MAX_RANDOM);
  CHECK(hpBefore2 - bulbasaur2.currentHp == 30);  // floor(20 * 1.5)
}

void superFangHalvesDefendersCurrentHp() {
  BattleCombatant charmander = makeCombatant(4, 30, {162});  // Super Fang
  BattleCombatant bulbasaur = makeCombatant(1, 5, {33});
  bulbasaur.currentHp = 50;  // override to a clean, easy-to-halve value
  pokemon::stepBattle(charmander, bulbasaur, 0, ZERO_RANDOM);
  CHECK(bulbasaur.currentHp == 25);
}

void lowKickDealsRealDamageInsteadOfTheOldFlatBug() {
  // Before FIXED_DAMAGE_TABLE/LOW_KICK_SIMPLIFIED_POWER existed, Low Kick's
  // power=0 in the source data meant it fell through to the normal formula
  // and dealt a useless ~2 flat damage. It should now deal real damage,
  // comfortably more than that old bug's amount.
  BattleCombatant charmander = makeCombatant(4, 30, {67});
  BattleCombatant bulbasaur = makeCombatant(1, 30, {33});
  const uint16_t hpBefore = bulbasaur.currentHp;
  pokemon::stepBattle(charmander, bulbasaur, 0, ZERO_RANDOM);
  CHECK(hpBefore - bulbasaur.currentHp > 5);
}

void counterReflectsDoubleTheLastPhysicalDamageTakenThisTurn() {
  // Charmander (higher base Speed, so faster even at an equal level) uses
  // Tackle; Squirtle's only move is Counter - it should reflect exactly 2x
  // the damage Tackle just dealt, back at Charmander, this same turn. Keep
  // the level gap small so Tackle doesn't just faint Squirtle outright
  // before its own Counter gets a turn.
  BattleCombatant charmander = makeCombatant(4, 6, {33});  // Tackle
  BattleCombatant squirtle = makeCombatant(7, 5, {68});    // Counter
  const uint16_t squirtleHpBefore = squirtle.currentHp;
  const uint16_t charmanderHpBefore = charmander.currentHp;

  const pokemon::BattleTurnResult result = pokemon::stepBattle(squirtle, charmander, 0, ZERO_RANDOM);

  const uint16_t tackleDamage = squirtleHpBefore - squirtle.currentHp;
  CHECK(tackleDamage > 0);
  const uint16_t counterDamage = charmanderHpBefore - charmander.currentHp;
  CHECK(counterDamage == tackleDamage * 2U);
  CHECK(result.player.event != BattleLogEvent::MoveFailed);
}

void counterFailsWhenUserHasNotTakenPhysicalDamageThisTurn() {
  // Squirtle uses Counter while Charmander uses Growl (a Status move, no
  // physical damage) - nothing to reflect this turn regardless of turn
  // order (Counter's own real -1 priority, added alongside move priority,
  // now always resolves it last anyway - see movePriority()), so it fails
  // ("But it failed!") instead of dealing damage.
  BattleCombatant squirtle = makeCombatant(7, 30, {68});   // Counter
  BattleCombatant charmander = makeCombatant(4, 5, {45});  // Growl
  const uint16_t charmanderHpBefore = charmander.currentHp;

  const pokemon::BattleTurnResult result = pokemon::stepBattle(squirtle, charmander, 0, ZERO_RANDOM);

  CHECK(result.player.event == BattleLogEvent::MoveFailed);
  CHECK(charmander.currentHp == charmanderHpBefore);
}

void flinchMoveCanPreventTheTargetsActionThisSameTurn() {
  // Charmander (higher base Speed, faster at an equal level) uses Stomp
  // (move 23, 30% flinch chance) on Bulbasaur; ZERO_RANDOM guarantees both
  // a critical hit and the flinch roll succeeding (0 < 30), so Bulbasaur's
  // own Tackle must never resolve this same turn. Level 20 for both keeps
  // Bulbasaur's HP comfortably above even a critical Stomp, so it survives
  // to (not) act.
  BattleCombatant charmander = makeCombatant(4, 20, {23});  // Stomp
  BattleCombatant bulbasaur = makeCombatant(1, 20, {33});   // Tackle
  const uint16_t charmanderHpBefore = charmander.currentHp;

  const pokemon::BattleTurnResult result = pokemon::stepBattle(charmander, bulbasaur, 0, ZERO_RANDOM);

  CHECK(result.opponent.event == BattleLogEvent::Flinched);
  CHECK(charmander.currentHp == charmanderHpBefore);
}

void flinchDoesNotPersistPastTheTurnItWasInflicted() {
  // A flinch flag left over from a turn where the flinched side never got
  // to act (e.g. it fainted first) must not carry into the next turn -
  // stepBattle() resets it up front, before either side's own action runs.
  BattleCombatant charmander = makeCombatant(4, 30, {45});  // Growl - harmless, never flinches
  BattleCombatant bulbasaur = makeCombatant(1, 5, {33});    // Tackle
  bulbasaur.flinched = true;
  const uint16_t charmanderHpBefore = charmander.currentHp;

  pokemon::stepBattle(charmander, bulbasaur, 0, ZERO_RANDOM);

  CHECK(charmander.currentHp < charmanderHpBefore);  // Bulbasaur's Tackle landed normally
}

void drainMoveHealsHalfTheDamageDealtAndCapsAtMaxHp() {
  // Charmander (faster) uses Absorb (move 71) on Bulbasaur; Bulbasaur's own
  // move is Growl (harmless, deals no damage), isolating Charmander's HP
  // change to the drain effect alone.
  BattleCombatant charmander = makeCombatant(4, 30, {71});  // Absorb
  BattleCombatant bulbasaur = makeCombatant(1, 5, {45});    // Growl
  charmander.currentHp = static_cast<uint16_t>(charmander.maxHp / 2U);
  const uint16_t hpBeforeHeal = charmander.currentHp;
  const uint16_t bulbasaurHpBefore = bulbasaur.currentHp;

  const pokemon::BattleTurnResult result = pokemon::stepBattle(charmander, bulbasaur, 0, ZERO_RANDOM);

  const uint16_t damageDealt = static_cast<uint16_t>(bulbasaurHpBefore - bulbasaur.currentHp);
  CHECK(damageDealt > 0);
  CHECK(result.player.drainApplied);
  const uint16_t healed = static_cast<uint16_t>(charmander.currentHp - hpBeforeHeal);
  CHECK(healed == std::max<uint16_t>(1, static_cast<uint16_t>(damageDealt / 2U)));

  // Starting at full HP, the same heal must clamp at maxHp instead of
  // overflowing past it.
  BattleCombatant charmander2 = makeCombatant(4, 30, {71});
  BattleCombatant bulbasaur2 = makeCombatant(1, 5, {45});
  pokemon::stepBattle(charmander2, bulbasaur2, 0, ZERO_RANDOM);
  CHECK(charmander2.currentHp == charmander2.maxHp);
}

void selfDestructMoveFaintsTheUserRegardlessOfHitOrMiss() {
  BattleCombatant charmander = makeCombatant(4, 30, {153});  // Explosion
  BattleCombatant bulbasaur = makeCombatant(1, 5, {33});
  pokemon::stepBattle(charmander, bulbasaur, 0, ZERO_RANDOM);
  CHECK(charmander.currentHp == 0);
}

void selfDestructHalvesDefendersDefenseForThisHit() {
  // Charmander (level 30) uses Explosion on a tanky level-100 Bulbasaur -
  // Normal vs. Grass/Poison is exactly 100% effectiveness and Charmander
  // isn't Normal-type (no STAB), so the only unknown left in the standard
  // formula is the Defense term, which the real Gen 1 quirk halves for this
  // one hit. A high defender level keeps the hit from one-shotting (and
  // thus clamping) the result, so the exact formula can be checked.
  //
  // Uses stepOpponentOnlyTurn() (with Charmander in the "opponent" role, so
  // it's the one guaranteed to act) rather than stepBattle(), specifically
  // to sidestep turn order - Bulbasaur's much higher level would otherwise
  // make it faster and let its own Tackle go first, which isn't what this
  // test is about.
  BattleCombatant charmander = makeCombatant(4, 30, {153});  // Explosion
  BattleCombatant bulbasaur = makeCombatant(1, 100, {33});
  const uint16_t hpBefore = bulbasaur.currentHp;

  uint32_t maxContext = 0xFFFFFFFFU;  // saturates any upperExclusive roll to its max
  const RandomSource maxRoll{&maxContext, fixedRoll};
  pokemon::stepOpponentOnlyTurn(bulbasaur, charmander, maxRoll);
  const uint16_t damage = static_cast<uint16_t>(hpBefore - bulbasaur.currentHp);

  const pokemon::BaseStats* attackerStats = pokemon::baseStatsFor(4);
  const pokemon::BaseStats* defenderStats = pokemon::baseStatsFor(1);
  const uint16_t attackStat = pokemon::battleWorkingStat(attackerStats->attack, 30);
  const uint16_t fullDefenseStat = pokemon::battleWorkingStat(defenderStats->defense, 100);
  const uint16_t halvedDefenseStat = std::max<uint16_t>(1, static_cast<uint16_t>(fullDefenseStat / 2U));
  const uint32_t expected = ((2U * 30U / 5U + 2U) * 250U * attackStat) / (50U * halvedDefenseStat) + 2U;
  CHECK(damage == expected);

  // Sanity check that halving actually mattered - the unhalved-defense
  // result would have been meaningfully smaller.
  const uint32_t withoutHalving = ((2U * 30U / 5U + 2U) * 250U * attackStat) / (50U * fullDefenseStat) + 2U;
  CHECK(expected > withoutHalving);
}

void selfDestructCausesASimultaneousKoWhenItAlsoFaintsTheTargetAndCountsAsAWin() {
  // Both the user (always, from using the move) and the low-level, low-HP
  // target (from the hit itself) faint the same turn. Regression test for
  // round 3 audit bug 2.4: this mid-turn simultaneous KO was directly
  // caused by the PLAYER's own action, so - unlike finishTurn()'s
  // end-of-turn tie, where genuinely neither side "won" (a shared status/
  // Leech Seed tick) - it must count as a win, not a loss: real Gen 1
  // resolves a mutual KO in favor of whoever's attack caused it, and the
  // UI must not force a switch/gym-loss against an opponent already at 0 HP.
  BattleCombatant charmander = makeCombatant(4, 100, {153});  // Explosion
  BattleCombatant bulbasaur = makeCombatant(1, 2, {33});
  const pokemon::BattleTurnResult result = pokemon::stepBattle(charmander, bulbasaur, 0, ZERO_RANDOM);
  CHECK(charmander.currentHp == 0);
  CHECK(bulbasaur.currentHp == 0);
  CHECK(result.outcome == pokemon::BattleOutcome::PlayerWon);
}

void damagingMoveWithAilmentDoesNotInflictItAgainstAnImmuneType() {
  // Body Slam (id 34, Normal, 30% paralysis) against a Ghost-type target
  // (Gastly, id 92, Ghost/Poison) deals 0 damage - Normal is 0% effective
  // against Ghost - and must not paralyze it either. Type immunity used to
  // only block the damage roll; the secondary ailment applied anyway with
  // no log line explaining why, since the ailment block never looked at
  // type effectiveness at all.
  BattleCombatant charmander = makeCombatant(4, 30, {34});  // Body Slam
  BattleCombatant gastly = makeCombatant(92, 5, {45});      // Growl - harmless filler
  const pokemon::BattleTurnResult result = pokemon::stepBattle(charmander, gastly, 0, ZERO_RANDOM);
  CHECK(result.player.event == BattleLogEvent::MoveNoEffect);
  CHECK(gastly.currentHp == gastly.maxHp);
  CHECK(gastly.status == Ailment::None);
}

void statusMoveWithAilmentRespectsTypeImmunity() {
  // Thunder Wave (id 86, Electric, status, "always" paralysis per the
  // ailment_chance==0-means-guaranteed convention) against a Ground-type
  // target (Sandshrew, id 27) - Electric is 0% effective against Ground.
  // Status-category moves used to skip the type chart entirely, so this
  // paralyzed a type that should be flatly immune.
  BattleCombatant charmander = makeCombatant(4, 30, {86});  // Thunder Wave
  BattleCombatant sandshrew = makeCombatant(27, 5, {45});   // Growl - harmless filler
  const pokemon::BattleTurnResult result = pokemon::stepBattle(charmander, sandshrew, 0, ZERO_RANDOM);
  CHECK(result.player.event == BattleLogEvent::MoveNoEffect);
  CHECK(sandshrew.status == Ailment::None);

  // Sanity check the other direction, so the fix isn't overly broad: the
  // same move against a non-immune type still paralyzes normally.
  BattleCombatant charmanderVsBulbasaur = makeCombatant(4, 30, {86});
  BattleCombatant bulbasaur = makeCombatant(1, 5, {45});
  const pokemon::BattleTurnResult resultVsBulbasaur =
      pokemon::stepBattle(charmanderVsBulbasaur, bulbasaur, 0, ZERO_RANDOM);
  CHECK(resultVsBulbasaur.player.event == BattleLogEvent::InflictedStatus);
  CHECK(bulbasaur.status == Ailment::Paralysis);
}

void twoTurnMoveChargesThenReleasesOnTheFollowingTurn() {
  // Charmander (faster) uses Fly on Bulbasaur - turn 1 charges (no damage,
  // no accuracy roll); turn 2 automatically releases using the same slot,
  // dealing real damage and spending PP only once (on the charge turn).
  BattleCombatant charmander = makeCombatant(4, 30, {19});  // Fly
  BattleCombatant bulbasaur = makeCombatant(1, 5, {33});    // Tackle
  const uint16_t hpBeforeCharge = bulbasaur.currentHp;
  const pokemon::MoveData* fly = pokemon::moveData(19);

  const pokemon::BattleTurnResult chargeResult = pokemon::stepBattle(charmander, bulbasaur, 0, ZERO_RANDOM);
  CHECK(chargeResult.player.event == BattleLogEvent::ChargingMove);
  CHECK(bulbasaur.currentHp == hpBeforeCharge);
  CHECK(charmander.forcedMoveId == 19);
  CHECK(charmander.moves[0].currentPp == fly->pp - 1);

  const pokemon::BattleTurnResult releaseResult = pokemon::stepBattle(charmander, bulbasaur, 0, ZERO_RANDOM);
  CHECK(releaseResult.player.event != BattleLogEvent::ChargingMove);
  CHECK(bulbasaur.currentHp < hpBeforeCharge);
  CHECK(charmander.forcedMoveId == 0);
  CHECK(charmander.moves[0].currentPp == fly->pp - 1);  // no second PP charge on release
}

void flyGrantsInvulnerabilityDuringTheChargeTurnButSolarBeamDoesNot() {
  // Fly (id 19) grants a charge-turn invulnerability that makes every
  // incoming move miss (simplified - see BattleCombatant::invulnerable) -
  // Bulbasaur's Tackle must whiff even with ZERO_RANDOM, which would
  // otherwise guarantee a hit.
  BattleCombatant charmanderFly = makeCombatant(4, 30, {19});
  BattleCombatant bulbasaurVsFly = makeCombatant(1, 5, {33});
  const uint16_t hpBeforeFlyCharge = charmanderFly.currentHp;
  pokemon::stepBattle(charmanderFly, bulbasaurVsFly, 0, ZERO_RANDOM);
  CHECK(charmanderFly.currentHp == hpBeforeFlyCharge);

  // Solar Beam (id 76) does NOT grant invulnerability - the same setup's
  // Tackle should land normally during its charge turn.
  BattleCombatant charmanderSolar = makeCombatant(4, 30, {76});
  BattleCombatant bulbasaurVsSolar = makeCombatant(1, 5, {33});
  const uint16_t hpBeforeSolarCharge = charmanderSolar.currentHp;
  pokemon::stepBattle(charmanderSolar, bulbasaurVsSolar, 0, ZERO_RANDOM);
  CHECK(charmanderSolar.currentHp < hpBeforeSolarCharge);
}

void twoTurnMoveReleasesEvenWhenItsLastPpWasSpentOnTheChargeTurn() {
  // Regression test for a real softlock: Fly/Dig only spend PP on the charge
  // turn (see twoTurnMoveChargesThenReleasesOnTheFollowingTurn()), so a slot
  // with exactly 1 PP remaining hits 0 PP right after the charge turn. The
  // release turn used to run the "is this slot out of PP?" check before it
  // could recognise "this 0 PP is a legitimate in-progress continuation, not
  // a fresh exhausted choice" - which rejected the release with MoveHadNoPp,
  // left forcedMoveId/invulnerable set forever, and hard-locked the battle
  // (nothing could ever hit the permanently-invulnerable attacker again, and
  // the UI's own forced-continuation guard blocks every other menu option
  // and the Back button while forcedMoveId is set). This must resolve like
  // any other release turn instead.
  BattleCombatant charmander = makeCombatant(4, 30, {19});  // Fly
  BattleCombatant bulbasaur = makeCombatant(1, 5, {33});    // Tackle
  charmander.moves[0].currentPp = 1;
  const uint16_t hpBeforeCharge = bulbasaur.currentHp;

  const pokemon::BattleTurnResult chargeResult = pokemon::stepBattle(charmander, bulbasaur, 0, ZERO_RANDOM);
  CHECK(chargeResult.player.event == BattleLogEvent::ChargingMove);
  CHECK(charmander.moves[0].currentPp == 0);
  CHECK(charmander.forcedMoveId == 19);
  CHECK(charmander.invulnerable);

  const pokemon::BattleTurnResult releaseResult = pokemon::stepBattle(charmander, bulbasaur, 0, ZERO_RANDOM);
  CHECK(releaseResult.player.event != BattleLogEvent::MoveHadNoPp);
  CHECK(releaseResult.player.event != BattleLogEvent::ChargingMove);
  CHECK(bulbasaur.currentHp < hpBeforeCharge);
  CHECK(charmander.forcedMoveId == 0);
  CHECK(!charmander.invulnerable);
  CHECK(charmander.moves[0].currentPp == 0);  // still no second PP charge on release
}

void trapMoveLocksTheAttackerIntoRepeatingItAndBypassesAccuracyOnFollowUpTurns() {
  // Charmander (faster - higher base Speed even at an equal level) uses
  // Wrap on Bulbasaur - a successful first hit locks Charmander into
  // automatically repeating Wrap. Equal levels keep Wrap's real but modest
  // power (15) from one-shotting Bulbasaur outright, which would prevent
  // the lock from ever engaging.
  BattleCombatant charmander = makeCombatant(4, 20, {35});  // Wrap
  BattleCombatant bulbasaur = makeCombatant(1, 20, {45});   // Growl - harmless filler
  pokemon::stepBattle(charmander, bulbasaur, 0, ZERO_RANDOM);
  CHECK(charmander.forcedMoveId == 35);
  CHECK(charmander.forcedTurnsRemaining > 0);

  // A follow-up turn always hits, even with a random source that would
  // otherwise miss against Wrap's 90 accuracy (a roll of 99).
  uint32_t highRollContext = 99;
  const RandomSource highRoll{&highRollContext, fixedRoll};
  const uint16_t hpBeforeContinuation = bulbasaur.currentHp;
  pokemon::stepBattle(charmander, bulbasaur, 0, highRoll);
  CHECK(bulbasaur.currentHp < hpBeforeContinuation);
}

void bideStoresDamageOverTwoTurnsThenReleasesDoubleItBack() {
  // Bulbasaur (level 80, so faster than Charmander's own base-Speed edge)
  // uses Bide; Charmander's Ember lands each turn. MAX_RANDOM avoids a
  // critical hit (which ZERO_RANDOM would otherwise guarantee) so the
  // stored/released amounts stay simple to compare. Charmander is level 50
  // (not 5) so it comfortably survives the doubled release instead of the
  // clamp-at-0 case obscuring the exact-double check.
  BattleCombatant bulbasaur = makeCombatant(1, 80, {117});  // Bide
  BattleCombatant charmander = makeCombatant(4, 50, {52});  // Ember
  const uint16_t charmanderHpBeforeRelease = charmander.currentHp;

  const pokemon::BattleTurnResult turn1 = pokemon::stepBattle(bulbasaur, charmander, 0, MAX_RANDOM);
  CHECK(turn1.player.event == BattleLogEvent::ChargingMove);
  CHECK(bulbasaur.bideTurnsRemaining == 1);
  const uint16_t storedAfterTurn1 = bulbasaur.bideDamageStored;
  CHECK(storedAfterTurn1 > 0);

  const pokemon::BattleTurnResult turn2 = pokemon::stepBattle(bulbasaur, charmander, 0, MAX_RANDOM);
  CHECK(turn2.player.event != BattleLogEvent::ChargingMove);
  CHECK(bulbasaur.bideTurnsRemaining == 0);
  // Charmander also took a second Ember hit from Bulbasaur this same turn,
  // in between - but that happens AFTER Bulbasaur's own (faster) release
  // action, so it doesn't affect what was already unleashed.
  const uint16_t chargedmanderDamageThisRelease =
      static_cast<uint16_t>(charmanderHpBeforeRelease - charmander.currentHp);
  CHECK(chargedmanderDamageThisRelease >= storedAfterTurn1 * 2U);
}

void leechSeedDrainsTheSeededSideAndHealsTheSeederAtEndOfTurn() {
  // Charmander (faster) seeds Squirtle (not Grass-type, so it isn't immune).
  BattleCombatant charmander = makeCombatant(4, 30, {73});  // Leech Seed
  BattleCombatant squirtle = makeCombatant(7, 5, {45});     // Growl - harmless filler
  charmander.currentHp = static_cast<uint16_t>(charmander.maxHp / 2U);
  const uint16_t charmanderHpBeforeTick = charmander.currentHp;
  const uint16_t squirtleHpBeforeTick = squirtle.currentHp;

  const pokemon::BattleTurnResult result = pokemon::stepBattle(charmander, squirtle, 0, ZERO_RANDOM);

  CHECK(result.player.event == BattleLogEvent::Seeded);
  CHECK(squirtle.seeded);
  CHECK(squirtle.currentHp < squirtleHpBeforeTick);
  CHECK(charmander.currentHp > charmanderHpBeforeTick);
}

void leechSeedFailsAgainstAGrassTypeTarget() {
  BattleCombatant charmander = makeCombatant(4, 30, {73});  // Leech Seed
  BattleCombatant bulbasaur = makeCombatant(1, 5, {45});    // Grass/Poison - immune
  const pokemon::BattleTurnResult result = pokemon::stepBattle(charmander, bulbasaur, 0, ZERO_RANDOM);
  CHECK(result.player.event == BattleLogEvent::MoveNoEffect);
  CHECK(!bulbasaur.seeded);
}

void reflectExactlyHalvesNonCriticalPhysicalDamage() {
  // Reflect protects whoever activated it FROM incoming damage - it goes on
  // the defender/target of this exchange (Charmander), not the attacker
  // (Bulbasaur, using Tackle). Uses stepOpponentOnlyTurn() so only
  // Bulbasaur's hit happens (no counter-Tackle back), and a high Charmander
  // level keeps that one hit from one-shotting it - either of which would
  // otherwise clamp the measured damage and hide the real halved value.
  BattleCombatant bulbasaur = makeCombatant(1, 30, {33});  // Tackle, physical
  BattleCombatant charmanderWithReflect = makeCombatant(4, 50, {33});
  charmanderWithReflect.reflectActive = true;
  const uint16_t hpBeforeReflected = charmanderWithReflect.currentHp;
  // avoid a crit, which bypasses Reflect
  pokemon::stepOpponentOnlyTurn(charmanderWithReflect, bulbasaur, MAX_RANDOM);
  const uint16_t reflectedDamage = static_cast<uint16_t>(hpBeforeReflected - charmanderWithReflect.currentHp);

  BattleCombatant bulbasaurControl = makeCombatant(1, 30, {33});
  BattleCombatant charmanderPlain = makeCombatant(4, 50, {33});
  const uint16_t hpBeforePlain = charmanderPlain.currentHp;
  pokemon::stepOpponentOnlyTurn(charmanderPlain, bulbasaurControl, MAX_RANDOM);
  const uint16_t plainDamage = static_cast<uint16_t>(hpBeforePlain - charmanderPlain.currentHp);

  CHECK(reflectedDamage == plainDamage / 2U);
}

void mistAndFocusEnergySetTheirOwnFields() {
  // Focus Energy reuses the exact same direHitActive field the Dire Hit
  // battle-boost item already uses for the identical effect (both really are
  // per-Pokemon, correctly ending on a switch). Mist gets its OWN field
  // (mistActive), deliberately separate from guardSpecActive, since Mist -
  // unlike Guard Spec. itself - protects the whole SIDE and must survive a
  // switch (round 6 audit item 2.5).
  BattleCombatant bulbasaurMist = makeCombatant(1, 20, {54});  // Mist
  BattleCombatant dummy1 = makeCombatant(4, 20, {45});
  pokemon::stepBattle(bulbasaurMist, dummy1, 0, ZERO_RANDOM);
  CHECK(bulbasaurMist.mistActive);
  CHECK(!bulbasaurMist.guardSpecActive);

  BattleCombatant bulbasaurFocus = makeCombatant(1, 20, {116});  // Focus Energy
  BattleCombatant dummy2 = makeCombatant(4, 20, {45});
  pokemon::stepBattle(bulbasaurFocus, dummy2, 0, ZERO_RANDOM);
  CHECK(bulbasaurFocus.direHitActive);
}

// --- Round-6 audit bug 2.5: Mist must keep blocking an opponent's
// stat-lowering move/secondary stat-drop via its OWN mistActive field, now
// that it's no longer aliased onto guardSpecActive (the Guard Spec. item's
// field, which correctly stays per-Pokemon and must NOT also be set by
// Mist) - the actual "survives a switch" behavior itself lives in
// PokemonActivity.cpp (setupBattlePlayer()/setupBattleOpponent()'s
// preserveSideEffects), outside this pure-engine test's reach, but the
// engine-level blocking behavior the split must not regress is covered
// here. ---

void mistBlocksOpponentStatLoweringMovesAndSecondaryStatDropsViaItsOwnField() {
  BattleCombatant mistUser = makeCombatant(1, 50, {45});  // Bulbasaur, target of Growl
  mistUser.mistActive = true;
  CHECK(!mistUser.guardSpecActive);                     // deliberately not aliased onto Guard Spec.'s own field
  BattleCombatant grower = makeCombatant(4, 50, {45});  // Charmander, Growl
  const pokemon::BattleTurnResult result = pokemon::stepOpponentOnlyTurn(mistUser, grower, ZERO_RANDOM);
  CHECK(result.opponent.event == BattleLogEvent::StatChangeFailed);
  CHECK(mistUser.attackStage == 0);

  BattleCombatant mistDefender = makeCombatant(7, 50, {45});  // Squirtle
  mistDefender.mistActive = true;
  BattleCombatant psychicUser = makeCombatant(1, 50, {94});  // Bulbasaur, Psychic
  pokemon::stepOpponentOnlyTurn(mistDefender, psychicUser, ZERO_RANDOM);
  CHECK(mistDefender.specialStage == 0);
}

void recoverHealsHalfMaxHpAndFailsAtFullHealth() {
  BattleCombatant bulbasaur = makeCombatant(1, 30, {105});  // Recover
  bulbasaur.currentHp = static_cast<uint16_t>(bulbasaur.maxHp / 4U);
  const uint16_t hpBeforeHeal = bulbasaur.currentHp;
  BattleCombatant dummy = makeCombatant(4, 5, {45});
  const pokemon::BattleTurnResult healResult = pokemon::stepBattle(bulbasaur, dummy, 0, ZERO_RANDOM);
  CHECK(healResult.player.drainApplied);
  CHECK(bulbasaur.currentHp > hpBeforeHeal);

  BattleCombatant bulbasaurFull = makeCombatant(1, 30, {105});
  BattleCombatant dummy2 = makeCombatant(4, 5, {45});
  const pokemon::BattleTurnResult fullResult = pokemon::stepBattle(bulbasaurFull, dummy2, 0, ZERO_RANDOM);
  CHECK(fullResult.player.event == BattleLogEvent::MoveFailed);
  CHECK(bulbasaurFull.moves[0].currentPp == pokemon::moveData(105)->pp - 1);  // the turn (and PP) is still spent
}

// Rest, Recover and Soft-Boiled all report "But it failed!" at full health;
// Rest must not put a healthy Pokemon to sleep for nothing.
void restAndSoftBoiledFailAtFullHealthWithoutSleeping() {
  BattleCombatant restUser = makeCombatant(1, 30, {156});  // Rest
  BattleCombatant dummy = makeCombatant(4, 5, {45});
  const pokemon::BattleTurnResult restResult = pokemon::stepBattle(restUser, dummy, 0, ZERO_RANDOM);
  CHECK(restResult.player.event == BattleLogEvent::MoveFailed);
  CHECK(restUser.status == Ailment::None);
  CHECK(restUser.moves[0].currentPp == pokemon::moveData(156)->pp - 1);

  BattleCombatant softBoiled = makeCombatant(1, 30, {135});  // Soft-Boiled
  BattleCombatant dummy2 = makeCombatant(4, 5, {45});
  CHECK(pokemon::stepBattle(softBoiled, dummy2, 0, ZERO_RANDOM).player.event == BattleLogEvent::MoveFailed);
}

void restFullyHealsCuresStatusAndSleepsForAFixedTwoTurns() {
  BattleCombatant bulbasaur = makeCombatant(1, 30, {156});  // Rest
  bulbasaur.currentHp = 1;
  bulbasaur.status = Ailment::Poison;
  BattleCombatant dummy = makeCombatant(4, 5, {45});
  pokemon::stepBattle(bulbasaur, dummy, 0, ZERO_RANDOM);
  CHECK(bulbasaur.currentHp == bulbasaur.maxHp);
  CHECK(bulbasaur.status == Ailment::Sleep);
  CHECK(bulbasaur.statusTurns == 2);
}

void whirlwindAndRoarAlwaysReportForcedSwitchOnUse() {
  // The engine can't know whether a switch is actually possible (a roster
  // concern only PokemonActivity.cpp can answer) - it just reports the
  // event unconditionally on use, since these moves have accuracy 0
  // ("never misses" in this dataset).
  BattleCombatant bulbasaur = makeCombatant(1, 20, {18});  // Whirlwind
  BattleCombatant charmander = makeCombatant(4, 5, {33});
  const pokemon::BattleTurnResult result = pokemon::stepBattle(bulbasaur, charmander, 0, ZERO_RANDOM);
  CHECK(result.player.event == BattleLogEvent::ForcedSwitch);
}

void disableLocksOutARandomMoveWithPpAndFailsIfNoneQualify() {
  BattleCombatant charmander = makeCombatant(4, 20, {50});     // Disable
  BattleCombatant bulbasaur = makeCombatant(1, 20, {33, 45});  // Tackle, Growl
  const pokemon::BattleTurnResult result = pokemon::stepBattle(charmander, bulbasaur, 0, ZERO_RANDOM);
  CHECK(result.player.event == BattleLogEvent::MoveDisabled);
  CHECK(bulbasaur.disableTurnsRemaining > 0);
  CHECK(bulbasaur.disabledMoveSlot < 2);

  BattleCombatant charmander2 = makeCombatant(4, 20, {50});
  BattleCombatant bulbasaurNoPp = makeCombatant(1, 20, {33});
  bulbasaurNoPp.moves[0].currentPp = 0;
  const pokemon::BattleTurnResult failResult = pokemon::stepBattle(charmander2, bulbasaurNoPp, 0, ZERO_RANDOM);
  CHECK(failResult.player.event == BattleLogEvent::MoveNoEffect);
}

void disableCountsDownAndEventuallyReleases() {
  BattleCombatant charmander = makeCombatant(4, 20, {50, 45});  // Disable, Growl
  BattleCombatant bulbasaur = makeCombatant(1, 20, {33, 45});   // Tackle, Growl
  pokemon::stepBattle(charmander, bulbasaur, 0, ZERO_RANDOM);   // Disable
  CHECK(bulbasaur.disableTurnsRemaining > 0);
  // Duration is 1-3 turns (simplified from the real 1-7, same range as
  // Sleep) - 3 more filler turns guarantees it reaches 0 regardless of the
  // actual roll.
  for (int i = 0; i < 3; ++i) {
    pokemon::stepBattle(charmander, bulbasaur, 1, ZERO_RANDOM);  // Growl - harmless filler
  }
  CHECK(bulbasaur.disableTurnsRemaining == 0);
}

void disabledMoveIsSkippedByTheAiFallingBackToStruggle() {
  // Bulbasaur (opponent) has only one move, currently disabled - the AI
  // must fall back to a forced Struggle turn instead of picking it anyway.
  // Struggle is the only move here that recoils, so that's what's checked.
  BattleCombatant charmander = makeCombatant(4, 20, {45});  // Growl, harmless
  BattleCombatant bulbasaur = makeCombatant(1, 5, {33});    // Tackle
  bulbasaur.disabledMoveSlot = 0;
  bulbasaur.disableTurnsRemaining = 2;
  const pokemon::BattleTurnResult result = pokemon::stepBattle(charmander, bulbasaur, 0, ZERO_RANDOM);
  CHECK(result.opponent.recoilApplied);
}

void substituteCostsAQuarterMaxHpAndAbsorbsDamageUntilItBreaks() {
  // stepOpponentOnlyTurn() so only Bulbasaur's own Substitute use happens -
  // a normal stepBattle() call would let Charmander's own counter-Tackle
  // land on the freshly-made substitute within the same turn, muddying the
  // "exactly 1/4 max HP" check below.
  BattleCombatant bulbasaur = makeCombatant(1, 30, {164});  // Substitute
  const uint16_t costExpected = std::max<uint16_t>(1, static_cast<uint16_t>(bulbasaur.maxHp / 4U));
  BattleCombatant charmander = makeCombatant(4, 5, {33});
  pokemon::stepOpponentOnlyTurn(charmander, bulbasaur, ZERO_RANDOM);
  CHECK(bulbasaur.substituteHp == costExpected);
  CHECK(bulbasaur.currentHp == bulbasaur.maxHp - costExpected);
}

void substituteBlocksAnOpponentsStatLoweringMove() {
  BattleCombatant bulbasaur = makeCombatant(1, 30, {33});
  bulbasaur.substituteHp = 10;
  BattleCombatant charmander = makeCombatant(4, 5, {45});  // Growl - opponent-debuff
  pokemon::stepOpponentOnlyTurn(bulbasaur, charmander, ZERO_RANDOM);
  CHECK(bulbasaur.attackStage == 0);
}

void substituteFailsWithoutEnoughHpToSpareOrIfAlreadyUp() {
  BattleCombatant bulbasaur = makeCombatant(1, 30, {164});
  bulbasaur.currentHp = 1;  // not enough to spare 1/4 max HP
  BattleCombatant dummy = makeCombatant(4, 5, {45});
  const pokemon::BattleTurnResult result = pokemon::stepBattle(bulbasaur, dummy, 0, ZERO_RANDOM);
  CHECK(result.player.event == BattleLogEvent::MoveNoEffect);
  CHECK(bulbasaur.substituteHp == 0);
}

// --- Conversion (160), Mimic (102), Metronome (118), Mirror Move (119),
// Transform (144) - the last group of the Gen 1 mechanics gaps audit. ---

void conversionCopiesTheDefendersEffectiveTypeOntoTheAttacker() {
  BattleCombatant charmander = makeCombatant(4, 30, {160});  // Conversion
  BattleCombatant squirtle = makeCombatant(7, 5, {33});      // pure Water, no secondary type
  const pokemon::BattleTurnResult result = pokemon::stepOpponentOnlyTurn(squirtle, charmander, ZERO_RANDOM);
  CHECK(result.opponent.event == BattleLogEvent::ConversionApplied);
  CHECK(charmander.conversionType1 == PokemonType::Water);
  CHECK(charmander.conversionType2 == PokemonType::None);
}

void conversionAppliesStabForTheNewTypeOnDamagingMoves() {
  // Bulbasaur (Grass/Poison) using Water Gun (Water) normally gets no STAB;
  // manually simulating "just used Conversion against a Water-type target"
  // (see BattleCombatant::conversionType1's doc comment) should make
  // computeDamage() apply STAB for it instead, dealing strictly more damage
  // than an otherwise-identical unconverted attacker.
  BattleCombatant converted = makeCombatant(1, 20, {55});  // Water Gun
  converted.conversionType1 = PokemonType::Water;
  BattleCombatant unconverted = makeCombatant(1, 20, {55});
  BattleCombatant defenderForConverted = makeCombatant(4, 50, {33});
  BattleCombatant defenderForUnconverted = makeCombatant(4, 50, {33});
  pokemon::stepOpponentOnlyTurn(defenderForConverted, converted, ZERO_RANDOM);
  pokemon::stepOpponentOnlyTurn(defenderForUnconverted, unconverted, ZERO_RANDOM);
  const uint16_t convertedDamage = defenderForConverted.maxHp - defenderForConverted.currentHp;
  const uint16_t unconvertedDamage = defenderForUnconverted.maxHp - defenderForUnconverted.currentHp;
  CHECK(convertedDamage > unconvertedDamage);
}

void leechSeedFailsAgainstAConvertedGrassTypeTarget() {
  BattleCombatant bulbasaur = makeCombatant(1, 30, {73});  // Leech Seed
  BattleCombatant charmander = makeCombatant(4, 5, {33});
  charmander.conversionType1 = PokemonType::Grass;  // pretend it just converted to Grass
  pokemon::stepOpponentOnlyTurn(charmander, bulbasaur, ZERO_RANDOM);
  CHECK(!charmander.seeded);
}

void mimicCopiesOneOfTheDefendersMovesWithFivePpIntoTheUsersSlot() {
  BattleCombatant mimicUser = makeCombatant(4, 30, {102});  // Mimic only
  BattleCombatant tackler = makeCombatant(7, 5, {33});      // Tackle only - deterministic pick
  const uint8_t mimicBasePp = pokemon::moveData(102)->pp;
  const pokemon::BattleTurnResult result = pokemon::stepOpponentOnlyTurn(tackler, mimicUser, ZERO_RANDOM);
  CHECK(result.opponent.event == BattleLogEvent::MimicCopied);
  CHECK(result.opponent.redirectedMoveId == 33);
  CHECK(mimicUser.moves[0].moveId == 33);
  CHECK(mimicUser.moves[0].currentPp == 5);
  CHECK(mimicUser.mimicActive);
  CHECK(mimicUser.mimicSlot == 0);
  CHECK(mimicUser.mimicOriginalPp == mimicBasePp - 1U);
}

void mimicFailsWhenTheDefenderHasNoOtherMoveToCopy() {
  BattleCombatant mimicUser = makeCombatant(4, 30, {102});
  BattleCombatant otherMimicUser = makeCombatant(7, 5, {102});  // only knows Mimic itself - no valid candidate
  const pokemon::BattleTurnResult result = pokemon::stepOpponentOnlyTurn(otherMimicUser, mimicUser, ZERO_RANDOM);
  CHECK(result.opponent.event == BattleLogEvent::MoveNoEffect);
  CHECK(!mimicUser.mimicActive);
}

void transformCopiesSpeciesStatStagesAndMovesetButKeepsHpLevelAndStatus() {
  BattleCombatant charmander = makeCombatant(4, 20, {144});  // Transform
  BattleCombatant squirtle = makeCombatant(7, 20, {55});     // Water Gun
  squirtle.attackStage = 2;
  const uint16_t originalHp = charmander.currentHp;
  const uint8_t originalLevel = charmander.level;
  const pokemon::BattleTurnResult result = pokemon::stepOpponentOnlyTurn(squirtle, charmander, ZERO_RANDOM);
  CHECK(result.opponent.event == BattleLogEvent::Transformed);
  CHECK(charmander.speciesId == 7);
  CHECK(charmander.attackStage == 2);
  CHECK(charmander.moves[0].moveId == 55);
  CHECK(charmander.moves[0].currentPp == 5);
  CHECK(charmander.currentHp == originalHp);
  CHECK(charmander.level == originalLevel);
  CHECK(charmander.transformed);
}

void transformFailsIfAlreadyTransformedThisBattle() {
  BattleCombatant charmander = makeCombatant(4, 20, {144});
  charmander.transformed = true;
  BattleCombatant squirtle = makeCombatant(7, 20, {55});
  const pokemon::BattleTurnResult result = pokemon::stepOpponentOnlyTurn(squirtle, charmander, ZERO_RANDOM);
  CHECK(result.opponent.event == BattleLogEvent::MoveFailed);
  CHECK(charmander.speciesId == 4);
}

void mirrorMoveReplaysTheLastMoveUsedAgainstTheUser() {
  BattleCombatant mirrorUser = makeCombatant(4, 30, {119});  // Mirror Move only
  BattleCombatant tackler = makeCombatant(7, 5, {33});       // Tackle only
  // Turn 1: the tackler hits mirrorUser with Tackle, recording it.
  pokemon::stepOpponentOnlyTurn(mirrorUser, tackler, ZERO_RANDOM);
  CHECK(mirrorUser.lastMoveUsedAgainstMe == 33);
  const uint16_t tacklerHpBefore = tackler.currentHp;
  // Turn 2: mirrorUser replays Tackle back at the tackler.
  const pokemon::BattleTurnResult result = pokemon::stepOpponentOnlyTurn(tackler, mirrorUser, ZERO_RANDOM);
  CHECK(result.opponent.redirectedMoveId == 33);
  CHECK(tackler.currentHp < tacklerHpBefore);
}

void mirrorMoveFailsWithNothingRecordedYet() {
  BattleCombatant mirrorUser = makeCombatant(4, 30, {119});
  BattleCombatant dummy = makeCombatant(7, 5, {33});
  const pokemon::BattleTurnResult result = pokemon::stepOpponentOnlyTurn(dummy, mirrorUser, ZERO_RANDOM);
  CHECK(result.opponent.event == BattleLogEvent::MoveFailed);
  CHECK(result.opponent.redirectedMoveId == 0);
}

void metronomeExecutesADifferentMovesEffectInsteadOfItsOwn() {
  // fixedRoll's context feeds every roll Metronome's redirected move makes
  // too (see pickRandomMetronomeMove()'s doc comment) - 32 as the very first
  // roll (out of MOVE_COUNT=165) lands on move id 33 (Tackle), an ordinary
  // damaging move with nothing move-copying-specific about it.
  uint32_t seed = 32;
  const RandomSource seeded{&seed, fixedRoll};
  BattleCombatant metronomeUser = makeCombatant(4, 30, {118});  // Metronome only
  BattleCombatant target = makeCombatant(7, 30, {45});          // Growl - harmless filler
  const uint16_t targetHpBefore = target.currentHp;
  const pokemon::BattleTurnResult result = pokemon::stepOpponentOnlyTurn(target, metronomeUser, seeded);
  CHECK(result.opponent.redirectedMoveId == 33);
  CHECK(result.opponent.event != BattleLogEvent::MoveNoEffect);
  CHECK(result.opponent.event != BattleLogEvent::MoveFailed);
  CHECK(target.currentHp < targetHpBefore);
}

void metronomeSelectedTrapMoveDoesNotLockTheAttackerIn() {
  // Context 19 -> move id 20 (Wrap), one of the 4 real partial-trapping
  // moves - a normal slot-based use would lock the attacker into repeating
  // it (see isTrapMove()'s doc comment), but a Metronome-redirected one
  // shouldn't, since there's no real slot to keep "choosing" it from.
  uint32_t seed = 19;
  const RandomSource seeded{&seed, fixedRoll};
  BattleCombatant metronomeUser = makeCombatant(4, 30, {118});
  BattleCombatant target = makeCombatant(7, 30, {45});
  pokemon::stepOpponentOnlyTurn(target, metronomeUser, seeded);
  CHECK(metronomeUser.forcedMoveId == 0);
}

// An end-of-turn poison/burn/Leech Seed tick used to overwrite the action's
// event unconditionally, but Teleported/ForcedSwitch are what
// PokemonActivity.cpp acts on: a poisoned Pokemon's Teleport (or Whirlwind)
// silently did nothing while still spending its PP.
void endOfTurnStatusTickDoesNotOverwriteTeleportOrForcedSwitch() {
  BattleCombatant teleporter = makeCombatant(4, 30, {100});  // Teleport
  teleporter.status = Ailment::Poison;
  BattleCombatant target = makeCombatant(7, 30, {45});
  const pokemon::BattleTurnResult teleported = pokemon::stepPlayerOnlyTurn(teleporter, target, 0, ZERO_RANDOM);
  CHECK(teleported.player.event == BattleLogEvent::Teleported);

  BattleCombatant roarer = makeCombatant(4, 30, {18});  // Whirlwind
  roarer.status = Ailment::Poison;
  BattleCombatant victim = makeCombatant(7, 30, {45});
  const pokemon::BattleTurnResult forced = pokemon::stepPlayerOnlyTurn(roarer, victim, 0, ZERO_RANDOM);
  CHECK(forced.player.event == BattleLogEvent::ForcedSwitch);

  // A plain tick with nothing to preserve is still reported.
  BattleCombatant plain = makeCombatant(4, 30, {45});  // Growl
  plain.status = Ailment::Poison;
  BattleCombatant other = makeCombatant(7, 30, {45});
  CHECK(pokemon::stepPlayerOnlyTurn(plain, other, 0, ZERO_RANDOM).player.event == BattleLogEvent::StatusDamage);
}

// resolveAction() zeroes the user's HP for Self-Destruct/Explosion itself, but
// a Metronome that redirected to one skipped that and left the user standing.
void metronomeRedirectedToExplosionFaintsTheUser() {
  uint32_t seed = 152;  // -> move id 153 (Explosion)
  const RandomSource seeded{&seed, fixedRoll};
  BattleCombatant metronomeUser = makeCombatant(4, 30, {118});
  BattleCombatant target = makeCombatant(7, 30, {45});
  const pokemon::BattleTurnResult result = pokemon::stepOpponentOnlyTurn(target, metronomeUser, seeded);
  CHECK(result.opponent.redirectedMoveId == 153);
  CHECK(metronomeUser.currentHp == 0);
}

// stepBattle() already gave both sides the faint cleanup on a mutual KO; the
// single-actor steps (the other side skipped its action for an item / AI heal
// / switch) only cleaned up the side they checked, so the exploding side kept
// its status and Toxic counter and a later Revive brought it back poisoned.
void mutualKoFromTheSingleActorStepsCleansUpBothSides() {
  BattleCombatant exploder = makeCombatant(4, 30, {120});  // Self-Destruct
  exploder.status = Ailment::Poison;
  BattleCombatant weak = makeCombatant(7, 5, {45});
  weak.currentHp = 1;
  const pokemon::BattleTurnResult opponentActs = pokemon::stepOpponentOnlyTurn(weak, exploder, ZERO_RANDOM);
  CHECK(opponentActs.outcome == pokemon::BattleOutcome::OpponentWon);  // the attacker's action caused it
  CHECK(weak.currentHp == 0 && exploder.currentHp == 0);
  CHECK(exploder.status == Ailment::None);
  CHECK(exploder.toxicCounter == 0);
  CHECK(opponentActs.opponent.event == BattleLogEvent::Fainted);

  BattleCombatant playerExploder = makeCombatant(4, 30, {120});
  playerExploder.status = Ailment::Poison;
  BattleCombatant weakOpponent = makeCombatant(7, 5, {45});
  weakOpponent.currentHp = 1;
  const pokemon::BattleTurnResult playerActs =
      pokemon::stepPlayerOnlyTurn(playerExploder, weakOpponent, 0, ZERO_RANDOM);
  CHECK(playerActs.outcome == pokemon::BattleOutcome::PlayerWon);
  CHECK(playerExploder.currentHp == 0 && weakOpponent.currentHp == 0);
  CHECK(playerExploder.status == Ailment::None);
  CHECK(playerActs.player.event == BattleLogEvent::Fainted);
}

// Transform overwrites speciesId with the copied form, so a wild Ditto that
// transformed used to be caught at (and yield EVs of) the COPIED species.
void transformedWildCombatantIsCaughtAtItsRealSpeciesRate() {
  BattleCombatant ditto = makeCombatant(16, 20, {144});   // Pidgey (catch rate 255) using Transform
  BattleCombatant mewtwo = makeCombatant(150, 20, {45});  // catch rate 3
  pokemon::stepOpponentOnlyTurn(mewtwo, ditto, ZERO_RANDOM);
  CHECK(ditto.transformed);
  CHECK(ditto.speciesId == 150);
  CHECK(ditto.realSpeciesId() == 16);

  ditto.currentHp = 1;  // near-dead: the HP factor is maxed, so only the species rate decides
  uint32_t roll = 100;
  const RandomSource fixed{&roll, fixedRoll};
  CHECK(pokemon::attemptCatch(ditto, pokemon::BallKind::Poke,
                              fixed));  // 255 clears a 100 threshold; Mewtwo's 3 would not

  BattleCombatant realMewtwo = makeCombatant(150, 20, {45});
  realMewtwo.currentHp = 1;
  CHECK(!pokemon::attemptCatch(realMewtwo, pokemon::BallKind::Poke, fixed));  // an untransformed Mewtwo still fails
  CHECK(realMewtwo.realSpeciesId() == 150);
}

// Only the continuation turn that kills a trapped target used to release the
// trapper's lock; losing the target any other way (a poison tick here) left it
// auto-repeating the trap move against whatever came in next.
void trapperIsReleasedWhenTheTrappedTargetFaintsFromAStatusTick() {
  BattleCombatant trapper = makeCombatant(4, 30, {20});  // Wrap
  trapper.forcedMoveId = 20;
  trapper.forcedTurnsRemaining = 3;
  BattleCombatant target = makeCombatant(7, 30, {45});
  target.trappedTurnsRemaining = 4;
  target.status = Ailment::Poison;
  target.currentHp = 1;
  const pokemon::BattleTurnResult result = pokemon::stepOpponentOnlyTurn(trapper, target, ZERO_RANDOM);
  CHECK(result.outcome == pokemon::BattleOutcome::PlayerWon);  // the poison tick finished the target
  CHECK(target.currentHp == 0);
  CHECK(trapper.forcedMoveId == 0);
  CHECK(trapper.forcedTurnsRemaining == 0);
}

// --- Round-2 Gen 1 authenticity fixes: Burn halving Attack, Dream Eater's
// sleep requirement, Hyper Beam's recharge turn, Rage, Thrash/Petal Dance,
// and Wrap/Bind/Fire Spin/Clamp trapping the TARGET too (not just locking
// the attacker in). ---

void burnHalvesAttackerAttackForPhysicalMoves() {
  BattleCombatant burned = makeCombatant(4, 20, {33});  // Tackle
  burned.status = Ailment::Burn;
  BattleCombatant healthy = makeCombatant(4, 20, {33});
  BattleCombatant defenderForBurned = makeCombatant(7, 50, {45});
  BattleCombatant defenderForHealthy = makeCombatant(7, 50, {45});
  pokemon::stepOpponentOnlyTurn(defenderForBurned, burned, ZERO_RANDOM);
  pokemon::stepOpponentOnlyTurn(defenderForHealthy, healthy, ZERO_RANDOM);
  const uint16_t burnedDamage = defenderForBurned.maxHp - defenderForBurned.currentHp;
  const uint16_t healthyDamage = defenderForHealthy.maxHp - defenderForHealthy.currentHp;
  CHECK(burnedDamage < healthyDamage);
}

void burnDoesNotAffectSpecialMoveDamage() {
  BattleCombatant burned = makeCombatant(7, 20, {55});  // Water Gun (Special)
  burned.status = Ailment::Burn;
  BattleCombatant healthy = makeCombatant(7, 20, {55});
  BattleCombatant defenderForBurned = makeCombatant(4, 50, {45});
  BattleCombatant defenderForHealthy = makeCombatant(4, 50, {45});
  pokemon::stepOpponentOnlyTurn(defenderForBurned, burned, ZERO_RANDOM);
  pokemon::stepOpponentOnlyTurn(defenderForHealthy, healthy, ZERO_RANDOM);
  CHECK(defenderForBurned.currentHp == defenderForHealthy.currentHp);
}

void dreamEaterFailsUnlessTargetIsAsleep() {
  BattleCombatant eater = makeCombatant(1, 30, {138});  // Dream Eater
  BattleCombatant awakeTarget = makeCombatant(4, 30, {45});
  const pokemon::BattleTurnResult result = pokemon::stepOpponentOnlyTurn(awakeTarget, eater, ZERO_RANDOM);
  CHECK(result.opponent.event == BattleLogEvent::MoveFailed);
  CHECK(awakeTarget.currentHp == awakeTarget.maxHp);
}

void dreamEaterDamagesAndDrainsASleepingTarget() {
  BattleCombatant eater = makeCombatant(1, 30, {138});
  eater.currentHp = static_cast<uint16_t>(eater.maxHp / 2U);
  BattleCombatant sleepingTarget = makeCombatant(4, 30, {45});
  sleepingTarget.status = Ailment::Sleep;
  sleepingTarget.statusTurns = 5;
  const uint16_t eaterHpBefore = eater.currentHp;
  const pokemon::BattleTurnResult result = pokemon::stepOpponentOnlyTurn(sleepingTarget, eater, ZERO_RANDOM);
  CHECK(result.opponent.event != BattleLogEvent::MoveFailed);
  CHECK(sleepingTarget.currentHp < sleepingTarget.maxHp);
  CHECK(eater.currentHp > eaterHpBefore);
}

void hyperBeamForcesARechargeTurnAfterHitting() {
  BattleCombatant hyperBeamer = makeCombatant(4, 30, {63});  // Hyper Beam
  BattleCombatant target = makeCombatant(7, 100, {45});      // durable enough to survive
  pokemon::stepOpponentOnlyTurn(target, hyperBeamer, ZERO_RANDOM);
  CHECK(hyperBeamer.mustRecharge);
  const uint16_t targetHpAfterFirstHit = target.currentHp;
  const pokemon::BattleTurnResult result = pokemon::stepOpponentOnlyTurn(target, hyperBeamer, ZERO_RANDOM);
  CHECK(result.opponent.event == BattleLogEvent::MustRecharge);
  CHECK(target.currentHp == targetHpAfterFirstHit);
  CHECK(!hyperBeamer.mustRecharge);
}

void rageRaisesAttackEachTimeItsUserIsHitWhileEnraged() {
  // Same level for both - Charmander's higher base Speed acts first (uses
  // Rage), then Squirtle's own Tackle (its only usable move, so the AI pick
  // is deterministic) lands on the now-enraged Charmander. Same level also
  // means Rage's low power doesn't risk one-shotting Squirtle before it
  // gets to act.
  BattleCombatant rager = makeCombatant(4, 50, {99});  // Rage - the player's explicit slot 0
  BattleCombatant attacker2 = makeCombatant(7, 50, {33});
  pokemon::stepBattle(rager, attacker2, 0, ZERO_RANDOM);
  CHECK(rager.enraged);
  CHECK(rager.attackStage == 1);
}

void rageRaisesAttackWhenAReleasedBideHitsTheEnragedPokemon() {
  // Bulbasaur (level 80, faster) releases a stored Bide on its turn; Charmander
  // was enraged going in. Charmander's own follow-up move (Growl) then ends the
  // rage, but the +1 Attack from being hit stays. Confusion's self-hit is not
  // an opposing move, so it is deliberately not covered here.
  BattleCombatant bulbasaur = makeCombatant(1, 80, {117});  // Bide, one turn from releasing
  bulbasaur.bideTurnsRemaining = 1;
  bulbasaur.bideDamageStored = 20;
  BattleCombatant charmander = makeCombatant(4, 50, {45});  // Growl - harmless filler
  charmander.enraged = true;
  pokemon::stepBattle(bulbasaur, charmander, 0, MAX_RANDOM);
  CHECK(charmander.attackStage == 1);
}

void aReleasedBideDoesNotRaiseAttackOfAPokemonThatIsNotEnraged() {
  BattleCombatant bulbasaur = makeCombatant(1, 80, {117});
  bulbasaur.bideTurnsRemaining = 1;
  bulbasaur.bideDamageStored = 20;
  BattleCombatant charmander = makeCombatant(4, 50, {45});
  pokemon::stepBattle(bulbasaur, charmander, 0, MAX_RANDOM);
  CHECK(charmander.attackStage == 0);
}

void ragingEndsAsSoonAsADifferentMoveIsChosen() {
  BattleCombatant rager = makeCombatant(4, 50, {99, 33});  // slot 0 Rage, slot 1 Tackle
  rager.enraged = true;
  rager.attackStage = 2;
  BattleCombatant dummy = makeCombatant(7, 5, {45});
  pokemon::stepBattle(rager, dummy, 1, ZERO_RANDOM);  // explicitly picks Tackle, not Rage
  CHECK(!rager.enraged);
}

void thrashLocksTheUserForTwoTurnsThenConfusesIt() {
  BattleCombatant thrasher = makeCombatant(4, 50, {37});  // Thrash
  BattleCombatant target = makeCombatant(7, 50, {45});
  pokemon::stepOpponentOnlyTurn(target, thrasher, ZERO_RANDOM);
  CHECK(thrasher.forcedMoveId == 37);
  CHECK(thrasher.forcedTurnsRemaining == 1);
  pokemon::stepOpponentOnlyTurn(target, thrasher, ZERO_RANDOM);
  CHECK(thrasher.forcedMoveId == 0);
  CHECK(thrasher.status == Ailment::Confusion);
}

void wrapImmobilizesTheTargetWhileTrapped() {
  BattleCombatant wrapper = makeCombatant(4, 50, {35});  // Wrap
  BattleCombatant target = makeCombatant(7, 50, {45});   // Growl - would lower wrapper's Attack if it ran
  pokemon::stepOpponentOnlyTurn(target, wrapper, ZERO_RANDOM);
  CHECK(target.trappedTurnsRemaining > 0);
  const pokemon::BattleTurnResult result = pokemon::stepBattle(target, wrapper, 0, ZERO_RANDOM);
  CHECK(result.player.event == BattleLogEvent::Trapped);
  CHECK(wrapper.attackStage == 0);
}

void wrapReleasesTheTargetOnceTheAttackersLockEnds() {
  BattleCombatant wrapper = makeCombatant(4, 50, {35});
  BattleCombatant target = makeCombatant(7, 50, {45});
  pokemon::stepOpponentOnlyTurn(target, wrapper, ZERO_RANDOM);
  int guard = 0;
  while (wrapper.forcedMoveId != 0 && guard++ < 10) {
    pokemon::stepBattle(target, wrapper, 0, ZERO_RANDOM);
  }
  CHECK(target.trappedTurnsRemaining == 0);
}

// --- Round-3 Gen 1 authenticity fixes: confusion self-hit's real damage
// formula, Splash-style no-op status moves, and Substitute blocking the
// round-2 target-trapping effect. ---

void confusionSelfHitUsesTheRealDamageFormulaNotAFlatMaxHpFraction() {
  BattleCombatant confused = makeCombatant(4, 30, {33});  // Tackle - irrelevant, self-hit preempts it
  confused.status = Ailment::Confusion;
  confused.statusTurns = 3;
  BattleCombatant dummy = makeCombatant(7, 5, {45});
  const pokemon::BattleTurnResult result = pokemon::stepOpponentOnlyTurn(dummy, confused, ZERO_RANDOM);
  CHECK(result.opponent.event == BattleLogEvent::ConfusionSelfHit);
  const uint16_t selfDamage = confused.maxHp - confused.currentHp;
  const uint16_t oldFlatFormula = std::max<uint16_t>(1, static_cast<uint16_t>(confused.maxHp / 8U));
  CHECK(selfDamage > 0);
  CHECK(selfDamage != oldFlatFormula);
}

void splashReportsNothingHappenedInsteadOfMoveHit() {
  BattleCombatant splasher = makeCombatant(4, 30, {150});  // Splash
  BattleCombatant dummy = makeCombatant(7, 30, {45});
  const pokemon::BattleTurnResult result = pokemon::stepOpponentOnlyTurn(dummy, splasher, ZERO_RANDOM);
  CHECK(result.opponent.event == BattleLogEvent::NothingHappened);
}

void substituteBlocksTheTargetImmobilizationFromATrapMove() {
  BattleCombatant wrapper = makeCombatant(4, 50, {35});  // Wrap
  BattleCombatant target = makeCombatant(7, 50, {45});
  target.substituteHp = 10;
  pokemon::stepOpponentOnlyTurn(target, wrapper, ZERO_RANDOM);
  CHECK(wrapper.forcedMoveId == 35);         // the attacker side still locks in
  CHECK(target.trappedTurnsRemaining == 0);  // but the real Pokemon was never touched
}

// --- Round-6 audit bug 2.3: Substitute must block a secondary consequence
// (primary status infliction, flinch, secondary stat-drop) on the exact hit
// that breaks it, not just on hits after it's already gone - previously
// these 3 checks looked at the POST-hit substituteHp (already 0 once
// broken) instead of a pre-hit snapshot, matching the pattern the trap-
// immobilization check above already got right. ---

void substituteBlocksAilmentInflictionOnTheHitThatBreaksIt() {
  // Ember (10% burn chance, always rolls true under ZERO_RANDOM) breaking a
  // 1-HP Substitute must not still burn the real Pokemon behind it.
  BattleCombatant attacker = makeCombatant(4, 50, {52});  // Charmander, Ember
  BattleCombatant defender = makeCombatant(7, 50, {45});  // Squirtle, Growl (unused)
  defender.substituteHp = 1;
  pokemon::stepOpponentOnlyTurn(defender, attacker, ZERO_RANDOM);
  CHECK(defender.substituteHp == 0);        // the hit broke it
  CHECK(defender.status == Ailment::None);  // but the real Pokemon was never touched
}

void substituteBlocksFlinchOnTheHitThatBreaksIt() {
  // Stomp (30% flinch chance, always rolls true under ZERO_RANDOM) breaking
  // a 1-HP Substitute must not still flinch the real Pokemon.
  BattleCombatant attacker = makeCombatant(4, 50, {23});  // Charmander, Stomp
  BattleCombatant defender = makeCombatant(7, 50, {45});
  defender.substituteHp = 1;
  pokemon::stepOpponentOnlyTurn(defender, attacker, ZERO_RANDOM);
  CHECK(defender.substituteHp == 0);
  CHECK(!defender.flinched);
}

void substituteBlocksSecondaryStatDropOnTheHitThatBreaksIt() {
  // Psychic (33% Special-drop chance, always rolls true under ZERO_RANDOM)
  // breaking a 1-HP Substitute must not still lower the real Pokemon's
  // Special stage.
  BattleCombatant attacker = makeCombatant(1, 50, {94});  // Bulbasaur, Psychic
  BattleCombatant defender = makeCombatant(7, 50, {45});  // Squirtle
  defender.substituteHp = 1;
  pokemon::stepOpponentOnlyTurn(defender, attacker, ZERO_RANDOM);
  CHECK(defender.substituteHp == 0);
  CHECK(defender.specialStage == 0);
}

// --- Round-6 audit bug 2.4: a trapped target is released immediately if
// the Pokemon holding it in a partial trap (Wrap/Bind/Fire Spin/Clamp)
// faints mid-trap, instead of staying immobilized against whatever comes in
// next. ---

void faintingTrapperReleasesTheTrappedTargetImmediately() {
  // Wrapper is mid-continuing-Wrap (forcedMoveId already set, as if this
  // were its second or later turn locked into the move) and separately
  // poisoned at 1 HP, so the end-of-turn poison tick faints it this same
  // turn. Target is trapped and stays that way through the turn (it can't
  // act at all while trapped), so only the faint-driven release matters here.
  BattleCombatant wrapper = makeCombatant(4, 50, {35});  // Wrap
  wrapper.forcedMoveId = 35;
  wrapper.forcedTurnsRemaining = 2;
  wrapper.status = Ailment::Poison;
  wrapper.currentHp = 1;
  BattleCombatant target = makeCombatant(7, 50, {45});  // Growl - irrelevant, target is trapped
  target.trappedTurnsRemaining = 3;

  pokemon::stepBattle(target, wrapper, 0, ZERO_RANDOM);

  CHECK(wrapper.currentHp == 0);
  CHECK(target.trappedTurnsRemaining == 0);
}

// --- Round-4 Gen 1 authenticity fixes: real badge stat boosts and the
// real two-roll catch algorithm. ---

void badgeBoostRaisesTheCorrespondingStatByTwelvePointFivePercent() {
  BattleCombatant boosted = makeCombatant(4, 20, {33});  // Tackle (physical)
  boosted.badgeBoostMask = pokemon::BADGE_BOOST_ATTACK;
  BattleCombatant unboosted = makeCombatant(4, 20, {33});
  BattleCombatant defenderForBoosted = makeCombatant(7, 50, {45});
  BattleCombatant defenderForUnboosted = makeCombatant(7, 50, {45});
  pokemon::stepOpponentOnlyTurn(defenderForBoosted, boosted, ZERO_RANDOM);
  pokemon::stepOpponentOnlyTurn(defenderForUnboosted, unboosted, ZERO_RANDOM);
  const uint16_t boostedDamage = defenderForBoosted.maxHp - defenderForBoosted.currentHp;
  const uint16_t unboostedDamage = defenderForUnboosted.maxHp - defenderForUnboosted.currentHp;
  CHECK(boostedDamage > unboostedDamage);
}

void speedBadgeBoostCanFlipWhichSideActsFirst() {
  // Squirtle's own base Speed (43) is lower than Bulbasaur's (45); at the
  // same level, Bulbasaur normally acts first. Explosion faints its own
  // user unconditionally, so whichever side acts first here ends the turn
  // immediately - result.opponent.acted stays false if Bulbasaur never got
  // a turn (Squirtle went first), or becomes true if Bulbasaur's own
  // harmless Growl already fired before Squirtle's Explosion ended things.
  BattleCombatant boostedSquirtle = makeCombatant(7, 50, {153});  // Explosion
  boostedSquirtle.badgeBoostMask = pokemon::BADGE_BOOST_SPEED;
  BattleCombatant bulbasaurVsBoosted = makeCombatant(1, 50, {45});  // Growl
  const pokemon::BattleTurnResult boostedResult =
      pokemon::stepBattle(boostedSquirtle, bulbasaurVsBoosted, 0, ZERO_RANDOM);
  CHECK(!boostedResult.opponent.acted);

  BattleCombatant unboostedSquirtle = makeCombatant(7, 50, {153});
  BattleCombatant bulbasaurVsUnboosted = makeCombatant(1, 50, {45});
  const pokemon::BattleTurnResult unboostedResult =
      pokemon::stepBattle(unboostedSquirtle, bulbasaurVsUnboosted, 0, ZERO_RANDOM);
  CHECK(unboostedResult.opponent.acted);
}

void wildOrTrainerOpponentsNeverGetABadgeBoost() {
  // Sanity check: a fresh BattleCombatant (however setupBattleOpponent()
  // builds one) always defaults badgeBoostMask to 0 - only
  // PokemonActivity::setupBattlePlayer() ever sets it, and only from the
  // player's own earned badges.
  const BattleCombatant freshOpponent{};
  CHECK(freshOpponent.badgeBoostMask == 0);
}

// --- Round-8 Gen 1 authenticity fix: fainting clears status, matching real
// Gen 1 (0 HP means nothing left to be poisoned/paralyzed/burned/asleep
// about) - previously a status could persist through a faint and come back
// with the Pokemon after a Revive/Max Revive. ---

void faintingFromADirectHitClearsStatus() {
  BattleCombatant strong = makeCombatant(4, 100, {52});  // Ember, way overleveled - guaranteed KO
  BattleCombatant weak = makeCombatant(1, 2, {33});
  weak.status = Ailment::Poison;
  weak.statusTurns = 3;
  const pokemon::BattleTurnResult result = pokemon::stepBattle(strong, weak, 0, ZERO_RANDOM);
  CHECK(result.outcome == BattleOutcome::PlayerWon);
  CHECK(result.opponent.event == BattleLogEvent::Fainted);
  CHECK(weak.status == Ailment::None);
  CHECK(weak.statusTurns == 0);
}

void faintingFromAnEndOfTurnPoisonTickAlsoClearsStatus() {
  BattleCombatant player = makeCombatant(1, 20, {45});  // Growl - harmless filler, deals no damage
  player.status = Ailment::Poison;
  player.currentHp = 1;  // the end-of-turn poison tick alone finishes it off
  BattleCombatant opponent = makeCombatant(4, 20, {45});
  const pokemon::BattleTurnResult result = pokemon::stepBattle(player, opponent, 0, ZERO_RANDOM);
  CHECK(result.outcome == BattleOutcome::OpponentWon);
  CHECK(player.currentHp == 0);
  CHECK(player.status == Ailment::None);
}

// --- Round 3/4 Gen 1 authenticity audit fixes: type-based status immunity,
// 3 new multi-hit moves, secondary stat-drop moves, Razor Wind, Hyper Beam's
// recharge-skip-on-faint, Jump Kick/Hi Jump Kick crash damage, Teleport,
// move priority, Toxic's escalating damage, wild run-away odds, Haze's full
// reset, and the Speed-tie coin flip. ---

void fireTypeCannotBeBurned() {
  // Ember (10% burn chance in this dataset) on Charmander (Fire) - ZERO_RANDOM
  // would otherwise guarantee the burn roll succeeding, so a still-unburned
  // target proves the type immunity, not just bad luck.
  BattleCombatant attacker = makeCombatant(1, 30, {52});   // Bulbasaur, Ember
  BattleCombatant charmander = makeCombatant(4, 5, {33});  // Fire
  const pokemon::BattleTurnResult result = pokemon::stepBattle(attacker, charmander, 0, ZERO_RANDOM);
  CHECK(charmander.status == Ailment::None);
  CHECK(charmander.currentHp < charmander.maxHp);  // the damage itself still lands
}

void poisonTypeCannotBePoisoned() {
  // Poison Powder's ailment_chance is 0 (== "always" for a Status move in
  // this dataset), so an unpoisoned Ekans (pure Poison-type) proves the
  // immunity rather than a missed chance roll.
  BattleCombatant attacker = makeCombatant(1, 30, {77});  // Poison Powder
  BattleCombatant ekans = makeCombatant(23, 5, {45});     // pure Poison
  const pokemon::BattleTurnResult result = pokemon::stepBattle(attacker, ekans, 0, ZERO_RANDOM);
  CHECK(result.player.event == BattleLogEvent::MoveNoEffect);
  CHECK(ekans.status == Ailment::None);
}

void iceTypeCannotBeFrozen() {
  BattleCombatant attacker = makeCombatant(7, 30, {58});  // Ice Beam
  BattleCombatant jynx = makeCombatant(124, 5, {45});     // Ice/Psychic
  const pokemon::BattleTurnResult result = pokemon::stepBattle(attacker, jynx, 0, ZERO_RANDOM);
  CHECK(jynx.status == Ailment::None);
  CHECK(jynx.currentHp < jynx.maxHp);  // the damage itself still lands
}

void doubleKickAlwaysHitsExactlyTwice() {
  BattleCombatant attacker = makeCombatant(1, 30, {24});  // Bulbasaur, Double Kick
  BattleCombatant defender = makeCombatant(4, 60, {33});
  const pokemon::BattleTurnResult result = pokemon::stepBattle(attacker, defender, 0, MAX_RANDOM);
  CHECK(result.player.hitCount == 2);
}

void bonemerangAlwaysHitsExactlyTwice() {
  // Bonemerang's accuracy is 90 (not 100 like Double Kick/Twineedle), so
  // MAX_RANDOM's worst-case accuracy roll would make it whiff outright -
  // ZERO_RANDOM instead (which also guarantees a critical hit, hence the
  // attacker being the much-higher-level/faster side here, and the padded
  // defender HP, so both intended hits always land rather than the attacker
  // getting critically KO'd first or the defender fainting after only one).
  BattleCombatant attacker = makeCombatant(1, 60, {155});  // Bulbasaur, Bonemerang
  BattleCombatant defender = makeCombatant(4, 30, {33});
  defender.maxHp = 300;
  defender.currentHp = 300;
  const pokemon::BattleTurnResult result = pokemon::stepBattle(attacker, defender, 0, ZERO_RANDOM);
  CHECK(result.player.hitCount == 2);
}

void spikeCannonRollsTheRandomMultiHitDistribution() {
  // Same fixedRoll(7) trick as the existing Comet Punch test - lands in the
  // top 1/8 bucket, always exactly 5 hits, proving Spike Cannon uses the
  // real rolled distribution rather than a fixed count.
  uint32_t context = 7;
  const RandomSource fixedRandom{&context, fixedRoll};
  BattleCombatant attacker = makeCombatant(4, 30, {131});  // Charmander, Spike Cannon
  BattleCombatant defender = makeCombatant(1, 30, {33});
  defender.maxHp = 300;
  defender.currentHp = 300;
  const pokemon::BattleTurnResult result = pokemon::stepBattle(attacker, defender, 0, fixedRandom);
  CHECK(result.player.hitCount == 5);
}

void secondaryStatDropMovesCanLowerATargetsStat() {
  // Acid: a 10% chance to lower the target's Defense - ZERO_RANDOM guarantees
  // the roll succeeding (and, incidentally, a critical hit) - defender HP is
  // padded so the hit never faints it first (the stat-drop guard requires it
  // still be standing after the hit, same as the flinch roll's own guard).
  BattleCombatant attacker = makeCombatant(4, 50, {51});  // Acid
  BattleCombatant defender = makeCombatant(7, 50, {33});
  defender.maxHp = 300;
  defender.currentHp = 300;
  const pokemon::BattleTurnResult result = pokemon::stepBattle(attacker, defender, 0, ZERO_RANDOM);
  CHECK(result.player.statDropApplied);
  CHECK(result.player.statDropStat == StatKind::Defense);
  CHECK(defender.defenseStage == -1);
}

void psychicCanLowerTheTargetsSpecialStage() {
  BattleCombatant attacker = makeCombatant(4, 50, {94});  // Psychic
  BattleCombatant defender = makeCombatant(1, 50, {33});
  defender.maxHp = 300;
  defender.currentHp = 300;
  const pokemon::BattleTurnResult result = pokemon::stepBattle(attacker, defender, 0, ZERO_RANDOM);
  CHECK(result.player.statDropApplied);
  CHECK(result.player.statDropStat == StatKind::Special);
  CHECK(defender.specialStage == -1);
}

void secondaryStatDropIsAGenuineChanceNotAGuarantee() {
  // MAX_RANDOM rolls the worst-case (highest) percentage-chance roll, which
  // must fail Acid's 10% chance.
  BattleCombatant attacker = makeCombatant(4, 50, {51});  // Acid
  BattleCombatant defender = makeCombatant(7, 50, {33});
  const pokemon::BattleTurnResult result = pokemon::stepBattle(attacker, defender, 0, MAX_RANDOM);
  CHECK(!result.player.statDropApplied);
  CHECK(defender.defenseStage == 0);
}

void razorWindIsATwoTurnMoveWithNoInvulnerability() {
  BattleCombatant charmander = makeCombatant(4, 30, {13});  // Razor Wind
  BattleCombatant bulbasaur = makeCombatant(1, 5, {33});
  const uint16_t hpBeforeCharge = bulbasaur.currentHp;

  const pokemon::BattleTurnResult chargeResult = pokemon::stepBattle(charmander, bulbasaur, 0, ZERO_RANDOM);
  CHECK(chargeResult.player.event == BattleLogEvent::ChargingMove);
  CHECK(bulbasaur.currentHp == hpBeforeCharge);
  CHECK(charmander.forcedMoveId == 13);
  // Razor Wind does NOT grant invulnerability (unlike Fly/Dig) - Bulbasaur's
  // Tackle should still land normally during Charmander's charge turn.
  CHECK(charmander.currentHp < charmander.maxHp);

  const pokemon::BattleTurnResult releaseResult = pokemon::stepBattle(charmander, bulbasaur, 0, ZERO_RANDOM);
  CHECK(releaseResult.player.event != BattleLogEvent::ChargingMove);
  CHECK(bulbasaur.currentHp < hpBeforeCharge);
  CHECK(charmander.forcedMoveId == 0);
}

void hyperBeamSkipsRechargeWhenTheHitFaintsTheTarget() {
  BattleCombatant hyperBeamer = makeCombatant(4, 100, {63});  // Hyper Beam, way overleveled
  BattleCombatant target = makeCombatant(7, 2, {45});         // guaranteed to faint from one hit
  const pokemon::BattleTurnResult result = pokemon::stepOpponentOnlyTurn(target, hyperBeamer, ZERO_RANDOM);
  CHECK(result.outcome == BattleOutcome::OpponentWon);  // `target` (the player param) fainted
  CHECK(!hyperBeamer.mustRecharge);
}

void jumpKickDealsOneHpCrashDamageOnAMiss() {
  BattleCombatant attacker = makeCombatant(4, 30, {26});  // Jump Kick, 95 accuracy
  BattleCombatant defender = makeCombatant(7, 30, {45});
  const uint16_t hpBefore = attacker.currentHp;
  const pokemon::BattleTurnResult result = pokemon::stepBattle(attacker, defender, 0, MAX_RANDOM);
  CHECK(result.player.event == BattleLogEvent::MoveMissed);
  CHECK(result.player.recoilApplied);
  CHECK(attacker.currentHp == hpBefore - 1U);
}

void hiJumpKickAlsoDealsOneHpCrashDamageOnAMiss() {
  BattleCombatant attacker = makeCombatant(4, 30, {136});  // Hi Jump Kick, 90 accuracy
  BattleCombatant defender = makeCombatant(7, 30, {45});
  const uint16_t hpBefore = attacker.currentHp;
  const pokemon::BattleTurnResult result = pokemon::stepBattle(attacker, defender, 0, MAX_RANDOM);
  CHECK(result.player.event == BattleLogEvent::MoveMissed);
  CHECK(result.player.recoilApplied);
  CHECK(attacker.currentHp == hpBefore - 1U);
}

void ordinaryMoveMissNeverAppliesCrashDamage() {
  BattleCombatant attacker = makeCombatant(4, 30, {77});  // Poison Powder, 75 accuracy
  BattleCombatant defender = makeCombatant(7, 30, {45});
  const uint16_t hpBefore = attacker.currentHp;
  const pokemon::BattleTurnResult result = pokemon::stepBattle(attacker, defender, 0, MAX_RANDOM);
  CHECK(result.player.event == BattleLogEvent::MoveMissed);
  CHECK(!result.player.recoilApplied);
  CHECK(attacker.currentHp == hpBefore);
}

void teleportReportsTeleportedEvent() {
  // The engine can't tell a wild battle from a trainer one (see
  // BattleLogEvent::Teleported's doc comment) - PokemonActivity.cpp is
  // responsible for interpreting this as an escape (wild) or rewriting it to
  // MoveFailed (trainer/gym), so the engine's own contract is just: Teleport
  // always reports this event once it doesn't miss.
  BattleCombatant attacker = makeCombatant(4, 30, {100});  // Teleport
  BattleCombatant defender = makeCombatant(7, 30, {45});
  const pokemon::BattleTurnResult result = pokemon::stepBattle(attacker, defender, 0, ZERO_RANDOM);
  CHECK(result.player.event == BattleLogEvent::Teleported);
}

void quickAttackAlwaysGoesFirstRegardlessOfSpeed() {
  // Squirtle (level 5, Quick Attack) is far slower than Charmander (level
  // 50, Tackle), but Quick Attack's real +1 priority must let it act first
  // anyway - proven by fainting Charmander (1 HP) before it ever gets to act.
  BattleCombatant slowAttacker = makeCombatant(7, 5, {98});
  BattleCombatant fastOpponent = makeCombatant(4, 50, {33});
  fastOpponent.currentHp = 1;
  const pokemon::BattleTurnResult result = pokemon::stepBattle(slowAttacker, fastOpponent, 0, ZERO_RANDOM);
  CHECK(result.outcome == BattleOutcome::PlayerWon);
  CHECK(!result.opponent.acted);
}

void counterAlwaysResolvesLastRegardlessOfSpeedAdvantage() {
  // Squirtle (Counter) is much faster than Charmander (Tackle), but
  // Counter's real -1 priority means Charmander's Tackle must still resolve
  // FIRST, giving Counter something to reflect instead of failing.
  BattleCombatant fastCounterUser = makeCombatant(7, 50, {68});
  BattleCombatant slowAttacker = makeCombatant(4, 5, {33});
  const pokemon::BattleTurnResult result = pokemon::stepBattle(fastCounterUser, slowAttacker, 0, ZERO_RANDOM);
  CHECK(result.player.event != BattleLogEvent::MoveFailed);
}

void toxicDamageEscalatesEachTurnItTicks() {
  BattleCombatant player = makeCombatant(4, 50, {45});    // Growl filler, deals no damage
  BattleCombatant opponent = makeCombatant(7, 50, {92});  // Toxic
  const pokemon::BattleTurnResult turn1 = pokemon::stepBattle(player, opponent, 0, ZERO_RANDOM);
  CHECK(player.status == Ailment::Poison);
  CHECK(player.toxicCounter == 2);  // set to 1 on infliction, then incremented by this same turn's tick
  const uint16_t hpAfterTurn1 = player.currentHp;
  const uint16_t expectedFirstTick = std::max<uint16_t>(1, static_cast<uint16_t>(player.maxHp / 16U));
  CHECK(player.maxHp - hpAfterTurn1 == expectedFirstTick);

  const pokemon::BattleTurnResult turn2 = pokemon::stepBattle(player, opponent, 0, ZERO_RANDOM);
  CHECK(player.toxicCounter == 3);
  const uint16_t expectedSecondTick = std::max<uint16_t>(1, static_cast<uint16_t>(player.maxHp * 2U / 16U));
  CHECK(hpAfterTurn1 - player.currentHp == expectedSecondTick);
  CHECK(expectedSecondTick > expectedFirstTick);  // the whole point: it escalates
}

void runAwayGuaranteedWhenFormulaExceedsTwoFiftyFive() {
  BattleCombatant fastPlayer = makeCombatant(4, 50, {33});
  const BattleCombatant zeroSpeedOpponent{};  // speciesId 0 -> effective Speed 0
  CHECK(pokemon::attemptRun(fastPlayer, zeroSpeedOpponent, 0, MAX_RANDOM));
}

void runAwayGuaranteedFailureWhenPlayerHasNoSpeed() {
  const BattleCombatant zeroSpeedPlayer{};  // speciesId 0 -> effective Speed 0, F == 0
  BattleCombatant fastOpponent = makeCombatant(4, 50, {33});
  CHECK(!pokemon::attemptRun(zeroSpeedPlayer, fastOpponent, 0, ZERO_RANDOM));
}

void runAwayCanFailAgainstAMuchFasterOpponent() {
  BattleCombatant slowPlayer = makeCombatant(1, 5, {33});                // low level -> low Speed
  BattleCombatant fastOpponent = makeCombatant(4, 60, {33});             // much higher level/Speed
  CHECK(!pokemon::attemptRun(slowPlayer, fastOpponent, 0, MAX_RANDOM));  // worst-case roll against low odds
}

void runAwayAttemptCountEventuallyGuaranteesEscape() {
  // The same matchup as the failing case above, but with enough prior failed
  // attempts (+30 per attempt) that F must exceed 255 regardless of RNG.
  BattleCombatant slowPlayer = makeCombatant(1, 5, {33});
  BattleCombatant fastOpponent = makeCombatant(4, 60, {33});
  CHECK(pokemon::attemptRun(slowPlayer, fastOpponent, 200, MAX_RANDOM));
}

void hazeClearsStatusConfusionScreensAndToxicCounterOnBothSides() {
  // Opponent uses Tackle (not a stat-changing move) rather than Growl -
  // Charmander (faster) resolves Haze first, and a Growl afterward would
  // re-lower hazer's just-reset Attack stage, muddying the assertion below.
  BattleCombatant hazer = makeCombatant(4, 50, {114});  // Charmander (faster), Haze
  hazer.attackStage = 3;
  hazer.reflectActive = true;
  BattleCombatant opponent = makeCombatant(7, 50, {33});  // Squirtle, Tackle
  opponent.defenseStage = -2;
  opponent.status = Ailment::Confusion;
  opponent.statusTurns = 2;
  opponent.toxicCounter = 3;
  opponent.lightScreenActive = true;
  opponent.mistActive = true;
  opponent.guardSpecActive = true;
  opponent.direHitActive = true;

  const pokemon::BattleTurnResult result = pokemon::stepBattle(hazer, opponent, 0, ZERO_RANDOM);

  CHECK(result.player.event == BattleLogEvent::StatsReset);
  CHECK(hazer.attackStage == 0);
  CHECK(!hazer.reflectActive);
  CHECK(opponent.defenseStage == 0);
  CHECK(opponent.status == Ailment::None);
  CHECK(opponent.toxicCounter == 0);
  CHECK(!opponent.lightScreenActive);
  CHECK(!opponent.mistActive);
  CHECK(!opponent.guardSpecActive);
  CHECK(!opponent.direHitActive);
}

void speedTieIsBrokenByACoinFlipInsteadOfAlwaysFavoringThePlayer() {
  // Identical species/level on both sides gives an exact Speed tie (and
  // equal move priority, both Explosion) - Explosion unconditionally faints
  // its own user, so whichever side's action actually ran ends the turn
  // immediately, a clean signal for which side the tie-break picked.
  BattleCombatant playerZero = makeCombatant(4, 50, {153});  // Explosion
  BattleCombatant opponentZero = makeCombatant(4, 50, {153});
  const pokemon::BattleTurnResult zeroResult = pokemon::stepBattle(playerZero, opponentZero, 0, ZERO_RANDOM);
  CHECK(!zeroResult.opponent.acted);  // coin roll 0 keeps this engine's old default: ties go to the player

  BattleCombatant playerMax = makeCombatant(4, 50, {153});
  BattleCombatant opponentMax = makeCombatant(4, 50, {153});
  const pokemon::BattleTurnResult maxResult = pokemon::stepBattle(playerMax, opponentMax, 0, MAX_RANDOM);
  CHECK(maxResult.opponent.acted);  // coin roll 1 now correctly lets the opponent win a tie instead
}

}  // namespace

// A wild encounter is over the moment the player Teleports away or blows the
// opponent out with Roar/Whirlwind: the opponent must not still get an attack in
// afterwards (it could otherwise defeat a player who had already left). A gym
// fight keeps the normal two-sided turn.
void wildTeleportOrRoarEndsTheEncounterBeforeTheOpponentActs() {
  for (const uint8_t leavingMove : {static_cast<uint8_t>(100), static_cast<uint8_t>(18)}) {  // Teleport, Whirlwind
    BattleCombatant player = makeCombatant(4, 40, {leavingMove});
    BattleCombatant wild = makeCombatant(7, 5, {33});  // slower, would hit back with Tackle
    const uint16_t playerHpBefore = player.currentHp;
    const pokemon::BattleTurnResult wildResult = pokemon::stepBattle(player, wild, 0, ZERO_RANDOM, true);
    CHECK(!wildResult.opponent.acted);
    CHECK(player.currentHp == playerHpBefore);
    CHECK(wildResult.outcome == BattleOutcome::InProgress);
    CHECK(wildResult.player.event == BattleLogEvent::Teleported ||
          wildResult.player.event == BattleLogEvent::ForcedSwitch);

    BattleCombatant gymPlayer = makeCombatant(4, 40, {leavingMove});
    BattleCombatant trainer = makeCombatant(7, 5, {33});
    const pokemon::BattleTurnResult gymResult = pokemon::stepBattle(gymPlayer, trainer, 0, ZERO_RANDOM, false);
    CHECK(gymResult.opponent.acted);
  }
}

// Recoil, draining and Counter/Bide work from the damage actually dealt, which
// cannot exceed the HP the target had left.
void recoilAndDrainAreCappedByTheTargetsRemainingHp() {
  uint32_t context = 70;
  const RandomSource fixedRandom{&context, fixedRoll};

  BattleCombatant attacker = makeCombatant(4, 20, {36});  // Take Down: recoil 1/4
  BattleCombatant target = makeCombatant(1, 20, {33});
  target.maxHp = 200;
  target.currentHp = 8;
  target.status = Ailment::Sleep;
  target.statusTurns = 5;
  const uint16_t attackerHpBefore = attacker.currentHp;
  const pokemon::BattleTurnResult recoilResult = pokemon::stepBattle(attacker, target, 0, fixedRandom);
  CHECK(target.currentHp == 0);
  CHECK(recoilResult.player.recoilApplied);
  CHECK(attackerHpBefore - attacker.currentHp == 2);  // 8 HP dealt / 4, not a quarter of the uncapped hit

  BattleCombatant drainer = makeCombatant(1, 30, {72});  // Mega Drain (Grass) vs a Water type
  drainer.currentHp = static_cast<uint16_t>(drainer.maxHp - 30);
  BattleCombatant prey = makeCombatant(7, 5, {33});
  prey.maxHp = 200;
  prey.currentHp = 4;
  prey.status = Ailment::Sleep;
  prey.statusTurns = 5;
  const uint16_t drainerHpBefore = drainer.currentHp;
  const pokemon::BattleTurnResult drainResult = pokemon::stepBattle(drainer, prey, 0, fixedRandom);
  CHECK(prey.currentHp == 0);
  CHECK(drainResult.player.drainApplied);
  CHECK(drainer.currentHp - drainerHpBefore == 2);  // half of the 4 HP actually taken
}

// Property test: thousands of random fights (random species, levels, movesets,
// statuses, both wild and trainer) must never break a structural invariant of the
// engine, and the live player's state must always be something the battle store
// can persist (an entry the codec rejects means a silently lost save).
namespace fuzz {
uint64_t rngState = 88172645463325252ULL;
uint32_t nextRand() {
  rngState ^= rngState << 13;
  rngState ^= rngState >> 7;
  rngState ^= rngState << 17;
  return static_cast<uint32_t>(rngState >> 11);
}
uint32_t below(void*, const uint32_t n) { return n == 0 ? 0 : nextRand() % n; }
const RandomSource RNG{nullptr, below};
int reported = 0;

#define FUZZ_INV(cond, what)                                                                                       \
  do {                                                                                                             \
    if (!(cond)) {                                                                                                 \
      if (reported < 10) std::fprintf(stderr, "battle %ld turn %d: invariant broken: %s\n", battleId, turn, what); \
      ++reported;                                                                                                  \
      ++failures;                                                                                                  \
    }                                                                                                              \
  } while (false)

void checkSide(const BattleCombatant& c, const long battleId, const int turn) {
  FUZZ_INV(c.currentHp <= c.maxHp, "hp above max");
  FUZZ_INV(c.attackStage >= -6 && c.attackStage <= 6, "attack stage");
  FUZZ_INV(c.defenseStage >= -6 && c.defenseStage <= 6, "defense stage");
  FUZZ_INV(c.specialStage >= -6 && c.specialStage <= 6, "special stage");
  FUZZ_INV(c.speedStage >= -6 && c.speedStage <= 6, "speed stage");
  FUZZ_INV(c.accuracyStage >= -6 && c.accuracyStage <= 6, "accuracy stage");
  FUZZ_INV(c.evasionStage >= -6 && c.evasionStage <= 6, "evasion stage");
  if (c.currentHp == 0) {
    FUZZ_INV(c.status == Ailment::None, "fainted combatant keeps a status");
    FUZZ_INV(c.toxicCounter == 0, "fainted combatant keeps a toxic counter");
  }
  if (c.toxicCounter > 0) FUZZ_INV(c.status == Ailment::Poison, "toxic counter without poison");
  FUZZ_INV(c.substituteHp <= c.maxHp, "substitute above max hp");
  FUZZ_INV(c.trappedTurnsRemaining <= 8, "trap duration");
  FUZZ_INV(c.disableTurnsRemaining <= 5, "disable duration");
  if (c.forcedMoveId != 0) {
    bool known = false;
    for (const auto& move : c.moves) known = known || move.moveId == c.forcedMoveId;
    FUZZ_INV(known, "locked into a move it does not have");
  }
  if (!c.transformed && !c.mimicActive) {
    bool sawEmpty = false;
    for (size_t i = 0; i < pokemon::BATTLE_MOVE_SLOTS; ++i) {
      if (c.moves[i].moveId == 0) {
        sawEmpty = true;
        continue;
      }
      FUZZ_INV(!sawEmpty, "gap in the moveset");
      const pokemon::MoveData* data = pokemon::moveData(c.moves[i].moveId);
      FUZZ_INV(data != nullptr, "unknown move id");
      if (data != nullptr) FUZZ_INV(c.moves[i].currentPp <= pokemon::maxPpFor(data->pp, c.ppUp[i]), "pp above max");
    }
  }
}

// The fork checked here that the live battle state fits its battle save entry
// (BattleRecordEntry). That save layer is not carried over; put the check back
// against our own record format once battle state is saved.
void checkPersistable(const BattleCombatant& /*p*/, const long /*battleId*/, const int /*turn*/) {}

BattleCombatant make(const uint16_t species, const uint8_t level, const bool player) {
  BattleCombatant c{};
  c.speciesId = species;
  c.level = level;
  for (auto& v : c.iv) v = static_cast<uint8_t>(nextRand() % 16);
  if (player) {
    for (auto& v : c.ev) v = static_cast<uint8_t>(nextRand() % 256);
  }
  const pokemon::BaseStats* stats = pokemon::baseStatsFor(species);
  c.maxHp = pokemon::battleMaxHp(stats->hp, level, c.iv[0], c.ev[0]);
  c.currentHp = static_cast<uint16_t>(nextRand() % 3 == 0 ? c.maxHp : 1 + nextRand() % c.maxHp);
  size_t filled = 0;
  while (filled < pokemon::BATTLE_MOVE_SLOTS) {
    if (filled >= 1 && nextRand() % 4 == 0) break;
    const uint8_t id = static_cast<uint8_t>(1 + nextRand() % pokemon::POKEMON_MOVE_ID_MAX);
    const pokemon::MoveData* data = pokemon::moveData(id);
    if (data == nullptr) continue;
    bool duplicate = false;
    for (size_t j = 0; j < filled; ++j) duplicate = duplicate || c.moves[j].moveId == id;
    if (duplicate) continue;
    c.ppUp[filled] = static_cast<uint8_t>(nextRand() % 4);
    const uint8_t maxPp = pokemon::maxPpFor(data->pp, c.ppUp[filled]);
    c.moves[filled] = BattleMoveSlot{id, static_cast<uint8_t>(1 + nextRand() % maxPp)};
    ++filled;
  }
  static const Ailment statuses[] = {Ailment::None, Ailment::None,      Ailment::None,  Ailment::Poison,
                                     Ailment::Burn, Ailment::Paralysis, Ailment::Sleep, Ailment::Freeze};
  c.status = statuses[nextRand() % 8];
  if (c.status == Ailment::Sleep) c.statusTurns = static_cast<uint8_t>(1 + nextRand() % 3);
  return c;
}
}  // namespace fuzz

void randomFightsNeverBreakAnEngineInvariant() {
  using namespace fuzz;
  rngState = 88172645463325252ULL;
  reported = 0;
  for (long battleId = 0; battleId < 6000; ++battleId) {
    BattleCombatant player =
        make(static_cast<uint16_t>(1 + nextRand() % 151), static_cast<uint8_t>(2 + nextRand() % 99), true);
    BattleCombatant opponent =
        make(static_cast<uint16_t>(1 + nextRand() % 151), static_cast<uint8_t>(2 + nextRand() % 99), false);
    const bool wild = nextRand() % 2 != 0;
    for (int turn = 0; turn < 80; ++turn) {
      uint8_t slot = pokemon::BATTLE_MOVE_SLOTS;
      const uint8_t forced = player.bideTurnsRemaining > 0 ? pokemon::BIDE_MOVE_ID : player.forcedMoveId;
      bool chosen = false;
      if (forced != 0) {
        for (uint8_t i = 0; i < pokemon::BATTLE_MOVE_SLOTS; ++i) {
          if (player.moves[i].moveId == forced) {
            slot = i;
            chosen = true;
            break;
          }
        }
      }
      if (!chosen) {
        uint8_t usable[4];
        int count = 0;
        for (uint8_t i = 0; i < pokemon::BATTLE_MOVE_SLOTS; ++i) {
          if (player.moves[i].moveId != 0 && player.moves[i].currentPp > 0 &&
              !(player.disableTurnsRemaining > 0 && player.disabledMoveSlot == i)) {
            usable[count++] = i;
          }
        }
        slot = count == 0 ? pokemon::BATTLE_MOVE_SLOTS : usable[nextRand() % count];
      }
      const pokemon::BattleTurnResult result = pokemon::stepBattle(player, opponent, slot, RNG, wild);
      checkSide(player, battleId, turn);
      checkSide(opponent, battleId, turn);
      checkPersistable(player, battleId, turn);
      if (result.outcome == BattleOutcome::PlayerWon)
        FUZZ_INV(opponent.currentHp == 0, "PlayerWon with the opponent standing");
      if (result.outcome == BattleOutcome::OpponentWon)
        FUZZ_INV(player.currentHp == 0, "OpponentWon with the player standing");
      if (result.outcome == BattleOutcome::InProgress) {
        FUZZ_INV(player.currentHp > 0 && opponent.currentHp > 0, "fight in progress with a fainted side");
      }
      if (result.outcome != BattleOutcome::InProgress) break;
      if (wild &&
          (result.player.event == BattleLogEvent::Teleported || result.player.event == BattleLogEvent::ForcedSwitch))
        break;
    }
  }
}

// --- Trainer AI (BattleCombatant::aiSkill > 0) ------------------------------
// MAX_RANDOM keeps the AI from taking its random-move "mistake" (that roll
// lands on 99, above every skill's mistake percentage) and breaks score ties
// towards the last slot, so these isolate the scoring itself.

uint8_t trainerPick(BattleCombatant player, BattleCombatant opponent) {
  opponent.aiSkill = 3;
  return pokemon::stepOpponentOnlyTurn(player, opponent, MAX_RANDOM).opponent.moveSlot;
}

void trainerAiPrefersARealAttackOverASameTypeZeroPowerMove() {
  // Growl and Tackle are both Normal: the old type-only AI saw them as equals
  // (and MAX_RANDOM's tie-break lands on Growl in slot 1); the trainer AI
  // knows Growl deals no damage here.
  const BattleCombatant player = makeCombatant(4, 20, {33});
  const BattleCombatant wild = makeCombatant(7, 20, {33, 45});
  BattleCombatant legacyPlayer = player;
  BattleCombatant legacyWild = wild;
  CHECK(pokemon::stepOpponentOnlyTurn(legacyPlayer, legacyWild, MAX_RANDOM).opponent.moveSlot == 1);
  // A healthy target at -6 Attack makes Growl worthless outright.
  BattleCombatant cappedPlayer = player;
  cappedPlayer.attackStage = -6;
  CHECK(trainerPick(cappedPlayer, wild) == 0);
}

void trainerAiTakesTheMostAccurateKnockout() {
  // Both Hydro Pump (80% accurate) and Tackle knock out a 1 HP Charmander;
  // the trainer takes the sure one.
  BattleCombatant player = makeCombatant(4, 20, {33});
  player.currentHp = 1;
  const BattleCombatant squirtle = makeCombatant(7, 20, {56, 33});
  CHECK(trainerPick(player, squirtle) == 1);
}

void trainerAiOnlyUsesStatusMovesThatWouldWork() {
  // A healthy, unstatused Charmander: paralysing it is worth more than a
  // Tackle at this level.
  const BattleCombatant player = makeCombatant(4, 20, {33});
  const BattleCombatant pikachu = makeCombatant(25, 20, {86, 33});
  CHECK(trainerPick(player, pikachu) == 0);
  // Already paralysed: Thunder Wave would do nothing, so attack instead.
  BattleCombatant paralysed = player;
  paralysed.status = pokemon::Ailment::Paralysis;
  CHECK(trainerPick(paralysed, pikachu) == 1);
  // Ground types are immune to Thunder Wave (Geodude, 74).
  const BattleCombatant geodude = makeCombatant(74, 20, {33});
  CHECK(trainerPick(geodude, pikachu) == 1);
}

void trainerAiOnlyDreamEatsASleepingTarget() {
  const BattleCombatant awake = makeCombatant(4, 20, {33});
  const BattleCombatant drowzee = makeCombatant(96, 20, {138, 33});
  CHECK(trainerPick(awake, drowzee) == 1);
  BattleCombatant asleep = awake;
  asleep.status = pokemon::Ailment::Sleep;
  asleep.statusTurns = 3;
  CHECK(trainerPick(asleep, drowzee) == 0);
}

void trainerAiKeepsExplosionForWhenItIsAboutToFaint() {
  const BattleCombatant player = makeCombatant(4, 20, {33});
  BattleCombatant geodude = makeCombatant(74, 20, {153, 33});
  CHECK(trainerPick(player, geodude) == 1);
  geodude.currentHp = static_cast<uint16_t>(geodude.maxHp / 5U);
  CHECK(trainerPick(player, geodude) == 0);
}

void estimateMoveDamageHandlesImmunityFixedDamageAndEmptySlots() {
  const BattleCombatant pikachu = makeCombatant(25, 20, {84, 69});  // Thunder Shock, Seismic Toss
  const BattleCombatant geodude = makeCombatant(74, 20, {33});
  const BattleCombatant squirtle = makeCombatant(7, 20, {33});
  CHECK(pokemon::estimateMoveDamage(pikachu, geodude, 0) == 0);  // Electric vs Ground
  CHECK(pokemon::estimateMoveDamage(pikachu, squirtle, 0) > 0);
  CHECK(pokemon::estimateMoveDamage(pikachu, squirtle, 1) == 20);  // Seismic Toss = the user's level
  CHECK(pokemon::estimateMoveDamage(pikachu, squirtle, 2) == 0);   // empty slot
  CHECK(pokemon::bestExpectedDamage(pikachu, squirtle) >= pokemon::estimateMoveDamage(pikachu, squirtle, 0));
}

void eachActionRecordsBothSidesHpAfterItAndTheTurnOrder() {
  // Tackle both ways; the player's +6 Speed stage makes it hit first, and
  // each action's snapshot is taken before end-of-turn damage.
  BattleCombatant player = makeCombatant(1, 30, {33});
  BattleCombatant opponent = makeCombatant(4, 30, {33});
  player.speedStage = 6;
  opponent.speedStage = -6;
  const uint16_t playerHpBefore = player.currentHp;
  const uint16_t opponentHpBefore = opponent.currentHp;
  const pokemon::BattleTurnResult result = pokemon::stepBattle(player, opponent, 0, MAX_RANDOM);
  CHECK(result.playerFirst);
  CHECK(result.player.acted && result.opponent.acted);
  CHECK(result.player.playerHpAfter == playerHpBefore);  // the opponent had not hit yet
  CHECK(result.player.opponentHpAfter < opponentHpBefore);
  CHECK(result.opponent.opponentHpAfter == result.player.opponentHpAfter);
  CHECK(result.opponent.playerHpAfter == player.currentHp);

  // Roles reversed: the opponent acts first.
  BattleCombatant slow = makeCombatant(1, 30, {33});
  BattleCombatant fast = makeCombatant(4, 30, {33});
  slow.speedStage = -6;
  fast.speedStage = 6;
  const uint16_t slowHpBefore = slow.currentHp;
  const pokemon::BattleTurnResult reversed = pokemon::stepBattle(slow, fast, 0, MAX_RANDOM);
  CHECK(!reversed.playerFirst);
  CHECK(reversed.opponent.playerHpAfter < slowHpBefore);
  CHECK(reversed.opponent.opponentHpAfter == fast.maxHp);
  CHECK(reversed.player.playerHpAfter == reversed.opponent.playerHpAfter);

  BattleCombatant hurt = makeCombatant(1, 30, {33});
  BattleCombatant attacker = makeCombatant(4, 30, {33});
  CHECK(!pokemon::stepOpponentOnlyTurn(hurt, attacker, MAX_RANDOM).playerFirst);
}

int main() {
  statFormulasScaleWithLevel();
  eachActionRecordsBothSidesHpAfterItAndTheTurnOrder();
  damagingMoveReducesDefenderHpAndReportsSuperEffective();
  fireMoveIsNotVeryEffectiveAgainstWaterAndCanBurn();
  statusMoveWithZeroAilmentChanceAlwaysAppliesItsAilment();
  paralysisCanPreventAnActionAndSpeedIsHalved();
  poisonAndBurnTickAtEndOfTurn();
  aiVariesItsMoveChoiceAcrossRandomSeedsInsteadOfAlwaysTheSameSlot();
  faintingEndsTheBattleImmediatelyWithoutARetaliation();
  alreadyFaintedCombatantsShortCircuitToAnOutcome();
  opponentOnlyTurnActsRegardlessOfSpeedSinceThePlayerAlreadySpentTheTurn();
  opponentOnlyTurnCanFaintThePlayer();
  opponentOnlyTurnStillAppliesEndOfTurnStatusDamage();
  opponentOnlyTurnShortCircuitsWhenAlreadyFainted();
  playerOnlyTurnActsRegardlessOfSpeedSinceTheOpponentAlreadySpentTheTurn();
  playerOnlyTurnCanFaintTheOpponent();
  playerOnlyTurnStillAppliesEndOfTurnStatusDamage();
  playerOnlyTurnShortCircuitsWhenAlreadyFainted();
  masterBallAlwaysCatchesRegardlessOfRandomness();
  pokeBallCatchOddsScaleWithHpAndCaptureRate();
  criticalHitExactlyDoublesDamageForAHighCritRatioMove();
  applyStatStageMultiplierMatchesGen1Table();
  applyAccuracyEvasionStageMultiplierMatchesGen1Table();
  statChangeForMoveIdentifiesSelfBuffsAndOpponentDebuffs();
  selfBuffMoveRaisesAttackStageAndReportsEvent();
  opponentDebuffMoveLowersDefendersStageNotTheUsers();
  statChangeAtCapReportsFailureInsteadOfExceedingBounds();
  hazeResetsBothSidesStatStages();
  applyBattleBoostItemRaisesEachXStatByOneStageUpToTheCap();
  guardSpecItemActivatesOnceAndBlocksOpponentStatLoweringMoves();
  guardSpecDoesNotBlockSelfBuffingStatMoves();
  direHitItemActivatesOnceAndRaisesCritRatioToTheHighTier();
  applyBattleBoostItemRejectsAnUnknownItemId();
  battleBoostItemWouldApplyMatchesApplyWithoutMutating();
  speedStageCanFlipWhichSideActsFirst();
  accuracyStageLoweringCanCauseAMissThatWouldOtherwiseHit();
  evasionStageRaisingCanCauseAMissThatWouldOtherwiseHit();
  criticalHitIgnoresAttackersUnfavorableNegativeAttackStage();
  criticalHitIgnoresDefendersUnfavorablePositiveDefenseStage();
  recoilMoveDamagesTheAttackerAfterDealingDamage();
  nonRecoilMoveNeverAppliesRecoil();
  multiHitMoveConnectsMultipleTimesAndReportsHitCount();
  twineedleAlwaysHitsExactlyTwice();
  nonMultiHitMoveReportsZeroHitCount();
  multiHitMoveStopsEarlyIfTheDefenderFaintsPartway();
  multiHitMoveStopsAfterBreakingASubstituteInsteadOfHittingRealHp();
  forcedStruggleSentinelDealsDamageAndRecoilsWithNoLearnedMove();
  opponentStrugglesWhenAllOfItsLearnedMovesAreOutOfPp();
  ivZeroEvZeroReproducesTheOriginalFormulaExactly();
  ivAndEvRaiseStatsAboveTheZeroBaseline();
  evBonusMatchesTheFlatDivideByFourFormula();
  outOfRangeIvIsClampedToTheRealGen1Ceiling();
  rollIvSetProducesValuesInTheRealGen1Range();
  maxPpForMatchesTheRealGen1PpUpProgression();
  battleVictoryXpScalesWithTheLevelGap();
  ohkoMoveInstantlyFaintsWhenAttackerLevelIsAtLeastDefenders();
  ohkoMoveNeverConnectsWhenAttackerIsALowerLevel();
  ohkoMoveHasNoEffectOnAnImmuneType();
  seismicTossAndNightShadeDealDamageEqualToUsersLevel();
  dragonRageAndSonicBoomDealFixedFlatDamage();
  psywaveDamageIsBoundedBetweenOneAndOnePointFiveTimesLevel();
  superFangHalvesDefendersCurrentHp();
  lowKickDealsRealDamageInsteadOfTheOldFlatBug();
  counterReflectsDoubleTheLastPhysicalDamageTakenThisTurn();
  counterFailsWhenUserHasNotTakenPhysicalDamageThisTurn();
  flinchMoveCanPreventTheTargetsActionThisSameTurn();
  flinchDoesNotPersistPastTheTurnItWasInflicted();
  drainMoveHealsHalfTheDamageDealtAndCapsAtMaxHp();
  selfDestructMoveFaintsTheUserRegardlessOfHitOrMiss();
  selfDestructHalvesDefendersDefenseForThisHit();
  selfDestructCausesASimultaneousKoWhenItAlsoFaintsTheTargetAndCountsAsAWin();
  damagingMoveWithAilmentDoesNotInflictItAgainstAnImmuneType();
  statusMoveWithAilmentRespectsTypeImmunity();
  twoTurnMoveChargesThenReleasesOnTheFollowingTurn();
  flyGrantsInvulnerabilityDuringTheChargeTurnButSolarBeamDoesNot();
  twoTurnMoveReleasesEvenWhenItsLastPpWasSpentOnTheChargeTurn();
  trapMoveLocksTheAttackerIntoRepeatingItAndBypassesAccuracyOnFollowUpTurns();
  bideStoresDamageOverTwoTurnsThenReleasesDoubleItBack();
  leechSeedDrainsTheSeededSideAndHealsTheSeederAtEndOfTurn();
  leechSeedFailsAgainstAGrassTypeTarget();
  reflectExactlyHalvesNonCriticalPhysicalDamage();
  mistAndFocusEnergySetTheirOwnFields();
  recoverHealsHalfMaxHpAndFailsAtFullHealth();
  restAndSoftBoiledFailAtFullHealthWithoutSleeping();
  restFullyHealsCuresStatusAndSleepsForAFixedTwoTurns();
  whirlwindAndRoarAlwaysReportForcedSwitchOnUse();
  disableLocksOutARandomMoveWithPpAndFailsIfNoneQualify();
  disableCountsDownAndEventuallyReleases();
  disabledMoveIsSkippedByTheAiFallingBackToStruggle();
  substituteCostsAQuarterMaxHpAndAbsorbsDamageUntilItBreaks();
  substituteBlocksAnOpponentsStatLoweringMove();
  substituteFailsWithoutEnoughHpToSpareOrIfAlreadyUp();
  conversionCopiesTheDefendersEffectiveTypeOntoTheAttacker();
  conversionAppliesStabForTheNewTypeOnDamagingMoves();
  leechSeedFailsAgainstAConvertedGrassTypeTarget();
  mimicCopiesOneOfTheDefendersMovesWithFivePpIntoTheUsersSlot();
  mimicFailsWhenTheDefenderHasNoOtherMoveToCopy();
  transformCopiesSpeciesStatStagesAndMovesetButKeepsHpLevelAndStatus();
  transformFailsIfAlreadyTransformedThisBattle();
  mirrorMoveReplaysTheLastMoveUsedAgainstTheUser();
  mirrorMoveFailsWithNothingRecordedYet();
  metronomeExecutesADifferentMovesEffectInsteadOfItsOwn();
  metronomeSelectedTrapMoveDoesNotLockTheAttackerIn();
  endOfTurnStatusTickDoesNotOverwriteTeleportOrForcedSwitch();
  wildTeleportOrRoarEndsTheEncounterBeforeTheOpponentActs();
  randomFightsNeverBreakAnEngineInvariant();
  recoilAndDrainAreCappedByTheTargetsRemainingHp();
  metronomeRedirectedToExplosionFaintsTheUser();
  mutualKoFromTheSingleActorStepsCleansUpBothSides();
  transformedWildCombatantIsCaughtAtItsRealSpeciesRate();
  trapperIsReleasedWhenTheTrappedTargetFaintsFromAStatusTick();
  burnHalvesAttackerAttackForPhysicalMoves();
  burnDoesNotAffectSpecialMoveDamage();
  dreamEaterFailsUnlessTargetIsAsleep();
  dreamEaterDamagesAndDrainsASleepingTarget();
  hyperBeamForcesARechargeTurnAfterHitting();
  rageRaisesAttackEachTimeItsUserIsHitWhileEnraged();
  rageRaisesAttackWhenAReleasedBideHitsTheEnragedPokemon();
  aReleasedBideDoesNotRaiseAttackOfAPokemonThatIsNotEnraged();
  ragingEndsAsSoonAsADifferentMoveIsChosen();
  thrashLocksTheUserForTwoTurnsThenConfusesIt();
  wrapImmobilizesTheTargetWhileTrapped();
  wrapReleasesTheTargetOnceTheAttackersLockEnds();
  confusionSelfHitUsesTheRealDamageFormulaNotAFlatMaxHpFraction();
  splashReportsNothingHappenedInsteadOfMoveHit();
  substituteBlocksTheTargetImmobilizationFromATrapMove();
  substituteBlocksAilmentInflictionOnTheHitThatBreaksIt();
  substituteBlocksFlinchOnTheHitThatBreaksIt();
  substituteBlocksSecondaryStatDropOnTheHitThatBreaksIt();
  faintingTrapperReleasesTheTrappedTargetImmediately();
  mistBlocksOpponentStatLoweringMovesAndSecondaryStatDropsViaItsOwnField();
  badgeBoostRaisesTheCorrespondingStatByTwelvePointFivePercent();
  speedBadgeBoostCanFlipWhichSideActsFirst();
  wildOrTrainerOpponentsNeverGetABadgeBoost();
  faintingFromADirectHitClearsStatus();
  faintingFromAnEndOfTurnPoisonTickAlsoClearsStatus();
  fireTypeCannotBeBurned();
  poisonTypeCannotBePoisoned();
  iceTypeCannotBeFrozen();
  doubleKickAlwaysHitsExactlyTwice();
  bonemerangAlwaysHitsExactlyTwice();
  spikeCannonRollsTheRandomMultiHitDistribution();
  secondaryStatDropMovesCanLowerATargetsStat();
  psychicCanLowerTheTargetsSpecialStage();
  secondaryStatDropIsAGenuineChanceNotAGuarantee();
  razorWindIsATwoTurnMoveWithNoInvulnerability();
  hyperBeamSkipsRechargeWhenTheHitFaintsTheTarget();
  jumpKickDealsOneHpCrashDamageOnAMiss();
  hiJumpKickAlsoDealsOneHpCrashDamageOnAMiss();
  ordinaryMoveMissNeverAppliesCrashDamage();
  teleportReportsTeleportedEvent();
  quickAttackAlwaysGoesFirstRegardlessOfSpeed();
  counterAlwaysResolvesLastRegardlessOfSpeedAdvantage();
  toxicDamageEscalatesEachTurnItTicks();
  runAwayGuaranteedWhenFormulaExceedsTwoFiftyFive();
  runAwayGuaranteedFailureWhenPlayerHasNoSpeed();
  runAwayCanFailAgainstAMuchFasterOpponent();
  runAwayAttemptCountEventuallyGuaranteesEscape();
  hazeClearsStatusConfusionScreensAndToxicCounterOnBothSides();
  speedTieIsBrokenByACoinFlipInsteadOfAlwaysFavoringThePlayer();
  trainerAiPrefersARealAttackOverASameTypeZeroPowerMove();
  trainerAiTakesTheMostAccurateKnockout();
  trainerAiOnlyUsesStatusMovesThatWouldWork();
  trainerAiOnlyDreamEatsASleepingTarget();
  trainerAiKeepsExplosionForWhenItIsAboutToFaint();
  estimateMoveDamageHandlesImmunityFixedDamageAndEmptySlots();
  return failures == 0 ? 0 : 1;
}
