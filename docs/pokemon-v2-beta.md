# Pokémon companion V2 beta

This X3 beta build adds a small reading companion to CrossInk. It
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
  and selecting a seen entry opens its packaged Pokédex card.
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

The game checks for a wild encounter every 15 credited minutes. Two of the five
roll values produce an encounter; after three misses, the next available check
is guaranteed. Species use four gentle rarity tiers instead of raw capture-rate
weights, with the three starters and Pikachu in the rarest tier. Evolution-item
checks remain hourly. When an hourly item and encounter are both due, the rarer
item event is shown first and the guaranteed encounter carries to the next
15-minute check.

## Install the beta

The locally generated, validated personal installation archive contains:

- `update.bin` (the exact filename consumed by the on-device updater)
- `.crosspoint/pokemon/` with the required artwork
- `POKEMON_ASSET_LICENSES.md`
- `SHA256SUMS.txt`
- `.crosspoint/pokemon/manifest.json`

Before installing, back up the SD card and keep the last known-good firmware.
Copy `update.bin` to the SD-card root and merge the supplied
`.crosspoint` folder into the SD-card root. Do not rename or flatten the artwork
folders. Pokédex cards live under
`/.crosspoint/pokemon/pokedex/{portrait,landscape}/001.bmp` through `151.bmp`;
they are part of the local artwork pack and do not depend on a personal sleep-screen
folder. Flash the firmware using the device's normal on-device update flow.

V2 snapshots are `/.crosspoint/pokemon-v2-a.bin` and
`/.crosspoint/pokemon-v2-b.bin`. Back up both to preserve a collection. The
in-device reset commits an empty snapshot to the inactive slot before replacing
the current collection, so an interrupted reset can recover the previous save.
Deleting both files manually also resets Pokémon onboarding without touching
books or reading statistics.

The artwork is user-supplied and generated locally. It is not part of public
source or public firmware releases. See [Artwork setup](artwork-setup.md) and
[Third-party assets](third-party-assets.md).

Pokédex detail cards are pre-sized and converted to one-bit BMPs on the computer,
then streamed one row at a time from the dedicated SD path. Portrait cards are
472×708 and landscape cards are 288×432. The device performs no grayscale
conversion and allocates no second framebuffer.

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
4. Verify list wrapping, dimension-derived Pokédex paging beyond the first
   screen, PC sorting, and the two-step reset. Open a seen Pokédex entry in both
   orientations and confirm the dedicated card is crisp and readable.
5. Turn real pages for at least one five-minute checkpoint, exit the book, and
   reboot. Confirm EXP and collection state survive.
6. Review Dashboard, Lyra, Lyra 3 Covers, and Rounded Raff in portrait and
   landscape. Confirm the companion band clears the dashboard controls and its
   sprite and complete EXP fraction remain distinct. A pending event should show
   only a `!` at the far right. Other themes intentionally omit the companion
   band.
7. Inspect `crash_report.txt` and serial logs if any restart occurs; preserve both
   Pokémon snapshots before reproducing a storage problem.

Until this checklist passes on the target X3, the archive is a software-verified
release candidate rather than a hardware-approved release.
