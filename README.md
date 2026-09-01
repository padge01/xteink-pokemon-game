# Pokémon for Xteink X3

Reading-powered Pokémon collection for the Xteink X3.

[Install the latest release](https://padge01.github.io/xteink-pokemon-game/install.html)

## Requirements

- Xteink X3
- SD card inserted in the X3
- SD-card backup before updating

Do not install this build on an X4, X4 Pro, or another device.

## Features

- Choose Bulbasaur, Charmander, Squirtle, or Pikachu as a first partner.
- Train the lead Pokémon through real reading activity.
- Carry six Pokémon and store the rest in the PC Box.
- Catch and evolve Pokémon from the original 151.
- Track seen and caught Pokémon in the Pokédex.
- Find evolution stones and Link Cables while reading.
- Show the lead Pokémon on supported CrossInk dashboards.
- Back up or reset Pokémon progress without changing books or reading statistics.

No battles, moves, breeding, trading, or combat statistics are included.

## Install

The installer provides two routes:

- **Wi-Fi update:** download the firmware, upload it through the X3 File Transfer page, then open **Settings → System → SD Card Firmware Update**.
- **Desktop USB:** connect the X3 with its SD card inserted and use Chrome or Edge.

After updating, open **File Transfer → Pokémon Setup** to create and install the artwork directly from the browser. Users do not need Python, a build environment, or a separate artwork archive.

## Firmware base

- Based on [CrossInk](https://github.com/uxjulia/CrossInk).
- Includes CrossPoint 1.5 release improvements adapted for CrossInk.
- Includes selected later CrossPoint 1.6 RC improvements adapted for the X3.
- Built and released independently; it is not an official CrossInk or CrossPoint build.

## Credits

- [CrossPoint Reader](https://github.com/crosspoint-reader/crosspoint-reader): original firmware and upstream improvements.
- [CrossPoint Tools](https://github.com/crosspoint-reader/crosspoint-tools): MIT-licensed browser flasher.
- [Joshua Miller's CrossPoint Reader Companion](https://github.com/JoshuaMillerCode/crosspoint-reader-companion): reading-companion concept and real-reading behavior.
- [PokeAPI Sprites](https://github.com/PokeAPI/sprites): Pokémon sprite source used during browser artwork setup.
- [Tesserae](https://github.com/dmellok/tesserae) and [u/xDaftTurtle's Pokédex cards](https://www.reddit.com/r/XTEINK/comments/1ve0pr4/comment/p1lpy0w/?context=3): Pokédex-card reference.

Full notices are in [NOTICE.md](NOTICE.md). Project code is covered by the inherited [MIT License](LICENSE).

Pokémon and related names, characters, and artwork belong to their respective rights holders. This is an unofficial, noncommercial project and is not affiliated with or endorsed by Nintendo, Creatures Inc., GAME FREAK Inc., The Pokémon Company, Xteink, CrossInk, or CrossPoint Reader.

## Development

- [Release checklist](docs/release-checklist.md)
- [Artwork contract](docs/artwork-setup.md)
- [File formats](docs/file-formats.md)
