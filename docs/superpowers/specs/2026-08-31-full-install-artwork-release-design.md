# Full X3 install and artwork release design

Date: 2026-08-31

This design supersedes the no-artwork release policy in
`2026-08-31-x3-installer-release-design.md` and
`2026-08-31-xteink-pokemon-game-repository-design.md`. It also reflects the
confirmed X3 installation path: SD-card firmware update only, with no USB
flashing support.

## Goal

Give a new Xteink X3 user one release ZIP that contains the tested firmware and
all artwork required by the Pokémon game. Installation must not require Python,
PlatformIO, image conversion, or a separate artwork download.

This design records the decision to publish the adapted artwork as part of an
unofficial, noncommercial fan project despite the absence of an explicit
Pokémon intellectual-property licence. Attribution and a removal process reduce
confusion and make good-faith correction possible; they do not grant permission
or prevent a rights holder from requesting removal or filing a takedown.

## Release assets

Each release provides:

- `xteink-pokemon-x3-full-v<version>.zip`, the recommended new-user download.
- `xteink-pokemon-x3-firmware-v<version>.bin`, for users who already have a
  complete artwork folder.
- `SHA256SUMS.txt`, covering both public downloads.

The full-install archive contains:

```text
update.bin
.crosspoint/pokemon/
  sprites/
  heroes/
  items/
  heroes/items/
  pokedex/portrait/
  pokedex/landscape/
  manifest.json
RIGHTS_AND_ATTRIBUTION.md
SHA256SUMS.txt
```

The archive never contains books, reader configuration, EPUB caches, sleep
screens, `pokemon-v2-a.bin`, `pokemon-v2-b.bin`, or any other save data.

## Installation and updates

The README and GitHub Pages install guide lead with **Download full install**.
The user extracts the archive into the SD-card root and allows folders to merge.
They must not delete the existing `.crosspoint` folder.

After returning the card to the X3, the user opens **Settings > System > SD
Card Firmware Update**, selects `update.bin`, and confirms. The X3 has no USB
flashing route.

Existing users may install the firmware-only file as `update.bin`. Full-install
archives remain safe for updates because they overwrite only the versioned
artwork paths and firmware file, not Pokémon saves or reading data. The guide
still tells users to back up `pokemon-v2-a.bin` and `pokemon-v2-b.bin` before an
update.

## Artwork sources and credits

The release records the exact source URL and revision for each asset family:

- Original-151 Generation VII 40x30 species icons from PokeAPI Sprites.
- Adapted 120x90 presentation images generated from those species icons.
- Evolution-stone icons from PokéSprite, with larger presentation variants
  generated for the X3.
- Original-151 X3 Pokédex card layout and rendered cards by
  [dmellok / u/xDaftTurtle](https://github.com/dmellok), created with
  [Tesserae](https://github.com/dmellok/tesserae) using PokéAPI data and
  sprites, and shared in the
  [original Reddit post](https://www.reddit.com/r/XTEINK/comments/1ve0pr4/comment/p1lpy0w/?context=3).

The notice distinguishes the Pokédex creator's layout and rendered X3 files
from the underlying Pokémon names, designs, data, and sprites. It does not say
that the creator granted a licence or owns the underlying Pokémon material.

## Rights and removal notice

`RIGHTS_AND_ATTRIBUTION.md`, the README, the release notes, and the full-install
ZIP include or link to this statement:

> This is an unofficial, noncommercial fan project. Pokémon and related names,
> characters, designs, and artwork belong to their respective rights holders.
> This project is not affiliated with or endorsed by Nintendo, Creatures Inc.,
> GAME FREAK Inc., or The Pokémon Company.
>
> Artwork has been adapted for the Xteink X3 e-ink display. Source credits are
> listed below. If you are a rights holder or original contributor and want an
> asset removed, replaced, or credited differently, please open a Rights or
> Attribution issue. We will respond promptly.

The project does not label the artwork as licensed, public-domain, or fair use.
It does not use the notice as a substitute for permission.

## Contact and removal process

The repository adds a **Rights or attribution request** issue template. It asks
for the affected asset, the requested action, and a private contact route only
if the requester chooses to provide one. The README links to the template and
to GitHub's own copyright-reporting process. A personal email address is not
required in the repository.

When a credible creator or rights-holder request arrives, the maintainer will:

1. Acknowledge the request promptly.
2. Remove the affected public release asset while it is reviewed.
3. Replace the artwork with an approved asset or a legible placeholder.
4. Rebuild and republish the full-install archive with a new checksum.
5. Correct the attribution documents and future releases.

The project does not promise to recall copies that users already downloaded.

## Packaging and publication

Converted artwork remains outside normal Git history. The existing release
packager assembles a local canonical artwork pack and the tested firmware into
the full ZIP. Release assets are the distribution boundary. This avoids adding
hundreds of binary revisions to the source repository and permits a disputed
pack to be removed without rewriting Git history.

The packager must validate before publication:

- IDs 001 through 151 exist for every required species-art family.
- All five evolution-stone files exist in both required sizes.
- Every BMP has the expected dimensions and one-bit format.
- `manifest.json` lists every distributed artwork file and its SHA-256 hash.
- `update.bin` is byte-identical to the separately published firmware file.
- Neither Pokémon save file nor any unrelated SD-card content is present.
- The archive contains the current rights and attribution notice.

Release publication is manual until the artwork source can be provided to CI
without putting it in Git history. The release checklist records the local
canonical pack hash so the package remains reproducible by the maintainer.

## Repository and site changes

- Lead the README with one **Download full install** link.
- Retain a secondary **Firmware only** link for updates.
- Replace the current claim that releases exclude artwork.
- Update installation, artwork, third-party-asset, and release-checklist docs.
- Add `RIGHTS_AND_ATTRIBUTION.md` and the rights-request issue template.
- Update the GitHub Pages guide to explain extraction to the SD-card root.
- Add a changelog entry for the full-install release.

The installer site remains a short guide and download surface. It does not add
WebSerial, WebUSB, accounts, analytics, or an in-browser art pipeline.

## Verification before public release

1. Run the packager and its artwork/archive tests.
2. Extract the release into an empty temporary SD-card tree and verify the
   expected paths, file counts, hashes, dimensions, and absence of saves.
3. Extract it over a temporary tree containing both Pokémon save snapshots and
   verify their hashes do not change.
4. Download the published release assets and verify their SHA-256 hashes.
5. Install the downloaded full package on the physical X3 and confirm firmware
   version, starter onboarding, dashboard artwork, party artwork, item artwork,
   and Pokédex cards.
6. Update the same X3 over an existing save and confirm progress remains intact.
7. Verify the README and GitHub Pages buttons resolve to the current release.

## Out of scope

- Claiming legal permission or guaranteeing that the release cannot be
  removed.
- Embedding the artwork inside the ESP32 firmware image.
- Supporting devices other than the Xteink X3 in this release.
- Restoring USB flashing to the X3-only install guide.
- Publishing private books, saves, reader configuration, or sleep-screen files.
