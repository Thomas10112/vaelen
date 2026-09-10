# VAELEN Roadmap

STATUS: VALIDATED for the Phase 00 status it reports (checked against the code and test
runs of 2026-09-05 on branch `claude/vaelen-master-prompt-aw7zqj`); everything from
Phase 01 onwards is PLANNED and does not exist in the repository.

World: AELVOR. Slogan: "NOTHING IS GIVEN. EVERYTHING IS INHERITED." Secondary: "HISTORY
BELONGS TO NO ONE." The player starts as an enslaved person in a huge autonomous mining
colony. No chosen one, no main quest, no canonical ending.

The master prompt is the authority for the phase list and its objectives; the objective
lines below paraphrase it in one sentence each. Work follows the protocol
PLAN -> EXPLAIN -> IMPLEMENT -> TEST -> VALIDATE -> NEXT, and every progress report shows
the BUILD STATUS block (`Docs/CONVENTIONS.md` section 14.2).

Contents

1. [Status labels](#1-status-labels)
2. [Exit criteria for a phase](#2-exit-criteria-for-a-phase)
3. [The 20 phases](#3-the-20-phases)
4. [Phase 00 - FOUNDATION: task breakdown and real status](#4-phase-00---foundation-task-breakdown-and-real-status)
5. [Phase 01 - CORE SIMULATION: task breakdown (PLANNED)](#5-phase-01---core-simulation-task-breakdown-planned)
6. [Phases 02-20: notes](#6-phases-02-20-notes)
7. [Current BUILD STATUS](#7-current-build-status)
8. [Verification record](#8-verification-record)

---

## 1. Status labels

| Label | Meaning in this roadmap |
|---|---|
| VALIDATED | Code exists, compiles warning-free with clang++ and g++, tests exist and were run and pass |
| PROTOTYPE | Code exists and works for a narrow case; API or behaviour may still change |
| INCOMPLETE | Code or deliverables exist with known missing parts, listed next to the label |
| UNVERIFIED | Code exists but cannot be compiled or run in this environment (Unreal-facing files) |
| PLANNED | Nothing exists yet |

A task is VALIDATED only if its tests were actually run (master prompt: no fake "done").

## 2. Exit criteria for a phase

A phase is closed, and may be used as a foundation by the next one, when all of the
following hold:

1. All tests are green on the CI matrix defined in `.github/workflows/kernel-ci.yml`
   and `CMakePresets.json`: Linux clang and gcc in Debug, RelWithDebInfo and
   RelWithDebInfo without assertions, clang-format, Windows MSVC Debug, macOS AppleClang
   Debug, including the `Kernel.Purity` and `Kernel.PuritySelfTest` entries.
2. Determinism tests exist and pass for every system of the phase: same seed and same
   inputs give identical results; state round-trips through save and restore; frozen
   values guard anything that feeds the save format.
3. No file in the modules of the phase carries `STATUS: INCOMPLETE`. PROTOTYPE files
   are allowed only if the closing report lists them with their limits. Engine-facing
   files may stay UNVERIFIED until an engine-backed runner exists, and the report says so.
4. The test categories required by the master prompt are present for every important
   system: unit, integration, deterministic, edge-case and long-duration (the last one
   from the first phase that has a simulation loop, Phase 01).
5. `Docs/ARCHITECTURE.md`, `Docs/CONVENTIONS.md`, `Docs/DECISIONS.md` and this file are
   updated, every architecture decision of the phase has an ADR, and the closing progress
   report shows the BUILD STATUS block with the exact commands and results.

Changing a closed phase's public API afterwards requires an ADR and, when a persisted
layout changes, a `VAELEN_SAVE_FORMAT_VERSION` bump (`Version.h`).

## 3. The 20 phases

| Phase | Name | Objective (one line) | Status |
|---|---|---|---|
| 00 | FOUNDATION | Engine-agnostic kernel skeleton with dual build, core primitives (types, ids, hashing, random streams, logging, assertions, versions), base interfaces limited to `ILogSink` and `AssertHandler` (system/archive interfaces deferred to Phase 01, section 4), test harness, purity check, CI and documentation. | VALIDATED headless (00.01-00.05); engine build UNVERIFIED |
| 01 | CORE SIMULATION | Entities, components, systems and a deterministic tick scheduler; simulation clock and calendar; event bus and event log; snapshot interfaces; deterministic replay; simulation LOD 0-4 hooks. | VALIDATED (headless, 01.01-01.08); UNVERIFIED (engine) |
| 02 | WORLD | Procedural world of AELVOR derived from the seed: regions, tiles, terrain, climate, hydrology, resource deposits. | VALIDATED (headless, 02.01-02.08); UNVERIFIED (engine) |
| 03 | HISTORY | Simulated pre-history that everything later inherits: eras, cultures, languages, religions, migrations, the historical record. | VALIDATED (headless); UNVERIFIED (engine) |
| 04 | POPULATION | Persons and families: birth, ageing, death, lineage, needs, demographics. | VALIDATED (headless, 04.01-04.08); UNVERIFIED (engine) |
| 05 | SOCIETY | Organisations, social structure, status, bondage and slavery as institutions, norms. | VALIDATED (headless, 05.01-05.08); UNVERIFIED (engine) |
| 06 | ECONOMY | Items, production, markets, prices, trade, wealth and its transmission. | VALIDATED (headless, 06.01-06.08); UNVERIFIED (engine) |
| 07 | POLITICS | Polities, laws, authority, succession, factions, diplomacy. | VALIDATED headless (07.01-07.08, phase closed); UNVERIFIED under UBT |
| 08 | MILITARY | Armies, conflicts, wars, security forces, conquest and its consequences. | CLOSED (08.01-08.08 VALIDATED headless; UNVERIFIED under UBT) |
| 09 | INFRASTRUCTURE | Buildings, settlements, routes, logistics and their decay. | CLOSED (09.01-09.08 VALIDATED headless; UNVERIFIED under UBT) |
| 10 | PLAYER | The player as one simulated person: enslaved start, body, needs, skills, relationships; player intent as commands into the simulation. | CLOSED (10.01-10.08, section 14) |
| 11 | MINING COLONY | The starting place: a huge autonomous mining colony simulated by the same systems at full detail. | CLOSED (11.01-11.08 VALIDATED headless, CI run 113 green on all nine jobs; UNVERIFIED under UBT) |
| 12 | GAMEPLAY | Interaction verbs, knowledge (documents, maps), reputation and consequences without main quest or canonical ending. | BROKEN DOWN (12.01-12.08, section 16) |
| 13 | PRESENTATION | Unreal rendering, animation and audio of the world state, strictly read-only. | PLANNED |
| 14 | UI | Interface and read-only views; command submission through the gameplay layer. | PLANNED |
| 15 | STREAMING & LOD | Engine streaming coupled to simulation LOD 0-4: what is simulated at which detail away from the player. | PLANNED |
| 16 | SAVE/PERSISTENCE | Save format, serialisation of the whole world state, checkpoints on disk, migrations keyed on the save-format version. | PLANNED |
| 17 | DEBUG TOOLS | Inspectors, replay tooling, determinism diff, world statistics, headless console. | PLANNED |
| 18 | STRESS TEST | Long-duration and large-world runs, performance budgets, determinism at scale. | PLANNED |
| 19 | MODDING | Data-driven definitions, mod loading, stable ids and APIs for mods. | PLANNED |
| 20 | POLISH | Balance, content, quality, release readiness. | PLANNED |

Planned module names per phase are listed in `Docs/ARCHITECTURE.md` section 3.2.
`IdKind` in `Ids.h` already reserves value ranges for Phases 01-12 (see ADR-0004).

## 4. Phase 00 - FOUNDATION: task breakdown and real status

Status below was established by reading the code and the tests in `Tests/Core` and by
running them (section 13). Test counts are from `VaelenCoreTests --list`. This numbering
is canonical: `Docs/STATUS.md` and commit subjects (`<phase>.<task>: ...`) use it.

Master prompt scope item "interfaces de base": Phase 00 delivers only the two interfaces
the foundation itself needs, `ILogSink` (`Log.h`) and the `AssertHandler` function
pointer (`Assert.h`). `ISystem`/`TickContext`, `IArchive`/snapshot and the command
interface are deliberately deferred to 01.03, 01.06 and Phase 10, because their shape
depends on the entity/component model that Phase 01 introduces. This scope change is
recorded here so that it stays visible.

### 00.01 Project architecture - VALIDATED (headless) / UNVERIFIED (engine)

Deliverables present:

- Unreal project: `Vaelen.uproject` (UE 5.6; modules `VaelenCore` PreDefault, `Vaelen`
  Default; plugins EnhancedInput, ModelingToolsEditorMode), `Source/Vaelen.Target.cs`,
  `Source/VaelenEditor.Target.cs`, `Source/VaelenCore/VaelenCore.Build.cs`
  (export/assert/log-floor definitions from the target configuration),
  `Source/Vaelen/Vaelen.Build.cs`, `Config/Default{Engine,Game,Editor,Input}.ini`.
- Engine bridge module `Source/Vaelen`: `FVaelenModule` installs `FVaelenLogSink`
  (kernel records to `UE_LOG`, UTF-8), the kernel assertion handler (per-site ensure
  dedupe) and aligns the kernel log floor with `LogVaelen`.
- Headless build: `/CMakeLists.txt` (options, warning set, `-fno-exceptions -fno-rtti
  -ffp-contract=off`, MSVC equivalents), `Source/VaelenCore/CMakeLists.txt` (explicit
  source list), `Tests/CMakeLists.txt`, `Tests/Core/CMakeLists.txt`, `CMakePresets.json`
  (schema 5, CMake >= 3.24; six Linux presets, macOS, Windows).
- CI: `.github/workflows/kernel-ci.yml` (Linux clang/gcc x Debug/RelWithDebInfo/
  no-asserts, clang-format 18, Windows MSVC, macOS `macos-15`; least-privilege token,
  concurrency cancellation, timeouts).
- Style and repository hygiene: `.clang-format` (reproduces the code, checked in CI),
  `.editorconfig`, `.gitattributes` (LF), `.gitignore`.
- Layering and module plan: `Docs/ARCHITECTURE.md` sections 1-4.

Verified: headless configure, build and `ctest` for all six Linux presets (section 13),
and the full GitHub CI matrix including Windows MSVC and macOS AppleClang (run 5, all
9 jobs green). Not verified: any UBT build; every engine-facing file is labelled
UNVERIFIED and has never been compiled in this repository.

### 00.02 Core primitives: ids, hash, random - VALIDATED

| Piece | Files | Tests (suite: count) |
|---|---|---|
| Fixed-width types (`long long` 64-bit aliases matching Unreal), export macro, endianness and IEEE-754 asserts, helpers | `CoreTypes.h` | CoreTypes: 1 (+ compile-time asserts); every suite indirectly |
| Versions | `Version.h`, `Version.cpp` | Version: 7 |
| Hashing (FNV-1a 64, `Mix64`, `HashCombine`, `_vhash`) | `Hash.h` | Hash: 15 |
| Random streams (xoshiro256**, SplitMix64, `Derive`/`Fork`, `Jump`, draws; zero-state sanitising; fp-contract pragmas) | `Random.h`, `Random.cpp` | Random: 29 |
| Persistent ids and allocator (never-reused serials, corrupt-state clamping, `GetTypeHash`) | `Ids.h`, `Ids.cpp` | Ids: 19 |

All headers carry `STATUS: VALIDATED (Phase 00)` with the note that integration and
long-duration tests are deferred to Phase 01. Tests include known answers against
independent reference implementations, frozen regression values for `HashCombine` and for
`Derive`/`Fork` seeds, determinism and state round trips, edge cases (full 64-bit ranges,
serial exhaustion, all-zero state, rounding at large magnitudes) and assertion paths.
Decisions: ADR-0003, ADR-0004, ADR-0007, ADR-0009.

### 00.03 Logging and assertions - VALIDATED

| Piece | Files | Tests (suite: count) |
|---|---|---|
| Categories (atomic thresholds), levels, literal-only macros, sinks (recursive lock, snapshot dispatch), compile-time floor | `Log.h`, `Log.cpp` | Log: 23 (8-thread serialisation and re-entrancy tests), LogFloor: 1 |
| `VAELEN_CHECK/CHECKF/VERIFY/ENSURE/UNREACHABLE`, pluggable handler installed as one unit, failure counter, stderr + log default handler, `std::abort` | `Assert.h`, `Assert.cpp` | Assert: 33 with assertions enabled, 23 with them disabled |

Decisions: ADR-0002, ADR-0005.

### 00.04 Test harness and purity - VALIDATED

- Harness `Tests/Harness/VaelenTest.h` (mathematically correct integer comparisons,
  negative self-test), runner `Tests/Harness/TestMain.cpp` (registry check: suite name
  must match the file, `--shuffle`, `--reverse`, zero-check warning), self-test
  `Tests/Core/Test_Harness.cpp` (Harness: 5). One CTest entry per `Test_<Suite>.cpp`
  plus `Core.Registry`, `Core.Shuffled`, `Core.Reversed`; 300 s timeouts.
- Purity checker `Tools/check_kernel_purity.py` (rules R0-R7 on headers and sources,
  exemptions, BOM/CRLF-safe, rejects symlinks, path-component module names and empty
  modules; `--self-test`: 36 checks, 0 failed), module list `Tools/kernel_modules.txt`,
  CTest entries `Kernel.Purity` (12 files, 0 violations) and `Kernel.PuritySelfTest`.

Decisions: ADR-0006, ADR-0008.

### 00.05 Foundation validation and docs - VALIDATED (headless)

Done:

- Adversarial review of the whole repository by eight independent lenses (kernel
  correctness, determinism/portability, Unreal integration, test quality, build/CI,
  documentation accuracy, master-prompt compliance, robustness/security): 189 findings,
  triaged and applied in commit `7d41751` (kernel: export macro owned by the kernel,
  Unreal-compatible 64-bit types, assertion policy from the target configuration,
  re-entrant logging, zero-state and corrupt-counter handling, fp-contract pragmas;
  harness, tests, build, CI, tools and the Unreal bridge as listed in that commit).
- `Docs/ARCHITECTURE.md`, `Docs/CONVENTIONS.md`, `Docs/DECISIONS.md` (ADR-0001 to
  ADR-0009), `Docs/STATUS.md`, `README.md` and this roadmap, refreshed against the final
  code.

Deliberately not applied (see `Docs/STATUS.md`, "Discrepancies"): `FPSemantics` in the
module rules, SHA-pinned GitHub Actions, trimming the `IdKind` placeholders.

Phase 00 against the exit criteria of section 2: (1) green on the whole CI matrix
(run 5: six Linux presets, clang-format, Windows MSVC, macOS); (2) met for Random, Ids, Hash, including
frozen derivation values; (3) met: no INCOMPLETE file exists, engine files are
UNVERIFIED; (4) unit, deterministic and edge-case present; integration and long-duration
deferred to Phase 01 and stated in every STATUS line; (5) docs present, closing report in
`Docs/STATUS.md`. Verdict: **Phase 00 VALIDATED on the headless side, UNVERIFIED on the
engine side until the first UE 5.6 build.**

## 5. Phase 01 - CORE SIMULATION: task breakdown and real status

Module `VaelenSim` (kernel) exists since 01.01; tasks 01.01-01.08 are VALIDATED
(headless). Directory
`Source/VaelenSim` with `Public/Vaelen/Sim/`, listed in `Tools/kernel_modules.txt`,
`/CMakeLists.txt`, `Vaelen.uproject` and both targets (`Docs/ARCHITECTURE.md` section 3.3,
rule 6). Tests in `Tests/Sim/Test_<Suite>.cpp` building `VaelenSimTests`. Each task ends
VALIDATED only with unit, deterministic and edge-case tests on both compilers; 01.07 and
01.08 supply the integration and long-duration categories for the whole phase. Every
design choice below that survives implementation gets an ADR (planned numbers 0010+).

### 01.01 Entity handles and registry - VALIDATED (headless)

- Delivered: `EntityHandle` (64-bit: 32-bit generation, 32-bit slot index; null is
  value 0, live generations start at 1) and `EntityRegistry` (`Create(PersistentId)`,
  `Create(IdAllocator&, IdKind)`, `Destroy`, `IsAlive`, `GetId`, `Find`, `ForEachAlive`
  in slot order, `GetState`/`SetState` with full consistency validation, `Clear`).
  Slots are recycled through a LIFO free list; a slot whose generation reaches
  `MaxGeneration` is retired. The `PersistentId` lookup is an `unordered_map` used for
  lookups only. Module `VaelenSim` with its own `VAELEN_SIM_API`, listed in
  `Tools/kernel_modules.txt`, `/CMakeLists.txt`, `Vaelen.uproject` and both targets.
- Tests (`Tests/Sim`): EntityHandle 3, EntityRegistry 13: dense allocation, LIFO reuse
  with generation bump, stale/null/out-of-range handles, slot-order iteration,
  determinism across instances, state round trip, rejection of inconsistent states,
  retirement at the last generation, one million create/destroy cycles, assertion paths.
- Decision: ADR-0010. Engine side (`VaelenSim.Build.cs`, `VaelenSimModule.cpp`)
  UNVERIFIED.

### 01.02 Component storage - VALIDATED (headless)

- Delivered: `ComponentTypeRegistry` (explicit, ordered registration of trivially
  copyable types under unique names; ids are registration indices, name hashes the
  stable identity; `LayoutDigest` for snapshot compatibility), `ComponentType<T>` (typed
  id, no RTTI), `ComponentPool<T>` (sparse set: dense data + dense full handles + sparse
  index by slot; `Add/Get/TryGet/Has/Remove`, swap-with-last removal, `ForEach`, validated
  `GetState`/`SetState`; stale generations never match and are replaced with a report),
  `ComponentStore` (one pool per created type, typed lookup, `RemoveAll` in id order).
- Tests (`Tests/Sim`): ComponentType 4, ComponentPool 8, ComponentStore 3: ordered ids,
  lookup and digest, add/get/remove, swap-remove consistency, stale handles, iteration,
  state round trip and rejection, determinism across instances, one million operations
  against a live registry, misuse paths.
- Decision: ADR-0011. Dense order is a function of the operation sequence, not slot
  order; systems needing a canonical order iterate the registry or sort by id.

### 01.03 Systems and tick scheduler - VALIDATED (headless)

- Delivered (`System.h`, `Scheduler.cpp`): `ISystem` (stable name, dependencies by
  name, `SimLod` 0-4, `Tick(TickContext&)`), `TickContext` (tick, clock, registry,
  component store, the system's own stream, event bus slot for 01.05), `LodSchedule`
  (periods 1, 4, 24, 720, 8640 ticks by default), `Scheduler` (`Add/Remove/Build/
  RunTick`): Kahn's algorithm with a name-hash tie-break, so the execution order is a
  pure function of the set of systems; unknown dependencies, cycles (self included),
  duplicate names and invalid LOD schedules are reported and refuse to run. Each system
  receives `WorldStream.Derive(name).Fork(tick)` every tick: its draws depend on the
  world seed, its own name and the tick only. `RunTick` advances the clock.
- Tests (`Tests/Sim/Test_Scheduler.cpp`, 8): order independent of registration order,
  hash-ordered independent systems, error reporting, LOD tick counts over two years,
  stream independence from other systems and equality with the documented derivation,
  dependency-ordered execution log, identical evolution of two worlds with the same seed
  (and divergence with another), misuse paths.
- Decision: ADR-0012.

### 01.04 Simulation clock and calendar - VALIDATED (headless)

- Delivered (`SimClock.h`, header-only, constexpr): `SimTick` (`uint64`),
  `CalendarRules` (data: ticks per hour, hours per day, days per month, months per year,
  months per season; defaults 1/24/30/12/3, i.e. a 360-day AELVOR year of four seasons),
  `Calendar::ToDate/ToTick` (pure inverses over the whole `uint64` range), `SimClock`
  (`Now`, `Advance` by exactly one tick, `Restore` for snapshots, `Date`).
- Tests (`Tests/Sim/Test_SimClock.cpp`, 4): boundaries (last tick of a year, first of
  the next, season edges), round trips on samples up to `MaxTick` and exhaustively over
  two years, custom rules and validation, clock advance/restore, constexpr evaluation.
- Decision: ADR-0013. Irregular calendars (leap days) remain a data decision for Phase
  02/03; `CalendarRules` is the extension point.

### 01.05 Event bus and event log - VALIDATED (headless)

- Delivered: `Event` (`Event.h`: 112-byte plain record with id of kind Event, tick,
  type hash, cause id, subject id and up to 64 payload bytes; every byte defined, so
  events hash and serialise as raw bytes; `EventType<T>` / `MakeEventType`), `EventLog`
  (`EventBus.h/.cpp`: append-only, running digest, byte image with count and digest,
  corruption rejected, `CountCausedBy`), `EventBus` (publish at a tick with subject and
  cause, next-tick delivery in publish order, listeners per type ordered by listener
  name hash, events published during dispatch deferred to the next tick, ids from the
  world allocator, every publication logged). `Scheduler::RunTick` dispatches the
  pending events before running the systems of a tick.
- Tests (`Tests/Sim`, 10): payload round trip and zero-filled bytes, hash coverage,
  append-only digest (order-sensitive), byte round trip and corruption detection,
  next-tick delivery order across ticks, type filtering and listener order independent
  of subscription order, deferral during dispatch with causal link, no subscribers /
  empty payload, scheduler integration with a birth -> death causal chain and equal
  digests for two identical worlds, misuse paths.
- Decision: ADR-0014.

### 01.06 Persistence interfaces and snapshot - VALIDATED (headless)

- Delivered: `World` (`World.h/.cpp`: owns every state block - id allocator, root
  random stream, clock, entity registry, component store, pending events, event log -
  and references the code that acts on it: type registrations, systems, listeners;
  `Build`, `Tick`, `TickMany`, `CreateEntity`, `DestroyEntity`), `IArchive` with
  `MemoryWriter` / `MemoryReader` (`Archive.h/.cpp`: one symmetric `Serialize` per type,
  no exceptions, a read past the end sets a sticky error and zero-fills, vector counts
  bounded), `SaveSnapshot` / `LoadSnapshot` / `ComputeStateDigest` (`Snapshot.h/.cpp`:
  header with magic, `VAELEN_SAVE_FORMAT_VERSION`, component layout digest and seed;
  clock, root stream, 256 id counters, entity slots written field by field, pools in
  type-id order with type id, name hash and element size, pending events, event log;
  FNV-1a trailer digest checked before any state changes; every rejection is an explicit
  `SnapshotResult`). The plain-data rule (`PlainData.h`): component and payload types
  must have a unique object representation (no padding), proven by the compiler for
  integer types and declared through `PlainDataTraits<T>` for floating-point members.
  `IComponentPool::Serialize` / `ElementSize` and `EventBus::GetPending` / `SetPending`
  expose the remaining state.
- Not in this task: the streams derived per system per tick are recomputed from the
  root seed and the tick (ADR-0012), so only the root stream state is saved; on-disk
  files, compression and migration between format versions come in Phase 16.
- Tests (`Tests/Sim`, 15): archive scalar/vector round trip, bounds error with
  zero-fill and sticky failure, count limit, empty writes; world build/tick/lifecycle,
  identical worlds give identical state digests (seed, entity count and tick each
  change it), tick before build reported; snapshot round trip of every block (including
  a destroyed entity and undelivered events), restored world continues exactly like the
  uninterrupted run (state digest, log digest and count after 300 ticks), byte-identical
  images for identical worlds, empty world, wrong version, bad magic, truncation before
  and inside the body, flipped bytes at five offsets, trailing bytes, different seed,
  different component layout, missing pool, 50 000-entity world.
- Decision: ADR-0015.

### 01.07 Deterministic replay test - VALIDATED (headless)

- Delivered: `Tests/Sim/Test_Replay.cpp`, the phase's deterministic and integration
  gate. A replay world with three systems at three LOD levels (Population every tick,
  Harvest daily with a famine path, Migration monthly founding and abandoning
  settlements, so entities are created and destroyed mid-run), a listener that turns
  every famine into a decree next tick (causal chain through the bus), and a recorded
  external input stream (found / raid / decree at given ticks, generated from a script
  seed). The reference run is 2000 ticks (83 days, so daily and monthly systems both
  fire many times).
- Tests (5): seed + inputs replayed in a fresh world give the identical event log
  (event by event), state digest and snapshot image; checkpoint at ticks 0, 1, 37, 720,
  721 and 1999, restore into a fresh world, run to 2000: state and log digests equal
  the uninterrupted run; eight chained generations of restore (every 250 ticks, each
  from the previous checkpoint) equal the uninterrupted run; any change (seed, tick
  count, one input value, one input tick, one dropped input) diverges in state and log;
  frozen reference values (state `dbb98f0004e8cd91`, log `2c1e775e47e45051`, 11 229
  events, 199 live entities) that every compiler and platform in CI must reproduce -
  clang 18 and gcc 13 do; Windows MSVC and macOS AppleClang are checked by the GitHub
  workflow on every push.
- Decision: the cross-compiler comparison is done with frozen constants inside the test
  rather than a CTest script diffing two binaries: it also covers MSVC and AppleClang,
  which never share a build directory with the Linux compilers, and a change of the
  simulation's observable behaviour becomes a deliberate edit of the constants.
- Rule confirmed by this task (ADR-0015 rule 6): systems hold no state of their own;
  the first version of the snapshot test kept a "born" list inside a system and could
  not continue identically after a restore.

### 01.08 Abstract mini-world end-to-end test - VALIDATED (headless)

- Delivered: `Tests/Sim/Test_MiniWorld.cpp`, the phase's long-duration gate. Four
  systems at four LOD levels (Demography every tick with capacity-driven deaths,
  Stockpile every 4 ticks, Omens monthly - random omen events, founding and abandoning
  villages, bounded by a land capacity of 40 villages -, Years yearly) and an Annals
  listener whose tallies live in a `Tally` component on a chronicle entity, so the
  listener's effects are world state. 100 000 ticks (11 years, 208 days) from seed
  `0x41454c564f52`.
- Tests (4): the long run holds every invariant at each of 100 checkpoints (village and
  stock pools in step, alive entities = villages + chronicle, no component on a dead
  entity, population ledger initial + births = alive + deaths, new-year tally = year
  boundaries passed, log grows monotonically with a moving digest, calendar date agrees
  with the tick, registry state re-validates) and logs the baseline; every 10 000 ticks a
  snapshot restored into a fresh world is byte-identical and, 700 ticks later, both
  worlds still share state and log digests (10 replays); the LOD systems fire exactly
  100 000 / 25 000 / 139 / 12 times and every NewYear sits on a year boundary; the frozen
  end state (state `0b6f6e9bd5887d35`, log `60cd10a389895804`, 305 027 events, 41
  entities) is reproduced by clang and gcc, and by MSVC and AppleClang in CI.
- Baseline (logged, not asserted): clang debug 255 k ticks/s, clang release 739 k
  ticks/s, gcc release without assertions 790 k ticks/s; snapshot of 34 MB (mostly the
  event log) in about 0.1 s. The event log is the dominant memory cost of long runs;
  Phase 16 decides on compaction and on-disk paging.
- Lesson: the first version had no land capacity and grew exponentially until the
  process was killed; every long-running system needs an explicit bound derived from
  state.
- Exit: VALIDATED; Phase 01 closed against section 2 (record in `Docs/STATUS.md`).

Phase 01 against the exit criteria of section 2: (1) green on the whole CI matrix for
01.06 and 01.07 (runs 13, 14), 01.08 checked by its own run; (2) determinism tests for
every system, snapshot round trips, frozen replay and mini-world values; (3) no
INCOMPLETE file, engine files UNVERIFIED; (4) unit, integration, deterministic, edge-case
and long-duration categories present; (5) ADR-0010 to ADR-0015, docs updated. Verdict:
**Phase 01 VALIDATED on the headless side, UNVERIFIED on the engine side until the first
UE 5.6 build.**

## 6. Phase 02 - WORLD: task breakdown and real status

Goal: the world of AELVOR derived from the seed alone - grid, terrain, climate,
hydrology, regions, resource deposits - as simulation state that the Phase 01 kernel
snapshots, replays and hashes like everything else. No content authoring, no rendering:
inspection happens through an ASCII map export and through numbers.

Decisions taken up front (each becomes an ADR when its task closes; the decision rule of
the master prompt applies: robust, then simple, then performant, then evolvable, then
deterministic, and every one of these was checked against determinism first):

- Integer arithmetic only in world generation. Noise, interpolation, slopes and flow use
  fixed-point (`int64`, Q32.32) and hash-based gradients; no `<cmath>`. Floating-point
  results are bit-stable inside one toolchain (ADR-0009) but `sin`, `exp` and friends
  differ between libm implementations, and the world must hash identically on clang,
  gcc, MSVC and AppleClang like the Phase 01 frozen values do.
- Tiles are not entities. A square grid of dense typed layers (`TileLayer<T>`, row-major,
  one value per tile) is a new state block of the `World`, serialised as its own snapshot
  section (`VAELEN_SAVE_FORMAT_VERSION` 1 -> 2). Regions, rivers, lakes and deposits are
  entities with components: there are thousands of them, not millions.
- Generation is a pipeline of stages, each a pure function of the seed, the
  `WorldGenConfig` and the previous stages, each with its own derived stream
  (`Root.Derive("elevation")`...) and its own frozen digest, so a change in one stage
  is localised to that stage and everything after it.
- Three reference sizes: 64x64 (unit tests), 256x256 (integration, frozen hashes),
  1024x1024 (the AELVOR default; long-duration and performance baseline).

### 02.01 Grid, tile layers, world-gen config and snapshot section - VALIDATED (headless)

- Delivered: `TileGrid.h` (`TileCoord`, `WorldGrid` up to 4096 x 4096 with row-major
  indexing, exact inverses, fixed neighbour order N, NE, E, SE, S, SW, W, NW clipped at
  the border, 4 or 8 neighbours; `TileLayer<T>` dense plain-data values, name-seeded
  digest, raw serialisation bounded by the maximum grid), `WorldMap.h/.cpp`
  (`WorldGenConfig` plain data with width, height, sea level and reserved stage
  parameters; layers declared by setup code under unique names and addressed by
  `TileLayerId<T>`, layout digest of the declared set, `Reset` adopting a config and
  zero-filling every layer, state digest, symmetric snapshot section), `World::Map()`,
  save format 2 (the header's layout digest now combines component and layer layouts).
  Misuse (duplicate name, invalid config, wrong element size, unknown id, out-of-range
  tile) is reported and answered with a scratch value, never a crash.
- Tests (10): index/coord inverses and bounds; neighbour order, clipping, 1x1 grid;
  layer reset, fill, hash, round trip, name-sensitive digest, truncation; misuse paths
  with capture counts; typed layer access and late-added layers; layout and state
  digests; snapshot section round trip through `World` with byte-identical re-save and
  layout mismatch on a different layer set; unset map round trip; a format-1 image
  refused with the target untouched; a 1024 x 1024 map (11 MB) round trip.
- Consequence: the frozen state digests of the replay and mini-world references were
  refrozen for format 2 (their log digests are unchanged, which proves the simulation
  itself did not move).
- Decision: ADR-0016.

### 02.02 Fixed-point math and deterministic noise - VALIDATED (headless)

- Delivered: `FixedPoint.h` (`Fix64`, Q32.32 in a signed 64-bit raw: constexpr
  FromInt/FromRatio/FromRaw, add/sub/neg wrapping on unsigned arithmetic (never
  undefined), Mul through a portable 64 x 64 -> 128 multiply in 32-bit halves rounding
  towards -inf, Div by 128-bit long division truncating towards zero with a saturating
  zero divisor, digit-by-digit Sqrt exact for perfect squares, Floor/Fraction/
  FloorToInt, Abs/Min/Max/Clamp/Lerp/SmoothStep, shifts, integer scaling, `_fx`
  literal), `Noise.h/.cpp` (SplitMix-style `LatticeHash(seed, x, y)`, `LatticeValue`
  in [-1, 1), `Value2D` bilinear with SmoothStep weights, `Gradient2D` with eight
  integer gradients and zero on the lattice, `Fractal2D` with per-octave derived seeds
  normalised to the base range, `Warped2D` domain warp from two derived seeds). No
  `<cmath>`, no floating point anywhere in these files.
- Tests (9): compile-time exactness of constants, ratios, zero divisors, square roots,
  smoothstep, lerp, floors and wrapping; Mul against a double reference at 1 ulp over
  200 000 pairs plus a full-precision product checked against Python big integers;
  Div at 2 ulp over 100 000 pairs plus exact fractions and sign rules; Sqrt as the
  floor of the exact root (100 000 random values, 2 000 perfect squares, sqrt 2 and
  sqrt of Max against exact references); helpers and wrap cases; value noise equal to
  lattice values on the lattice and inside the corner range between them; gradient
  noise zero on the lattice, bounded and spread; continuity along a line; fractal
  statistics over 65 536 samples (mean, standard deviation, range), seed and
  parameter sensitivity, warp bounded and identity at zero strength; frozen values for
  the hash, value, gradient, fractal and warped noise at a fixed point plus a 128 x 128
  field digest, reproduced by clang and gcc and checked on MSVC and AppleClang by CI.
- Lesson: at exact cell centres gradient noise takes quantised values that two seeds
  can share; sensitivity tests sample away from cell centres.
- Decision: ADR-0017.

### 02.03 Elevation and coastline - VALIDATED (headless)

- Delivered: `WorldGen.h/.cpp` - `WorldLayers::Declare` (elevation Fix64, terrain
  flags, slope), `ParamIndex` names into the 32-slot parameter block that replaced the
  reserved words of `WorldGenConfig` (save format 3), `ElevationParams::Resolve` with
  defaults where the config says zero, `GenerateElevation` (stage seeds derived from the
  world seed and the stage name; a warped continental mask at 3 lattice cells across the
  map plus a bias, an edge falloff that drops the mask to deep sea along the border, a
  6-octave relief and cubed ridge noise that rises only where the continent is solid),
  `ClassifyTerrain` (land above the sea level, coast and shore from the 4-neighbours,
  border, slope as the max |dz| over the 8 neighbours), `MeasureElevation` (land and
  sea tiles, coast tiles, border land, landmasses by 4-connected flood fill in scan
  order, extremes), `LayerDigest`, `ExportAscii` (downsampled 2:1 cells, six glyphs).
  A `Fix64::Div` fast path (dividend below 2^32) with the same result.
- Tests (6): the AELVOR seed at 256 has 39.4 % land, a largest landmass holding 98.4 %
  of the land, no land on the border, 1607 coast tiles, mountains above 1500 and sea
  below -1000, slopes below 1500, and passes every invariant (flags versus elevation,
  coast versus neighbours, border, slope recomputed); seeds change the world and the
  same seed repeats it; continent bias adds land, a higher sea level drowns land
  without touching the elevation layer; a 96 x 40 map; snapshot round trip and
  regeneration in the restored world; misuse before Reset; frozen digests at 64 and
  256 for elevation and terrain reproduced by clang and gcc (MSVC and AppleClang by
  CI); the 1024 x 1024 baseline (6.7 s debug, 0.74 s release).
- Lesson: the log line is capped at 2048 bytes; the 64 x 32 ASCII picture is logged
  in four slices.
- Decision: ADR-0018.

### 02.04 Climate and biomes - VALIDATED (headless)

- Delivered (in `WorldGen.h/.cpp`): four new layers (sea distance uint16, temperature,
  moisture, biome), `ClimateParams` with eight parameter slots, `LatitudeOfRow` (exact
  -1 / 0 / +1), `PrevailingWind` (easterlies under 1/3, westerlies to 2/3, polar
  easterlies), `SeasonalOffset` (spring, summer, autumn, winter; amplitude 4 + 16 |lat|),
  `ClassifyBiome` (Ocean; Alpine above 2500; Ice, Tundra, Boreal forest / Cold steppe,
  Temperate forest / Grassland / Scrubland, Tropical forest / Savanna / Desert by
  temperature and moisture thresholds), `GenerateClimate` (multi-source BFS sea
  distance in scan order; temperature = latitude band - lapse per 1000 units of
  altitude + local noise; moisture from a humidity parcel advected along the row by
  the prevailing wind that rains a base fraction per tile - one over the decay
  distance, expressed as a fraction of the map width so the model does not depend on
  the resolution - plus an orographic share of any climb, recovers over sea, blended
  with a rational sea-proximity term and local noise), `MeasureClimate`,
  `ExportBiomeAscii`.
- Tests (6): exact latitudes, wind bands and their edges, seasonal offsets, every
  biome reachable through the table, names and glyphs; the AELVOR map at 256 (sea
  tiles are Ocean with distance 0, land never Ocean, moisture in [0, 1], temperature
  never above its band plus the noise amplitude, Alpine iff above 2500, equator rows
  20 degrees warmer than polar rows, at least 7 distinct land biomes - 10 measured -,
  sea distance and moisture in plausible bands, the biome map logged in slices); a
  synthetic 64 x 9 ridge map proving the rain shadow (ridge wetter than windward,
  leeward and far leeward drier, never zero, decay with distance, exact sea distances);
  seed and warm-parameter sensitivity with the elevation layer untouched and misuse
  before Reset; snapshot round trip of all seven layers; frozen digests at 256 for
  temperature, moisture and biome reproduced by clang and gcc (MSVC and AppleClang by
  CI).
- Lesson: the first moisture model lost a fixed 1/12 per tile and turned the interior
  into desert (mean land moisture 0.15); a decay defined per fraction of the map width
  plus a sea-proximity share gives 0.45 and every biome family at 256.
- Decision: ADR-0019.

### 02.05 Hydrology - VALIDATED (headless)

- Delivered: `Hydrology.h/.cpp` - `WorldTypes::Declare` (River and Lake component
  types and pools), `HydroLayers::Declare` (filled elevation, flow direction,
  accumulation, river index, lake index), `RiverInfo` / `LakeInfo` plain-data
  components, `IdKind::Lake`, `GenerateHydrology` (priority flood + epsilon from every
  sea tile with ties broken by index; D8 steepest descent on the filled surface with
  the diagonal drop scaled by 181/256 and ties kept in the fixed neighbour order;
  accumulation in decreasing filled order; basins as 4-connected raised components
  where a basin shallower than `LakeMinDepth` or smaller than `LakeMinTiles` is filled
  with sediment - its elevation rises to the water surface and becomes a plain - and a
  deeper one becomes a lake entity with surface, tiles and outlet; `ClassifyTerrain`
  rerun; rivers as tiles above `RiverThreshold` outside lakes, traced from their
  sources in scan order to the sea, a lake or an existing river, dropped below
  `MinRiverLength`, one entity each), `MeasureHydrology`, `StepsToSea`,
  `ExportHydroAscii`. The stage runs after elevation and before climate.
- Tests (5): a synthetic cone with a carved pit (one lake of nine tiles with an outlet,
  every pit tile reaches the sea, the summit has accumulation 1, a rerun replaces the
  entities); the AELVOR map at 256 (every land tile drains to the sea - all of them,
  not a sample -, flow descends, accumulation grows downstream, the total flow into
  the sea equals the land count, lake tiles are raised and never river tiles, entities
  agree with the index layers, 40 rivers with the longest at 46 tiles, 22 lakes under
  4 % of the map, the river map logged); determinism across worlds including the
  entities, snapshot round trip of twelve layers, a lower threshold gives more river
  tiles, misuse before Reset; frozen digests for flow, accumulation and river index
  plus the river and lake counts; the 1024 x 1024 baseline (1.5 s debug, 0.36 s
  release).
- Lesson: pure filling turned every fractal pit into a lake (375 lakes, rivers cut to
  ten tiles); the basin-depth rule keeps only real lakes and lets rivers cross the
  filled plains.
- Decision: ADR-0020.

### 02.06 Regions - VALIDATED (headless)

- Delivered: `Regions.h/.cpp` - `RegionInfo` component (tiles, seed, centroid, coast,
  river and lake tiles, dominant biome, mean elevation), `RegionTypes` / `RegionLayers`
  declared by setup code, four parameter slots (seed spacing as a fraction of the
  width, size floor, slope and river costs), `GenerateRegions` (seeds on a jittered
  lattice - the land tile nearest to the jittered cell centre, scan order on ties -
  plus one seed per landmass left without; multi-source least-cost growth over the 4
  neighbours with cost 1 + slope cost per 1000 units of climb + river cost, ties by
  index, so ridges and rivers become borders; regions below the floor merge into the
  neighbour with the longest shared border, smallest first, islands below the floor
  kept; indices compacted in seed order; one entity per region), `BuildRegionGraph`
  (derived, sorted neighbour lists with shared 4-edge counts, never stored),
  `MeasureRegions`, `ExportRegionAscii`.
- Tests (5): the AELVOR map at 256 (exact cover of the land, every region contiguous
  from its seed, seed and centroid inside, dominant biome never Ocean, regions below
  the floor have no neighbours, adjacency and shared borders symmetric, no self
  adjacency, 126 regions with at most 8 neighbours, the region map logged); a
  two-island synthetic map where no region straddles the channel and the graph has no
  edge between islands; determinism across worlds including the entities, snapshot
  round trip of thirteen layers, wider spacing gives fewer regions, a rerun replaces
  the entities, misuse before Reset; frozen digest and count at 256; the 1024 x 1024
  baseline (2.2 s debug, 0.35 s release).
- Decision: ADR-0021.

### 02.07 Resource deposits - VALIDATED (headless)

- Delivered: `Deposits.h/.cpp` - `ResourceKind` (Stone, Timber, Clay, FertileSoil, Salt,
  IronOre, CopperOre, Gold) with names, `DepositInfo` component (tile, kind, tier 1-3,
  richness 1-1000, region), `DepositTypes` / `DepositLayers`, two parameter slots
  (density, spacing), `DepositSuitability` (public, pure: timber on forests, clay on
  wet lowland, fertile soil on arable lowland doubled by water, salt on hot dry coasts
  or desert flats, stone on slopes and uplands away from rivers, iron and copper by
  altitude with biome factors, gold above 1400 units and richer on mountain rivers),
  `GenerateDeposits` (per tile and kind a hashed draw against base chance times
  suitability times density; one deposit per kind per spacing cell keeps the best
  draw; materialised in tile order with one deposit per tile; richness seven tenths
  from suitability; the base tier of the kind raised for the richest draws; one entity
  per deposit tagged with its region), `MeasureDeposits`.
- Tests (5): every rule of the suitability table checked explicitly; the AELVOR map at
  256 (every deposit on land where its rule allows it, layer and entities agree, fields
  in range, region matches, every kind present, 1 % to 12.5 % of the land, tiers
  strictly rarer upwards, gold rarer than stone, most regions hold something - 1364
  deposits, 99 of 126 regions); determinism across worlds, snapshot round trip of
  fourteen layers, lower density and wider spacing give fewer deposits, rerun replaces
  the entities, misuse; frozen digest and count at 256; the 1024 x 1024 baseline
  (0.6 s debug, 0.3 s release, 18 523 deposits).
- Lesson: the first richness formula saturated at 1000 and promoted every deposit a
  tier; the first slope limits left plains without clay or soil. Both are now pinned
  by the frozen counts.
- Decision: ADR-0022.

### 02.08 World-gen determinism and long-duration gate; Phase 02 close - VALIDATED (headless)

- Delivered: `WorldGenPipeline.h/.cpp` - `WorldSetup::Declare` (every Phase 02 layer
  and component type in a fixed order), `WorldGenStage`, `GenerateWorld` (Reset,
  elevation, hydrology, climate, regions, deposits, stoppable after any stage, invalid
  config or stage refused), `ReportWorld` (every stage's statistics and the entity
  count).
- Tests (4): two fresh worlds give byte-identical snapshot images and a restored world
  re-saves the same bytes and reports the same numbers, regeneration in the same world
  reproduces the layers, another seed differs; partial runs match the full run's
  elevation from hydrology onwards and leave later layers empty, a 160 x 48 map, a
  drowned world (sea level above every peak: every stage succeeds with zero entities),
  the 1 x 1 map, invalid config and stage refused; the Phase 01 kernel over a generated
  world (ticks leave the map digest unchanged, a mid-run snapshot continues
  identically); the whole-world digests frozen at 64, 256 and 1024 with the full
  baseline logged (0.1 s, 1.5 s and 25 s in debug).
- Decision: ADR-0023.

Phase 02 against the exit criteria of section 2: (1) CI matrix green for every task
(runs 18 to 24), 02.08 by its own run; (2) frozen digests per stage and for the whole
world, byte-identical regeneration, snapshot re-hash; (3) no INCOMPLETE file, engine
files UNVERIFIED; (4) unit, integration, deterministic, edge-case and long-duration
categories present; (5) ADR-0016 to ADR-0023, docs updated. Verdict: **Phase 02
VALIDATED on the headless side, UNVERIFIED on the engine side until the first UE 5.6
build.**

## 7. Phase 03 - HISTORY: task breakdown and real status

Goal: the simulated pre-history everything later inherits - eras, cultures, languages,
religions, migrations and the historical record - produced by the Phase 01 kernel
ticking over the Phase 02 world at LOD 4 (yearly and monthly systems), so that a new
game starts on a world with centuries behind it and every fact of that past is an
event in the log with a cause.

Decisions taken up front (each becomes an ADR when its task closes):

- History is simulated, not authored: cultures, languages and religions are entities
  created by systems from regions and events; nothing is named or placed by hand.
  Names come from a deterministic phonology per language (03.03), never from lists.
- Coarse population per region (integer counts by culture), not persons: persons and
  families are Phase 04. Migration, growth and collapse move counts between regions
  along the region graph.
- The historical record is the event log plus a chronicle of `Record` entities that
  summarise events per era and region; both are queryable by cause chain ("why").
- Frozen digests per era of the reference history at 256 so any change to a system is
  deliberate.

### 03.01 Eras, the era calendar and the historical record - VALIDATED (headless)

- Delivered: `History.h/.cpp` - `EraInfo` (index, trigger founding / span / requested,
  start, end, cause event), `RecordInfo` (event, tick, type, subject, era, region),
  `HistoryState` singleton component (pending request and its cause, open era, counts),
  `HistoryTypes::Declare`, `InitializeHistory` (creates the history entity once, on a
  fresh world only), `EraSystem` (LOD World: founds the first era, closes the open one at
  its span or when a request is pending and opens the next, publishing EraClosed and
  EraOpened with the era as subject and the request's event as cause),
  `EraSystem::RequestEra` (first cause wins until the yearly tick), `Chronicle`
  listener (one Record entity per event of every subscribed type, era at the event's
  tick, region when the subject is a region entity), queries `EraAt`, `FindEvent`
  (binary search on monotonic ids), `CauseChain` (root-cause walk, guarded against
  links that cannot precede their effect), `EventsInEra`, `EventsAbout`;
  `IdKind::Era`. Era and record ids use kinds Era and Document.
- Tests (3): 260 simulated years over a generated 32 x 32 world with an omen system
  and a collapse listener (eras contiguous from tick 0 without gap or overlap, exactly
  one open, the founding era first, 52 requested eras each with a resolving two-link
  cause chain collapse <- omen, the first requested era opening at the yearly tick after
  the request; span eras exactly every 30 years without a cause in a world without
  collapses); the chronicle (one record per chronicled event, fields equal to the
  event, era equal to `EraAt`, regions only on collapses, the state's record count,
  era and subject queries, unknown ids); determinism across two worlds, a pending
  request surviving a snapshot taken three ticks before the yearly tick and the
  restored world continuing identically for twenty years, double initialisation
  refused, ticking without history reported and harmless.
- Decision: ADR-0024.

### 03.02 Cultures and coarse population - VALIDATED (headless)

- Delivered: `Population.h/.cpp` - `CultureInfo` (index, home region, parent, generation,
  founding tick, identity hash), `RegionPopulation` (six culture slots with counts, total,
  capacity, majority, years settled; exact bookkeeping helpers), `PopulationTypes::Declare`,
  `PopulationRules` (public rule table: seeds, capacity per biome tile, per river tile and
  per fertile deposit, growth and decline per mille, migration threshold, share and
  minimum wave, assimilation share, split distance and years), `SeedCultures` (the
  highest-capacity mutually non-adjacent regions, one CultureFounded and one caused
  RegionSettled each), `PopulationSystem` (LOD World, yearly: logistic growth bounded by
  capacity, decline above it, assimilation of minorities below the share, abandonment,
  splits), `MigrationSystem` (LOD Statistic, monthly: a majority above the crowding
  threshold sends a share to the least crowded neighbour along the region graph, decided
  on start-of-tick state in region order; RegionSettled caused by the wave when the
  destination was empty; people stay when no slot is free), events CultureFounded,
  CultureSplit, RegionSettled, RegionAbandoned, MigrationWave, `MeasurePopulation`,
  `ExportCultureAscii`.
- Split rule: a region settled for `SplitYears` whose majority's home lies at least
  `SplitDistance` away in the region graph splits together with its connected far
  component of the same culture; the block joins the nearest sibling culture (same
  parent) whose home is closer than `SplitDistance`, otherwise founds a culture whose
  home is the block's lowest region. Culture homes of one lineage therefore stay at
  least `SplitDistance` apart, which bounds the number of cultures by the graph.
- Tests (5): exact bookkeeping (slots, totals, majority, overflow refused); seeding on
  AELVOR 128 (four non-adjacent homes, events, second seeding refused); 500 years at 128
  with invariants each century (counts consistent, people within capacity plus a wave,
  no collapse without a cause), the continent fills up, splits happened and equal
  cultures minus seeds, more than a hundred waves, migration alone conserves people,
  ASCII map by culture; two worlds identical and a snapshot at year 60 continuing
  identically, a drowned world stays empty; frozen 500 years at 128: state
  `f2afaa068c0f717d`, 47 587 people, 18 cultures.
- Decision: ADR-0025.

### 03.03 Languages and naming - VALIDATED (headless)

- Delivered: `Naming.h/.cpp` - `Phonology` (bit inventories over fixed onset, coda and
  vowel tables, syllable-shape weights, syllable range; 20 bytes), `LanguageInfo`
  (culture, parent language, generation, founding tick, identity, sounds, names given),
  `NameText` (24-byte NUL-terminated), `NameScope` (culture, language, region, river,
  lake, era, person), `NameInfo` component on the named entity (language, scope, key,
  salt, generation, text), `LanguageTypes::Declare`, `LanguageRules` (drift span, salt
  budget, river / lake / era toggles), pure functions `DerivePhonology`,
  `MutatePhonology`, `NormalisePhonology`, `IsNormalised`, `GenerateName`,
  `IsPronounceable`, `NameLength`, `NameEquals`; `LanguageSystem` (LOD World, after
  Population: founds a language per culture, a child of the parent culture's language
  after a split, drifts one sound per `DriftTicks`, then names nameless cultures,
  languages, settled regions, rivers and lakes whose source region is settled, and eras
  in the language of the largest culture; every name unique in its scope by salt
  retry); events LanguageFounded, LanguageDrifted, Named (subject = the named entity);
  queries `NameOf`, `IsNameUsed`, `MeasureNames`, `ExportNames`.
- Name construction: syllables are onset + vowel + coda drawn by a hash stream over
  (phonology, scope, key, salt); never two vowels across a boundary, a single
  consonant only after a single coda and never the same letter twice, a vowel after a
  cluster coda, no syllable repeating its predecessor, a two-syllable floor for
  un-suffixed scopes, a language-specific river / lake suffix chosen by the stem's
  last letter, stems capped at 12 letters. `IsPronounceable` (letters only, capital
  then lower case, 2-23 letters, at most three consonants or two vowels in a row) is
  asserted on every generated name.
- Tests (5): phonologies derived, normalised, deterministic and drifting by at most
  two bits or one weight, an empty phonology repaired, component sizes; 14 336 names
  over 16 phonologies and every scope all pronounceable, 3-16 letters, at least 113 of
  128 distinct per scope, salt changes 14 334 of them, two languages agree on at most 2
  of 64 keys, the invariant rejects hand-written counter-examples and hidden bytes; 500
  years on AELVOR 128 (one language per culture, parents follow splits, root
  generations 3, drift / founded / named events equal to the state, every settled
  region named with its index as key, eras named, no duplicates, sample names logged);
  two worlds identical, names surviving a snapshot byte for byte and the restored world
  continuing identically, a different seed naming differently, rules honoured (no
  drift, no river / lake / era names), a drowned world nameless; frozen 500 years at
  128: state `871cdd11bea18906`, 154 names, culture 1 "Oldegedim", region 1 "Thuthanyo".
- Decision: ADR-0026.

### 03.04 Religions - VALIDATED (headless)

- Delivered: `Religion.h/.cpp` - `Tenets` (eight axes 0-255: authority, nature,
  ancestors, war, trade, mystery, purity, tolerance), `ReligionInfo` (culture, parent,
  generation, home region, founding kind, founding tick, founding event id - never 0,
  identity, creed; 56 bytes), `RegionFaith` component on regions (four faith slots with
  believers, majority; exact bookkeeping, saturating add, zero adds take no slot),
  `FaithState` singleton (religion count, up to eight pending founding requests,
  requested and refused counters), `ReligionTypes::Declare`, `InitializeFaith` (once, on
  a fresh world), `ReligionRules` (era foundings, schisms on splits with a hashed chance,
  founding share, yearly conversion inside a region and spread to neighbours, fade share),
  `DeriveTenets`, `SchismTenets` (one or two axes moved visibly), `ReligionSystem` (LOD
  World, after Population: clamps believers to the people of each region, founds the
  pending requests in order - a schism when the region's majority faith is the parent -
  with `FoundingSharePerMille` of the region converted at once, spreads every majority
  faith inside its region and into its neighbours' unconverted, capped by the live room,
  publishes RegionConverted when a majority changes, names religions in the founding
  culture's language when `NameWith` is set), `ReligionSystem::RequestFounding(region,
  cause, kind)` for later phases (first request per region wins, null causes refused),
  `FaithListener` (MigrationWave carries believers with the wave in the source's
  proportions, never beyond the destination's people; CultureSplit requests a schism
  where a faith is held; EraOpened requests a founding in the largest culture's home
  when it has no faith), events ReligionFounded and Schism (subject the religion, cause
  the founding event), RegionConverted; `MeasureFaith`, `ExportFaithAscii`;
  `NameScope::Religion`.
- Rule: no religion without a founding event. Believers never exceed a region's people
  once the waves of the last tick are delivered (the bus dispatches at the start of the
  next tick).
- Tests (5): exact bookkeeping (ties to the lowest index, four slots, saturation,
  removals clamped), tenets derived deterministically and schisms moving one or two axes,
  component sizes; 500 years on AELVOR 128 with the invariants each century (believers
  bounded and consistent), at least three religions and one schism, most people and
  regions converted, every religion's founding event found in the log before its
  founding and of the right kind (era opening or culture split at the same region), every
  founding event caused by that event and matching the entity, conversions beyond the
  founding regions, names in the founding culture's language, faith map logged; spread
  along the graph proven by running the yearly step by hand for 40 years (a region gains
  a faith only next to a region where it was the majority; believers only where people
  live), the request queue (duplicates, null cause, overflow to eight, unsettled region
  refused at the yearly tick, no faith state), double initialisation refused; two worlds
  identical, a pending request surviving a snapshot and the restored world continuing
  identically, silent rules leaving the world faithless, zeal converting more; frozen 500
  years at 128: state `169e51de300cea9f`, 10 religions, 45 682 believers.
- Fixed after CI run 29 (Windows MSVC): `FaithListener` re-resolves the source region's
  faith after `FaithOf` may have moved the pool (a dangling pointer made the migration
  carry depend on the STL's vector growth factor: 2x on libstdc++, 1.5x on MSVC, so
  only MSVC reallocated mid-carry). The Linux digest is unchanged; CI run 30 checks MSVC.
- Decision: ADR-0027.

### 03.05 Disasters and omens - VALIDATED (headless)

- Delivered: `Disasters.h/.cpp` - `DisasterKind` (drought, flood, eruption, plague),
  `DisasterInfo` record component (kind, region, severity 1-3, tick, omen event id - never
  0, deaths, people before; 40 bytes), `RegionHazard` (derived: tiles, river tiles,
  mountain tiles, mean moisture, risk 0-1000 per kind), `DisasterState` singleton
  (counters, up to 32 pending omens), `DisasterTypes::Declare`, `InitializeDisasters`,
  `DisasterRules` (omen chance per kind at full risk, strike chance, deaths per mille by
  kind and severity, drought moisture line, river and mountain shares for full risk,
  mountain elevation, plague density, faith shaken share, founding and era severities,
  era deaths), `ComputeHazards` (one pass over the tiles: drought where the mean moisture
  is below the line, flood by river share, eruption by the share of tiles at least
  1 200 m above the sea), `PlagueRisk` (people per tile), `DisasterSystem` (LOD World,
  after Population, system stream: last year's omens strike with `StrikePerMille`,
  severity escalating with the risk, deaths per culture in proportion, a record entity,
  a DisasterStruck event caused by the omen; the majority faith loses a share of its
  believers, a severe disaster requests a faith founding where none is held and a new
  era when deadly enough; then this year's omens region by region and kind by kind as
  Omen events about the region, queued for next year), `MeasureDisasters`.
- Rule: every disaster has a place (its region) and a cause (its omen), and the
  physical kinds strike only where the world allows them.
- Tests (5): hazards derived from the world (each risk only where its cause exists, 29
  / 40 / 34 of 99 AELVOR regions at drought / flood / eruption risk, deterministic,
  plague risk by density, rules moving the lines, a drowned world without hazards);
  500 years on AELVOR 128 with per-century logs (250 disasters, every kind between 2 and
  300, both mild and severe, omens at least as many as disasters, no omen dropped, 56
  regions struck, the world surviving), every record and every DisasterStruck event
  traced to an Omen event about the same region and kind that precedes it, deaths within
  the people found, faiths founded and eras opened by disasters; rules (no omens, omens
  that never strike, a cursed world with more disasters, deaths and dropped omens, zero
  deaths per mille, double initialisation refused); two worlds identical, pending omens
  surviving a snapshot and the restored world continuing identically, another seed
  striking differently; frozen 500 years at 128: state `073bf8b246734ad8`, 250
  disasters, 4 691 deaths.
- Decision: ADR-0028.

### 03.06 Pre-history run and the starting state - VALIDATED (headless)

- Delivered: `PreHistory.h/.cpp` - `PreHistoryRules` (population, era, language,
  religion and disaster rules plus the default length, 500 years), `PreHistoryTypes`
  (every Phase 02 and 03 type set), the `PreHistory` object (constructed before
  `World::Build`: declares the types, owns and adds the six systems - Population,
  Migration, Eras, Languages, Religions, Disasters - wires the faith listener, era and
  religion naming, faith shaking and era requests, and a chronicle of era, culture,
  settlement, language, religion and disaster events), `Generate(config, years)` (fresh
  world only: generation, first cultures, history / faith / disaster state, then the
  run; refused without change when the world has history or its clock moved, when
  generation fails or when nobody can be seeded), `Run(years)`, `HasHistory`,
  `ReportPreHistory` (population, names, faiths, disasters, eras, records, events,
  entities, state and log digests), `ExportPreHistoryText` (deterministic lines),
  `TicksPerYear`.
- Tests (5): one call on AELVOR 128 (every measure populated, the six systems scheduled,
  the text deterministic and complete, a second generation refused without change, an
  invalid config and a drowned world refused without history); the AELVOR 256 reference
  run frozen per century - state `8142f69ae490df39` (100), `3ed2555f9853634f` (200), `7c3534c0220f2be8`
  (300), `48cbf2c16ab18460` (400), `445cf9df1b63ba34` (500), log `e494dd9db829f5ef` - equal whether run in one
  call or century by century, centuries distinct; a snapshot seven ticks into year 250
  restored into a fresh object (history detected, generation refused) and continued 250
  years identically in state, log and report text, agreeing with an uninterrupted run;
  rules flowing to every system (a quiet rule set gives no disaster and no faith),
  seeds and a non-square world; the 1024 baseline (500 years, timed and logged, sanity
  bounds).
- Performance, found by the 1024 baseline (123 s in debug at first): `PopulationSystem`
  rebuilt the region graph (a full-map pass) every year, and `MigrationSystem`,
  `ReligionSystem` and `DisasterSystem` hashed the whole region layer every tick to key
  their derived caches. The graph is now cached in `PopulationSystem` and the three caches
  are keyed by a hash of the generation config, which the region layer is a pure function
  of. Every frozen digest is unchanged; 256 x 500 years dropped from 9.3 s to 2.6 s and
  1024 x 500 years from 123 s to 29 s (clang debug).
- Decision: ADR-0029.

### 03.07 Queryable history - VALIDATED (headless)

- Delivered: `HistoryText.h/.cpp` - `WhyStep` (event, era, region), `NameEntity` /
  `NameRegion` (the name given in 03.03 or a deterministic fallback such as "region 12",
  "culture 3", "river 4", "entity 0"), `OriginOf` (the earliest chronicled event whose
  subject is the entity), `Why` (from an event id, or from an entity through its origin,
  the cause chain to the root with the era and region of every step), `RegionTimeline`
  (every record about a region in tick then id order), `DescribeEvent` (one line per
  event: "Year 25, age of Divik: a terrible flood struck Vushu." with type-specific
  sentences for eras, cultures, settlements, migrations, languages, faiths, schisms,
  conversions, omens, disasters and names, and a generic line for unknown types),
  `DescribeRecord`, `ExportChronicle` (all records in order, capped when asked),
  `ExportRegionChronicle`, `ExportWhy` ("because ..." lines to the root cause),
  `CheckChronicle` (records resolved, era-consistent, placed, described).
- Tests (5): every record of a 300-year run resolves to its event, agrees with `EraAt`,
  is described by a specific line, every line starts with its year, ends with a full
  stop and follows tick order, the head is capped; why-chains from every disaster (to its
  omen) and every schism (to its split), from every religion entity (origin = founding,
  root without a cause), explanation text, unknown ids explain nothing; region timelines
  partition the placed records and are ordered, the longest story logged, region 0 and
  unknown regions empty; name fallbacks deterministic (regions, entities, rivers with
  an unsettled source), cultures named as on the entity, unknown event types generic;
  the chronicle text identical across two worlds and a restored snapshot, frozen at
  `c0a39beace60c36b` (179 lines) for AELVOR 128 after 300 years, another seed telling another story.
- Decision: ADR-0030.

### 03.08 Phase 03 gate; Phase 03 close - VALIDATED (headless)

- Delivered: `Tests/Sim/Test_HistoryGate.cpp` - 2000 years on AELVOR 256 through
  `PreHistory`, every Phase 03 invariant checked every decade (population bookkeeping
  exact and within capacity, believers never above the living except where a wave of the
  last tick is still travelling, names unique per scope, one language per culture, every
  religion and every disaster traced to an event in the log, eras contiguous from tick 0
  with exactly one open), the world alive and layered after two millennia (people near
  capacity, cultures, faiths, disasters, eras, records), the chronicle still resolving
  completely; digests frozen at 1000, 1500 and 2000 years and the log at 2000; a
  snapshot at year 1000 (32 MB) restored into a fresh object, continued 1000 years to the
  same digests and re-saved byte for byte.
- Two rules tightened by the gate (both refreeze earlier digests): `FaithListener`
  rounds the carried believers up and never puts them back, so a source region's
  believers never exceed its people between yearly clamps; `DisasterSystem` takes the
  dead from the believers before shaking the faith. Religion 128: `169e51de300cea9f` (11
  religions, 45 682 believers); disasters 128: `073bf8b246734ad8`; pre-history 256 from
  year 300: `7c3534c0220f2be8`, `48cbf2c16ab18460`, `445cf9df1b63ba34`, log `e494dd9db829f5ef`. The chronicle text digest is
  unchanged.
- Decision: ADR-0031.

Phase 03 against the exit criteria of section 2: (1) CI matrix green for every task
(runs 26 to 31; run 29 red on MSVC and fixed in 03.05), 03.08 by its own run; (2)
determinism tests for every system (two worlds, snapshot mid-run, frozen digests per
task and per century of the reference run, on four compilers through CI); (3) no
INCOMPLETE file, engine files UNVERIFIED; (4) unit, integration, deterministic,
edge-case and long-duration categories present (2000 years at 256, 500 years at 1024);
(5) ADR-0024 to ADR-0031, docs updated. Verdict: **Phase 03 VALIDATED on the headless
side, UNVERIFIED on the engine side until the first UE 5.6 build.**

## 8. Phase 04 - POPULATION: task breakdown and real status

Goal: persons and families over the pre-history - birth, ageing, death, lineage, needs
and demographics - simulated by the Phase 01 kernel at LOD 0-2 where the player is and
kept as the coarse counts of Phase 03 everywhere else, so that a region can be
promoted to persons and demoted back to counts without changing what the world knows.

Decisions taken up front (each becomes an ADR when its task closes):

- Two grains of population, one truth: the region's `RegionPopulation` counts (Phase
  03) stay the aggregate; persons exist only in regions at detailed LOD and their
  numbers by culture and faith always sum to the counts. Promotion materialises persons
  from the counts deterministically (seed, region, tick); demotion folds them back.
- Persons are plain-data components on entities of kind `Person`: birth tick, sex,
  culture, language, religion, region, family, parents, state (alive, dead at tick),
  needs and traits as small integers. Names come from `NameScope::Person` (03.03).
- Life cycles are yearly at LOD 2 and monthly at LOD 0-1: ageing by band, mortality and
  fertility as per-mille tables per age band and culture, marriages inside the region
  by rules of culture and faith, inheritance of culture, language and faith from the
  parents. Every birth, death and marriage is an event with a cause where one exists
  (a disaster, a famine).
- Needs (food, health, rest) are integers that decay and refill from the region's
  capacity and events; starvation and disease deaths are caused by the Phase 03
  disasters, so persons and history stay one story.
- The chronicle records persons only when they matter (founders, victims of a
  catastrophe, heads of families); `DescribeEvent` (03.07) gains the person cases.

| Task | Content | Tests |
|---|---|---|
| 04.01 | `VaelenPopulation` module (UBT + CMake), `PersonInfo`, `PersonTypes`, deterministic materialisation of a region from its counts, demotion back, conservation | unit, deterministic, edge (empty region, six cultures), snapshot |
| 04.02 | Ageing, mortality and fertility tables, births to couples, deaths with causes, reconciliation with the region counts every year | integration over 200 years in one detailed region, bands, frozen |
| 04.03 | Families and lineage: `FamilyInfo`, parents and children, marriages by culture and faith, genealogy queries (ancestors, descendants, siblings) | unit, deterministic, edge (orphans, extinct families) |
| 04.04 | Needs and body: food, health and rest, decay and refill, famine from a drought, disease from a plague, deaths caused by the disaster's event | integration with 03.05, frozen |
| 04.05 | Traits and skills from identity and upbringing, person names, inheritance of language and faith at birth | unit, distributions in bands, frozen |
| 04.06 | LOD bridge: promotion and demotion of regions, the coarse counts as the aggregate, digests conserved across a promote / demote cycle | deterministic, long-duration (500 years alternating) |
| 04.07 | Persons in history: births, deaths and marriages that matter in the chronicle, why-queries and text lines for persons | integration with 03.07, text deterministic |
| 04.08 | Phase 04 gate: 500 years with one detailed region over the 256 pre-history, invariants every decade, frozen digests on four compilers; Phase 04 closed against section 2 | long-duration |

Every task ends with the usual report block, the docs refreshed and a commit.

### 04.01 Persons and the two grains of population - VALIDATED (headless)

- Delivered: the `VaelenPopulation` module (UBT Runtime module `PreDefault` depending on
  `VaelenCore` and `VaelenSim`; CMake static library `Vaelen::Population`; listed in
  `Tools/kernel_modules.txt`, `Vaelen.uproject` and both targets; `PopulationApi.h`
  export macro; `VaelenPopulationModule.cpp` the only Unreal-facing file) and
  `Persons.h/.cpp` - `PersonInfo` (64 bytes: index, region, culture, religion, language,
  family, mother, father, birth and death ticks, identity, sex, state), `RegionDetail`
  (on a region while it is detailed), `PersonTypes::Declare`, `MaterialiseRules` (cap
  per region, female share, oldest age, young share), `PromoteRegion` (one Person
  entity per counted person, culture by culture, faith handed out in slot order, sex and
  age from a hash stream of the world seed, the region and the tick; refused without
  change when unknown, already detailed, unsettled or above the cap), `DemoteRegion`
  (counts and believers become what the living persons say, every person of the region
  destroyed), `IsDetailed`, `CountPersons`, `IsConsistent`, `MeasureDetail` (persons,
  alive, dead, inconsistent regions, a persons digest).
- Tests (5, `Tests/Population/Test_Persons.cpp`, CTest `Population.*`): promotion of the
  busiest region of AELVOR 128 after 300 years materialises the counts exactly (persons,
  cultures, faiths, near-half sexes, a young pyramid, unique indices and identities, ids
  of kind Person, ages within the rule), refusals (again, region 0, unknown, unsettled,
  above the cap); demotion after seven deaths and a conversion by hand folds the living
  back, destroys the persons and leaves the entity count as before, a clean round trip
  changes nothing; every settled region promoted at once gives exactly the world's
  people and believers and demotes back to the same world; two worlds materialise the
  same persons, a snapshot keeps them byte for byte and continues identically, the tick
  is part of the draw, rules matter; frozen persons digest `a92da70b85f09c0f` (19 781
  persons).
- Decision: ADR-0032.

### 04.02 Ageing, mortality, fertility and the reconciliation of the grains - VALIDATED (headless)

- Delivered: `Lives.h/.cpp` - `LifeRules` (nine age bands with yearly deaths per mille:
  60 / 15 / 3 / 4 / 8 / 18 / 40 / 150 / 500 for 0-1, 1-5, 5-15, 15-30, 30-45, 45-60,
  60-75, 75-90, 90+; fertile ages 16-40, fathers 16-55, 480 births per mille per fertile
  woman at full room falling to 50 at capacity, newborn sex), events PersonBorn and
  PersonDied (subject the person; payload index, region, age, mother), `AgeYears`,
  `BandOf`, `ReconcileRegion` (the coarse counts per culture and the believers per faith
  become what the living persons say; capacity untouched), `LifeSystem` (LOD World, after
  Population: for every detailed region in index order, deaths by band, births to
  couples - a father of the mother's culture picked among the eligible men - with the
  child inheriting culture, language, faith and family, then reconciliation),
  `MeasureLives`. In `VaelenSim`: `RegionLod` (a marker with `DetailedLevel` 2),
  `ObserveLod` on `PopulationSystem`, `MigrationSystem` and `DisasterSystem` (growth,
  decline, assimilation, abandonment and splits skip detailed regions; waves neither
  leave nor reach them; disasters strike them without deaths until 04.04), `PreHistory`
  accessors `Peoples` and `Migrations`; `PersonTypes` gains the `Lod` type,
  `Declare(World&, PreHistory&)` and `Attach`; promotion adds the marker, demotion
  removes it. Opt-in, so every Phase 03 digest is unchanged.
- Balance: a detailed region settles near 80 percent of its capacity, where the coarse
  model keeps its regions too (region 26 of AELVOR 128: 1 460 people at year 300,
  1 499 at year 500, never below 1 445 nor above 1 576 for a capacity of 1 878).
- Tests (5): ages and bands; two centuries in the busiest region with the grains
  consistent every year, the oldest under 110, births and deaths present, children
  and elders, the region peopled at 70 percent or more of capacity and never above
  120, one PersonBorn per person with a mother (who has a father and a birth in the
  run) and one PersonDied per dead person, the rest of the world moving on; the coarse
  systems leaving the detailed region alone (no wave leaves or reaches it in 30 years
  while waves move elsewhere, the grains consistent) and picking it up again after
  demotion, the life system doing nothing without a detailed region; two worlds
  identical, a snapshot between yearly ticks continued identically with persons, rules
  (no births leaves only deaths, no deaths leaves only births); frozen persons digest
  `9f2615d35856a752` after 200 years (1 499 alive, 1 499 born there).
- Decision: ADR-0033.

### 04.03 Families and lineage - VALIDATED (headless)

- Delivered: `Families.h/.cpp` - `FamilyInfo` (48 bytes: culture, home region, head,
  founder, generation, founding and extinction ticks, identity; ids of kind Family),
  `FamilyTypes::Declare`, `FamilyRules` (marrying ages 18-50, 350 per mille yearly chance
  for a groom to seek a bride, age gap 15, faith matters, a groom without a family founds
  one), events PersonMarried, FamilyFounded, FamilyExtinct, `FamilySystem` (LOD World,
  after Lives: the dead release their spouses; in every detailed region grooms pick the
  n-th eligible bride in index order - unmarried, of age, same culture and faith, within
  the gap, not kin within two generations - the groom founds a family when he has none
  and the bride joins it; heads replaced by the eldest living member, families without a
  living member declared extinct, the generation depth from the founder kept), lineage
  queries `FindPerson`, `Ancestors`, `Descendants`, `Siblings`, `AreKin`,
  `FamilyMembers` (pure functions over the mother and father links, one person index
  built per query), `MeasureFamilies` (families, extinct, married, adults, in a family,
  largest, broken spouse links). `PersonInfo` gains `Spouse` in its reserved tail (still
  64 bytes); `LifeRules.SpouseRequired` (off by default, so the 04.02 digest holds) and a
  married mother's child has her husband as father and her family.
- Tests (5): after 50 years in the busiest region of AELVOR 128, most adults are married
  and every spouse link is symmetric, alive, opposite sex, same culture and faith, in the
  same family and not close kin, every family has a living head that belongs to it or is
  extinct with no member, one FamilyFounded per family and a PersonMarried per couple;
  children of married mothers carry their husband as father and their family, a
  grandchild's ancestors and the grandparent's descendants agree, siblings share a
  parent, kinship is symmetric and depth-bounded, unknown persons have no lineage,
  families reach two generations and some go extinct with one event each; widows and
  widowers are released, remarriage happens, no marriages when the chance is zero,
  mixed-faith couples when faith does not matter, nothing without the system; two
  worlds identical, a snapshot between yearly ticks restored and continued identically;
  frozen persons digest `25e9435bfb921b5b` after 200 years (542 families, 816 married).
- Decision: ADR-0034.

### 04.04 Needs and body - VALIDATED (headless)

- Delivered: `Needs.h/.cpp` - `PersonNeeds` (8 bytes: food, health, a rest slot for
  Phase 10, hungry years), `NeedTypes::Declare`, `NeedRules` (a year burns 200 food and a full ration brings
  300, so good years rebuild the stores; hunger when the stores fall under a year's 200;
  a hungry year costs 40 health plus a draw below the deficit, a fed year restores 40;
  drought cuts 300/600/900 per mille of the ration by severity; plague strikes
  150/300/500 per mille of the persons for a draw below 300/360/420; infants and elders
  from 60 take 30 percent more; a drought names the famine for three years), `DeathCause`
  (natural, famine, starvation, plague) carried in `PersonPayload::Other`, `NeedSystem`
  (LOD World, after Lives: the DisasterStruck events of the last year in detailed regions
  are read from the log - the coarse system killed nobody there since 04.02 - the ration
  is the capacity over the living cut by the droughts, every living person eats, hungers
  or recovers, a plague strikes a random share, whoever reaches zero health dies with the
  disaster's event as the cause id, then the counts are reconciled), `MeasureNeeds`
  (living with needs, hungry, weak, food and health sums, deaths by cause from the log,
  deaths with a cause id).
- Tests (6): defaults and rule sanity; the busiest region of AELVOR 128 fed for 200 years
  stays whole and peopled, everyone alive carries needs, every caused death points at an
  earlier DisasterStruck of the matching kind and region whose coarse record killed
  nobody, and caused deaths appear exactly when a drought or plague struck; a cursed
  world (drought and plague every year, full strike) gives famine and plague deaths,
  hungry years and a shrinking region with every cause checked, while immune rules give
  none; a region shrunk to a quarter of its capacity starves without a cause; nothing
  happens without a detailed region; two worlds identical, a snapshot between yearly ticks
  continued identically; frozen persons digest `0e64632a2f2ded8c` after 200 years (1497 alive,
  42 caused deaths).
- Decision: ADR-0035.

### 04.05 Traits, skills and names - VALIDATED (headless)

- Delivered: `Traits.h/.cpp` - `PersonTraits` (16 bytes: six traits vigour, wit, will,
  charm, boldness, piety; four skills farming, craft, fighting, lore; a named flag),
  `TraitTypes::Declare`, `TraitRules` (heritability 500 per mille, skills from 8 to 45,
  yearly draw below 6 scaled by the trait behind the skill, apprenticeship until 15 adds
  a parent's skill over 64, decline from 60, names on), pure `TraitsFromIdentity` (three
  bytes of a lattice draw averaged: a bell around 128), `TraitsFromParents` (the own draw
  pulled toward the parents' mean), `TraitBehind`, `TraitSystem` (LOD World, after Lives:
  traits for every person that lacks them in index order so parents come first, a name
  built by `GenerateName` in the person's language - or its culture's latest - stored as
  a NameInfo of scope Person on the person entity, then a year of skill for the living),
  `PersonName`, `MeasureTraits` (living with traits, named, trait and skill sums, extremes,
  a digest of every person's traits and skills in index order).
- Tests (5): identities give deterministic traits with a mean in 118-138 and real tails,
  heritability 0 leaves the own draw and 1000 copies the parents; after 60 years in the
  busiest region of AELVOR 128 every person has traits and a pronounceable name in its
  language, children sit far closer to their parents than to strangers; skills are zero
  before 8, rise band by band to 45 and fade past 75, never exceed their cap, apprentices
  of skilled parents know more, no names when refused; two worlds identical in state,
  traits digest and names, another seed gives other names, a snapshot between yearly
  ticks continued identically; frozen traits digest `2c2d67a504110d60` after 200 years (1499 named,
  skill sum 358884).
- Decision: ADR-0036.

### 04.06 LOD bridge - VALIDATED (headless)

- Delivered: `Lod.h/.cpp` - `LodState` (singleton component: up to 8 wanted regions in
  request order, promotions, demotions, emigrants, immigrants, refusals), `LodTypes::Declare`,
  `LodStateOf` (created on first use), `RequestDetail` / `ReleaseDetail` / `IsWanted`,
  `LodRules` (4 detailed at once, the materialise rules, crowded above 750 per mille of the
  capacity and room below 650, 200 per mille of the crowd over the line leaves a year and
  200 per mille of a crowded neighbour's crowd arrives, movers are unmarried adults of 16
  to 40), events RegionPromoted, RegionDemoted, PersonLeft and
  PersonArrived (the other region in `Other`), `LodSystem` (LOD World, after Lives: demote
  the detailed regions nobody wants, promote the wanted ones up to the limit - an empty
  region is refused and counted - then for every detailed region in index order send the
  crowd over the line to the coarse neighbour with the most room, the persons marked gone
  and the destination's counts and faith raised, and take the crowd of every crowded
  coarse neighbour as new persons of its majority culture and faith while there is room,
  then reconcile), `MeasureLod`.
- Tests (5): requests kept in order, bounded, released and compacted, one state entity;
  a promote / demote cycle without a tick conserves the counts and the faith slots
  exactly, three wanted with a limit of two detail the first two in request order with
  one event each, a release frees a place the next year, an empty region is refused;
  a shrunk region sends unmarried young adults to a neighbour (persons gone, counts
  conserved up to births and deaths) and a full neighbour sends people into a widened
  region (new persons of age with culture and language, the neighbour's counts down),
  zero shares close the border; 500 years at 64 with two of the three busiest regions detailed in
  turn every 25 years keep every invariant each decade, two worlds identical, the year
  250 snapshot continued to the same year 500; frozen persons digest `d0f481c80e1e49d8` after 200
  years of alternation at 128 (207 emigrants, 0 immigrants).
- Decision: ADR-0037.

### 04.07 Persons in history - VALIDATED (headless)

- Delivered: `PersonHistory.h/.cpp` - `PersonChronicleRules` (record foundings, extinctions,
  deaths with a cause, deaths and marriages of heads of house, the focus moving; crossings
  off; 64 records a year per region), `PersonChronicleState` (records, dropped, the running
  yearly count), `PersonChronicle` listener (`Attach` subscribes to the eight person event
  types; `OnEvent` decides what matters against the world at dispatch - a head is still on
  its family - and writes a Phase 03 `RecordInfo` with the region from the payload and the
  era at the tick, counting the history state's records), `NamePerson` ("Umamissar" or
  "person 12"), `NameFamily` ("the house of Ukro"), `DescribePersonEvent` (one sentence for
  births with the mother, deaths with their cause and age, marriages, houses founded and
  died out, crossings, the chronicle turning to or leaving a region; every other event
  through `History::DescribeEvent`), `ExportChronicleWithPersons` (the whole chronicle in
  tick order), `PersonTimeline` (born, married on either side, left, arrived, died, a
  house founded), `DeathOf`, `ExportPersonStory` (the timeline, then "because ..." lines
  from the death's cause to the root), `CheckPersonChronicle`. Emigrants of the bridge
  (04.06) are no longer destroyed: `LifeState::Gone` keeps the person in the world out of
  every count so a PersonLeft line keeps its name (`DetailStats.Gone` counts them; the
  04.06 frozen digest is unchanged because the regions they left were demoted).
- Tests (5): after 40 years in the busiest region of AELVOR 128 every person event has a
  specific line with the Phase 03 prefix, a name and its verb, every other event reads
  exactly as Phase 03 writes it, fallbacks for unknown persons and houses; in a cursed
  region with a cap of three, records plus dropped equal the events that mattered, every
  recorded death has a cause, no year of the region holds more than the cap, silent rules
  record nothing and the Phase 03 chronicle check still holds; the story of a plague death
  ends with the plague that struck, a famine death reaches the drought, a natural death
  has no why, an unknown person no story, a bride's timeline holds her marriage; two
  worlds give the same digest and text, a snapshot between yearly ticks restores the
  records and continues identically; frozen 1248 records and chronicle text `f097d2b9938ed0bc` after
  100 years.
- Decision: ADR-0038.

### 04.08 Phase 04 gate; Phase 04 close - VALIDATED (headless)

- Delivered: `Tests/Population/Test_PopulationGate.cpp` - AELVOR 256 after 300 years of
  pre-history, the busiest region requested and lived through 500 years with every Phase
  04 system (lives with births to couples, families, needs, traits and names, the bridge,
  the person chronicle); every Phase 04 invariant checked every decade: exactly the
  requested region detailed and its two grains in agreement, every living person with
  needs, traits and a name, nobody past the last band, no broken spouse link, every head
  of house alive and in its house, every caused death pointing at a disaster of its
  region, the coarse bookkeeping exact and believers never above the people of the
  detailed region, the chronicle resolving completely with every person record
  described; the region alive after five centuries (more born there than living,
  houses risen and fallen, hundreds of person records, the world's people near
  capacity); the state frozen at 250 and 500 years with the persons digest and the log;
  a snapshot at year 250 restored into a fresh object and continued to the same year
  500.
- The gate found one bug: the family system judged its heads against the person index
  built at the start of its tick, so a head who married into another house that very
  year kept the old house for a year; the index is rebuilt before the heads step. The
  04.03 frozen values hold (heads do not move persons); the 04.07 chronicle refreezes
  (1222 records, text `f77936ba24c24e68`) because a head's marriage is a record.
- Three things the gate tightened: `FamilySystem::RunAfter` lets a world with needs and
  the bridge order the family system after them, so a head killed by a plague or gone
  over the border is replaced in the same yearly tick (the gate's harness uses it; the
  earlier suites keep their order and their frozen values); the trait system names a
  person whose culture has no language yet at a later year instead of never; the
  person-history text builds one `PersonIndex` per export or check instead of scanning
  the person pool for every name (the 100-year chronicle of 04.07 was cheap, the
  500-year one with eighty thousand persons in the pool was not). The gate's CTest
  entry gets 1800 s and `Population.Shuffled` 5400 s, for the MSVC debug runner.
- Decision: ADR-0039.

Phase 04 against the exit criteria of section 2: (1) CI matrix green for every task
(runs 34 to 39), 04.08 by its own run; (2) determinism tests for every system (two
worlds, snapshot mid-run, frozen digests per task and for the gate at 250 and 500
years, on four compilers through CI); (3) no INCOMPLETE file, engine files UNVERIFIED;
(4) unit, integration, deterministic, edge-case and long-duration categories present
(500 years at 256 with a detailed region, 500 years of LOD alternation at 64); (5)
ADR-0032 to ADR-0039, docs updated. Verdict: **Phase 04 VALIDATED on the headless
side, UNVERIFIED on the engine side until the first UE 5.6 build.**

## 9. Phase 05 - SOCIETY: task breakdown and real status

Goal: the society of AELVOR above the persons and below the polities - organisations
that persons found, join and leave, the standing of persons and houses, the norms of a
culture that parametrise how persons marry, inherit and hold others, and bondage and
slavery as institutions with entries, exits and counts - simulated at the person grain
in detailed regions and as shares of the coarse counts everywhere else, so that a
region can be promoted and demoted without losing its social shape.

Decisions taken up front (each becomes an ADR when its task closes):

- Organisations are entities of kind `Organization` (councils, temples, guilds, warbands,
  clans) with a seat region, a kind, a head, members by person index in detailed regions
  and a member count in coarse ones; founded from a region's persons by rules of the
  culture's norms and of the traits and skills of the founders (04.05).
- Standing is computed, not stored as truth: a person's rank derives yearly from its
  house (04.03), age, traits, skills and offices; the elite of a region is the top of
  that order. What is stored is what the world decided (offices held, honours given).
- Norms are a per-culture table (`NormSet`) read by the Phase 04 systems through their
  rules: marrying ages and kin depth, descent (patrilineal, matrilineal), faith
  tolerance, mobility, the bondage institutions allowed; they drift with events.
- Bondage and slavery are states on persons (`BondState`: free, bonded, enslaved) and
  shares of the coarse counts; entries by debt, capture (Phase 08 wars later) and birth,
  exits by manumission, flight and death; every entry and exit is an event with a cause.
- The social shape survives the grain: strata shares per region (elite, free, bonded,
  enslaved) are coarse counts kept through demotion and honoured at promotion.

| Task | Content | Tests |
|---|---|---|
| 05.01 | `VaelenSociety` module (UBT + CMake), `OrganizationInfo`, `OrganizationTypes`, membership in detailed regions and member counts in coarse ones, founding of councils and temples from a region's persons, events | unit, deterministic, edge (empty region, one person), snapshot |
| 05.02 | Status and rank: yearly standing from house, age, traits, skills and offices; the elite of a region; heads of house and organisation heads as offices | unit, distributions in bands, frozen |
| 05.03 | Norms: `NormSet` per culture read by the Phase 04 rules (marriage, descent, faith, mobility, bondage allowed), drift on events (a schism, a disaster, a split) | unit, deterministic, integration with 04.03 |
| 05.04 | Bondage and slavery as institutions: `BondState` on persons, coarse shares per region, entries by debt and birth, exits by manumission, flight and death, events with causes | integration over 200 years, edge (institution forbidden), frozen |
| 05.05 | Organisations acting: yearly decisions of councils (stores against famine), temples (faith and piety), guilds (skills), warbands (raids as omens for Phase 08); events with causes | integration with 04.04 and 03.04, deterministic |
| 05.06 | Social shape across the grains: strata shares per region as coarse counts, kept through demotion, honoured at promotion; digests conserved across a cycle | deterministic, long-duration (500 years alternating) |
| 05.07 | Society in history: foundings, disbandings, decisions, enslavements and manumissions that matter in the chronicle; text lines; why-queries | integration with 04.07, text deterministic |
| 05.08 | Phase 05 gate: 500 years with one detailed region over the 256 pre-history with every Phase 04 and 05 system, invariants every decade, frozen digests on four compilers; Phase 05 closed against section 2 | long-duration |

Every task ends with the usual report block, the docs refreshed and a commit.

### 05.01 VaelenSociety module and organisations - VALIDATED (headless)

- Delivered: `Source/VaelenSociety` (fourth kernel module: `VaelenSociety.Build.cs` depending
  on Core, VaelenCore, VaelenSim and VaelenPopulation; `CMakeLists.txt` with the explicit
  source list; `SocietyApi.h`; the Unreal-facing `VaelenSocietyModule.cpp` excluded from the
  headless build; listed in `Tools/kernel_modules.txt`, `Vaelen.uproject` and both targets),
  `Organizations.h/.cpp` - `OrganizationKind` (council, temple; guild, warband and clan
  reserved for 05.05), `OrganizationInfo` (64 bytes: index, kind, seat region, culture,
  faith, head, members, seats, founded and disbanded ticks, identity from the world seed),
  `Membership` on persons (organisation, role, since), `OrganizationTypes::Declare`,
  `OrganizationRules` (a council from 300 people with 7 seats, a temple from 200 believers
  of the majority faith with 50 per mille of them up to 64 seats, members from 20, disbanded
  after 3 empty years), events OrganizationFounded, OrganizationDisbanded, MemberJoined,
  MemberLeft, HeadSeated, `OrganizationSystem` (LOD World, after Families, `RunAfter` for
  Lod and Traits: for every detailed region in index order found what the people and the
  faith warrant, release the memberships of the dead and the departed, fill the empty
  seats - the heads of the largest houses for a council, the most pious of the faith for a
  temple, free persons of age only - seat a head when none is a living member - the eldest
  of a council, the most pious of a temple - keep the member count and disband the empty
  after the rule's years; coarse seats are left as they are), queries `OrganizationsOf`,
  `MembersOf`, `OrganizationOf`, `MeasureOrganizations` (totals by kind, memberships,
  astray links, heads alive, count mismatches, a digest of every organisation).
  `Tests/Society` mirrors `Tests/Population` (runner `VaelenSocietyTests`, entries
  `Society.<Suite>`, Registry and Shuffled).
- Tests (5): three years after the busiest region of AELVOR 128 is detailed a council and a
  temple exist with one founding and one head each, the council's seats are heads of houses
  no smaller than any house left outside, its head the eldest, the temple's seats the most
  pious of the majority faith, its head the most pious; sixty years keep every membership
  on a living person of the seat, every count equal to its persons, every head a living
  member, no seat over its number, with members released as they die and heads replaced;
  a demotion keeps both organisations with their last count and no membership, a
  promotion fills the same organisations again without a second founding; thresholds
  nobody reaches found nothing, a council nobody of age can join is disbanded after
  three years and founded again, nothing happens without a detailed region; two worlds
  identical in state and organisations digest, a snapshot between yearly ticks continued
  identically; frozen organisations digest `e4725ff3ae419103` after 100 years (2 organisations,
  71 members).
- Decision: ADR-0040.

### 05.02 Status and rank - VALIDATED (headless)

- Delivered: `Standing.h/.cpp` - `Tier` (common, notable, elite), `Office` bits (head of
  house, a seat, the head of a seat), `PersonStanding` (8 bytes: score, rank 0..255,
  tier, offices), `StandingTypes::Declare`, `StandingRules` (adults from 16; 4 points a
  house member up to 40, 60 for a headship, 80 for a seat, 120 for the head of one; age
  bands 0/20/40/30; a quarter of the mean of charm and will; three tenths of the best
  skill; the top 50 per mille elite, the next 150 notable), pure `StandingScore`,
  `StandingSystem` (LOD World, after Organizations: the standings of the dead, the gone,
  the young and the coarse dropped; in every detailed region the living adults scored
  in index order from one pass over houses and organisation heads, ranked by score then
  index, the rank scaled to 0..255 and the tiers cut by share), `EliteOf` (rank
  descending, then index; the elite tier or the top N), `StandingOf`, `MeasureStanding`
  (ranked, per tier, score sums, offices, stale standings, a digest in index order).
- Tests (4): the score follows the rules point by point (house, headship, seats, age
  bands, charm and will only, the best skill, the house cap); after five years in the
  busiest region of AELVOR 128 every ranked person is a living adult of the region,
  scores never rise as rank falls, the tiers cut at 5 and 15 percent with the elite above
  the notables above the common, nine in ten of the elite hold an office and the
  council's head is among them, the top three are the top ranks; forty years keep every
  adult ranked and nothing stale, a demotion drops every standing and a promotion ranks
  again the same year, zero shares leave everyone common, nothing happens without a
  detailed region; two worlds identical in state and standing digest, a snapshot
  between yearly ticks continued identically; frozen standing digest `c62c5f7a89880cb9` after 100
  years (1045 ranked, 52 elite).
- Decision: ADR-0041.

### 05.03 Norms - VALIDATED (headless)

- Delivered: `Norms.h/.cpp` - `Descent` (patrilineal, matrilineal), `Bondage` bits (debt,
  capture, birth; read by 05.04), `NormSet` (56 bytes on the culture entity: the
  `MarriageNorms`, descent, tolerance, mobility, the bondage allowed, drifts, the tick of
  the last change), `NormTypes::Declare` (the NormSet and the `MarriageNorms` mirror),
  `NormRules` (ranges for every custom, seven in ten cultures care for faith in marriage,
  one in five matrilineal, each bondage institution allowed with an even chance; a schism
  hardens the faith and costs 100 per mille of tolerance, a disaster of severity two or
  more adds 50 per mille of mobility), `NormField` and `NormFieldName`, events NormChanged
  (culture, field, before, after), pure `NormsFromIdentity` (lattice draws per custom),
  `NormsOfChild` (the parent's customs with one of the child's own), `NormValue`,
  `NormSystem` (LOD World, after Population: customs for every culture that lacks them,
  parents before children, then the drifts from last year's schisms and disasters read
  from the log, the mirror kept equal), `NormsOf`, `SetNorms`, `MeasureNorms`. In the
  population module `MarriageNorms` and `FamilySystem::ObserveNorms`: when told to
  observe the type, the family system marries by the groom's culture's customs
  (marrying ages, gap, faith, eagerness) where the culture entity carries them, and by
  its FamilyRules elsewhere; without the call nothing changes (the 04.03 digest holds).
- Tests (4): customs from an identity are deterministic, in range and varied over 256
  identities with about a fifth matrilineal and seven in ten caring for faith, a child
  differs from its parent in at most one custom; in AELVOR 128 every culture has customs
  with the mirror equal and split cultures within one custom of their parents, and with
  the busiest region's majority culture set to marry from 30 without regard to faith its
  marriages are all of age while the same world under the family rules alone marries
  young and never across faiths; twelve cursed years raise mobility once per great
  disaster with one event each, a schism hardens the faith and lowers the tolerance,
  a quiet world never drifts; two worlds identical in state and norms digest, a
  snapshot between yearly ticks continued identically, the family rules alone give
  another world; frozen norms digest `f2b7de051eab022b` after 100 years (4 cultures, 16
  drifts).
- Decision: ADR-0042.

### 05.04 Bondage and slavery as institutions - VALIDATED (headless)

- Delivered: `Bondage.h/.cpp` - `BondKind` (free, bonded, enslaved), `BondEntry` (debt,
  birth; capture reserved for Phase 08), `BondExit` (manumission, flight, the holder's
  death, death), `BondState` (16 bytes on a person: kind, entry, holder, since),
  `RegionStrata` (16 bytes on a region: free, bonded, enslaved), `BondageTypes::Declare`,
  `BondageRules` (15 per mille yearly chance of debt bondage for a common adult from 16
  where the culture allows it, hardening after 15 years, 25 per mille manumission and 8
  per mille flight a year, birth follows an enslaved mother, at most 12 held per holder),
  events BondEntered and BondLeft (person, region, kind, reason; the birth event or the
  death event as cause where one exists), `BondageSystem` (LOD World, after Standing and
  Norms: for every detailed region in index order the holders are the elite not
  themselves bound; the dead and the gone leave with their death as cause, the bonded
  whose holder died leave with that death as cause, then manumission and flight are
  drawn, the enslaved of a dead holder pass to the holder with the fewest held, bondage
  past the hardening years becomes slavery with a second BondEntered; newborns of an
  enslaved mother are born enslaved where birth bondage is allowed, common adults of a
  culture allowing debt bondage are drawn into it and given to the holder with the
  fewest held; the region's strata are written from the living), `BondOf`, `StrataOf`,
  `MeasureBondage` (bonded, enslaved, stale bonds, holders lost, entries and exits by
  reason from the log, causes carried, a digest of every bond then every strata). The
  bond state and its enums live in `BondState.h`, shared with the standing system:
  `StandingSystem::ObserveBonds` drops the bound from the ranks (bondage removes one
  from standing; the 05.02 digest holds because that world has no bonds).
- Tests (4): ten years in the busiest region of AELVOR 128 with every institution
  allowed bind more than ten common adults for debt to living members of the elite,
  never over the cap, with the strata equal to the living by kind, and forbidding every
  institution binds nobody; fast hardening without manumission or flight makes the
  enslaved outnumber the bonded, children of the enslaved are born enslaved with the
  birth as cause, holders' deaths and deaths free the bonded with the death as cause,
  every event names a person of the region with its kind and reason, and a culture
  refusing birth bondage has nobody born enslaved; generous manumission and flight free
  most within years with manumission ahead of flight, a demotion drops the bonds and
  keeps the strata as the last count, a promotion binds again, nothing happens without
  a detailed region; two worlds identical in state and bondage digest, a snapshot
  between yearly ticks continued identically; frozen bondage digest `99ec62d866000c68` after 200
  years (2888 entries, 2227 exits).
- Decision: ADR-0043.

### 05.05 Organisations acting - VALIDATED (headless)

- Delivered: `Decisions.h/.cpp` - `DecisionKind` (grain laid in, preached, trained, a raid
  planned), `DecisionTypes` (the `RegionStores` the need system observes), `DecisionRules`
  (grain kept three years after a drought and absorbing 400 per mille of a drought's cut,
  20 per mille of the region's people of other faiths converted a year up to 64, 6 craft
  points a year for a guild's members, a raid every 5 years at 10 strength a member),
  events DecisionMade (organisation, kind, seat, value; the drought as cause for grain,
  the raid for a raid) and RaidPlanned (organisation, from, target, strength; for Phase
  08), `DecisionSystem` (LOD World, after Organizations: for every living, peopled
  organisation of a detailed region in index order - a council with a drought in memory
  writes the region's grain, a temple turns the least pious of the other faiths and
  reconciles the counts, a guild trains its members, a warband plans a raid on the most
  peopled neighbour on its years; grain not renewed is spent), `StoresOf`,
  `MeasureDecisions`. The organisation system founds a guild once 20 of the region's
  living are skilled in craft (80 or more) and a warband once 20 are skilled in fighting,
  seats them by craft and by fighting and gives them the most skilled as head. The need
  system gains `RegionStores` and `ObserveStores`: a region's grain cuts what a drought
  takes.
- Refreeze: guilds and warbands now exist in the 05.01, 05.02 and 05.04 reference runs, so
  the organisations digest becomes `8ce703a8fa934de0`, the standing digest `ddad5d79565a46f8` (seats are offices)
  and the bondage digest `a1a92018c62bc034` (the elite changes); the Phase 03 and 04 digests are
  unchanged.
- Tests (4): after thirty years in the busiest region of AELVOR 128 a guild and a warband
  exist, seated by the skilled with the most skilled as head, training and raids in the
  log; ten cursed years in two worlds - one whose need system observes the stores, one
  not - leave fewer famine deaths and more food with the grain, one grain decision a
  year each naming its drought, the stores spent once the droughts are years away;
  preaching converts persons of other faiths year by year with the counts reconciled,
  training raises a member's craft on top of the year's growth, zero rules preach,
  train and raid nothing, nothing happens without a detailed region; two worlds
  identical in state and log, a snapshot between yearly ticks continued identically;
  frozen stores digest `d99d3be56bcbfe4f` after 100 years (155 decisions by 4 organisations).
- Decision: ADR-0044.

### 05.06 Social shape across the grains - VALIDATED (headless)

- Delivered: in `Bondage.h/.cpp` - `BondEntry::Promotion` and `BondExit::Departure`,
  `BondageSystem::RunAfter` (Lod, so a promotion's persons are bound in the same tick),
  and a first step of the yearly tick: a detailed region whose living carry no bond
  while its kept strata count the bonded and the enslaved binds as many of its common
  adults again, in index order, the enslaved first, each held by the holder with the
  fewest held, with a BondEntered event of reason Promotion; the departed (gone over the
  border) leave their bonds with a BondLeft of reason Departure instead of a death.
  The strata written from the living while detailed and kept while coarse (05.04) are
  thereby honoured at a promotion; the organisations' member counts (05.01) and the
  standings (05.02) already follow the persons.
- Tests (3): twenty years of bondage without manumission in the busiest region of AELVOR
  128, then a demotion keeps the strata and the bondage digest unchanged, and a promotion
  binds the strata's count again the same tick (within the year's deaths and the holders'
  room), the enslaved first, with one Promotion entry each, the strata equal to the
  persons again; a shrunk region sends people over the border and their bonds leave by
  departure, none sits on a gone person; 500 years at 64 with two of the three busiest
  regions detailed in turn every 25 years keep every invariant each decade - strata of
  detailed regions equal to the living by kind, coarse strata bounded, no stale bond or
  standing, no lost holder, no astray membership, no mirror mismatch - two worlds
  identical, the year-250 snapshot continued to the same year 500; frozen bondage digest
  `7d6be89760aa4807` (21 promotions, 1168 bound again by the strata).
- Decision: ADR-0045.

### 05.07 Society in history - VALIDATED (headless)

- Delivered: `SocietyHistory.h/.cpp` - `SocietyChronicleRules` (foundings and disbandings,
  heads seated, grain laid in, raids planned, customs changed, enslavements other than by
  a promotion, manumissions; sermons off; 32 records a year per region),
  `SocietyChronicleState`, `SocietyContext` (the person, family and organisation types
  the text needs), `SocietyChronicle` listener (`Attach` subscribes to the eight society
  event types; `OnEvent` decides what matters and writes a Phase 03 `RecordInfo` with the
  seat as region - a custom has none - and the era at the tick, under the cap),
  `NameOrganization` ("the council of Edavaken", "the temple of Oldiss in Edavaken"),
  `NameCulture`, `DescribeSocietyEvent` (one sentence for foundings, disbandings, joins,
  leaves, heads, decisions with their value, raids with target and strength, customs
  with before and after, bonds entered with their reason, freed with theirs; every other
  event through the person and history text), `ExportChronicleWithSociety` (the whole
  chronicle in tick order), `ExportWhyWithSociety` (the event, then "because ..." lines
  to the root), `CheckSocietyChronicle`.
- Tests (3): thirty cursed years in the busiest region of AELVOR 128 with every institution
  and a schism give every society event type its own named line with the Phase 03 prefix
  and every other event the person text, fallbacks for unknown organisations and
  cultures; with a cap of four, records plus dropped equal the events that mattered, no
  year of a region holds more than the cap, every record is described and era-consistent,
  the why of a grain decision reaches the drought, silent rules record nothing and the
  Phase 03 and 04 chronicles still check; two worlds give the same text, a snapshot
  between yearly ticks restores the records and continues identically; frozen 622
  records and chronicle text `2e503b54e8c9604d` after 100 years.
- Decision: ADR-0046.

### 05.08 Phase 05 gate; Phase 05 close - VALIDATED (headless)

- Delivered: `Tests/Society/Test_SocietyGate.cpp` - AELVOR 256 after 300 years of
  pre-history, the busiest region requested and lived through 500 years with every Phase
  04 and 05 system (lives with births to couples, families under the customs of their
  cultures, needs softened by the councils' grain, traits and names, the bridge, the
  person chronicle, organisations of every kind, standing, norms, bondage, decisions,
  the society chronicle); every invariant checked every decade: one detailed region with
  its two grains in agreement, ages bounded, no astray membership, count mismatch or
  headless organisation, no stale standing and the tiers whole, every culture with
  customs and its mirror equal, no stale bond or lost holder and the strata equal to the
  living by kind, every society cause in the log, the three chronicles resolving with
  every record described; the society lived (organisations of every kind, hundreds of
  decisions, raids planned, customs drifted); the state frozen at 250 and 500 years with
  the log and the chronicle text; a snapshot at year 250 restored into a fresh object and
  continued to the same year 500 with the same text.
- Decision: ADR-0047.

Phase 05 against the exit criteria of section 2: (1) CI matrix green for every task
(runs 42 to 48), 05.08 by its own run; (2) determinism tests for every system (two
worlds, snapshot mid-run, frozen digests per task and for the gate at 250 and 500
years, on four compilers through CI); (3) no INCOMPLETE file, engine files UNVERIFIED;
(4) unit, integration, deterministic, edge-case and long-duration categories present
(500 years at 256 with a detailed region, 500 years of alternation at 64 with every
system); (5) ADR-0040 to ADR-0047, docs updated. Verdict: **Phase 05 VALIDATED on the
headless side, UNVERIFIED on the engine side until the first UE 5.6 build.**

## 10. Phase 06 - ECONOMY: task breakdown and real status

Goal: the economy of AELVOR at both grains - goods produced from the land, the skills
and the organisations, kept as stocks by regions and by houses, exchanged on markets at
prices that move with scarcity, carried along routes between regions, held as wealth
that passes with inheritance - so that famine, standing and the decisions of Phase 05
have a material side, and so that a region can be promoted and demoted without losing
what it owns.

Decisions taken up front (each becomes an ADR when its task closes):

- Goods are kinds in a table (grain, cloth, tools, ore, timber, salt, luxuries), never
  entities; stocks are integer counts per good on a region (coarse) and on a house
  (detailed), conserved through promotion and demotion like the strata.
- Production is yearly from the region's land (the Phase 02 biomes and deposits), its
  people and their skills (04.05), scaled by the customs and the organisations (a guild
  raises craft output); consumption follows the needs (04.04) so that grain feeds and
  the stores of 05.05 become real stock.
- Markets are per region; prices are integers moved by the ratio of stock to yearly
  need, with a floor and a ceiling; trade moves goods along the region graph by routes
  (entities of kind Route) opened where two markets differ enough, and a settlement
  (entity of kind Settlement) marks a region whose market is busy enough.
- Wealth is a house's stocks at prices; it enters standing (05.02) as a weight and
  passes to the heir by the descent custom (05.03) when a head dies; bondage (05.04)
  gains debt as its entry with a number.
- Items of Phase 09 (crafted, named, carried) are not in this phase: Phase 06 counts
  goods, Phase 09 makes things.

| Task | Content | Tests |
|---|---|---|
| 06.01 | `VaelenEconomy` module (UBT + CMake), `Good` kinds, `RegionStock` and `HouseStock` components, conservation through promotion and demotion, events | unit, deterministic, edge (empty region, no houses), snapshot |
| 06.02 | Production and consumption: yearly output from land, people, skills, guilds; needs fed from stock, the stores of 05.05 as real grain | integration with 04.04 and 05.05, frozen |
| 06.03 | Markets and prices: per-region markets, integer prices from stock over need, floors and ceilings, price events | unit, distributions, deterministic |
| 06.04 | Trade and routes: routes opened along the graph where prices differ, goods carried yearly, settlements marked | deterministic, long-duration (500 years at 64) |
| 06.05 | Wealth and inheritance: a house's wealth at prices, its weight in standing, inheritance by descent at a head's death, debt as a number in bondage | integration with 05.02, 05.03, 05.04, frozen |
| 06.06 | Economy across the grains: stocks kept through demotion and honoured at promotion, houses' stocks folded into the region's | deterministic, long-duration (500 years alternating) |
| 06.07 | Economy in history: routes opened and closed, prices that mattered, fortunes made and lost, in the chronicle; text; why | integration with 05.07, text deterministic |
| 06.08 | Phase 06 gate: 500 years with one detailed region over the 256 pre-history with every Phase 04, 05 and 06 system, invariants every decade, frozen digests on four compilers; Phase 06 closed against section 2 | long-duration |

Every task ends with the usual report block, the docs refreshed and a commit.

### 06.01 VaelenEconomy module, goods and stocks - VALIDATED (headless)

- Delivered: `Source/VaelenEconomy` (fifth kernel module: `VaelenEconomy.Build.cs` depending
  on Core, VaelenCore, VaelenSim, VaelenPopulation and VaelenSociety; `CMakeLists.txt` with
  the explicit source list; `EconomyApi.h`; the Unreal-facing `VaelenEconomyModule.cpp`
  excluded from the headless build; listed in `Tools/kernel_modules.txt`, `Vaelen.uproject`
  and both targets), `Stocks.h/.cpp` - `Good` (grain, cloth, tools, ore, timber, salt,
  luxuries; a table, never entities), `RegionStock` (32 bytes, the goods a region holds in
  common: its whole stock while coarse) on region entities, `HouseStock` (32 bytes) on the
  family entities of a detailed region, `EconomyTypes::Declare`, `EconomyRules` (the
  endowment: 500 per mille of the capacity in grain, timber, ore and salt per point of
  richness of the region's deposits, luxuries at 100 per mille of its gold; 800 per mille
  of the common stock split among the houses at a promotion), events StockEndowed,
  StockSplit, StockFolded, StockReturned, StockAdded, StockTaken (`StockPayload`: region,
  house or count of houses, good or every good, units), `StockSystem` (LOD World, after
  Families, `RunAfter` for Lod: the land endows every region once from its capacity and
  its deposits; the houses of a region coarse again fold their goods into the common stock
  and an extinct house returns its own; in every detailed region every living house holds
  a stock, and a region whose houses hold none yet - just promoted - splits the rule's
  share of its common stock among them by their living members, the rest staying in
  common; a house founded later starts with nothing), queries `StockOf`, `HouseStockOf`,
  `TotalStock` (common and houses by good), `AddStock` (units added to or taken from the
  common stock or a house's, clamped at zero and at the top, the units moved returned, an
  event with the cause), `MeasureStocks` (regions and houses holding, stale house stocks,
  totals and commons by good, events by kind, a digest of every common stock in region
  order then every house stock in family order). `Tests/Economy` mirrors `Tests/Society`
  (runner `VaelenEconomyTests`, entries `Economy.<Suite>`, Registry and Shuffled).
- Tests (4): every region of AELVOR 128 is endowed exactly once, grain following its
  capacity and timber its timber deposits (regions with and without both exist), nothing
  made yet, ten more years changing nothing, the names distinct with "goods" for every
  good, the lookups null for the unknown, the same world giving the same digest; a
  promotion of the busiest region gives every living house a stock, most of them grain,
  the split event counting the houses and the units, the whole of the region unchanged,
  and unchanged through thirty years in which extinct houses return their goods, a
  demotion folds everything into the common stock and a second promotion splits again;
  moving stock by hand adds, takes, is clamped at zero and saturates at the top, refuses
  the unknown region, house or good and a house while its region is coarse, counts its
  events with the cause on them; two worlds identical every decade for a century with
  the whole world's stock conserved and no stale house stock, a snapshot at year 50
  continued to the same year 100; frozen stocks digest `b96d3bc0a8ee4141` after 100 years (27581 grain,
  179 houses holding).
- Decision: ADR-0048.

### 06.02 Production and consumption - VALIDATED (headless)

- Delivered: `Production.h/.cpp` - `ProductionTypes` (`RegionRation`, a Population type the
  need system observes), `ProductionRules` (a person needs 4 grain a year; a worker harvests
  7 at ordinary farming; seven tenths of the people on the land within the capacity work in
  a coarse region, the persons of age 12 in a detailed one, each yielding 850 to 1150 per
  mille by farming skill, the land taking no more workers than the capacity's share; grain
  in store spoils 100 per mille; droughts cut 300, 600, 900 per mille by severity; deposits
  yield 50 per mille of their richness a year once 20 people live there; one timber burnt
  per 10 people, one salt per 50, one cloth made per 20 and worn per 25, one tool made per
  40 from one ore and worn per 50, the craft output 850 to 1150 per mille by the crafters'
  mean skill in a detailed region), events Harvest (the region's units, the drought as cause
  when it cut) and Shortfall (the units short), `ProductionSystem` (LOD World, after Stocks,
  `RunAfter`; `ObserveTraits` for the skills, `ObserveStores` for a council's granary:
  that share of a detailed region's harvest goes to the common stock instead of the
  houses; the harvest, the spoilage, the meals - a house from its own stock then the
  common one, the unhoused from the common one - the ration written, the other goods),
  `RationOf`, `MeasureProduction` (harvests, units, cut, shortfalls, units short, regions
  rationed, the lowest ration, a digest of every ration in region order). `Needs.h/.cpp`
  (Phase 04) gain `RegionRation`, `NeedSystem::ObserveRation` - the economy's ration
  replaces the land's, droughts included - and `RunAfter`, the Phase 04 digests unchanged.
- Tests (4): at year 300 every peopled region of AELVOR 128 harvested last year, is
  rationed at a full ration, holds no more than a dozen harvests in store, the timbered
  hold timber, and a drought curse cuts the busiest region's harvest by a quarter or more
  with the drought as the harvest's cause; detailed, the busiest region keeps a full
  ration with a tenth at most hungry and most houses holding grain after the meals, a
  people needing seven grain a head gets a ration under 700, shortfalls every year, a
  hungry majority and starvation deaths (not famine: no drought) while the well fed have
  none, and a council's granary leaves more grain in common for the same harvest; over
  ten years the detailed harvest is within three tenths of the coarse one and, demoted
  again, so is the next decade's with the houses' grain folded, luxuries neither made nor
  used in either grain; two worlds identical every decade for a century, a snapshot at
  year 50 continued to the same year 100; frozen stocks digest `2e21806d2979bc15`, rations digest
  `06b87708f4c215ef`, 23070167 grain harvested.
- Decision: ADR-0049.

### 06.03 Markets and prices - VALIDATED (headless)

- Delivered: `Markets.h/.cpp` - `RegionMarket` (32 bytes, one integer price per good) on
  every region holding a stock, `MarketTypes::Declare`, `MarketRules` (base prices grain 10,
  cloth 40, tools 60, ore 20, timber 8, salt 30, luxuries 200; a floor at a quarter and a
  ceiling at eight times the base; a market wants two years of its people's grain, three of
  every used good and one luxury per 200 people; a price moved by 250 per mille is an
  event), `WantedStock` (what a region wants of a good for so many people, by the same
  consumption rules the production uses), `PriceFor` (the base times wanted over held,
  clamped; the floor when nothing is wanted, the ceiling when nothing is held), event
  PriceChanged (region, good, the new price; the year's harvest as cause for grain),
  `MarketSystem` (LOD World, after Production, `RunAfter`: for every region holding a stock
  the living or the counted people, the common stock and the houses' together, the seven
  prices, an event for each that moved enough or on the first pricing), `MarketOf`,
  `ValueOf` (a stock at a market's prices), `MeasureMarkets` (markets, the lowest and the
  highest price of every good, markets at the floor and at the ceiling, price events and
  those with a cause, a digest of every market in region order).
- Tests (4): the price rule alone (floor when nothing is wanted, ceiling when nothing is
  held, the base when held equals wanted, never under the floor, an unknown good priced
  at nothing, the wanted stocks, the value of a stock), then AELVOR 128 at year 300 with
  a market on every region holding a stock, every price within the bounds, grain held
  beyond two years never dearer than the base, the busiest region's grain price climbing
  once its store is taken and at the floor once flooded; a drought on a thin store moves
  the grain price and the price event's cause is the year's harvest whose cause is the
  drought, with price events sparse over three centuries; grain dear somewhere and cheap
  elsewhere, ore at the ceiling where no deposit is, grain mostly between the bounds, two
  worlds the same digest, and the busiest region's prices within a factor of two of the
  coarse ones after five years detailed; determinism and snapshot continuity; frozen
  markets digest `5c3edf001360621e` after 100 years (1277 price events).
- Decision: ADR-0050.

### 06.04 Trade and routes - VALIDATED (headless)

- Delivered: `Trade.h/.cpp` - `RouteInfo` (48 bytes: index, the two regions in order,
  idle years, opened and closed ticks, units carried over its life, identity) on entities
  of kind Route, `SettlementInfo` (48 bytes: index, region, open routes and traffic of the
  last year, founded and abandoned ticks, quiet years, identity) on entities of kind
  Settlement, `TradeTypes::Declare`, `TradeRules` (a route opens on a good twice as dear on
  one side, wanted there and in surplus on the other; a quarter of the surplus carried a
  year, 1000 units a good at most; four routes a region; closed after five idle years; a
  settlement from fifty units of traffic a year, abandoned after ten quiet years), events
  RouteOpened, RouteClosed, GoodsCarried (the route, the two regions, the units), 
  SettlementFounded, SettlementAbandoned (`TradePayload`), `TradeSystem` (LOD World, after
  Markets, `RunAfter`; the region graph cached as in 05.05: every open route in index
  order carries every good from the cheaper side's common stock to the dearer side's, up
  to the seller's surplus share, the buyer's want and the limit, the traffic counted on
  both regions, the idle closed; then between neighbours with markets and no open route
  the warranted are opened, a closed road between the same regions reopened; then the
  settlements founded where the traffic is and abandoned where it stopped), `RoutesOf`,
  `RouteBetween`, `SettlementOf`, `MeasureTrade` (routes open and closed, carries and
  units from the log, settlements alive and abandoned, the busiest settlement's traffic,
  bad routes - not adjacent, opened twice, out of order or past a region's number - and a
  digest of every route then every settlement in index order).
- Tests (4): AELVOR 128 at year 300 has open routes joining two markets each, units
  carried, settlements on regions with routes and nothing bad, fewer markets at the ore
  ceiling than the same world without trade (which has no route at all), and a region
  flooded with luxuries beside a drained neighbour sees them cross on the route between
  them the next year, the route's count rising; a world flooded with everything closes
  every route within five years and abandons every settlement within ten with an event
  each, nothing carried since, and drained again opens routes and founds settlements
  with new indices; one route a region at most holds, a hundredfold gap opens nothing,
  islands without a neighbour have no route, two worlds the same digest; 500 years at 64
  with the busiest region detailed hold every road and settlement invariant every fifty
  years, a snapshot at year 250 continued to the same year 500; frozen trade digest
  `2e83bc0c672aba8f` (134864 units carried, 25 settlements founded).
- Decision: ADR-0051.

### 06.05 Wealth and inheritance - VALIDATED (headless)

- Delivered: `Wealth.h/.cpp` - `WealthTypes` (`HouseWealth`, a Society type declared here and
  observed by the standing system; `HouseHeir`, an Economy type observed by the stock
  system), `WealthRules` (a child inherits from twelve; a rank moved by 250 per mille is an
  event), events FortuneChanged (the house, its old and new rank) and HeirNamed (the house,
  the heir's house, the heir), `WealthSystem` (LOD World, after Markets, `RunAfter`: a house
  that died out or whose region went coarse keeps neither wealth nor heir; every living
  house of a detailed region is valued at its region's prices - `ValueOf` from 06.03 - and
  the region's houses are ranked 0 to 255 among themselves; each house then names its heir,
  the eldest living child of the head who carries the line by the culture's descent custom
  and has a house of their own in the region), `WealthOf`, `HeirOf`, `RichestOf`,
  `MeasureWealth` (houses valued and with an heir, the richest, the sum of values, fortune
  events, heirs named, inheritances, stale rows and a digest of every wealth then every heir
  in family order). `Standing.h/.cpp` (Phase 05) gain `HouseWealth`, `StandingRules::
  WealthPointsPerMille` (400 of the rank), a defaulted wealth argument to `StandingScore`,
  `StandingSystem::ObserveWealth` and `RunAfter`; without an observer the score is the
  Phase 05 one and the 05.02 digest holds. `Stocks.h/.cpp` (06.01) gain `HouseHeir`,
  `StockSystem::ObserveHeirs` and the StockInherited event; without an observer an extinct
  house's goods return to the common stock as before and the 06.01 digests hold.
- Tests (5): the score rule alone (a full wealth rank is worth exactly its share, no
  observer is worth nothing), then every house of the busiest region of AELVOR 128 valued at
  its market with the ranks spanning 0 to 255, the richest first, and a pile of luxuries
  lifting the poorest house to the top with a fortune event; the region's elite belong to
  wealthier houses than its mean while standing keeps its own invariants; heirs follow each
  house's own culture (cultures born of a split keep their descent), never the house itself,
  and always a living child of the head who carries the line - and because a bride joins her
  husband's house while a groom who has one keeps it (04.03), a matrilineal world names many
  times more heirs than a patrilineal one, whose sons hold the house itself; sixty years
  with heirs honoured give inheritances that name a living house of the region, while the
  same world without the hook inherits nothing and returns more to the commons; determinism
  and snapshot continuity; frozen wealth digest `8347935dd85ca3e9` after 100 years (4 houses with an
  heir, 12 inheritances).
- Decision: ADR-0052.

### 06.06 The economy across the grains - VALIDATED (headless)

- Delivered: the last thing the fine grain left behind is gone. `WealthSystem` now sweeps,
  every year and even when no region is detailed, every house that died out or whose region
  went coarse again, and removes its `HouseWealth` and `HouseHeir`: both are readings of a
  detailed region, and the stock system folded its goods the same tick (06.01), so nothing
  of the fine grain outlives it. `MeasureWealth` counts a purse or an heir on a dead house
  as stale, and the stat is zero at every check of both long runs.
- Tests (Grains, 2): a still world of AELVOR 64 - only the owners of goods, with no
  production, trade, wealth, meals or minds - keeps every unit of every good, to the unit,
  through five hundred years in which three regions are detailed in turn every twenty-five
  (21 promotions, nineteen demotions, a split at every promotion and a fold at every
  demotion), with no stale or orphaned house stock at any check; frozen still digest
  `ace29fbb38633cbf`. A living world of the same map, with every Phase 04, 05 and 06 system, holds
  every economic invariant every twenty-five years - nothing stale or orphaned, every price
  within its floor and ceiling, every ration at most a full one, every road between markets
  that exist, a market wherever a stock is - stays identical to a second world tick for
  tick, receives inheritances across the centuries and keeps its roads open; a snapshot at
  year 250 continues to the same year 500; frozen living digest `9ac28d8ee3a7ef34`.
- Decision: ADR-0053.

### 06.07 The economy in the chronicle - VALIDATED (headless)

- Delivered: `EconomyHistory.h/.cpp` - `EconomyChronicleRules` (what is recorded and the
  thresholds: a fortune must move 128 of 255, a shortfall must reach 20 units, a road must
  have carried 100 before its abandonment is history, at most 16 records a region a year),
  `EconomyChronicleState`, `EconomyChronicleTypes`, `EconomyContext`, the `EconomyChronicle`
  listener over RouteOpened, RouteClosed, SettlementFounded, SettlementAbandoned,
  PriceChanged, Shortfall, FortuneChanged and StockInherited, `NameRoute` ("the road from
  Edavaken to Ekum"), `NameSettlement` ("the town of Edavaken"), `DescribeEconomyEvent`
  (every economic event gets its own sentence after the year and the age; the lower layers
  keep their own words), `ExportChronicleWithEconomy`, `ExportWhyWithEconomy`,
  `CheckEconomyChronicle`.
- Two corrections the writing of the chronicle brought out. A market appearing published
  seven price-changed events - nearly seven hundred for the world at its first tick - so
  the first pricing of a region is now silent and the 06.03 frozen count of changes becomes
  584. A road reopened on a passing price gap published the same event as a road built, so
  `RouteInfo` counts its openings (the field pairs with `Idle` in the same four bytes, the
  struct keeps its 48) and only the first is history; the 06.04 trade digest becomes
  `54cba9c0fc9ee231`.
- Tests (3): every economic event of ten years of a detailed region has a line of its own,
  prefixed by the year and the age, generic for nothing, naming no bare person or family,
  while the other events keep their lines, and the names fall back plainly for a road or a
  town that is gone; only what matters is recorded (1147 records where the same span
  publishes thousands of harvests and carries), every recorded price is at a bound, a rule
  that records nothing records nothing, and the why of a dear loaf reaches the drought
  through the harvest; determinism, snapshot continuity, and the chronicle text of a
  restored world identical to the original; frozen text digest `31b14de751d3d588`.
- Decision: ADR-0054.

### 06.08 Phase 06 gate - VALIDATED (headless)

- Delivered: `Tests/Economy/Test_EconomyGate.cpp`. AELVOR 256 after 300 years of
  pre-history, its busiest region detailed, and every Phase 04, 05 and 06 system running
  for 500 years: lives, families with the marriage norms, needs fed by the economy's
  ration, traits, lod, organisations, standing weighed by wealth, norms, bondage with the
  strata, decisions, stocks with heirs, production, markets, trade, wealth, and the
  person, society and economy chroniclers. Every decade the invariants of all three
  phases hold: persons consistent with the counts, no astray membership, standing over
  the living and free only, bonds and strata sound, customs within their bounds, every
  cause resolving in the log, and - new here - nothing stale or orphaned in the economy,
  a market wherever a stock is, every price within its floor and ceiling, every ration at
  most a full one, no bad road, and every record of all three chronicles described and
  era-consistent. A snapshot at year 250 restored and run to year 500 gives the same
  world, the same log and the same chronicle text. Frozen: world `6e106563938bb288` at year 250,
  `55c39d36a2171b64` at year 500, log `ac48bb403c40fd73`, chronicle text `e5f20b503614f6cc`. The gate takes 203 s plus
  124 s for the snapshot half on the clang debug build; CTest gives it 1800 s and the
  shuffled Economy run 5400 s.
- What the gate found, and it found both only by running everything at once:
  the orderings of Phases 05 and 06 cannot both hold. `Houses->RunAfter("Needs")` (a
  family forms after this year's hunger) and `Body->RunAfter("Production")` (the needs
  read this year's ration) close a cycle through Stocks, which runs after Families. The
  chain that carries the grain wins and the family system forms its marriages on the
  living of the tick before. And the economy's describer was giving the society's events
  the plainer words of the person layer, so a founding or a raid lost its sentence in the
  world's own chronicle; the describer of the topmost layer now speaks for every layer
  under it.
- Phase 06 ECONOMY: VALIDATED (headless). Phase 07 POLITICS broken down into 07.01-07.08
  in section 11.
- Decision: ADR-0055.

## 11. Phase 07 - POLITICS: task breakdown and real status

Goal: authority over the regions of AELVOR - who rules where, by what right, under what
law, with what following - so that the organisations of Phase 05 and the wealth of Phase
06 have a power above them, and so that Phase 08 has something to make war over.

Decisions taken up front (each becomes an ADR when its task closes):

- A polity is an entity of kind Polity holding regions; a region belongs to at most one
  polity, and belonging is a component on the region so that a demotion never loses it.
- Authority is exercised through the organisations of Phase 05: a polity's seat is a
  council, its ruler the head of that council, and its reach the regions whose councils
  answer to it. No new kind of person: rulers are persons with standing and offices.
- Law is a small table of rules a polity sets (taxes in goods, bondage allowed or not,
  tribute owed), written as components the lower systems observe exactly as the need
  system observes the ration - the political layer never reaches into them.
- Succession follows the descent custom of the ruler's culture (05.03) as inheritance
  does (06.05); a disputed succession is a faction, not a special case.
- Diplomacy is a standing relation between two polities, moved by tribute, raids (05.05)
  and trade (06.04), and read by Phase 08.

| Task | Content | Tests |
|---|---|---|
| 07.01 | `VaelenPolitics` module (UBT + CMake), `PolityInfo`, region belonging, founding and dissolution, the seat and the ruler from a council | unit, deterministic, edge (no council, a region twice claimed), snapshot |
| 07.02 | Law: a polity's rules as components the population, society and economy systems observe; taxes taken in goods | integration with 04.04, 05.04 and 06.02, frozen |
| 07.03 | Authority and reach: regions won and lost, the cost of distance, a polity that cannot hold what it claims | deterministic, long-duration |
| 07.04 | Succession: the ruler's death, the heir by descent, a regency, a disputed succession | integration with 05.03 and 06.05, frozen |
| 07.05 | Factions: persons and houses gathered behind a claim, their standing and wealth weighing, defection | unit, distributions, deterministic |
| 07.06 | Diplomacy: relations between polities moved by tribute, raids and trade; treaties as records | integration with 05.05 and 06.04 |
| 07.07 | Politics in history: foundings, laws, successions, treaties in the chronicle; text; why | integration with 06.07, text deterministic |
| 07.08 | Phase 07 gate: 500 years at 256 with every Phase 04 to 07 system, invariants every decade, frozen digests; Phase 07 closed against section 2 | long-duration |

Every task ends with the usual report block, the docs refreshed and a commit.

### 07.01 VaelenPolitics module and polities - VALIDATED (headless)

- Delivered: `Source/VaelenPolitics` (sixth kernel module: `VaelenPolitics.Build.cs` depending on
  Core, VaelenCore, VaelenSim, VaelenPopulation, VaelenSociety and VaelenEconomy; `CMakeLists.txt`
  with the explicit source list; `PoliticsApi.h`; the Unreal-facing `VaelenPoliticsModule.cpp`
  excluded from the headless build; listed in `Tools/kernel_modules.txt`, `Vaelen.uproject` and
  both targets), `Polities.h/.cpp` - `PolityInfo` (48 bytes: index, seat region, culture, ruler,
  council, regions held, founding and dissolution ticks, identity from the world seed) on entities
  of kind Polity, `RegionRule` (16 bytes: polity, since) on region entities, `PolityTypes::Declare`,
  `PolityRules` (a seat holds 400 living and a council of 4; a polity ruling nothing is dissolved
  after 3 years; a ruler is 20), events PolityFounded, PolityDissolved, RulerSeated, RegionClaimed,
  RegionLost, `PolitySystem` (LOD World, after Organizations, `RunAfter`: found what the councils
  warrant, seat the ruler as the council's living head of age, count the regions from the regions
  themselves, dissolve what has no council or nothing to rule and free its regions), `PolityOf`,
  `RuleOf`, `RegionsOf`, `MeasurePolities` (standing and dissolved, regions ruled and freed,
  headless polities, bad ones - a seat outside its own polity, a ruler who is not the council's
  head, a region held by a polity that is gone - events by kind, and a digest of every polity in
  index order then every rule in region order).
- Tests (4): a detailed region of AELVOR 128 founds one polity on its council within two years,
  seated there, ruled by the council's head, holding one region that says so from its own side,
  and the ruler follows the head across twenty years of deaths; the rule survives a demotion with
  its date unchanged and the polity is never founded twice, and a polity whose last region is taken
  dissolves and stays as history while the same tick founds a new one on the same council - a seat
  whose council still sits does not stay masterless; a threshold nobody meets founds nothing, a
  ruler nobody is old enough to be leaves the seat empty while the polity still holds its region,
  and two worlds of one seed agree on every polity and every rule; determinism and snapshot
  continuity; frozen polities digest `ab11336dba47da54` after 100 years (1 standing, 9 rulers seated).
- Decision: ADR-0056.

### 07.02 Law, and the dues taken in grain - VALIDATED (headless)

- Delivered: `Law.h/.cpp` - `PolityLaw` (32 bytes: polity, share per mille, moves, consecutive
  failed collections, the tick the share last moved, grain taken since the founding) and
  `Treasury` (32 bytes, by Good, as a region's stock) on the polity entity; `RegionDues`
  (8 bytes: share per mille, owed) added to `Source/VaelenEconomy/.../Stocks.h` - the lower
  module declares the struct, the higher one writes it, as the ration of 06.02 is declared by
  the population and written by the economy; `ProductionSystem::ObserveDues`, which assesses the
  share on the harvest of the year in the year it is reaped and leaves the grain where it lies;
  `LawTypes::Declare`, `LawRules` (a new polity demands 80 per mille, never under 20 nor over
  250, moving 20 at a time; it wants 2 grain in store per person it rules, relents at three
  times that or after three years of short collection), events LawChanged, DuesPaid, DuesUnpaid,
  `LawSystem` (LOD World, after Polities and the harvest: found a law and proclaim it, collect
  in region order, let the share move, write it down again; a polity that is gone demands
  nothing more; a region nobody rules is demanded nothing of and keeps its arrears),
  `LawOf`, `TreasuryOf`, `DuesOf`, `MeasureLaws` (laws, regions demanded of, regions owing,
  grain held and in arrears, events by kind, and the bad - a share outside its bounds, a
  dissolved polity still demanding, dues with no master - with a digest of every law and
  treasury in polity order then every dues in region order).
- Tests (4): the founding writes a law and proclaims it on the seat, the years fill the treasury,
  and what the log says was paid equals what the polity says it took equals what it holds - nothing
  is spent yet and nothing is invented; the share climbs to its ceiling under a want no harvest can
  meet, falls to its floor under a want already met, and a region stripped of its common stock pays
  nothing, leaves arrears standing and makes the polity relent; a region nobody rules has no dues at
  all, the count of the demanded never runs ahead of the count of the ruled, the share written on
  the region never drifts from the law, an absurd rule is still clamped to its bounds, and two
  worlds of one seed agree on every law and every due; determinism, snapshot continuity, frozen law
  digest `6e8b720c1543fb20` after 100 years (3476 grain taken, 50 moves of law).
- Decision: ADR-0057.

### 07.03 Authority and reach - VALIDATED (headless)

- Delivered: `Reach.h/.cpp` - `RegionAuthority` (16 bytes: whose authority runs here, hops from
  that polity's seat, hold per mille) on every ruled region and `PolityReach` (32 bytes: hops its
  word carries, regions taken, regions slipped, grain spent, upkeep unpaid last year) on the
  polity; `ReachTypes::Declare`, `ReachRules` (a thousand per mille at the seat, 250 lost a hop,
  a region under 250 slips free, 6 grain a year per region per hop, 120 to take a region, a hop
  of reach per 400 grain held up to four, 200 per mille lost where the upkeep went unpaid),
  events RegionTaken, RegionSlipped, UpkeepUnpaid, `ReachSystem` (LOD World, after Law: walk the
  region graph from the seat through the polity's own ground, pay the upkeep out of the treasury,
  write the hold of every region it rules, let go what falls under the floor - never the seat -
  then take the unruled edge in region order while the treasury can pay, writing the taken
  region's authority in the same tick), `AuthorityOf`, `ReachOf`, `MeasureReach` (regions held,
  those held firmly, the greatest distance, grain spent and unpaid, events by kind, and the bad -
  authority of a polity that is gone, a hold above the seat's, a ruled region whose authority
  names nobody, an authority disagreeing with the rule - with a digest of every reach in polity
  order then every authority in region order).
- Tests (4): the seat is held whole at no distance and no cost, a treasury buys ground, and the
  hold of everything taken is exactly the rule - one step per hop, never under the floor; a heavy
  upkeep with an emptied treasury makes the far edge slip free while the seat never does, and what
  slipped carries nobody's authority and belongs to nobody; a word never carries past its ceiling
  however rich the polity, a world with no polity has nobody's authority anywhere, the regions
  held equal the regions ruled every year for twelve years running, and two worlds of one seed
  take the same ground in the same year; determinism, snapshot continuity, frozen reach digest
  `b0849188fdca0aae` after 100 years (41 regions held, 152520 grain spent).
- Decision: ADR-0058.

### 07.04 Succession - VALIDATED (headless)

- Delivered: `Succession.h/.cpp` - `PolityLine` (48 bytes: the person the system last saw on the
  seat, rulers seated, interregna, disputed successions, unrest per mille, the claimant the custom
  names next, the ticks of the seating and of the vacancy) on the polity; `SuccessionTypes::Declare`,
  `SuccessionRules` (a claimant is of age at 20; 250 per mille of unrest when the custom is passed
  over, 150 when the seat falls empty, 50 forgotten a year, never more than 600 at once), events
  SeatFellVacant, SuccessionSettled, SuccessionDisputed, `SuccessionSystem` (LOD World, after
  Polities: one walk of the persons finds the eldest living child of age of every sitting ruler on
  both lines of descent, the seat is compared against what the system last saw in it, the passing is
  judged against the claimant the culture's `Descent` custom named, and the unrest fades),
  `ReachSystem::ObserveLine` so that the unrest comes off the hold of every region the polity rules,
  `LineOf`, `MeasureSuccession` (lines, empty seats, troubled polities, the worst unrest, rulers,
  interregna, disputes, events by kind, and the bad - a line disagreeing with the polity's ruler,
  unrest past its ceiling, a claimant who is not alive - with a digest of every line in polity order).
- Tests (4): the line says what the polity says, every passing of a seat is in the log once, and a
  claimant the custom names is alive, of age, and a child of whoever sits on the culture's line;
  striking the ruler out of the world leaves a shock that is felt in every region at once - each held
  under what the distance alone would give - while the seat is never let go, and the world forgets it
  year by year; no unrest ever stands above its ceiling however sharp the rules, a claimant nobody is
  old enough to be means no succession is ever contested, a world with no polity has no line, and two
  worlds of one seed keep the same line; determinism, snapshot continuity, frozen line digest
  `77726480cad71e27` after 100 years (9 rulers seated, 3 settled by the custom).
- Decision: ADR-0059.

### 07.05 Factions - VALIDATED (headless)

- Delivered: `Factions.h/.cpp` - `Grievance` (PassedOver, Neglect) and `GrievanceName`, `FactionInfo`
  (48 bytes: polity, region, claimant, strength per mille, cause, the ticks of forming and ending,
  identity from the world seed) on entities of the new kind `IdKind::Faction`, `RegionPatience`
  (8 bytes: whose neglect, years running) on the region, `FactionTypes::Declare`, `FactionRules`
  (a region held under 500 per mille for 3 years running breeds a faction; it is born at 100 per
  mille, gains 60 a year while aggrieved and loses 80 when answered, takes its region at 500, adds
  40 to the polity's unrest while it stands, never past the ceiling succession keeps), events
  FactionFormed, FactionRevolted, FactionFaded, `FactionSystem` (LOD World, after Reach, last of the
  politics systems so its unrest is felt the year after - a faction is not news the day it forms),
  `FactionOf`, `FactionsOf`, `MeasureFactions` (standing and ended, the strongest, regions counting
  years, events by kind, and the bad - a faction of a polity that is gone, in a region it does not
  rule, a strength past its ceiling, a claimant who is not alive - with a digest of every faction in
  index order then every patience in region order).
- Tests (4): a polity of one region breeds nothing - a seat is not a province - and when it
  overreaches, its far provinces count the years and raise a faction that names no one; left
  unanswered it grows and its region simply leaves, while the seat is never taken this way and no
  faction ever rises in a capital; a grievance with a cause the polity can remove (an upkeep it
  cannot pay) is answered by paying it, and the faction comes to nothing without taking anything;
  a polity bears one faction at a time however many provinces are aggrieved, no strength stands
  above its ceiling, a world with no polity has no faction and nobody's patience, and two worlds of
  one seed raise the same factions in the same years; determinism, snapshot continuity, frozen
  factions digest `2906077fcd8b815d` after 100 years (15 formed, 13 took their region).
- Decision: ADR-0060.

### 07.06 Diplomacy - VALIDATED (headless)

- Delivered: `Diplomacy.h/.cpp` - `Stance` (Pact, Peace, Rivalry, War) and `StanceName`, `Relation`
  (48 bytes: the two polities with A always below B, stance, warmth per mille, border length, the
  ticks of meeting and of turning, identity from the world seed) on entities of the new kind
  `IdKind::Treaty`; `RegionInPlay` (8 bytes) declared by `Reach.h` and written here, so the reach
  system can read it without ever learning what a war is; `DiplomacyTypes::Declare`,
  `DiplomacyRules` (500 per mille at contact; a year warms by 40 for a shared culture and 15 per
  road across the border, and cools by 8 per adjacent region pair and 20 per region the larger
  holds over the smaller; pact at 750, peace at 450, rivalry at 200, with 60 of hysteresis), events
  ContactMade, StanceChanged, RegionContested, `DiplomacySystem` (LOD World, after Factions: count
  who touches whom and how long each border is by walking the region graph in region order, warm
  and cool, turn the stances, and mark the weaker side's border regions in play wherever there is
  war), `RelationBetween` (either way round), `NeighboursOf`, `MeasureDiplomacy` (relations by
  stance, regions in play, events by kind, and the bad - a relation with a polity that is gone, A
  not below B, a warmth or stance out of range, a contested region its supposed holder does not
  hold - with a digest of every relation in index order then every contest in region order).
  `ReachSystem` gains `ObserveContest`, `AnnexCost` and `RegionAnnexed`: ground put in play is
  taken at the border rather than from the seat, so a polity's reach does not bind a war.
- Tests (4): two powers on one world found, grow, meet, and hold exactly one relation for the pair
  whichever side is asked; a long border between unequal neighbours cools to war, the weaker side's
  regions go in play, and the stronger takes them one year at a time, every annexation naming the
  polity it was taken from; with every drift silenced, a stance holds at the bar exactly and turns
  only once the warmth has passed the edge by the hysteresis, a pact survives one bad year, war puts
  ground in play and peace takes it straight back out; one power alone is nothing to anybody and
  annexes nothing, the lookups refuse a pair that makes no sense, and two worlds of one seed keep
  the same relations year for year; determinism, snapshot continuity, frozen treaty digest `9f07d235c61d3712`
  after 100 years (4 contacts, 5 stances turned).
- Decision: ADR-0061.

### 07.07 Politics in the chronicle - VALIDATED (headless)

- Delivered: `PoliticsHistory.h/.cpp` - `PoliticsChronicleRules` (what is recorded: foundings and
  endings, empty seats and disputed successions, laws at a bound, revolts, contacts and the two ends
  of the diplomatic scale, annexations; sixteen records a region a year at most),
  `PoliticsChronicleState`, `PoliticsChronicleTypes::Declare`, `PoliticsContext` (carrying an
  optional `EconomyContext` so the topmost describer speaks for every layer under it),
  `PoliticsChronicle` listener, `NamePolity` ("the polity of Edavaken"), `NameFaction`,
  `DescribePoliticsEvent` giving each of the twenty-one politics events its own sentence after the
  year and the age, `ExportChronicleWithPolitics`, `ExportWhyWithPolitics`, `CheckPoliticsChronicle`.
- Tests (3): every political event of forty years has a line of its own - prefixed by the year and
  the age, never falling back to the generic event text, never naming a bare index where a name was
  meant - while the events of the layers below keep their own words through the same describer; only
  what a century would remember is recorded (1838 political events leave 30 records), a settled
  succession is never among them, every recorded law stood at one of its own bounds, every line of
  the chronicle ends in a full stop, and the why of an annexation runs back through the causes each
  system wrote; determinism, snapshot continuity, and the chronicle text of a restored world
  identical to the original; frozen text digest `2082522228252ab5` after 100 years (80 records, 4301 lines).
- Decision: ADR-0062.

### 07.08 Phase 07 gate - VALIDATED (headless)

- Delivered: `Tests/Politics/Test_PoliticsGate.cpp` - AELVOR at 256 after 300 years of pre-history,
  its two most peopled regions detailed (a world with one polity has no diplomacy, no war and no
  ground that can change hands), and every Phase 04, 05, 06 and 07 system running for 500 years.
  Every decade the invariants of all four phases hold: persons consistent with the counts, no astray
  membership, standing over the living and free only, bonds and strata sound, customs within their
  bounds, every cause resolving in the log, nothing stale or orphaned in the economy, a market
  wherever a stock is, every price within its bounds, every ration at most a full one, no bad road -
  and, new here, no polity ruling from ground it does not hold, no region demanded of by nobody,
  every ruled region carrying the authority of its master, no line disagreeing with its polity, no
  faction in ground its polity does not hold, no relation with a power that has ended, the regions
  held equal to the regions ruled, and every record of all four chronicles described and
  era-consistent. Frozen: world `2ab8cc1ac3511587` at year 250, `3fa464fbbbad5fe6` at year 500,
  log `9dee76991a6bc9c8`, chronicle text `a4e5ee1fb64db8ab`. A snapshot at year 250 restored into a fresh
  world runs to the same year 500, the same log and the same text.
- Found by the gate, and only by running everything at once for five centuries:
  1. A region freed by a slip (07.03) or a revolt (07.05) went on being assessed for a year,
     because the law system runs before the systems that free ground. Whoever takes the ground away
     now cancels the demand on it; what it already owes it still owes.
  2. A polity whose capital was annexed (07.06) went on standing, ruling through a council that sat
     in another realm. The rule that resolves it makes the other three consistent: **a seat cannot
     be taken** - it does not slip for want of hold (07.03), no faction rises in it (07.05), and no
     war puts it in play (07.06). Taking a capital is a siege, and a siege is Phase 08.
  3. A polity that lost its seat inside its founding grace was kept alive by that grace. Losing a
     capital is not a failure to grow, so a seat lost now ends a polity at once.
- Decision: ADR-0063.
## 12. Phase 08 - MILITARY: task breakdown

Broken down on the closing of Phase 07. Phases 09-20 are broken down as each previous
phase closes.

| Task | Content | Test kind |
|---|---|---|
| 08.01 | `VaelenMilitary` module; a levy raised from a polity's people and paid out of its treasury; ArmyInfo on entities of kind Army; MeasureArmies | unit, deterministic |
| 08.02 | Marching: an army moves on the region graph, one hop a season, and costs grain where it stands | unit, integration with 07.03 |
| 08.03 | Battle: two armies in one region resolve by strength, ground and a stream drawn from the world seed; losses in persons where the region is detailed | unit, deterministic, edge |
| 08.04 | Siege: an army before a seat; a capital taken only here, and what that does to the polity of 07.01 | integration with 07.01 and 07.06 |
| 08.05 | War as a thing with a beginning and an end: aims, exhaustion, terms; the stance of 07.06 follows it rather than leading it | unit, integration |
| 08.06 | What war costs the living: the dead, the displaced, the harvests taken, the standing of those who fought | integration with 04.x and 06.02 |
| 08.07 | War in the chronicle: levies, marches, battles, sieges, terms; the why of a lost province | text, deterministic |
| 08.08 | Phase 08 gate: 500 years at 256 with every Phase 04 to 08 system, invariants every decade, frozen digests; Phase 08 closed against section 2 | long-duration |

Every task ends with the usual report block, the docs refreshed and a commit.

### 08.01 Levies and armies - VALIDATED (headless)

- Delivered: `Source/VaelenMilitary` (seventh kernel module: `VaelenMilitary.Build.cs` depending on
  Core and the six kernel modules; `CMakeLists.txt` with the explicit source list; `MilitaryApi.h`;
  the Unreal-facing `VaelenMilitaryModule.cpp` excluded from the headless build; listed in
  `Tools/kernel_modules.txt`, `Vaelen.uproject` and both targets), `Armies.h/.cpp` - `ArmyInfo`
  (48 bytes: polity, where it stands, men, grain given last year, years gone hungry, the ticks of
  raising and disbanding, identity from the world seed) on entities of kind Army, `RegionLevy`
  (8 bytes: who called them up, men away) on the region, `ArmyTypes::Declare`, `ArmyRules` (20 men
  per thousand living at a full hold, nothing from a region held under 300 per mille, 3 grain a man
  a year, a levy under 60 men is not worth calling, 400 per mille lost in a year unfed, three years
  hungry and what is left goes home, one host a polity), events ArmyRaised, ArmyDisbanded,
  ArmyStarved, `ArmySystem` (LOD World, after Diplomacy: send home what has no polity or no war
  left, feed what stands out of the treasury, let what cannot be fed melt, then raise a host where
  there is a war and none stands), `ArmyOf`, `ArmiesOf`, `LevyOf`, `MeasureArmies` (armies standing
  and gone, men under arms, men the regions say are away, regions levied, grain eaten, events by
  kind, and the bad - an army of a polity that is gone, standing in ground it does not hold, with no
  men, or a levy owed to nobody - with a digest of every army in index order then every levy in
  region order).
- Tests (4): a world at peace has nobody under arms however rich; war comes and with it a host that
  stands at its polity's seat, and what the regions say is away is exactly what is under arms, with
  no region giving men it does not have and none held under the floor giving any; a host whose
  treasury is emptied is not disbanded by an order - it melts, its men walk home, and no region is
  left owing men to nobody; a polity at peace never calls anybody up, a polity bears one host at a
  time, the books balance every year for twenty years running, a host outlives neither its polity
  nor its war, and two worlds of one seed call up the same men in the same years; determinism,
  snapshot continuity, frozen armies digest `028ec9ea8f5092f7` after 100 years (2 levies called, 254 men).
- Decision: ADR-0064.

### 08.02 Marching - VALIDATED (headless)

- Delivered: `March.h/.cpp` - `MarchOrder` (16 bytes: the region it is marching on, hops still to
  walk, hops walked since it was raised, whether it is standing on its aim) on the army entity,
  `RegionForage` (8 bytes: grain taken off it last year, consecutive years a host has stood on it)
  on the region, `MarchTypes::Declare`, `MarchRules` (four hops a year - one a season, nothing
  further than twelve hops marched on, 2 grain a man a year taken off the ground, 60 per mille of
  hold cost to a ruler an enemy host is standing on), events ArmyMarched, ArmyForaged, ArmyArrived,
  `MarchSystem` (LOD World, after Armies: a breadth-first walk of the region graph from where the
  host stands gives the nearest enemy ground - ties to the lower region index, so the road is the
  same on every run - then up to four hops of the shortest way towards it, then the host eats off
  the region it ends the year on and loosens the grip of any ruler it is standing on), `OrderOf`,
  `ForageOf`, `MeasureMarches` (hosts under an order, arrived, idle, abroad, hops walked, regions
  foraged, grain taken, events by kind, and the bad - an order left on a host that went home, an
  aim that is not enemy ground, an arrival that is not where the host stands, grain taken off
  ground nobody stood on - with a digest of every order in army index order then every forage in
  region order). `MeasureArmies` relaxed accordingly: where a host stands is the march's business,
  and a marching host is on enemy ground on purpose.
- Tests (4): a host walks no faster than a hop a season and, while its aim does not change, gets
  strictly closer to it every year until it is standing on it; a host eats off the ground it ends
  the year on, ground nobody stood on is not eaten, and a ravenous host strips a region's common
  stock to nothing, so the grain really leaves the world rather than being counted twice; a host
  with no hops to give never leaves the seat it was raised at, one that cannot see past its own
  ground marches on nothing, one that takes nothing takes nothing, a host that went home is under
  no order, and the lookups refuse what does not exist; determinism, snapshot continuity, frozen
  marches digest `ae71515bd66bae56` after 100 years (16 hops walked, 9 marches).
- Decision: ADR-0065.
- Amended by 08.03: the aim rule as first written sent every host to the nearest ground an enemy
  ruled, which is the province on its own side of the border, so two powers were at war for two
  hundred and forty years without their hosts meeting once. A host now marches on the nearest ground
  an enemy host is standing on, read live so that a host that marched earlier the same year is
  marched on where it is now, and falls back on nearest enemy ground only when no host is in reach.
  `MeasureMarches` checks the aim against both. Marches digest refrozen to `2956e5ee3a48ec46`
  (6 hops walked, 2 marches).

### 08.03 Battle - VALIDATED (headless)

- Delivered: `IdKind::Battle`, `Battle.h/.cpp` - `BattleInfo` (64 bytes: the ground, the two polities
  and their two armies, the men each brought and each lost, the winner, what the ground was worth,
  the tick, an identity from the world seed) on entities of kind Battle, written once and never
  changed because a battle is a thing that happened; `BattleTypes::Declare`, `BattleRules` (300 per
  mille at a full hold for fighting on your own ground, a swing of 150 per mille either way from the
  stream, 350 per mille of the beaten host lost and 120 of the winner, a loser left under 350 per
  mille of the winner is gone), events BattleFought, ArmyBroken, ArmyRetreated, `BattleSystem` (LOD
  World, after Marching: in every region where two hosts of polities at war stand, the first pair in
  army index order settles it - strength, the defender's hold, and two draws from a stream fixed by
  the world seed and the tick; the fallen of both sides leave the levies of the regions that gave
  them; a broken host is disbanded where it stands and a beaten one falls back on the nearest
  neighbouring region its own polity rules and loses its marching orders), `BattleOf`, `BattlesIn`,
  `MeasureBattles`. Two supporting changes: `ReleaseLevy` lifted out of `ArmySystem` as the single
  way men ever leave an army - going home, drifting away unfed, or falling - and `WarPairs` lifted
  out of the march, so who is at war with whom is read one way.
- Defect closed in 08.01: a region whose men were already away for one polity could be levied again
  by whoever next held the ground, which wrote over the first claim; when that first army was
  destroyed its men could not be found, and the count of men away stopped matching the count of men
  under arms. Such a region now gives nobody, and every release is checked to the man with an ensure.
- Tests (4): two hosts on one ground settle it, every record is a battle between two powers on ground
  that exists with a winner that was there and neither side losing men it did not bring, the loser
  loses the greater share, every battle ends in a retreat or a breaking and nothing it does breaks the
  books of 08.01 or the orders of 08.02; with no ground and no luck the bigger host wins and a tie
  goes to the side that did not have to come, and with the ground made decisive the bonus is the
  defender's alone; a host that never breaks always falls back, one that always breaks and loses every
  man doing it leaves no region owing men to nobody, and the lookups refuse what does not exist;
  determinism, snapshot continuity, frozen battles digest `7a8c0c2222568ac7` after 100 years
  (75 fought, 2186 fallen).
- Decision: ADR-0066.

### 08.04 Siege - VALIDATED (headless)

- Delivered: `Siege.h/.cpp` - `SiegeInfo` (32 bytes: the polity sitting before it and its host, the
  polity whose seat it is, consecutive years pressed, the wall in per mille, times the seat has been
  stormed, the tick the present siege was laid) on the seat's region, outliving the siege because a
  wall that has been breached stays breached; `SiegeTypes::Declare`, `SiegeRules` (a whole wall is
  1000 per mille, a host under 60 men cannot shut a seat in, a hundred men bring down 250 per mille a
  year, nothing at all comes down the year the host arrives, 120 per mille mended a year when nobody
  is sitting before it), events SiegeLaid, SiegeLifted, SeatTaken, `SiegeSystem` (LOD World, after
  Battles: press every siege a host is sitting on, mend every seat nobody is sitting on, and hand over
  the seats whose walls have gone - belonging written on the region itself, and the authority of
  07.03 reset to nothing so the taker earns its grip back), `SiegeOf`, `MeasureSieges`.
- Tests (4): sieges are laid and lifted, none is ever pressed by nobody, and a seat that falls changes
  hands and its holder is dissolved the next year by 07.01, which is the whole phase joined up - a levy
  called, marched onto the enemy host, a battle, a host destroyed, a capital invested and stormed, and
  a power gone; a wall never rises while a host sits before it, never stands taller than whole, and is
  mended by exactly the rule's amount in a year nobody is there; a host too small never invests
  anything however long it stands, and a seat that can be invested but never breached keeps a whole
  wall through sixty years of war; determinism, snapshot continuity, frozen sieges digest `{SH}`
  after 100 years ({SS} seat stormed, {SL} sieges laid).
- Decision: ADR-0067.

### 08.05 War as a thing with a beginning and an end - VALIDATED (headless)

- Delivered: `War.h/.cpp` - `WarInfo` (64 bytes: the two sides, years run, men each has lost, what it
  has worn each of them down to in per mille, capitals that changed hands in it, the winner, the ticks
  it began and ended, an identity from the world seed) on entities of kind War, outliving the fighting
  because a war that has ended is a thing the world remembers; `WarTypes::Declare`, `WarRules` (60 per
  mille of exhaustion a hundred men lost, 20 a year for the war simply going on, 400 for a capital
  lost, a side worn to 700 takes any terms, two worn to 450 make a white peace, no war ends in the
  three years it began, a peace is written at 450 warmth), events WarBegan, WarEnded, `WarSystem` (LOD
  World, after Sieges: open a war for every relation that has turned to one, hold the stance at war
  while it runs, wear both sides down with what the year cost them, and write the peace when one of
  them has had enough), `WarOf`, `WarBetween`, `MeasureWars`.
- Defect closed, from the adversarial review and reproduced there: a relation outlives the polities in
  it (07.06 keeps it as a record) and 07.06 only ever revisits pairs whose ground still touches, so a
  war stance left standing over a dissolved polity was read as a live war for ever after - the
  survivor kept a host raised and fed it out of its treasury against an enemy that no longer existed,
  which drained the same treasury 07.03 spends on holding provinces. A war ends when a side does, and
  a war stance over a polity that is gone is written back to peace.
- Tests (4): wars open when a relation turns, the count begun is the count running plus the count
  over, a running war holds its relation at war and an ended one leaves it at peace warm enough that
  07.06 will not turn it straight back round; every ended war either lost a side or ran its least
  years and wore somebody past willing, and a war somebody won is one the other side could not go on
  with; a war nobody will agree to stop only ends when a side does, one neither side has the stomach
  for ends the moment it is allowed to, and the lookups refuse what does not exist; determinism,
  snapshot continuity, frozen wars digest `{WH}` after 100 years ({WO} wars over, {WF} fallen).
- Decision: ADR-0068.

### 08.06 What war costs the living - VALIDATED (headless)

- Delivered: `LevyEnd` and the `LevyReleased` event on `ReleaseLevy`, one per region a release touched,
  so that a higher layer can tell men who walked home from men who fell; `Toll.h/.cpp` - `RegionToll`
  (16 bytes: men of the region who did not come back, men who did, people who left, years a war cost it
  anything) on the region, `TollTypes::Declare`, `TollRules` (a host must sit two years before anybody
  leaves, 30 per mille of the region a year after that, a region under forty people loses nobody),
  events WarDead, PeopleFled, `TollSystem` (LOD World, after Wars: the fallen struck out where they
  came from - real persons where the region is simulated person by person, people off the count where
  it is not - the standing of those who came home, and the flight of people who will not live under a
  host), `TollOf`, `MeasureToll`. In VaelenSociety, `PersonService` declared beside the standing it
  weighs, `StandingSystem::ObserveService`, and `StandingScore` given the wars a person has come home
  from - worth something, and only so far.
- Tests (4): men fall, the regions that gave them bury them, every death recorded is a death in a
  region and the sum of what the regions say they lost is what the world says it lost; service is
  worth a fixed amount per war up to a cap and nobody carries more wars than the world has fought;
  people who leave a region arrive in another one and every flight names where it went, a people that
  will not be moved is not moved though it still loses its men, and the lookups refuse what does not
  exist; determinism, snapshot continuity, frozen toll digest `bcb49ab4dbd74565` after 100 years
  (1847 fallen over 39 regions).
- Decision: ADR-0071.

### 08.07 War in the chronicle - VALIDATED (headless)

- Delivered: `MilitaryHistory.h/.cpp`, the shape of 07.07 one layer up - `MilitaryChronicleRules` (which
  kinds are kept, and the marks a toll must pass to be worth remembering: a region losing two or three
  men is not history, a region losing eight in a year is the year it is remembered for),
  `MilitaryChronicleState`, `MilitaryChronicleTypes::Declare`, `MilitaryContext` (every military type
  plus, optionally, the politics context, so that a polity is named by its seat rather than its
  number), `MilitaryChronicle` listener over ArmyRaised, ArmyStarved, BattleFought, ArmyBroken,
  SiegeLaid, SeatTaken, WarBegan, WarEnded, WarDead and PeopleFled, `NameArmy`, `NameWar`,
  `DescribeMilitaryEvent` (a sentence for all seventeen military events, stamped with the year and the
  age in the same hand as every layer below, and falling through to the politics text for everything
  else), `ExportChronicleWithMilitary`, `ExportWhyWithMilitary`, `CheckMilitaryChronicle`.
- Causes chained, so the why of a lost province is a chain and not a single line: a battle publishes
  first and hands its event id to the levy releases it caused and to the breaking or the falling back
  that followed, and a burial in a region names the release that reported the fallen. A burial now
  walks back three steps - "Kratfa buried one of its own, because one man of Kratfa did not come back,
  because the polity of Eva won a battle in Zakru".
- Tests (4): the chronicle keeps what a century would remember and every record has a sentence, sits
  in the era it happened in, and names a region unless it is a war; every one of the four hundred and
  eighty-four military events of a sixty-year war reads as a sentence stamped with its year, a storm
  names both polities, and a burial walks back three steps to the battle that caused it; a chronicle
  that keeps nothing keeps nothing though the war still happens, one allowed a line a region a year
  drops the rest and says so, and a toll too small to be news is not news though the people still
  died; two worlds of one seed write the same chronicle, word for word, frozen at
  `88eee3886fdd6d57` over 241 records.
- Decision: ADR-0072.

### 08.08 Phase 08 gate - VALIDATED (headless), and Phase 08 closed

- Delivered: `Tests/Military/Test_MilitaryGate.cpp` - five centuries at 256 with every Phase 04 to 08
  system running, the two most peopled regions simulated person by person, every invariant of every
  military measure checked each decade along with the Phase 07 ones under it, a snapshot at year 250
  reloaded and replayed to the same year 500, and four frozen digests: state at 250 and at 500, the
  event log, and the whole chronicle as text.
- Four defects the gate found, all closed:
  - A snapshot of year 250 replayed to year 500 gave a different world. The worst kind of defect this
    project can have, and the reason a gate exists.
  - `MeasurePolities` counted a polity whose seat had just been stormed as incoherent. It is not: 08.04
    can take a seat, and 07.01 dissolves what has lost one on its next tick. Split out as `Doomed`.
  - The same for a polity whose ruler had just been killed in a war, which 08.06 can now do to anybody.
    Split out as `Bereft`; `Bad` keeps its meaning, which is incoherence.
  - A host kept a marching order on ground the enemy had just left - beaten and fallen back, destroyed,
    or a capital that had just changed hands. An order to march on nothing. The war, the siege and the
    battle each now clear the orders they invalidate.
- Phase 08 closed against section 2: all six Linux presets green with every gate (92 CTest entries
  each), purity 136 files 0 violations, clang-format clean, every file of the phase VALIDATED, every
  system with unit, integration, deterministic, edge and long-duration tests, and an ADR for every
  architecture decision (ADR-0064 to ADR-0073).
- Decision: ADR-0073.

## 12b. Phases 09-20: notes

 Fixed points already in the code: `IdKind` values for Region, Tile, River,
ResourceDeposit (Phase 02), Culture, Language, Religion, Person, Family, Organization
(Phases 03-05), Item, Building, Settlement, Market, Route (Phases 06, 09), Polity, Law,
Army, War, Faction, Treaty, Battle (Phases 07-08), Document, Map (Phase 12); `VAELEN_SAVE_FORMAT_VERSION`
(Phase 16); `Config/DefaultEngine.ini` and `DefaultInput.ini` note that the game engine
class and Enhanced Input mappings arrive in Phase 10.

## 13. Phase 09 - INFRASTRUCTURE: task breakdown

Broken down on the closing of Phase 08. Phases 10-20 are broken down as each previous
phase closes.

What already exists, and what this phase is therefore not: 06.04 gave the world routes
between regions and settlements where they meet, as artefacts of trade - a route opens
because a price gap opened and closes because nothing crossed it. 05.05 gave councils a
granary as a number on a region. Phase 09 does not replace either. It gives the world the
things people actually build and keep - buildings that cost goods and labour and decay
when nobody minds them - and makes the routes and the granaries of the earlier phases the
first two examples of it.

| Task | Content | Test kind |
|---|---|---|
| 09.01 | `VaelenInfrastructure` module; `BuildingInfo` on entities of kind Building - what it is, where it stands, who raised it, what it cost, and its state of repair; raised out of the goods of 06.01 and the labour of the people; `MeasureBuildings` | unit, deterministic |
| 09.02 | What a building does: a granary that holds grain against a famine (folding in the `RegionStores` of 05.05), a mill that lifts the harvest of 06.02, walls that raise the wall of 08.04. A building is the reason a number moves, never a number of its own | integration with 05.05, 06.02 and 08.04 |
| 09.03 | Settlements as places rather than as marks on a trade route: a size that grows with the people and the traffic, the buildings it holds, and the first thing on the map that is smaller than a region | integration with 06.04 and 04.x |
| 09.04 | Roads as things that are built and kept: a route of 06.04 becomes a road with a state, built with goods and labour, cheapening what crosses it, and falling back to a track when nobody keeps it | integration with 06.04 |
| 09.05 | Decay: everything built falls down unless it is kept, at a rate set by what it is, by the weather of 02.x and by the wars of 08.x; ruins as a thing the world remembers and can build on | unit, long-duration, edge |
| 09.06 | Logistics: what a road is worth to an army (08.02 marches further on one and eats less beside it) and to a polity (the word of 07.03 carries further and costs less along one) | integration with 07.03 and 08.02 |
| 09.07 | Infrastructure in the chronicle: a granary raised, a road cut, a mill fallen in; the why of a famine that a granary would have stopped | text, deterministic |
| 09.08 | Phase 09 gate: 500 years at 256 with every Phase 04 to 09 system, invariants every decade, frozen digests; Phase 09 closed against section 2 | long-duration |

Every task ends with the usual report block, the docs refreshed and a commit.

The rule the phase is written to: **a building is never a number the simulation reads
instead of the world.** A granary does not make a famine less likely; it holds grain, and
the famine of 06.02 finds the grain there. A road does not make an army faster; it lowers
the cost of a hop that 08.02 already walks. Anything that cannot be expressed as a thing
that holds, lowers or raises something an earlier phase already computes does not belong
in this phase.

## 14. Phase 10 - PLAYER: task breakdown

Broken down on the closing of Phase 09. Phases 11-20 are broken down as each previous
phase closes.

What this phase is not. The player is not a new kind of thing. Nine phases have built a
world of people who are born, eat, work, are bound and freed, marry, hold office, march
and die, and the player is ONE OF THEM - a marker on an existing person of a detailed
region, not an entity with its own rules. Everything the player does goes through a
system that already owns that part of the world; nothing is written into the world from
outside the simulation. That is what makes a replay of a played life possible at all, and
it is the whole architectural content of the phase.

| Task | Content | Test kind |
|---|---|---|
| 10.01 | `VaelenPlayer` module; the player as a marker on one existing person (04.01) of a detailed region, with the rule that removing the marker leaves a world that runs on unchanged; `MeasurePlayer` | unit, deterministic |
| 10.02 | The enslaved start: the player begins bound (05.04), on the ground of the mining colony, with the standing 05.02 gives someone in that condition and the family 04.03 gives them - a place in a world, not a character sheet | integration with 04.03, 05.02, 05.04 |
| 10.03 | The player's grain: the person the player is runs at the finest LOD the scheduler has, while the world around them runs at the year; what that costs and what it must not change | unit, deterministic, edge |
| 10.04 | Intent as commands: a queue the player submits to, validated against the world and applied by a system INSIDE the simulation. Nothing is ever written into the world from outside, so a recorded command stream replays to the same life | unit, edge, deterministic, replay |
| 10.05 | What the player can do: work, rest, eat, move, speak, give, take - each a command that costs time and changes the world only through the system that already owns that change | integration with 04.04, 06.01, 05.04 |
| 10.06 | What the people around the player make of them, built from what the player actually did (the event log) and from the standing of 05.02 - never from a dialogue tree | integration with 05.02, 04.03 |
| 10.07 | The player in the chronicle: a life as records, and the why of anything that happened to them walked back through every layer below | text, deterministic |
| 10.08 | Phase 10 gate: a world at 256 with the player placed and driven by a recorded command stream for a lifetime, every invariant of every phase each decade, frozen digests, and the command stream replayed to exactly the same life | long-duration, replay |

Every task ends with the usual report block, the docs refreshed and a commit.

The rule the phase is written to: **the player is a person the world already had.** Any
command that cannot be expressed as something a person in that world could do, applied by
the system that already owns it, does not belong in this phase. And the test that keeps
it honest is the replay: a recorded stream of commands, applied to the same seed, gives
the same life. A player who can write to the world from outside it cannot be replayed,
and a world that cannot be replayed is not this project.

The engine-facing part (the game engine class and the Enhanced Input mappings the
`Config/` files already note for this phase) stays UNVERIFIED until the Phase 13
presentation work, as every engine-facing file in the project has since Phase 00.

## 15. Phase 11 - MINING COLONY: task breakdown

Broken down on the closing of Phase 10. Phases 12-20 are broken down as each previous
phase closes.

What this phase is. Ten phases have built a world that runs at the year, with two regions
at a time simulated person by person because that is what a chronicle needs. The starting
place of the game is not that: it is one huge colony, simulated at full detail
continuously, with hundreds of people whose work, hunger, standing and quarrels are all
individual - and it has to run at that detail while the rest of AELVOR keeps its yearly
grain around it. Nothing new is invented for it. It is the same systems at a different
setting, which is the claim to be tested rather than asserted.

Three things learned in Phase 10 point straight at this phase. The scheduler's finer
grains work and cost what they should (10.03), but they have only ever run for ONE
person; a colony wants them for a region. The LOD bridge will take a person out of the
fine grain when it feels like it (the hold of 04.06, added when the gate found the played
person emigrated); a colony needs that decided by policy rather than by crowding. And a
bound person of a crowded region dies young (10.08), which is either this world being
harsh or the ration of 04.04 being wrong at that density - the colony is where that gets
settled.

| Task | Content | Test kind |
|---|---|---|
| 11.01 | The colony as a region the world keeps detailed: a LOD policy that holds it there rather than letting crowding decide, and what that costs per tick measured honestly | unit, deterministic, performance |
| 11.02 | A colony's people at the day: the finer grain of 10.03 for a whole region rather than one person, with the cost of it measured against the yearly world around it | integration with 10.03, long-duration |
| 11.03 | The work of a colony: ore out of the ground through 06.01 and 09.02, the deposits of 02.07 depleting as it is taken, and what a colony eats that it does not grow | integration with 06.01, 09.02, 02.07 |
| 11.04 | Who holds it: the bondage of 05.04 at colony scale, the overseers of 05.01, and the standing of 05.02 in a place where everybody's rank is the same work | integration with 05.01, 05.02, 05.04 |
| 11.05 | What a colony does to the people in it: the ration of 04.04 at that density, sickness, and whether the Phase 10 finding (a bound person dies young) is the world being harsh or the model being wrong | integration with 04.04, edge |
| 11.06 | The colony and the world: what it sends out, what it needs in, and the roads of 09.04 that carry both | integration with 06.04, 09.04, 09.06 |
| 11.07 | The colony in the chronicle, and the player's place in it: the start of 10.02 on the ground it was always meant for | text, integration with 10.02, 10.07 |
| 11.08 | Phase 11 gate: the colony at full detail for a century with the world around it at the year, every invariant of every phase each decade, frozen digests, and a played life inside it replayed | long-duration, replay |

Every task ends with the usual report block, the docs refreshed and a commit.

The rule the phase is written to: **the colony is the same systems at a different
setting.** Any behaviour it needs that cannot be had by configuring what Phases 04 to 10
already do belongs in one of those phases, as a rule they were missing, and not in a
special case for the colony. A colony with its own economy would be a second simulation,
and the project has spent ten phases not building one of those.

## 15b. Phase 11 closed

Closed against section 2 on 2026-09-09: CI run 113 green on all nine jobs
(six Linux presets, clang-format, Windows MSVC, macOS AppleClang) at 6be319f;
the eight phase gates green locally in one run (GATES-DONE 0 failing); purity
173 files, 0 violations; determinism tests for every system of the phase and
frozen digests on the gate; no file carrying STATUS: INCOMPLETE; ADR-0090 to
ADR-0095 for every architecture decision.

What the phase established, one line per task:

| Task | What it settled |
|---|---|
| 11.01 | A region the world keeps detailed by policy rather than by crowding, and what that costs, measured |
| 11.02 | Its people live at the day while the world keeps the year, and a year of days sums to the year exactly |
| 11.03 | They lift ore from a seam that runs out, credited through `AddStock` and nothing else so the chronicle can tell it |
| 11.04 | Founded bound, because bondage cannot GROW into a colony, and held by the colony because no elite could hold that many |
| 11.05 | The bond does not shorten a life; the ground does |
| 11.06 | The world cannot feed a colony - AELVOR is a subsistence world - so a colony feeds itself |
| 11.07 | What the world does with no event behind it cannot be remembered |
| 11.08 | A gate has to be full of what it claims to measure |

The two rules the phase paid for and wrote down:

- **A rule is fixed when the system is built; a fact has a date.** Anything that
  BEGINS during a world's life must be a component the system finds by looking.
  Paid for three times before it was written: `ProductionRules::MinedRegion`
  starved a region through three centuries of pre-history, `LodRules::Held`
  emptied the region it was meant to keep, and `MiningRules::Region` mined
  nothing because a harness cannot know a region before it builds its systems.
- **A gate has to be full of what it claims to measure.** Read the volume before
  the verdict. Paid for twice: the Phase 10 gate spent three quarters of itself
  refusing intents at a corpse, and the Phase 11 gate spent nine tenths of its
  century with nobody played. Both passed their first run.

The thirteen PROTOTYPE files of the phase, and their limits, as section 2
requires them to be named:

`Source/VaelenColony/{ColonyApi.h, Mining.h, Mining.cpp, Holders.h,
Holders.cpp, ColonyHistory.h, ColonyHistory.cpp}` and
`Tests/Colony/{Test_Mining, Test_Holders, Test_Toll, Test_Supply,
Test_Chronicle, Test_ColonyGate}.cpp`.

- **A colony is founded by a caller, not by the world.** Nothing in AELVOR
  decides to send people to the ore. The phase gives a colony everything it
  needs to BE one and nothing that makes one happen; that belongs to Phase 12
  or to a scenario.
- **Overseers are not unseated when a sitting member is later bound.** Measured
  at two of twenty-four after three years. Whether an institution should is a
  Phase 05 question, and the test measures the drift rather than asserting it
  away.
- **A colony's supply is its own fields.** 06.04 carries about a fiftieth of
  what a colony eats. A colony provisioned from outside would need the dues of
  07.02, which is Phase 12.
- **`MeasureHolding` walks every person of a region on each call.** Fine for a
  test and on no tick path, but it is not a runtime query.

Two defects the pre-push re-read found, neither reachable by any test in the
phase, both fixed before the push: the coarse branch of 06.02 still zeroed a
mined region's harvest (the 11.06 defect surviving in the half that had not
been fixed, unreachable because 11.01 holds a colony detailed), and
`Society::BindPerson` accepted a Holder equal to the person.

## 16. Phase 12 - GAMEPLAY: task breakdown

Broken down on the closing of Phase 11. Phases 13-20 are broken down as each
previous phase closes.

What this phase is not. 10.05 already gave seven verbs - work, rest, eat, move,
speak, give, take - and every one goes through the system that owns the change.
Reading before writing showed something more: `Doings::Do` takes a PERSON INDEX,
not "the player", so the verbs are already anybody's. What is the player's is
the plumbing around them, the intent queue and `PlayerOrderSystem`.

So this phase does not add a second way to act. It adds the two things a player
has that a simulated person has never needed: a head that holds less than the
world does, and a name that travels further than they do.

A first reading of this said the world was full of omniscience: thirty-one files
read `Log().All()`, and two of them - `Regard.cpp:95` and `Decisions.cpp:91` -
decide what somebody believes. Reading FURTHER, past the line that opens the
loop, says the opposite, and the correction is the more useful finding.

`Regard.cpp` walks back only to the current tick and stops, takes only acts
AIMED at somebody (work and eating are "nobody else's business" in its own
words), and records the opinion on the person who was acted upon. That is
already exactly "what reached them". `Decisions.cpp` reads droughts within a
memory window - a council knowing the weather in its own region.

So nobody in AELVOR is omniscient. The gap is the other way round, and it is
narrower and more interesting: **an opinion exists only about the played person**
(`PlayerRegard` lives on the played person, and 12.01 has just given unplayed
people acts of their own), and **nothing travels**. An opinion moves only between
the two people involved; nobody has ever heard anything second-hand. A
reputation is exactly what a world gets when what happened to one person reaches
a third, and this world has no way at all for that to happen.

| Task | Content | Test kind |
|---|---|---|
| 12.01 | `VaelenGameplay` module; a way for a person nobody is playing to act, using the verbs of 10.05 unchanged - what decides an unplayed person's intent is the phase's real question; `MeasureVerbs` | unit, deterministic |
| 12.02 | An opinion between any two people, and hearsay: what one person tells another about a third. 10.06 gives an opinion only about the played person and only to whoever was acted upon; this is the same idea for everybody, plus the one thing no layer has - something heard rather than suffered | unit, edge, integration with 10.06 |
| 12.03 | Documents: a thing that carries knowledge between people, outlives them, is copied, is lost, and is wrong when its writer was | integration with 03.07, 12.02 |
| 12.04 | Maps as the one document the world can check: what a person believes about ground they have not walked, and how it is wrong | integration with 02.x, 12.03 |
| 12.05 | Reputation beyond the region: 10.06 gives what the people around somebody make of them; this carries it on the roads of 09.04 and decays with distance and time | integration with 10.06, 09.04 |
| 12.06 | Consequences: what a polity, an organisation or a family does about somebody whose repute has reached them, through the systems that already act (07.02 dues, 05.05 decisions, 05.04 bondage) | integration with 05.05, 07.02 |
| 12.07 | Gameplay in the chronicle: what was known, by whom, and when - the why of a decision walked back to what its maker believed rather than to what was true | text, deterministic |
| 12.08 | Phase 12 gate: a played life at 256 inside the colony with knowledge, documents and repute, every invariant each decade, frozen digests, replayed - and FULL: 14400 intents, not 1081 | long-duration, replay |

Every task ends with the usual report block, the docs refreshed and a commit.

The rule the phase is written to: **a person acts on what they believe, and the
world acts on what it has heard.** Anything that lets a player read the world's
true state, or lets the world react to something nobody could have told it, does
not belong in this phase. That is what makes a document worth forging and a
reputation worth minding, and it is the first time in twelve phases that the
simulation and somebody's picture of it are allowed to differ at all.

## 16b. Phase 12 closed

Closed against section 2 on 2026-09-10. Eight tasks, ADR-0096 to ADR-0103, an
eleventh kernel module (`VaelenGameplay`), no file carrying `STATUS: INCOMPLETE`,
purity 188 files and 0 violations, and the ten phase gates green in one run.

**The gate, and it is full.** A century at 256 with the mining colony at full
detail and its people living lives of their own; forty years played a day at a
time inside it, then replayed into a fresh world of the same seed:

| | |
|---|---|
| played intents | 14400 over two lives - one on every one of the 14400 days |
| acts in all | 327267, of which 67769 given and 1175 taken |
| opinions | 6939 people thought of, 55074 opinions held, 81911 tellings |
| the scale | best 175, worst -92 |
| names | 96 carried in 12 places, 88 of them from abroad, 3 roads at the furthest |
| judgements | 21 condemned, 1 pardoned, 10 still bound |
| chronicle | 82647 events of belief, every one with a sentence |
| replay | 14400 of 14400 intents answered identically; state, log, mining, fame, repute, judgement and belief digests all matched |

**The eight PROTOTYPE files and what each of them does not do**, as section 2
requires:

| File | What it is | The limit |
|---|---|---|
| `Living.h/.cpp` | a person nobody plays, acting through 10.05's own verbs | Speak, Give and Take only. Work, Eat and Rest are done in aggregate by 06.02 and 04.04 and doing them here would count a life twice (ADR-0096) |
| `Repute.h/.cpp` | an opinion between any two people, and hearsay | eight holders per person, oldest evicted; and `Repute` is their AVERAGE, which is why a name is not built on it (ADR-0100) |
| `Documents.h/.cpp` | knowledge that outlives its teller, is copied, is lost, is wrong | a document never changes its mind; nothing forges one by itself |
| `Maps.h/.cpp` | the one document the world can check | eight regions to a head, and walked ground pushes read ground out and never the reverse (ADR-0099) |
| `Fame.h/.cpp` | a name held by a place, travelling the roads of 06.04 | eight names to a place, dropped rather than faded - a per-year decay was written and could not be reached (ADR-0100) |
| `Judgement.h/.cpp` | what a place does about a name | bondage only. 07.02's dues are laid on a region and not on a person, so a polity cannot charge a thief more without an axis this world does not have (ADR-0101) |
| `GameplayHistory.h/.cpp` | the chronicle of what was BELIEVED | every chain measured is two steps, because nobody in AELVOR ever moves and a place only judges people standing in it (ADR-0102) |
| `Test_GameplayGate.cpp` | the phase gate | the living rate is 20 in a thousand rather than 12.01's 200, the one rule of the phase the gate moves, so that a colony of four thousand does not make eleven million acts |

**The one thing this phase built and could not feed.** `Player::Intent::Move`
exists, 12.01 does not use it, and until somebody can earn a name in one place
and stand in another, nobody is ever judged on a rumour. The machinery is built,
wired, chronicled and measured; the world gives it nothing to carry. That is the
first question Phase 13 or a later gameplay phase should be asked.

## 17. Current BUILD STATUS

```
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
VAELEN BUILD STATUS
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

PHASE       : 13 — PRESENTATION — KERNEL HALF DONE (12 of 12) · ENGINE HALF STARTED (13.06 of 3)
TASK        : 13.08f done — the viewer page is checked by CTest, not by me looking at it
CI          : run 148 on 910a99d — 9 of 9 GREEN. ADR-0120's 44 re-frozen digests
              hold on gcc, clang, AppleClang AND MSVC, in debug and release.
              They were recorded from linux-gcc-release alone.
DECIDED     : ADR-0120 FIXED (and it was three fixes) · ADR-0118 decided, unchanged
OPEN        : ADR-0111 nothing eaten · ADR-0129 a road cannot be abandoned
13.07c      : COMPILES. 877 lines written with no engine, built first try on
              UE 5.6.1 / MSVC 14.44: 14 modules, 167 actions, Succeeded, 7m25.
              Still NOT SEEN - the exit criterion is a screenshot, and compiling
              only proves it is the shape of a program.
DETERMINISM : and that build gave the best result of the day. ADR-0120 changed
              the road network on purpose; the editor and the headless tool then
              agreed on ELEVEN values at two sizes, roads included - 77 at 256
              where both said 78 this morning. Two builds of the same code
              agreeing proves the seed is the seed. Both halves moving together
              when the code changes underneath them is the actual property.
              ADR-0130.
STATUS      : PROTOTYPE (headless) / VALIDATED (UE 5.6, compiles, links and runs)

PROGRESS
█████████████████████████████ 13 of 21 phases closed (00-12) · Phase 13 kernel half done

CURRENTLY
→ The kernel half of Phase 13 is finished: 13.01 to 13.05, VaelenView as the
  twelfth kernel module, ADR-0104 to ADR-0108, View 18/18 and 215 checks under
  gcc-debug, gcc-release and clang-release.

    a century at 256 — 55177 people becoming 110474
    360 frames watched a day at a time — 12 carrying anything, 45 regions at most
    35432 bytes of delta against 2560320 of frames — fourteen per thousand
    the screen matched a fresh frame on ALL 360 days, not just the last
    50.9 s, the cheapest gate in the project: the view reads and never simulates

  The screen is checked every single day and not once at the end, because a delta
  that is right 359 times and wrong once shows the wrong world on the 360th and
  never recovers. ADR-0108.

→ An open question put to a person rather than decided alone. The chronicle can
  say what a region GREW and not what it ATE: measured, a region harvested 7329
  units of grain, its stores rose by six, and the log names none of the other
  7323. 06.02 writes stocks directly for spoilage and meals and publishes only
  Harvest and Shortfall. Routing it through AddStock is cheap in itself and
  moves the EVENT LOG DIGEST, which is frozen in eleven gates - a deliberate
  single-pass re-freeze, and somebody's call rather than mine. ADR-0111 and
  Tests/Economy/Test_Ledger.cpp have the numbers.

→ **13.06 IS DONE AND THE PROJECT HAS NO UNVERIFIED FILE LEFT.** The first
  UnrealBuildTool build of the whole kernel: 157 actions, 92.52 seconds, MSVC
  19.51 under UE 5.6, thirteen modules linked as DLLs, Result: Succeeded. The
  fourteen engine-side files that had carried STATUS: UNVERIFIED since the phase
  they were written in have been read by the toolchain they were written for.
  Nothing RAN - that is 13.09's gate - but the code the engine will compile,
  the engine has now compiled.

  It also refuted me inside an hour. I predicted a hollow build from four-phase
  stale .Target.cs files; UBT linked all twelve modules anyway because it builds
  what the uproject declares. ADR-0112 is rewritten against the log. The fourth
  defect this session found where this machine cannot look - the first three
  needed a wider CI matrix, this one needed a toolchain that does not exist on
  Linux at all.

→ **13.07a IS DONE, AND IT MOVED THE DAILY LOOP OFF THE ENGINE.** Two things,
  and the first is a finding rather than a file. `AVaelenAtlasActor` reads
  `Map.GetLayer(Layers.Biome)` straight out of the world because **a WorldView is
  regions and the coastline is not in it** — ninety-nine cells against six
  thousand four hundred and fifty-nine land tiles. Presentation cannot be asked
  to stop reaching past a boundary the boundary does not carry. So the view got
  the ground: `Vaelen/View/Land.h`, eight bytes a tile, no handle, no way back.
  ADR-0115.

  And `Tools/Atlas`: AELVOR generated, run and written out as JSON with no
  engine anywhere, built by all nine CI jobs and run by four CTest entries that
  read the file back rather than trusting an exit code. ADR-0116.

    Linux, GCC : year 420, 6459 land tiles, 65 peopled, 36374 living
    Windows, MSVC, inside UE 5.6 : year 420, 6459 land tiles, 65 peopled, 36374 living

  Determinism has been asserted by tests since Phase 00. That is the first time
  it has been WATCHED across the engine boundary, on two operating systems and
  two compilers.

→ **13.07b IS DONE, AND AELVOR HAS BEEN LOOKED AT.** `Tools/Viewer/Atlas.html`
  draws the world from the JSON of 13.07a and from nothing else - relief by
  slope shading, biomes, rivers, the shelf, region borders, settlements, every
  tile readable on hover. The first screenshot of this project was cubes on a
  plate. This is a continent with ice caps, mountain ranges that run, biome
  bands by latitude, an eastern archipelago and fifty-two towns on it. The open
  question about the generator is answered: **the terrain has shape.**

  It is NOT `VaelenPresentation` - that is now 13.07c, engine-side - and
  ADR-0113's C4251 decision stays open, because MSVC never reads a web page.

→ And the equilibrium's "84 of 126 regions peopled" is not a third of the world
  lying empty. The forty-two that never take anyone have a **median size of 3
  tiles** against 301 for the rest - they are offcuts of the region partition
  along the coast, not bad land. Any per-region statistic on the page is diluted
  by them: "55 peopled of 126" says 55 of the 84 regions that could ever hold
  anyone. No threshold is introduced to make the number read better; the
  observation is written down instead. ADR-0127.

→ **AELVOR SETTLES, AND HOLDS FOR NINE HUNDRED YEARS.** Every gate stops at four
  or five centuries; nothing had ever run this world further. Fifteen centuries
  at 256, in 275 s, with assertions on:

    year  250    31 872 alive, 22 regions peopled, 36 towns
    year  600   202 373 alive, 84 regions peopled, 60 towns
    year 1500   203 791 alive, 84 regions peopled, 65 towns

  Population climbs for six centuries and then holds within three per cent for
  nine hundred years. Nothing was capped: it is a carrying capacity emerging
  from land, harvest, hunger and death. One seed, so not a gate - but the first
  thing this project has ever learned about its own long run. ADR-0126.

  **And it corrected me inside the hour.** ADR-0118 had ended on "the towns of
  AELVOR are mostly, and always were, empty", built entirely on measurements
  taken at year 420. At year 1500 not one town stands in an empty region: the
  abandonment rule does clear them, it just takes centuries. What survives is
  narrower and still real - traffic founds a town without asking whether anyone
  lives there - and the defect is self-limiting. Every claim made from a gate is
  a claim about a YOUNG world, and nothing inside the data says so.

→ **ADR-0111 IS NOT ONLY ABOUT A DIGEST.** With the chronicle readable, the gap
  can be stated in the world's own words. This is a complete explanation, as
  AELVOR gives it:

    Year 202: Grain could not be had in Iarist.
       because  Iarist harvested 1085 of grain.

  A famine explained by a harvest, and the chain stops there. Across everything
  the world remembers and every cause behind it - 8595 sentences - "harvested"
  appears 53 times and "could not be had" 159, and **eaten, consumed, spoiled,
  rationed and stored appear zero times.** Not one sentence in the memory of a
  four-century world says anything was ever eaten. Routing 06.02's spoilage and
  meals through `AddStock` is what would close that link.

→ **ADR-0118 IS BIGGER THAN IT LOOKED, AND IT IS THE OTHER HALF.** The six
  settlements standing in empty regions were the question "why can they not
  die". The chronicle answers one nobody had asked - how were they BORN:

    Year 405: The town of region 14 rose on the traffic of its roads.

  Counted over four centuries at 256: **121 towns founded, 100 of them in a
  region with no inhabitants at all**, 75 empty both before and after, 15 where
  anybody lived. Forty-four regions had a town rise more than once, one of them
  four times. Traffic founds a town and traffic keeps it; nobody is asked at
  either end. The six at year 420 are not the anomaly - they are the residue of
  a process that has been doing this for four hundred years, and the towns on
  the map of AELVOR are mostly, and always were, empty.

  It does not decide the four answers; it changes what they are worth. "Rename
  it" stops being about a word - the world would have 121 caravan stops and 15
  towns. "Require people" stops being a one-line fix - it would remove most of
  the towns this world has ever had.

→ **AND THEN ADR-0120 WAS MEASURED RATHER THAN ESTIMATED.** The ADR said the
  fix was small and that applying it would move digests frozen in eleven gates.
  That was a guess, and a decision owed by the owner should not rest on a guess.
  So the fix went into the working tree, the whole suite ran, the world was
  written out before and after, and the tree was reverted to the byte. **Nothing
  was committed - the decision is still not mine to take.**

  What it does, at 256 over 420 years: 317 route entities become **184**, twins
  **131 pairs to zero**, and the 317 chronicled first openings become 184, of
  which **none is false where 131 were**. Everything about people is unmoved to
  the digit - freed 6275, enslaved 5379, died 2466, married 1327, living 206710.

  What it costs: **26 of 155 tests fail**, and twenty-four are frozen constants,
  mechanical to re-freeze. The other two are the finding.

    Test_EconomyHistory.cpp:494  VT_CHECK(S.Records < Harvests / 4)
    Test_PlayerGate.cpp:1574     VT_CHECK(Lives > 1)

  The first says **the one-line fix is necessary and not sufficient.** With
  twins, every reopening was a new entity and so read as a first opening -
  falsely, but symmetrically with the closings. Fix the twins and the symmetry
  goes: a road is now said to open once and to fall out of use 1395 times. So
  ADR-0120 also asks 06.07 a question nobody has asked it: **is a road reopening
  history?**

  The second is sharper. `PlayerGate` asserts *"the world's mortality really did
  end a life and start another"*. With the fix it does not - one played life
  spans the forty years. Nothing about mortality changed; the roads changed, so
  the food changed, so the bound person lived. That assertion is either an
  invariant of the design or an accident of the seed, and until now nobody had
  to say which.

  **And the blast radius is exactly the layering.** The gates that move are
  ECONOMY, POLITICS, MILITARY, INFRASTRUCTURE, COLONY, PLAYER, GAMEPLAY, VIEW.
  The ones that do not are HISTORY, POPULATION, SOCIETY - every phase below 06.
  A change in `TradeSystem` reaches everything above it and nothing beneath it.
  The layering rule of this project, holding under a real change instead of in a
  diagram. Full measurement in ADR-0120.

  One correction to the paragraph below: at 128 the same fix moves the living
  from 45535 to 45544. Nine people. The 256 run happened to cancel; the fix is
  **not** people-neutral, and saying it was would have been the comfortable
  reading rather than the true one.

→ **ADR-0120 IS WORSE THAN IT LOOKED, AND THE CHRONICLE PROVED IT.** 06.07
  records a road's opening only when `Openings <= 1` - a first building is
  history, a reopening is not. A twin is a NEW entity, so it starts at one, so
  its opening is chronicled as a first. Everything AELVOR says about one road:

    Year 0:   The road from Miogu to Yiotur was opened.
    Year 385: The road from Miogu to Yiotur was opened.

  Opened for the first time twice, with nothing in between about it closing.
  The chronicle claims 275 roads opened for the first time; the world contains
  170 pairs ever linked; **105 of those claims are false.** Thirty-eight per
  cent of the road history of this world is untrue - not missing, untrue. That
  is a different order of defect from a wasted entity.

→ And two suspected defects found in the same ten minutes were NOT defects: the
  chronicle does record road closings (it says "fell out of use", 541 of them,
  and the grep was wrong), and roads opening in "year 0" are the first year of
  the run, not a stopped clock. Both settled in under a minute. ADR-0125 keeps
  the lesson, because having just been right about two real duplicate defects is
  exactly the state of mind in which a third gets invented.

→ **13.08f IS DONE: THE PAGE IS CHECKED BY CTEST, NOT BY ME LOOKING AT IT.**
  `Tools/Viewer/Atlas.html` is 1044 lines and was, until today, the only file
  in this repository nothing compiled and nothing tested. It broke three times
  on the day it was written: a name declared nowhere, a `getElementById` for an
  id the markup spelled differently, and a literal U+2009 where an entity
  belonged - the mojibake the user saw and reported. All three are silent until
  a human opens the page.

  `Tools/check_viewer.py` reads it without a browser. Seven rules; all three of
  the day's real bugs replayed against it and caught, each named precisely -
  rule 4 reports `['\u2009']`. Three CTest entries: the template, the rules'
  own self-test against ten deliberate breaks, and `Viewer.Built`, which runs
  the builder over a world the atlas wrote and checks the *result*, because a
  sound template can still be inlined into a broken page.

  **What it does not catch is the part worth writing down.** Rule 6 finds a
  name declared nowhere; it does not find one declared in another function's
  scope. That is exactly the `html is not defined` bug, and I said this checker
  would have caught it. It would not. Catching it needs a real JavaScript
  parser, which is a larger dependency than the page it would guard. The limit
  is stated first in the file's own header, ahead of the rules, so the next
  person reads it before trusting the green. ADR-0128.

→ **13.08e IS DONE: THE WORLD SAYS WHY.** Every event has carried a `Cause`
  since Phase 03 and `CauseChain` has been able to walk it since. Nothing had
  ever asked. `--why` asks:

    Year 342: Grain could not be had in Zakru.
       because  Zakru harvested 102 of grain.
         because  a drought struck Zakru.
           because  omens of drought were seen over Zakru.

    Year 325: the age of Oldiss ended.
       because  a terrible eruption struck Wadumfu and 489 died.

  **An age of the world ended because a volcano killed four hundred and
  eighty-nine people.** Four systems wrote those lines and none knew about the
  others. The chain is walked over the LOG and not the chronicle, because a
  cause is often a thing nobody thought worth remembering - the harvest behind
  the hunger is not history, the drought behind the harvest is. ADR-0124.

  It also settled a suspected defect the honest way. Pairs at year 0 and 14
  identical later lines looked like ADR-0120's twin roads; emitting each
  record's event id gave 8158 ids for 8158 lines, all distinct. Nothing is
  recorded twice - the year-0 pairs are two events and the chain says the second
  is BECAUSE the first, and the 14 others are different events that read alike
  because two people were given the same generated name. The cost of checking
  was one field in a file; the cost of not checking would have been a third ADR
  about duplicates that were never there.

→ **13.08d IS DONE, AND IT WAS NOT NEW WORK.** Phases 04 to 11 each built a
  chronicle listener and a describer that puts a sentence on an event in the
  words of the world. Every one is tested, every one is green in eleven gates,
  and until today nothing outside a test had ever read one. Three listeners and
  one describer later:

    Year 0, age of Divik: the Oldegedim first settled Edavaken.
    Year 21, age of Ubu: a great eruption struck Miogu.
    Year 21, age of Ubu: The road from Kiodanam to Entam was opened.
    Year 419, age of Okerdun: Kudihumho was enslaved by debt in Edavaken.
    Year 419, age of Okerdun: Arord was freed by manumission in Edavaken.

  AELVOR at 256 remembers 8158 things over four centuries, and the tally is the
  finding: freed 1697, died 1628, enslaved 1473, married 1114, roads 937, towns
  71. **After death, bondage is the most recorded fact of this world** - 3170
  entries about people being owned and ceasing to be owned. Nobody wrote that.
  The premise in the README is not a story laid over the simulation; it is what
  the simulation does most. ADR-0123.

  And the digests do not move with `--chronicle` on: the chronicle observes
  without touching.

→ **13.08c IS DONE: THE WORLD MOVES.** `--every N` keeps a frame; the page has a
  slider and every reading on it comes from the chosen year. 42 frames at 256,
  1.2 MB, and the arc is the point:

    year  10        992 alive,  8 roads open,  4 regions peopled
    year 250     31 872 alive, 55 roads open
    year 420    123 600 alive, 78 roads open, 55 regions peopled

  Getting the pre-history INTO the timeline meant moving those three hundred
  years out of `Generate` and into `Run`. The first attempt did that and produced
  a DIFFERENT world - 27564 alive against 36374 - not because the years changed
  but because `RequestDetail` now fired at year zero, where nobody has spread and
  no region is worth detailing. A run is not defined by which call ticks the
  clock; it is defined by when the world is asked to pay attention. With the
  request back at its own year, the restructure reproduces the old run to the
  bit. ADR-0122.

→ **13.08a IS DONE, AND THE OBVIOUS TEST FOUND A DEFECT.** `RegionView::Roads`
  is a COUNT - three routes touch this region, and not which three - so a map
  drawn from a frame had fifty-two towns on it and no lines between them.
  `Vaelen/View/Net.h` carries the roads themselves and the colony with them,
  and the page draws both. ADR-0121, including the reason the network is NOT a
  field on `WorldView`: 13.02's Delta diffs a frame REGION BY REGION, so routes
  living in that struct would be carried, ignored by the diff, and stale in
  every screen rebuilt from a delta. A structure the diff does not know about
  must not live inside the thing the diff claims to describe.

  Then the test that compared each route to the world **failed on 96 of 254**.
  06.04 sorts routes into open and closed once at the top of a tick; a road
  closed during that tick stays in the open list, so the reuse pass cannot find
  it and builds a SECOND entity for a pair that already has one. 105 of 275 at
  256. It has never been seen because the only check that looks for duplicate
  pairs counts pairs with two OPEN roads, and every twin is one open and one
  closed. `Openings` restarts at one on the twin, so a road opened five times
  reads as five roads. ADR-0120: proposed, not applied - it moves the frozen
  digest, like ADR-0111 and ADR-0118.

  Two things measuring caught that reading would not: a colony on ground with no
  ore seam lifts nothing and puts nobody on anything, and a MiningSystem that
  was never CONSTRUCTED (an edit whose anchor had moved, applied without an
  assertion that it matched) is green in the build, green in the tests, and
  reports zero. Only running it says so.

→ CI: run 140 came back **7 of 9 green**, Windows MSVC and macOS included, and
  the two debug legs were CANCELLED at 117 minutes by a 120-minute timeout. Not
  by the work 13.07a added - `View.Land` takes 1.69 s and 150 of 151 tests had
  already passed. The job died waiting on `Gameplay.Shuffled`, which ctest
  picked up ninety-five minutes in and which needs forty-five of its own.

  The wall clock of a leg is set by when its longest test STARTS, not by how
  much work there is. So gcc-debug finishing at 114 of 120 minutes was never a
  passing job - it was a job that passed when the packing was lucky. Every
  `*.Shuffled` now carries `COST 10000` and every gate `COST 5000` so ctest
  starts them first, and the budget goes to 180 minutes on all three long jobs.
  ADR-0119.

→ A SECOND open question put to a person rather than decided alone, and this one
  was found by LOOKING. One hour after the viewer existed: **six settlements
  stand in regions with zero inhabitants**, and five of them can never be
  abandoned, because 06.04's rule counts traffic and traffic is credited to both
  ends of a route. A place nobody lives in that is still being shipped to is
  immortal. Nine thousand units of goods sit in one such region.

  Fixing it moves the settlement count, which moves the event-log digest frozen
  in eleven gates - the same class of decision as ADR-0111. And the fix may not
  be the point: a depot kept alive by the trade passing through it is not
  obviously a bug in a world whose premise is that systems cause events nobody
  wrote. ADR-0118 has the measurements and the four answers.

  What it says about the project is larger than the defect. Thirteen phases of
  tests all test something somebody already suspected. Nobody had ever LOOKED at
  the world, and the first look found something no gate could catch, because
  every gate compares the world to what the world did last time.

→ **PHASE 13 STILL CANNOT BE CLOSED HERE.** Its remaining three tasks are 13.07c
  VaelenPresentation and the world drawn in the engine, 13.08 a person and a
  colony and a road drawn from the view of 13.01, and 13.09 the editor open on
  AELVOR at 256 with a frame rate written down. Every one needs UE 5.6, a GPU and
  somebody looking at a screen. This container has clang, gcc, cmake and ninja
  and no engine. Section 19 has the split and the rule for the machine that has
  the engine: it reports errors in the kernel modules, it does not fix them.

  What 13.09's number will be measured against is already known, because the
  machine that has the engine measured it: **6.05 s to generate and run AELVOR at
  256 for 420 years** (6.24 / 6.40 / 6.52 on repeats), against 1.00 s at 128.
  Four times the tiles, 3.4 times the people, six times the seconds.

WAS
→ Phase 12 GAMEPLAY closed against section 2 with a gate that is full: 14400
  played intents over two lives, 327267 acts, 6939 people thought of, 96 names
  in 12 places, 21 condemned, 82647 events of belief every one with a sentence,
  and a replay identical down to seven digests. It was hollow on its first run
  and the volume assertions of ADR-0095 caught it.

WAS
→ Phase 11 closed against section 2 on CI run 113: nine jobs green at 6be319f, the eight phase gates
  green locally in one run, purity 173 files and 0 violations, no file carrying INCOMPLETE, and
  ADR-0090 to ADR-0095 for every decision. Section 15b lists the thirteen PROTOTYPE files with their
  limits, as section 2 requires.

  Phase 12 is broken down in section 16, and reading before writing has already halved its first
  task and given its second an exact surface. Doings::Do takes a PERSON INDEX, not "the player", so
  the seven verbs of 10.05 are already anybody's - 12.01 has nothing to promote, and the real
  question becomes what decides an unplayed person's intent. And of the thirty-one files that read
  the world's whole event log, only two decide what somebody BELIEVES rather than what the world
  does: Regard.cpp:95 and Decisions.cpp:91. Every person in AELVOR is currently omniscient about
  anything that touched them, and those two call sites are the whole of 12.02.

COMPLETED
✓ Phases 00-10 closed · Phase 11 MINING COLONY closed
✓ 11.01 a region kept detailed · 11.02 a colony's people at the day · 11.03 the work of a colony
✓ 11.04 who holds it · 11.05 what it does to them · 11.06 the colony and the world
✓ 11.07 the colony in the chronicle · 11.08 the phase gate
✓ 12.01 a person nobody is playing · 12.02 an opinion between two people, and hearsay
✓ 12.03 the documents · 12.04 the maps · 12.05 a name that travels
✓ 12.06 what the world does about a name · 12.07 gameplay in the chronicle
✓ 12.08 the phase gate — a century of a lived colony, full and replayed
✓ 13.01 a read-only view of the world for a frame · 13.02 what changed since the last frame
✓ 13.03 level of detail for the eye · 13.04 the view across a snapshot and a reload
✓ 13.05 the Phase 13 kernel gate — a century watched a day at a time

NEXT
→ 12.06 — consequences: what a polity, an organisation or a family does about somebody whose repute
  has reached them. It is also where AELVOR's real gap is: every person in 12.01 acts at the same
  rate under the same rules, so nobody is famous for anything - fame is persistent but not yet
  EARNED, and only a repute that costs or gains somebody something will spread the distribution

TESTS
✓ CI run 113: nine jobs green (six Linux presets, clang-format, Windows MSVC, macOS AppleClang)
✓ Eleven phase gates in run_gates.sh; ten green in one local run before View.ViewGate joined them
  (GATES-DONE 0 failing, 19m14s, linux-clang-release)
✓ VaelenGameplayTests: Living, Repute, Documents, Maps, Fame, Judgement, Chronicle and the phase gate
  — 23 tests, gcc-debug/gcc-release/clang-release, purity 188 files and 0 violations
✓ The Phase 12 gate: 98 checks, 12m53s under gcc-debug and 2m05s under clang-release, frozen digests
  identical across both — half=a93a0190543952ea end=6a93fa19a2016086 log=cb125f942294717c
  belief=aa9c8172fa306bd1
✓ VaelenColonyTests 17 run, 17 passed across six suites, 1817 checks, CTest 8/8 with Shuffled
✓ The Phase 11 gate: a century at 256, a colony of 4007 bound on 4463 of seam, 5 lives and 14400
  intents, replayed blind to the same state digest, event log and mining digest

BLOCKERS
∅ (13.06 cleared the last UNVERIFIED file; 13.07-13.09 need a GPU and a person at a screen)
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

```

## 18. Verification record

**Two presets are not enough, and CI run 118 proved it.** Every task since Phase
10 has been verified against one debug preset and one clang preset, for speed.
That pair cannot see a whole class of defect: `-Wformat-truncation` and the rest
of gcc's optimiser-driven warnings only fire with `-O2` ON and only under gcc, so
`linux-gcc-release` and `linux-gcc-noasserts` are the two legs that catch them.
12.04 shipped a 64-byte buffer that `NameDocument` could overrun by twelve bytes,
and both of my presets called it green. **A task is verified against
`linux-gcc-debug`, a clang preset AND `linux-gcc-release` from here on** - the
third build is a couple of minutes and it is the one that reads the code the way
the compiler will in a release.

**A new module is declared in four places, and only three of them matter.**
`Tools/kernel_modules.txt`, the module's own `CMakeLists.txt` entry,
`Vaelen.uproject`, and **both** `Source/*.Target.cs` files. The first three are
read by something that runs on this machine, so a phase that forgets one fails
here within the hour. `ExtraModuleNames` in the targets is read by
UnrealBuildTool alone, and five phases in a row forgot it without consequence -
13.06 built all twelve modules from the stale targets, because UBT builds what
the project descriptor declares. Keep the targets current anyway: two lists
naming one set should agree, and a target that says seven when the answer is
thirteen misinforms the next reader. ADR-0112.

Commands run on 2026-09-05 (clang++ 18.1.3, g++ 13.3.0, CMake 3.28.3, Ninja 1.11.1, Python 3.11.15, clang-format 18.1.3, Linux x86_64) with the checked-in presets, each into
`out/build/<preset>`:

```
cmake --preset <preset> && cmake --build --preset <preset> && ctest --preset <preset>
out/build/<preset>/Tests/Core/VaelenCoreTests
python3 Tools/check_kernel_purity.py --self-test                       # 36 checks, 0 failed
python3 Tools/check_kernel_purity.py --root . --verbose                # 12 files, 0 violations
```

| Preset | Build | `ctest` | `VaelenCoreTests` |
|---|---|---|---|
| linux-clang-debug | 0 warnings | 14/14 passed | 133 run, 133 passed, 21914 checks |
| linux-gcc-debug | 0 warnings | 14/14 passed | 133 run, 133 passed, 21914 checks |
| linux-clang-release | 0 warnings | 14/14 passed | 133 run, 133 passed, 21914 checks |
| linux-gcc-release | 0 warnings | 14/14 passed | 133 run, 133 passed, 21914 checks |
| linux-clang-noasserts | 0 warnings | 14/14 passed | 108 run, 108 passed, 21701 checks |
| linux-gcc-noasserts | 0 warnings | 14/14 passed | 108 run, 108 passed, 21701 checks |

Per-suite counts: Assert 33, CoreTypes 1, Harness 5, Hash 15, Ids 19, Log 23, LogFloor 1, Random 29, Version 7 (133 tests with assertions, 108 without).

GitHub Actions run 5 (commit `71bad2d`, https://github.com/Thomas10112/vaelen/actions/runs/33977296696): all 9 jobs green - six Linux presets, clang-format 18, Windows MSVC 19.44 (`windows-msvc-debug`, 14/14 CTest entries), macOS 15 AppleClang (`macos-debug`, 14/14). The UBT/engine build is 13.06, run on 2026-09-10 under UE 5.6 on Win64: 157 actions, 92.52 s, MSVC 19.51, thirteen modules linked, Result: Succeeded, zero errors and several thousand C4251 warnings (ADR-0113).

## 19. Phase 13 - PRESENTATION: task breakdown

**Read this first, because it changes how the phase runs.** Every phase from 00
to 12 was done in a container with clang, gcc, cmake and ninja and nothing else.
Phase 13 is the first that cannot be. `VaelenPresentation` is an Unreal module;
it needs UE 5.6, an editor, a GPU and a person looking at a screen. Thirteen
phases of engine-side files - eleven `*.Build.cs`, eleven `*Module.cpp`,
`Vaelen.uproject`, `Config/` - carried `STATUS: UNVERIFIED` on the promise that
this phase would pay the debt. 13.06 paid it on 2026-09-10: the project has no
UNVERIFIED file left. What still needs the engine is everything that has to be
LOOKED at.

So the phase splits in two, and the split is not a nicety - it is which of two
machines does the work:

**Kernel-side (headless, this environment).** Everything that answers "what does
the renderer need to be told" without knowing what a renderer is. It is ordinary
kernel work: engine-agnostic, tested under the presets, covered by a gate.

| Task | Content | Test kind |
|---|---|---|
| 13.01 | A read-only VIEW of the world for a frame: the state a renderer needs, taken once per frame, never written back. The layering rule of the whole project says PRESENTATION reads and does not touch, and this is the surface that makes that structural rather than a promise | unit, deterministic |
| 13.02 | What changed since the last frame: a renderer that re-reads a world of a hundred thousand people every frame is not a renderer. The delta, and the proof that applying it to the previous view gives the current one | unit, integration |
| 13.03 | Level of detail for the eye rather than for the simulation: 01.03's SimLod says how finely the world THINKS, and this says how finely it is SHOWN, which is a different question with a different answer | unit, edge |
| 13.04 | The view under a snapshot and a reload, because a frame taken across a save must not tear | integration, deterministic |
| 13.05 | Phase 13 kernel gate: a century at 256 with a view taken every frame of the last year, the deltas applied, and the result identical to the view taken fresh | long-duration, replay |
| 13.07a | The GROUND in the view (`Vaelen/View/Land.h`), and `Tools/Atlas`: AELVOR generated, run and written out as numbers, with no engine anywhere | unit, integration, deterministic, edge | **DONE 2026-09-10** |
| 13.07b | The world DRAWN from that view and from nothing else (`Tools/Viewer/Atlas.html`): relief, biomes, rivers, regions, settlements, every tile readable | rendered headless, checked against the run | **DONE 2026-09-10** |
| 13.08a | The NETWORK in the view (`Vaelen/View/Net.h`): roads as roads and not as a count, and the colony. Drawn on the same page | unit, integration, deterministic, edge | **DONE 2026-09-10** |
| 13.08c | The world IN TIME: `--every N` keeps a frame, the page scrubs four centuries, and the pre-history stops being a black box | deterministic (a timelined run is the same run), checked output | **DONE 2026-09-10** |
| 13.08d | What HAPPENED: the chronicle listeners of Phases 04-11 wired for the first time outside a test, and the world's own sentences on the page beside the year | deterministic (the chronicle observes without touching), checked output | **DONE 2026-09-10** |
| 13.08e | And WHY: `CauseChain` walked for every record and written under it, four systems deep | checked output, cause depth capped | **DONE 2026-09-10** |
| 13.08f | The page CHECKED (`Tools/check_viewer.py`): the one file in this repository that nothing compiled, now read without a browser — script parses, every element it reaches for exists, nothing rendered is non-ASCII, every CSS token is defined on bare `:root`. Three CTest entries; the built page checked too | self-test (10 deliberate breaks), checked output | **DONE 2026-09-10** |

**Engine-side (needs UE 5.6, another machine).** Nothing here can be written
honestly from a container without the engine, and writing it anyway is how a
file ends up marked VALIDATED on the strength of having compiled in somebody's
head.

| Task | Content | How it is verified |
|---|---|---|
| 13.06 | The first UBT build of all eleven kernel modules. This is the task the UNVERIFIED marks have been waiting for since Phase 00 | it builds, or it does not | **DONE 2026-09-10 - it builds** |
| 13.07c | `VaelenPresentation`: the UE module, and the world drawn as regions **from that view alone** inside the engine. 13.07b proved the view is sufficient to draw from; this is the same claim in Unreal, and the first place ADR-0113's C4251 decision can be shown to do anything | a screenshot | **COMPILES 2026-09-10 (UE 5.6.1, MSVC 14.44, 14/14 DLLs) — NOT YET SEEN** |
| 13.08b | A person, a colony and a road drawn from the view of 13.01 **inside the engine** | a screenshot |
| 13.09 | Phase 13 gate: the editor open on AELVOR at 256, a century running, and the frame rate written down | measured on the machine that has the engine |

### 13.06 The first UBT build - VALIDATED (UE 5.6, Win64 Development Editor)

```
Result: Succeeded
157 actions, 92.52 seconds, MSVC 19.51 (14.51.36231), UE 5.6, Win64 Development Editor
thirteen modules compiled and linked as DLLs, zero errors
```

Thirteen phases of engine-side files were written for a toolchain that had never
run. Fourteen of them carried `STATUS: UNVERIFIED` on the promise that this task
would pay for it. **The project now contains no UNVERIFIED file at all.**

Two things came out of it that reading the files could not have produced.

**The targets were four phases stale and it did not matter.** Both
`.Target.cs` files named seven simulation modules of twelve. I predicted a hollow
build. The build was run before the fix and linked all twelve anyway: UBT builds
what the project descriptor declares, and `Vaelen.uproject` was complete. The
targets are complete now because two lists describing one set should agree, not
because anything depended on it. ADR-0112, rewritten against the log rather than
against my reasoning.

**Several thousand C4251 warnings, and they are not nothing.** Seventy-nine
kernel classes carry an export macro and most hold a `std::vector`; MSVC warns
once per member per translation unit that sees it. Safe today because Unreal
builds one target with one CRT, absent entirely in a monolithic link, and
genuinely load-bearing the day two toolchains meet. ADR-0113.

**Named limit carried into 13.07.** The C4251 decision belongs there, because
13.07 is the first task with a real cross-module consumer AND the first place the
change can be verified - a `#pragma warning(disable:4251)` guarded by `_MSC_VER`
compiles to nothing on this container, so the eleven local gates would go green
having tested none of it. Two candidates: suppress per module the way Unreal's
own code does and lose the signal, or export free functions over opaque handles
and move headers eleven gates depend on. Neither is decidable without a Windows
build in the same pass.

**What it did not verify.** Nothing ran. `Result: Succeeded` says the kernel
compiles and links as twelve DLLs. Whether a world ticks inside the editor is
13.09's gate and nothing before it.

### The kernel runs in the editor - 2026-09-10, between 13.06 and 13.07

Not a task. It happened because the atlas actor was already there and somebody
typed a console command.

```
Cmd: Vaelen.Atlas
LogVaelenAtlas: AELVOR 128x128, seed 0x41454c564f52: year 420,
                6459 land tiles, 65 regions peopled, 36374 living,
                43 towns, 85 roads, 1 polities standing
                (region 26 simulated person by person).
                Simulated in 1.00 s.

LogVaelen: VAELEN 0.0.1 - kernel save format v3 - kernel asserts on - module started
```

Twelve modules loaded into a running editor, the log sink carried the kernel's
own logging into Unreal's, a world generated and lived four hundred and twenty
years with every system on, and it did it in one second **with assertions
enabled**. No crash, no assertion, nothing in the log after the report line.

**What it does not settle.** Nothing was seen - the plate went into a level whose
camera was eighty-five thousand units away, on top of the Open World template's
Landscape. Legibility is 13.07b. And "Simulated in 1.00 s" is the cost of
building the world once, not the cost of a frame, which is 13.09's question.

ADR-0114, which exists because 13.06 put "nothing RAN" on fourteen files twenty
minutes earlier: a project that records what it has not proved has to be as
quick to record when it proves it.

### 13.07 broken down - and why reading the repository changed it

Written after 13.06, because 13.06 was the first time anybody looked at the
engine-side code with a toolchain that could read it.

**`AVaelenAtlasActor` already exists and already draws AELVOR.** Seven hundred
and nineteen lines, written early as "the first stone" of a layer that was then
numbered Phase 15. It builds a `World`, runs its pre-history and its centuries
inside the editor, and lays the result out as relief: one instanced cube per
tile, coloured by biome, raised by elevation, towns and roads marked on it. It
has even already paid a lesson this task would otherwise have paid again - a
comment records that the plate came out unpainted the first time because
`BasicShapeMaterial`'s colour parameter is not called what one would expect, so
the code asks the engine for the parameter list instead of guessing.

**And it reads the world directly.** `Map.GetLayer(T.World.Layers.Biome)`, the
region pools, the settlement components. That is precisely what 13.01 to 13.05
were built to stop: PRESENTATION reads a frame, not the simulation.

So 13.07 is not "make the first image". The first image has code behind it
already. 13.07 is **make the image come from the view**, and the useful thing
reading the code produced is that this is far smaller than it looked.

**The property that makes it small.** A `WorldView` holds no pointer, no handle
and no reference to the world it came from - that was 13.01's whole design, and
the consequence nobody had needed yet is that **the view outlives the world**.
`BuildAelvor` can take a frame of a world that dies when the function returns,
and keep it. No subsystem owning a live world, no duplicating the hundred and
forty lines of system wiring the atlas actor already has.

**13.07a** - the atlas actor fills a `ViewSources` from the types it already
holds, calls `TakeView`, calls `MeasureView`, and writes the result to the log.
Sixty lines in a file that now compiles, against an API eleven gates verify. It
proves the world-to-view path inside the editor.

> **Superseded on the same day, and the reason is worth keeping.** What is
> written above is what 13.07a looked like before anybody counted what a
> `WorldView` actually contains. It contains regions - ninety-nine of them on
> AELVOR at 128 - and the coastline the actor draws is six thousand four hundred
> and fifty-nine land tiles that are not in it. An atlas actor logging ViewStats
> would have proved the world-to-view path and left the drawing exactly where it
> was: reading the map directly, because the map is where the ground is. The
> task 13.07a became is below.

**13.07b** - `VaelenPresentation` as a module, and an actor that draws one slab
per region from the stored view and from nothing else. It cannot reach the
simulation even by mistake, because a `WorldView` gives it nothing to reach
with.

**Why 13.07a is not written yet, and this is a sequencing decision rather than a
delay.** These files are in no `CMakeLists`, so the nine CI jobs never compile
them: anything written here for them is unverified until a Windows build reads
it. The engine build currently WORKS. Pushing a blind change that fails to
compile would take that away and block the one action worth doing next - opening
the editor and pressing Build AELVOR on a world nobody has ever seen run. Code
first, image second is the wrong order when the code cannot be compiled here and
the image can be produced there today.

**The C4251 decision (ADR-0113) resolves in 13.07c**, which is the first task
with a real cross-module consumer INSIDE UNREAL and the first place a `#pragma`
guarded by `_MSC_VER` can be shown to do anything at all. It did not resolve in
13.07b: that task draws the world from a page, MSVC never reads it, and saying
otherwise would be claiming a warning was dealt with because a different
compiler never emitted it.

**How the two halves meet.** The kernel half is done here, pushed, and green in
CI. The engine half is done on the machine with UE 5.6, and the standing rule
for that machine holds: it does not fix anything inside `Source/VaelenCore`,
`VaelenSim`, `VaelenPopulation`, `VaelenSociety`, `VaelenEconomy`,
`VaelenPolitics`, `VaelenMilitary`, `VaelenInfrastructure`, `VaelenColony`,
`VaelenPlayer` or `VaelenGameplay` - those are validated by the headless CI. It
reports the errors and they are fixed here, where the tests are.

### 13.07a done - the ground in the view, and AELVOR with no engine

**The finding first, because it is worth more than the code.** `AVaelenAtlasActor`
reads `Map.GetLayer(T.World.Layers.Biome)` directly, and thirteen phases of
layering rule say it should not. Reading both sides together says something
sharper than "the actor is non-compliant": a `WorldView` is REGIONS, and the
coast is not in it. The actor reads the world because the world is where the
coast is.

> A layering rule with nothing behind it is a rule everybody breaks. Before
> asking a consumer to stop reaching past a boundary, look at what it is
> reaching for. If the boundary does not carry it, the consumer is not
> undisciplined - the boundary is incomplete.

**What was built.**

- `Source/VaelenView/Public/Vaelen/View/Land.h` and `Private/Land.cpp`: a
  `MapView` of `TileView`s, one per tile, eight bytes each - elevation in Q16.16,
  biome, region, and a byte of ground flags whose first four bits are asserted
  against `WorldGen::TerrainFlag` so they cannot drift. No pointer, no handle,
  no way back, exactly as 13.01. ADR-0115.
- `Tools/Atlas/Main.cpp`: the atlas actor's job minus Unreal. Generates AELVOR,
  runs its centuries with every system (bondage included - the player of VAELEN
  starts owned, and a view that cannot say who is bound is missing the number the
  premise turns on), takes both views, writes them out as JSON. ADR-0116.
- `Tools/check_atlas_output.py`: reads the file back. Twenty checks, each one
  proved to fire by `--self-test`, because a tool exiting 0 proves only that it
  did not crash.

**How it is verified.** `Tests/View/Test_Land.cpp` - five suites, thirty-three
checks: no way back into the world (compiler-checked), the ground matches the
world tile for tile across all 9216 tiles of a 96-map, the ground outlives the
world that made it, one seed gives one ground and two seeds do not, and a world
with no map says so instead of reading an empty layer by tile index. Plus four
CTest entries that RUN the tool: `Atlas.Runs`, `Atlas.RunsAgain`,
`Atlas.Output`, `Atlas.Deterministic`, `Atlas.OutputSelfTest`.

**The number that came free.**

```
Linux, GCC, headless          : AELVOR 128, year 420,  6459 land, 65 peopled,  36374 living
Windows, MSVC, inside UE 5.6  : AELVOR 128, year 420,  6459 land, 65 peopled,  36374 living
Linux, GCC, headless          : AELVOR 256, year 420, 25842 land, 55 peopled, 123600 living  (5.65 s)
Windows, MSVC, inside UE 5.6  : AELVOR 256, year 420, 25842 land, 55 peopled, 123600 living  (6.05 s)
```

Two operating systems, two compilers, two build systems, one world. Determinism
has been asserted by tests since Phase 00; this is the first time it has been
watched across the engine boundary.

**What it changes about how the project runs.** The daily loop moves off Unreal
and Unreal stays the target. Every question about the world - does the coast look
like a coast, does the terrain have ranges or noise, what happened in year 300 -
used to cost a fifty-seven-second editor start on a machine the engine warns
about at every launch. It now costs one command. The engine is still where the
game is, and 13.09 still needs it.

### 13.07b done - the world drawn from the view and from nothing else

`Tools/Viewer/Atlas.html` reads one thing: the JSON `Tools/Atlas` writes. No
World, no kernel header, no way to ask the simulation anything - and by the time
the page opens, the world that made the numbers has been destroyed. Thirteen
phases of layering rule, finally load-bearing rather than remembered.

What it draws, all of it out of the two views:

- the ground, one pixel a tile, in the kernel's own palette
  (`AVaelenAtlasActor::PaintColour`, copied so page and engine agree)
- **relief by slope shading**, light from the north-west. This is the toggle
  that closes a question open since the first screenshot: does the terrain have
  SHAPE, or is it flat noise with colour on it? AELVOR has ranges, and they run.
- height alone on a single ramp, for when biome colour answers first
- rivers, lakes, the shelf darkening with depth, shore water apart from open sea
- region borders, settlements, and the one region simulated person by person
- every tile readable on hover: what it is, how high, whose region, how many
  live there, how many of them are bound

`Tools/Viewer/build_viewer.py` inlines a run into the page and refuses to do it
for a file `check_atlas_output.py` rejects. A viewer that draws a broken world
convincingly is worse than one that will not open.

**What this is NOT.** It is not `VaelenPresentation`, and the engine-side table
now calls that 13.07c. Drawing AELVOR in a browser proves the view carries
enough to draw from; it proves nothing about Unreal, and ADR-0113's C4251
decision stays open because MSVC never reads this page.

### 13.08a done - the roads as roads, and the obvious test that found a defect

**What was missing.** `RegionView::Roads` is a count. A renderer holding a frame
knows a region is touched by three routes and not which three, so the first
picture of AELVOR had fifty-two towns on it and not one line between them: the
whole economy of the world was undrawable.

`Vaelen/View/Net.h` carries `RouteView` (32 bytes, no padding) and `ColonyView`
(16), in stable order, with `RouteOf` for the unambiguous lookup by index and
`RouteBetween` for the pair. `Tools/Atlas` writes both out; `Tools/Viewer` draws
the roads weighted by what they have carried, and the colony on top of them.

**The decision worth keeping** is the one about where it does NOT live:

> 13.02's `Delta` diffs a `WorldView` region by region. A vector of routes added
> to that struct would be carried by the view, ignored by the diff, and silently
> missing from every screen rebuilt from a delta. **A structure the diff does not
> know about must not live inside the thing the diff claims to describe.**

**And then the obvious test failed.** Comparing each route in the view to the
route in the world, keyed on the pair of regions, failed on 96 of 254 - because
a pair does not identify a road. 06.04 builds a second entity when it reopens a
road it closed earlier in the same tick. 105 of 275 at 256. ADR-0120 has the
four lines that do it and the one-line fix, which is not applied because it
moves a digest eleven gates have frozen.

**The colony.** `--colony` founds one on the busiest region that has ore under
it; without the flag the tool declares no colony types at all, so its digests
are provably the digests of every run before this task - checked against the
published numbers rather than asserted.

```
AELVOR 256, --colony : 5689 hands on the rock, 990 units of ore lifted,
                       78 roads open of 275, 105 of them twinned
AELVOR 256, plain    : frame 0xd0407d9684ab4f5a, ground 0x1faebda9e6c61a5b - unmoved
```

**What it cost to learn twice.** A colony on ground with no ore seam lifts
nothing. And a system that is never constructed - an edit whose anchor had moved,
applied without asserting it matched - builds green, tests green, and reports
zero hands. Both were found by running the thing and reading the number.

### 13.08c done - four centuries, and what actually defines a run

The page showed one frozen year. The world has four hundred and twenty.

`--every N` keeps a `WorldView` and a `NetView` every N years; the page gets a
slider, and every number on it - living, bound, settlements, roads open, the
towns drawn, the weight of each road - is read from the chosen year. A frame
carries regions and routes and NOT the ground, which does not change and would
otherwise be the whole file; both go in as flat arrays with a stride.

**The finding.** The pre-history ran inside `PreHistory::Generate`, so the
timeline could not see into the first three centuries - the ones where nine
hundred people become twenty thousand. Seeding with `Generate(Config, 0, false)`
and putting every year through `Run` fixes that, and the first attempt at it
produced a different world: 27564 alive against 36374, and no region detailed at
all. The years had not changed. `RequestDetail` had moved to year zero, where
nobody has spread yet and `Busiest()` finds nothing worth detailing.

> A run is not defined by which call ticks the clock. It is defined by WHEN the
> world is asked to pay attention.

With the request back after exactly `PreHistory` years, the restructured run
reproduces the old one to the bit - frame `0x1ad9b6c934b65257`, ground
`0x8f7f4948f49b6e86`, network `0x7f0e8fbdef66b895` at 128, all unmoved. That is
what makes it a restructure.

**And what the page refuses to draw.** The kept frames carry no colonies, so the
only year the page knows there is one is the last. The marker appears in that
year alone and the panel says `Colony (at year 420)`, because a mine on the map
at year 10 - three centuries before anybody dug it - is the confident wrong
picture this whole layer exists to prevent.

### 13.08d done - the world already knew how to tell its own history

Phases 04 to 11 each built a chronicle listener and a describer - `PersonChronicle`,
`SocietyChronicle`, `EconomyChronicle`, `PoliticsChronicle`, `MilitaryChronicle`,
`LifeChronicle`, `ColonyChronicle`. Each turns the events that matter into
`RecordInfo` documents and can put a sentence on one in the words of the world.
Every one is tested and green in eleven gates. **Nothing outside a test had ever
read one.**

Wiring three of them into `Tools/Atlas` behind `--chronicle` is a page of code.
What comes out of it is the thing this project has been building for thirteen
phases and had never looked at:

```
Year 0, age of Divik: the Oldegedim first settled Edavaken.
Year 21, age of Ubu: a great eruption struck Miogu.
Year 21, age of Ubu: The road from Kiodanam to Entam was opened.
Year 419, age of Okerdun: Kudihumho was enslaved by debt in Edavaken.
Year 419, age of Okerdun: Arord was freed by manumission in Edavaken.
```

**The tally is the finding.** 8158 records over four centuries at 256:

```
freed 1697 · died 1628 · enslaved 1473 · married 1114 · roads 937 · towns 71 · settled 59
```

After death, bondage is the most recorded fact of this world. Three thousand one
hundred and seventy entries about people being owned and ceasing to be owned,
and nobody wrote a line of it as content.

**What the page says out loud.** Person-level events exist only where the world
runs person by person - one region of a hundred and twenty-six - so the panel's
late decades are one town's marriages and extinct houses. That is the shape of
the LOD design and not a hole in the record, and the page carries a line saying
so rather than letting a reader conclude the rest of the world is empty.

**The rule.** Eleven phases of capability, built and verified and never used. A
test proves a thing works; only a reader proves it is worth anything. Ask the
systems you already have what they know before building another one.

**What must not happen in this phase.** A file marked VALIDATED because it
looked right. UNVERIFIED is not an embarrassment to be cleared by assertion; it
is an accurate statement about eleven `Build.cs` files that no compiler has ever
read, and it stays until one has.
