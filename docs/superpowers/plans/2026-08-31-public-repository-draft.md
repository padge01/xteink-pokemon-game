# Public Repository Draft Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Produce the complete local, public-facing documentation package for “Pokémon Game for Xteink X3” without publishing the repository or redistributing Pokémon artwork.

**Architecture:** Keep the inherited CrossInk firmware history and MIT licence intact. Replace the fork-oriented landing page with a direct project README, put upstream notices and artwork limitations in dedicated documents, and keep all Pokémon images in a user-supplied local workspace excluded from Git. Repository creation, pushing, and release publication remain a later explicit action after the user reviews these drafts.

**Tech Stack:** Markdown, GitHub issue forms, Python artwork utilities already present in the repository, Git tracked-file audits.

**Spec:** `docs/superpowers/specs/2026-08-31-xteink-pokemon-game-repository-design.md`

## Global Constraints

- Public display title is **Pokémon Game for Xteink X3**.
- Repository slug remains `xteink-pokemon-game`.
- The project is unofficial, independently maintained, noncommercial, and not endorsed by Pokémon, Nintendo, Creatures, GAME FREAK, The Pokémon Company, Xteink, CrossInk, or CrossPoint.
- X3 is the only supported launch target.
- Preserve the inherited MIT `LICENSE` and all required copyright notices.
- Do not commit or publish Pokémon artwork, rendered Pokédex cards, private archives, firmware binaries, save data, credentials, or local SD-card contents.
- Do not claim that attribution grants permission to redistribute Pokémon intellectual property.
- Do not create the GitHub repository, push, or publish a release during this plan.
- Preserve unrelated uncommitted firmware changes.

---

### Task 1: Align the approved repository design

**Files:**
- Modify: `docs/superpowers/specs/2026-08-31-xteink-pokemon-game-repository-design.md`

**Interfaces:**
- Consumes: the licensing review approved in chat.
- Produces: the authoritative project name and publication boundary used by every public document.

- [ ] **Step 1: Change the display title**

Replace **Xteink Pokémon Game** with **Pokémon Game for Xteink X3** while keeping the approved repository slug.

- [ ] **Step 2: Strengthen the artwork policy**

State explicitly that public source and releases contain no Pokémon images or rendered cards; users obtain source artwork themselves and run local conversion tools. State that attribution is not redistribution permission and that xDaftTurtle’s cards require explicit permission before this project may rehost them.

- [ ] **Step 3: Self-check the spec**

Run:

```powershell
rg -n "Xteink Pokémon Game|rehost|redistribut|permission|Pokémon Game for Xteink X3" docs/superpowers/specs/2026-08-31-xteink-pokemon-game-repository-design.md
```

Expected: no old display title; explicit permission and no-rehosting language is present.

### Task 2: Replace the repository landing page

**Files:**
- Modify: `README.md`

**Interfaces:**
- Consumes: the project boundaries in the approved spec.
- Produces: the public landing page linking installation, artwork, testing, credits, and upstream projects.

- [ ] **Step 1: Write the direct project introduction**

Use the title **Pokémon Game for Xteink X3**, immediately label it unofficial beta firmware, and state that it is a complete CrossInk-based firmware build rather than a plugin.

- [ ] **Step 2: Add compact feature and rule sections**

Document verified-reading EXP, starter selection, Party/PC/Bag/Pokédex, encounters, evolution, and the deliberately omitted battle/move/breeding systems. Keep this to short statements and bullets.

- [ ] **Step 3: Add installation and recovery sections**

Explain that users need both a locally built `update.bin` and a locally generated `/.crosspoint/pokemon/` artwork tree. Put SD backup, Pokémon save backup, rollback firmware, and X3-only warnings beside the installation steps.

- [ ] **Step 4: Add development and verification sections**

List the exact `pokemon-x3` and `pokemon-simulator-X3` environments. Clearly distinguish automated checks from physical X3 acceptance and avoid any unverified safety claim.

- [ ] **Step 5: Add concise legal and credits links**

