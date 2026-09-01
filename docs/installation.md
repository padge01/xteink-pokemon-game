---
title: Installation
nav_order: 2
---

# Installation

## Supported Devices

- Xteink X3 only

Do not install this build on an X4, X4 Pro, Sticky, or another device.

Download the current X3 `.bin` from the
[project install guide](https://padge01.github.io/xteink-pokemon-game/install.html)
or the [GitHub releases page](https://github.com/padge01/xteink-pokemon-game/releases).

## Wi-Fi update

1. Leave the SD card inside the X3 and open **File Transfer**.
2. Open the address displayed by the X3 in a browser.
3. Upload the downloaded `.bin` to the SD-card root.
4. Exit File Transfer.
5. Open **Settings → System → SD Card Firmware Update**.
6. Select the `.bin` and confirm.

## SD card reader

1. Power off the X3 and remove its SD card.
2. Insert the card in the computer.
3. Back up `/.crosspoint/pokemon-v2-a.bin` and `pokemon-v2-b.bin`.
4. Copy the downloaded `.bin` to the card root.
5. Safely eject the card and return it to the X3.
6. Open **Settings → System → SD Card Firmware Update**, select the file, and confirm.

The card reader is only used to copy files. The firmware update runs on the X3.

## Artwork

Firmware releases do not include Pokémon artwork. Preserve an existing
`/.crosspoint/pokemon/` folder during updates. For a new installation, follow
[Artwork setup](artwork-setup.md).
