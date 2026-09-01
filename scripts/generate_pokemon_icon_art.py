#!/usr/bin/env python3
"""Convert user-supplied Pokémon and item PNGs into the X3 SD artwork layout."""

from __future__ import annotations

import argparse
from pathlib import Path

try:
    from PIL import Image
except ImportError as error:  # pragma: no cover - environment-specific message
    raise SystemExit("Pillow is required: python -m pip install Pillow") from error


ITEMS = ("moon-stone", "fire-stone", "thunder-stone", "water-stone", "leaf-stone")


def species_source(root: Path, species_id: int) -> Path:
    for name in (f"{species_id}.png", f"{species_id:03}.png"):
        candidate = root / name
        if candidate.is_file():
            return candidate
    raise ValueError(f"missing species icon {species_id:03}")


def item_source(root: Path, item: str) -> Path:
    candidate = root / f"{item}.png"
    if not candidate.is_file():
        raise ValueError(f"missing item icon {item}")
    return candidate


def one_bit_canvas(source: Path, size: tuple[int, int]) -> Image.Image:
    with Image.open(source) as opened:
        icon = opened.convert("RGBA")
    if icon.width > size[0] or icon.height > size[1]:
        icon.thumbnail(size, Image.Resampling.NEAREST)
    canvas = Image.new("RGBA", size, (255, 255, 255, 255))
    canvas.alpha_composite(icon, ((size[0] - icon.width) // 2, (size[1] - icon.height) // 2))
    return canvas.convert("L").convert("1", dither=Image.Dither.FLOYDSTEINBERG)


def save_pair(source: Path, small: Path, large: Path, small_size: tuple[int, int], scale: int) -> None:
    rendered = one_bit_canvas(source, small_size)
    small.parent.mkdir(parents=True, exist_ok=True)
    rendered.save(small, format="BMP")
    large.parent.mkdir(parents=True, exist_ok=True)
    rendered.resize(
        (small_size[0] * scale, small_size[1] * scale), Image.Resampling.NEAREST
    ).save(large, format="BMP")


def build_icons(
    pokemon_source: Path,
    item_source_root: Path,
    output: Path,
    species_count: int = 151,
    items: tuple[str, ...] = ITEMS,
) -> None:
    for species_id in range(1, species_count + 1):
        save_pair(
            species_source(pokemon_source, species_id),
            output / "sprites" / f"{species_id:03}.bmp",
            output / "heroes" / f"{species_id:03}.bmp",
            (40, 30),
            3,
        )
    for item in items:
        save_pair(
            item_source(item_source_root, item),
            output / "items" / f"{item}.bmp",
            output / "heroes/items" / f"{item}.bmp",
            (32, 32),
            2,
        )


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--pokemon-source",
        type=Path,
        required=True,
        help="Local folder containing 1.png through 151.png (zero padding is also accepted)",
    )
    parser.add_argument(
        "--item-source",
        type=Path,
        required=True,
        help="Local PokéSprite evo-item folder containing the five stone PNGs",
    )
    parser.add_argument("--output", type=Path, required=True, help="Local Pokémon art-pack root")
    args = parser.parse_args()
    build_icons(args.pokemon_source.resolve(), args.item_source.resolve(), args.output.resolve())
    print(f"Generated species and item artwork in {args.output.resolve()}")


if __name__ == "__main__":
    main()
