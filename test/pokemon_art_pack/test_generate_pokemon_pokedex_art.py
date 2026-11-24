#!/usr/bin/env python3

import hashlib
import importlib.util
import tempfile
import unittest
from pathlib import Path

from PIL import Image

REPO_ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location(
    "generate_pokemon_pokedex_art", REPO_ROOT / "scripts/generate_pokemon_pokedex_art.py"
)
GENERATOR = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(GENERATOR)


class PokemonPokedexArtGeneratorTest(unittest.TestCase):
    def test_builds_deterministic_one_bit_cards_for_both_orientations(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = root / "source"
            output = root / "output"
            source.mkdir()
            image = Image.new("L", (528, 792), 255)
            for x in range(64, 464):
                for y in range(96, 696):
                    image.putpixel((x, y), (x + y) % 256)
            image.save(source / "001-bulbasaur.bmp")

            GENERATOR.build_cards(source, output, species_count=1)
            portrait = output / "pokedex/portrait/001.bmp"
            landscape = output / "pokedex/landscape/001.bmp"
            with Image.open(portrait) as portrait_image, Image.open(landscape) as landscape_image:
                self.assertEqual(portrait_image.size, (472, 708))
                self.assertEqual(landscape_image.size, (288, 432))
                self.assertEqual(portrait_image.mode, "1")
                self.assertEqual(landscape_image.mode, "1")
            first_hash = hashlib.sha256(portrait.read_bytes()).hexdigest()
            GENERATOR.build_cards(source, output, species_count=1)
            self.assertEqual(hashlib.sha256(portrait.read_bytes()).hexdigest(), first_hash)

    def test_rejects_a_missing_species_card(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = root / "source"
            source.mkdir()
            Image.new("L", (528, 792), 255).save(source / "001-bulbasaur.bmp")
            with self.assertRaisesRegex(ValueError, "missing source card 002"):
                GENERATOR.build_cards(source, root / "output", species_count=2)


if __name__ == "__main__":
    unittest.main()
