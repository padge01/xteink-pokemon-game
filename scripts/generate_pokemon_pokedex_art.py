#!/usr/bin/env python3
"""Create compact, high-contrast Pokédex cards for the private SD asset pack."""

from __future__ import annotations

import argparse
from pathlib import Path

try:
    from PIL import Image
except ImportError as error:  # pragma: no cover - environment-specific message
    raise SystemExit("Pillow is required: python -m pip install Pillow") from error


CARD_SIZES = {
    Path("pokedex/portrait"): (472, 708),
    Path("pokedex/landscape"): (288, 432),
}


def source_card(source: Path, species_id: int) -> Path:
    matches = sorted(source.glob(f"{species_id:03}-*.bmp"))
    if len(matches) != 1:
        raise ValueError(f"missing source card {species_id:03}" if not matches else
                         f"multiple source cards {species_id:03}")
    return matches[0]


def convert_card(source: Path, destination: Path, size: tuple[int, int]) -> None:
    with Image.open(source) as opened:
        grayscale = opened.convert("L")
        if grayscale.size != (528, 792):
            raise ValueError(f"unexpected source dimensions for {source.name}: {grayscale.size}")
        resized = grayscale.resize(size, Image.Resampling.LANCZOS)
        # The source cards use four grayscale levels. Darkening the two midtones
        # before one offline error-diffusion pass gives the X3's binary panel a
        # stronger result than repeatedly dithering the 8-bit file at runtime.
        gamma_lut = [round(255 * ((value / 255) ** 1.15)) for value in range(256)]
        one_bit = resized.point(gamma_lut).convert("1", dither=Image.Dither.FLOYDSTEINBERG)
        destination.parent.mkdir(parents=True, exist_ok=True)
        one_bit.save(destination, format="BMP")


def build_cards(source: Path, output: Path, species_count: int = 151) -> None:
    for species_id in range(1, species_count + 1):
        card = source_card(source, species_id)
        for relative, size in CARD_SIZES.items():
            convert_card(card, output / relative / f"{species_id:03}.bmp", size)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", type=Path, required=True, help="Folder containing 001-name.bmp through 151-name.bmp")
    parser.add_argument("--output", type=Path, required=True, help="Private Pokémon art-pack root")
    args = parser.parse_args()
    build_cards(args.source.resolve(), args.output.resolve())
    print(f"Generated {151 * len(CARD_SIZES)} Pokédex cards in {args.output.resolve()}")


if __name__ == "__main__":
    main()