Link `NOTICE.md`, `docs/artwork-setup.md`, `docs/pokemon-v2-beta.md`, and the upstream projects. State that no Pokémon artwork is included.

- [ ] **Step 6: Check public-language requirements**

Run:

```powershell
rg -n "private demonstration|X4|X4 Pro|safe|fully tested|Pokémon Game for Xteink X3|NOTICE.md|artwork-setup" README.md
```

Expected: X4 appears only as unsupported; private-demo wording is gone; no unqualified safety or completeness claim is present.

### Task 3: Add exact notices and artwork instructions

**Files:**
- Create: `NOTICE.md`
- Create: `docs/artwork-setup.md`
- Create: `scripts/generate_pokemon_icon_art.py`
- Create: `test/pokemon_art_pack/test_generate_pokemon_icon_art.py`
- Modify: `docs/third-party-assets.md`
- Modify: `docs/pokemon-v2-beta.md`
- Modify: `docs/file-formats.md`
- Modify: `platformio.ini`
- Modify: `.gitignore`

**Interfaces:**
- Consumes: pinned source revisions already recorded in `docs/third-party-assets.md` and the existing local conversion/package utilities.
- Produces: auditable attribution plus a user-controlled local artwork workflow that never puts artwork into Git.

- [ ] **Step 1: Write a failing offline-converter test**

Create controlled RGBA PNG fixtures for species and stones. Assert that the
converter writes one-bit 40×30 and 120×90 species BMPs plus one-bit 32×32 and
64×64 stone BMPs, rejects a missing source file, and never accesses the
network.

- [ ] **Step 2: Run the converter test and verify RED**

Run:

```powershell
python test/pokemon_art_pack/test_generate_pokemon_icon_art.py -v
```

Expected: import failure because `scripts/generate_pokemon_icon_art.py` does
not exist.

- [ ] **Step 3: Implement the offline converter and verify GREEN**

Accept `--pokemon-source`, `--item-source`, and `--output` local paths. Resolve
species as `{id}.png` or `{id:03}.png`, resolve the five named stone PNGs, place
each source on the required transparent canvas without enlarging its pixel art,
and convert alpha-aware grayscale to one-bit BMP. Run the same unittest and
expect all cases to pass.

- [ ] **Step 4: Write `NOTICE.md`**

Include:

- CrossInk and CrossPoint as MIT-licensed firmware ancestry.
- Joshua Miller’s companion as the reference for the reading-companion concept and real-reading session-credit behaviour.
- Tesserae and dmellok/xDaftTurtle as the source of the rendered Pokédex-card design/reference.
- PokeAPI Sprites and PokéSprite as artwork source references, with the important distinction that their repository licences do not grant this project ownership of Pokémon imagery.
- The Pokémon and Xteink trademark/no-affiliation statement.

- [ ] **Step 5: Write `docs/artwork-setup.md`**

Document only a user-supplied local workflow:

1. Obtain the original-151 X3 cards from the creator’s Reddit post or provide an equivalent local source.
2. Obtain the pinned PokeAPI and PokéSprite repositories separately.
3. Build the required local folder at an ignored path.
4. Run `scripts/generate_pokemon_icon_art.py` with the two local source paths.
5. Run `scripts/generate_pokemon_pokedex_art.py` for card conversion.
6. Run `scripts/package_pokemon_v2_release.py` only for personal installation.
7. Do not upload the generated pack or archive to GitHub.

Clearly list the required SD paths and dimensions and note that the current repository does not automatically download Pokémon images.

- [ ] **Step 6: Rewrite `docs/third-party-assets.md` for public release**

Remove “private demonstration pack” language. Keep exact revisions and ownership statements. Add the direct PokeAPI licence warning: its repository says CC0 while also saying all images are copyright The Pokémon Company and disclaiming clearance of third-party rights. Add PokéSprite’s distinction between MIT code/non-art material and copyrighted sprites.

- [ ] **Step 7: Extend ignore rules**

Ignore local artwork staging roots and release archives without broad patterns that could hide source files:

