#!/usr/bin/env python3
"""Create the public X3 firmware artifact and checksum."""

from __future__ import annotations

import argparse
import hashlib
import re
import shutil
from pathlib import Path


VERSION_PATTERN = re.compile(r"[0-9A-Za-z][0-9A-Za-z._-]*")


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(65536), b""):
            digest.update(block)
    return digest.hexdigest()


def build_release(firmware: Path, version: str, output: Path) -> tuple[Path, Path]:
    if not firmware.is_file() or firmware.stat().st_size == 0:
        raise ValueError("firmware is missing or empty")

    clean_version = version.removeprefix("v")
    if VERSION_PATTERN.fullmatch(clean_version) is None:
        raise ValueError("invalid release version")

    output.mkdir(parents=True, exist_ok=True)
    binary = output / f"xteink-pokemon-x3-v{clean_version}.bin"
    shutil.copyfile(firmware, binary)

    checksums = output / "SHA256SUMS"
    checksums.write_text(f"{sha256(binary)}  {binary.name}\n", encoding="ascii")
    return binary, checksums


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--firmware", type=Path, required=True)
    parser.add_argument("--version", required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()

    binary, checksums = build_release(args.firmware, args.version, args.output)
    print(binary)
    print(checksums)


if __name__ == "__main__":
    main()
