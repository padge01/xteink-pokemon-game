#!/usr/bin/env python3

import importlib.util
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
    def test_smoke_environment_uses_the_isolated_sd_root(self) -> None:
        args = SimpleNamespace(
            headless=True,
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


if __name__ == "__main__":
    unittest.main()
