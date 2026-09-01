---
title: Installation
nav_order: 2
---

# Installation

## Supported device

Xteink X3 only. Do not install this build on an X4, X4 Pro, Sticky, or another device.

Download the current package from the [download and install guide](https://padge01.github.io/xteink-pokemon-game/install.html) or the [GitHub releases page](https://github.com/padge01/xteink-pokemon-game/releases).

## Full installation

1. Download `xteink-pokemon-x3-full-v<version>.zip` to a computer.
2. Extract the ZIP. Do not copy the ZIP file itself to the SD card.
3. Power off the X3 and put its SD card in the computer.
4. Back up `/.crosspoint/pokemon-v2-a.bin` and `/.crosspoint/pokemon-v2-b.bin` if present.
5. Copy the extracted `update.bin` and `.crosspoint` folder to the SD-card root.
6. Merge `.crosspoint`; do not replace or delete the existing folder.
7. Confirm `update.bin` and `.crosspoint` are directly at the SD-card root.
8. Safely eject the card and return it to the X3.
9. Open **Settings → System → SD Card Firmware Update**, select `update.bin`, and confirm.

The full ZIP contains the firmware and required artwork. It contains no Pokémon saves, books, reader configuration, or reading data.

## Firmware-only Wi-Fi update

Use this only when `/.crosspoint/pokemon/` is already installed.

1. Download `xteink-pokemon-x3-firmware-v<version>.bin`.
2. Leave the SD card inside the X3 and open **File Transfer**.
3. Open the address displayed by the X3 in a browser.
4. Upload the `.bin` to the SD-card root.
5. Exit File Transfer.
6. Open **Settings → System → SD Card Firmware Update**, select the file, and confirm.

File Transfer only copies the firmware to the card. The firmware update always runs from the X3's settings.
