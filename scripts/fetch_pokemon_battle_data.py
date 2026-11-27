#!/usr/bin/env python3
"""One-time data fetcher: pull Gen 1 (Red/Blue) battle data from PokeAPI.

Writes four CSVs under scripts/data/ that the build-time generators consume:

  pokemon-stats.csv      base stats for the original 151
  pokemon-moves.csv      the 165 Generation I moves
  pokemon-learnsets.csv  Red/Blue level-up learnsets
  pokemon-tmhm.csv       Red/Blue TM/HM compatibility

Responses are cached under scripts/.pokeapi-cache/ so re-runs are cheap. The
cache is not checked in; the generated CSVs are.

Usage:  python3 scripts/fetch_pokemon_battle_data.py
"""

import csv
import json
import sys
import time
import urllib.error
import urllib.request
from pathlib import Path

API = "https://pokeapi.co/api/v2"
SPECIES_COUNT = 151
VERSION_GROUP = "red-blue"

SCRIPT_DIR = Path(__file__).resolve().parent
DATA_DIR = SCRIPT_DIR / "data"
CACHE_DIR = SCRIPT_DIR / ".pokeapi-cache"

# Generation I stored one "Special" stat; PokeAPI exposes the modern split.
# special-attack is the faithful stand-in for Red/Blue's Special.
STAT_KEYS = ("hp", "attack", "defense", "special-attack", "speed")

TYPE_NAMES = {
    "normal": "Normal", "fire": "Fire", "water": "Water", "electric": "Electric",
    "grass": "Grass", "ice": "Ice", "fighting": "Fighting", "poison": "Poison",
    "ground": "Ground", "flying": "Flying", "psychic": "Psychic", "bug": "Bug",
    "rock": "Rock", "ghost": "Ghost", "dragon": "Dragon", "dark": "Dark",
    "steel": "Steel", "fairy": "Fairy",
}

# PokeAPI ailment slugs -> the status set the battle engine models.
AILMENTS = {
    "none": "None", "unknown": "None",
    "paralysis": "Paralysis", "sleep": "Sleep", "freeze": "Freeze",
    "burn": "Burn", "poison": "Poison", "confusion": "Confusion",
}


def fetch(path):
    """GET {API}/{path} as JSON, memoised on disk."""
    cache_path = CACHE_DIR / (path.replace("/", "_") + ".json")
    if cache_path.exists():
        return json.loads(cache_path.read_text(encoding="utf-8"))

    url = f"{API}/{path}"
    request = urllib.request.Request(url, headers={"User-Agent": "xteink-pokemon-game-data-fetch/1.0"})
    for attempt in range(5):
        try:
            with urllib.request.urlopen(request, timeout=30) as response:
                payload = json.loads(response.read().decode("utf-8"))
            break
        except (urllib.error.URLError, TimeoutError) as error:
            if attempt == 4:
                raise SystemExit(f"failed to fetch {url}: {error}")
            time.sleep(2 * (attempt + 1))
    else:  # pragma: no cover - loop always breaks or raises
        raise SystemExit(f"failed to fetch {url}")

    cache_path.parent.mkdir(parents=True, exist_ok=True)
    cache_path.write_text(json.dumps(payload), encoding="utf-8")
    time.sleep(0.05)
    return payload


def display_name(slug):
    """PokeAPI slugs -> the in-game spelling, e.g. double-edge -> Double-Edge."""
    return "-".join(part.capitalize() for part in slug.split("-"))


def gen1_move_ids():
    generation = fetch("generation/1")
    ids = []
    for entry in generation["moves"]:
        ids.append(int(entry["url"].rstrip("/").rsplit("/", 1)[1]))
    return sorted(ids)


