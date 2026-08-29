# Pokemon Companion V2 Hardware and Release Plan

> Begin only after core tests and UI visual approval. All long checks need visible approval before execution.

**Goal:** Prove the clean base and V2 separately on the physical X3, then produce a recoverable private beta package.

## Task 1: Isolate the existing reader-menu crash with clean firmware

- Use the preserved firmware, ELF, and hashes recorded in `docs/superpowers/evidence/2026-08-29-pokemon-v2-clean-baseline.md`.
- Back up the SD card, V1 persistence, books, artwork, and `crash_report.txt`.
- Flash the checksummed clean non-Pokemon baseline.
- Open and close menus repeatedly from EPUB, TXT, and XTC; also exercise orientation and dashboard navigation.
- If it crashes, decode the new report against the clean ELF and fix that base defect before proceeding.
- Record the exact firmware hash and outcome.

## Task 2: Build and measure the integrated candidate

- Use the private `pokemon-x3` environment and compile guards created at the core gate.
- Build simulator once, then X3 candidate once; run static analysis only on touched code.
- Compare image size to the clean baseline and retain at least 128 KB partition headroom.
- Instrument activity entry/exit internal free heap, largest internal block, and touched task stack high-water marks in beta builds.
- Use the simulator for lifecycle/navigation regression checks only. On the physical X3, open and close the activity twenty times and reject internal-heap or largest-block drift above 2 KB; simulator memory is not acceptance evidence for ESP32-C3 heap recovery.
- Commit: `chore: measure pokemon v2 private candidate`.

## Task 3: Run the V2 physical beta matrix

- Flash the checksummed V2 candidate and install the art pack.
- Test fresh onboarding, chosen gender, nickname Yes/No, Party/PC/Bag/Summary/Pokedex, wraparound, full paging, and reset.
- Verify all four dashboards on real e-ink for contrast, spacing, level/EXP readability, and blank notices.
- Read with real page turns and confirm credited XP; leave a book idle and confirm zero XP; exit between checkpoints and confirm valid remainder.
- Use host-generated valid snapshot fixtures for encounter, item, evolution, full Party, PC paging, legendary, and 150-caught/Mew scenarios. Do not add cheat menus to production firmware.
- Reboot around save operations and confirm the older valid snapshot survives interruption.
- Any crash, save failure, lost record, broken navigation, or unreadable dashboard blocks release.

## Task 4: Package and document the private beta

- Produce `firmware-x3x4.bin`, `/pokemon/`, SHA-256 sums, and a known-good rollback firmware.
- Document wireless file-transfer installation, SD backup/restore, reset, save filenames, and V1 incompatibility.
- Update `docs/file-formats.md`, `docs/third-party-assets.md`, `README` private-build notes, and `CHANGELOG.md`.
- Verify a clean package extraction contains only declared artifacts and no local/generated build debris.
- Commit: `docs: package pokemon companion v2 beta`.

## Release gate

- Targeted tests, simulator build, X3 build, clean-baseline hardware test, and V2 hardware matrix all pass with recorded evidence.
- Repository status contains no unexpected files.
- Do not merge, publish, upload, or replace the user's rollback image without explicit approval.
