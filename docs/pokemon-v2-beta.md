# Pokémon companion V2 beta

This optional private X3/X4 build adds a small reading companion to CrossInk. It
is a collection and progression feature, not a recreation of a full Pokémon
game. Normal CrossInk builds compile it out completely.

## What it includes

- Choose Bulbasaur, Charmander, Squirtle, or Pikachu as a first partner, choose
  its gender, and optionally give it a nickname.
- Carry up to six Pokémon. The first Party member is the lead Pokémon shown on
  supported Home dashboards and receives reading EXP.
- Move Party members, deposit them in the PC Box, withdraw them, rename them,
  and sort the PC by catch date, Pokédex number, or name.
- Browse a complete original-151 Pokédex. Seen and caught entries are tracked,
  and selecting a seen entry opens its full-size card from `/sleep`.
- Find wild Pokémon and rare evolution items through verified reading. A pending
  event waits in the Pokémon menu until the reader chooses what to do.
- Evolve by level, an applicable stone, or the Link Cable item. Evolution prompts
  can be disabled for a Pokémon without blocking level 100.
- Reset only Pokémon progress from the bottom of the Pokémon menu during beta
  testing. Reset requires two confirmations.

There are no battles, moves, combat statistics, egg system, breeding, trading,
or friendship system in V2. This keeps storage, RAM use, menu depth, and failure
surface appropriate for an ESP32-C3 reader.

## How reading progression works

CrossInk's existing reader lifecycle remains the source of truth. The companion
credits a minute only while manually rendered pages are actually turning; an
open idle book and automatic page turns earn nothing. Progress is checkpointed
in five-minute batches and flushed when leaving the reader. Reading statistics,
book progress, and Pokémon progress remain separate.

The lead Pokémon gains EXP from credited minutes. Encounters and items use
bounded reading counters rather than timers or background tasks, so the feature
does no work while the device is idle. Wild levels scale with book progress,
while uncommon species and evolution items remain rarer outcomes.

## Install the private beta

The verified release archive contains:

- `update.bin` (the exact filename consumed by the on-device updater)
- `.crosspoint/pokemon/` with the required artwork
- `POKEMON_ASSET_LICENSES.md`
- `SHA256SUMS.txt`
- `.crosspoint/pokemon/manifest.json`

Before installing, back up the SD card and keep the last known-good firmware.
Copy `update.bin` to the SD-card root and merge the supplied
`.crosspoint` folder into the SD-card root. Do not rename or flatten the artwork
folders. The original-151 Pokédex cards must remain in `/sleep`, with filenames
such as `/sleep/001-bulbasaur.bmp`; the detail screen streams these existing
cards directly rather than keeping a second copy in memory. Flash the firmware
using the device's normal on-device update flow.

V2 snapshots are `/.crosspoint/pokemon-v2-a.bin` and
`/.crosspoint/pokemon-v2-b.bin`. Back up both to preserve a collection. The
in-device reset commits an empty snapshot to the inactive slot before replacing
the current collection, so an interrupted reset can recover the previous save.
Deleting both files manually also resets Pokémon onboarding without touching
books or reading statistics.

The artwork is for private demonstration only and is not part of public source
or ordinary firmware releases. See [Third-party assets](third-party-assets.md).

Pokédex detail cards are streamed one row at a time from `/sleep` and converted
from their existing grayscale palette to the X3's 1-bit display with a stable,
allocation-free dither. This preserves midtone detail without loading a second
framebuffer or changing the small menu and dashboard sprites.

## X3 release checklist

Software builds and simulator checks cannot prove the firmware is safe on the
physical ESP32-C3. Before treating a package as install-ready:

The simulator injects logical front and side button actions. It does not test
the X3's raw GPIO mapping, physical switch behavior, or actual e-ink pixels.

1. Boot with the artwork installed and complete starter, gender, and nickname
   choices using both the side buttons and front buttons. Confirm the Pokémon
   title is fully below the header rule and a selected row does not erase its
   sprite.
2. Open and close Pokémon repeatedly from Home, including immediately after
   leaving a book. Confirm no crash report is created.
3. With one Party member, confirm Move and Deposit are absent. After adding a
   second member, confirm Move reorders only occupied slots and Deposit cannot
   empty the Party.
4. Verify list wrapping, Pokédex paging beyond the first screen, PC sorting, and
   the two-step reset. Open a seen Pokédex entry and confirm its `/sleep` card
   is detailed rather than solid black; Nidoran♀ and Nidoran♂ should resolve to
   their `-f` and `-m` filenames.
5. Turn real pages for at least one five-minute checkpoint, exit the book, and
   reboot. Confirm EXP and collection state survive.
6. Review Dashboard, Lyra, Lyra 3 Covers, and Rounded Raff in portrait and
   landscape. Confirm the companion band clears the dashboard controls and its
   sprite and EXP text remain distinct. Other themes intentionally omit the
   companion band.
7. Inspect `crash_report.txt` and serial logs if any restart occurs; preserve both
   Pokémon snapshots before reproducing a storage problem.

Until this checklist passes on the target X3, the archive is a software-verified
release candidate rather than a hardware-approved release.
