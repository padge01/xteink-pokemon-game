# Pokemon Companion V2 Master Workstream

> Execute inline with `superpowers:executing-plans`. Do not delegate. Stop at each named gate for review.

**Goal:** Deliver a private, compile-gated Kanto reading companion for the X3 that reuses CrossInk's reader and UI infrastructure and remains reliable on ESP32-C3.

**Base:** `fix/kosync-2xx-compatibility` at `1ef5a97ac5153cc6c7a5a9d7ffd6c3de637d3a4f`.

**Branch:** `feat/pokemon-companion-v2`.

**Public-scope decision:** This is a game and therefore outside `SCOPE.md`. Keep it out of standard builds and do not propose it upstream. The private `pokemon-x3` environment is the only supported release target.

## Locked product boundary

- Original 151 only; no eggs, babies, friendship, day/night, battle stats, favourites, Johto, or event queue.
- Four starters: Bulbasaur, Charmander, Squirtle, Pikachu; starter gender is chosen.
- One XP per verified reading minute; only Party position one trains.
- Party of six, PC Box, six-item Bag, continuous Pokédex, nicknames, encounters, and confirmed evolutions.
- Five Kanto stones plus Link Cable make the Pokédex completable. Eevee has only its three Kanto stone evolutions.
- PC offers capture order and Pokédex-number order.
- Dashboard support is limited to Dashboard, Lyra, Lyra 3 Covers, and Rounded Raff.
- V1 beta data is not migrated. V2 uses different filenames and offers an explicit beta reset.

## Hard engineering gates

- No more than 2,200 handwritten production lines. Generated species/i18n/art data and tests are excluded.
- One framebuffer; no roster-sized in-memory container; no per-render allocation; no bare `new`.
- Snapshot storage is bounded to 1,024 records and about 49 KB per file.
- Final image retains at least 128 KB partition headroom and adds no more than 100 KB without review.
- Activity open/close cycles return heap to within 2 KB of baseline; no allocation failure may abort.
- Long PlatformIO checks are batched and require a visible approval immediately before execution.

## Phase sequence

1. **Baseline:** preserve V1, create this clean worktree, save plans, capture clean firmware size and produce clean X3 diagnostic firmware.
2. **Core and storage:** execute `2026-08-29-pokemon-companion-v2-core.md` using host tests and the 2,200-line projection gate.
3. **UI and dashboards:** execute `2026-08-29-pokemon-companion-v2-ui-dashboard.md`; require visual approval of every surface.
4. **Hardware and release:** execute `2026-08-29-pokemon-companion-v2-hardware-release.md`; flash clean baseline first, then V2.

## Completion definition

The workstream is complete only when deterministic host tests pass, the simulator build passes, the real four dashboards are approved, the clean firmware does not reproduce the reader-menu crash, the V2 hardware walkthrough passes, and a checksummed firmware/art/rollback package is available. Do not merge, publish, or deploy without explicit approval.
