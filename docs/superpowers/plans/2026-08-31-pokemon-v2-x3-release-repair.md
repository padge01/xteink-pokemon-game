# Pokemon V2 X3 Release Repair Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Correct the confirmed Pokemon V2 rendering defects, strengthen the exact regression checks that previously failed on X3, produce one hash-verified release candidate, and complete a bounded physical acceptance pass before calling it install-ready.

**Architecture:** Keep the agreed lightweight V2 game and persistence model unchanged. Fix nickname artwork at the activity boundary, add an allocation-free 1-bit dither policy to the existing row-streaming BMP renderer, propagate render failures, and strengthen simulator fixtures without adding a second framebuffer or a new game subsystem. Treat simulator evidence and physical-X3 evidence as separate gates.

**Tech Stack:** C++20, Arduino-ESP32/PlatformIO, FreeInk UI, SdFat `FsFile`, the existing `Bitmap`/`GfxRenderer` row-streaming pipeline, CMake host tests, Python simulator/package tooling.

**Spec:** `docs/pokemon-v2-beta.md`

## Global Constraints

- This repairs the agreed lightweight V2. Do not add eggs, babies, friendship, Espeon/Umbreon, battles, moves, combat statistics, trading, or breeding.
- Preserve the original-151 collection, six-member Party, PC Box, verified-reading EXP, encounters, items, evolution routes, and complete Pokédex.
- Keep one 48,000-byte framebuffer on ESP32-C3; do not allocate a second framebuffer or assume PSRAM.
- Continue streaming `/sleep` cards from one explicitly closed `FsFile`; do not duplicate the 151 cards in the release package.
- Do not use `Bitmap(file, true)`: its current Atkinson path uses fallible bare `new` allocations. The card policy must use the renderer's existing reusable row scratch and allocation-free deterministic dithering.
- Existing generic bitmap callers retain their current output. The new dither policy is opt-in for Pokédex cards only.
- All recoverable allocation, file, parse, and row-read failures must log and return failure so the translated fallback UI appears.
- Do not redesign header, list, summary, dashboard, gameplay balance, snapshot format, or package layout unless a focused test or the physical gate proves another defect.
- The release artifact must be named exactly `update.bin`, and the staged file must hash-identically to the built `firmware.bin`.
- Preserve unrelated modifications in `lib/XmlParserUtils/XmlParserUtils.h` and `test/xml_parser_utils/XmlParserUtilsTest.cpp`.

---

### Task 1: Correct nickname artwork and summary failure states

**Files:**
- Create: `lib/Pokemon/PokemonPromptContext.h`
- Modify: `src/activities/pokemon/PokemonActivity.h`
- Modify: `src/activities/pokemon/PokemonActivity.cpp:257-275,384-398,727-809`
- Modify: `test/pokemon_types/PokemonTypesTest.cpp`

**Interfaces:**
- Consumes: `PokemonService::readRecord(uint32_t, PokemonRecord&)` and the existing `nicknameRecordId_` lifecycle.
- Produces: a bounded `PokemonPromptContext { speciesId, recordId }`, which always identifies the Pokemon shown by `Screen::NicknameQuestion`; visible translated load-error output for failed summary records.

- [ ] **Step 1: Add a failing caught-nickname context test**

Add a `PokemonTypesTest` case that describes the prompt value the activity needs without exposing private activity state or adding a test-only production API:

The state expectation is:

```cpp
const auto starter = pokemon::PokemonPromptContext::forStarter(1);
CHECK(starter.speciesId == 1);
CHECK(starter.recordId == 0);

const auto caught = pokemon::PokemonPromptContext::forCaught(133, 42);
CHECK(caught.speciesId == 133);
CHECK(caught.recordId == 42);
CHECK(!caught.isStarter());
```

- [ ] **Step 2: Run the focused host target and confirm the new API is missing**

Run:

```powershell
cmake --build build-tests --target PokemonTypesTest
ctest --test-dir build-tests -R PokemonTypesTest --output-on-failure
```

Expected: build failure because `PokemonPromptContext` does not exist.

- [ ] **Step 3: Store the prompt species at each entry point**

Add the header-only value type and replace the separate nickname record scalar with it:

```cpp
struct PokemonPromptContext {
  uint16_t speciesId = 1;
  uint32_t recordId = 0;

  static constexpr PokemonPromptContext forStarter(uint16_t speciesId);
  static constexpr PokemonPromptContext forCaught(uint16_t speciesId, uint32_t recordId);
  constexpr bool isStarter() const { return recordId == 0; }
};
```

