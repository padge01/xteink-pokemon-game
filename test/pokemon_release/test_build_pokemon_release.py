import hashlib
import importlib.util
import tempfile
import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]
MODULE_PATH = REPO_ROOT / "scripts" / "build_pokemon_release.py"


def load_release_module():
    spec = importlib.util.spec_from_file_location("build_pokemon_release", MODULE_PATH)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"could not load {MODULE_PATH}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class BuildPokemonReleaseTest(unittest.TestCase):
    def test_build_release_uses_public_filename_and_checksum(self) -> None:
        release = load_release_module()
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            firmware = root / "firmware.bin"
            firmware.write_bytes(b"x3 firmware")

            binary, sums = release.build_release(firmware, "1.0.0", root / "dist")

            self.assertEqual(binary.name, "xteink-pokemon-x3-v1.0.0.bin")
            digest = hashlib.sha256(b"x3 firmware").hexdigest()
            self.assertEqual(
                sums.read_text(encoding="ascii"),
                f"{digest}  xteink-pokemon-x3-v1.0.0.bin\n",
            )
            self.assertEqual(binary.read_bytes(), b"x3 firmware")

    def test_build_release_rejects_empty_firmware(self) -> None:
        release = load_release_module()
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            firmware = root / "firmware.bin"
            firmware.write_bytes(b"")

            with self.assertRaisesRegex(ValueError, "empty"):
                release.build_release(firmware, "1.0.0", root / "dist")


if __name__ == "__main__":
    unittest.main()
