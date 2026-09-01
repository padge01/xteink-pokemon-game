# Full X3 Install and Artwork Release Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Publish one verified X3 SD-card ZIP containing the tested firmware and every required Pokémon artwork file, with direct installation, attribution, and removal-request documentation.

**Architecture:** Keep converted artwork out of Git history and assemble it at release time with the existing Python packager. GitHub Release assets become the binary distribution boundary: a complete SD-card ZIP for new users, a firmware-only binary for existing users, and one checksum file covering both. The Astro site and repository documentation point new users to the complete ZIP while retaining the X3's normal SD-card firmware-update path.

**Tech Stack:** Python 3 standard library, `unittest`, Astro 5, browser JavaScript, Node built-in test runner, GitHub Releases, GitHub Pages.

**Spec:** `docs/superpowers/specs/2026-08-31-full-install-artwork-release-design.md`

## Global Constraints

- Xteink X3 only; no X4, X4 Pro, Sticky, WebSerial, WebUSB, or direct USB-flashing claims.
- The full archive contains `update.bin` and `/.crosspoint/pokemon/`; artwork is not embedded in firmware.
- The package must never contain `pokemon-v2-a.bin`, `pokemon-v2-b.bin`, books, caches, settings, or sleep-screen files.
- Existing saves and reading data must survive extraction of the full package over an existing SD-card tree.
- Converted artwork stays out of normal Git history and is distributed only as a GitHub Release asset.
- Credits and removal language do not claim that the artwork is licensed, public-domain, or legally approved.
- The full-install archive must include the same rights and attribution notice published in the repository.
- The already hardware-tested RC firmware may be reused only after its downloaded SHA-256 is verified; this slice does not rebuild firmware.
- Publishing occurs only after local packaging tests, site tests, archive verification, and release-download verification pass.

---

### Task 1: Public full-install packaging contract

**Files:**
- Modify: `scripts/package_pokemon_v2_release.py`
- Modify: `test/pokemon_art_pack/test_package_pokemon_v2_release.py`

**Interfaces:**
- Consumes: a complete local artwork directory, tested firmware binary, release version, and `RIGHTS_AND_ATTRIBUTION.md`.
- Produces: `xteink-pokemon-x3-full-v<version>.zip`, `xteink-pokemon-x3-firmware-v<version>.bin`, and `SHA256SUMS.txt`.
- Function: `build_public_release(source: Path, firmware: Path, notice: Path, version: str, output: Path) -> tuple[Path, Path, Path]`.
- Function: `verify_archive(archive: Path, firmware: Path, notice: Path) -> None`.

- [ ] **Step 1: Write failing tests for public filenames and archive contents**

Extend the existing synthetic-art test with these assertions:

```python
full_zip, firmware_asset, sums = PACKAGE.build_public_release(
    source, firmware, notice, "0.1.0-RC", root / "dist"
)
self.assertEqual(full_zip.name, "xteink-pokemon-x3-full-v0.1.0-RC.zip")
self.assertEqual(firmware_asset.name, "xteink-pokemon-x3-firmware-v0.1.0-RC.bin")
self.assertEqual(sums.name, "SHA256SUMS.txt")

with zipfile.ZipFile(full_zip) as package:
    names = set(package.namelist())
self.assertIn("update.bin", names)
self.assertIn("RIGHTS_AND_ATTRIBUTION.md", names)
self.assertIn(".crosspoint/pokemon/manifest.json", names)
self.assertIn(".crosspoint/pokemon/sprites/001.bmp", names)
self.assertIn(".crosspoint/pokemon/pokedex/portrait/151.bmp", names)
self.assertNotIn(".crosspoint/pokemon-v2-a.bin", names)
self.assertNotIn(".crosspoint/pokemon-v2-b.bin", names)
```

- [ ] **Step 2: Write the failing save-preservation extraction test**

```python
sd_root = root / "sd"
(sd_root / ".crosspoint").mkdir(parents=True)
save_a = sd_root / ".crosspoint/pokemon-v2-a.bin"
save_b = sd_root / ".crosspoint/pokemon-v2-b.bin"
save_a.write_bytes(b"save-a")
save_b.write_bytes(b"save-b")
with zipfile.ZipFile(full_zip) as package:
    package.extractall(sd_root)
self.assertEqual(save_a.read_bytes(), b"save-a")
self.assertEqual(save_b.read_bytes(), b"save-b")
```

