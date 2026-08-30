#!/usr/bin/env python3
"""Build and verify the private original-151 Pokémon X3 release archive."""

from __future__ import annotations

import argparse
import hashlib
import json
import shutil
import struct
import zipfile
from pathlib import Path

ITEMS = ("moon-stone", "fire-stone", "thunder-stone", "water-stone", "leaf-stone")


def expected_art() -> dict[Path, tuple[int, int]]:
    files: dict[Path, tuple[int, int]] = {}
    for species_id in range(1, 152):
        files[Path("sprites") / f"{species_id:03}.bmp"] = (40, 30)
        files[Path("heroes") / f"{species_id:03}.bmp"] = (120, 90)
    for item in ITEMS:
        files[Path("items") / f"{item}.bmp"] = (32, 32)
        files[Path("heroes/items") / f"{item}.bmp"] = (64, 64)
    return files


def bmp_info(path: Path) -> tuple[int, int, int]:
    with path.open("rb") as handle:
        header = handle.read(30)
    if len(header) != 30 or header[:2] != b"BM":
        raise ValueError(f"not a BMP: {path}")
    width, height = struct.unpack_from("<ii", header, 18)
    bits = struct.unpack_from("<H", header, 28)[0]
    return width, abs(height), bits


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(65536), b""):
            digest.update(block)
    return digest.hexdigest()


def validate_art(root: Path, allow_extra: bool = False) -> list[dict[str, object]]:
    expected = expected_art()
    actual = {path.relative_to(root) for path in root.rglob("*.bmp")}
    missing = sorted(set(expected) - actual)
    extra = sorted(actual - set(expected))
    if missing or (extra and not allow_extra):
        raise ValueError(f"art set mismatch: {len(missing)} missing, {len(extra)} extra")
    assets: list[dict[str, object]] = []
    for relative, dimensions in sorted(expected.items(), key=lambda item: str(item[0])):
        path = root / relative
        width, height, bits = bmp_info(path)
        if (width, height) != dimensions or bits != 1:
            raise ValueError(f"invalid BMP {relative}: {width}x{height} {bits}bpp")
        assets.append({"path": f".crosspoint/pokemon/{relative.as_posix()}", "sha256": sha256(path),
                       "width": width, "height": height, "bits_per_pixel": bits})
    return assets


def build(source: Path, firmware: Path, notice: Path, output: Path) -> Path:
    source = source.resolve()
    firmware = firmware.resolve()
    if not firmware.is_file() or not notice.is_file():
        raise ValueError("firmware or asset notice is missing")
    assets = validate_art(source, allow_extra=True)
    if output.exists():
        shutil.rmtree(output)
    art_output = output / ".crosspoint/pokemon"
    for relative in expected_art():
        destination = art_output / relative
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source / relative, destination)
    firmware_output = output / "firmware-x3-x4.bin"
    shutil.copy2(firmware, firmware_output)
    shutil.copy2(notice, output / "POKEMON_ASSET_LICENSES.md")
    manifest = {"format": 1, "scope": "original-151", "assets": assets}
    (art_output / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    hashes = []
    for path in sorted((p for p in output.rglob("*") if p.is_file()), key=lambda p: str(p)):
        hashes.append(f"{sha256(path)}  {path.relative_to(output).as_posix()}")
    (output / "SHA256SUMS.txt").write_text("\n".join(hashes) + "\n", encoding="ascii")
    archive = output.with_suffix(".zip")
    if archive.exists():
        archive.unlink()
    with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED) as package:
        for path in sorted((p for p in output.rglob("*") if p.is_file()), key=lambda p: str(p)):
            package.write(path, path.relative_to(output).as_posix())
    verify(output)
    return archive


def verify(output: Path) -> None:
    validate_art(output / ".crosspoint/pokemon")
    required = (output / "firmware-x3-x4.bin", output / "POKEMON_ASSET_LICENSES.md",
                output / "SHA256SUMS.txt", output / ".crosspoint/pokemon/manifest.json")
    if any(not path.is_file() for path in required):
        raise ValueError("release package is incomplete")
    for line in (output / "SHA256SUMS.txt").read_text(encoding="ascii").splitlines():
        digest, relative = line.split("  ", 1)
        if sha256(output / relative) != digest:
            raise ValueError(f"hash mismatch: {relative}")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-pack", type=Path, required=True)
    parser.add_argument("--firmware", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--notice", type=Path, default=Path("docs/third-party-assets.md"))
    args = parser.parse_args()
    archive = build(args.source_pack, args.firmware, args.notice, args.output)
    print(f"Verified private release: {archive}")


if __name__ == "__main__":
    main()
