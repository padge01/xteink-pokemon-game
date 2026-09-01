# X3 Installer and Release Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Publish a tested X3 firmware binary through this repository's own installer and provide one-button browser artwork installation from the X3 file-transfer website.

**Architecture:** GitHub Actions builds the `pokemon-x3` firmware and publishes a checksum alongside it. The existing Astro site becomes the independent installer and vendors only the X3 OTA flashing portion of the MIT CrossPoint Tools flasher. The Pokémon firmware adds a gated `/pokemon-setup` page; its JavaScript fetches CORS-enabled PokeAPI data and sprites, renders one-bit BMP files in the browser, uploads them through the existing X3 WebSocket/HTTP file-transfer protocol, and writes `manifest.json` last.

**Tech Stack:** PlatformIO/pioarduino, C++17/Arduino-ESP32, Astro 5, browser JavaScript, Canvas 2D, WebSerial/esptool-js, Node built-in test runner, Python unittest for release packaging.

**Spec:** `docs/superpowers/specs/2026-08-31-x3-installer-release-design.md`

## Global Constraints

- Xteink X3 only; an SD card must be inserted before installation.
- Wi-Fi/SD update is the primary route; USB WebSerial is secondary.
- The installer must not depend on CrossPoint's hosted website.
- The public repository and releases must not host Pokémon artwork.
- The normal artwork flow is one button and must not ask for archives or Python.
- Artwork is generated in the browser and uploaded sequentially; the ESP32-C3 does not decode, resize, or dither source art.
- `manifest.json` is uploaded only after all 614 expected BMP files succeed.
- Retrying restarts generation and safely overwrites expected paths; no per-file resume database.
- Pokémon-only web assets and routes are compiled only when `CROSSINK_ENABLE_POKEMON` is defined.
- Generated headers under `src/network/html/` are never edited directly.
- Existing CrossInk and Pokémon save data is not modified by installation or artwork setup.
- Public copy is short and direct; build, test, and beta details live under `docs/`.

---

### Task 1: Release artifact contract

**Files:**
- Create: `scripts/build_pokemon_release.py`
- Create: `test/pokemon_release/test_build_pokemon_release.py`
- Modify: `scripts/git_branch.py`
- Modify: `test/git_branch/test_git_branch.py`
- Modify: `.github/workflows/release.yml`
- Modify: `.github/workflows/release_candidate.yml`
- Modify: `CHANGELOG.md`

**Interfaces:**
- Consumes: `.pio/build/pokemon-x3/*.bin`, release version string.
- Produces: `dist/xteink-pokemon-x3-v<version>.bin`, `dist/SHA256SUMS`.
- Function: `build_release(firmware: Path, version: str, output: Path) -> tuple[Path, Path]`.

- [x] **Step 1: Write the failing release-contract tests**

```python
def test_build_release_uses_public_filename_and_checksum(self):
    with tempfile.TemporaryDirectory() as temporary:
        root = Path(temporary)
        firmware = root / "firmware.bin"
        firmware.write_bytes(b"x3 firmware")
        binary, sums = RELEASE.build_release(firmware, "1.0.0", root / "dist")
        self.assertEqual(binary.name, "xteink-pokemon-x3-v1.0.0.bin")
        self.assertTrue(sums.read_text("ascii").endswith("  xteink-pokemon-x3-v1.0.0.bin\n"))

def test_build_release_rejects_empty_firmware(self):
    with tempfile.TemporaryDirectory() as temporary:
        root = Path(temporary)
        firmware = root / "firmware.bin"
        firmware.write_bytes(b"")
        with self.assertRaisesRegex(ValueError, "empty"):
            RELEASE.build_release(firmware, "1.0.0", root / "dist")
```

- [x] **Step 2: Run the focused test and confirm the module is missing**

Run: `python test/pokemon_release/test_build_pokemon_release.py -v`

