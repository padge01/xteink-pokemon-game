# Pokémon V2 Simulator Integration Gate Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Compile the complete Pokémon-enabled X3 simulator and prove the real firmware activity can complete onboarding and navigate its core collection surfaces in both orientations without crashing or hanging.

**Architecture:** Keep production behavior unchanged unless the complete build or runtime exposes a concrete defect. Extend the existing simulator smoke harness with a Pokémon-specific route that drives logical X3 buttons through `MappedInputManager`, renders each real activity state, and exits through the existing success/failure markers.

**Tech Stack:** PlatformIO native simulator, C++20, SDL simulator, `SimulatorSmokeTest`, Python smoke runner.

**Spec:** `docs/superpowers/plans/2026-08-29-pokemon-companion-v2-hardware-release.md`

## Global Constraints

- Build `pokemon-simulator-X3`; do not substitute HTML mockups for firmware surfaces.
- Use an isolated temporary `fs_` for every automated run.
- Exercise logical `MappedInputManager::Button` input, matching X3 side/front mappings.
- Do not add production cheat menus, game behavior, heap allocations, or dashboard redesign.
- Fix only defects demonstrated by the complete build or simulator run.
- Simulator results do not prove ESP32-C3 memory safety or physical e-ink appearance.

---

### Task 1: Complete-build gate

**Files:**
- Read: `platformio.ini`
- Generated only: `.pio/build/pokemon-simulator-X3/`

**Interfaces:**
- Produces `.pio/build/pokemon-simulator-X3/program`.
- Consumes the committed Pokémon activity, service, store, rules, art paths, and generated species table.

- [ ] **Step 1: Build the full Pokémon X3 simulator**

Run:

```powershell
C:\Users\Padraig\.platformio\penv\Scripts\pio.exe run -e pokemon-simulator-X3
```

Expected: `SUCCESS` and a native `program` executable. If it fails, preserve the first compiler/linker error and enter systematic debugging before editing.

- [ ] **Step 2: Confirm the binary and clean source status**

Run:

```powershell
Get-Item .pio/build/pokemon-simulator-X3/program
git status --short
```

Expected: the simulator binary exists; only this plan is untracked before smoke-harness work.

### Task 2: Pokémon firmware smoke route

**Files:**
- Modify: `src/simulator/SimulatorSmokeTest.cpp`
- Modify: `scripts/run_simulator_smoke_test.py`
- Test: the built `pokemon-simulator-X3` executable in an isolated filesystem

**Interfaces:**
- `--pokemon` sets `CROSSINK_SIMULATOR_START_POKEMON=1` and selects a Pokémon-only smoke route.
- `--landscape` additionally sets `CROSSINK_SIMULATOR_POKEMON_LANDSCAPE=1`.
- The route must emit the existing `Simulator smoke test passed` marker or exit nonzero.

- [ ] **Step 1: Demonstrate the current harness cannot complete Pokémon mode**

Run the built executable with both `CROSSINK_SIMULATOR_SMOKE_TEST=1` and `CROSSINK_SIMULATOR_START_POKEMON=1`.

Expected before the fix: the generic smoke `Start` step replaces the Pokémon activity with Home, so no Pokémon onboarding route is exercised.

- [ ] **Step 2: Route smoke startup through the real Pokémon activity**

Move Pokémon startup into `SimulatorSmokeTest::tickImpl()` before generic Home startup. Add `Pokemon` and `PokemonInput` smoke steps guarded by `CROSSINK_ENABLE_POKEMON`; remove the competing one-off startup block from `runSimulatorSmokeTestTick()`.

- [ ] **Step 3: Drive the minimum real button flow**

Build a fixed smoke script that:

1. asserts the current activity is `Pokemon`;
2. chooses Bulbasaur;
3. chooses Female;
4. answers No to the nickname question;
5. opens Party, Summary, and Actions;
6. returns to the menu;
7. opens the Pokédex and navigates beyond its first five entries;
8. opens an empty PC Box;
9. renders after every transition and exits through the normal success marker.

Use the existing `inputScript` storage and logical press/release helpers; do not create a second automation framework.

- [ ] **Step 4: Add runner options**

Add `pokemon-simulator-X3` to `--env`, plus `--pokemon` and `--landscape`. Reject `--landscape` without `--pokemon` so invalid test requests fail clearly.

- [ ] **Step 5: Rebuild the simulator**

Run the same PlatformIO command from Task 1. Expected: `SUCCESS`.

### Task 3: Runtime and visual gate

**Files:**
- Generated only: isolated temporary simulator filesystems and local simulator output
- Modify production files only if a reproduced defect requires a narrowly tested fix

**Interfaces:**
- Portrait command: `python scripts/run_simulator_smoke_test.py --env pokemon-simulator-X3 --pokemon --no-build`
- Landscape command: the same command with `--landscape`

- [ ] **Step 1: Run headless portrait smoke**

Expected: exit 0, success marker present, and no configured crash pattern.

- [ ] **Step 2: Run headless landscape smoke**

Expected: exit 0, success marker present, and no configured crash pattern.

- [ ] **Step 3: Inspect the real simulator window**

Run the portrait route with `--window`, inspect starter, menu, Party, Summary, Actions, Pokédex, and PC surfaces, then repeat landscape only where geometry materially differs. Record concrete defects; do not polish speculative issues.

- [ ] **Step 4: Re-run focused host regression tests after any fix**

Run:

```bash
cmake --build build/test --target PokemonGameTest PokemonStoreTest PokemonServiceTest -j 8
ctest --test-dir build/test -R "Pokemon(Game|Store|Service)Test" --output-on-failure
```

Expected: 3/3 pass.

- [ ] **Step 5: Review and checkpoint**

Run `git diff --check`, inspect all changes, update `CHANGELOG.md` only if a user-facing defect changed, and commit the simulator harness/fixes as a separate checkpoint.

## Exit Criteria

- Complete `pokemon-simulator-X3` build succeeds.
- Real Pokémon onboarding and core navigation complete headlessly in portrait and landscape.
- Real simulator surfaces receive an interactive visual review.
- Any production changes are failure-driven and covered by focused regression tests.
- Physical X3 build, flash, heap measurements, and e-ink approval remain a later gate.