When leaving starter gender selection, assign `nicknamePrompt_ = PokemonPromptContext::forStarter(starterSpecies_)`. After a successful catch, assign `nicknamePrompt_ = PokemonPromptContext::forCaught(pending.speciesId, caught)` before entering `NicknameQuestion`. Render `nicknamePrompt_.speciesId`, never `starterSpecies_`, on that screen. Use `nicknamePrompt_.recordId` for the existing rename/cancel flow; no snapshot format change is needed.

- [ ] **Step 4: Render a translated summary failure instead of a blank page**

Replace the silent returns around summary record/species loading with a log and visible error:

```cpp
if (service_.readRecord(focusedRecordId_, record) != pokemon::ServiceStatus::Ok) {
  LOG_ERR("PokemonActivity", "Failed to load summary record %lu",
          static_cast<unsigned long>(focusedRecordId_));
  centered(renderer, UI_12_FONT_ID, contentTop + 100,
           tr(STR_POKEMON_LOAD_ERROR), EpdFontFamily::BOLD);
  return;
}
```

Apply the same fallback when `speciesData(record.speciesId)` returns null. The existing Back hint remains available.

- [ ] **Step 5: Rerun the host test, then rebuild the simulator once and rerun the focused onboarding route**

Run:

```powershell
pio run -e pokemon-simulator-X3
python scripts/run_simulator_smoke_test.py --pokemon --env pokemon-simulator-X3 --no-build
```

Expected: the host test passes for starter and caught prompt contexts; the simulator onboarding route passes and preserves its existing starter/gender assertion.

- [ ] **Step 6: Commit the isolated activity fix**

```powershell
git add lib/Pokemon/PokemonPromptContext.h src/activities/pokemon/PokemonActivity.h src/activities/pokemon/PokemonActivity.cpp test/pokemon_types/PokemonTypesTest.cpp
git commit -m "fix: show correct Pokemon in nickname prompts"
```

---

### Task 2: Stream readable 1-bit Pokédex cards and propagate render failures

**Files:**
- Modify: `lib/GfxRenderer/BitmapHelpers.h`
- Modify: `lib/GfxRenderer/BitmapHelpers.cpp:95-109`
- Modify: `lib/GfxRenderer/GfxRenderer.h:252-254`
- Modify: `lib/GfxRenderer/GfxRenderer.cpp:1961-2058`
- Modify: `src/components/pokemon/PokemonArt.cpp:23-62`
- Create: `test/bitmap_bw_policy/CMakeLists.txt`
- Create: `test/bitmap_bw_policy/BitmapBwPolicyTest.cpp`

**Interfaces:**
- Produces: `enum class BitmapBwPolicy : uint8_t { ExistingThreshold, DitherNativeGray };`
- Produces: `bool GfxRenderer::drawBitmap(..., BitmapBwPolicy policy = BitmapBwPolicy::ExistingThreshold) const`.
- Consumes: packed native 2-bit levels from `Bitmap::readNextRow`; `DitherNativeGray` converts levels 0-3 to deterministic black/white pixels without heap allocation.

- [ ] **Step 1: Write failing tests for the pure 2-bit-to-1-bit policy**

Create a host test that checks the invariant, not one exact noise pattern:

```cpp
CHECK(ditherNativeGrayTo1Bit(0, 10, 10) == 0);
CHECK(ditherNativeGrayTo1Bit(3, 10, 10) == 1);

int darkWhite = 0;
int lightWhite = 0;
for (int y = 0; y < 32; ++y) {
  for (int x = 0; x < 32; ++x) {
    darkWhite += ditherNativeGrayTo1Bit(1, x, y);
    lightWhite += ditherNativeGrayTo1Bit(2, x, y);
  }
}
CHECK(darkWhite > 0);
CHECK(lightWhite < 32 * 32);
CHECK(darkWhite < lightWhite);
```

The helper accepts only levels 0-3 and maps them to luminance `level * 85` before applying the existing coordinate hash threshold. It must not call `adjustPixel` a second time because `Bitmap::readNextRow` already adjusted the source.

- [ ] **Step 2: Run the new host target and confirm it fails to compile**

Run:

```powershell
cmake -S test/bitmap_bw_policy -B test/bitmap_bw_policy/build
cmake --build test/bitmap_bw_policy/build
ctest --test-dir test/bitmap_bw_policy/build --output-on-failure
```

