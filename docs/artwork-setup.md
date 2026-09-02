# Artwork setup

The firmware reads Pokémon artwork from the SD card. Normal users should
download the complete X3 installation ZIP from the project release page; it
already contains the converted artwork in the correct folders.

The tools below are for maintainers rebuilding or validating that public
package. Keep source and converted working files outside normal Git history.

## Requirements

- Python 3
- [Pillow](https://pypi.org/project/pillow/)
- A local checkout or download of the pinned PokeAPI Sprites files
- A local checkout or download of the pinned PokéSprite files
- A local original-151 X3 Pokédex-card set or equivalent source cards

Install Pillow in your own Python environment:

```sh
python -m pip install Pillow
```

## 1. Obtain the source files

### Species icons

Use [PokeAPI Sprites](https://github.com/PokeAPI/sprites) revision
`4bc9d60186fe2e499ee2f3d4d1b796806cb99a67`.

The required files are:

```text
sprites/pokemon/versions/generation-vii/icons/1.png
...
sprites/pokemon/versions/generation-vii/icons/151.png
```

### Evolution stones

Use [PokéSprite](https://github.com/msikma/pokesprite) revision
`c5aaa610ff2acdf7fd8e2dccd181bca8be9fcb3e`.

Point the converter at the local `items/evo-item/` folder containing:

```text
moon-stone.png
fire-stone.png
thunder-stone.png
water-stone.png
leaf-stone.png
```

The Link Cable deliberately has no icon.

### Pokédex cards

The original X3 card set was shared by dmellok/xDaftTurtle in
[this Reddit post](https://www.reddit.com/r/XTEINK/comments/1ve0pr4/comment/p1lpy0w/?context=3).
Obtain it from the creator's link rather than from this repository.

The converter expects one 528×792 BMP for each species, named with a three-digit
Pokédex prefix:

```text
001-bulbasaur.bmp
...
151-mew.bmp
```

The creator publicly provided the download but did not attach an explicit
redistribution licence. Complete release archives include adapted cards with
direct credit and the removal process documented in
[`RIGHTS_AND_ATTRIBUTION.md`](../RIGHTS_AND_ATTRIBUTION.md).

## 2. Prepare ignored local folders

The following repository-root folders are ignored:

```text
pokemon-art-source/
pokemon-art-output/
pokemon-release-local/
pokemon-release-local.zip
```

One possible local layout is:

```text
pokemon-art-source/
├── pokeapi-sprites/
├── pokesprite/
└── pokedex-x3/
```

## 3. Convert species and item icons

Run from the repository root:

```sh
python scripts/generate_pokemon_icon_art.py \
  --pokemon-source pokemon-art-source/pokeapi-sprites/sprites/pokemon/versions/generation-vii/icons \
  --item-source pokemon-art-source/pokesprite/icons/evo-item \
  --output pokemon-art-output
```

This produces one-bit BMPs without contacting the network:

```text
pokemon-art-output/
├── sprites/             # 001.bmp–151.bmp, 40×30
├── heroes/              # 001.bmp–151.bmp, 120×90
├── items/               # five stones, 32×32
└── heroes/items/        # five stones, 64×64
```

## 4. Convert Pokédex cards

```sh
python scripts/generate_pokemon_pokedex_art.py \
  --source pokemon-art-source/pokedex-x3 \
  --output pokemon-art-output
```

This adds:

```text
pokemon-art-output/pokedex/portrait/001.bmp–151.bmp    # 472×708
pokemon-art-output/pokedex/landscape/001.bmp–151.bmp   # 288×432
```

All output is one-bit BMP data prepared on the computer so the X3 can stream it
without allocating another framebuffer.

## 5. Build and validate the public installation archive

Build the X3 firmware:

```sh
pio run -e pokemon-x3
```

Then package the exact firmware and local artwork:

```sh
python scripts/package_pokemon_v2_release.py \
  --source-pack pokemon-art-output \
  --firmware .pio/build/pokemon-x3/firmware.bin \
  --notice RIGHTS_AND_ATTRIBUTION.md \
  --version 0.1.0 \
  --output dist
```

The packager rejects missing files, incorrect dimensions, non-one-bit images,
unsafe archive paths, firmware mismatches, and manifest/checksum mismatches. It
produces:

```text
xteink-pokemon-x3-full-v0.1.0.zip
xteink-pokemon-x3-firmware-v0.1.0.bin
SHA256SUMS.txt
```

The full ZIP contains `update.bin`, `.crosspoint/pokemon/`,
`RIGHTS_AND_ATTRIBUTION.md`, and internal checksums. Publish it as a GitHub
Release asset, not as hundreds of tracked binary files.

## SD-card paths

Copy the generated `.crosspoint/pokemon/` tree to the SD-card root. The final
paths are:

```text
/.crosspoint/pokemon/sprites/
/.crosspoint/pokemon/heroes/
/.crosspoint/pokemon/items/
/.crosspoint/pokemon/heroes/items/
/.crosspoint/pokemon/pokedex/portrait/
/.crosspoint/pokemon/pokedex/landscape/
/.crosspoint/pokemon/manifest.json
```

See [Third-party assets](third-party-assets.md) and
[Rights and attribution](../RIGHTS_AND_ATTRIBUTION.md) for provenance,
ownership, and removal information. Attribution records provenance; it does not
grant permission or guarantee that a release cannot be removed.
