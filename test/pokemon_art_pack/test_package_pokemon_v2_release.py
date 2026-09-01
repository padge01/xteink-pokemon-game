#!/usr/bin/env python3

import importlib.util
import hashlib
import struct
import tempfile
import unittest
import zipfile
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location(
    "package_pokemon_v2_release", REPO_ROOT / "scripts/package_pokemon_v2_release.py"
)
PACKAGE = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(PACKAGE)


def write_bmp(path: Path, width: int, height: int) -> None:
    header = bytearray(30)
    header[:2] = b"BM"
    struct.pack_into("<ii", header, 18, width, height)
    struct.pack_into("<H", header, 28, 1)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(header)


class PokemonArtPackTest(unittest.TestCase):
    def make_complete_source(self, root: Path) -> Path:
        source = root / "source"
        for relative, dimensions in PACKAGE.expected_art().items():
            write_bmp(source / relative, *dimensions)
        return source

    def test_release_requires_both_pokedex_orientations_for_every_species(self) -> None:
        expected = PACKAGE.expected_art()
        self.assertEqual(expected[Path("pokedex/portrait/001.bmp")], (472, 708))
        self.assertEqual(expected[Path("pokedex/landscape/001.bmp")], (288, 432))
        self.assertEqual(expected[Path("pokedex/portrait/151.bmp")], (472, 708))
        self.assertEqual(expected[Path("pokedex/landscape/151.bmp")], (288, 432))
        self.assertEqual(len(expected), 614)

    def test_release_filters_dirty_source_and_verifies_exact_output(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = root / "source"
            for relative, dimensions in PACKAGE.expected_art().items():
                write_bmp(source / relative, *dimensions)
            write_bmp(source / "sprites/152.bmp", 40, 30)
            write_bmp(source / "egg.bmp", 40, 30)

            with self.assertRaisesRegex(ValueError, "2 extra"):
                PACKAGE.validate_art(source)

            firmware = root / "firmware.bin"
            firmware.write_bytes(b"firmware")
            notice = root / "notice.md"
            notice.write_text("asset notice\n", encoding="utf-8")
            output = root / "release"
            archive = PACKAGE.build(source, firmware, notice, output)

            self.assertTrue(archive.is_file())
            PACKAGE.verify(output)
            self.assertEqual((output / "update.bin").read_bytes(), b"firmware")
            self.assertFalse((output / "firmware-x3-x4.bin").exists())
            self.assertFalse((output / ".crosspoint/pokemon/sprites/152.bmp").exists())
            self.assertFalse((output / ".crosspoint/pokemon/egg.bmp").exists())

    def test_public_release_contains_firmware_artwork_and_rights_notice(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = self.make_complete_source(root)
            firmware = root / "firmware.bin"
            firmware.write_bytes(b"tested firmware")
            notice = root / "RIGHTS_AND_ATTRIBUTION.md"
            notice.write_bytes(b"rights and credits\n")

            full_zip, firmware_asset, sums = PACKAGE.build_public_release(
                source, firmware, notice, "0.1.0-RC", root / "dist"
            )

            self.assertEqual(full_zip.name, "xteink-pokemon-x3-full-v0.1.0-RC.zip")
            self.assertEqual(
                firmware_asset.name, "xteink-pokemon-x3-firmware-v0.1.0-RC.bin"
            )
            self.assertEqual(sums.name, "SHA256SUMS.txt")
            self.assertEqual(firmware_asset.read_bytes(), b"tested firmware")

            with zipfile.ZipFile(full_zip) as package:
                names = set(package.namelist())
                self.assertEqual(package.read("update.bin"), b"tested firmware")
                self.assertEqual(
                    package.read("RIGHTS_AND_ATTRIBUTION.md"), b"rights and credits\n"
                )
            self.assertIn(".crosspoint/pokemon/manifest.json", names)
            self.assertIn(".crosspoint/pokemon/sprites/001.bmp", names)
            self.assertIn(".crosspoint/pokemon/pokedex/portrait/151.bmp", names)
            self.assertNotIn(".crosspoint/pokemon-v2-a.bin", names)
            self.assertNotIn(".crosspoint/pokemon-v2-b.bin", names)

            expected_lines = {
                f"{hashlib.sha256(full_zip.read_bytes()).hexdigest()}  {full_zip.name}",
                f"{hashlib.sha256(firmware_asset.read_bytes()).hexdigest()}  {firmware_asset.name}",
            }
            self.assertEqual(
                set(sums.read_text(encoding="ascii").splitlines()), expected_lines
            )
            PACKAGE.verify_archive(full_zip, firmware_asset, notice)

    def test_extracting_public_release_preserves_existing_saves(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = self.make_complete_source(root)
            firmware = root / "firmware.bin"
            firmware.write_bytes(b"tested firmware")
            notice = root / "RIGHTS_AND_ATTRIBUTION.md"
            notice.write_bytes(b"rights and credits\n")
            full_zip, _, _ = PACKAGE.build_public_release(
                source, firmware, notice, "0.1.0-RC", root / "dist"
            )

            sd_root = root / "sd"
            crosspoint = sd_root / ".crosspoint"
            crosspoint.mkdir(parents=True)
            save_a = crosspoint / "pokemon-v2-a.bin"
            save_b = crosspoint / "pokemon-v2-b.bin"
            save_a.write_bytes(b"save-a")
            save_b.write_bytes(b"save-b")

            with zipfile.ZipFile(full_zip) as package:
                package.extractall(sd_root)

            self.assertEqual(save_a.read_bytes(), b"save-a")
            self.assertEqual(save_b.read_bytes(), b"save-b")


if __name__ == "__main__":
    unittest.main()