Expected: build failure because `ditherNativeGrayTo1Bit` does not exist.

- [ ] **Step 3: Add the allocation-free pure dither helper**

Declare and implement:

```cpp
uint8_t ditherNativeGrayTo1Bit(uint8_t nativeLevel, int x, int y);
```

Return black for 0, white for 3, and distribute levels 1 and 2 with the existing deterministic coordinate hash. Clamp an invalid level to 3 defensively. Do not allocate rows or ditherer objects.

- [ ] **Step 4: Make `drawBitmap` report success and accept the opt-in policy**

Change the return type from `void` to `bool`; existing callers may ignore it. Return `false` when the scratch lock cannot be acquired, scratch growth fails, or `readNextRow` fails. Return `true` only after all required source rows are read.

In BW mode, preserve the current default branch:

```cpp
const bool black = policy == BitmapBwPolicy::DitherNativeGray
                       ? ditherNativeGrayTo1Bit(val, screenX, screenY) == 0
                       : val < 3;
if (black) drawPixel(screenX, screenY);
```

Do not alter `GRAYSCALE_MSB`, `GRAYSCALE_LSB`, scaling, cropping, or other callers.

- [ ] **Step 5: Use the new policy only for `/sleep` Pokédex cards**

Extend the internal `drawPath` helper with a `BitmapBwPolicy` parameter. Species sprites and item art use `ExistingThreshold`; `drawPokemonPokedexArt` uses `DitherNativeGray`. Treat `renderer.drawBitmap(...) == false` as a render failure, log the path, close the file, and draw the existing fallback when requested.

Keep the local `Bitmap` object. It is approximately 300 bytes, exists only for one synchronous render, and avoids repeated heap churn; measure the activity stack high-water mark in Task 5.

- [ ] **Step 6: Run the focused renderer policy test**

Run:

```powershell
cmake --build test/bitmap_bw_policy/build
ctest --test-dir test/bitmap_bw_policy/build --output-on-failure
```

Expected: PASS with dark gray producing fewer white pixels than light gray and no dynamic allocation in the helper.

- [ ] **Step 7: Commit the renderer repair separately**

```powershell
git add lib/GfxRenderer/BitmapHelpers.h lib/GfxRenderer/BitmapHelpers.cpp lib/GfxRenderer/GfxRenderer.h lib/GfxRenderer/GfxRenderer.cpp src/components/pokemon/PokemonArt.cpp test/bitmap_bw_policy
git commit -m "fix: dither streamed Pokedex cards safely"
```

---

### Task 3: Strengthen regression fixtures without pretending they prove hardware

**Files:**
- Modify: `scripts/run_simulator_smoke_test.py:17-25,53-67,108-137`
- Modify: `src/simulator/SimulatorSmokeTest.cpp:454-527`
- Modify: `test/button_navigator/ButtonNavigatorTest.cpp`
- Modify: `test/pokemon_art_path/PokemonArtPathTest.cpp`
- Modify: `docs/pokemon-v2-beta.md`
- Modify: `CHANGELOG.md`

**Interfaces:**
- Simulator fixture stages only the card the scripted caught/seen species opens; it does not copy all 151 cards for every run.
- Host navigation tests exercise both logical pairs: `Down/Right` for next and `Up/Left` for previous.
- Documentation continues to identify raw GPIO mapping and e-ink pixels as physical-only evidence.

- [ ] **Step 1: Add the missing special filename assertion**

Extend `PokemonArtPathTest.cpp` with:

```cpp
EXPECT_STREQ(pokemon::pokemonPokedexArtPath(32, "Nidoran M", path, sizeof(path)),
             "/sleep/032-nidoran-m.bmp");
```

Retain the existing Nidoran F, Farfetch'd, and Mr. Mime checks.

- [ ] **Step 2: Add complete logical navigation tests**

Add checks showing `onNextPress` reacts once to Down and Right, `onPreviousPress` reacts once to Up and Left, and `nextIndex`/`previousIndex` wrap `150 -> 0` and `0 -> 150` for a 151-entry list. These tests prove the activity's logical use; they must not claim to prove X3 GPIO wiring.

- [ ] **Step 3: Stage one production-size `/sleep` card fixture in the isolated simulator filesystem**

Update `prepare_pokemon_assets` to generate a deterministic 528x792 8-bit grayscale card at the scripted `temp_root/fs_/sleep` path. This preserves the private-card licensing boundary while exercising the real `/sleep` path and the production downscale/dither behavior. Add these output failures to the runner scan:

