#!/usr/bin/env python3
"""Build and run the simulator smoke test against an isolated fs_ directory."""

from __future__ import annotations

import argparse
import os
import shutil
import struct
import subprocess
import sys
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_BOOK = ROOT / "test" / "epubs" / "test_reader_rendering_matrix.epub"
POKEMON_ASSETS = ROOT / "fs_" / ".crosspoint" / "pokemon"
CRASH_PATTERNS = (
    "std::bad_alloc",
    "terminating due to uncaught exception",
    "Assertion failed",
    "Segmentation fault",
    "AddressSanitizer",
    "UndefinedBehaviorSanitizer",
)
POKEMON_ART_ERROR_PATTERNS = (
    "[PKART] Missing Pokemon art",
    "[PKART] Invalid Pokemon art",
    "[PKART] Could not render Pokemon art",
    "[GFX] Failed to read row",
)
POKEMON_DETAIL_SUCCESS_MARKER = "Pokemon Pokedex detail card rendered"
THEMES = {
    "classic": 0,
    "lyra": 1,
    "lyra-extended": 2,
    "lyra_extended": 2,
    "lyra3": 2,
    "lyra-3-covers": 2,
    "roundedraff": 3,
    "rounded-raff": 3,
    "lyra-carousel": 4,
    "lyra_carousel": 4,
    "carousel": 4,
    "dashboard": 6,
}


def program_path(env_name: str) -> Path:
    return ROOT / ".pio" / "build" / env_name / "program"


def build_simulator(env_name: str) -> None:
    print(f"Building {env_name} simulator...", flush=True)
    proc = subprocess.run(["pio", "run", "-e", env_name], cwd=ROOT)
    if proc.returncode != 0:
        raise SystemExit(proc.returncode)


def prepare_fs(temp_root: Path, book: Path) -> str:
    books_dir = temp_root / "fs_" / "books"
    books_dir.mkdir(parents=True, exist_ok=True)

    target = books_dir / book.name
    shutil.copy2(book, target)
    return f"/books/{book.name}"


def prepare_pokemon_assets(temp_root: Path) -> None:
    if not POKEMON_ASSETS.is_dir():
        raise FileNotFoundError(f"Pokemon assets not found: {POKEMON_ASSETS}")
    target = temp_root / "fs_" / ".crosspoint" / "pokemon"
    shutil.copytree(POKEMON_ASSETS, target)
    prepare_pokedex_card_fixture(target / "pokedex" / "portrait" / "004.bmp")
    prepare_pokedex_card_fixture(target / "pokedex" / "landscape" / "004.bmp", 288, 432)


