---
title: Third-party assets
nav_order: 20
---

# Third-party assets

## Pokémon and item artwork

Pokémon sprite images are © Nintendo/Creatures Inc./GAME FREAK Inc. Pokémon and related names and artwork belong to their respective owners. CrossInk is not affiliated with or endorsed by Nintendo, Creatures Inc., GAME FREAK Inc., The Pokémon Company, PokéSprite, or PokéAPI.

The optional private Pokémon demonstration pack uses pinned images from the following sources:

- Original 151 Pokémon wide icons: [PokeAPI sprites](https://github.com/PokeAPI/sprites) revision `4bc9d60186fe2e499ee2f3d4d1b796806cb99a67`, path `sprites/pokemon/versions/generation-vii/icons/{id}.png`.
- Moon, Fire, Thunder, Water, and Leaf Stones: [PokéSprite](https://github.com/msikma/pokesprite) revision `c5aaa610ff2acdf7fd8e2dccd181bca8be9fcb3e`, under `items/evo-item/`.
- Link Cable is intentionally rendered without an icon.

PokéSprite's program code and non-art materials are offered under its [MIT License](https://github.com/msikma/pokesprite/blob/master/LICENSE); that license does not replace the artwork ownership notice above.

Downloaded PNGs and converted BMPs are excluded from the public source repository and public firmware releases. The separately generated private-demo archive includes this declaration and a SHA-256 manifest. The release pack contains only IDs 001–151, five evolution-stone images, and their larger presentation versions; eggs and later-generation Pokémon are deliberately excluded.

Species icons are one-bit 40×30 BMPs and their presentation versions are 120×90. Stone icons are one-bit 32×32 BMPs and their presentation versions are 64×64. The release verifier rejects missing, extra, incorrectly sized, or non-one-bit artwork.
