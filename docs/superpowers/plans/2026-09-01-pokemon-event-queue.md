# Pokémon Event Queue Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the single pending Pokémon event with a backward-compatible three-event FIFO queue.

**Architecture:** Store three compacted `PendingEvent` values directly in `PokemonState`, expose small queue helpers, and make the game and service operate only through those helpers. Encode new snapshots as version 2 with a 116-byte state while continuing to read and migrate version 1 snapshots with their 96-byte state.

**Tech Stack:** C++20, fixed `std::array` storage, SdFat-backed alternating snapshots, CMake/CTest host tests, PlatformIO ESP32-C3 build.

**Spec:** `docs/superpowers/specs/2026-09-01-pokemon-event-queue-design.md`

## Global Constraints

- Queue capacity is exactly three and includes encounters, item finds, and evolution prompts.
- No heap allocation, new activity, setting, dependency, file, task, or additional SD write path.
- Existing version 1 saves migrate without losing Pokémon, XP, party order, items, Pokédex state, pity counters, or reading progress.
- Reading and lead-Pokémon XP continue while a full queue defers event generation.
- The dashboard remains a single `!` indicator and does not show queue depth.
- All user-facing UI strings continue to use the existing translated strings.
- Keep the work safe for the ESP32-C3 X3 and validate the default PlatformIO environment after host tests.

---

### Task 1: Fixed Queue Model

**Files:**
- Modify: `lib/Pokemon/PokemonTypes.h:45-108`
- Modify: `lib/Pokemon/PokemonTypes.cpp:184-240`
- Test: `test/pokemon_types/PokemonTypesTest.cpp`

**Interfaces:**
- Produces: `PENDING_EVENT_CAPACITY`, `PokemonState::pendingEvents`, `pendingEventCount`, `pendingEventFront`, `enqueuePendingEvent`, `dequeuePendingEvent`, and `removePendingEvolutionsForRecord`.
- Consumes: existing `PendingEvent`, `PendingEventKind`, and `validateState` rules.

- [ ] **Step 1: Write failing queue tests**

Add focused checks that an empty queue has no front, three distinct events enqueue in FIFO order, a fourth enqueue fails without mutation, dequeue compacts the array, and removing evolution events for one record preserves unrelated entries:

```cpp
PokemonState state{};
CHECK(pendingEventCount(state) == 0);
CHECK(pendingEventFront(state) == nullptr);
CHECK(enqueuePendingEvent(state, encounter));
CHECK(enqueuePendingEvent(state, item));
CHECK(enqueuePendingEvent(state, evolution));
CHECK(!enqueuePendingEvent(state, fourth));
CHECK(*pendingEventFront(state) == encounter);
CHECK(dequeuePendingEvent(state));
CHECK(*pendingEventFront(state) == item);
CHECK(removePendingEvolutionsForRecord(state, evolution.recordId) == 1);
CHECK(pendingEventCount(state) == 1);
```

- [ ] **Step 2: Run the focused target and confirm the tests fail**

Run:

```powershell
cmake -S test -B build/test
cmake --build build/test --target PokemonTypesTest -j 8
ctest --test-dir build/test -R PokemonTypesTest --output-on-failure
```

Expected: compilation fails because the queue API does not exist.

- [ ] **Step 3: Implement the fixed compacted queue**

Use the following public shape and keep all mutation inside `PokemonTypes.cpp`:

```cpp
constexpr size_t PENDING_EVENT_CAPACITY = 3;

struct PokemonState {
  std::array<uint32_t, PARTY_SIZE> partyRecordIds{};
  std::array<PendingEvent, PENDING_EVENT_CAPACITY> pendingEvents{};
  // existing fields unchanged
};

size_t pendingEventCount(const PokemonState& state);
const PendingEvent* pendingEventFront(const PokemonState& state);
PendingEvent* pendingEventFront(PokemonState& state);
bool enqueuePendingEvent(PokemonState& state, const PendingEvent& event);
bool dequeuePendingEvent(PokemonState& state);
size_t removePendingEvolutionsForRecord(PokemonState& state, uint32_t recordId);
```