def prepare_pokedex_card_fixture(target: Path, width: int = 472, height: int = 708) -> None:
    """Write a packaged-size 1-bit card that exercises the dedicated SD path."""
    row_stride = ((width + 31) // 32) * 4
    palette = bytes((0, 0, 0, 0, 255, 255, 255, 0))
    pixels = bytearray()
    for y in range(height):
        row = bytearray(b"\xff" * row_stride)
        for x in range(width):
            if (x % 23 == 0 or y % 29 == 0):
                row[x // 8] &= ~(0x80 >> (x % 8))
        pixels.extend(row)

    pixel_offset = 14 + 40 + len(palette)
    file_size = pixel_offset + len(pixels)
    header = struct.pack("<2sIHHI", b"BM", file_size, 0, 0, pixel_offset)
    dib = struct.pack("<IiiHHIIiiII", 40, width, height, 1, 1, 0, len(pixels), 2835, 2835, 2, 2)
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_bytes(header + dib + palette + pixels)


def pokemon_smoke_output_error(output: str) -> str | None:
    for pattern in POKEMON_ART_ERROR_PATTERNS:
        if pattern in output:
            return f"artwork failure: {pattern}"
    if POKEMON_DETAIL_SUCCESS_MARKER not in output:
        return "Pokédex detail card was not rendered"
    return None


def build_smoke_environment(
    base_env: dict[str, str], args: argparse.Namespace, storage_root: Path, simulator_book_path: str
) -> dict[str, str]:
    env = base_env.copy()
    env["CROSSPOINT_SIM_SD"] = str(storage_root)
    env["CROSSINK_SIMULATOR_SMOKE_TEST"] = "1"
    env["CROSSINK_SIMULATOR_SMOKE_BOOK"] = simulator_book_path
    env["CROSSINK_SIMULATOR_SMOKE_PAGE_TURNS"] = str(args.page_turns)
    if args.pokemon:
        env["CROSSINK_SIMULATOR_START_POKEMON"] = "1"
    if args.landscape:
        env["CROSSINK_SIMULATOR_POKEMON_LANDSCAPE"] = "1"
    if args.theme:
        env["CROSSINK_SIMULATOR_SMOKE_THEME"] = str(THEMES[args.theme])
    if args.headless:
        env.setdefault("SDL_VIDEODRIVER", "dummy")
    return env


def run_smoke(args: argparse.Namespace) -> int:
    book = Path(args.book).resolve()
    if not book.exists():
        print(f"Smoke test book not found: {book}", file=sys.stderr)
        return 2

    if args.build:
        build_simulator(args.env)

    program = program_path(args.env)
    if not program.exists():
        print(f"Simulator binary not found: {program}", file=sys.stderr)
        print(f"Run: pio run -e {args.env}", file=sys.stderr)
        return 2

    with tempfile.TemporaryDirectory(prefix="crossink-sim-smoke-") as temp_dir_name:
        temp_root = Path(temp_dir_name)
        simulator_book_path = prepare_fs(temp_root, book)
        if args.pokemon:
            prepare_pokemon_assets(temp_root)

        env = build_smoke_environment(os.environ, args, temp_root / "fs_", simulator_book_path)

        print(f"Running simulator smoke test with isolated fs_: {temp_root / 'fs_'}", flush=True)
        proc = subprocess.run(
            [str(program)],
            cwd=temp_root,
            env=env,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            timeout=args.timeout,
        )

    print(proc.stdout, end="")

    if proc.returncode != 0:
        print(f"Simulator smoke test failed with exit code {proc.returncode}", file=sys.stderr)
        return proc.returncode

    for pattern in CRASH_PATTERNS:
        if pattern in proc.stdout:
            print(f"Simulator smoke test output contained crash pattern: {pattern}", file=sys.stderr)
            return 2

    if args.pokemon:
        pokemon_error = pokemon_smoke_output_error(proc.stdout)
        if pokemon_error is not None:
            print(f"Pokemon smoke test failed: {pokemon_error}", file=sys.stderr)
            return 2

    if "Simulator smoke test passed" not in proc.stdout:
        print("Simulator smoke test did not print its success marker", file=sys.stderr)
        return 2

    return 0


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--book", default=str(DEFAULT_BOOK), help="EPUB fixture to copy into the isolated simulator fs_")
    parser.add_argument("--env", choices=("simulator", "sticky-simulator", "x4-pro-simulator", "pokemon-simulator-X3"), default="simulator",
                        help="PlatformIO simulator environment to build and run")
    parser.add_argument("--timeout", type=int, default=45, help="Seconds before the simulator run is treated as hung")
    parser.add_argument("--page-turns", type=int, default=2, help="Number of EPUB page-forward taps to run")
    parser.add_argument("--theme", choices=sorted(THEMES), help="UI theme to use during the smoke test")
    parser.add_argument("--pokemon", action="store_true", help="Run the Pokemon onboarding and collection smoke route")
    parser.add_argument("--landscape", action="store_true", help="Run the Pokemon route in landscape orientation")
    parser.add_argument("--no-build", dest="build", action="store_false", help="Run the existing simulator binary")
    parser.add_argument("--window", dest="headless", action="store_false", help="Show the SDL window instead of using dummy video")
    parser.set_defaults(build=True, headless=True)
    args = parser.parse_args()
    if args.landscape and not args.pokemon:
        parser.error("--landscape requires --pokemon")
    if args.pokemon and args.env != "pokemon-simulator-X3":
        parser.error("--pokemon requires --env pokemon-simulator-X3")
    return args


def main() -> int:
    return run_smoke(parse_args())


if __name__ == "__main__":
    raise SystemExit(main())
