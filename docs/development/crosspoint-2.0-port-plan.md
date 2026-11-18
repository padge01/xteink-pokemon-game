# Porting the Pokémon game onto CrossPoint 2.0

Research notes, 2026-11-18. Decision: do a **fresh port of the Pokémon module onto upstream
CrossPoint `2.0.0`**, rather than rebasing or merging this fork.

Facts below were checked against git on 2026-11-18. Items marked *(inference)* are judgement calls
that still need confirming during the port.

## Dev environment setup (do this first on a new machine)

```sh
git clone --recurse-submodules https://github.com/padge01/xteink-pokemon-game.git
cd xteink-pokemon-game
git remote add upstream https://github.com/crosspoint-reader/crosspoint-reader.git
git fetch upstream --tags          # gives you tags 1.4.0, v1.5.0, 1.6.5rc, 2.0.0rc, 2.0.0

# Use the same PlatformIO core as CI (.github/workflows/ci.yml), NOT stock PlatformIO.
uv tool install --force https://github.com/pioarduino/platformio-core/archive/refs/tags/v6.1.19.zip
pio run -e pokemon-x3
```

Gotcha: stock PlatformIO 6.2.0 fails at `firmware.elf` with
`ModuleNotFoundError: No module named 'SCons.Tool.FortranCommon'` against the pinned pioarduino
platform `55.03.37`. Installing CI's pioarduino core 6.1.19 is the expected fix (not yet verified
locally).

## Where this fork sits relative to upstream

