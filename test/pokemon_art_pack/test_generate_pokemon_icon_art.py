#!/usr/bin/env python3

import importlib.util
import tempfile
import unittest
from pathlib import Path

from PIL import Image

REPO_ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location(
    "generate_pokemon_icon_art", REPO_ROOT / "scripts/generate_pokemon_icon_art.py"
)
GENERATOR = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(GENERATOR)


def write_rgba_icon(path: Path, size: tuple[int, int]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    image = Image.new("RGBA", size, (255, 255, 255, 0))
    width, height = size
    image.putpixel((width // 2, height // 2), (0, 0, 0, 255))
    image.save(path)


class PokemonIconArtGeneratorTest(unittest.TestCase):
    def test_builds_species_and_item_sizes_as_one_bit_bmps(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            pokemon_source = root / "pokemon"
            item_source = root / "items"
            output = root / "output"
            write_rgba_icon(pokemon_source / "1.png", (40, 30))
            write_rgba_icon(item_source / "fire-stone.png", (32, 32))

            GENERATOR.build_icons(
                pokemon_source,
                item_source,
                output,
                species_count=1,
                items=("fire-stone",),
            )

            expected = {
                output / "sprites/001.bmp": (40, 30),
                output / "heroes/001.bmp": (120, 90),
                output / "items/fire-stone.bmp": (32, 32),
                output / "heroes/items/fire-stone.bmp": (64, 64),
            }
            for path, size in expected.items():
                with Image.open(path) as image:
                    self.assertEqual(image.mode, "1")
                    self.assertEqual(image.size, size)

            with Image.open(output / "sprites/001.bmp") as sprite:
                self.assertEqual(sprite.getpixel((0, 0)), 255)
                self.assertEqual(sprite.getpixel((20, 15)), 0)

    def test_accepts_zero_padded_species_filenames(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            write_rgba_icon(root / "pokemon/001.png", (40, 30))
            write_rgba_icon(root / "items/fire-stone.png", (32, 32))

            GENERATOR.build_icons(
                root / "pokemon",
                root / "items",
                root / "output",
                species_count=1,
                items=("fire-stone",),
            )

            self.assertTrue((root / "output/sprites/001.bmp").is_file())

    def test_rejects_a_missing_species_icon(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / "pokemon").mkdir()
            write_rgba_icon(root / "items/fire-stone.png", (32, 32))

            with self.assertRaisesRegex(ValueError, "missing species icon 001"):
                GENERATOR.build_icons(
                    root / "pokemon",
                    root / "items",
                    root / "output",
                    species_count=1,
                    items=("fire-stone",),
                )


if __name__ == "__main__":
    unittest.main()
