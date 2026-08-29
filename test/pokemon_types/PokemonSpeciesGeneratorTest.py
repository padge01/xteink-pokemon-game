#!/usr/bin/env python3
"""Behavior checks for the offline Kanto species generator."""

from __future__ import annotations

import csv
import hashlib
import json
import subprocess
import sys
import tempfile
from pathlib import Path


EXPECTED_METADATA_SHA256 = "722844206ef6447d6bd5edfe7c3ac9106c70a05da4e2a13658799b68d3a8669d"


def main() -> int:
    script = Path(sys.argv[1])
    source = Path(sys.argv[2])

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

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
