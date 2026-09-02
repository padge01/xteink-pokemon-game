#!/usr/bin/env python3
"""Build and verify original-151 Pokémon X3 release archives."""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import shutil
import struct
import tempfile
import zipfile
from pathlib import Path, PurePosixPath

ITEMS = ("moon-stone", "fire-stone", "thunder-stone", "water-stone", "leaf-stone")
VERSION_PATTERN = re.compile(r"[0-9A-Za-z][0-9A-Za-z._-]*")


def expected_art() -> dict[Path, tuple[int, int]]:
    files: dict[Path, tuple[int, int]] = {}
    for species_id in range(1, 152):
        files[Path("sprites") / f"{species_id:03}.bmp"] = (40, 30)
        files[Path("heroes") / f"{species_id:03}.bmp"] = (120, 90)
        files[Path("pokedex/portrait") / f"{species_id:03}.bmp"] = (472, 708)
        files[Path("pokedex/landscape") / f"{species_id:03}.bmp"] = (288, 432)
    for item in ITEMS:
        files[Path("items") / f"{item}.bmp"] = (32, 32)
        files[Path("heroes/items") / f"{item}.bmp"] = (64, 64)
    return files


def bmp_info(path: Path) -> tuple[int, int, int]:
    return _validate_bmp_bytes(path.read_bytes(), str(path))


def _validate_bmp_bytes(data: bytes, label: str) -> tuple[int, int, int]:
    error = f"invalid or truncated BMP: {label}"
    if len(data) < 54 or data[:2] != b"BM":
        raise ValueError(error)

    declared_size = struct.unpack_from("<I", data, 2)[0]
    pixel_offset = struct.unpack_from("<I", data, 10)[0]
    dib_size = struct.unpack_from("<I", data, 14)[0]
    width, height = struct.unpack_from("<ii", data, 18)
    planes, bits = struct.unpack_from("<HH", data, 26)
    compression = struct.unpack_from("<I", data, 30)[0]
    image_size = struct.unpack_from("<I", data, 34)[0]
    colors_used = struct.unpack_from("<I", data, 46)[0]

    if dib_size < 40 or width <= 0 or height == 0:
        raise ValueError(error)
    if planes != 1 or bits != 1 or compression != 0:
        raise ValueError(error)

    absolute_height = abs(height)
    row_size = ((width * bits + 31) // 32) * 4
    expected_image_size = row_size * absolute_height
    palette_entries = colors_used or (1 << bits)
    minimum_pixel_offset = 14 + dib_size + palette_entries * 4
    if declared_size != len(data):
        raise ValueError(error)
    if pixel_offset < minimum_pixel_offset or pixel_offset + expected_image_size > len(data):
        raise ValueError(error)
    if image_size not in (0, expected_image_size):
        raise ValueError(error)

    return width, absolute_height, bits


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(65536), b""):
            digest.update(block)
    return digest.hexdigest()


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def normalize_version(version: str) -> str:
    clean = version.removeprefix("v")
    if VERSION_PATTERN.fullmatch(clean) is None:
        raise ValueError("invalid release version")
    return clean


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
    firmware_output = output / "update.bin"
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
    required = (output / "update.bin", output / "POKEMON_ASSET_LICENSES.md",
                output / "SHA256SUMS.txt", output / ".crosspoint/pokemon/manifest.json")
    if any(not path.is_file() for path in required):
        raise ValueError("release package is incomplete")
    for line in (output / "SHA256SUMS.txt").read_text(encoding="ascii").splitlines():
        digest, relative = line.split("  ", 1)
        if sha256(output / relative) != digest:
            raise ValueError(f"hash mismatch: {relative}")


def _write_tree_checksums(root: Path) -> None:
    lines = []
    for path in sorted((candidate for candidate in root.rglob("*") if candidate.is_file()),
                       key=lambda candidate: candidate.relative_to(root).as_posix()):
        lines.append(f"{sha256(path)}  {path.relative_to(root).as_posix()}")
    (root / "SHA256SUMS.txt").write_text("\n".join(lines) + "\n", encoding="ascii")


def _write_deterministic_zip(source: Path, archive: Path) -> None:
    with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED) as package:
        for path in sorted((candidate for candidate in source.rglob("*") if candidate.is_file()),
                           key=lambda candidate: candidate.relative_to(source).as_posix()):
            name = path.relative_to(source).as_posix()
            entry = zipfile.ZipInfo(name, date_time=(1980, 1, 1, 0, 0, 0))
            entry.compress_type = zipfile.ZIP_DEFLATED
            entry.create_system = 3
            entry.external_attr = 0o100644 << 16
            with path.open("rb") as input_file, package.open(entry, "w") as output_file:
                shutil.copyfileobj(input_file, output_file, length=65536)