Expected: FAIL because `scripts/build_pokemon_release.py` does not exist.

- [x] **Step 3: Implement deterministic release naming and SHA-256 output**

```python
def build_release(firmware: Path, version: str, output: Path) -> tuple[Path, Path]:
    if not firmware.is_file() or firmware.stat().st_size == 0:
        raise ValueError("firmware is missing or empty")
    clean_version = version.removeprefix("v")
    if not re.fullmatch(r"[0-9A-Za-z][0-9A-Za-z._-]*", clean_version):
        raise ValueError("invalid release version")
    output.mkdir(parents=True, exist_ok=True)
    binary = output / f"xteink-pokemon-x3-v{clean_version}.bin"
    shutil.copyfile(firmware, binary)
    digest = hashlib.sha256(binary.read_bytes()).hexdigest()
    sums = output / "SHA256SUMS"
    sums.write_text(f"{digest}  {binary.name}\n", encoding="ascii")
    return binary, sums
```

- [x] **Step 4: Change both release workflows to build only `pokemon-x3` for this project and attach both output files**

The release workflow command becomes:

```yaml
- name: Build Pokémon firmware
  run: pio run -j1 -e pokemon-x3

- name: Package release
  run: >-
    python3 scripts/build_pokemon_release.py
    --firmware .pio/build/pokemon-x3/firmware-x3-x4-v${{ env.RELEASE_VERSION }}.bin
    --version "${{ env.RELEASE_VERSION }}"
    --output dist
```

The release asset glob becomes:

```yaml
files: |
  dist/xteink-pokemon-x3-v${{ env.RELEASE_VERSION }}.bin
  dist/SHA256SUMS
```

- [x] **Step 5: Run the focused tests**

Run: `python test/pokemon_release/test_build_pokemon_release.py -v`

Expected: PASS.

- [x] **Step 6: Commit the release contract**

```bash
git add scripts/build_pokemon_release.py test/pokemon_release .github/workflows/release.yml .github/workflows/release_candidate.yml CHANGELOG.md
git commit -m "build: package installable X3 releases"
```

### Task 2: Independent installer page and release download

**Files:**
- Create: `site/src/pages/install.astro`
- Create: `site/src/lib/installer/release-client.mjs`
- Create: `site/src/styles/installer.css`
- Create: `site/test/release-client.test.mjs`
- Modify: `site/src/layouts/SiteLayout.astro`
- Modify: `site/src/pages/index.astro`
- Modify: `site/package.json`
- Modify: `README.md`

**Interfaces:**
- Consumes: GitHub Releases API for `padge01/xteink-pokemon-game`.
- Produces: `/install/` with primary Wi-Fi download and secondary USB controls.
- Function: `selectReleaseAssets(release: object) -> { firmwareUrl: string, checksumUrl: string, version: string }`.

- [ ] **Step 1: Write release-response tests**

```js
test('selects the X3 binary and checksum', () => {
  const result = selectReleaseAssets({
    tag_name: 'v1.0.0',
    assets: [
      { name: 'xteink-pokemon-x3-v1.0.0.bin', browser_download_url: 'https://example/fw' },
      { name: 'SHA256SUMS', browser_download_url: 'https://example/sums' },
    ],
  });
  assert.deepEqual(result, {
    firmwareUrl: 'https://example/fw', checksumUrl: 'https://example/sums', version: '1.0.0',
  });
});

test('rejects a release without both assets', () => {
  assert.throws(() => selectReleaseAssets({ tag_name: 'v1', assets: [] }), /incomplete release/);
});
```

- [ ] **Step 2: Run the Node test and confirm it fails**

Run: `node --test site/test/release-client.test.mjs`

Expected: FAIL because the module is missing.

- [ ] **Step 3: Implement the release selector and browser fetch helper**