def write_moves(move_ids):
    rows = []
    for index, move_id in enumerate(move_ids, start=1):
        move = fetch(f"move/{move_id}")
        meta = move.get("meta") or {}
        ailment = (meta.get("ailment") or {}).get("name", "none")
        if ailment not in AILMENTS:
            ailment = "none"
        rows.append({
            "id": move_id,
            "name": display_name(move["name"]),
            "type": TYPE_NAMES[move["type"]["name"]],
            "power": move["power"] or 0,
            "accuracy": move["accuracy"] or 0,
            "pp": move["pp"] or 0,
            "damage_class": move["damage_class"]["name"],
            "ailment": AILMENTS[ailment],
            "ailment_chance": meta.get("ailment_chance", 0) or 0,
        })
        print(f"  move {index}/{len(move_ids)}: {rows[-1]['name']}", file=sys.stderr)
    _write_csv("pokemon-moves.csv", rows)


def write_species_tables():
    stats_rows = []
    learn_rows = []
    machine_rows = []

    for species_id in range(1, SPECIES_COUNT + 1):
        pokemon = fetch(f"pokemon/{species_id}")
        by_stat = {s["stat"]["name"]: s["base_stat"] for s in pokemon["stats"]}
        # "effort" is PokeAPI's own per-species EV-yield table (a modern-games
        # mechanic, not a real Gen 1 one - see BaseStats's doc comment in
        # PokemonBattleTypes.h) - reused here as this project's simplified EV
        # yield source.
        by_effort = {s["stat"]["name"]: s["effort"] for s in pokemon["stats"]}
        stats_rows.append({
            "id": species_id,
            "name": display_name(pokemon["name"]),
            "hp": by_stat["hp"],
            "attack": by_stat["attack"],
            "defense": by_stat["defense"],
            "special": by_stat["special-attack"],
            "speed": by_stat["speed"],
            "ev_hp": by_effort["hp"],
            "ev_attack": by_effort["attack"],
            "ev_defense": by_effort["defense"],
            "ev_special": by_effort["special-attack"],
            "ev_speed": by_effort["speed"],
        })

        level_ups = []
        machines = []
        for slot in pokemon["moves"]:
            move_id = int(slot["move"]["url"].rstrip("/").rsplit("/", 1)[1])
            for detail in slot["version_group_details"]:
                if detail["version_group"]["name"] != VERSION_GROUP:
                    continue
                method = detail["move_learn_method"]["name"]
                if method == "level-up":
                    level_ups.append((detail["level_learned_at"], move_id))
                elif method == "machine":
                    machines.append(move_id)

        # A species can list the same move at several levels; keep the earliest.
        earliest = {}
        for level, move_id in level_ups:
            if move_id not in earliest or level < earliest[move_id]:
                earliest[move_id] = level
        for move_id, level in sorted(earliest.items(), key=lambda kv: (kv[1], kv[0])):
            learn_rows.append({"species_id": species_id, "level": level, "move_id": move_id})

        for move_id in sorted(set(machines)):
            machine_rows.append({"species_id": species_id, "move_id": move_id})

        print(f"  species {species_id}/{SPECIES_COUNT}: {stats_rows[-1]['name']} "
              f"({len(earliest)} level-up, {len(set(machines))} TM/HM)", file=sys.stderr)

    _write_csv("pokemon-stats.csv", stats_rows)
    _write_csv("pokemon-learnsets.csv", learn_rows)
    _write_csv("pokemon-tmhm.csv", machine_rows)


def _write_csv(filename, rows):
    path = DATA_DIR / filename
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8", newline="") as handle:
        handle.write(f"# PokeAPI {API} - generation-i / {VERSION_GROUP}\n")
        writer = csv.DictWriter(handle, fieldnames=list(rows[0].keys()), quoting=csv.QUOTE_ALL)
        writer.writeheader()
        writer.writerows(rows)
    print(f"wrote {path} ({len(rows)} rows)", file=sys.stderr)


def main():
    print("fetching Generation I move list...", file=sys.stderr)
    move_ids = gen1_move_ids()
    print(f"{len(move_ids)} moves", file=sys.stderr)
    write_moves(move_ids)
    print("fetching species stats and learnsets...", file=sys.stderr)
    write_species_tables()
    print("done", file=sys.stderr)


if __name__ == "__main__":
    main()
