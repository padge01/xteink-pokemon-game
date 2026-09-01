# Pokémon for Xteink X3

Reading-powered Pokémon collection for the Xteink X3.

[Download the full X3 install](https://padge01.github.io/xteink-pokemon-game/install.html)

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

The install guide provides two downloads:

- **Full install:** one ZIP containing `update.bin` and the complete
  `/.crosspoint/pokemon/` artwork folder. Extract it into the SD-card root,
  return the card to the X3, then run **Settings → System → SD Card Firmware
  Update → update.bin**.
- **Firmware only:** for an X3 that already has the artwork folder. Upload the
  `.bin` through File Transfer or copy it to the SD-card root.

The full ZIP contains no Pokémon saves, books, or reading data. Merge the
`.crosspoint` folder; do not delete the existing folder. Back up
`pokemon-v2-a.bin` and `pokemon-v2-b.bin` before updating.

## Firmware base

- Based on [CrossInk](https://github.com/uxjulia/CrossInk).
- Includes CrossPoint 1.5 release improvements adapted for CrossInk.
- Includes selected later CrossPoint 1.6 RC improvements adapted for the X3.
- Built and released independently; it is not an official CrossInk or CrossPoint build.

## Credits

- [CrossPoint Reader](https://github.com/crosspoint-reader/crosspoint-reader): original firmware and upstream improvements.
- [Joshua Miller's CrossPoint Reader Companion](https://github.com/JoshuaMillerCode/crosspoint-reader-companion): reading-companion concept and real-reading behavior.
- [PokeAPI Sprites](https://github.com/PokeAPI/sprites): Pokémon sprite source used during browser artwork setup.
- [Tesserae](https://github.com/dmellok/tesserae) and [u/xDaftTurtle's Pokédex cards](https://www.reddit.com/r/XTEINK/comments/1ve0pr4/comment/p1lpy0w/?context=3): Pokédex-card reference.

Full notices are in [NOTICE.md](NOTICE.md) and
[Rights and attribution](RIGHTS_AND_ATTRIBUTION.md). Project code is covered by
the inherited [MIT License](LICENSE).

Pokémon and related names, characters, and artwork belong to their respective rights holders. This is an unofficial, noncommercial project and is not affiliated with or endorsed by Nintendo, Creatures Inc., GAME FREAK Inc., The Pokémon Company, Xteink, CrossInk, or CrossPoint Reader.

## Development

- [Release checklist](docs/release-checklist.md)
- [Artwork contract](docs/artwork-setup.md)
- [File formats](docs/file-formats.md)