```js
export function selectReleaseAssets(release) {
  const firmware = release.assets?.find((asset) => /^xteink-pokemon-x3-v.+\.bin$/.test(asset.name));
  const checksum = release.assets?.find((asset) => asset.name === 'SHA256SUMS');
  if (!firmware || !checksum) throw new Error('incomplete release');
  return {
    firmwareUrl: firmware.browser_download_url,
    checksumUrl: checksum.browser_download_url,
    version: String(release.tag_name || '').replace(/^v/, ''),
  };
}

export async function fetchLatestRelease(fetchImpl = fetch) {
  const response = await fetchImpl('https://api.github.com/repos/padge01/xteink-pokemon-game/releases/latest');
  if (!response.ok) throw new Error(`release lookup failed (${response.status})`);
  return selectReleaseAssets(await response.json());
}
```

- [ ] **Step 4: Build the direct installer page**

The page contains exactly these user-facing sections:

```text
Pokémon for Xteink X3
1. Insert the SD card.
2. Choose Wi-Fi update or USB flash.

[Download firmware]  [Flash over USB]

Wi-Fi update
Start File Transfer, upload the downloaded .bin, then select it under
Settings > System > SD Card Firmware Update.
```

Do not include build commands, beta checklists, internal environment names, or artwork-source instructions.

- [ ] **Step 5: Replace the README front page with the approved compact structure**

Keep only description, install button, feature bullets, X3/SD requirement, firmware-base statement, and credit/licence links. Link development documentation instead of reproducing it.

- [ ] **Step 6: Run the release-client test and Astro build**

Run: `node --test site/test/release-client.test.mjs`

Expected: PASS.

Run: `npm run build --prefix site`

Expected: Astro writes `site/dist/install/index.html` with no broken internal links.

- [ ] **Step 7: Commit the installer shell**

```bash
git add site README.md
git commit -m "feat: add independent X3 installer page"
```

### Task 3: X3-only WebSerial flashing

**Files:**
- Create: `site/src/lib/installer/x3-flasher.js`
- Create: `site/public/vendor/crosspoint/esptool.bundle.js`
- Create: `site/public/vendor/crosspoint/LICENSE`
- Create: `site/test/x3-flasher.test.mjs`
- Modify: `site/src/pages/install.astro`
- Modify: `NOTICE.md`

**Interfaces:**
- Consumes: `Uint8Array` firmware downloaded from this project's release.
- Produces: `flashX3Firmware(firmware, callbacks) -> Promise<{ partition: string, success: true }>`.
- Wraps: the partition parser and OTA writer adapted from CrossPoint Tools `src/lib/flasher.js`.

- [ ] **Step 1: Write tests for browser and firmware safety gates**

```js
test('requires WebSerial before downloading firmware', async () => {
  await assert.rejects(() => assertWebSerial({}), /Chrome or Edge/);
});

test('rejects a non-ESP image before opening the serial port', async () => {
  await assert.rejects(() => validateFirmwareImage(new Uint8Array([0, 1, 2])), /invalid firmware/);
});

test('accepts only the X3 CrossPoint partition layout', () => {
  assert.equal(assertX3Layout(X3_PARTITIONS).device, 'X3');
  assert.throws(() => assertX3Layout(X4_PARTITIONS), /X3/);
});
```

- [ ] **Step 2: Run the focused Node test and confirm failure**

Run: `node --test site/test/x3-flasher.test.mjs`

Expected: FAIL because `x3-flasher.js` is missing.

- [ ] **Step 3: Adapt only the required CrossPoint Tools code**

Copy the MIT notice verbatim. Retain only:

- WebSerial connection and disconnect.
- ESP image-shape validation.
- Partition-table parsing.
- X3/CrossPoint layout validation.
- OTA inactive-slot selection.
- Firmware write and otadata switch verification.

Exclude stock firmware, repair, full-flash backup, font builds, beta catalogs, analytics, admin APIs, and non-X3 device definitions.

Expose this narrow wrapper:

```js
export async function flashX3Firmware(firmware, callbacks = {}) {
  assertWebSerial(globalThis.navigator);
  await validateFirmwareImage(firmware);
  const flasher = new CrossPointFlasher({ expectedDevice: 'x3' });
  return flasher.flashFirmware(firmware, callbacks);
}
```

- [ ] **Step 4: Connect the USB button to the latest release binary**

The button remains disabled until release metadata loads. Before `requestPort()`, show: `Insert the SD card and connect the X3.` Display the flasher's six verified steps and never offer repair/full-flash controls.

- [ ] **Step 5: Run safety tests and site build**

Run: `node --test site/test/x3-flasher.test.mjs site/test/release-client.test.mjs`

Expected: PASS.

Run: `npm run build --prefix site`

Expected: PASS and the bundled installer contains no CrossPoint network API URLs.

- [ ] **Step 6: Commit the USB flasher**

```bash
git add site/src/lib/installer/x3-flasher.js site/public/vendor/crosspoint site/test site/src/pages/install.astro NOTICE.md
git commit -m "feat: flash X3 releases from the project site"
```

### Task 4: Pokémon setup page in the X3 file-transfer server

**Files:**
- Create: `web/pages/pokemon-setup.html`
- Create: `web/pages/pokemon-setup.css`
- Create: `web/pages/pokemon-setup.js`
- Create: `web/lib/pokemon-art-core.js`
- Create: `test/web_pokemon_art/pokemon_art_core_test.js`
- Modify: `web/templates/base.html`
- Modify: `scripts/build_web.py`
- Modify: `scripts/preview_web.py`
- Modify: `src/network/CrossPointWebServer.h`
- Modify: `src/network/CrossPointWebServer.cpp`

**Interfaces:**
- Produces: `GET /pokemon-setup`, gated by `CROSSINK_ENABLE_POKEMON`.
- Extends: `GET /api/status` with `features.pokemon: true` only in Pokémon builds.
- Pure function: `expectedPokemonArt() -> Array<{ path: string, width: number, height: number }>`.

- [ ] **Step 1: Write tests for the 614-file contract and generated-page inclusion**

```js
test('defines the complete original-151 pack', () => {
  const files = expectedPokemonArt();
  assert.equal(files.length, 614);
  assert.deepEqual(files.find((f) => f.path === 'sprites/001.bmp'),
    { path: 'sprites/001.bmp', width: 40, height: 30 });
  assert.deepEqual(files.find((f) => f.path === 'pokedex/portrait/151.bmp'),
    { path: 'pokedex/portrait/151.bmp', width: 472, height: 708 });
});
```

- [ ] **Step 2: Run the page test and confirm failure**

Run: `node --test test/web_pokemon_art/pokemon_art_core_test.js`

Expected: FAIL because the core module/page is missing.

- [ ] **Step 3: Add the generated page without adding it to ordinary firmware**

Add `pokemon-setup` to `PAGES` and generate `PokemonSetupPageHtml.generated.h`. In C++, wrap the generated include, route registration, handler declaration, and handler definition with:

```cpp
#if defined(CROSSINK_ENABLE_POKEMON)
// Pokémon setup route/include/handler
#endif
```

Return the capability from `/api/status`:

```cpp
#if defined(CROSSINK_ENABLE_POKEMON)
  doc["features"]["pokemon"] = true;
#endif
```

- [ ] **Step 4: Reveal the navigation link only when the capability is present**

Render a hidden link in the shared template:

```html
<a id="pokemon-setup-link" href="/pokemon-setup" hidden>Pokémon Setup</a>
```

Use a short shared inline script to call `/api/status` and remove `hidden` only when `features.pokemon === true`.

- [ ] **Step 5: Run page tests and regenerate headers**

Run: `node --test test/web_pokemon_art/pokemon_art_core_test.js`

Expected: PASS.

Run: `python scripts/build_web.py`

