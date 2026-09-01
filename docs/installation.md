---
title: Installation
nav_order: 2
---

# Installation

## Supported Devices

- Xteink X3 only

Do not install this build on an X4, X4 Pro, Sticky, or another device.

Download the current X3 package from the
[project install guide](https://padge01.github.io/xteink-pokemon-game/install.html)
or the [GitHub releases page](https://github.com/padge01/xteink-pokemon-game/releases).

## Full installation

1. Download `xteink-pokemon-x3-full-v<version>.zip`.
2. Power off the X3 and insert its SD card into a computer.
3. Back up `/.crosspoint/pokemon-v2-a.bin` and `pokemon-v2-b.bin` if present.
4. Extract the ZIP into the SD-card root. Merge `.crosspoint`; do not delete
   the existing folder.
5. Safely eject the card and return it to the X3.
6. Open **Settings → System → SD Card Firmware Update**.
7. Select `update.bin` and confirm.

The full ZIP contains the firmware and required artwork. It contains no saves,
books, reader configuration, or reading data.

## Firmware-only Wi-Fi update

Use this only when `/.crosspoint/pokemon/` is already installed.

1. Download `xteink-pokemon-x3-firmware-v<version>.bin`.
2. Leave the SD card inside the X3 and open **File Transfer**.
3. Open the address displayed by the X3 in a browser.
4. Upload the `.bin` to the SD-card root.
5. Exit File Transfer.
6. Open **Settings → System → SD Card Firmware Update**, select the file, and confirm.

The firmware update always runs on the X3. An SD card reader or File Transfer
only copies files to the card.
