#!/usr/bin/env python3
"""Behavior checks for the moves/stats/learnsets/items/gyms generators."""

from __future__ import annotations

import runpy
import subprocess
import sys
import tempfile
from pathlib import Path


def _check_output_flag(script: Path, extra_args: list[str]) -> None:
    result = subprocess.run(
        [sys.executable, str(script), *extra_args, "--check"],
        capture_output=True,
        text=True,
        check=False,
    )
    if result.returncode != 2 or "--output" not in result.stderr:
        raise AssertionError(f"{script.name}: generator must require an explicit --output path")


def _run_generator(script: Path, extra_args: list[str], output: Path) -> None:
    subprocess.run([sys.executable, str(script), *extra_args, "--output", str(output)], check=True)
    if not output.is_file():
        raise AssertionError(f"{script.name}: did not create its requested output")


def main() -> int:
    root = Path(sys.argv[1])
    scripts_dir = root / "scripts"
    data_dir = scripts_dir / "data"

    checks = [
        (scripts_dir / "generate_pokemon_moves.py", ["--input", str(data_dir / "pokemon-moves.csv")]),
        (scripts_dir / "generate_pokemon_stats.py", ["--input", str(data_dir / "pokemon-stats.csv")]),
        (scripts_dir / "generate_pokemon_items.py", ["--input", str(data_dir / "pokemon-items.csv")]),
        (scripts_dir / "generate_pokemon_gyms.py", ["--input", str(data_dir / "pokemon-gyms.csv")]),
    ]
    for script, extra_args in checks:
        _check_output_flag(script, extra_args)

    # generate_pokemon_learnsets.py takes two --input flags, not one.
    learnsets_script = scripts_dir / "generate_pokemon_learnsets.py"
    learnsets_args = [
        "--learnsets-input", str(data_dir / "pokemon-learnsets.csv"),
        "--tmhm-input", str(data_dir / "pokemon-tmhm.csv"),
    ]
    _check_output_flag(learnsets_script, learnsets_args)

    with tempfile.TemporaryDirectory() as directory:
        build_dir = Path(directory) / "build" / "pokemon-x3"
        appended: dict[str, list[str]] = {}

        class FakeEnvironment:
            def subst(self, value: str) -> str:
                return {"$PROJECT_DIR": str(root), "$BUILD_DIR": str(build_dir)}[value]

            def Append(self, **values: list[str]) -> None:
                for key, value in values.items():
                    appended.setdefault(key, [])
                    appended[key] = value

        build_hooks = [
            "generate_pokemon_moves_build.py",
            "generate_pokemon_stats_build.py",
            "generate_pokemon_learnsets_build.py",
            "generate_pokemon_items_build.py",
            "generate_pokemon_gyms_build.py",
        ]
        expected_headers = [
            "PokemonMoves.generated.h",
            "PokemonStats.generated.h",
            "PokemonLearnsets.generated.h",
            "PokemonItems.generated.h",
            "PokemonGyms.generated.h",
        ]
        for hook_name, header_name in zip(build_hooks, expected_headers):
            runpy.run_path(
                str(scripts_dir / hook_name),
                init_globals={"env": FakeEnvironment(), "Import": lambda _name: None},
            )
            generated = build_dir / "generated" / "pokemon" / header_name
            if not generated.is_file():
                raise AssertionError(f"{hook_name}: did not generate into the PlatformIO build directory")
            if appended.get("CPPPATH") != [str(generated.parent)]:
                raise AssertionError(f"{hook_name}: must expose only its generated include directory")

    platformio = (root / "platformio.ini").read_text(encoding="utf-8")
    for env_name in ("[env:pokemon-x3]", "[env:pokemon-simulator-X3]"):
        section = platformio.split(env_name, 1)[1].split("\n[env:", 1)[0]
        for hook_name in build_hooks:
            if f"pre:scripts/{hook_name}" not in section:
                raise AssertionError(f"{env_name} must invoke {hook_name}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