`validateState` must reject a `None` gap followed by a non-empty event and validate every populated event with the existing exhaustive kind switch.

- [ ] **Step 4: Run `PokemonTypesTest` and confirm it passes**

Run the Step 2 build and CTest commands. Expected: PASS.

- [ ] **Step 5: Commit the queue model**

```powershell
git add lib/Pokemon/PokemonTypes.h lib/Pokemon/PokemonTypes.cpp test/pokemon_types/PokemonTypesTest.cpp
git commit -m "feat: add fixed pokemon event queue"
```

### Task 2: Versioned State Codec

**Files:**
- Modify: `lib/Pokemon/PokemonStoreCodec.h:11-38`
- Modify: `lib/Pokemon/PokemonStoreCodec.cpp:10-139`
- Modify: `test/pokemon_store/PokemonStoreCodecTest.cpp`

**Interfaces:**
- Consumes: compacted `PokemonState::pendingEvents` from Task 1.
- Produces: snapshot version constants, `SnapshotHeader::version`, `snapshotStateBytes`, v2 `encodeState`, and size-aware `decodeState`.

- [ ] **Step 1: Write failing v1/v2 codec tests**

Add tests that v2 round-trips three events and remains 116 bytes; a hand-built v1 header and 96-byte state decode the old pending event into queue index zero; unknown versions remain unsupported; and malformed queue gaps fail validation.

```cpp
CHECK(POKEMON_STATE_BYTES == 116);
CHECK(POKEMON_STATE_V1_BYTES == 96);
CHECK(header.version == POKEMON_SNAPSHOT_VERSION);
CHECK(snapshotStateBytes(1) == 96);
CHECK(snapshotStateBytes(2) == 116);
CHECK(snapshotStateBytes(3) == 0);
CHECK(decoded.pendingEvents[0] == legacyPending);
CHECK(decoded.pendingEvents[1].kind == PendingEventKind::None);
```

- [ ] **Step 2: Run `PokemonStoreCodecTest` and confirm failure**

```powershell
cmake --build build/test --target PokemonStoreCodecTest -j 8
ctest --test-dir build/test -R PokemonStoreCodecTest --output-on-failure
```

Expected: compilation fails on the missing versioned codec API.

- [ ] **Step 3: Implement snapshot version 2 and legacy decoding**

Define exact sizes and make the header carry the decoded version:

```cpp
constexpr uint16_t POKEMON_SNAPSHOT_VERSION_V1 = 1;
constexpr uint16_t POKEMON_SNAPSHOT_VERSION = 2;
constexpr size_t POKEMON_STATE_V1_BYTES = 96;
constexpr size_t POKEMON_STATE_BYTES = 116;

struct SnapshotHeader {
  uint16_t version = POKEMON_SNAPSHOT_VERSION;
  uint32_t sequence = 0;
  uint32_t recordCount = 0;
  bool operator==(const SnapshotHeader&) const = default;
};

size_t snapshotStateBytes(uint16_t version);
bool decodeState(const uint8_t* bytes, size_t size, uint16_t version, PokemonState& output);
```

Keep v1 offsets unchanged. For v2, encode the three 10-byte events from offset 24, item counts from 54, seen bits from 66, caught bits from 85, lifetime minutes at 104, sequence at 108, and the three counters/notice at 112-115. Continue zero-initializing the candidate before encoding.

- [ ] **Step 4: Run `PokemonStoreCodecTest` and confirm it passes**

Run Step 2 commands. Expected: PASS.

- [ ] **Step 5: Commit the versioned codec**

```powershell
git add lib/Pokemon/PokemonStoreCodec.h lib/Pokemon/PokemonStoreCodec.cpp test/pokemon_store/PokemonStoreCodecTest.cpp
git commit -m "feat: migrate pokemon saves to event queue"
```

