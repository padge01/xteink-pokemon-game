# Pokémon v2 handoff

Written 27 November 2026 to move the work to another machine. It records what
is decided, what is on each branch, and what to do next. Delete it, or fold it
into the design spec, before this branch merges.

## Branches

| Branch | Holds | State |
| --- | --- | --- |
| `main`, `legacy/0.1.x` | v0.1.1, the game on CrossInk 1.5 | Released |
| `port/crossink-2.0-v2` | The same game ported to CrossInk 2.0 (`release/v2.0.0`) | Builds, tests pass, checked in the simulator. Not released. |
| `feat/battle-engine` | The port plus the battle engine and battle data from the thebitsworld fork | Compiles and tests pass. Nothing in the game calls the engine yet. |

CrossInk is the `crossink` remote (`https://github.com/uxjulia/CrossInk.git`).
Add it after cloning. CrossInk 2.0 was still at `v2.0.0-rc3` when this was
written.

## Decided

- **Base:** CrossInk 2.0, not plain CrossPoint 2.0. The Home accessory needs
  CrossInk themes and the X3 simulator.
- **Reading comes first:** reading is never interrupted, it is the main source
  of XP, and nothing expires or punishes a missed day.
- **v2 scope:** moves, stats, turn-based battles, weaken-then-catch with balls
  earned by reading, shared party XP, the classic Kanto gyms, Elite Four and
  Champion, road trainers, a sleep-screen companion card, book memories, a
  Trainer Card and the 18-type chart.
- **Pacing at one hour a day:** 3–4 months to Champion (about 95–115 reading
  hours) and 6–12 months to finish the Pokédex.
- **Kanto map:** you start walking from the first town along the main path
  without picking a destination, and can change destination in the menu.
  Reading is steps. Where you are decides which Pokémon you can meet. A gym
  battle is an encounter waiting in its town.
- **Battles:** take the battle engine and data from the thebitsworld fork, and
  replace or rewrite whatever in it is not worth taking.
- **Content split:** the table of league leaders, their teams and badges, and
  the table of Kanto towns, routes and species per area, come from the project
  owner. The fork's existing data files are copied as they are. The systems,
  screens and data formats are built against a placeholder map.

## Proposed, not yet confirmed by the owner

- **Shared XP:** every party member gets 1 XP per 2 reading minutes and battle
  wins are split across the party. A full team reaches about level 57–60 at
  100 hours. Without halving, the lead alone reaches about level 84.
- **Map details:** steps come from credited reading minutes, not raw page
  turns. Each area has its own species list and level range, which replaces the
  book-progress gating. You cannot walk past a gym town until you win, losing
  costs nothing, and you can walk back at any time. Arriving in a town heals
  the party. Road trainers appear only on routes. The map is drawn with lines
  and labels, with no new artwork.
- **Cut from v1.0:** hidden stat variation, TMs, the rested bonus and badge
  stat boosts. The fork's engine uses IVs and EVs, so cutting them means
  feeding it fixed values.
- **Release:** ship the port as v0.2.0 labelled beta until CrossInk tags 2.0.0.
- **Save migrations:** small chained steps, not one large frozen format.

## What is on `feat/battle-engine`

Copied unchanged from the fork at `v2.1.0` (`ace5fe078`):

- `lib/Pokemon/PokemonBattleTypes.h`, `PokemonTrainers.{h,cpp}`,
  `PokemonMoveData.cpp`, `PokemonLearnsets.cpp`, `PokemonBaseStats.cpp`,
  `PokemonItemData.cpp`, `PokemonGymData.cpp`
- `scripts/generate_pokemon_{moves,stats,learnsets,items,gyms}.py`, their
  `_build.py` twins and `scripts/fetch_pokemon_battle_data.py`
- `scripts/data/pokemon-{gyms,items,learnsets,moves,stats,tmhm}.csv`
- `scripts/dev/simulate_pokemon_pace.py`

Copied and then run through clang-format 21, with no other change:
`lib/Pokemon/PokemonBattle.{h,cpp}`, `lib/Pokemon/PokemonTypeChart.cpp`,
`test/pokemon_battle/PokemonBattleTest.cpp` and
`test/pokemon_battle_data/PokemonBattleDataTest.cpp`.

