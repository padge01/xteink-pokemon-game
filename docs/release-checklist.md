# X3 release checklist

Use this checklist for one exact commit and one exact firmware binary. A check
performed on another build is not evidence for the release candidate.

## Repository

- [ ] The release commit is identified.
- [ ] The working tree contains no unreviewed release changes.
- [ ] Submodules resolve to recorded commits.
- [ ] `LICENSE` preserves the inherited MIT notice.
- [ ] `NOTICE.md` and `docs/third-party-assets.md` match the sources actually
      used.
- [ ] No Pokémon images, converted cards, artwork packs, firmware binaries,
      personal saves, SD-card contents, credentials, or private paths are
      tracked in Git. Converted artwork is distributed only through the
      reviewed GitHub Release archive.
- [ ] Inherited GitHub workflows have been reviewed for this repository and
      cannot publish CrossInk, Sticky, X4, font, Pages, or catalog artifacts by
      mistake.
- [ ] Inherited funding links have been removed or explicitly approved by the
      named recipient.

## Full-install artwork and packaging

- [ ] The source revisions match `docs/third-party-assets.md`.
- [ ] `scripts/generate_pokemon_icon_art.py` completed locally.
- [ ] `scripts/generate_pokemon_pokedex_art.py` completed locally.
- [ ] The canonical local artwork directory contains all 614 required one-bit
      BMPs with the documented dimensions.
- [ ] `scripts/package_pokemon_v2_release.py` accepted the complete local pack
      and included `RIGHTS_AND_ATTRIBUTION.md`.
- [ ] The full archive contains `update.bin`, `.crosspoint/pokemon/`, the
      artwork manifest, internal checksums, and the rights notice.
- [ ] The full archive does not contain `pokemon-a.bin`, `pokemon-b.bin`,
      legacy `pokemon-v2-*.bin` saves, books, sleep screens, settings, or cache
      files.
- [ ] Extracting the full archive over temporary existing Pokémon saves leaves
      both save hashes unchanged.
- [ ] The firmware inside the full archive is byte-identical to the separately
      published firmware-only asset.
- [ ] The release-level `SHA256SUMS.txt` covers both public downloads.

## Automated checks

- [ ] Pokémon host tests pass.
- [ ] Artwork generator and packager tests pass.
- [ ] Full-archive layout, path-safety, manifest, checksum, and
      save-preservation tests pass.
- [ ] Portrait Pokémon simulator smoke route passes.
- [ ] Landscape Pokémon simulator smoke route passes.
- [ ] `pio run -e pokemon-x3` succeeds.
- [ ] The SHA-256 of the tested firmware binary is recorded.
- [ ] Test commands and results are copied into the draft release notes.

Automated checks do not approve a hardware release.

## Physical X3 acceptance

- [ ] Back up the SD card and both Pokémon snapshot files.
- [ ] Keep a known-good rollback firmware and verified recovery path available.
- [ ] Install the exact binary whose SHA-256 was recorded above.
- [ ] Complete starter, gender, and optional nickname setup with the physical
      side and front buttons.
- [ ] Confirm the header does not clip, selected rows retain their sprites, and
      list navigation works through every page.
- [ ] Confirm Party movement, PC deposit/withdrawal, PC sorting, Bag, Pokédex,
      and two-step reset.
- [ ] Open seen Pokédex entries in portrait and landscape and inspect contrast
      and load time.
- [ ] Read with real page turns through at least one five-minute checkpoint,
      exit the reader, and confirm EXP.
- [ ] Leave a book open without turning pages and confirm that no EXP is added.
- [ ] Reboot and confirm Party and EXP persist.
- [ ] Inspect supported dashboards in portrait and landscape.
- [ ] Open and close Pokémon repeatedly after leaving a book.
- [ ] Confirm that no new `crash_report.txt` was created.
- [ ] Record the completed results in an X3 device-test issue.

## Publication

- [ ] Review the final commit diff.
- [ ] Review the exact GitHub Actions configuration on the release commit.
- [ ] Confirm converted artwork is present only in the full-install Release
      ZIP and is not committed to Git history.
- [ ] Confirm the release includes the full ZIP, firmware-only binary, and
      `SHA256SUMS.txt` with the approved public filenames.
- [ ] Confirm release notes link to `RIGHTS_AND_ATTRIBUTION.md` and the Rights
      or Attribution issue form.
- [ ] Put backup, installation, rollback, known limitations, test evidence, and
      all public asset SHA-256 values in the release notes.
- [ ] Download every published asset again, verify its SHA-256, open the full
      ZIP, and rerun archive verification against the download.
- [ ] Confirm the live GitHub Pages primary button downloads the full ZIP and
      the secondary button downloads the firmware-only binary.
- [ ] Mark the release as a beta.
- [ ] Publish only after the physical X3 report passes every required item.
