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
| 03 | HISTORY | Simulated pre-history that everything later inherits: eras, cultures, languages, religions, migrations, the historical record. | IN PROGRESS (03.01 VALIDATED headless; 03.02-03.08 PLANNED) |
| 04 | POPULATION | Persons and families: birth, ageing, death, lineage, needs, demographics. | PLANNED |
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
running them (section 10). Test counts are from `VaelenCoreTests --list`. This numbering
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

Verified: headless configure, build and `ctest` for all six Linux presets (section 10),
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

### 03.02 Cultures and coarse population

- `Culture` entities seeded on fertile regions, `RegionPopulation` components (counts
  per culture), growth bounded by deposits and biome, monthly migration along the
  region graph with a deterministic choice, assimilation and splits.
- Tests: conservation of counts, bounds, determinism, frozen digests.

### 03.03 Languages and naming

- `Language` entities per culture with a phonology (syllable inventory drawn from the
  seed), deterministic name generation for regions, rivers, eras, cultures and later
  persons; drift and split when cultures split.
- Tests: names are pronounceable by construction, unique per scope, frozen.

### 03.04 Religions

- `Religion` entities born from cultures and events (omens, disasters, founders), spread
  along migration, schisms; tenets as data that later phases read.
- Tests: spread follows the graph, no religion without a founding event.

### 03.05 Disasters and omens

- Yearly random events tied to the world (drought from moisture, flood from rivers,
  eruption from mountains, plague from population density) with causal consequences
  in population and religion.
- Tests: every disaster has a place and a cause; frequencies in bands.

### 03.06 Pre-history run and the starting state

- A `GeneratePreHistory` call runs N years (default 500) at LOD 4 over a generated
  world and returns the world ready for Phase 04; ASCII export of cultures by region.
- Tests: reference run frozen per century at 256; snapshot mid-history continues
  identically; 1024 baseline.

### 03.07 Queryable history

- "Why" queries: from any entity or event to its cause chain and era; per-region
  timeline; the chronicle as text lines built from records and names.
- Tests: every record resolves; text is deterministic.

### 03.08 Phase 03 gate

- Long-duration run (2000 years at 256), invariants every decade, frozen digests on
  four compilers; Phase 03 closed against section 2.

## 8. Phases 04-20: notes

No task breakdown exists yet for Phases 04-20; each is broken down when the previous
phase closes. Fixed points already in the code: `IdKind` values for Region, Tile, River,
ResourceDeposit (Phase 02), Culture, Language, Religion, Person, Family, Organization
(Phases 03-05), Item, Building, Settlement, Market, Route (Phases 06, 09), Polity, Law,
Army, War (Phases 07-08), Document, Map (Phase 12); `VAELEN_SAVE_FORMAT_VERSION`
(Phase 16); `Config/DefaultEngine.ini` and `DefaultInput.ini` note that the game engine
class and Enhanced Input mappings arrive in Phase 10.

## 9. Current BUILD STATUS

```
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
VAELEN BUILD STATUS
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

PHASE       : 03 — HISTORY
TASK        : 03.01 — ERAS, THE ERA CALENDAR AND THE HISTORICAL RECORD
STATUS      : VALIDATED (headless) / UNVERIFIED (engine)

PROGRESS
███░░░░░░░░░░░░░░░░░░░░░ 12%

CURRENTLY
→ 03.01 closed: Era entities opened yearly by span or by a caused request, Record entities
  per chronicled event with era and region, history state in a singleton component,
  cause-chain and era/subject queries

COMPLETED
✓ Phase 00 — FOUNDATION ; Phase 01 — CORE SIMULATION ; Phase 02 — WORLD (headless)
✓ 03.01 Eras and the historical record (3 tests: History)

NEXT
→ 03.02 Cultures and coarse population (Culture entities, RegionPopulation, growth, migration)
→ 03.03 Languages and naming
→ Monday: first UE 5.6 build on the PC (ARCHITECTURE section 8 checklist)

FILES
+ Source/VaelenSim/Public/Vaelen/Sim/History.h, Private/History.cpp
~ Ids.h/.cpp (IdKind::Era = 15)
+ Tests/Sim/Test_History.cpp

TESTS
✓ Core 133 (108 without asserts) + Sim 130 (127 without asserts); ctest 42/42 in all six Linux presets
✓ 260 simulated years: eras contiguous without gaps, 52 requested eras each with a two-link cause chain
  (collapse <- omen), span eras exactly every 30 years, records = chronicled events
✓ Purity: 52 files, 0 violations

BLOCKERS
None
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
```

## 10. Verification record

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
