# Pokemon Companion V2 Completion Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Complete the private Kanto reading companion from the committed V2 core through a packaged X3 beta candidate.

**Architecture:** `PokemonGame` owns rules, `PokemonStore` owns the two durable snapshots, and `PokemonService` exposes all UI commands and bounded queries. One FreeInkUI activity renders the game with fixed page buffers; a small art helper and four theme-specific dashboard calls complete presentation without a second model or framebuffer.

**Tech Stack:** C++20, PlatformIO/Arduino-ESP32, FreeInkUI, SdFat through HAL storage, GoogleTest host tests, Python artwork/package verification.

**Spec:** `docs/superpowers/plans/2026-08-30-pokemon-companion-v2-completion-design.md`

## Global Constraints

- Compile all Pokemon production code only with `CROSSINK_ENABLE_POKEMON`; support only `pokemon-x3`.
- Use `tr(STR_*)` for every user-facing string.
- Use `MappedInputManager::Button::*`, FreeInkUI, `UITheme`, and oriented safe bounds.
- No per-render allocation, bare `new`, second framebuffer, roster-sized container, persistent open SD file, or new dependency.
- Keep total handwritten Pokemon production code at or below 3,500 nonblank lines and each new Pokemon UI file at or below 700.
- Keep final partition headroom at or above 192 KiB and incremental static RAM below 2 KiB over the committed core.

---

### Task 1: Complete the service command/query boundary

**Files:**
- Modify: `src/pokemon/PokemonService.h`
- Modify: `src/pokemon/PokemonService.cpp`
- Modify: `src/pokemon/PokemonStore.h`
- Test: `test/pokemon_service/PokemonServiceTest.cpp`
- Test: `test/pokemon_store/PokemonStoreTest.cpp`

**Interfaces:**
- Produce `ServiceStatus`, `PokemonSnapshot`, and `PokemonDashboardSnapshot` fixed-size value types.
- Produce commands `createStarter`, `renamePokemon`, `movePartyMember`, `depositPokemon`, `withdrawPokemon`, `setEvolutionPrompts`, `resolveEncounter`, `acknowledgeItem`, `resolveEvolution`, `useEvolutionItem`, and `reset`.
- Produce queries `loadSnapshot`, `loadDashboardSnapshot`, `readRecord`, `readPcPage`, and `recordCount`.

- [ ] Add a failing service test proving a valid starter creates one durable record, marks it seen/caught, occupies Party slot one, and cannot be created twice.
- [ ] Run `PokemonServiceTest` and confirm failure because `createStarter` does not exist.
- [ ] Implement the minimal starter/query API and make the test pass.
- [ ] Add failing tests for nickname validation, one-member Move no-op, dense reorder, last-member deposit rejection, full-Party withdrawal rejection, successful deposit/withdraw, and write-failure atomicity.
- [ ] Implement minimal commands using one `PokemonStore::commit` per action and make the tests pass.
- [ ] Add failing tests for encounter Catch/Pass, item acknowledgement, evolution accept/cancel, evolution-prompt toggle, usable/unusable items, dashboard snapshot, PC paging, and reset.
- [ ] Implement the remaining service facade and make the focused service/store tests pass.
- [ ] Refactor only after green; verify no method leaks store/rule details into the activity API.
- [ ] Commit `feat: complete pokemon v2 service facade`.

### Task 2: Add the minimum complete Pokemon activity

**Files:**
- Create: `src/activities/pokemon/PokemonActivity.h`
- Create: `src/activities/pokemon/PokemonActivity.cpp`
- Create: `src/activities/pokemon/PokemonActivityScreens.cpp`
- Create: `src/activities/pokemon/PokemonArt.h`
- Create: `src/activities/pokemon/PokemonArt.cpp`
- Modify: `src/activities/home/HomeActivity.cpp`
- Modify: `src/activities/ActivityResult.h`
- Modify: `lib/I18n/translations/en.yaml`
- Regenerate: `lib/I18n/*` through `scripts/gen_i18n.py`

**Interfaces:**
- `PokemonActivity` consumes only `PokemonService` commands and snapshots.
- `PokemonArt::drawSprite(renderer, speciesId, rect, focused)` draws exact SD assets or an unboxed fallback.
- The first screen is Starter when storage is empty, Event when one is pending, otherwise the Pokemon menu.

- [ ] Add a failing host test for any pure selection/page helper needed by the activity; use existing `ButtonNavigator` directly when no new behavior is needed.
- [ ] Implement the compile-gated home-menu action and one activity shell with fixed menu/Party buffers.
- [ ] Implement starter species, gender, classic nickname question, and existing keyboard result flow.
- [ ] Implement Pokemon menu, Party, fixed-geometry Summary, pending encounter/item/evolution screen, and protected beta reset.
- [ ] Ensure selected rows keep a white sprite gutter; bottom hints list only real Back/Select actions and side buttons perform Up/Down.
- [ ] Generate i18n from YAML; do not edit generated translation files directly.
- [ ] Build the simulator target and walk the minimum loop in both orientations.
- [ ] Commit `feat: add pokemon v2 minimum activity`.

### Task 3: Add collection management and complete event actions

