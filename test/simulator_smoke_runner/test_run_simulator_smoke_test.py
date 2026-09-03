#!/usr/bin/env python3

import importlib.util
import struct
import tempfile
import unittest
from pathlib import Path
from types import SimpleNamespace


REPO_ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location(
    "run_simulator_smoke_test", REPO_ROOT / "scripts" / "run_simulator_smoke_test.py"
)
RUNNER = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(RUNNER)


class SimulatorSmokeRunnerTest(unittest.TestCase):
    def test_pokemon_assets_are_staged_at_the_runtime_visible_path(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            source = root / "source"
            source.mkdir()
            (source / "marker.bmp").write_bytes(b"art")
            previous_source = RUNNER.POKEMON_ASSETS
            RUNNER.POKEMON_ASSETS = source
            try:
                RUNNER.prepare_pokemon_assets(root / "simulator")
            finally:
                RUNNER.POKEMON_ASSETS = previous_source

            visible_art = root / "simulator" / "fs_" / "pokemon" / "marker.bmp"
            self.assertTrue(visible_art.is_file(), "artwork was not staged under /pokemon")
            self.assertEqual(visible_art.read_bytes(), b"art")
            self.assertFalse(
                (root / "simulator" / "fs_" / ".crosspoint" / "pokemon").exists()
            )

    def test_smoke_environment_uses_the_isolated_sd_root(self) -> None:
        args = SimpleNamespace(
            headless=True,
            home_navigation=False,
            landscape=False,
            page_turns=2,
            pokemon=True,
            theme=None,
        )
        storage_root = Path("/tmp/crossink-smoke/fs_")

        env = RUNNER.build_smoke_environment(
            {"PRESERVE_ME": "yes"}, args, storage_root, "/books/test.epub"
        )

        self.assertEqual(env["CROSSPOINT_SIM_SD"], str(storage_root))
        self.assertEqual(env["CROSSINK_SIMULATOR_SMOKE_BOOK"], "/books/test.epub")
        self.assertEqual(env["CROSSINK_SIMULATOR_START_POKEMON"], "1")
        self.assertEqual(env["SDL_VIDEODRIVER"], "dummy")
        self.assertEqual(env["PRESERVE_ME"], "yes")

    def test_home_navigation_environment_enables_the_real_home_route(self) -> None:
        args = SimpleNamespace(
            headless=True,
            home_navigation=True,
            landscape=False,
            page_turns=0,
            pokemon=False,
            theme="classic",
        )

        env = RUNNER.build_smoke_environment(
            {}, args, Path("/tmp/crossink-smoke/fs_"), "/books/test.epub"
        )

        self.assertEqual(env["CROSSINK_SIMULATOR_HOME_NAVIGATION"], "1")

    def test_minimal_home_navigation_uses_the_minimal_theme(self) -> None:
        args = SimpleNamespace(
            headless=True,
            home_navigation=True,
            landscape=False,
            page_turns=0,
            pokemon=False,
            theme="minimal",
        )

        env = RUNNER.build_smoke_environment(
            {}, args, Path("/tmp/crossink-smoke/fs_"), "/books/test.epub"
        )

        self.assertEqual(env["CROSSINK_SIMULATOR_SMOKE_THEME"], "5")

    def test_pokedex_fixture_uses_packaged_portrait_card_format(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            card = Path(temp_dir) / "004-charmander.bmp"
            RUNNER.prepare_pokedex_card_fixture(card)
            content = card.read_bytes()

        self.assertEqual(content[:2], b"BM")
        self.assertEqual(struct.unpack_from("<ii", content, 18), (472, 708))
        self.assertEqual(struct.unpack_from("<H", content, 28)[0], 1)
        self.assertLess(len(content), 50_000)

    def test_pokemon_output_requires_a_rendered_detail_card(self) -> None:
        self.assertEqual(
            RUNNER.pokemon_smoke_output_error("Simulator smoke test passed"),
            "Pokédex detail card was not rendered",
        )
        self.assertIsNone(
            RUNNER.pokemon_smoke_output_error(
                f"{RUNNER.POKEMON_DETAIL_SUCCESS_MARKER}\nSimulator smoke test passed"
            )
        )

    def test_pokemon_output_rejects_artwork_errors(self) -> None:
        output = f"{RUNNER.POKEMON_DETAIL_SUCCESS_MARKER}\n[PKART] Could not render Pokemon art"
        self.assertEqual(
            RUNNER.pokemon_smoke_output_error(output),
            "artwork failure: [PKART] Could not render Pokemon art",
        )


if __name__ == "__main__":
    unittest.main()
