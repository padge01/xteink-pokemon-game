# Pokemon Companion V2 Core Implementation Plan

> Execute inline with `superpowers:executing-plans`; use test-driven development for each behavior slice.

**Goal:** Implement the complete Kanto rules, atomic store, and verified-reading integration without UI or dashboard code.

**Architecture:** Pure C++ rules live in `lib/Pokemon`; SD adapters and the small application facade live in `src/pokemon`. The rules never include renderer, Activity, Arduino, or SD types. Readers only report verified minutes to the facade.

## Task 1: Define compact records and generated Kanto data

**Create:** `lib/Pokemon/PokemonTypes.*`, `lib/Pokemon/PokemonSpecies.*`, `scripts/data/pokemon-kanto-v2.csv`, `scripts/generate_pokemon_v2_species.py`, `test/pokemon_types/`.

- Write failing tests for 48-byte explicit record encoding, nickname bounds, all 151 species, types, gender ratios, XP levels, and every Kanto evolution.
- Define `PokemonRecord`: record ID, species ID, total XP, caught level, gender, origin, evolution-prompt flag, and `nickname[33]`.
- Define `PokemonState`: compact Party IDs, one tagged pending event, six item counts, 19-byte seen/caught bitsets, reading remainder/counters, lifetime minutes, sequence, and dashboard notice.
- Pin species names, types, gender rates, capture rates, and Kanto evolution facts to PokeAPI `api-data` revision `837b3642001d4853249114d2b5601d46e1004a45`. Check in a compact CSV with that provenance and an offline generator. Host tests generate the ignored header into their build directory without network access; Task 4 must give the private firmware environment an equivalent build-directory generation step before it links this library.
- Generate only immutable species/evolution tables; keep them `static const` in flash. Generated rows also tag evolution stage and acquisition as Wild, EvolutionOnly, Legendary, or Special.
- Prove malformed species, levels, flags, nicknames, and disk bytes fail without partial mutation.
- Commit: `feat: define lightweight pokemon v2 records`.

## Task 2: Implement pure progression, evolution, encounter, and item rules

**Create:** `lib/Pokemon/PokemonGame.*`, `test/pokemon_game/`.

Public operations:

```cpp
CreditResult applyCreditedMinutes(PokemonState&, PokemonRecord& leader, uint16_t minutes,
                                  uint8_t bookProgressPercent, OwnedEvolutionNeeds,
                                  RandomSource&);
bool resolveEncounter(PokemonState&, EncounterChoice, const char* nickname, RecordMutation&);
bool resolveEvolution(PokemonState&, PokemonRecord&, EvolutionChoice, RecordMutation&);
bool useEvolutionItem(PokemonState&, PokemonRecord&, EvolutionItem, RecordMutation&);
```

- One XP per minute. Let `n = clamp(level, 1, 100) - 1`; use `xpRequired(level) = 10*n + 3*n*n/4`, so Level 1 begins at zero XP, and clamp accumulated XP at Level 100.
- Encounter check each 60 verified minutes: 25%, with the sixth consecutive check forced after five misses. Book-progress bands 0/25/50/75/95 percent produce levels 2-6/5-10/9-16/14-24/18-30.
- A regular encounter pool excludes Legendary, Special, and EvolutionOnly rows. Base-stage rows are eligible at all progress, middle-stage rows at 50%+, and final-stage rows at 75%+. Select among eligible rows using the pinned capture rate as the integer weight, except Bulbasaur, Charmander, Squirtle, and Pikachu use weight 8 so all four remain rare wild encounters.
- EvolutionOnly contains the twenty item/Link Cable targets: Raichu, Nidoqueen, Nidoking, Clefable, Ninetales, Wigglytuff, Vileplume, Arcanine, Poliwrath, Alakazam, Machamp, Victreebel, Golem, Cloyster, Gengar, Exeggutor, Starmie, Vaporeon, Jolteon, and Flareon. They never appear wild, keeping all six items necessary for completion.
- On a triggered encounter, make one 5% legendary substitution roll. Articuno, Zapdos, and Moltres become eligible at 1,200 lifetime verified minutes and 75% current-book progress; Mewtwo becomes eligible at 3,000 lifetime minutes and 95% progress. Prefer an uncaught eligible legendary, then allow duplicates when all eligible candidates are caught.
- Once species 1-150 are all caught and Mew is not, the next triggered encounter is Mew instead of running the regular or legendary selection. This is the only Special acquisition rule.
- Item check each hour: 5%, forced after nineteen misses; prefer an item in the six-bit
  `OwnedEvolutionNeeds` mask, otherwise choose any item. `PokemonService` computes the mask by
  streaming records once at an item-check boundary; the rules layer never reads storage.
