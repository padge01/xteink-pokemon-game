#!/usr/bin/env python3

import importlib.util
import struct
import tempfile
import unittest
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


if __name__ == "__main__":
    unittest.main()