Changes made to fit our tree:

- `lib/Pokemon/PokemonTypes.h` gained `TRAINER_CLASS_COUNT` and six table-size
  constants the engine expects.
- `platformio.ini` runs the five new generators for `pokemon-x3` and
  `pokemon-simulator-X3`.
- `test/CMakeLists.txt` adds the two new test folders.
- `test/pokemon_battle/CMakeLists.txt` no longer builds the fork's battle save
  codec, and `checkPersistable` in `PokemonBattleTest.cpp` is a stub. It
  checked that live battle state fits the fork's battle save entry. Put it
  back against our own save format once battle state is saved.

Checks on 27 November 2026: 18 of 18 Pokémon tests pass. `pio run -e
pokemon-x3` gives 6,329,216 bytes, the same as the port, because the linker
drops the unused engine. The slot is 6,553,600 bytes, so 224,384 are free. The
simulator was not rebuilt for this step.

Not adapted yet: `scripts/dev/simulate_pokemon_pace.py` still looks for
`test/build/pokemon_service/generated/PokemonGyms.generated.h`.

Before a release, credit the thebitsworld fork and PokeAPI (the source of the
battle data CSVs) in `RIGHTS_AND_ATTRIBUTION.md`.

## Audit of the fork

File and line numbers refer to the fork at `v2.1.0`.

| Verdict | Files | Why |
| --- | --- | --- |
| Take as is (done) | Battle engine, type chart, trainers, data wrappers, generators, CSVs, tests, pace simulation | Pure code with an injected random source, deterministic and tested |
| Take, not yet copied | Artwork pack (trainers, badges, items, effects), `scripts/dev/edit_pokemon_save.py` | The save tool only understands the fork's save |
| Merge by hand | `PokemonGame.*`, `PokemonTypes.*`, `PokemonStoreCodec.*`, `PokemonStore.*`, `PokemonService.*`, art components | The service changes one record at a time, and shared XP needs to change six. Each sprite draw makes two `std::vector` allocations. |
| Keep ours | Reading tracker and reader hook, store fixes, No-first prompts | The fork credits reading on the page-turn request and saves from `loop()` without the lock |
| Rewrite | `src/activities/pokemon/PokemonActivity.cpp`, the four sidecar save stores | See below |
| Skip | `PokemonSaveTransfer.*`, `PokemonSaveBundleCodec.*` | Depend on the fork's NearbySync |

The screen file is 6,037 lines and 88 KB compiled. It takes the render lock
once where ours takes it eight times, writes to the SD card from the render
path (`ensureIvEv`), and holds trainer heal and switch decisions and the XP and
gym sequencing in the UI (about `:1303-1431` and `:1848-2291`). It has no
tests.

The fork's save is a main file (version 11, 437-byte state) plus four separate
A/B files for battle state, movesets, IVs/EVs and the Hall of Fame. Nothing
commits them together, and the 518-entry tables cost about 15 KB of permanent
heap.

Bugs in the fork that must not come across:

- XP for a wild win is saved before the encounter is cleared and the result is
  ignored, so a failed write lets the battle be fought again.
- A save migrated from v0.1.x has no balls, because only a new game grants the
  starting 10 Poké Balls and 1 Potion, and catching needs one.
- Its save conversion is one-way.

The fork as a whole cannot be the base. It is 875 commits past its merge-base
with CrossInk and 133 behind `release/v2.0.0`, and carries Lua apps, a Game
Boy emulator, Bluetooth and NearbySync. Its game code depends on them only
through NearbySync save transfer, `SETTINGS.pokemonBattleAnimations`, a Home
icon, `features::pokemonGame()` and 153 extra `STR_POKEMON_*` strings.

## Numbers

- Compiled game objects before linking: fork 198,129 bytes, ours 64,088 (about
  47 KB once linked). The fork's engine is about 11 KB.
- Fork `pokemon-x3` firmware: 6,247,520 bytes, RAM 79,232 of 327,680.
- Fork pace simulation (`--runs 60 --plan c`): gyms at a median of 10 hours,
  Pokédex at a median of 189. Its own pace doc gives about 34 hours to Champion
  with one Pokémon. That is a third of the target, and the map and shared XP
  are how it is stretched.

