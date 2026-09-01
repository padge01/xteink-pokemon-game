# Pokémon Event Queue Design

## Goal

Allow up to three Pokémon events to wait while the user continues reading. A
pending event must no longer prevent later encounters, item finds, or evolution
prompts from being retained.

The change must preserve existing Pokémon saves, remain safe on the Xteink X3,
and add no heap allocation, activity, setting, dependency, or additional SD
write path.

## Queue behavior

- Store three `PendingEvent` values directly in `PokemonState` as a compacted
  first-in, first-out queue.
- The event at index zero is the active event shown by the Pokémon activity.
- Encounters, item finds, and evolution prompts all use the same queue.
- Events generated during one credited minute retain the engine's existing
  evaluation order: hourly item, encounter, then level evolution.
- Resolving or dismissing the active event shifts the remaining entries toward
  index zero. If another event remains, the activity shows it immediately.
- The dashboard displays its existing `!` indicator whenever the queue is not
  empty. It does not display a queue count.
- Reading time and lead-Pokémon XP continue while the queue is full.
- A full queue does not generate and discard a fourth event. Encounter and item
  guarantee counters remain primed so generation can succeed at the next
  eligible check after space becomes available.
- Disabling evolution prompts removes a matching active or queued evolution
  event for that Pokémon without disturbing unrelated events.

## Representation and resource cost

Use a fixed `std::array<PendingEvent, 3>` with empty entries compacted at the
end. Queue fullness and emptiness are determined from the existing
`PendingEventKind::None` sentinel, so no head, tail, count, vector, or dynamic
allocation is required.

Replacing one in-memory `PendingEvent` with three adds approximately 24 bytes
to `PokemonState`. The serialized state grows by exactly 20 bytes because each
event already has a 10-byte wire representation. Normal event operations copy
or shift at most two small values and do not add SD writes.

## Save compatibility

Introduce Pokémon snapshot format version 2 with a 116-byte state payload.
Version 2 stores the three 10-byte events consecutively and shifts the fields
that currently follow the single event.

The loader accepts both formats:

- A version 1 snapshot reads its existing 96-byte state and places its single
  pending event at queue index zero.
- A version 2 snapshot reads the new 116-byte state directly.
- Snapshot size and CRC validation use the version declared in the header.
- Alternating snapshot files may temporarily contain different versions; the
  valid snapshot with the newest sequence remains authoritative.
- The next successful commit writes version 2 through the existing alternating,
  CRC-protected store. The older valid snapshot remains available for recovery
  until a later commit replaces it.

No Pokémon, XP, party order, items, Pokédex progress, pity counters, or reading
progress is reset during migration.

## Code boundaries

- `lib/Pokemon/PokemonTypes.*`: queue representation, validation, and small
  front/push/pop helpers.
- `lib/Pokemon/PokemonGame.*`: enqueue generated events, defer when full, and
  remove resolved or disabled events.
- `lib/Pokemon/PokemonStoreCodec.*`: versioned 96-byte and 116-byte state
  decoding plus version 2 encoding.
- `src/pokemon/PokemonStore.*`: choose payload size by snapshot version and
  preserve alternating-snapshot recovery.
- `src/pokemon/PokemonService.*`: expose the front event and resolve it without
  changing the UI-facing service shape unnecessarily.
- `src/activities/pokemon/PokemonActivity.*`: remain on the event screen while
  another queued event exists.
- `docs/file-formats.md` and `CHANGELOG.md`: document the migration and
  user-facing behavior.

No new screen, setting, task, file, or dependency is introduced.

## Failure behavior

- Invalid event kinds, malformed compacted queues, bad versions, incorrect file
  lengths, and CRC failures reject that snapshot through the existing recovery
  path.
- A failed save leaves the current in-memory queue unchanged and retains the
  previous valid alternating snapshot.
- If one snapshot is corrupt or unsupported, the other valid snapshot is used.
- If neither snapshot is valid, the existing recovery UI remains responsible
  for reporting the problem.

## Validation

Targeted tests must prove:

1. FIFO ordering across encounters, items, and evolutions.
2. Immediate presentation of the next event after resolving the front event.
3. Reading and XP continue while a full queue defers additional generation.
4. Encounter and item guarantees remain primed while full.
5. Multiple events generated at one boundary retain item, encounter, evolution
   order without exceeding three entries.
6. Disabling evolution prompts removes only matching evolution events.
7. Version 1 snapshots migrate without losing any field.
8. Mixed-version alternating snapshots select the newest valid sequence.
9. Corrupt and interrupted version 2 snapshots recover from the other copy.
10. Existing reset, catch, pass, item acknowledgement, and evolution behavior
    remains correct.

After the focused host tests pass, build the default ESP32-C3 X3 firmware. No
claim of hardware readiness is made until the resulting build is exercised on
the physical X3 through a full queue-and-resolution flow.