Expected: Generates `PokemonSetupPageHtml.generated.h`; generated headers remain unstaged.

- [ ] **Step 6: Commit the setup page shell**

```bash
git add web scripts/build_web.py scripts/preview_web.py src/network/CrossPointWebServer.cpp src/network/CrossPointWebServer.h test/web_pokemon_art
git commit -m "feat: add X3 Pokémon artwork setup page"
```

### Task 5: Browser-side one-bit artwork renderer

**Files:**
- Modify: `web/lib/pokemon-art-core.js`
- Modify: `test/web_pokemon_art/pokemon_art_core_test.js`

**Interfaces:**
- `sourceUrls(speciesId: number) -> { icon: string, species: string }`.
- `renderIcon(imageData, width, height) -> ImageData`.
- `renderPokedexCard(species, sprite, width, height) -> ImageData`.
- `ditherOneBit(imageData) -> Uint8Array` with one byte per output pixel (`0` or `1`).
- `encodeOneBitBmp(bits, width, height) -> Uint8Array`.
- `sha256Hex(bytes) -> Promise<string>`.

- [ ] **Step 1: Write deterministic renderer and BMP tests**

```js
test('encodes a padded 1-bit BMP', () => {
  const bmp = encodeOneBitBmp(new Uint8Array([0, 1, 1, 0]), 2, 2);
  assert.equal(String.fromCharCode(bmp[0], bmp[1]), 'BM');
  assert.equal(readU32(bmp, 18), 2);
  assert.equal(readU32(bmp, 22), 2);
  assert.equal(readU16(bmp, 28), 1);
  assert.equal(bmp.length, 70); // 62-byte header/palette + two padded 4-byte rows
});

test('uses the approved CORS-enabled sources', () => {
  assert.equal(sourceUrls(1).icon,
    'https://raw.githubusercontent.com/PokeAPI/sprites/master/sprites/pokemon/versions/generation-vii/icons/1.png');
  assert.equal(sourceUrls(1).species, 'https://pokeapi.co/api/v2/pokemon-species/1/');
});
```

- [ ] **Step 2: Run tests and confirm the missing implementations fail**

Run: `node --test test/web_pokemon_art/pokemon_art_core_test.js`

Expected: FAIL on missing renderer exports.

- [ ] **Step 3: Implement fixed-memory sequential rendering**

Use one canvas and one `ImageData` buffer at a time. Render:

- Gen VII icon source centered into `40x30`; nearest-neighbour `120x90` hero.
- Item source centered into `32x32`; nearest-neighbour `64x64` hero.
- One logical `528x792` Pokédex card containing number, English name, Gen I English flavor text, and a large sprite.
- Resize that logical card to `472x708` and `288x432` before one Floyd-Steinberg pass.

`encodeOneBitBmp` writes a 62-byte BMP header/palette, 4-byte-aligned rows, a white/black two-entry palette, and bottom-up pixel rows.

- [ ] **Step 4: Add source validation and bounded fetch concurrency**

Fetch at most four species records concurrently. Reject responses without an English Gen I flavor-text entry or a decodable sprite. Normalize `\n` and `\f` to spaces before line wrapping.

- [ ] **Step 5: Run renderer tests**

Run: `node --test test/web_pokemon_art/pokemon_art_core_test.js`

Expected: PASS with deterministic headers, paths, source URLs, and dither output.

- [ ] **Step 6: Commit the renderer**

```bash
git add web/lib/pokemon-art-core.js test/web_pokemon_art/pokemon_art_core_test.js
git commit -m "feat: render X3 Pokémon artwork in the browser"
```

### Task 6: Sequential upload and manifest-last installation

**Files:**
- Create: `web/lib/pokemon-upload-client.js`
- Create: `test/web_pokemon_art/pokemon_upload_client_test.js`
- Modify: `web/pages/pokemon-setup.js`
- Modify: `scripts/build_web.py`
- Modify: `scripts/preview_web.py`