## Next steps

1. Design one save file that holds moves, HP, PP and battle state per Pokémon,
   map position and gym progress, with an update that can change all six party
   members in one commit. Grant starting balls when an older save is upgraded.
2. Merge `PokemonGame`, `PokemonTypes` and `PokemonService` by hand.
3. Write a battle controller in `lib/Pokemon/` that owns turn order, trainer
   decisions and XP and gym sequencing, with tests.
4. Write the battle screens as separate, locked views on top of it.
5. Add the map: auto-walk, redirect from the menu, encounters by area, gym on
   arrival.
6. Add shared XP, point the pace simulation at our tree, and tune to the
   3–4 month target.

Still open from the earlier plan, and skipped over to start on battles:

- Release the port as v0.2.0: packaging, README and install page, changelog,
  and one check on a physical X3.
- Write the settled design spec into `docs/`.
- Open a draft pull request so CI runs. This waits for the owner's go-ahead.

## Building and testing

These notes are from Ubuntu 24.04 with GCC 13.3.

```sh
git clone https://github.com/padge01/xteink-pokemon-game.git
cd xteink-pokemon-game
git checkout feat/battle-engine
git submodule update --init --recursive
git remote add crossink https://github.com/uxjulia/CrossInk.git
```

- **PlatformIO:** use pioarduino 6.1.19, both as the `pio` command and inside
  PlatformIO's own environment: `uv pip install --python
  ~/.platformio/penv/bin/python "pioarduino==6.1.19"`. Otherwise the second
  build pass fails with `No module named 'SCons.Tool.FortranCommon'`.
- **Firmware:** `pio run -e pokemon-x3`
- **Simulator:** `PLATFORMIO_BUILD_FLAGS="-Wno-narrowing -D__LINUX__" pio run
  -e pokemon-simulator-X3`. The flags are needed because the simulator settings
  assume macOS clang.
- **Native tests:**

  ```sh
  python3 scripts/gen_i18n.py
  cmake -S test -B ../pokemon-test-build
  cmake --build ../pokemon-test-build -j8 --target \
    PokemonArtPathTest PokemonBattleDataTest PokemonBattleTest \
    PokemonDashboardLayoutTest PokemonGameTest PokemonServiceTest \
    PokemonStoreCodecTest PokemonStoreTest PokemonTrackerTest \
    PokemonTypesTest PokemonUiLayoutTest -- -k
  (cd ../pokemon-test-build && ctest -R Pokemon)
  ```

  About 22–24 unrelated upstream tests fail to build under GCC 13.3 on the
  stock tree as well, so judge by the Pokémon tests and by new failures.
- **Do not run `scripts/gen_i18n.py` while `pio` is building the same tree.**
  It scrambled every UI string in the build.
- **Formatting:** `uvx clang-format@21.1.8`. Older versions cannot read
  `.clang-format`.

### Driving the simulator without a display

Run the built `program` from a folder whose `fs_/` subfolder is the SD card.
Artwork comes from the v0.1.1 full-install zip.

```sh
SDL_AUDIODRIVER=dummy \
CROSSPOINT_SIM_INPUT_SCRIPT='2000:KEY;3000:KEY' \
CROSSPOINT_SIM_SCREENSHOTS='2500:shots/a.bmp;3500:shots/b.bmp' \
timeout 120 xvfb-run -a -s "-screen 0 1280x1024x24" ./program
```

Times are milliseconds from start. SDL's dummy video driver saves no
screenshots, which is why `xvfb-run` is used. The smoke test does not check
layout, so look at screenshots after any screen change.

### The fork, for reference

```sh
git clone https://github.com/thebitsworld/xteink-pokemon-game.git fork-thebitsworld
git -C fork-thebitsworld checkout v2.1.0
```

Its artwork is `xteink-pokemon-sd-card-assets.zip` on its release page. To
start its simulator in the game, set `CROSSINK_SIMULATOR_START_POKEMON=1` and
leave `CROSSINK_SIMULATOR_SMOKE_TEST` unset, because its smoke test stops on an
unrelated check. `scripts/dev/edit_pokemon_save.py` can queue encounters and
trainers in a fork save for testing.
