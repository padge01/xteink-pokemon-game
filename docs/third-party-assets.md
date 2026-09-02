---
title: Third-party assets
nav_order: 20
---

# Third-party assets

Converted artwork remains outside normal Git history. Complete GitHub Release
archives include the tested firmware and the X3-adapted artwork required by the
game. Maintainers rebuild that package with the tools described in
[Artwork setup](artwork-setup.md).

Attribution records where material came from. It does not grant permission to
redistribute Pokémon intellectual property.

## Species icons

The local pack uses the original-151 Generation VII 40×30 icon files from
[PokeAPI Sprites](https://github.com/PokeAPI/sprites) revision
`4bc9d60186fe2e499ee2f3d4d1b796806cb99a67`, under
`sprites/pokemon/versions/generation-vii/icons/`.

PokeAPI Sprites says its repository is distributed under CC0 1.0 while also
stating that all image contents are copyright The Pokémon Company. Its licence
disclaims responsibility for clearing third-party rights. The CC0 declaration
therefore must not be represented as this project's permission to redistribute
the Pokémon images.

## Evolution-stone icons

The local pack uses Moon, Fire, Thunder, Water, and Leaf Stone files from
[PokéSprite](https://github.com/msikma/pokesprite) revision
`c5aaa610ff2acdf7fd8e2dccd181bca8be9fcb3e`, under `items/evo-item/`.

PokéSprite applies the MIT License to its program code and non-art materials.
Its documentation separately states that the sprite images are copyright
Nintendo/Creatures Inc./GAME FREAK Inc. The MIT licence does not replace that
artwork ownership notice.

The Link Cable is intentionally rendered without an icon.

## Pokédex cards

[dmellok/xDaftTurtle](https://github.com/dmellok) created an original-151
Pokédex-card set with [Tesserae](https://github.com/dmellok/tesserae) and
publicly shared X3 files in
[this Reddit post](https://www.reddit.com/r/XTEINK/comments/1ve0pr4/comment/p1lpy0w/?context=3).
The post states that the cards use PokéAPI data and sprites.

The public download is the design and conversion source for this project, but
it does not state a licence allowing this project to rehost the archive.
Complete X3 installation releases nevertheless include adapted cards with
direct credit to dmellok/u/xDaftTurtle and a documented removal or
credit-correction process.

Tesserae itself is AGPL-3.0-or-later. This repository does not copy or embed
Tesserae code; crediting Tesserae does not change the rights attached to the
rendered Pokémon content.

## Generated local layout

The offline tools create only the files needed by the X3:

- Species icons: one-bit 40×30 BMPs.
- Large species presentation art: one-bit 120×90 BMPs.
- Stone icons: one-bit 32×32 BMPs.
- Large stone presentation art: one-bit 64×64 BMPs.
- Pokédex cards: one-bit 472×708 portrait and 288×432 landscape BMPs.

The validator accepts IDs 001–151 and the five named evolution stones. Eggs,
later-generation Pokémon, extra BMPs, and incorrectly sized files are rejected.
The public ZIP contains only this generated tree, the tested firmware, internal
checksums, and [the rights notice](../RIGHTS_AND_ATTRIBUTION.md). It contains no
Pokémon saves, books, reader settings, caches, or sleep-screen files.

## Ownership and affiliation

Pokémon and related names, characters, and artwork belong to their respective
rights holders, including Nintendo, Creatures Inc., GAME FREAK Inc., and The
Pokémon Company.

Xteink Pokémon Game is not affiliated with or endorsed by those rights
holders, Xteink, PokéAPI, PokéSprite, Tesserae, CrossInk, or CrossPoint Reader.
