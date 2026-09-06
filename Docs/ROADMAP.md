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
| 04 | POPULATION | Persons and families: birth, ageing, death, lineage, needs, demographics. | IN PROGRESS (04.01-04.06 VALIDATED headless; 04.07-04.08 PLANNED) |
| 05 | SOCIETY | Organisations, social structure, status, bondage and slavery as institutions, norms. | PLANNED |
| 06 | ECONOMY | Items, production, markets, prices, trade, wealth and its transmission. | PLANNED |
| 07 | POLITICS | Polities, laws, authority, succession, factions, diplomacy. | PLANNED |
| 08 | MILITARY | Armies, conflicts, wars, security forces, conquest and its consequences. | PLANNED |
| 09 | INFRASTRUCTURE | Buildings, settlements, routes, logistics and their decay. | PLANNED |
| 10 | PLAYER | The player as one simulated person: enslaved start, body, needs, skills, relationships; player intent as commands into the simulation. | PLANNED |
| 11 | MINING COLONY | The starting place: a huge autonomous mining colony simulated by the same systems at full detail. | PLANNED |
| 12 | GAMEPLAY | Interaction verbs, knowledge (documents, maps), reputation and consequences without main quest or canonical ending. | PLANNED |
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
running them (section 11). Test counts are from `VaelenCoreTests --list`. This numbering
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

Verified: headless configure, build and `ctest` for all six Linux presets (section 11),
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
  crowd over the line to the coarse neighbour with the most room, the persons destroyed
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

## 9. Phases 05-20: notes

No task breakdown exists yet for Phases 05-20; each is broken down when the previous
phase closes. Fixed points already in the code: `IdKind` values for Region, Tile, River,
ResourceDeposit (Phase 02), Culture, Language, Religion, Person, Family, Organization
(Phases 03-05), Item, Building, Settlement, Market, Route (Phases 06, 09), Polity, Law,
Army, War (Phases 07-08), Document, Map (Phase 12); `VAELEN_SAVE_FORMAT_VERSION`
(Phase 16); `Config/DefaultEngine.ini` and `DefaultInput.ini` note that the game engine
class and Enhanced Input mappings arrive in Phase 10.

## 10. Current BUILD STATUS

```
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
VAELEN BUILD STATUS
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

PHASE       : 04 — POPULATION
TASK        : 04.06 — LOD BRIDGE
STATUS      : VALIDATED (headless) / UNVERIFIED (engine)

PROGRESS
██████████░░░░░░░░░░░░░░ 41%

CURRENTLY
→ 04.06 closed: LodState (the regions the world wants detailed, up to 8, and the bridge's tallies),
  RequestDetail / ReleaseDetail / IsWanted, the yearly LodSystem (demotes what is no longer wanted,
  promotes what is wanted up to MaxDetailed in request order, refuses empty regions), and the
  crossings: a crowded detailed region (over 75 percent of its capacity) sends unmarried young
  adults to the neighbour with the most room (under 65 percent), a crowded coarse neighbour sends
  people into a detailed region with room as new persons; RegionPromoted / RegionDemoted / PersonLeft / PersonArrived events

COMPLETED
✓ Phases 00-03 (headless) ; 04.01-04.04 (CI 34-36) ; 04.05 Traits, skills and names (CI 37)
✓ 04.06 LOD bridge (5 tests: Lod) (CI 38)

NEXT
→ 04.07 Persons in history (chronicle lines and why-queries for persons)
→ 04.08 Phase 04 gate (500 years with a detailed region over the 256 pre-history)
→ Monday: first UE 5.6 build on the PC (ARCHITECTURE section 8 checklist)

FILES
+ Source/VaelenPopulation/Public/Vaelen/Population/Lod.h, Private/Lod.cpp
+ Tests/Population/Test_Lod.cpp
~ Source/VaelenPopulation/CMakeLists.txt

TESTS
✓ Core 133 (108 without asserts) + Sim 162 (159 without asserts) + Population 31 (31 without asserts);
  ctest 57/57 in all six Linux presets; the 04.01-04.05 digests unchanged
✓ AELVOR 128, the two busiest regions detailed in turn every 50 years for 200 years: 207 emigrants,
  0 immigrants, grains consistent; frozen persons digest d0f481c80e1e49d8
✓ 500 years at 64 with two of the three busiest regions detailed in turn every 25 years: invariants
  every decade, two worlds identical, the year-250 snapshot continued to the same year 500
✓ Purity: 77 files, 0 violations

BLOCKERS
∅ (engine-side files stay UNVERIFIED until the first UE 5.6 build)
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
```

## 11. Verification record

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

GitHub Actions run 5 (commit `71bad2d`, https://github.com/Thomas10112/vaelen/actions/runs/33977296696): all 9 jobs green - six Linux presets, clang-format 18, Windows MSVC 19.44 (`windows-msvc-debug`, 14/14 CTest entries), macOS 15 AppleClang (`macos-debug`, 14/14). Not run: any UBT/engine build.
