# Pokémon game

## Included

- Bulbasaur, Charmander, Squirtle, or Pikachu as a first partner
- Optional nickname and gender selection
- Party of six with PC storage
- Original-151 Pokédex with packaged detail cards
- Wild encounters and evolution items earned through active reading
- Level, stone, and Link Cable evolutions
- Per-Pokémon evolution prompt control
- Pokémon-only reset

## Reading progression

The Pokémon at the top of the Party receives experience from active reading. CrossInk credits time as pages are turned, not while a book is left open or automatic page turning is running.

Wild encounters and item finds can occur every 15–60 minutes. Book progress influences the level and rarity of wild Pokémon. Pending events appear as a `!` on supported dashboards and wait in the Pokémon menu.

Books, reading statistics, and Pokémon saves remain separate.

## Install and saves

Use the [download and install guide](https://padge01.github.io/xteink-pokemon-game/install.html). The full install contains the firmware, Pokémon artwork, Pokédex cards, checksums, and rights notice. It contains no Pokémon saves, books, or reading data.

Pokémon saves are `/.crosspoint/pokemon-a.bin` and `/.crosspoint/pokemon-b.bin`. Back up both files to preserve a collection. Earlier `pokemon-v2-a.bin` and `pokemon-v2-b.bin` saves migrate automatically.

A save from v0.1.x is upgraded the first time the newer firmware opens it. Every Pokémon, level and Pokédex entry carries over; each Pokémon gets the moves for its level, and 10 Poké Balls and a Potion are added. The save as v0.1.x left it is kept as `/.crosspoint/pokemon-before-upgrade.bin`. To go back to v0.1.x, delete `pokemon-a.bin` and `pokemon-b.bin` and rename that file to `pokemon-a.bin`; anything earned since the upgrade is lost.

See [Rights and attribution](../RIGHTS_AND_ATTRIBUTION.md) and [Third-party assets](third-party-assets.md) for artwork provenance and the correction or removal process.

## X3 release checks

Before a stable release, verify on a physical X3 that onboarding, Party and PC actions, Pokédex cards, reading experience, saves, both orientations, supported dashboards, and pending events work without a crash report. The simulator cannot validate physical button mapping, e-ink appearance, SD-card behavior, or the X3's constrained heap.