**Interfaces:**
- `ensurePokemonFolders(fetchImpl) -> Promise<void>`.
- `uploadBlob(path: string, bytes: Uint8Array, transport: object) -> Promise<void>`.
- `installPokemonArt({ fetchImpl, upload, onProgress, signal, plan }) -> Promise<void>`.
- Manifest bytes are the fixed UTF-8 string `{"format":1,"scope":"original-151","assetCount":614}\n`.

- [ ] **Step 1: Write transport-order tests**

```js
test('writes the manifest only after all art files', async () => {
  const uploaded = [];
  await installPokemonArt({
    fetchImpl: fixtureFetch,
    upload: async (path) => uploaded.push(path),
    onProgress() {},
    plan: fixturePlanForOneSpecies,
  });
  assert.equal(uploaded.at(-1), '/.crosspoint/pokemon/manifest.json');
  assert.ok(uploaded.slice(0, -1).every((path) => path.endsWith('.bmp')));
});

test('does not upload a manifest after a failed art file', async () => {
  const uploaded = [];
  await assert.rejects(() => installPokemonArt({
    fetchImpl: fixtureFetch,
    plan: fixturePlanForOneSpecies,
    upload: async (path) => {
      uploaded.push(path);
      throw new Error('write failed');
    },
  }));
  assert.equal(uploaded.includes('/.crosspoint/pokemon/manifest.json'), false);
});
```

- [ ] **Step 2: Run upload tests and confirm failure**

Run: `node --test test/web_pokemon_art/pokemon_upload_client_test.js`

Expected: FAIL because the client is missing.

- [ ] **Step 3: Implement folder creation and existing WebSocket protocol**

Create these paths through `POST /mkdir` in parent-before-child order:

```text
/.crosspoint
/.crosspoint/pokemon
/.crosspoint/pokemon/sprites
/.crosspoint/pokemon/heroes
/.crosspoint/pokemon/heroes/items
/.crosspoint/pokemon/items
/.crosspoint/pokemon/pokedex
/.crosspoint/pokemon/pokedex/portrait
/.crosspoint/pokemon/pokedex/landscape
```

Treat `already exists` as success. Upload one file at a time using:

```text
START:<filename>:<size>:<destination-directory>
READY
<binary chunks>
DONE
```

Fall back to multipart `POST /upload?path=<directory>` when port 81 cannot connect.

- [ ] **Step 4: Build the one-button state machine**

The page has these states only:

```text
Ready -> Downloading -> Creating artwork -> Installing -> Verifying -> Artwork installed
                                                                      -> Retry
```

Show `current / 614`, the current species name, transferred bytes, and a Cancel button. Cancel closes the WebSocket and leaves the manifest absent.

- [ ] **Step 5: Run upload and renderer tests**

Run: `node --test test/web_pokemon_art/*.js`

Expected: PASS, including manifest-last and failed-upload behavior.

- [ ] **Step 6: Regenerate the web header and inspect compressed size**

Run: `python scripts/build_web.py`

Expected: The command reports the compressed Pokémon setup page size. Record the flash delta before deciding whether any source split is needed; do not claim a memory saving from gzip alone.

- [ ] **Step 7: Commit the artwork installer**

```bash
git add web scripts/build_web.py scripts/preview_web.py test/web_pokemon_art
git commit -m "feat: install Pokémon artwork from the X3 browser"
```

### Task 7: Firmware artwork readiness and end-to-end release verification

**Files:**
- Create: `src/components/pokemon/PokemonArtStatus.h`
- Create: `src/components/pokemon/PokemonArtStatus.cpp`
- Create: `test/pokemon_art_status/PokemonArtStatusTest.cpp`
- Create: `test/pokemon_art_status/CMakeLists.txt`
- Modify: `test/CMakeLists.txt`
- Modify: `src/activities/pokemon/PokemonActivity.cpp`
- Modify: `src/network/CrossPointWebServer.cpp`
- Modify: `README.md`
- Modify: `docs/release-checklist.md`
- Modify: `CHANGELOG.md`