### Task 3: Alternating Snapshot Migration

**Files:**
- Modify: `src/pokemon/PokemonStore.cpp:12-300`
- Modify: `test/pokemon_store/PokemonStoreTest.cpp`

**Interfaces:**
- Consumes: `SnapshotHeader::version`, `snapshotStateBytes`, and size-aware `decodeState` from Task 2.
- Produces: mixed-version inspection, loading, record offsets, CRC verification, and v2-only writes.

- [ ] **Step 1: Write failing store migration and recovery tests**

Create v1 snapshot fixtures through test helpers and prove:

```cpp
CHECK(store.begin() == StoreBeginResult::Ready);
CHECK(store.loadState(state));
CHECK(*pendingEventFront(state) == legacyPending);
CHECK(store.commit(state, {}));
CHECK(readHeader(inactivePath).version == POKEMON_SNAPSHOT_VERSION);
```

Add mixed v1/v2 snapshots where the newer valid sequence wins, corrupt v2 falls back to valid v1, and records remain readable after migrating because their offset is derived from the active header version.

- [ ] **Step 2: Run `PokemonStoreTest` and confirm failure**

```powershell
cmake --build build/test --target PokemonStoreTest -j 8
ctest --test-dir build/test -R PokemonStoreTest --output-on-failure
```

Expected: migration assertions fail because the store assumes one fixed state size.

- [ ] **Step 3: Make every state and record offset version-aware**

Replace the fixed `RECORDS_OFFSET` with:

```cpp
uint64_t recordsOffset(const SnapshotHeader& header) {
  return POKEMON_SNAPSHOT_HEADER_BYTES + snapshotStateBytes(header.version);
}
```

Inspection reads only the payload size declared by the accepted header version into a 116-byte zeroed buffer, hashes exactly those bytes, and calls the size-aware decoder. Loading and every record scan seek via `recordsOffset(activeHeader_)`. Writing always uses a v2 header and copies records from the active version's derived offset.

- [ ] **Step 4: Run codec and store tests together**

```powershell
cmake --build build/test --target PokemonStoreCodecTest PokemonStoreTest -j 8
ctest --test-dir build/test -R "PokemonStore(Codec)?Test" --output-on-failure
```

Expected: both tests PASS.

- [ ] **Step 5: Commit store migration**

```powershell
git add src/pokemon/PokemonStore.cpp test/pokemon_store/PokemonStoreTest.cpp
git commit -m "fix: preserve legacy pokemon saves during queue migration"
```

### Task 4: Queue Generation and Resolution

**Files:**
- Modify: `lib/Pokemon/PokemonGame.cpp:100-480`
- Modify: `src/pokemon/PokemonService.cpp:180-310`
- Modify: `src/pokemon/PokemonService.h:24-72`
- Modify: `src/activities/pokemon/PokemonActivity.cpp:130-450`
- Modify: `test/pokemon_game/PokemonGameTest.cpp`
- Modify: `test/pokemon_service/PokemonServiceTest.cpp`

**Interfaces:**
- Consumes: queue helpers from Task 1 and migrated store from Task 3.
- Produces: FIFO event generation, full-queue deferral, front-event resolution, dashboard projection, and immediate display of the next event.

- [ ] **Step 1: Replace old single-event test setup with queue helpers and add failing behavioral tests**

Cover all approved behavior:

```cpp
CHECK(result.status == CreditStatus::Applied);
CHECK(leader.totalXp == xpBefore + creditedMinutes);
CHECK(pendingEventCount(state) == 3);
CHECK(state.encounterMisses == 3);
CHECK(*pendingEventFront(state) == firstEvent);
```

Add same-boundary item/encounter/evolution ordering, catch/pass/acknowledge/evolve popping only the front, prompt disabling removing every matching queued evolution, and the service snapshot projecting the front event while the dashboard remains pending until the queue empties.

