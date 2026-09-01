# Xteink Pokémon Game installer and release design

Date: 2026-08-31

## Goal

Publish a real X3 release that ordinary users can install without Python,
PlatformIO, a terminal, or manual artwork conversion.

The project will own its installer. CrossPoint's website is not a dependency.

## Release contents

Each GitHub release provides:

- `xteink-pokemon-x3.bin`: the exact X3 firmware tested for the release.
- `SHA256SUMS`: checksum for the firmware.
- Short release notes covering changes, known problems, and rollback.
- A link to the project's GitHub Pages installer.

Pokémon artwork is not included in the repository or release assets.

## Installation routes

An SD card must be inserted in the X3 before either route begins.

### Existing CrossInk or CrossPoint installation

This is the primary route.

1. Open the project's installer page.
2. Download the release firmware.
3. Start File Transfer on the X3.
4. Upload the firmware to the SD card using the X3 file manager.
5. On the X3, open **Settings > System > SD Card Firmware Update**.
6. Select the uploaded firmware and confirm.

The installer shows these instructions as a short guided flow. It does not
attempt cross-origin uploads from GitHub Pages to the X3's local HTTP server.

### USB installation or recovery

This is the secondary route for compatible desktop Chromium browsers.

1. Insert the SD card.
2. Connect the X3 to the computer.
3. Open the project's installer in Chrome or Edge.
4. Select the X3 and flash the release firmware.

The flasher will be based on the MIT-licensed CrossPoint Tools WebSerial
implementation. The project will retain the required licence and attribution.
It will load firmware from this project's GitHub releases and will not depend
on CrossPoint's hosted service.

Mobile Safari and browsers without WebSerial use the Wi-Fi/SD route.

## Artwork setup

After the firmware is installed, the X3 file-transfer website provides a
**Pokémon Setup** page.

The page runs in the phone or computer browser but is served by the X3. This
keeps artwork processing off the ESP32-C3 and lets the browser upload files to
the SD card through the existing same-origin file-transfer API.

The setup flow is:

1. The user presses **Install artwork**.
2. Fetch Pokémon data and source sprites directly from the public PokeAPI
   endpoints and repositories.
3. Validate that the expected source data and images are present.
4. Render icons, item art, and Pokédex cards locally in the browser, then
   convert them to the required monochrome BMP sizes.
5. Create `/.crosspoint/pokemon/` and its subfolders.
6. Upload generated files through the existing WebSocket uploader, with HTTP
   upload as a fallback.
7. Upload the manifest last.
8. Verify the manifest, file count, required paths, dimensions, and checksums.
9. Display **Artwork installed** or a specific repair instruction.

The normal flow does not ask the user to find, download, or select artwork
archives. Artwork setup requires an internet-connected network. A manual
source-file option may remain in developer documentation for diagnosing an
unavailable upstream source, but it is not part of the main installer.

The installer does not depend on the Catbox sleep-screen ZIP. That host does
not currently allow browser code to read the archive across origins. The
installer instead recreates the approved X3 card layout locally from PokeAPI
data and sprites, which are available to browser code through CORS.

The firmware's Pokémon menu reports **Artwork setup required** when the pack is
missing or incomplete and points the user to File Transfer > Pokémon Setup.

## Artwork boundary

The project does not host or package Pokémon artwork. The installer fetches
artwork from the original public locations at the user's request, processes it
locally in the browser, and sends the generated files directly to their X3.

The source manifest records the original URL and attribution for every asset
set. A release must not silently replace a source or start mirroring its files.

Python artwork scripts remain optional maintainer and CI reference tools. They
are removed from end-user installation instructions.

## Installer site

GitHub Pages hosts a small static site with:

- **Install over Wi-Fi**
- **Flash over USB**
- **Download firmware**
- **Rollback**
- A compact device and browser compatibility note

The site retrieves release metadata and firmware from this repository's GitHub
releases. It contains no build service, account system, analytics, or admin
dashboard.

## Public repository structure

The front README contains only:

- One-sentence description
- Install button
- Short feature list
- X3 and SD-card requirements
- Credits and licence links

Developer builds, tests, internal validation, beta procedures, file formats,
and artwork implementation details move under `docs/`.

Release readiness is communicated through GitHub's prerelease/release state,
not repeated warnings throughout the README.

## Firmware base wording

The repository states:

- Based on CrossInk v1.5.0.
- Incorporates the released CrossPoint v1.5.0 improvements adapted for
  CrossInk.
- Includes selected later CrossPoint 1.6 RC parity work adapted for the X3.
- Is an independent CrossInk-based build, not an official CrossPoint release.

## Failure handling

- Firmware downloads include a SHA-256 checksum.
- The USB flasher validates the X3 partition layout before writing.
- Failed artwork uploads leave the manifest absent or invalid, so incomplete
  packs are never reported as installed.
- Uploads can resume by skipping files whose path and checksum already match.
- Artwork setup never edits books, reading progress, or Pokémon save snapshots.
- Rollback instructions always link to a known-good X3 firmware source.

## Verification

Before publishing a release:

1. Build the exact `pokemon-x3` release binary.
2. Verify its checksum and GitHub release download.
3. Test Wi-Fi/SD installation on a physical X3 with the SD card inserted.
4. Test USB installation on a physical X3 with the SD card inserted.
5. Test artwork setup from a clean SD-card state.
6. Interrupt one artwork upload and verify safe resume.
7. Verify missing artwork produces the setup prompt, not a crash.
8. Verify rollback using the documented route.

Simulator and host tests support these checks but do not replace physical X3
installation testing.

## Out of scope

- Hosting Pokémon artwork.
- Building firmware on demand in the browser or on a server.
- Supporting X4, X4 Pro, or other devices in the first release.
- Reproducing CrossPoint's beta catalog, font builder, admin dashboard, or
  Cloudflare infrastructure.
