# Pokemon Companion V2 UI and Dashboard Plan

> Begin only after the core gate passes. Use the existing FreeInkUI patterns; do not recreate the V1 state machine.

**Goal:** Expose the approved game through one compact activity and integrate one readable companion band into four real dashboards.

## Task 1: Build shared list/layout primitives and onboarding

**Create:** `src/activities/pokemon/PokemonActivity.*`, `src/components/pokemon/PokemonUi.*`, targeted host layout/state tests.

- One activity owns an enum of about twelve screens and fixed page-sized row arrays.
- Use `TouchHeaderBackButton`, `UITheme`, oriented safe bounds, `ButtonNavigator` wrapping, and FreeInkUI list selection/follow behavior.
- Bottom hints expose only real Back/Select controls; side buttons are Up/Down.
- Starter flow: species, chosen gender, then `Would you like to give a nickname to [species]?` and the existing keyboard activity.
- Keep sprite gutter white so selection inversion never erases art.
- Commit: `feat: add pokemon v2 onboarding and navigation`.

## Task 2: Add event, Party, PC, Summary, Bag, and Pokédex screens

- Event screens show encounter level/gender and Catch/Pass; durable Catch precedes the nickname prompt.
- Party actions are Summary, Move, Deposit. Hide Move for one member and Deposit for the final member; move only among occupied positions.
- PC reads bounded pages and toggles capture-order/Pokédex-number order; Withdraw appends to Party.
- Summary has fixed identity alignment and actions for Nickname, Evolutions On/Off, and applicable items.
- Bag shows owned counts; stone art is used and Link Cable intentionally has no icon.
- Pokédex is a continuous 151-row paged list with Seen/Caught/`???`; it must reach entry 151 and wrap correctly.
- All new strings use `tr(STR_*)`; generate i18n output from translation sources.
- Exact notices: `NEW POKEMON`, `ITEM FOUND`, `WHAT'S THIS?`, or blank. Remove internal/recovery wording from normal surfaces.
- Commit: `feat: add pokemon v2 collection screens`.

## Task 3: Generate exact-size one-bit artwork

**Create/modify:** artwork generator, manifest validator, `pokemon-pack/`, third-party attribution.

- Keep the pinned Generation VII 40x30 wide-icon source and license pathway.
- Produce exact 80x60 row/dashboard and 160x120 focus assets using crop plus integer nearest-neighbour scaling.
- Validate dimensions, one-bit palette, hashes, and minimum ink bounding-box coverage.
- Missing art renders an unboxed `?` and logs once.
- Commit: `build: generate pokemon v2 artwork pack`.

## Task 4: Integrate four real dashboards

**Modify:** `HomeActivity` and the Dashboard, Lyra, Lyra 3 Covers, and Rounded Raff theme paths only.

- Compile the accessory entirely out of standard builds.
- Each theme clears and owns a measured full-width band, but it does not force every datum into one row. Priority is: an 80x60 sprite and name/level always; numeric EXP next; gender next; optional notice last. A notice may replace lower-priority secondary text or use a second line, but must never shrink the sprite or collide with controls.
- Measure each real theme independently. One-line and two-line bands are both valid; sharing the data/view-model does not require sharing identical geometry.
- Do not change Classic, Minimal, Lyra Carousel, or other themes.
- Generate actual simulator captures in portrait and landscape; browser mockups are not acceptance evidence.
- Require explicit visual approval for every activity screen and each of the eight theme/orientation captures.
- Commit: `feat: integrate pokemon v2 dashboards`.

## UI exit gate

- Navigation tests cover wraparound, page boundaries, full Party, one-member Party, empty/full PC, and all pending events.
- Simulator walkthrough shows consistent headers and no selected-row sprite erasure.
- UI/dashboard production code keeps the overall workstream within 3,500 Pokemon production lines, with no new Pokemon UI file above 700 nonblank lines.
- The core gate may run its one approved X3 link/size build. Do not build the integrated UI/dashboard X3 candidate until visual approval.
