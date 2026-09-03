#!/usr/bin/env python3

import importlib.util
import hashlib
import os
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
    row_size = ((width + 31) // 32) * 4
    image_size = row_size * height
    pixel_offset = 62
    file_size = pixel_offset + image_size
    header = bytearray(pixel_offset)
    header[:2] = b"BM"
    struct.pack_into("<I", header, 2, file_size)
    struct.pack_into("<I", header, 10, pixel_offset)
    struct.pack_into("<I", header, 14, 40)
    struct.pack_into("<ii", header, 18, width, height)
    struct.pack_into("<H", header, 26, 1)
    struct.pack_into("<H", header, 28, 1)
    struct.pack_into("<I", header, 34, image_size)
    struct.pack_into("<I", header, 46, 2)
    header[54:62] = b"\x00\x00\x00\x00\xff\xff\xff\x00"
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(header + bytes(image_size))


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

    def test_bmp_validation_rejects_header_without_pixel_payload(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "truncated.bmp"
            header = bytearray(30)
            header[:2] = b"BM"
            struct.pack_into("<ii", header, 18, 40, 30)
            struct.pack_into("<H", header, 28, 1)
            path.write_bytes(header)

            with self.assertRaisesRegex(ValueError, "truncated|invalid BMP"):
                PACKAGE.bmp_info(path)

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
            self.assertFalse((output / "pokemon/sprites/152.bmp").exists())
            self.assertFalse((output / "pokemon/egg.bmp").exists())

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
            self.assertIn("pokemon/manifest.json", names)
            self.assertIn("pokemon/sprites/001.bmp", names)
            self.assertIn("pokemon/pokedex/portrait/151.bmp", names)
            self.assertFalse(any(name.startswith(".crosspoint/pokemon/") for name in names))
            self.assertNotIn(".crosspoint/pokemon-v2-a.bin", names)
            self.assertNotIn(".crosspoint/pokemon-v2-b.bin", names)
            self.assertNotIn(".crosspoint/pokemon-a.bin", names)
            self.assertNotIn(".crosspoint/pokemon-b.bin", names)

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
            protected_files = {
                sd_root / "books" / "current.epub": b"book",
                sd_root / "sleep" / "001.bmp": b"sleep-cover",
                sd_root / ".crosspoint" / "crossink-settings.json": b"settings",
                sd_root / ".crosspoint" / "progress" / "current.json": b"progress",
                sd_root / ".crosspoint" / "stats" / "current.json": b"stats",
                sd_root / ".crosspoint" / "pokemon-a.bin": b"save-a",
                sd_root / ".crosspoint" / "pokemon-b.bin": b"save-b",
                sd_root / ".crosspoint" / "pokemon-v2-a.bin": b"legacy-a",
                sd_root / ".crosspoint" / "pokemon-v2-b.bin": b"legacy-b",
            }
            before = {}
            for path, content in protected_files.items():
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(content)
                before[path] = hashlib.sha256(path.read_bytes()).hexdigest()

            with zipfile.ZipFile(full_zip) as package:
                self.assertFalse(any(name.startswith(".crosspoint/") for name in package.namelist()))
                package.extractall(sd_root)

            after = {
                path: hashlib.sha256(path.read_bytes()).hexdigest()
                for path in protected_files
            }
            self.assertEqual(after, before)

    def test_public_release_zip_is_stable_when_source_mtimes_change(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = self.make_complete_source(root)
            firmware = root / "firmware.bin"
            firmware.write_bytes(b"tested firmware")
            notice = root / "RIGHTS_AND_ATTRIBUTION.md"
            notice.write_bytes(b"rights and credits\n")

            first, _, _ = PACKAGE.build_public_release(
                source, firmware, notice, "0.1.0-RC", root / "dist-one"
            )
            first_bytes = first.read_bytes()
            changed_time = 2_000_000_000
            for path in source.rglob("*.bmp"):
                os.utime(path, (changed_time, changed_time))

            second, _, _ = PACKAGE.build_public_release(
                source, firmware, notice, "0.1.0-RC", root / "dist-two"
            )
            self.assertEqual(first_bytes, second.read_bytes())


if __name__ == "__main__":
    unittest.main()