```python
POKEMON_RENDER_FAILURES = (
    "[PKART] Missing Pokemon art",
    "[PKART] Invalid Pokemon art",
    "[PKART] Failed to render Pokemon art",
    "[GFX] Failed to read row",
)
```

- [ ] **Step 4: Make the scripted Pokédex route prove the detail card rendered**

After selecting a seen entry, require a simulator-only success marker emitted only after the Pokédex detail card renders. Preserve the second-page step so navigation beyond the first five entries remains covered.

- [ ] **Step 5: Run focused host tests**

Run:

```powershell
cmake -S test/button_navigator -B test/button_navigator/build
cmake --build test/button_navigator/build
ctest --test-dir test/button_navigator/build --output-on-failure
cmake -S test/pokemon_art_path -B test/pokemon_art_path/build
cmake --build test/pokemon_art_path/build
ctest --test-dir test/pokemon_art_path/build --output-on-failure
```

Expected: both targets PASS.

- [ ] **Step 6: Run one portrait and one landscape smoke route using the already-built simulator**

Run:

```powershell
python scripts/run_simulator_smoke_test.py --pokemon --env pokemon-simulator-X3 --no-build
python scripts/run_simulator_smoke_test.py --pokemon --env pokemon-simulator-X3 --landscape --no-build
```

Expected: both routes exit 0, render the production-size staged card without PKART/GFX errors, navigate to the second Pokédex page, and persist the selected starter/gender.

- [ ] **Step 7: Record the exact evidence boundary**

Update `docs/pokemon-v2-beta.md` and `CHANGELOG.md` to state:

- Pokédex cards use streamed 1-bit dithering in ordinary activity rendering.
- Simulator checks cover logical front/side directions but not raw X3 GPIO mapping.
- Header position, selected sprite pixels, card readability, and dashboard clearance remain physical acceptance items.

- [ ] **Step 8: Commit the regression gate**

```powershell
git add scripts/run_simulator_smoke_test.py src/simulator/SimulatorSmokeTest.cpp test/button_navigator/ButtonNavigatorTest.cpp test/pokemon_art_path/PokemonArtPathTest.cpp docs/pokemon-v2-beta.md CHANGELOG.md
git commit -m "test: cover Pokemon X3 release regressions"
```

---

### Task 4: Build and package one software-verified release candidate

**Files:**
- Verify: `.pio/build/pokemon-x3/firmware.bin`
- Verify: the existing private art source pack
- Produce: the existing release-output directory and `.zip`
- Verify: `update.bin`, `SHA256SUMS.txt`, `.crosspoint/pokemon/manifest.json`

**Interfaces:**
- Consumes: Tasks 1-3 with focused tests passing.
- Produces: one release candidate whose `update.bin` is byte-identical to the X3 build and whose artwork manifest contains exactly 151 sprites, 151 heroes, and the five stone item pairs.

- [ ] **Step 1: Obtain explicit approval for the long build gate**

The X3 build previously took approximately 17 minutes. Before running it, state that this gate consists of one `pokemon-x3` build plus packaging and is expected to take 20-25 minutes. Do not start without approval.

- [ ] **Step 2: Build the X3 firmware once**

Run:

```powershell
pio run -e pokemon-x3
```

Expected: PASS; static RAM remains comfortably below the C3 limit, and the application partition retains at least 192 KiB free.

- [ ] **Step 3: Run the focused package test and build the release archive**

Run the existing package unittest, then invoke `scripts/package_pokemon_v2_release.py` with the verified private art source, the new `.pio/build/pokemon-x3/firmware.bin`, and the established release output path.

Expected package contents:

```text
update.bin
SHA256SUMS.txt
POKEMON_ASSET_LICENSES.md
.crosspoint/pokemon/manifest.json
.crosspoint/pokemon/sprites/001.bmp ... 151.bmp
.crosspoint/pokemon/heroes/001.bmp ... 151.bmp
.crosspoint/pokemon/items/<five stones>.bmp
.crosspoint/pokemon/heroes/items/<five stones>.bmp
```

The Link Cable intentionally has no image file.

- [ ] **Step 4: Verify byte identity and asset counts**

Compare SHA-256 for `firmware.bin` and packaged `update.bin`; they must match exactly. Verify 151 sprite files, 151 hero files, five item icons, five item heroes, and every manifest hash. Do not include `/sleep` because those cards already live on the SD card.

- [ ] **Step 5: Review repository status before staging**