**Interfaces:**
- `pokemonArtManifestReady(std::string_view content) -> bool`.
- `pokemonArtInstalled() -> bool` reads one bounded manifest file from storage.
- `GET /api/pokemon/art-status -> { ready: bool, expected: 614 }`.

- [ ] **Step 1: Write host tests for manifest-last readiness**

```cpp
TEST(PokemonArtStatus, RejectsMissingOrWrongManifest) {
  EXPECT_FALSE(pokemon::pokemonArtManifestReady(""));
  EXPECT_FALSE(pokemon::pokemonArtManifestReady(
      R"({"format":1,"scope":"original-151","assetCount":613})"));
}

TEST(PokemonArtStatus, MatchingManifestIsReady) {
  EXPECT_TRUE(pokemon::pokemonArtManifestReady(
      "{\"format\":1,\"scope\":\"original-151\",\"assetCount\":614}\n"));
}
```

- [ ] **Step 2: Run the host target and confirm failure**

Run: `cmake -S test -B build/test && cmake --build build/test --target PokemonArtStatusTest`

Expected: FAIL because the status component is missing.

- [ ] **Step 3: Implement a bounded manifest check**

Open only `/.crosspoint/pokemon/manifest.json`, read at most 80 bytes into a fixed local buffer, close the file on every path, and compare it with the fixed manifest string. Do not scan or hash 614 files on the C3. The 80-byte stack buffer is bounded and below the project's 256-byte justification threshold.

- [ ] **Step 4: Add the user-facing setup path**

When art is not ready, the Pokémon menu shows:

```text
Artwork setup required
Open File Transfer > Pokémon Setup.
```

The game remains usable with existing `?` fallbacks and does not crash. `/api/pokemon/art-status` exposes the same state to the setup page for final verification.

- [ ] **Step 5: Run targeted host and browser tests**

Run: `cmake -S test -B build/test && cmake --build build/test --target PokemonArtStatusTest && ctest --test-dir build/test -R PokemonArtStatusTest --output-on-failure`

Expected: PASS.

Run: `node --test test/web_pokemon_art/*.js site/test/*.test.mjs`

Expected: PASS.

- [ ] **Step 6: Run approved build checks**

Run: `npm run build --prefix site`

Expected: PASS.

Run: `pio run -j1 -e pokemon-x3`

Expected: PASS and produces the release input binary.

Run: `pio run -j1 -e simulator`

Expected: PASS; this validates compilation and host UI flow only.

- [ ] **Step 7: Perform physical X3 checks before publishing**

With an SD card inserted:

1. Preserve the existing Pokémon save snapshots.
2. Install the exact release candidate by Wi-Fi/SD update.
3. Confirm File Transfer shows Pokémon Setup.
4. Install all artwork from a clean `/.crosspoint/pokemon/` state.
5. Confirm icons, item art, portrait cards, and landscape cards render.
6. Interrupt setup once, confirm it does not report ready, then retry successfully.
7. Verify starter selection, Party, Pokédex, Bag, dashboard, book opening, and return from reader.
8. Verify the documented rollback firmware route.

- [ ] **Step 8: Commit readiness behavior and documentation**

```bash
git add src/components/pokemon src/activities/pokemon/PokemonActivity.cpp src/network/CrossPointWebServer.cpp test/pokemon_art_status README.md docs/release-checklist.md CHANGELOG.md
git commit -m "feat: verify Pokémon artwork installation"
```

- [ ] **Step 9: Create the public release only after physical approval**

Create a GitHub prerelease first. Confirm that its binary checksum matches the physically tested file. Promote that same release without rebuilding; never replace the tested asset with a later local build.