- [ ] **Step 3: Run the packaging test and confirm the new API is absent**

Run: `python test/pokemon_art_pack/test_package_pokemon_v2_release.py -v`

Expected: FAIL because `build_public_release` does not exist.

- [ ] **Step 4: Implement deterministic public release packaging**

Use the existing `expected_art()`, `validate_art()`, `sha256()`, and BMP checks. Add strict version normalization and stage the archive in a temporary directory:

```python
VERSION_PATTERN = re.compile(r"[0-9A-Za-z][0-9A-Za-z._-]*")

def normalize_version(version: str) -> str:
    clean = version.removeprefix("v")
    if VERSION_PATTERN.fullmatch(clean) is None:
        raise ValueError("invalid release version")
    return clean

def build_public_release(source, firmware, notice, version, output):
    clean = normalize_version(version)
    output.mkdir(parents=True, exist_ok=True)
    firmware_asset = output / f"xteink-pokemon-x3-firmware-v{clean}.bin"
    full_zip = output / f"xteink-pokemon-x3-full-v{clean}.zip"
    # Validate artwork, copy the tested firmware, stage only expected files,
    # create manifest and internal checksums, then ZIP the staging tree.
    # Write SHA256SUMS.txt for full_zip and firmware_asset after both exist.
    return full_zip, firmware_asset, output / "SHA256SUMS.txt"
```

The staging tree must include `RIGHTS_AND_ATTRIBUTION.md`, not the old private-package filename `POKEMON_ASSET_LICENSES.md`. Iterate only over `expected_art()` so dirty source files are excluded.

- [ ] **Step 5: Implement independent archive verification**

`verify_archive()` opens the ZIP without extracting it, rejects duplicate names and absolute or `..` paths, verifies the exact 614 BMP paths plus the four required non-art paths, checks the embedded `update.bin` hash against the firmware-only asset, and validates every manifest checksum.

- [ ] **Step 6: Run focused packaging tests**

Run: `python test/pokemon_art_pack/test_package_pokemon_v2_release.py -v`

Expected: all packaging, validation, dirty-source, filename, and save-preservation tests PASS.

- [ ] **Step 7: Commit the packaging contract**

```bash
git add scripts/package_pokemon_v2_release.py test/pokemon_art_pack/test_package_pokemon_v2_release.py
git commit -m "build: package complete X3 installs"
```

### Task 2: Rights, attribution, and removal surface

**Files:**
- Create: `RIGHTS_AND_ATTRIBUTION.md`
- Create: `.github/ISSUE_TEMPLATE/rights_attribution.yml`
- Modify: `NOTICE.md`
- Modify: `docs/third-party-assets.md`
- Modify: `docs/artwork-setup.md`
- Modify: `docs/release-checklist.md`
- Modify: `CHANGELOG.md`

**Interfaces:**
- Consumes: the approved notice text and exact source links in the design.
- Produces: the notice embedded by Task 1 and a public rights-request route at `/issues/new?template=rights_attribution.yml`.

- [ ] **Step 1: Add the repository rights and attribution notice**

Use the approved no-affiliation and removal wording verbatim. Add distinct source entries for:

```text
Species icons: PokeAPI Sprites, Generation VII 40x30 icons
Evolution-stone icons: PokéSprite
Pokédex layout and X3 renders: dmellok / u/xDaftTurtle
Pokédex renderer: Tesserae
Pokédex data and sprite inputs: PokéAPI
```

Link the original Reddit post and state that source credit does not claim an explicit redistribution licence.

- [ ] **Step 2: Add a structured rights-request issue template**

Create a GitHub YAML form with this contract:

```yaml
name: Rights or attribution request
description: Request removal, replacement, or corrected credit for an asset.
title: "[Rights/Attribution] "
labels: ["rights-request"]
body:
  - type: dropdown
    attributes:
      label: Requested action
      options: [Remove an asset, Replace an asset, Correct attribution]
    validations:
      required: true
  - type: textarea
    attributes:
      label: Asset or credit
      description: Identify the affected file, artwork family, source, or credit.
    validations:
      required: true
  - type: textarea
    attributes:
      label: Request details
    validations:
      required: true
```