- Allow only one pending event. Accrue counters while blocked and generate at most one event per commit after resolution.
- Always catch on Catch; append to Party below six, otherwise PC. Use Link Cable for Kadabra, Machoke, Graveler, and Haunter.
- Tests cover all evolution branches, exact pity boundaries, deterministic rarity, full Party, full PC, and Mew.
- Commit: `feat: add lightweight pokemon v2 rules`.

## Task 3: Implement alternating atomic snapshots

**Create:** `lib/Pokemon/PokemonStoreCodec.*`, `src/pokemon/PokemonStore.*`, `test/pokemon_store/`; document in `docs/file-formats.md`.

Files are `/.crosspoint/pokemon-v2-a.bin` and `/.crosspoint/pokemon-v2-b.bin`. Each contains magic, version, sequence, sizes, count, state, 48-byte records, and whole-file CRC.

Public store boundary:

```cpp
enum class StoreBeginResult { Empty, Ready, Corrupt, Unsupported };
StoreBeginResult begin();
bool loadState(PokemonState&) const;
bool readRecord(uint32_t recordId, PokemonRecord&) const;
size_t readPcPage(PcOrder, size_t offset, std::span<PokemonRecord>) const;
bool commit(const PokemonState&, const RecordMutation& = {});
bool reset();
```

- Stream active to inactive while applying at most one replacement or append; sync, close, reopen, and verify before accepting its sequence.
- Explicitly close every `FsFile`; never open the same path twice. Different source/destination paths may be open concurrently.
- Startup chooses the newest valid snapshot. Preserve both files and return Corrupt if neither validates.
- PC ordering is capture order or stable species/record-ID order without loading the roster.
- Fault-injection tests interrupt every header/state/record/CRC boundary and prove the previous snapshot survives.
- Commit: `feat: add atomic pokemon v2 snapshots`.

## Task 4: Implement the service and Joshua-compatible reading tracker

**Create:** `src/pokemon/PokemonService.*`, `lib/Pokemon/PokemonTracker.*`, `test/pokemon_tracker/`. **Modify:** `platformio.ini` plus EPUB/TXT/XTC reader activities only at successful-turn, checkpoint, and exit boundaries.

```cpp
void PokemonTracker::onSuccessfulPageTurn(uint32_t nowMs);
void PokemonTracker::checkpointIfDue(uint32_t nowMs);
void PokemonTracker::flushOnExit(uint32_t nowMs);
bool PokemonService::creditMinutes(uint16_t minutes, uint8_t bookProgressPercent);
```

- Port Joshua Miller's active trailing-five-minute `SessionAccumulator` semantics exactly.
- Credit only elapsed intervals proven by successful page turns; idle-open time earns nothing.
- Checkpoint each five credited minutes and flush a valid remainder on reader exit.
- `PokemonService` is the only device facade; every operation builds a candidate state/record mutation and reports success only after durable commit.
- Golden tests feed identical timelines to Joshua's reference and V2 for EPUB/TXT/XTC semantics.
- Add the private `pokemon-x3` environment and compile guards `CROSSINK_ENABLE_POKEMON`, `CROSSINK_POKEMON_DEFAULT_ENABLED`, and `CROSSINK_POKEMON_BETA_TOOLS`. Standard targets must not reference or link the feature.
- Commit: `feat: credit pokemon from verified reading`.

## Core exit gate

- Run only the targeted host CMake/CTest targets during implementation.
- Count production lines and project UI/dashboard totals. Stop if core exceeds 1,050 lines or the total projection exceeds 2,200.
- Review every heap allocation and storage failure path before beginning UI work.
- With visible approval, build `pokemon-x3` once at this gate. Compare against the recorded clean baseline, reject less than 128 KB partition headroom, and stop for review if the core delta makes the 100 KB total budget implausible.
