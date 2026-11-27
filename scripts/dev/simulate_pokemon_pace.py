#!/usr/bin/env python3
"""Monte Carlo estimate of how many reading hours it takes to beat every gym and fill the Pokedex.

Models the rules in lib/Pokemon/PokemonGame.cpp and the gym teams; see
docs/development/pokemon-pace-plan.md for the assumptions and the results. Run from the repo root
after a test build (it reads the generated gym header):

    python3 scripts/dev/simulate_pokemon_pace.py [--runs 150] [--plan current|c]
"""

from __future__ import annotations

import argparse
import csv
import random
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
GYM_HEADER = ROOT / "test/build/pokemon_service/generated/PokemonGyms.generated.h"

PLANS = {
    # unc: weight multiplier for species not caught yet; leg: legendary chance per encounter;
    # mewtwo: book progress % from which Mewtwo can appear; item_chance/item_pity: hourly stone roll.
    "current": dict(unc=1, leg=1 / 20, mewtwo=95, item_chance=1 / 20, item_pity=19),
    "c": dict(unc=3, leg=1 / 10, mewtwo=90, item_chance=1 / 10, item_pity=9),
}

XR = [0] + [10 * (level - 1) + (3 * (level - 1) ** 2) // 4 for level in range(1, 101)]
MAX_XP = 8340


def level_for(xp: float) -> int:
    level = 1
    while level < 100 and XR[level + 1] <= xp:
        level += 1
    return level


def load_species():
    lines = (ROOT / "scripts/data/pokemon-kanto-v2.csv").read_text().splitlines()[1:]
    rows = {int(r["id"]): r for r in csv.DictReader(lines)}

    def weight(r):
        sid, rate = int(r["id"]), int(r["capture_rate"])
        if sid in (1, 4, 7, 25):
            return 1
        return 4 if rate >= 200 else 3 if rate >= 120 else 2 if rate >= 60 else 1

    wild = [(sid, r["stage"], weight(r)) for sid, r in rows.items() if r["acquisition"] == "Wild"]
    level_evo, item_evo = {}, {}
    for sid, r in rows.items():
        for evo in filter(None, r["evolutions"].split(";")):
            target, trigger, level, _item = evo.split(":")
            if trigger == "Level":
                level_evo[int(target)] = (sid, int(level))
            else:
                item_evo.setdefault(int(target), sid)
    return wild, level_evo, item_evo


def load_gyms():
    text = GYM_HEADER.read_text()
    levels = [int(m.group(2)) for m in re.finditer(r"GymTeamMember\{(\d+), (\d+),", text)]
    gyms = [(int(a), int(b)) for a, b in re.findall(r"GymData\{[^}]*?, (\d+), (\d+)\}", text)]
    return [levels[offset : offset + count] for offset, count in gyms]


def run(plan, rng, wild, level_evo, item_evo, gyms, book_hours=8, gym_margin=5, trainer_xp=10, train_evolutions=True):
    caught = {1: XR[5]}  # species -> XP of the record that carries it
    main, gym, minutes = 1, 0, 0
    misses = trainer_misses = item_misses = 0
    gyms_done = dex_done = None

    def leader():
        if gym < len(gyms) or not train_evolutions:
            return main
        best = None
        for target, (parent, level) in level_evo.items():
            if target not in caught and parent in caught:
                need = XR[level] - caught[parent]
                if best is None or need < best[0]:
                    best = (need, parent)
        return best[1] if best else main

    def give(xp):
        record = leader()
        caught[record] = min(MAX_XP, caught[record] + xp)
        changed = True
        while changed:
            changed = False
            for target, (parent, level) in level_evo.items():
                if parent in caught and target not in caught and level_for(caught[parent]) >= level:
                    caught[target] = caught[parent]
                    changed = True

    while minutes < 6000 * 60 and not (gyms_done and dex_done):
        minutes += 15
        hours = minutes / 60
        progress = int(100 * ((hours % book_hours) / book_hours))
        give(15)

        if misses >= 3 or rng.random() < 0.4:
            misses = 0
            if 151 not in caught and all(s in caught for s in range(1, 151)):
                caught[151] = XR[5]
            else:
                species = None
                if rng.random() < plan["leg"]:
                    eligible = []
                    if minutes >= 1200 and progress >= 75:
                        eligible += [144, 145, 146]
                    if minutes >= 3000 and progress >= plan["mewtwo"]:
                        eligible += [150]
                    if eligible:
                        species = rng.choice([s for s in eligible if s not in caught] or eligible)
                if species is None:
                    pool = [
                        (s, w * (plan["unc"] if s not in caught else 1))
                        for s, stage, w in wild
                        if not (stage == "Middle" and progress < 50) and not (stage == "Final" and progress < 75)
                    ]
                    roll = rng.uniform(0, sum(w for _, w in pool))
                    for s, w in pool:
                        roll -= w
                        if roll < 0:
                            species = s
                            break
                band = next(b for p, b in ((95, (18, 30)), (75, (14, 24)), (50, (9, 16)), (25, (5, 10)), (0, (2, 6))) if progress >= p)
                level = rng.randint(*band)
                if species in caught:
                    give(4 * level)  # beaten for XP, current wild formula
                else:
                    caught[species] = XR[level]  # assumes every new species is caught
        else:
            misses += 1

        if trainer_misses >= 3 or rng.random() < 0.4:  # road trainers on the wild schedule
            trainer_misses = 0
            give(trainer_xp)
        else:
            trainer_misses += 1

        if minutes % 60 == 0:
            if item_misses >= plan["item_pity"] or rng.random() < plan["item_chance"]:
                item_misses = 0
                needed = [t for t, parent in item_evo.items() if parent in caught and t not in caught]
                if needed:
                    target = rng.choice(needed)
                    caught[target] = caught[item_evo[target]]
            else:
                item_misses += 1

        while gym < len(gyms) and level_for(caught[main]) >= max(gyms[gym]) + gym_margin:
            for opponent in gyms[gym]:
                caught[main] = min(MAX_XP, caught[main] + 6 * opponent)  # current trainer formula
            gym += 1
            if gym == len(gyms):
                gyms_done = hours
        if dex_done is None and len(caught) == 151:
            dex_done = hours
    return gyms_done or 9999, dex_done or 9999


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--runs", type=int, default=150)
    parser.add_argument("--plan", choices=PLANS, default="c")
    parser.add_argument("--no-training", action="store_true", help="never switch leader to train evolutions")
    args = parser.parse_args()
    data = load_species() + (load_gyms(),)
    results = [run(PLANS[args.plan], random.Random(seed), *data, train_evolutions=not args.no_training) for seed in range(args.runs)]
    gyms = sorted(r[0] for r in results)
    dex = sorted(r[1] for r in results)
    pick = lambda values, q: values[min(len(values) - 1, int(len(values) * q))]
    print(f"gyms median {pick(gyms, .5):.0f} h | dex p10 {pick(dex, .1):.0f} median {pick(dex, .5):.0f} p90 {pick(dex, .9):.0f} h")


if __name__ == "__main__":
    main()