- [ ] **Step 3: Replace no-hosting statements in repository documentation**

Update `NOTICE.md`, `docs/third-party-assets.md`, and `docs/artwork-setup.md` to say that converted X3 artwork is distributed in the full-install Release ZIP, remains excluded from Git history, and can be removed or replaced on request. Preserve every existing ownership caveat and exact source revision.

- [ ] **Step 4: Rewrite the release checklist around the full package**

Require exact art counts/dimensions, notice inclusion, no-save verification, package SHA-256, extraction over clean and save-bearing SD trees, and download verification. Remove the obsolete checks that forbid artwork release assets.

- [ ] **Step 5: Add the changelog entry**

Under the current unreleased/version section add:

```markdown
### Added

- Complete X3 installation archives now include the tested firmware and all required Pokémon artwork in one SD-card ZIP.
- Added a public rights and attribution request process for artwork removal, replacement, or corrected credit.
```

- [ ] **Step 6: Run documentation integrity checks**

Run:

```powershell
rg -n "does not contain Pokémon artwork|must prepare the local artwork|must not.*host|remains local and is not attached" README.md NOTICE.md docs site/src
git diff --check
```

Expected: no current user documentation claims that public releases exclude artwork; `git diff --check` exits successfully.

- [ ] **Step 7: Commit the rights surface**

```bash
git add RIGHTS_AND_ATTRIBUTION.md .github/ISSUE_TEMPLATE/rights_attribution.yml NOTICE.md docs/third-party-assets.md docs/artwork-setup.md docs/release-checklist.md CHANGELOG.md
git commit -m "docs: add artwork rights and removal process"
```

### Task 3: Full-install download and SD-card guide

**Files:**
- Modify: `site/src/lib/installer/release-client.mjs`
- Modify: `site/test/release-client.test.mjs`
- Modify: `site/src/pages/install.astro`
- Modify: `site/test/install-page.test.mjs`
- Modify: `README.md`
- Modify: `docs/installation.md`

**Interfaces:**
- Consumes: GitHub Releases containing full ZIP, firmware-only BIN, and `SHA256SUMS.txt`.
- Produces: `selectReleaseAssets(release) -> { fullInstallUrl, firmwareUrl, checksumUrl, version }` and a site whose primary action downloads the full ZIP.

- [ ] **Step 1: Write failing release-client tests for all three assets**

```js
const result = selectReleaseAssets({
  tag_name: 'v0.1.0',
  assets: [
    { name: 'xteink-pokemon-x3-full-v0.1.0.zip', browser_download_url: 'https://example/full' },
    { name: 'xteink-pokemon-x3-firmware-v0.1.0.bin', browser_download_url: 'https://example/fw' },
    { name: 'SHA256SUMS.txt', browser_download_url: 'https://example/sums' },
  ],
});
assert.equal(result.fullInstallUrl, 'https://example/full');
assert.equal(result.firmwareUrl, 'https://example/fw');
```

Also assert that missing full-install ZIP throws `Release is missing the full X3 install package.`

- [ ] **Step 2: Write failing install-page copy tests**

Require `Download full install`, `Extract`, `.crosspoint`, `Firmware only`, `Wi-Fi update`, and `SD card reader`. Continue rejecting `Flash over USB`, `navigator.serial`, and `x3-flasher`.

- [ ] **Step 3: Run site tests and confirm they fail against the firmware-only page**

Run: `npm test --prefix site`

Expected: release-client and install-page assertions FAIL for missing full-install behavior.

- [ ] **Step 4: Implement full-install release selection**

Match exact public filenames:

```js
const fullInstall = assets.find((asset) => /^xteink-pokemon-x3-full-v.+\.zip$/.test(asset.name));
const firmware = assets.find((asset) => /^xteink-pokemon-x3-firmware-v.+\.bin$/.test(asset.name));
const checksum = assets.find((asset) => asset.name === 'SHA256SUMS.txt');
```

Return all three URLs and reject an incomplete release with a specific error.

- [ ] **Step 5: Make the complete ZIP the primary page action**

The page starts with `Download full install (.zip)`. The primary route says:

1. Power off the X3 and insert its SD card into a computer.
2. Back up `/.crosspoint/pokemon-v2-a.bin` and `pokemon-v2-b.bin` if present.
3. Extract the ZIP into the SD-card root and merge `.crosspoint`; do not delete the existing folder.
4. Safely eject the card and return it to the X3.
5. Run **Settings > System > SD Card Firmware Update > update.bin**.

Keep `Firmware only (.bin)` and Wi-Fi upload in a clearly secondary **Existing artwork installation** section.

- [ ] **Step 6: Update the README and installation document**

Lead with the full-install button and the same five SD-card steps. State clearly that the ZIP contains firmware and artwork but no saves, and that firmware-only updates preserve an existing `/.crosspoint/pokemon/` folder.

- [ ] **Step 7: Run the focused site validation**

Run: `npm test --prefix site`

Expected: all Node tests PASS.

Run: `npm run build --prefix site`

Expected: Astro successfully generates the two static pages.

- [ ] **Step 8: Commit the download experience**

```bash
git add site README.md docs/installation.md
git commit -m "feat: make full X3 install the primary download"
```

### Task 4: Build, inspect, and publish the complete RC package

**Files:**
- Local-only input: canonical converted artwork directory containing the 614 required BMP files.
- Local-only output: `dist/xteink-pokemon-x3-full-v0.1.0-RC.zip` and companion release files.
- External update: GitHub Release `rc-0.1.0-209ad6f` and GitHub Pages.

**Interfaces:**
- Consumes: the published, hardware-tested RC firmware and local canonical artwork.
- Produces: a public full-install Release asset and live full-install download page.

- [ ] **Step 1: Download and verify the tested RC firmware**

Download `xteink-pokemon-x3-v0.1.0-RC.bin` from the existing RC release and verify:

```text
SHA-256 97c4bee176b0215c77b624c0cfc08591894e52154496ed3c308adb956570d30b
Size    6,314,992 bytes
```

Abort packaging if either value differs.

- [ ] **Step 2: Validate the canonical artwork before packaging**

Run the packager's validator against the local source pack. Expected: exactly 614 required BMPs are accepted, all are one-bit, IDs 001–151 exist in both Pokédex orientations, and the five stone icons exist in both sizes.

- [ ] **Step 3: Build the public release assets**

Run:

```powershell
python scripts/package_pokemon_v2_release.py `
  --source-pack $env:POKEMON_ART_SOURCE `
  --firmware $env:POKEMON_TESTED_FIRMWARE `
  --notice RIGHTS_AND_ATTRIBUTION.md `
  --version 0.1.0-RC `
  --output dist
```

Expected: full ZIP, firmware-only BIN, and `SHA256SUMS.txt` are produced.

- [ ] **Step 4: Run archive and save-preservation verification**

Use the same verification API exercised by Task 1, then extract over temporary save files and compare hashes. Inspect the ZIP member list for absolute paths, `..`, duplicate names, unrelated SD files, and both Pokémon save filenames. Expected: all checks PASS.

- [ ] **Step 5: Review the final repository diff and release assets**

Run `git status --short`, `git diff --check`, targeted Python tests, site tests, and the Astro build. Record the exact three asset sizes and hashes. Do not run a PlatformIO firmware build because firmware source is unchanged and the package uses the previously tested binary.

- [ ] **Step 6: Push the reviewed commits and deploy Pages**

Push the implementation commits to `pokemon-origin/main`. Wait for the Pages workflow and verify the live page contains the full-install and firmware-only links without USB-flashing copy.

- [ ] **Step 7: Attach the full package to the existing RC release**

Upload the full ZIP, renamed firmware-only BIN, and new `SHA256SUMS.txt` to `rc-0.1.0-209ad6f`. Preserve the existing tested firmware asset until the new links are verified. Update release notes with installation steps, credits, the removal-request link, and package hashes.

- [ ] **Step 8: Verify the public downloads**

Download all published assets again, verify their hashes against the published checksum, open the ZIP, and run `verify_archive()`. Confirm the GitHub Pages primary button downloads the full ZIP and the secondary button downloads the firmware-only BIN.

- [ ] **Step 9: Commit any release-note or catalog changes**

```bash
git add README.md CHANGELOG.md docs site
git commit -m "release: publish complete X3 install package"
```

Skip this commit when publication produces no tracked-file changes.