```gitignore
/pokemon-art-source/
/pokemon-art-output/
/pokemon-release-local/
/pokemon-release-local.zip
```

- [ ] **Step 8: Check notices and links**

Run:

```powershell
rg -n "CrossInk|CrossPoint|Joshua|Tesserae|xDaftTurtle|PokeAPI|PokéSprite|Nintendo|Creatures|GAME FREAK|Pokémon Company|Xteink|not affiliated|not.*permission" NOTICE.md docs/artwork-setup.md docs/third-party-assets.md
```

Expected: every source and the no-permission distinction appears.

- [ ] **Step 9: Remove obsolete private-demo wording**

Update the beta guide, Pokémon file-format introduction, and build-environment
comment to describe an X3 beta with locally generated art. Do not change binary
formats, build flags, or runtime behaviour.

### Task 4: Add launch-facing GitHub templates and checklist

**Files:**
- Modify: `.github/ISSUE_TEMPLATE/bug_report.yml`
- Create: `.github/ISSUE_TEMPLATE/x3_device_test.yml`
- Delete: `.github/FUNDING.yml`
- Create: `docs/release-checklist.md`

**Interfaces:**
- Consumes: the X3 hardware checks in `docs/pokemon-v2-beta.md`.
- Produces: issue intake and a release gate that cannot accidentally treat simulator success as physical acceptance.

- [ ] **Step 1: Focus the bug form on this project**

Remove CrossInk-specific Discussions/email language and unsupported-device choices. Ask for firmware version or commit, X3 hardware revision if known, orientation, exact reproduction steps, whether artwork is installed, and `crash_report.txt` when present.

- [ ] **Step 2: Add the X3 device-test form**

Collect test build SHA, installation path, artwork manifest status, navigation/selection rendering, reading-credit behaviour, save persistence, Pokédex card rendering, dashboard rendering, and crash-log result.

- [ ] **Step 3: Add the release checklist**

Require clean tracked-file and secret audits, successful targeted tests/builds, generated-art exclusion, save/rollback instructions, physical X3 acceptance, and a final diff against the commit used to build the binary. Keep release creation blocked until every required item has evidence.

- [ ] **Step 4: Validate YAML syntax**

Run a read-only YAML parse with the repository’s available Python YAML module if installed; otherwise inspect the files and report YAML parsing as unverified rather than installing a dependency.

- [ ] **Step 5: Remove the inherited funding link**

Delete `.github/FUNDING.yml` so the standalone project does not display a
donation link for an upstream maintainer who has not approved this repository.

### Task 5: Audit the complete local draft

**Files:**
- Review only: all files changed by Tasks 1–4.

**Interfaces:**
- Consumes: the full local draft.
- Produces: the exact text shown to the user for approval before any GitHub mutation.

- [ ] **Step 1: Audit tracked binary and artwork paths**

Run:

```powershell
git ls-files | rg -i "(^|/)(pokemon-art-source|pokemon-art-output|pokemon-release-local|\.crosspoint/pokemon)(/|$)|\.(bmp|png|zip|bin)$"
```

Expected: no Pokémon art pack, card archive, firmware binary, or save data is tracked. Existing unrelated documentation images must be identified rather than silently removed.

- [ ] **Step 2: Audit credentials and private paths**

Search only the newly drafted files for tokens, local absolute paths, private IP addresses, email addresses, and personal SD-card paths.

- [ ] **Step 3: Review the diff without staging unrelated work**

Run:

```powershell
git diff -- README.md NOTICE.md .gitignore docs/artwork-setup.md docs/third-party-assets.md docs/release-checklist.md .github/ISSUE_TEMPLATE/bug_report.yml .github/ISSUE_TEMPLATE/x3_device_test.yml docs/superpowers/specs/2026-08-31-xteink-pokemon-game-repository-design.md
```

Expected: only the approved public package is present.

- [ ] **Step 4: Present the complete public-facing text**

Show the user the README, notice, artwork-policy summary, issue templates, and release checklist. Do not create, push, or publish the GitHub repository until the user approves this review package.