- **`main` has no shared git history with upstream.** Its root commit `ff8f841e` ("Xteink Pokemon
  Game v0.1.0") is a squashed snapshot. `git merge-base origin/main 2.0.0` returns nothing, so you
  can't rebase it.
- **`origin/upstream-history` holds the full history.** It branched from upstream at `6e8dbd7f`
  (tag **1.4.0**, 2026-06-24). It has 660 fork-only commits, mostly CrossInk work.
  - Upstream has 593 commits from 1.4.0 to 2.0.0 (167 up to v1.5.0, then 426 to 2.0.0).
- **The code itself is closest to v1.5.0.** CrossInk's later syncs with upstream were squashed, so
  git doesn't record them. Commit `1ef5a97a` ("checkpoint CrossPoint 1.6 parity work") hand-ported
  some post-1.5 changes.
- **Overlap since v1.5.0** (not counting fonts and translations):
  - 358 files changed on both sides.
  - 32 files this fork modified were deleted upstream: `TxtReaderActivity`, `RecentBooksActivity`,
    `ClockOffsetActivity`, `OpdsParser`, the Noto Serif fonts and others.
- **Why rebasing isn't practical:** it would replay 660 commits, with squashed syncs, against 593
  upstream commits.

## What's new in CrossPoint 2.0

- **New devices:** four ESP32-S3 devices (Metalio E-Ink 4, EEGO A4, BOOX Picco, plus x4c and
  papermono envs).
- **SD-card plugins** (`src/activities/plugins`, `src/util/Plugin*`). Upstream says plugins will
  never support on-device games, so **the Pokémon game must stay compiled-in firmware**.
- **Bluetooth page turners** (`lib/BlePageTurner`).
- **Reading features:** highlights and clippings, multi-word dictionary lookup, better EPUB
  rendering, GIF images and OPDS 2.0.
- **UI and fonts:** a redesigned web UI and grouped settings (`SettingsList.cpp`). Noto Serif is now
  an optional SD-card font instead of being built in.
- **Reader architecture:**
  - `ReaderActivity` is now an abstract **base class** that `EpubReaderActivity` and
    `XtcReaderActivity` inherit from. In this fork it's a `final` dispatcher.
  - `TxtReaderActivity` is gone; TXT and Markdown now go through the EPUB pipeline.
- **SDK:** both sides use the `freeink-sdk` submodule (fork at `1ff02026`, 2.0 at `5e94fe74`). This
  is only a pointer bump.
- **Build:** `[crosspoint] version = 2.0.0`. The envs are default, slim, sticky, x4pro, x4c,
  metalio_eink4, papermono, eego_a4 and picco, each with a `gh_release` variant. There is no X3- or
  Pokémon-specific env.

### The 2.0 hook that replaces our per-reader turn tracking

`src/activities/reader/ReaderActivity.h` @ 2.0.0:

```cpp
ReaderSession readerSession;                 // protected member
void notePageTurn(bool forward, bool succeeded);
void flushReaderSession();
void prepareForSleep() override;
void onExit() override;
```

`src/activities/reader/ReaderSession.h` @ 2.0.0:

```cpp
void noteTurn(bool forward, bool succeeded);  // reported before render
void onRenderComplete(uint32_t monotonicMs, int64_t trustedEpoch, int progressBp);  // after render
bool isEmitWorthy() const;  // startEpoch > 0 && duration >= 10s
```

This "note the turn, then confirm it after a successful render" flow is the same thing our Pokémon
turn verifier does by hand (request → `renderSucceeded` → `consume`). *(inference)* With it, the
Pokémon reading hooks can live in **one place in the base class** (`notePageTurn`, the
render-complete path and `flushReaderSession`/`prepareForSleep`). That one place then covers EPUB,
XTC, TXT and Markdown, instead of the current ~6 hook sites in each reader.

## What the Pokémon layer is (the part to port)

The self-contained module, about 4.1k lines of firmware plus about 4.3k lines of tests:

- `lib/Pokemon/` (15 files): game rules, species, tracker, codec and layouts.
- `src/pokemon/`: `PokemonService` and `PokemonStore` (save data in its own store files).
- `src/activities/pokemon/PokemonActivity.*`
- `src/components/pokemon/*`
- `test/pokemon_*`
- `scripts/*pokemon*.py`, including the `generate_pokemon_v2_species_build.py` pre-build step, and
  `scripts/data/pokemon-kanto-v2.csv`.
- Docs: `docs/pokemon-game.md`, `docs/artwork-setup.md`.

### Hook points in core code

All of these sit behind `#if CROSSINK_ENABLE_POKEMON`. The number is how many guarded sites each file
has:

| File | Sites | What it does | 2.0 equivalent |
|---|---|---|---|
| `src/activities/reader/EpubReaderActivity.{cpp,h}` | 7 + 2 | session begin, turn verifier, checkpoint, flush | move to `ReaderActivity` base |
| `src/activities/reader/XtcReaderActivity.{cpp,h}` | 6 + 2 | same | move to `ReaderActivity` base |
| `src/activities/reader/TxtReaderActivity.{cpp,h}` | 7 + 2 | same | **file deleted in 2.0**, covered by base |
| `src/activities/home/HomeActivity.{cpp,h}` | 14 + 3 | menu item, dashboard snapshot, home-screen accessory | re-add to 2.0's `HomeActivity` (580 lines, much simpler) |
| `src/CrossPointSettings.h`, `src/SettingsList.h` | 1 + 1 | `pokemonHomeScreen` setting | re-add in 2.0's grouped `SettingsList.cpp` |
| `src/simulator/SimulatorSmokeTest.cpp` | 8 | simulator smoke coverage | re-add |
| `english.yaml` (i18n) | ~84 lines | strings | re-add |
| `AppVersion.h`, `platformio.ini` | — | product identity, `pokemon-x3` / `pokemon-simulator-X3` envs | new envs based on 2.0's `[env:default]` |

### Dependencies to check in 2.0

- **Present:** FreeInkUI, `UiAppHelpers.h`, `UIThemeTokens.h`, `KeyboardEntryActivity` and `Memory`.
- **Missing:** `TouchHeaderBackButton.h`. Replace it with 2.0's `src/components/HeaderBackTapTarget.h`.
- **Themes:** the home-screen accessory checks for CrossInk-only themes (DASHBOARD, MINIMAL,
  LYRA_CAROUSEL), which 2.0 doesn't have. Limit it to 2.0's themes, such as the cover-grid home
  (`CoverGridHomeUi`).

## Port plan

1. Branch from upstream: `git switch -c port/crosspoint-2.0 2.0.0`.
2. Copy the self-contained module unchanged from this fork's `main`: `lib/Pokemon/`, `src/pokemon/`,
   `src/activities/pokemon/`, `src/components/pokemon/`, `test/pokemon_*`, the Pokémon scripts and
   data, and the Pokémon docs.
3. Add `[env:pokemon-x3]` and `[env:pokemon-simulator-X3]` to `platformio.ini`, extending 2.0's base
   envs. Keep the `pre:scripts/generate_pokemon_v2_species_build.py` step and the
   `-DCROSSINK_ENABLE_POKEMON=1` / `-DCROSSINK_PRODUCT_XTEINK_POKEMON=1` flags. Check that 2.0 still
   supports X3 the way this fork does (`-DFREEINK_DEVICE_X3=1`). Rename the flags to something like
   `POKEMON_*` if you no longer want the CrossInk prefix.
4. Wire reading progress into the `ReaderActivity` base class:
   - Begin a session in `onEnter`.
   - Feed turns from `notePageTurn`.
   - Credit the turn only after a successful render.
   - Checkpoint and flush in `flushReaderSession`, `prepareForSleep` and `onExit`.

   This must keep the current guarantee that **only active reading counts**: a book left open
   earns nothing. 2.0's `ReaderSession` already ignores sessions shorter than 10 seconds.
5. Re-add the Home menu entry, the `!` encounter indicator and the dashboard snapshot to 2.0's
   `HomeActivity`.
6. Re-add the `pokemonHomeScreen` setting in the new grouped settings, plus the i18n strings.
7. Swap `TouchHeaderBackButton` for `HeaderBackTapTarget` and fix the theme checks.
8. Run the `test/pokemon_*` suites. Run `pio run -e pokemon-simulator-X3` and compare against
   `docs/screenshots/`. Run `pio run -e pokemon-x3` and test on the physical X3.
9. Check that an existing save from the 0.1.x firmware still loads. Pokémon state is in its own
   store files, so *(inference)* only the `pokemonHomeScreen` setting key should be affected by 2.0's
   settings changes.

## What you lose by moving to plain 2.0

These CrossInk-only features aren't in upstream 2.0:

- The Dashboard, Minimal and Lyra Carousel themes.
- The Bookmarks and Saved Items home screens.
- Nearby position sync and file transfer.
- USB serial transfer.
- Other CrossInk reader extras (this fork's `EpubReaderActivity.cpp` is 6.7k lines against 2.0's
  3.5k).

If you need any of these, the alternative is to wait for CrossInk itself to sync with 2.0 and then
reapply the Pokémon layer. That's still only the hook points listed above.