Confirm generated `.pio` output, local overrides, release archives, and the two unrelated XML parser modifications are not staged. Commit only intended source, test, documentation, and changelog files if a final commit is still needed.

---

### Task 5: Stage and physically accept the release on the X3

**Files/device state:**
- Stage: SD-root `/update.bin`
- Preserve: last known-good firmware and the current recoverable save backups
- Inspect after test: `/crash_report.txt`, `/.crosspoint/pokemon-v2-a.bin`, `/.crosspoint/pokemon-v2-b.bin`

**Interfaces:**
- Consumes: the exact hash-verified artifact from Task 4.
- Produces: a hardware acceptance record. Only this task can change the label from software-verified release candidate to X3-approved beta.

- [ ] **Step 1: Stage but do not flash until hashes match over the transfer path**

Upload the candidate as `/update.bin`, download it back through the X3 file-transfer service, and compare its SHA-256 with the packaged artifact. Confirm both active V2 snapshot files are absent for fresh onboarding while keeping the archived pre-fresh backups recoverable.

- [ ] **Step 2: Flash using the normal on-device updater**

Keep the device powered and do not interrupt the updater. If the updater rejects the file or the device does not return to Home, stop and use the known-good firmware rather than repeatedly retrying.

- [ ] **Step 3: Recheck the three original first-screen failures before choosing a starter**

On the physical starter screen confirm:

1. The `Pokemon` header does not touch or cross the rule.
2. Highlighting every row leaves its sprite visible and readable.
3. Both front buttons and both side buttons move selection in the expected direction.
4. Moving past Pikachu wraps to Bulbasaur; moving above Bulbasaur wraps to Pikachu.

Stop the release immediately if any item fails; photograph the screen before further changes.

- [ ] **Step 4: Verify onboarding and summary**

Choose one starter and gender, answer the classic nickname question, and inspect Summary. Confirm the large sprite, stable species/nickname spacing, No., level, gender, type, `EXP 0 / 16`, met level, evolution route, and evolution-prompt state. Confirm there is no battle-stat block or large progress bar.

- [ ] **Step 5: Verify Pokédex and collection navigation**

Confirm all six Party slots are visible, unavailable Move/Deposit actions are hidden for one Pokemon, Pokédex movement reaches page two and wraps across all 151 entries, and selecting the seen starter opens its `/sleep` card. Check that the card retains readable midtones rather than becoming a dark silhouette.

- [ ] **Step 6: Verify the supported dashboards in both orientations**

Inspect Dashboard, Lyra, Lyra 3 Covers, and Rounded Raff in portrait and landscape. For each, confirm the sprite is large/readable, species and optional nickname fit, level and local EXP do not collide, gender is absent, empty notice space remains blank, and Home controls remain unobstructed. Unsupported themes should omit the band.

- [ ] **Step 7: Verify reader lifecycle and persistence**

Open a book, turn real pages for at least five credited minutes, exit to Home, open and close Pokemon repeatedly, reboot, and confirm local EXP and the Party survive. Leaving a book open without page turns must not add EXP.

- [ ] **Step 8: Check crash and memory evidence**

Confirm no new `/crash_report.txt` was created. Capture free heap, largest allocatable heap block, and the Pokemon activity task stack high-water mark while opening a full Pokédex card; compare these with idle Home. A failed card must show the translated load-error fallback instead of a blank screen.

- [ ] **Step 9: Record the release decision**

Mark the candidate X3-approved only if every physical item passes. Otherwise preserve the exact firmware hash, photo, crash report, and reproduction steps and return to the smallest failing task; do not bundle unrelated improvements into the repair.

---

## Self-Review

- Spec coverage: the plan covers the confirmed nickname defect, streamed card readability, render failures, logical navigation, full-card fixtures, one X3 build, exact packaging, fresh staging, and every previously reported physical regression.
- Scope exclusions: eggs and friendship remain deliberately absent under `docs/pokemon-v2-beta.md`; they are not release blockers for lightweight V2.
- Type consistency: `BitmapBwPolicy` is defined once in `GfxRenderer.h`; `drawBitmap` returns `bool`; only Pokédex cards select `DitherNativeGray`.
- Resource discipline: the plan adds no framebuffer, no PSRAM dependency, no background task, no snapshot field, and no persistent allocation. Card rendering reuses the existing renderer scratch buffers.
- Evidence discipline: simulator checks prove logical flows and file availability; only the physical gate proves X3 GPIO behavior and e-ink pixels.
