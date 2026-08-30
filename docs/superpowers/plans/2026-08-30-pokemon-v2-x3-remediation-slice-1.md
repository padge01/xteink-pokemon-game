# Pokémon V2 X3 Remediation — Slice 1

**Goal:** Correct the confirmed collection/storage defects without changing gameplay, dashboard layout, or the snapshot format.

## Boundaries

- Keep Pokémon absent from ordinary CrossInk builds.
- Add no activities, settings, mechanics, dependencies, or heap allocations.
- Do not touch dashboard rendering in this slice.
- Run only the focused Pokémon host targets; defer simulator and X3 builds.

## Tasks

1. Make PC page reads return success separately from the returned row count. A valid empty page succeeds with count zero; an SD read failure returns `StorageError` and the activity displays a load error.
2. Derive the Summary action list from the occupied Party count. Hide Move and Deposit when only one Pokémon is carried, and hide Withdraw when the Party is full.
3. Reset by writing and verifying a newer zero-record A/B snapshot. A failed or interrupted reset must leave the previous snapshot bootable; a completed reset must allow starter onboarding again.
4. Correct the beta checklist so it no longer instructs testers to move one Pokémon into an empty slot.
5. Add a changelog entry describing the user-visible fixes.

## Test-first checks

- `PokemonStoreTest`: valid empty PC vs read failure; successful reset; every interrupted reset byte preserves the old save.
- `PokemonServiceTest`: PC read failure becomes `StorageError`; completed reset permits a new starter.
- `PokemonGameTest`: the fixed-capacity action policy omits unavailable Move, Deposit, and Withdraw actions.

## Exit criteria

- The three focused targets pass with no warnings introduced by touched code.
- The diff contains no dashboard, gameplay-balance, or firmware packaging changes.
- Repository status contains only the intended Slice 1 files.