**Files:**
- Modify: `src/activities/pokemon/PokemonActivity.h`
- Modify: `src/activities/pokemon/PokemonActivity.cpp`
- Modify: `src/activities/pokemon/PokemonActivityScreens.cpp`
- Modify: `lib/I18n/translations/en.yaml`
- Test: `test/pokemon_service/PokemonServiceTest.cpp`

**Interfaces:**
- PC pages contain at most six `PokemonRecord` values.
- Sort modes are Catch Date, Pokedex Number, and Alphabetical.
- Pokedex renders species IDs 1 through 151 from state bitsets without loading records.

- [ ] Add or extend failing service tests for Party-full Catch routing to PC, duplicate species, all PC orders, and entry 151 Pokedex state.
- [ ] Implement PC deposit/withdraw/actions and the three sort modes with bounded paging.
- [ ] Implement Bag counts and applicable stone/Link Cable actions; Link Cable has no icon.
- [ ] Implement continuous Pokedex paging, wrapping, Seen/Caught/unknown rows, and access to entry 151.
- [ ] Complete rename and evolution-prompt actions from Summary.
- [ ] Run focused tests and simulator navigation checks for empty/full PC, one/full Party, page boundaries, and every pending event.
- [ ] Commit `feat: add pokemon v2 collection screens`.

### Task 4: Produce and verify the Kanto artwork pack

**Files:**
- Create: `scripts/package_pokemon_v2_art.py`
- Create: `scripts/verify_pokemon_v2_art.py`
- Create: `scripts/data/pokemon-v2-art-sources.json`
- Create: `docs/third-party-assets.md` if absent, otherwise modify it
- Create: `build/pokemon-v2-art/` as ignored output
- Test: `test/pokemon_art_pack/` host script test

**Interfaces:**
- Input is the pinned approved PokeAPI/PokeSprite-derived pack already used by V1.
- Output contains exactly `sprites/001.bmp` through `151.bmp`, `heroes/001.bmp` through `151.bmp`, five stone icons and heroes, a compact manifest, attribution, and no egg/baby files.

- [ ] Add a failing script test using a temporary fixture with an extra species and egg; expect the verifier to reject it.
- [ ] Implement a deterministic filter/packager and verifier for count, paths, dimensions, 1-bit palette, and SHA-256.
- [ ] Run the verifier against the produced Kanto pack and record its total bytes.
- [ ] Document the pinned source/revisions and the same attribution pathway as the approved PokeSprite pack.
- [ ] Commit `build: package pokemon v2 artwork` without committing generated pack output.

### Task 5: Integrate the four real dashboards

**Files:**
- Create: `src/components/pokemon/PokemonHomeAccessory.h`
- Create: `src/components/pokemon/PokemonHomeAccessory.cpp`
- Modify: `src/activities/home/HomeActivity.cpp`
- Modify only as required: `src/components/themes/dashboard/DashboardTheme.*`
- Modify only as required: `src/components/themes/lyra/LyraTheme.*`
- Modify only as required: `src/components/themes/lyra/Lyra3CoversTheme.*`
- Modify only as required: `src/components/themes/roundedraff/RoundedRaffTheme.*`

**Interfaces:**
- `PokemonHomeAccessory::draw(renderer, rect, snapshot, layout)` receives a preloaded bounded snapshot and allocates nothing.
- Layout variants are Dashboard, Lyra, Lyra3Covers, and RoundedRaff; unsupported themes do not call it.

- [ ] Add a failing geometry test for each layout proving the 80x60 sprite, identity, numeric EXP, and button/footer bounds do not overlap in 480x800 or 800x480.
- [ ] Implement a borderless accessory with an always-readable sprite and name/level; numeric EXP is secondary, gender tertiary, notice last.
- [ ] Integrate actual home render paths for only the four approved themes; omit accessory on snapshot/art failure.
- [ ] Capture actual simulator output for eight theme/orientation combinations and inspect for crowding, erased sprites, and footer collisions.
- [ ] Commit `feat: integrate pokemon v2 dashboards`.

### Task 6: Final verification, documentation, and package

**Files:**
- Modify: `CHANGELOG.md`
- Modify: `docs/file-formats.md`
- Modify: `README.md`
- Create: `docs/pokemon-v2-beta.md`
- Create: `scripts/package_pokemon_v2_release.py`
- Create: `build/pokemon-v2-release/` as ignored output

**Interfaces:**
- Package output is `firmware-x3x4.bin`, `.crosspoint/pokemon/`, `SHA256SUMS.txt`, installation guide, and the preserved rollback firmware.

- [ ] Run all focused Pokemon host tests and the artwork/package script tests.
- [ ] Build `simulator` once and complete the scripted/menu walkthrough.
- [ ] Count nonblank production lines and reject over 3,500 or any new Pokemon UI file over 700.
- [ ] Build `pokemon-x3` once, compare RAM/flash with the recorded core and clean baselines, and reject less than 192 KiB headroom or 2 KiB incremental static RAM.
- [ ] Update user-facing documentation, changelog, file format, installation/reset, V1 incompatibility, and attribution.
- [ ] Generate the private package and verify every hash and declared path.
- [ ] Commit `docs: package pokemon companion v2 beta`.
- [ ] Record that physical X3 navigation, heap recovery, e-ink contrast, and reader-menu crash verification remain a release-blocking user/device gate until actually run.
