#!/usr/bin/env python3
"""Behavior checks for the offline Kanto species generator."""

from __future__ import annotations

import csv
import hashlib
import json
import runpy
import subprocess
import sys
import tempfile
from pathlib import Path


EXPECTED_METADATA_SHA256 = "722844206ef6447d6bd5edfe7c3ac9106c70a05da4e2a13658799b68d3a8669d"


def main() -> int:
    script = Path(sys.argv[1])
    source = Path(sys.argv[2])
    build_script = Path(sys.argv[3])
    platformio_ini = Path(sys.argv[4])

    missing_output = subprocess.run(
        [sys.executable, str(script), "--input", str(source), "--check"],
        capture_output=True,
        text=True,
        check=False,
    )
    if missing_output.returncode != 2 or "--output" not in missing_output.stderr:
        raise AssertionError("the generator must require an explicit --output path")

    with source.open("r", encoding="utf-8", newline="") as csv_file:
        csv_file.readline()
        rows = list(csv.DictReader(csv_file))
    metadata = json.dumps(rows, sort_keys=True, separators=(",", ":"), ensure_ascii=False).encode()
    actual_metadata_sha256 = hashlib.sha256(metadata).hexdigest()
    if actual_metadata_sha256 != EXPECTED_METADATA_SHA256:
        raise AssertionError(f"pinned Kanto metadata changed: {actual_metadata_sha256}")

    with tempfile.TemporaryDirectory() as directory:
        output = Path(directory) / "PokemonSpecies.generated.h"
        subprocess.run(
            [sys.executable, str(script), "--input", str(source), "--output", str(output)],
            check=True,
        )
        if not output.is_file():
            raise AssertionError("the generator did not create its requested output")

        build_dir = Path(directory) / "build" / "pokemon-x3"
        appended: dict[str, list[str]] = {}

        class FakeEnvironment:
            def subst(self, value: str) -> str:
                return {
                    "$PROJECT_DIR": str(script.parent.parent),
                    "$BUILD_DIR": str(build_dir),
                }[value]

            def Append(self, **values: list[str]) -> None:
                appended.update(values)

        runpy.run_path(
            str(build_script),
            init_globals={"env": FakeEnvironment(), "Import": lambda _name: None},
        )
        generated = build_dir / "generated/pokemon/PokemonSpecies.generated.h"
        if not generated.is_file():
            raise AssertionError("the firmware hook must generate into the PlatformIO build directory")
        if appended.get("CPPPATH") != [str(generated.parent)]:
            raise AssertionError("the firmware hook must expose only its generated include directory")

    platformio = platformio_ini.read_text(encoding="utf-8")
    pokemon_environment = platformio.split("[env:pokemon-x3]", 1)[1].split("[env:", 1)[0]
    if "${base.extra_scripts}" not in pokemon_environment:
        raise AssertionError("pokemon-x3 must retain the base build hooks")
    if "pre:scripts/generate_pokemon_v2_species_build.py" not in pokemon_environment:
        raise AssertionError("pokemon-x3 must invoke the species generation hook")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
