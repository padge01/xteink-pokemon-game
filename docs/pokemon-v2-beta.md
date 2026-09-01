# Xteink Pokémon Game beta

This X3 beta adds a small Pokémon collection and progression feature to CrossInk. It is not a Game Boy game and does not try to become one.

## Included

- Bulbasaur, Charmander, Squirtle, or Pikachu as a first partner
- Optional nickname and gender selection
- Party of six, PC Box, renaming, moving, depositing, withdrawing, and PC sorting
- Original-151 Pokédex with packaged detail cards
- Wild encounters and evolution items earned through verified reading
- Level, stone, and Link Cable evolutions
- Per-Pokémon evolution prompt toggle
- Pokémon-only reset with two confirmations

There are no battles, moves, combat statistics, eggs, breeding, trading, or friendship system.

## Reading progression

CrossInk's reader lifecycle remains the source of truth. The companion credits time only while manually rendered pages are turning. Leaving a book open or using automatic page turns earns nothing.

Progress is checkpointed in five-minute batches and flushed when leaving the reader. Books, reading statistics, and Pokémon saves remain separate.

The lead Pokémon is the first member of the Party and receives reading EXP. The game checks for a wild encounter every 15 credited minutes. Two of five roll values produce an encounter; after three misses, the next available check is guaranteed. Wild levels scale with book progress. The starters and Pikachu are in the rarest species tier. Evolution-item checks remain hourly.

The companion uses reading counters, not background timers, so it does no work while the X3 is idle.

## Install and saves

Use the [download and install guide](https://padge01.github.io/xteink-pokemon-game/install.html). The public full-install ZIP contains:

- `update.bin`
- `.crosspoint/pokemon/` with 614 required BMP assets
- `.crosspoint/pokemon/manifest.json`
- `RIGHTS_AND_ATTRIBUTION.md`
- `SHA256SUMS.txt`

The package contains no Pokémon saves, books, or reading data.

Pokémon saves are `/.crosspoint/pokemon-v2-a.bin` and `/.crosspoint/pokemon-v2-b.bin`. Back up both files to preserve a collection. Deleting both files resets Pokémon onboarding without touching books or reading statistics.

See [Rights and attribution](../RIGHTS_AND_ATTRIBUTION.md) and [Third-party assets](third-party-assets.md) for artwork sources and the correction or removal process.

## X3 hardware checks

Before promoting a beta to a stable release, verify on a physical X3:

1. Starter, gender, and nickname choices work with the side and front buttons.
2. Pokémon menus open repeatedly from Home and after leaving a book without a crash report.
3. Party actions cannot empty the Party or move into an invalid slot.
4. Lists wrap and page correctly; Pokédex cards open in both orientations.
5. Reading EXP and collection state survive a reboot.
6. Dashboard, Lyra, Lyra 3 Covers, and Rounded Raff remain readable in portrait and landscape.
7. A pending event shows only `!` in the dashboard band.

The simulator does not validate raw GPIO mapping, physical switches, real e-ink pixels, or the X3's constrained heap.