def build_public_release(source: Path, firmware: Path, notice: Path, version: str,
                         output: Path) -> tuple[Path, Path, Path]:
    source = source.resolve()
    firmware = firmware.resolve()
    notice = notice.resolve()
    clean_version = normalize_version(version)
    if not firmware.is_file() or firmware.stat().st_size == 0:
        raise ValueError("firmware is missing or empty")
    if not notice.is_file():
        raise ValueError("rights and attribution notice is missing")

    assets = validate_art(source, allow_extra=True)
    output.mkdir(parents=True, exist_ok=True)
    firmware_asset = output / f"xteink-pokemon-x3-firmware-v{clean_version}.bin"
    full_zip = output / f"xteink-pokemon-x3-full-v{clean_version}.zip"
    release_sums = output / "SHA256SUMS.txt"
    shutil.copy2(firmware, firmware_asset)

    with tempfile.TemporaryDirectory(prefix="xteink-pokemon-full-") as temporary:
        staging = Path(temporary)
        art_output = staging / ".crosspoint/pokemon"
        for relative in expected_art():
            destination = art_output / relative
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source / relative, destination)
        shutil.copy2(firmware, staging / "update.bin")
        shutil.copy2(notice, staging / "RIGHTS_AND_ATTRIBUTION.md")
        manifest = {"format": 1, "scope": "original-151", "assets": assets}
        (art_output / "manifest.json").write_text(
            json.dumps(manifest, indent=2) + "\n", encoding="utf-8"
        )
        _write_tree_checksums(staging)

        if full_zip.exists():
            full_zip.unlink()
        _write_deterministic_zip(staging, full_zip)

    verify_archive(full_zip, firmware_asset, notice)
    release_sums.write_text(
        f"{sha256(full_zip)}  {full_zip.name}\n"
        f"{sha256(firmware_asset)}  {firmware_asset.name}\n",
        encoding="ascii",
    )
    return full_zip, firmware_asset, release_sums


def _parse_checksums(text: str) -> dict[str, str]:
    checksums: dict[str, str] = {}
    for line in text.splitlines():
        digest, separator, relative = line.partition("  ")
        if separator != "  " or len(digest) != 64 or any(char not in "0123456789abcdef" for char in digest):
            raise ValueError("invalid checksum entry")
        if relative in checksums:
            raise ValueError(f"duplicate checksum entry: {relative}")
        checksums[relative] = digest
    return checksums


def _bmp_bytes_info(data: bytes) -> tuple[int, int, int]:
    return _validate_bmp_bytes(data, "archive artwork")


def verify_archive(archive: Path, firmware: Path, notice: Path) -> None:
    expected_paths = {
        f".crosspoint/pokemon/{relative.as_posix()}" for relative in expected_art()
    }
    expected_paths.update({
        ".crosspoint/pokemon/manifest.json",
        "RIGHTS_AND_ATTRIBUTION.md",
        "SHA256SUMS.txt",
        "update.bin",
    })

    with zipfile.ZipFile(archive) as package:
        names = package.namelist()
        if len(names) != len(set(names)):
            raise ValueError("archive contains duplicate paths")
        for name in names:
            path = PurePosixPath(name)
            if path.is_absolute() or ".." in path.parts or "\\" in name:
                raise ValueError(f"unsafe archive path: {name}")
        actual_paths = set(names)
        if actual_paths != expected_paths:
            missing = expected_paths - actual_paths
            extra = actual_paths - expected_paths
            raise ValueError(f"archive layout mismatch: {len(missing)} missing, {len(extra)} extra")

        if package.read("update.bin") != firmware.read_bytes():
            raise ValueError("archive firmware does not match firmware-only asset")
        if package.read("RIGHTS_AND_ATTRIBUTION.md") != notice.read_bytes():
            raise ValueError("archive rights notice does not match repository notice")

        manifest = json.loads(package.read(".crosspoint/pokemon/manifest.json"))
        manifest_assets = {entry["path"]: entry for entry in manifest.get("assets", [])}
        if set(manifest_assets) != expected_paths - {
            ".crosspoint/pokemon/manifest.json", "RIGHTS_AND_ATTRIBUTION.md",
            "SHA256SUMS.txt", "update.bin",
        }:
            raise ValueError("archive manifest does not match artwork files")

        for relative, dimensions in expected_art().items():
            name = f".crosspoint/pokemon/{relative.as_posix()}"
            data = package.read(name)
            width, height, bits = _bmp_bytes_info(data)
            if (width, height) != dimensions or bits != 1:
                raise ValueError(f"invalid archived BMP {name}: {width}x{height} {bits}bpp")
            entry = manifest_assets[name]
            if entry.get("sha256") != sha256_bytes(data):
                raise ValueError(f"manifest hash mismatch: {name}")
            if (entry.get("width"), entry.get("height"), entry.get("bits_per_pixel")) != (
                width, height, bits
            ):
                raise ValueError(f"manifest metadata mismatch: {name}")

        embedded_sums = _parse_checksums(package.read("SHA256SUMS.txt").decode("ascii"))
        checksummed_paths = expected_paths - {"SHA256SUMS.txt"}
        if set(embedded_sums) != checksummed_paths:
            raise ValueError("embedded checksums do not match archive contents")
        for name, digest in embedded_sums.items():
            if sha256_bytes(package.read(name)) != digest:
                raise ValueError(f"embedded checksum mismatch: {name}")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-pack", type=Path, required=True)
    parser.add_argument("--firmware", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--notice", type=Path, default=Path("RIGHTS_AND_ATTRIBUTION.md"))
    parser.add_argument("--version", required=True)
    args = parser.parse_args()
    full_zip, firmware_asset, sums = build_public_release(
        args.source_pack, args.firmware, args.notice, args.version, args.output
    )
    print(full_zip)
    print(firmware_asset)
    print(sums)


if __name__ == "__main__":
    main()
