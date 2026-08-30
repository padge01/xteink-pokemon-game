# Pokemon Companion V2 Completion Design

**Status:** Approved in conversation on 2026-08-30.

## Goal

Complete the private `pokemon-x3` reading companion as a small, reliable layer over CrossInk's existing reader, storage, activity, and dashboard infrastructure. The firmware remains an e-reader first; the companion rewards verified reading and never runs a second timing or session system.

## Locked scope

- Original 151 only. No eggs, babies, friendship, day/night, battles, moves, stats, favourites, Johto, or event queue.
- Starter is Bulbasaur, Charmander, Squirtle, or Pikachu. The user chooses gender and is asked, `Would you like to give a nickname to <species>?`
- One XP per verified reading minute. Party position one is the lead and the only Pokemon that trains.
- Six dense Party slots, a paged PC Box, six Bag items, continuous 151-entry Pokedex, nicknames, encounters, and confirmed evolution.
- Five Kanto stones plus text-only Link Cable make all 151 obtainable. Level 100 without evolving is valid.
- Pending events remain outside books. Dashboard notices are exactly `NEW POKEMON`, `ITEM FOUND`, `WHAT'S THIS?`, or blank.
- Dashboard support is only Dashboard, Lyra, Lyra 3 Covers, and Rounded Raff. Classic, Minimal, and Lyra Carousel remain unchanged.
- Existing Generation VII wide icons remain unchanged. Use exact integer-size derivatives: 40x30 source, 80x60 list/dashboard, 120x90 focused presentation.
- V1 data is not migrated. V2 uses its existing dual-snapshot files and provides a protected beta reset.
- The feature is compiled only by `pokemon-x3`; standard builds contain no Pokemon references.

## Architecture

`PokemonGame` remains the pure rule engine and `PokemonStore` remains the only file owner. `PokemonService` becomes the single command/query boundary used by UI and dashboards. The activity never calls the store or rule engine directly and never keeps a roster-sized container in memory.

One compact `PokemonActivity` owns navigation and page-sized row buffers. It delegates text entry to the existing `KeyboardEntryActivity`, uses FreeInkUI plus `ButtonNavigator`, and renders with runtime oriented bounds. There is no V1-style activity model, recovery layer, event queue, or duplicated reading tracker.

`PokemonArt` is a small SD bitmap path/drawing helper. Missing Pokemon art renders an unboxed `?`; Link Cable deliberately has no icon. Art is never compiled into firmware and no image cache or second framebuffer is added.

Dashboard integration consumes one bounded `PokemonDashboardSnapshot` from the service and draws directly in each real theme's measured layout. Dashboard has no graphical EXP bar; it uses numeric `EXP current/next`. A dashboard read failure omits the accessory rather than affecting the home screen.

## Resource constraints

- At most 3,500 nonblank handwritten Pokemon production lines for the complete V2; generated species data, tests, scripts, and documentation are excluded.
- No new Pokemon UI source file over 700 nonblank lines.
- No per-render heap allocation, bare `new`, second framebuffer, roster-sized vector, or persistent open SD file.
- Page buffers are fixed arrays: Party 6, PC 6, menu 6, Pokedex page limited by visible rows.
- Final `pokemon-x3` image must retain at least 192 KiB partition headroom. Any lower result stops release for review.
- Additional static RAM above the committed V2 core must remain below 2 KiB.
- Activity open/close must return internal free heap and largest free block to within 2 KiB of baseline on hardware.

## Interaction rules

- Side Up/Down and touchscreen selection invoke the same actions. Up/Down wraps for finite menus and Party; long press pages PC and Pokedex.
- Empty Party slots are display-only and cannot be move targets. With one member, Move is absent. Party order is always dense.
- Species is always visible. Nickname occupies a reserved second identity line; blank nickname never changes header geometry.
- Encounter screens always show species, level, and gender before Catch/Pass. Catch commits first, then optionally opens nickname entry.
- A caught Pokemon fills Party until six; subsequent catches go to PC.
- The last owned Pokemon cannot be deposited. Withdraw is unavailable when Party is full.
- Evolution prompts can be disabled per Pokemon and remain disabled until explicitly enabled.
- Reset requires two deliberate confirmations and returns to starter selection.

## Delivery gates

1. Service command/query tests pass, including failure atomicity and invalid-party operations.
2. The minimum loop (starter, nickname, Party, Summary, pending event, reset) builds and is simulator-reviewable.
3. PC, Bag, Pokedex, and all event/evolution actions pass focused tests and simulator review.
4. Four real dashboard integrations are captured in both orientations and approved visually.
5. Host tests, simulator build, exact `pokemon-x3` build, size comparison, packaging verification, and physical X3 walkthrough pass before release.

Physical-device evidence cannot be substituted by simulator evidence. A candidate can be software-complete and packaged before hardware verification, but it is not release-approved until the X3 matrix passes.