- [ ] **Step 2: Run game and service tests and confirm failure**

```powershell
cmake --build build/test --target PokemonGameTest PokemonServiceTest -j 8
ctest --test-dir build/test -R "Pokemon(Game|Service)Test" --output-on-failure
```

Expected: compilation or assertions fail at remaining `state.pending` behavior.

- [ ] **Step 3: Route all generation and resolution through queue helpers**

Build a complete `PendingEvent` locally before enqueueing. When full, do not consume a random roll or clear pity. On resolution, validate only the front event, persist the mutation and shifted candidate state atomically, then publish the new front through the existing service snapshot.

The activity entry and post-action paths use:

```cpp
setScreen(snapshot_.pending.kind == PendingEventKind::None ? Screen::Menu : Screen::Event);
```

where `snapshot_.pending` remains the service's front-event projection. After a successful event action, reload the snapshot and remain on `Screen::Event` if the next front exists.

- [ ] **Step 4: Run all focused Pokémon tests**

```powershell
cmake --build build/test --target PokemonTypesTest PokemonGameTest PokemonStoreCodecTest PokemonStoreTest PokemonServiceTest -j 8
ctest --test-dir build/test -R "Pokemon(Types|Game|Store|StoreCodec|Service)Test" --output-on-failure
```

Expected: all focused tests PASS.

- [ ] **Step 5: Commit queue behavior**

```powershell
git add lib/Pokemon/PokemonGame.cpp src/pokemon/PokemonService.cpp src/pokemon/PokemonService.h src/activities/pokemon/PokemonActivity.cpp test/pokemon_game/PokemonGameTest.cpp test/pokemon_service/PokemonServiceTest.cpp
git commit -m "feat: queue pokemon reading events"
```

### Task 5: Documentation, Gallery, and X3 Gate

**Files:**
- Modify: `docs/file-formats.md:46-81`
- Modify: `CHANGELOG.md:1-40`
- Modify: `README.md:1-45`
- Add: `docs/screenshots/pokemon-menu.jpg`
- Add: `docs/screenshots/wild-encounter.jpg`
- Add: `docs/screenshots/party.jpg`
- Add: `docs/screenshots/pc-box.jpg`
- Add: `docs/screenshots/pokemon-summary.jpg`
- Add: `docs/screenshots/pokedex.jpg`

**Interfaces:**
- Consumes: final v2 offsets and verified queue behavior.
- Produces: public migration documentation, user-facing changelog entry, and the approved six-image interface gallery.

- [ ] **Step 1: Document exact v1/v2 behavior and add the approved gallery**

Update the file-format table to show both payload sizes and v2 offsets. Add a concise changelog bullet: “Up to three encounters, item finds, and evolution prompts now wait in order while reading continues; existing Pokémon saves migrate automatically.” Add a two-column README gallery after the introduction and label it “Interface preview.”

- [ ] **Step 2: Run documentation and whitespace checks**

```powershell
git diff --check
node --test site/test/*.test.mjs
```

Expected: no whitespace errors and all site tests PASS.

- [ ] **Step 3: Build the X3 firmware**

Run:

```powershell
pio run -e default
```

Expected: SUCCESS with no unsupported allocations or compile-time regressions. Record the final firmware size; do not claim a memory improvement from this change.

- [ ] **Step 4: Review the complete diff and commit documentation**

```powershell
git diff --stat HEAD~4
git status --short
git add README.md CHANGELOG.md docs/file-formats.md docs/screenshots
git commit -m "docs: explain queued pokemon events"
```

- [ ] **Step 5: Report the hardware verification path**

On the physical X3, preserve the current v1 save, flash the new firmware, and verify: existing party and progress load; three events can wait; the dashboard keeps one `!`; resolving the first immediately shows the second; reading with a full queue still grants XP; and a later eligible check fills the freed slot. Do not publish a release until this path succeeds.
