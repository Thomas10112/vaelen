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
| 13 | PRESENTATION | Unreal rendering, animation and audio of the world state, strictly read-only. | CLOSED (13.01-13.09; gate PASSED 2026-09-14 at 100 fps on a T400; engine and headless kernel agree on every figure at 256) |
| 14 | UI | Interface and read-only views; command submission through the gameplay layer. | **CLOSED** 2026-09-16, all five clauses met - (e) certified by CI run 215 on 4137b08, ten jobs of ten green (14.01-14.10 - eighty-three days played at the keyboard in UE 5.6, replayed headlessly to the same four digests byte for byte, and the HUD measured at 0.52 ms of game thread over a scene that runs at about 122 fps; the breakdown is section 20) |
| 15 | STREAMING & LOD | Engine streaming coupled to the simulation's grains: what is simulated at which detail away from the player. **The row said "simulation LOD 0-4" until 15.09 found that two different things were being called LOD.** `SimLod` (Sim/System.h) is a real five-rung ladder of how OFTEN a system runs - 1, 4, 24, 720, 8640 ticks - and 15.02 moved a system down it. A REGION has two grains and not five: person by person, or counts per culture. `RegionLod::Level` is written in one place, always the same value, and levels 0, 1 and 3 have never been reachable. | **CLOSED 2026-09-21** (15.01-15.10, section 21). The planning found five defects before a line was written; the review of 15.10 found 27 more, among them that the engine host recorded a world its own replay does not reach. The gate is met on a walk a person actually lived. |
| 16 | SAVE/PERSISTENCE | **The row said "serialisation of the whole world state" until the planning measured it: that shipped in Phase 01 and works.** What is not saved is the RUN - `Run::Aelvor`'s own members - so a restored world continues identically until somebody LOOKS, and then parts company with the world it was copied from. The gap is about thirty bytes, one part in 700,000 of the image. So the phase is a save that can refuse, a load that cannot half-apply, a container around the untouched image, a verb by which a run adopts a world it did not generate, a file that cannot destroy the last good one, and a migration chain. | BROKEN DOWN (16.01-16.14, section 22; the planning found twelve defects first, among them that `SaveSnapshot` cannot fail and cannot say so in exactly the builds a player runs) |
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
CI          : run 151 on 46b275b — 9 of 9 GREEN. ADR-0111's 28 re-frozen digests
              hold on gcc, clang, AppleClang and MSVC, debug and release, as
              ADR-0120's 44 did in run 148. Both were recorded from
              linux-gcc-release alone.
DECIDED     : ADR-0120 FIXED (three fixes) · ADR-0118 decided, unchanged
              ADR-0111 FIXED - the ledger of a region closes for everything
              06.02 makes, eats, spoils and wears, and the world did not move
              a single unit doing it
OPEN        : ADR-0129 a road cannot be abandoned · ADR-0131 the log says how
              much crossed a road, not what it was
13.07c      : DONE AND SEEN. AELVOR stands in the editor with its biomes, its
              rivers and lakes, its 44 towns and the 90 roads between them, and
              it is the same world the headless kernel builds under Linux: 6459
              land tiles, ten biomes at identical counts, 36374 living.
              Getting there cost four defects, all mine, all in the drawing:
                - the land was multiplied by 700 cm per elevation unit, a
                  constant copied from VaelenAtlasActor where the value it
                  multiplies is 65536x smaller. Land sat 4.5 km above a 128 m
                  map. What looked like a continent all evening was the HOLE in
                  the sea where the land should have been;
                - the biome palette held eleven colours for twelve biomes and
                  started at the wrong one;
                - nothing in the project counted biomes, so neither could be
                  contradicted by any output;
                - the tile material was left unset by default, and the command
                  spawns its actor into an unsaved level, so every restart threw
                  the assignment away and looked like a rendering bug.
              Relief is now a fraction of the map's width, normalised to the
              highest land in the view: a number that cannot be wrong by three
              orders of magnitude. MapStats counts biomes, the atlas writes
              them, the actor logs them and the colours it painted.
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
| 13.07c | `VaelenPresentation`: the UE module, and the world drawn as regions **from that view alone** inside the engine. 13.07b proved the view is sufficient to draw from; this is the same claim in Unreal, and the first place ADR-0113's C4251 decision can be shown to do anything | a screenshot | **DONE 2026-09-10 — seen: 12 biomes, rivers, 44 towns, 90 roads** |
| 13.08b | A person, a colony and a road drawn from the view of 13.01 **inside the engine** | a screenshot | **DONE 2026-09-14 — 1467 people drawn on 117 tiles, seen** |
| 13.09 | Phase 13 gate: the editor open on AELVOR at 256, a century running, and the frame rate written down | measured on the machine that has the engine | **PASSED 2026-09-14 — 100 fps / 11 ms on a T400 4GB** |

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

### 13.08b written - the view had no person in it

**The task could not be started.** "A person, a colony and a road drawn from the
view of 13.01" - and 13.01 has no person. `RegionView` carries a head COUNT,
which draws a number over a province and cannot put one figure anywhere. The
colony and the road were delivered by 13.08a; the person was not deliverable,
and nothing said so, because nothing in the project had ever tried to draw one.

That is the shape of the finding worth keeping: **the gap was in the surface,
not in the renderer.** It stayed invisible for as long as the only consumer of
the view was a test that read the view.

**`Vaelen/View/Folk.h`.** `PersonView` is 40 bytes, no padding: index, region,
family, culture, religion, AGE IN YEARS, spouse, sex, state, identity. An age
and not a birth tick, because a renderer draws a child or an elder and should
not have to know what a tick is. No parents - 13.01 says the state a renderer
NEEDS, and a family tree is Phase 14's business.

Taken separately from `WorldView`, for `NetView`'s reason and one of its own:
13.02's `Delta` would carry a vector of people without diffing it, and a frame
of AELVOR at 128 is 5600 bytes, which is 13.01's promise and not something to
spend on a crowd most renderers never draw.

**Only materialised people are in it.** A region the simulation thinks about in
aggregate has a population and no persons. The view reports who EXISTS rather
than estimating who would exist if somebody looked, and a renderer that finds no
people in a region is being told the truth about that region.

**What the drawer found that the tests had not.** `DrawFolk` draws the LIVING.
Asking whether a person is alive means reading `PersonView::State`, which is a
raw `uint8` whose meaning lives in `Population::LifeState` - in a module the
presentation layer may not include, by a prohibition that is the whole point of
the layer. So the view could COUNT the living and could not IDENTIFY one.

`View::IsAlive` and `View::AliveState` close it, checked against the enum in
`Folk.cpp`, the one translation unit allowed to see both - the same arrangement
`BiomeKinds` got in 13.07c, and found the same way: by writing the thing that
was supposed to read the view and watching it come up short. Not "not dead":
`LifeState` also has `Gone`, somebody who left the detailed grain and is kept
for history. They are not dead and they are nowhere.

```
region 36 after 40 years: 932 people, 570 living by the predicate, 362 not
region 38 detailed:      1164 people materialised, 1164 in the view, 911 living
same seed twice:          698 people, 582 living, oldest 85, digest ec2dc961979d1743
```

**Where a person stands is INVENTED, and the file says so.** The simulation has
no coordinate for a person - `PersonInfo` names a region and stops - so the view
has none either, and it must not: a view that made one up would be reporting a
position the world does not hold. `DrawFolk` chooses a tile from the ones the
MAP says the person's region owns, by a hash of that person's own identity, so
the same person in the same world always stands in the same place and one
screenshot can be compared with the next. It is a drawing decision, it lives in
the drawing layer, and Phase 14 may replace it the day a person has somewhere to
actually be.

It also means the crowd is spread over the ground the region actually holds,
rather than stacked on the centroid: `DrawFolk` takes the ground and the people
and NOT the frame, which is the reason it is worth being a function. The actor
logs the count of DISTINCT TILES used, because a pillar of nine hundred figures
and a populated province are the same picture from above.

**STATUS: UNVERIFIED, and deliberately.** The headless suite is green - 6/6 on
`View.Folk`, 156/156 overall - and that covers `Folk.h` and nothing in
`VaelenPresentation`, which the CI cannot build and no compiler on this machine
has read. 13.07c pushed a rename that did not compile and cost a round trip on
somebody else's evening. This is the same exposure, named in advance instead of
after, and it is why 13.08b is marked WRITTEN and not DONE.

### ADR-0134 applied - the CI now reads the module the CI cannot build

**Why this, and why now.** `VaelenPresentation` was the only module in the
project that no build on a CI runner and no build on the kernel machine ever
compiled. `VaelenPresentation.Build.cs` explains why, and is right: "a module
that needs an editor to exist has no business claiming a green light from a
build that has none."

That is right about VALIDATION and wrong about SYNTAX, and the difference cost
two round trips on the project owner's machine - a rename in 13.07c that did
not compile, and 13.08b's `DrawFolk`, which nothing had read at all.

The occasion was the owner being away from that machine for a weekend. The one
thing making that machine a bottleneck is that it holds the only compiler that
reads this module, so the weekend went on removing the bottleneck rather than
waiting behind it.

**What runs.** `Tools/parse_engine_modules.py` puts clang's front end over the
module's three translation units against `Tools/EngineShim`, with
`-fsyntax-only -Wall -Wextra -Werror`. One CI job, seconds long.

**The comfortable version was refused.** Both defects that actually shipped are
in `VaelenViewActor.cpp` - the file with the `UCLASS`, the `.generated.h` and
the `TObjectPtr`. A shim covering only the drawer would have been half the work
and would have caught neither. The `.generated.h` stubs are written by the
script for every one it finds *included*, so an actor added next year is
covered the day it is written.

**The measurement.** `Tools/test_engine_shim.py` breaks the module seven ways
on a temporary copy and fails if the parse did not notice - the first case
being `How.ReliefScale = ReliefScale`, the line that actually shipped:

```
control: an untouched copy parses
caught  the real 13.07c defect: a renamed setting the actor still assigns
caught  an argument dropped from a drawer call
caught  a tally field renamed in the header and not at its use
caught  UE_LOG naming a log category that was never declared
caught  a log argument naming a member that is gone
caught  a drawer handed the wrong view entirely
caught  a colour function called with the settings missing
7 mutations, all caught, control clean
```

The control runs first and must pass: without it, a parser that failed on
everything would look like a perfect detector.

**Not one STATUS line moved, and that is the point.** Parsing proves the shape
of a program - which is exactly what this module's STATUS blocks already say
compiling proves, and no more. `VaelenPresentation` stays UNVERIFIED until an
editor has run it. 13.08b stays WRITTEN, not DONE: it still needs a screenshot.

**The hazard is named in the file, not hidden.** A shim that drifts from the
engine gives a green light to code that does not build, which is worse than no
light at all. The rule: the UBT build is the authority, and when the two
disagree the shim is what gets fixed. A second guard is mechanical - the shim's
`CoreMinimal.h` refuses to compile unless `VAELEN_SHIM_PARSE` is defined, which
only the parse script defines, so it can never shadow the real engine header in
a real build.

### 13.08b done and seen - a crowd that outlived its world

**What is on the screen.** AELVOR at 128, year 420, with a speckle of pink, red
and white standing on one region of the east: 1467 people, one cube each, in
three age bands. The world they belong to was destroyed before the first of
them was placed.

```
AELVOR people: 4555 in the view, 1467 living in 1 regions, oldest 90,
               182240 bytes, digest 34839c0c725fe825
               - drawn 1467 on 117 tiles, 0 unplaced
AELVOR 128x128, year 420: 16384 tiles (6459 land), 99 regions (65 peopled),
               36374 living, 44 towns, 90 roads of 163, view 5600 bytes,
               simulated in 1.02 s, 1467 people drawn on 117 tiles
```

**The three numbers that were named in advance, and what each settled.**

`drawn 1467`, `living 1467`, `0 unplaced` - exact to the unit. Every living
person the view reported was placed on ground the map agrees their region owns.
Had the two views disagreed about which regions exist, the difference would be
sitting in `unplaced`, which is what that counter is for.

`on 117 tiles` - not 1. The crowd is spread over the ground the region actually
holds rather than stacked on its centroid, which is the whole reason `DrawFolk`
takes the map and not the frame. From above, a pillar of 1467 figures and a
populated province are the same picture, so the count of distinct tiles is the
only thing that tells them apart.

`182240 bytes` - 4555 x 40 + 40, exactly. **`sizeof(PersonView) == 40` holds
under MSVC**, not only under gcc and clang. The no-padding `static_assert` that
`MeasurePeopleView` depends on survives the Microsoft compiler, which no
headless CI leg could ever have shown.

**A prediction that was wrong, and why.** The digest was announced in advance as
`ec2dc961979d1743`. It came out `34839c0c725fe825`, and the announcement was
simply wrong: `ec2dc961979d1743` is the headless suite's number for a **64x64
map run 15 years**, and this is **128x128 run 120 years**. Two different worlds
have two different digests. A digest is only ever comparable against the same
configuration, and naming one across configurations is not a weak check - it is
no check at all.

**The real cross-check, run afterwards.** `Tools/Atlas --size 128 --years 120`
at the same seed gives year 420, 6459 land tiles, 99 regions (65 peopled),
36374 living, 90 roads open of 163 - every figure the engine printed, matching
exactly. The engine and the headless kernel simulate the same AELVOR, and the
people are drawn from it.

**And a stale binary nearly turned that into a false alarm.** The first run of
this comparison used `out/build/linux-gcc-debug/`, picked up by a `find` while a
release build was what had just been compiled. It reported 85 roads of 271
against the engine's 90 of 163, and for several minutes there was a divergence
that did not exist - the debug tree simply predated ADR-0131 and ADR-0132.

Twice now in this project a `find` for a binary has measured the wrong tree.
**Name the build directory, never search for it.**

### 13.09 PASSED - the phase gate, and what a gate is actually for

**The number.** AELVOR at 256, a century run, drawn in the editor:

```
100 fps, ~11 ms per frame
NVIDIA T400 4GB, driver 472.47, D3D11, 1920x1080 editor
65536 ground slabs + 6996 people + 42 towns + 75 roads = 72649 instances
```

A T400 is an entry-level workstation card, not a gaming GPU, on a driver four
years old that the engine itself warns about. 11 ms with 72649 instanced meshes
is not a tight budget - it is a comfortable one on hardware chosen badly.

**Everything the engine printed, against the headless kernel at the same seed
and settings:**

```
                     engine          Tools/Atlas --size 256 --years 100
year                 400             400
land tiles           25842           25842
regions (peopled)    126 (46)        126 (46)
living               109394          109394
roads open / total   75 / 163        75 of 163
```

Every figure. The engine and the headless kernel simulate the same AELVOR, and
Phase 13's whole claim - that a renderer can be handed numbers and nothing else
- is now measured on both sides of the engine boundary at the phase's own scale.

```
AELVOR people: 19925 in the view, 6996 living in 1 regions, oldest 92,
               797040 bytes, digest 5a4466c1e1fb91aa
               - drawn 6996 on 639 tiles, 0 unplaced
```

`797040 = 19925 x 40 + 40` exactly, as at 128. `sizeof(PersonView) == 40` holds
under MSVC at both scales. 6996 people on 639 distinct tiles, `0 unplaced`.

**AND THE GATE FOUND A DEFECT, which is the point of a gate.**

The log carried this four times, once per layer, and the world drew anyway:

```
AttachTo : 'Plate' is not static, cannot attach 'Ground' which is static.
Aborting.
```

`USceneComponent` is Movable by default; the four instanced-mesh layers are
Static; Unreal refuses to attach a Static child to a Movable parent. So the
ground, the towns, the roads and the people were **never attached to the
actor's root**. They are placed in world space, and AELVOR appears at the world
origin regardless of where the actor is.

It looked correct for exactly as long as the actor happened to sit at the origin
too. The first person to DRAG a "VAELEN View" actor across a level would have
watched the world stay behind, and no test in this project could have found
that - it is a message in a log that only appears when an editor is open, on a
machine with a GPU, with somebody reading.

Fixed by making `Plate` Static, and by setting mobility before attachment rather
than after, since the engine checks the pair when the attach resolves.

**What the gate is for, then.** Not the frame rate. The frame rate was never in
doubt. A gate is for the things that only appear when the whole thing runs at
full size in front of a person - and this one produced a defect that was
invisible to 156 headless tests, to nine CI jobs, to a compiler, and to a
screenshot that looked right.

### ADR-0135 applied - one world, three wirings, and a guard that reads all three

**What changed.** `BondageSystem` is now in `AVaelenAtlasActor` and
`AVaelenViewActor`, declared ninth - between `Norms` and `Economy`, where
`Tools/Atlas` declares it, because declaration order fixes component type ids.
The view actor also names the bondage types in its `ViewSources`, which was the
second half of the omission and the easier one to miss: without it, the system
runs and the view still reports nobody bound.

**What the engine prints now.** `AELVOR digests: frame …, ground …`, every run.
With bondage in, the frame digest is the number the headless atlas publishes
for the same seed and settings. That is the strongest check between the two
halves of the project, and it had been unplugged since the day the atlas gained
a system the actors did not.

**What stops it recurring.** `Tools/check_world_wiring.py` reads the three
files and compares their declaration order and their system order. Every
deliberate difference is named one by one with its reason; nothing else is
excused. CTest entry `Kernel.WorldWiring`, and a CI step. Tested by removing
`BondageSystem` from one actor on a copy: it points at the row and the column.

**And ADR-0134's parse now reads the game module too** - six translation units
instead of three - because touching `VaelenAtlasActor.cpp` without a compiler
was the same exposure 13.07c paid for. Extending it found two defects in the
shim itself, one of them caught by the project's own `static_assert`.

```
157/157 headless · purity 201/0 · shim-test 7/7 · parse 6 TU · wiring 3/3 agree
```

**Validated the same afternoon.** The Windows build ran `Vaelen.View 128 120`
and printed `frame abc5a5767c6cf9dd, ground 8f7f4948f49b6e86` - both identical
to `Tools/Atlas` - with every other figure unchanged and the four `AttachTo`
warnings gone. The engine and the headless kernel now agree on the digest of
the frame, which is the check this ADR set out to restore. `AVaelenAtlasActor`'s
half stays UNVERIFIED: that run does not exercise it.

## 20. Phase 14 - UI: task breakdown

**How this breakdown was made, because the method is part of the record.**
Four independent proposals from four lenses (layering purist, player-first,
testability-first, minimal-and-honest), three judges scoring all four against
the project's rules with the headers open, one synthesis, then two skeptics
whose whole job was to refute it - three rounds, until the layering skeptic
could no longer find an include that reached the simulation and the
measurability skeptic was down to counting corrections. Every claim below that
cites a file:line was opened by at least one of them.

**What the planning itself found, before a line of Phase 14 exists:** the
transitive include closure of `Vaelen/View/Frame.h` is 63 headers and reaches
`Player/Commands.h` - `Submit(World&)` - through `Gameplay/Fame.h` and
`Gameplay/Repute.h`. A read-only surface that can name the write. That is
ADR-0137 and task 14.02, and it is the reason 14.02 comes before any screen.

**The decisions this rests on:** ADR-0136 (the command surface as a leaf; the
played input as a stream, day turns included), ADR-0137 (the view headers as
leaves; `Take.h`), ADR-0138 (the day turn as the one recorded host input;
`VaelenGame` as host; the `-game -nullrhi` invocation). All three Proposed,
applied by the tasks that name them.

Approved by the project owner on 2026-09-14.

## Phase 14 - UI: task breakdown (third round)

Base unchanged: the **Minimal and honest** proposal with the first-round grafts (Intent.h leaf, kernel-side screen with its digest as the last row, hand-played stream replayed by CTest, `Issued` stamped by the world side, `VaelenGame` as the host, flat `ChronicleView`, `Lively=false`, Atlas flags instead of a replay binary) and the second-round leaf split (14.02). What this round changes: the day turn becomes a recorded input (the stream was not a function of the keys while Space was a key and not a record), the restricted parse gets a control that can actually pass (the parser stubs `.generated.h` per module, so VaelenGame's header was unstubbed inside VaelenUI's parse), and every count in a done-when is one a script prints or a CI leg runs.

Verified in the tree this round, each opened: `Source/VaelenView/Public/Vaelen/View/Frame.h:26-33` includes `Colony/Mining.h`, `Economy/Trade.h`, `Gameplay/Fame.h`, `Infrastructure/Roads.h`, `Player/Player.h`, `Population/Persons.h`, `Sim/PreHistory.h`, `Society/Bondage.h`; `class World;` is forward-declared at `Frame.h:40`, `Land.h:39`, `Net.h:37`, `Folk.h:43`; `Eye.h:23` includes `Vaelen/Sim/Regions.h` and `Eye.h:61-62,68-69` take a `WorldGen::RegionGraphCache&` with the comment at `:57-60` ("the choice is made where somebody can see it rather than hidden in a static"); `Doings.h:99` `mutable WorldGen::RegionGraphCache Ways`; `Regions.h:95` `AreAdjacent`, `Regions.h:108-121` `RegionGraphCache` with `Builds()` ("Times the graph has actually been built, for the tests"); p14_closure.txt gives `Frame.h -> Gameplay/Fame.h -> Gameplay/Repute.h -> Player/Commands.h`, `Submit(World&...)` at `Commands.h:221`. `Tools/parse_engine_modules.py:56` `ENGINE_MODULES = ["VaelenPresentation", "Vaelen"]`; `:62-70` `kernel_include_dirs()` reads `ROOT`, not `--source-root`; `:83-106` `stub_generated_headers(source_root, module, into)` scans only `Source/<module>/{Public,Private}`; `:134-136` writes them into a `TemporaryDirectory` inside the per-module loop and sets `includes = [SHIM, generated, public] + kernel_include_dirs()`. `Tools/test_engine_shim.py:96-100` copies only `ENGINE_MODULES` into the pristine tree, `:109-118` runs the CONTROL first, `:47-91` holds seven mutations, all in `VaelenPresentation` files; its docstring says "Nothing here writes to the working tree". The only `.generated.h` includes today are `VaelenViewActor.h:61` and `VaelenAtlasActor.h:21`, each inside its own module; `Tools/EngineShim/CoreMinimal.h:93` `#define GENERATED_BODY(...)` (a no-op); `Tools/EngineShim/Engine/World.h` is a bare `class UWorld : public UObject` with `SpawnActor<T>()` and no `GetGameInstance()`; there is no `Engine/GameInstance.h`; `Engine/Engine.h:16` `extern UEngine* GEngine`; `GameFramework/Actor.h` has `FActorTickFunction` and no `Tick(` virtual. `.github/workflows/kernel-ci.yml`: `linux` (`:21`, six presets, `ctest --preset` at `:50`), `format` (`:52`, clang-format only), `parse` (`:64`, `test_engine_shim.py`, `parse_engine_modules.py`, `check_world_wiring.py` - no ctest), `windows` (`:98`, `ctest --preset windows-msvc-debug -E Shuffled` at `:117`), `macos` (`:119`, `ctest --preset macos-debug` at `:138`) - ten jobs, eight run CTest; `CMakePresets.json:150,226` define `windows-msvc-release` and no job runs it. `Tools/check_world_wiring.py:138` prints `all three agree` and `:141` `the three wirings` as literals; only `:145-147` uses `len(WIRINGS)`, `len(OPTIONAL_DECLARES)`, `len(OPTIONAL_SYSTEMS)`; `OPTIONAL_DECLARES` (`:57-62`) names Colony, PersonChronicle, SocietyChronicle, EconomyChronicle and `OPTIONAL_SYSTEMS` (`:63-68`) MiningSystem plus the three chronicles - four and four; `DECLARE` (`:70`) strips `Types::Declare`, `MAKE` (`:71`) reads the class inside `make_unique<>`. `Tools/Atlas/Main.cpp` declares nineteen `*Types` (Bondage, Colony x2 sites, EconomyChronicle, Economy, Family, Lod, Market, Need, Norm, Organization, PersonChronicle, Person, Polity, Production, SocietyChronicle, Standing, Trade, Trait, Wealth) and sixteen systems (Bondage, Family, Life, Lod, Market, Mining, Need, Norm, Organization, Polity, Production, Standing, Stock, Trade, Trait, Wealth); it declares no Player, Gameplay, Politics-beyond-Polity, Military or Infrastructure type - the ADR-0135 wiring is Phases 01-06 plus the optional colony and chronicles, NOT "every type of 01-12" as the previous two rounds wrote; `--prehistory` defaults to 300 (`:500`); no `--replay`, `--empty`, `--panel`. `Tools/check_kernel_purity.py:636-639` prints `[purity] module X: N files` only under `verbose`; `:655` prints `[purity] N files, N violations` always; `Tests/CMakeLists.txt:21-22` runs it without `--verbose`; `Tools/kernel_modules.txt` lists twelve modules; `Docs/ROADMAP.md:3780` reads `purity 201/0 · shim-test 7/7 · parse 6 TU · wiring 3/3 agree`. `Tests/Player/Test_PlayerGate.cpp:1253-1254` `LifeYears = 40`, `DaysPerYear = 360`; `:1258` `Recorded`, `:1270` `Taking`, `:1628-1640` the replay loop bounded by those constants; the same private `Recorded` at `Test_Commands.cpp:436`, `Test_Doings.cpp:548`, `Test_GameplayGate.cpp:1361`, `Test_ColonyGate.cpp:1279`, and `Taking` at `Test_GameplayGate.cpp:1373`, `Test_ColonyGate.cpp:1291`. `Tests/Gameplay/CMakeLists.txt:11` links no `Vaelen::View`; `:36,48` `GameplayGate` `TIMEOUT 7200`, `COST 5000`; `Tests/` has no `Run` directory. `Docs/ROADMAP.md:2990-2992`: 6.05 s (repeats 6.24 / 6.40 / 6.52) to generate and run AELVOR at 256 for 420 years, release, on the owner's machine. `Regard.h:91` is the field `int32 ForTaking`. `PlayerHistory.h:67` `RecordRefusals`, `:83-86` `LifeChronicleTypes::Declare`; the gate wires the system as `make_unique<LifeChronicle>`. `Start.h:83` `BeginEnslaved`, `:96` `StartOf`; `Hours.h:114` `HoursLeft`; `Commands.h:128` `HoursOf[16]`. `VaelenView.Build.cs:29` lists eleven kernel modules as PUBLIC dependencies. `VaelenViewActor.cpp:519` `FAutoConsoleCommandWithWorldAndArgs`. `Test_ViewGate.cpp:61-62` the two frozen digests, asserted at `:323`. The 128/120 pair lives in `Docs/DECISIONS.md:9069` and `Docs/ROADMAP.md:3784`, in no CTest. `Docs/ARCHITECTURE.md:165,167` name `VaelenGame` and `VaelenUI`; `:252-254` are 3.3 rules 3-4. Last ADR is 0135 (`Docs/DECISIONS.md:8930`).

| Task | Content | Test kind | Done when |
|---|---|---|---|
| 14.01 | The command surface as a leaf. `Vaelen/Player/Intent.h` receives `Intent`, `Refusal`, `PlayerCommand` with its `static_assert(sizeof == 24)` (`Commands.h:99`), `IntentName`, `RefusalName`, and includes only `Vaelen/Core/CoreTypes.h` and `Vaelen/Player/PlayerApi.h`; `Commands.h` includes it back, nothing compiled today changes, `Submit(World&)` is not nameable from a file that includes only the surface. Beside it `Vaelen/Player/Stream.h`, THREE record kinds: `Recorded{Tick, Command, Verdict}` lifted from the five test files that declare it privately; `TakenUp{Tick, Person}` (the gate's `Taking`, renamed for a self-describing leaf - `Regard.h:91` is a field, no collision); and **`DayTurned{Tick}`, one per day turn, because the day turn is an input the host makes and a replay must know how many to make** - without it a replay stops at the last command's tick and every trailing Space is lost. `EncodeStream`/`DecodeStream` to and from one record per line under `vaelen-stream 1 <seed> <size> <prehistory> <years>` (the history BEFORE play; the played days are the count of `DayTurned` lines), no file I/O. ADR-0136: an addition to a closed Phase 10 API, no symbol renamed, no layout changed, no `VAELEN_SAVE_FORMAT_VERSION` bump; the stream is an input record, NOT a save | unit, edge, deterministic | `Player.Stream` green on the eight CTest legs: round trip byte-identical for all three record kinds; a corrupt line counted in `BadLines`, not crashed on; a header of another seed or size refused; `Player.IntentLeaf`: a probe TU including only `Intent.h` and `Stream.h` compiles with `-I Source/VaelenCore/Public -I Source/VaelenPlayer/Public` and its `-E` output carries no `Commands.h`, `Vaelen/Sim/` or `Vaelen/Population/` line marker; `Test_PlayerGate` replays through the lifted `Recorded`/`TakenUp` with its digests unmoved (its loop stays bounded by `LifeYears`/`DaysPerYear`, `:1253-1254`; it records no `DayTurned` - the Door does); `Kernel.Purity` `[purity] N files, 0 violations`; ADR-0136 applied. **DONE 2026-09-14** (PROTOTYPE; two commits - the change and the review's corrections) |
| 14.02 | The view headers as leaves. `Frame.h`, `Land.h`, `Net.h`, `Folk.h`, `Delta.h`, `Eye.h` keep only their structs, `RegionIn`, `GrainName`, `Measure*`, `Diff`, and include only `Vaelen/Core/CoreTypes.h`, `Vaelen/Core/Hash.h`, `Vaelen/View/ViewApi.h`, `<vector>` - and no `class World;` (`Frame.h:40`, `Land.h:39`, `Net.h:37`, `Folk.h:43` lose it). `ViewSources` and every `Take*(const World&, const ViewSources&, ...)` - `TakeView`, `TakeMapView`, `TakeNetView`, `TakePeopleView`, `TakeViewFor`, `BordersBetween` with their `RegionGraphCache&` - move to `Vaelen/View/Take.h`, the only VaelenView header allowed to include kernel modules and the one VaelenUI may never include; `Eye.h` drops `Vaelen/Sim/Regions.h` and keeps `Eye{Region, Reach, Most}` as plain numbers. The five `.cpp`, `VaelenViewActor.cpp`, eight view tests and `Tools/Atlas/Main.cpp` add `#include "Vaelen/View/Take.h"` (the twenty files of the grep, one line each); `VaelenViewDrawer.h` changes nothing. ADR-0137 (13.01 is closed): what moved, that no struct, field or layout moved, that the digests below are asserted unmoved | unit (probe TU), frozen digests, parse | `View.Leaf` green on the eight CTest legs: a probe TU including every `Vaelen/View/*.h` except `Take.h` plus `Vaelen/Player/Intent.h` compiles with `-I Source/VaelenCore/Public -I Source/VaelenView/Public -I Source/VaelenPlayer/Public` and nothing else; `grep -lE 'Vaelen/(Sim\|Population\|Society\|Economy\|Colony\|Infrastructure\|Gameplay\|Player/Player\|Player/Commands)' Source/VaelenView/Public/Vaelen/View/*.h` returns only `Take.h`; `View.ViewGate` still asserts `VAELEN_VIEWGATE_FROZEN_VIEW 0x115c2ff70a5327c4` and `_STATE 0x5bc8478689958433`; NEW `Atlas.Frozen128` (`VaelenAtlas --size 128 --years 120 --out ...` then `check_atlas_output.py --expect frame=0xabc5a5767c6cf9dd ground=0x8f7f4948f49b6e86`) green on the eight CTest legs - the first CI assertion of the ADR-0135 numbers, which today only `Docs/DECISIONS.md:9069` holds; `parse` job green with `VaelenViewDrawer.h` untouched; `Kernel.Purity` 0 violations; `Kernel.WorldWiring` `3 wirings of AELVOR agree`; ADR-0137 applied. **DONE 2026-09-14** (PROTOTYPE) |
| 14.03 | One wiring of a played AELVOR as the thirteenth kernel module `VaelenRun` (`Vaelen/Run/Aelvor.h`, rule 6's obligations). The DEFAULT wiring is the ADR-0135 wiring EXACTLY as `Tools/Atlas/Main.cpp` has it with neither `--colony` nor `--chronicle`: fifteen types in Atlas order, fifteen systems - it does NOT wire Phases 07-09 (Politics beyond Polity, Military, Infrastructure), because the Atlas does not and the frozen frame digest is of a world without them; a claim of "every type of 01-12" was the previous rounds' error. Behind `Options{Size, PreHistory=300, Colony, Play, Lively=false}`: `Colony` adds ColonyTypes + MiningSystem after Polity, as Atlas; `Play` adds Player, Start, Hour, Order, Regard, LifeChronicle types after those (the wiring checker's own rule for a type that shifts nothing) and PlayerDaySystem, PlayerOrderSystem, RegardSystem, LifeChronicle; `Lively` adds Living, Repute, Fame types and LivingSystem, ReputeSystem, FameSystem, JudgementSystem. `TakeUp()` = `BeginEnslaved` (`Start.h:83`, 0 when nothing is written); when the played person dies the next `Day()` takes up another and records a `TakenUp`. `Vaelen/Run/Door.h`: `Refusal Mean(PlayerCommand C)` sets `C.Issued = W.Now()` whatever the caller wrote, calls `Player::Submit`, appends `Recorded`; **`Day()` = `TickMany(24)` AND appends `DayTurned{Now()}` - the one host input that is not a gameplay command, and it is in the stream**; `Replay(Aelvor& Fresh, stream) -> {Answered, Wrong, Days, ByKind[9], State, Log, Life}`: for each `DayTurned` in order, submit every `Recorded` with `Tick <= Now()` (as `Test_PlayerGate.cpp:1634-1638`), then `Day()`, then take up any `TakenUp` due - bounded by the records, not by `LifeYears`. `Tools/Atlas` gains `--replay FILE` and `--empty`; the default path is untouched. `check_world_wiring.py`: a fourth `WIRINGS` entry, the two literal strings at `:138` (`all three agree`) and `:141` (`the three wirings`) become `len(WIRINGS)`, a `LISTEN = re.compile(r'(\w+)->Attach\(\)')` resolved through the same `MAKE` map (`:71`) and a third compared list, "listeners", beside declarations and systems - `LifeChronicle` is an `IEventListener` wired by `->Attach()`, which the `ADD` regex (`:72`, only `Systems().Add(X.get())`) never sees, as is already true of the three chronicle entries in `OPTIONAL_SYSTEMS` today - and BEFORE the first green the OPTIONAL lists are extended by name, each with "Run only, behind Play/Lively": `OPTIONAL_DECLARES` += Player, Start, Hour, Order, Regard, LifeChronicle, Living, Repute, Fame (4 + 9 = 13); `OPTIONAL_LISTENERS`, a new dict = PersonChronicle, SocietyChronicle, EconomyChronicle (moved out of `OPTIONAL_SYSTEMS`), LifeChronicle (4); `OPTIONAL_SYSTEMS` keeps MiningSystem and gains the seven optional systems by name: PlayerDaySystem, PlayerOrderSystem, RegardSystem, LivingSystem, ReputeSystem, FameSystem, JudgementSystem. `VaelenRun` added to `Tools/kernel_modules.txt` | unit, integration, deterministic, replay | `Run.Aelvor`, `Run.Door` green: with `Colony=Play=false` at 128/120 the frame digest is `abc5a5767c6cf9dd` and the ground `8f7f4948f49b6e86` (if either moves the task STOPS and an ADR says why); `Issued == Now()` for a command submitted with `Issued=0` and with `Issued=12345`; a 360-day round-robin of the eight verbs at 96, encoded, decoded, replayed into a fresh Run gives `Wrong=0`, `Days=360` and equal state/log digests; **a stream ending in five `DayTurned` after the last `Recorded` replays to the same state digest as the Run that wrote it, and the same stream with its last `DayTurned` line removed does not**; `Kernel.Purity` prints `[purity] 206 files, 0 violations` - 201 today plus the .h/.cpp under `Source/VaelenRun/{Public,Private}` minus `VaelenRunModule.cpp` (`Aelvor.h`, `Door.h`, `RunApi.h`, `Aelvor.cpp`, `Door.cpp` = 5), the counter counting only .h/.cpp under Public/Private and excluding `*Module.cpp` - and in the `parse` job `python3 Tools/check_kernel_purity.py --root . --verbose \| grep -c '^\[purity\] module '` prints `13`; `Kernel.WorldWiring` prints exactly `[wiring] 4 wirings of AELVOR agree (13 declarations, 8 systems and 4 listeners deliberately optional, each named)` and its three per-kind lines (declarations, systems, listeners) read `all 4 agree`; `Atlas.Frozen128` still green; the log carries `run: day mean N ms, max M ms at 256 with the colony` (LOGGED, not asserted - ADR-0109). **DONE 2026-09-14** (PROTOTYPE; purity 210 files, not 206 - the row was derived from 201 before 14.01/14.02 added five) |
| 14.04 | `LifeView` (`Vaelen/View/Life.h`), a leaf like 14.02's: Person, Region, Year, Day, Awake/Spent/Left hours (`Hours.h:114`), Food/Health/Rest 0..255 (`Needs.h:38-40`), the start (`Start.h:96 StartOf`), the queue (Held/Taken/Refused/Dropped, `LastRefusal`, `Waiting[8]`), the regard (`Regard.h:151 ReputeOf`, Kindnesses, Wrongs, `Known[8]`), `Cost[16]` from `OrderRules::HoursOf` (`Commands.h:128`), `Near[8]` from `RegionGraph::AreAdjacent` (`Regions.h:95`) - the Move targets that will not be refused `TooFar`, the first feed for `Intent::Move` since 16b - `Company[16]`, fixed `char[32]` names. `Life.h` includes only `CoreTypes.h`, `Hash.h`, `ViewApi.h`, `Player/Intent.h`; not `Frame.h`, not `Take.h`. **`TakeLifeView(const World&, const ViewSources&, WorldGen::RegionGraphCache& Ways, LifeView&)` is declared in `Take.h`, mirroring `TakeViewFor` (`Eye.h:61-62`): the graph is a walk of the whole map, so the cache belongs to the caller - the subsystem of 14.08 and the test own one; no static.** `ViewSources` gains `HasLife` + the Hour/Order/Regard/Start/Need/Family types; `Life.cpp` is the one TU including Hours.h, Commands.h, Regard.h, Start.h, Needs.h, Families.h, Regions.h. `Person = 0` and every name empty when nobody is played. ADR-0137 covers the widening (views are never persisted: no save-format bump) | unit, edge, deterministic, integration with 10.02-10.06 | `View.Life` green: `sizeof(LifeView)` equals its fields, trivially copyable, standard layout; on a played 96-map every number equals the kernel accessor named above and every `Near` entry is one `AreAdjacent` affirms; sixty takes leave state and log digests unchanged **and `Ways.Builds() == 1` after them (`Regions.h:115`)**, the take's ms logged; the view outlives the world (Test_Land's brace, `:173`); every name byte ASCII; `View.Leaf` (now including `Life.h`) still green and its `-E` output has no `Commands.h`, `Vaelen/Sim/`, `Vaelen/Population/` marker; `VAELEN_VIEWGATE_FROZEN_VIEW` unmoved. **DONE 2026-09-14** (PROTOTYPE) |
| 14.05 | `ChronicleView` (`Vaelen/View/Chronicle.h`), a leaf: the last 32 lines about the played person as ONE flat `char` buffer plus `LineView{Tick, Year, Kind, Begin, Length}` - no `std::string`, no handle - from `DescribeLifeEvent` over `LifeTimeline` with `RecordRefusals=1` (`PlayerHistory.h:67`) so a refusal is a sentence carrying its `RefusalName`, plus the why of the newest record (`ExportWhyWithLife`, two steps; deeper is Phase 17). `TakeChronicleView` in `Take.h`, INCREMENTAL from `ChronicleView::Since`; `MeasureChronicleView` reports Lines, Bytes, EventsRead, NonAscii, Truncated, Digest. `Chronicle.h` includes `CoreTypes.h`, `Hash.h`, `ViewApi.h`; `Chronicle.cpp` includes `PlayerHistory.h`, `Sim/History.h` | text, deterministic, edge, integration with 10.07 | `View.Chronicle` green: lines byte-equal to the matching lines of `ExportLife`; grown over 360 days equals a fresh take byte for byte, `EventsRead` per day printed; a refused Eat with no grain yields a line containing `Nothing`; `NonAscii 0`, `Truncated 0`, `Bytes <= 8192` on the 96-map round-robin; trivially copyable; the view outlives the world; `View.Leaf` still green with `Chronicle.h` added. PROTOTYPE **DONE 2026-09-14** (PROTOTYPE) |
| 14.06 | The first screen composed kernel-side: `Vaelen/View/Panel.h`, a leaf including `Frame.h`, `Life.h`, `Chronicle.h`, `Player/Intent.h`, with `TakePanel(const WorldView&, const LifeView&, const ChronicleView&, PanelView&)` - views in, lines out, no World and no `ViewSources`, nothing from `Take.h`. The page: date, self (`bound to <holder> since year N`), body (Food/Health/Rest), hours (`hours left X of Y`, held/taken/refused, `last: TooFar`), the eight verbs with cost, key and `Offered` (0 when nobody is played, dead, queue full or `Cost > Left`) with the foreseeable refusal, neighbours, company, chronicle lines, and as its LAST line `MeasurePanel().Digest` in hex. `Press(const PanelView&, Verb, Target, Amount, PlayerCommand& Out)` refuses an unoffered verb and otherwise fills Kind/Target/Amount with `Issued = 0`. `Lines(const PanelView&, char* Out)` writes the rows the widget draws and the test hashes. `Tools/Atlas --empty --panel` and `--replay FILE --panel` print those rows | unit, deterministic, two frozen digests, one measured line, edge | `View.Panel` green: **`VAELEN_PANEL_FROZEN_EMPTY`** on the unplayed 128/120 Run (reproduced byte for byte by `Tools/Atlas --empty --size 128 --years 120 --panel`, CTest `Atlas.PanelEmpty`) and **`VAELEN_PANEL_FROZEN_PLAYED`** on the 96-map round-robin after 30 days, **both held on the eight CTest legs: the six Linux presets (clang/gcc x debug/release/noasserts), `windows-msvc-debug`, `macos-debug` - MSVC release and AppleClang release are not built by any job and are not claimed**; `Bytes <= 4096`, `Lines <= 48`, every byte ASCII; queue 8/8 greys every verb with `Full` foreseen; a Press on an unoffered verb is refused before any world is asked. The two panel digests need no gate-length build (both worlds are ones 14.03 already builds); the 256 page is a MEASURED line only: `Run.Aelvor`'s already-logged 256 day gains `panel: bytes B, lines L, truncated 0, digest <16hex>`. The one 256 build CI does pay for is `Replay.Played`, costed in 14.10. PROTOTYPE **DONE 2026-09-14** (PROTOTYPE) |
| 14.07 | The UI's includes, read by the CI that cannot build the UI. `Tools/parse_engine_modules.py`: `ENGINE_MODULES` gains `VaelenGame` and `VaelenUI`; **`stub_generated_headers` runs ONCE over every module in `ENGINE_MODULES` into ONE shared generated dir created before the per-module loop** (today `:83-106` scans only the module being parsed, into a dir inside the loop at `:134-136`, so `VaelenWorldSubsystem.generated.h` from `VaelenGame/Public` would be `file not found` while VaelenUI is parsed - the control would fail on its first line; latent today only because `VaelenViewActor.h:61` and `VaelenAtlasActor.h:21` each sit inside their own module); `includes` becomes per-module - every module keeps `[SHIM, generated, public] + kernel_include_dirs()` except `VaelenUI`, parsed with `[SHIM, generated, VaelenUI/Public, VaelenGame/Public, VaelenView/Public, VaelenCore/Public, VaelenPlayer/Public]` and no other kernel dir, so `Take.h`, `Commands.h`, `Sim/World.h` are `fatal error: file not found` from a real front end, and `Vaelen::World` is an unknown name because after 14.02 no reachable header declares it. Under `--verbose` it prints `[parse] generated stubs: <n> (<names>)` once and `[parse] VaelenUI: N translation units, OK`; a `--stub-scope own` flag restores today's per-module scan for the self-test. `Tools/check_ui_fence.py` (CTest `Kernel.UiFence`, a step of the `parse` job): every `Vaelen/` include under `Source/VaelenUI` and in `Source/VaelenGame/Public` is one of `Vaelen/View/{Frame,Land,Net,Folk,Delta,Eye,Life,Chronicle,Panel,ViewApi}.h`, `Vaelen/Player/{Intent,Stream,PlayerApi}.h`, `Vaelen/Core/*.h`; refused prefixes `Vaelen/View/Take.h`, `ViewSources`, `Vaelen/Sim/`, `Vaelen/Population/`, `Vaelen/Society/`, `Vaelen/Economy/`, `Vaelen/Colony/`, `Vaelen/Infrastructure/`, `Vaelen/Gameplay/`, `Vaelen/Run/`, `Vaelen/Player/Commands.h`; the script walks the transitive closure of every allowed header (regex over `#include "Vaelen/..."` from `Source/*/Public`, as p14_closure.txt was made) and fails if any closure leaves `Vaelen/View/`, `Vaelen/Core/`, `Vaelen/Player/{Intent,Stream,PlayerApi}.h`. **Token ban as word-bounded regexes, not substrings**: `\bSubmit\s*\(`, `\bVaelen::World\b`, `\bTakeView\w*\s*\(`, `\bTakeLifeView\s*\(`, `\bBeginEnslaved\s*\(`, `\bVaelen::Run\b`, `\bFPlatformTime\b`, `\bFDateTime\b`, `\bDeltaSeconds\b`, `\bTick\s*\(`, `\brand\s*\(`; `TakePanel` exempt (it takes views); `--self-test` mutates a copy for each and carries a CONTROL that a UI file containing the comment `the operand` and the identifier `Ticker` passes. `Tools/EngineShim` gains `GameFramework/HUD.h`, `Engine/Canvas.h`, `GameFramework/PlayerController.h`, `GameFramework/GameModeBase.h`, `Components/InputComponent.h`, `InputCoreTypes.h`, `Subsystems/GameInstanceSubsystem.h`, `Engine/GameInstance.h` (with `template <class T> T* GetSubsystem()`), `Misc/FileHelper.h`, `Misc/Paths.h`, **and the existing `Engine/World.h` gains `UGameInstance* GetGameInstance()`** so 14.08's console commands parse (`HAL/IConsoleManager.h` exists). The honest limit in both docstrings: `VaelenView.Build.cs:29` makes every kernel include path transitive under UBT, so this closure check is the only thing standing between the UI and `Commands.h` | self-test (mutations, control first), checked output | `test_engine_shim.py`: the CONTROL is the healthy VaelenUI parsed with `VaelenWorldSubsystem.h` included and its generated header stubbed from the shared dir (possible only because of 14.02 AND the shared stub dir); then the existing 7 mutations plus: `#include "Vaelen/View/Take.h"` added to `AVaelenHUD.cpp`; `#include "Vaelen/Player/Commands.h"`; `#include "Vaelen/Sim/World.h"`; `Vaelen::World* W = nullptr;` in a UI TU; **`AVaelenHUD.cpp` reading `Life.HoursRemaining` where `LifeView` has `Left` - the mutation lives in an ENGINE file and the real front end catches it against the unmodified kernel header (a kernel-side rename is NOT a case: the pristine copy holds only `ENGINE_MODULES`, `:96-100`, and `kernel_include_dirs()` reads `ROOT`)**; `Press()` called with an argument dropped; and a REVERSE control: the healthy copy parsed with `--stub-scope own` must FAIL with `VaelenWorldSubsystem.generated.h` not found, proving the shared stub is load-bearing (deleting the `.generated.h` include is not a catchable case while `GENERATED_BODY(...)` is a no-op at `CoreMinimal.h:93`, and it is not listed) - each caught, none stale. `Kernel.UiFence` green on the healthy tree; `Kernel.UiFenceSelfTest` catches the grep-only cases (`FDateTime::Now()`, a `Submit(` call, `DeltaSeconds`, a header whose closure reaches `Vaelen/Sim/` through an allowed include) and passes the `operand`/`Ticker` control. The `parse` job prints `grep -ho '[A-Za-z]*\.generated\.h' Source/Vaelen*/Public/*.h Source/Vaelen*/Private/*.cpp \| sort -u \| wc -l` beside the `--verbose` line `[parse] generated stubs: <n> (<names>)` and the two are equal - on this plan's own file set n is 6 (VaelenAtlasActor, VaelenGameMode, VaelenHUD, VaelenPlayerController, VaelenViewActor, VaelenWorldSubsystem); today's tree gives 2. Not one STATUS line moved (ADR-0134's rule). VALIDATED for the scripts, which is all they claim **DONE 2026-09-14** (VALIDATED for the scripts) |
| 14.08 | `VaelenGame`, the Unreal module `ARCHITECTURE.md:165` names (added to `Vaelen.uproject` and both targets): `UVaelenWorldSubsystem : UGameInstanceSubsystem` holds `Run::Aelvor`, `Run::Door` and one `WorldGen::RegionGraphCache` behind a pimpl; the PUBLIC header includes `CoreMinimal.h`, `Subsystems/GameInstanceSubsystem.h`, the leaf view headers, `Player/Intent.h`, `Player/Stream.h`, the `.generated.h`, and names no World; it exposes `Begin(size, years)`, `AdvanceDay(n)` (n `Door::Day()` calls, each a `DayTurned` record, then re-takes World/People/Life/Chronicle/Panel), `Refusal Mean(const PlayerCommand&)` (which re-takes Life and Panel after every call, as `AdvanceDay` does after its days, so a stale panel is never how a door refusal is produced), const view accessors, `WriteStream()` via `FFileHelper` to `Saved/Vaelen/<seed>-<size>.stream`. NO Tick: the world moves on `AdvanceDay` only. **ADR-0138 says in so many words: the day turn is the one host input that is not a gameplay command; it is recorded in the stream as `DayTurned`; rule 1 is read as "no write but commands and the recorded day turn"; paused unless asked - the fixed-step question is decided here.** Console commands `Vaelen.Play [size] [years]`, `Vaelen.Day [n]`, `Vaelen.Do <verb> [target] [amount]`, `Vaelen.Stream.Write`, each resolving the subsystem through the command's `UWorld*` -> `GetGameInstance()` -> `GetSubsystem<>()` and printing `LogVaelenPlay: no game instance - run with -game` and returning when there is none. ONE format string, quoted identically here, in 14.10 and in ADR-0138, printed by `Vaelen.Stream.Write`: `LogVaelenPlay: AELVOR <size> seed <12hex>: played <name> (person P, region R) <D> days, N intents (T taken, R refused by the world, X dropped at the door), state <16hex>, log <16hex>, life <16hex>, panel <16hex>` followed by `LogVaelenPlay: verbs work W rest R eat E wait T speak S give G take K move M` (the `ByKind[9]` of the Door, minus None). T/R/D are the queue's own life counters, `OrderStats{Taken, Refused, Dropped}` (`Commands.h:235-237`), already carried by 14.04's `LifeView`: `Door::Mean` returns what `Player::Submit` returns, which is only `NoPlayer`, `Unknown`, `Full`, or `None` (and `None` means QUEUED, not done); the world's refusals (`Dead`, `Costly`, `Stale`, `Nothing`, `TooFar`, `NoOne`) are applied later by `PlayerOrderSystem`, counted in `OrderStats::Refused`, and never come back from `Submit`. The Door's verdict tally (`Answered`/`Wrong`, `ByKind`) is kept only for replay equality; through `Press()` with a fresh panel every door verdict is expected to be `None`. Per day: `LogVaelenPlay: day D year Y: H held, T taken, R refused, X ms`. The exact invocation: `UnrealEditor-Cmd.exe Vaelen.uproject -game -nullrhi -unattended -log -ExecCmds="Vaelen.Play 256 100, Vaelen.Do work, Vaelen.Day 30, Vaelen.Stream.Write, quit"` (13.07c's `Vaelen.View` is a `FAutoConsoleCommandWithWorldAndArgs` that built its own world, `VaelenViewActor.cpp:519`; this one needs a GameInstance, hence `-game`). `VaelenPresentation` is not touched | parse (14.07) headless; one pasted log line on the owner's machine | Headless: `parse` job reports 4 engine modules, 0 errors; `Kernel.UiFence` and `Kernel.WorldWiring` green. Owner: UBT `Result: Succeeded` (same build as 14.09); the invocation above prints the two `LogVaelenPlay:` lines with `D = 30`, `N = 1`, and `Tools/Atlas --replay Saved/Vaelen/41454c564f52-256.stream --panel` on Linux prints `replay: 1 of 1 answered identically, 30 of 30 days` then the same two lines byte-identical after the `LogVaelenPlay: ` prefix (the stream carries one `Recorded` and thirty `DayTurned`, so the replay runs the thirty days the engine ran); `Vaelen.View 128 120` still prints `frame abc5a5767c6cf9dd, ground 8f7f4948f49b6e86`. If the failure line appears instead, the fallback is stated now: hold Run/Door in a `UEngineSubsystem` reached by `GEngine->GetEngineSubsystem<>()`, same public header, one more build. UNVERIFIED until the line is pasted **DONE 2026-09-14** (UNVERIFIED: headless half done, the engine build is the owner's) |
| 14.09 | `VaelenUI`, the first screen: `AVaelenHUD::DrawHUD` draws `Lines()` of the subsystem's cached `PanelView` with `Canvas->DrawText`, verbatim, so the bytes on screen are the bytes `View.Panel` digests; `AVaelenPlayerController` binds keys with `UInputComponent::BindKey` (`UEnhancedInputComponent` derives from it, so `DefaultInput.ini`'s class still takes it; no asset, no UMG, no Blueprint): W work, R rest, E eat, T wait, S speak, G give, K take, M move, Tab cycles the target through `Company`/`Near`, Space = `AdvanceDay(1)` (one `DayTurned`), F9 = `WriteStream`; a held key is one press. Every verb key goes through `Press()` and hands the struct to `Mean` - the module's single write; Space is the recorded day turn of ADR-0138. `AVaelenGameMode` in VaelenUI names the HUD and controller; one line `GlobalDefaultGameMode=/Script/VaelenUI.VaelenGameMode` under `[/Script/EngineSettings.GameMapsSettings]` in `DefaultEngine.ini` (config the parse cannot see). Build.cs: Core, CoreUObject, Engine, InputCore, VaelenCore, VaelenView, VaelenPlayer, VaelenGame - not VaelenPresentation | restricted parse + `Kernel.UiFence` (headless); UNVERIFIED until built; seen at 14.10 | Headless: VaelenUI parsed under the restricted include set, 0 errors; `Kernel.UiFence` green. Owner: UBT `Result: Succeeded` in the same build as 14.08; the log carries `LogVaelenUI: <verb> -> queued\|<door refusal>` for each of the eight verbs pressed once in PIE (`queued` expected for every verb the panel offered; these lines are the door's verdicts, not a count of refusals - the world's refusals are read from 14.08's line and the chronicle). The screenshot is the gate's. UNVERIFIED until built **DONE 2026-09-14** (UNVERIFIED: written, parsed and fenced; the build and the screen are the owner's) |
| 14.10 | Phase 14 gate: a month played by hand at 256, replayed by no hand. On the T400: `Vaelen.Play 256 100` in the 13.09 level (the Phase 13 actor's 72649 instances drawn from its own world, the HUD over them), thirty days through the keys - every verb at least once, one refusal read on screen, one Move to a `Near` neighbour, Space thirty times - then F9; `stat fps` read with the HUD up between day steps; one screenshot whose last line is the panel digest. No new code: the lines it reads are 14.08's. Headless: `Tools/Atlas --replay --panel` of that stream, checked in as `Tests/Run/Streams/aelvor256-<date>.stream`, CTest `Replay.Played` in `Tests/Run/CMakeLists.txt` pinning its four digests, **costed: `TIMEOUT 1800`, `COST 4000`; expected release cost about 1.5 s of generation (`Docs/ROADMAP.md:2990`: 6.05 s for 256 x 420 years, so 100 years is a quarter of it) plus 30 detailed days of one region, the debug multiple unknown until measured - the first Linux run logs `replay: 256/100 + 30 days in N s (release), M s (debug)`; it stays on `windows-msvc-debug` unless M > 600 s, in which case `-E Shuffled` becomes `-E 'Shuffled\|Replay.Played'` and (e) names seven legs**. Then the close against section 2: ROADMAP, ARCHITECTURE (VaelenRun in the kernel map; VaelenGame and VaelenUI in the Unreal map, 3.3 rules 3-4 now describing modules that exist, rule 4 with ADR-0138's reading), DECISIONS (ADR-0136..0138), STATUS, every PROTOTYPE with its limit | measured on the machine that has the engine, then replay in CI | All of: (a) the 14.08 format, verbatim: `LogVaelenPlay: AELVOR 256 seed 41454c564f52: played <name> (person P, region R) 30 days, N intents (T taken, R refused by the world, X dropped at the door), state <16hex>, log <16hex>, life <16hex>, panel <16hex>` with N >= 30, R >= 1 (world refusals, `OrderStats::Refused`), and the `LogVaelenPlay: verbs ...` line with every count >= 1 and `move M` counted among the taken (`Taken` includes a Move, its `PlayerRefusedEvent` absent); (b) `Tools/Atlas --replay --panel` prints `replay: N of N answered identically, 30 of 30 days` and the same two lines byte-identical after the prefix; (c) fps >= 90 (<= 11.1 ms) with the HUD over the 13.09 scene between day steps (the world does not tick per frame, so no year-turn frame exists to exclude); ms per `Vaelen.Day` at 256 written down, reported not gated; (d) one screenshot showing a refusal sentence whose `RefusalName` is among the R world refusals of (a) and the panel digest of (a) as its last line; (e) **`Replay.Played` green on the eight CTest legs (six Linux presets, `windows-msvc-debug`, `macos-debug`) and `Kernel.UiFence` green in the `parse` job** - `format` and `parse` run no ctest. Any one missing and the phase stays open |

### First screen

THE LIFE SCREEN: one played person, their day, and their verbs - Canvas text drawn from a `PanelView` composed of `LifeView`, `ChronicleView` and `WorldView` and nothing else: who they are and where, `bound to <holder>`, the date, hours left, Food/Health/Rest, the eight verbs with cost and whether the page offers them, the neighbours a Move will not be refused for, the company a Speak/Give/Take can be aimed at, the last lines of the life with every refusal as a sentence, and the panel's own digest as its last row. Chosen over a map or a character sheet because 10.02 says the start is "a place in a world, not a character sheet", 13.07c-13.09 already drew the map (the read half only), and this is the smallest screen that exercises BOTH halves of the phase sentence on every keypress: every line comes from a view (through leaf headers that cannot name a World), every verb key becomes a `PlayerCommand` handed to `Mean`, the verdict comes back as a view field (the door's, which through `Press()` is `None` - queued; the refusal read on screen is the WORLD's, from the chronicle line and `OrderStats::Refused`, not the door's), and it is the first thing to feed `Intent::Move`. Because the world advances only on Space, Space is a `DayTurned` record, and `Issued` is stamped from `W.Now()` by the Door, the stream is a function of the key sequence alone - Space included - which is what makes the gate a replay rather than an adjective. Canvas text rather than UMG or Slate: legible in a screenshot, parseable by the shim, nothing the owner must author.

### Includes per engine-side task

- **14.08 `VaelenWorldSubsystem.h` (public, VaelenGame)**: `CoreMinimal.h`, `Subsystems/GameInstanceSubsystem.h`, `Vaelen/View/Frame.h`, `Land.h`, `Net.h`, `Folk.h`, `Life.h`, `Chronicle.h`, `Panel.h` (leaves after 14.02), `Vaelen/Player/Intent.h`, `Vaelen/Player/Stream.h`, `VaelenWorldSubsystem.generated.h` (stubbed from the shared dir when VaelenUI is parsed). Walked by `check_ui_fence.py`'s closure check because the UI includes it.
- **14.08 `VaelenWorldSubsystem.cpp` (private, the one engine file that holds a world)**: `Vaelen/Run/Aelvor.h`, `Vaelen/Run/Door.h`, `Vaelen/View/Take.h`, `Vaelen/Player/Commands.h`, `Vaelen/Sim/Regions.h` (the cache it owns for `TakeLifeView`), `Engine/World.h`, `Engine/GameInstance.h`, `HAL/IConsoleManager.h`, `Misc/FileHelper.h`, `Misc/Paths.h`. Allowed: this IS the gameplay-layer module of 3.3 rule 4. `VaelenGame.Build.cs`: Core, CoreUObject, Engine, VaelenCore, VaelenView, VaelenPlayer, VaelenRun and the kernel modules Run links.
- **14.09 `Source/VaelenUI`**: `Vaelen/View/Panel.h`, `Life.h`, `Chronicle.h`, `Frame.h`, `Folk.h` (leaves; closure inside `Vaelen/View/`, `Vaelen/Core/`, `Vaelen/Player/Intent.h`); `Vaelen/Player/Intent.h`; `VaelenWorldSubsystem.h`; `GameFramework/HUD.h`, `Engine/Canvas.h`, `GameFramework/PlayerController.h`, `GameFramework/GameModeBase.h`, `Components/InputComponent.h`, `InputCoreTypes.h`. NOT included, and file-not-found under the restricted parse: `Vaelen/View/Take.h`, `Vaelen/Player/Commands.h`, anything under `Vaelen/Sim`, `Vaelen/Population`, `Vaelen/Run`, `Vaelen/Gameplay`, `Vaelen/Colony`, `Vaelen/Society`, `Vaelen/Economy`, `Vaelen/Infrastructure`; `FDateTime`, `FPlatformTime`, `DeltaSeconds` refused by the fence's word-bounded regexes.
- **14.10**: no new code.

### What needs the owner's machine

- One UBT build for 14.08 + 14.09 together, verified by the pasted `-game -nullrhi` lines of 14.08 (two `LogVaelenPlay:` lines, byte-compared after the prefix against `Tools/Atlas --replay --panel`) and the eight `LogVaelenUI:` lines of 14.09. If `LogVaelenPlay: no game instance` appears, the `UEngineSubsystem` fallback costs one more build - named now so it is a decision, not an experiment.
- One editor sitting for 14.10: thirty days by hand, F9, `stat fps`, one screenshot, the stream copied out. Same afternoon as the build if the parse held.
- Everything else (14.01-14.07, the headless half of 14.10) is green on the eight CTest legs and the `parse` job before the engine machine is touched.

### Risks

- **The fence under UBT is a script, not the compiler**: `VaelenView.Build.cs:29` makes every kernel include path transitive, so VaelenUI CAN include `Take.h` and build. The restricted parse is a real front end in CI; the closure walk is the second net; the docstrings say so.
- **`Run.Aelvor`'s idle-digest claim** (`abc5a5767c6cf9dd` unmoved with nobody played) rests on the Run's default wiring being the Atlas wiring verbatim and on `Play`/`Lively` types being declared after it; the previous rounds' "every type of 01-12" would have moved it by construction (Military and Politics systems act every year). If it still moves, stop and ADR; `Atlas.Frozen128` catches it in CI, not on the T400. Phases 07-09 in the Run are Phase 15+ work if a screen ever needs them.
- **The day turn as a recorded input** is a reading of rule 1 ("no write but commands and the recorded day turn"), stated in ADR-0138 rather than assumed; the truncated-stream test in 14.03 is what shows the record is load-bearing.
- **Three closed-phase changes under ADR** (Intent/Stream out of Commands.h; the view-header leaf split; `ViewSources` widened): no layout moves, no save-format bump, the stream file is not a save. 14.02 touches twenty files by one include line each - the price of rule 1 being an include rule.
- **The cross-module `.generated.h` stub** is the first time the parser crosses modules; the reverse control (`--stub-scope own` must fail) is the reason to believe the shared dir is doing the work rather than a wider include path.
- **`-game` and the GameInstance**: the pasted-line check depends on `-game`; the failure line names the alternative. `GlobalDefaultGameMode` in ini is unparseable headless and only shows at 14.10.
- **Legacy `BindKey` under `EnhancedPlayerInput`**: `DefaultInput.ini` selects the Enhanced classes; `BindKey` is inherited and expected to work. If 5.6 ignores it, the fallback is `NewObject<UInputAction>`/`UInputMappingContext` built in C++ at startup - still no asset - and `Tools/EngineShim` gains the two stubs.
- **`Replay.Played` on `windows-msvc-debug`** is the one 256 build CI pays for in this phase; its debug cost is measured on the first Linux run and the 600 s rule above decides whether it joins `Shuffled` in the `-E`.
- **Two worlds at PIE start at 256**: the Phase 13 actor's and the subsystem's; CPU memory doubles for the build; feeding the drawer from the live world is Phase 15's first task.
- **Day cost at 256 in the editor is unmeasured**: 14.03 logs the headless number first; the gate reports ms per `Vaelen.Day` and does not gate it; `Lively=false` keeps Phase 12's systems off the bill.
- **The shim grows** (ten new stubs and two members on existing ones): ADR-0134's hazard; the control-must-pass case, the reverse control and the new mutations are the reason to believe it.
- **`BeginEnslaved` can return 0**: the subsystem prints `nobody to be`, the panel is the EMPTY page, the `played` field of the log line is empty.
- **Strangers have no names**: `PersonView` carries an `Identity` hash only; company beyond the named few is index+age+sex.
- **Chronicle truncation**: `Truncated` asserted 0 on the round-robin world; the buffer grows once if not.
- **Cut, by name**: map/atlas screen, camera, per-frame `Eye`/`Delta`, inspectors (17), family trees, document/map verbs, save/load (16), UMG/Slate, Enhanced Input assets, `DrawLive`, moving the two existing actors onto `VaelenRun`, Phases 07-09 in the Run.

### Changes from the previous round

- **Finding: Space is a key and not in the stream (rule 2; rule 1's one non-command write)**: taken - `Stream.h` gains `DayTurned{Tick}` (14.01), `Door::Day()` appends it and `Replay` is bounded by the records, returning `Days` (14.03), with the two new done-when cases (five trailing day turns replay to the same state digest; the stream minus its last day turn does not); ADR-0138 states the day turn as the one host input that is not a gameplay command, recorded, and the reading of rule 1 (14.08); 14.08's `Vaelen.Day 30` now yields thirty records so its byte comparison against `Atlas --replay` is measurable, and "First screen" says "Space included".
- **Findings: the restricted CONTROL cannot parse because `.generated.h` stubs are per-module (two findings, one defect)**: taken - `stub_generated_headers` runs once over every `ENGINE_MODULES` entry into one shared dir before the loop, the docstring says so, the control criterion is stated as "VaelenUI parses with `VaelenWorldSubsystem.h` included and its generated header stubbed", the `--verbose` line quoted is the shared count (3), and `Engine/World.h` `GetGameInstance()` plus `Engine/GameInstance.h` `GetSubsystem<T>()` are listed among the shim changes. Not taken as stated: the mutation "delete the `.generated.h` include from the subsystem header must be caught" - `CoreMinimal.h:93` makes `GENERATED_BODY(...)` a no-op, so no parser can catch it; replaced by a REVERSE control (`--stub-scope own` on the healthy copy must fail) that proves the same thing the finding wanted proved.
- **Finding: the `LifeView` field-rename mutation cannot run** (`test_engine_shim.py:96-100` copies only engine modules; `kernel_include_dirs()` reads `ROOT`): taken - turned around into `AVaelenHUD.cpp` reading `Life.HoursRemaining`, caught against the unmodified kernel header; the kernel-side rename is dropped rather than costed.
- **Finding: bare-token ban fails the healthy tree** (`rand` in "operand", `Tick(` in an override): taken - word-bounded regexes listed in 14.07, with an `operand`/`Ticker` control in `--self-test`.
- **Finding: `Near[8]` needs a built `RegionGraph`** (`Eye.h:57-62`, `Doings.h:99`): taken - `TakeLifeView(const World&, const ViewSources&, WorldGen::RegionGraphCache& Ways, LifeView&)` in `Take.h`; the subsystem and the test own the cache; `Ways.Builds() == 1` after sixty takes, using the counter `Regions.h:115` already provides.
- **Finding: 14.08's line and 14.10(a)'s line differ while 14.10 has no new code**: taken - one format string in 14.08, 14.10 and ADR-0138, plus the `LogVaelenPlay: verbs ...` line so `ByKind` is on the pasted evidence; `Atlas --replay --panel` prints `replay: N of N answered identically, D of D days` then the same two lines, so (b) is a byte comparison after the category prefix.
- **Findings: "ten CI jobs that run CTest" (twice) and "gcc, clang, MSVC, AppleClang, debug and release"**: taken - eight CTest legs named (six Linux presets, `windows-msvc-debug`, `macos-debug`), `Kernel.UiFence` in the `parse` job; the per-compiler debug/release claim dropped since `windows-msvc-release` (`CMakePresets.json:150`) is built by no job.
- **Finding: `Replay.Played` uncosted and 14.06's "no second gate-length build anywhere"**: taken - `TIMEOUT 1800`, `COST 4000`, the expected release cost derived from `Docs/ROADMAP.md:2990`, a measured line for the debug number, the 600 s rule for `-E` on Windows; 14.06's sentence qualified to the two panel digests and the 256 build named as `Replay.Played`'s.
- **Finding: `Kernel.Purity` prints no module count**: taken - `VaelenRun` in `Tools/kernel_modules.txt` (twelve today), the printed `[purity] N files, 0 violations` with N grown by six, and `--verbose | grep -c '^\[purity\] module '` = 13 in the `parse` job.
- **Finding: `check_world_wiring.py` prints "all three agree" over four columns, and the OPTIONAL names are unlisted**: taken - the two literals at `:138` and `:141` become `len(WIRINGS)`, the OPTIONAL names are enumerated in 14.03 (nine declares, eight systems, verified against the `*Types` structs of VaelenPlayer/VaelenGameplay and the `make_unique<>` class names the gate uses, `LifeChronicle` among them), and the exact expected last line is quoted: `[wiring] 4 wirings of AELVOR agree (13 declarations and 12 systems deliberately optional, each named)`.
- **A correction the findings did not ask for but the enumeration exposed**: the Run's default wiring is the Atlas wiring (fifteen types, fifteen systems; Phases 01-06 plus optional colony and chronicles), not "every type and system of Phases 01-12" as both earlier rounds wrote; that claim contradicted the frozen-digest done-when by construction and is withdrawn. Phases 07-09 are named under "Cut".
- Carried unchanged from round two: findings 1-7 of the first skeptic pass (leaf split as 14.02, restricted parse dependent on it, `TakeLifeView` in `Take.h`, grep-only mutations in `Kernel.UiFenceSelfTest`, two panel digests, the `-game` invocation with its failure line, `TakenUp`), and the judges' first-round violations (leaf grep covering Colony/Economy/Gameplay/Infrastructure/Society, `IConsoleManager.h` not claimed as new, Atlas writing JSON not a line, `Atlas.Frozen128` added, 3.3 rules 3-4 named at the close, the ini line as config, `BindKey` as a stated risk). Paths to `DECISIONS.md` and `ROADMAP.md` now carry their `Docs/` prefix, which the previous round omitted.
- **Round-3 fix: the two refusal counters were conflated** (`Door::Mean` returns what `Player::Submit` returns - only `NoPlayer`, `Unknown`, `Full`, or `None`, and `None` means queued, not done; the world's `Dead`/`Costly`/`Stale`/`Nothing`/`TooFar`/`NoOne` are applied later by `PlayerOrderSystem`, counted in `OrderStats::Refused`, and never come back from `Submit`, so through `Press()` with a fresh panel the door verdict is always `None` and a gate clause "F >= 1 refused" on the door tally was unattainable): taken - the one format string in 14.08 and 14.10 now reads `N intents (T taken, R refused by the world, X dropped at the door)` from `OrderStats{Taken, Refused, Dropped}` (14.04's `LifeView`), the Door's `Answered`/`Wrong`/`ByKind` kept only for replay equality and expected all `None` through `Press()`; the subsystem re-takes Life and Panel after every `Mean` as well as after `AdvanceDay`; 14.09's line is `LogVaelenUI: <verb> -> queued|<door refusal>` with no refusal count read from it; 14.10(a) requires `R >= 1`, (d)'s screenshot refusal is one of those R by `RefusalName`, and the Move check is "`Taken` includes a Move (its `PlayerRefusedEvent` absent)"; "First screen" says the refusal read on screen is the world's, not the door's.
- **Round-3 fix: `[parse] generated stubs: 3 (...)` hardcoded a count that is wrong on the healthy tree** once VaelenUI declares `AVaelenHUD`, `AVaelenPlayerController` and `AVaelenGameMode`: taken - 14.07 quotes `[parse] generated stubs: <n> (<names>)` and its done-when cross-checks `<n>` against `grep -ho '[A-Za-z]*\.generated\.h' Source/Vaelen*/Public/*.h Source/Vaelen*/Private/*.cpp | sort -u | wc -l`, printed by the `parse` job; on this plan's own file set n is 6 (VaelenAtlasActor, VaelenGameMode, VaelenHUD, VaelenPlayerController, VaelenViewActor, VaelenWorldSubsystem), on today's tree 2.
- **Round-3 fix: "N grown by the six files of rule 6"** - rule 6 names eight obligations, not six files, and the purity counter counts only .h/.cpp under Public/Private, excluding `*Module.cpp`: taken - 14.03's done-when reads `[purity] 206 files, 0 violations`, derived as 201 + the .h/.cpp under `Source/VaelenRun/{Public,Private}` minus `VaelenRunModule.cpp` (`Aelvor.h`, `Door.h`, `RunApi.h`, `Aelvor.cpp`, `Door.cpp` = 5); the `--verbose | grep -c '^\[purity\] module '` = 13 clause is kept.
- **Round-3 fix: `OPTIONAL_SYSTEMS += ... LifeChronicle` named a listener, not a system** (`LifeChronicle` is an `IEventListener` wired by `->Attach()`, which `check_world_wiring.py`'s `ADD` regex - only `Systems().Add(X.get())` - never sees; the same is already true of the three chronicle entries today): taken - 14.03 gives the script a `LISTEN = re.compile(r'(\w+)->Attach\(\)')` resolved through the same `MAKE` map and a third compared list "listeners", moves `PersonChronicle, SocietyChronicle, EconomyChronicle, LifeChronicle` into an `OPTIONAL_LISTENERS` dict, names the seven optional systems (`PlayerDaySystem, PlayerOrderSystem, RegardSystem, LivingSystem, ReputeSystem, FameSystem, JudgementSystem`), and quotes the expected last line as `[wiring] 4 wirings of AELVOR agree (13 declarations, 8 systems and 4 listeners deliberately optional, each named)` in place of `(... 12 systems ...)` and `4 + 8 = 12`; the `all three agree`/`the three wirings` literals becoming `len(WIRINGS)` were already in the row.

### 14.01 done - the leaf, the stream, and what the review found before the CI did

Pushed as bc8efe0 with the full suite and the adversarial review still running,
which the commit message said. Both came back with something.

**What landed.** `Vaelen/Player/Intent.h` (Intent, Refusal, PlayerCommand and
its 24-byte assertion, the two name tables) includes CoreTypes.h and
PlayerApi.h and nothing else; `Commands.h` includes it back, and nothing
compiled before is different. `Vaelen/Player/Stream.h` carries the three record
kinds - `Recorded` 40 bytes, `TakenUp` 16, `DayTurned` 8, padding named - and
the text form, one record per line under `vaelen-stream 1 <seed> <size>
<prehistory> <years>`. The five gate tests use them instead of five private
copies. A month of a life is 73 records and 1538 bytes, round trip
byte-identical.

**What the review found** (three lenses, fifteen findings, two refuters each;
eleven confirmed, four refuted) and **what the CI found** (two red legs of ten):
one blocker, a sentinel tick that was also a value; the tie rule at equal
ticks backwards against the gates that record it; a version field that did
nothing; a line count off by one; a probe that fenced one of the leaf's two
headers and compiled under no warning standard; an unformatted file; and, from
the Windows leg alone, MSVC C4701 on the decoder's short-circuit chain, which
the review had raised and gcc and clang had refuted. All seven corrections and
the reasoning are in ADR-0136's *Applied* section; the tie rule is now
**takings, then commands, then day turns**, which is the order a life is lived
in, and `Player.Stream` has eight tests.

**The lesson is the same one as the last two times**: the fast checks exist to
be run before a push, not after the leg goes red. And a review that finishes
after the push is a review of a pushed commit; the corrections are a second
commit, and this section says so rather than folding them in.

STATUS: PROTOTYPE. Green on the eight CTest legs with the second commit; the
clang-format leg green with it; the engine parse unaffected (nothing under
Source/Vaelen* that the shim reads changed).

### 14.02 done - the view headers as leaves, and the first CTest of the ADR-0135 pair

The defect the planning found (ADR-0137): the six view headers each included
`Frame.h`, and `Frame.h` included eight kernel headers so that `ViewSources`
and `TakeView(const World&, ...)` could be declared beside `WorldView`. The
closure of `Frame.h` was 64 project headers and reached
`Player/Commands.h` - `Submit(World&)` - through `Gameplay/Fame.h`. A read-only
surface that could name the write, by include.

**What moved: declarations, and only those.** `ViewSources` and the six
`Take*` - `TakeView`, `TakeMapView`, `TakeNetView`, `TakePeopleView`,
`TakeViewFor`, `BordersBetween` - are in `Vaelen/View/Take.h`, with the kernel
includes and the `class World;` that came with them. `Frame.h`, `Land.h`,
`Net.h`, `Folk.h`, `Delta.h`, `Eye.h` include Core and ViewApi and nothing
else; `Eye.h` dropped `Sim/Regions.h` and keeps `Unreached` with a note.
Fifteen files that take a view include `Take.h`, one line each. The drawer
changed nothing.

**What it measures to.** `g++ -M`: `Frame.h` 64 -> 4 project headers; the
other five 5 each; `Take.h` 70. `Commands.h` in the old closure, in none of
the six now. `grep -lE 'Vaelen/(Sim|Population|Society|Economy|Colony|
Infrastructure|Gameplay|Player/Player|Player/Commands)'` over
`Source/VaelenView/Public/Vaelen/View/*.h` returns `Take.h` and nothing else.

**What holds it.** `Tests/View/Probe_ViewLeaf.cpp` - every view header but
`Take.h`, plus `Player/Intent.h`, compiled with Core, View and Player public
headers on the include path, linked to `vaelen_build_flags` and to no
library - CTest `View.Leaf`. Mutation-tested before the commit: a kernel
include regrown in `Frame.h`, in `Eye.h` (`Sim/Regions.h` back) or in
`Folk.h` (`Population/Persons.h`), and `Take.h` included by the probe, each
fail the build; an old-style cast in the probe fails it too (the flags are
real); the control builds. And **`Atlas.Frozen128`**: `VaelenAtlas --size 128
--years 120` then `check_atlas_output.py --expect frame=0xabc5a5767c6cf9dd
ground=0x8f7f4948f49b6e86` - the ADR-0135 pair, held until today by a
paragraph in DECISIONS.md and by nothing a CI leg reads. Green after the
split; red on one changed digit of either; `--expect` has a self-test case.
`VAELEN_VIEWGATE_FROZEN_VIEW` and `_STATE` unmoved.

162 tests locally, 159 + `View.Leaf` + `Atlas.Runs128` + `Atlas.Frozen128`;
`Tools/verify_fast.sh` green (format, purity, shim 7/7, parse 6 TU with the
one new include in `VaelenViewActor.cpp`, wiring 3/3).

STATUS: PROTOTYPE, as the row says; the leaves are the surface 14.04's
`LifeView` and 14.08's subsystem build on. Next: 14.03, `Replay()` and
`Door`.

### 14.03 done - the thirteenth kernel module, and the wiring a host uses

`VaelenRun`: `Vaelen/Run/Aelvor.h` (one wiring of a played AELVOR),
`Vaelen/Run/Door.h` (the door its host's inputs come through, and `Replay`),
`RunApi.h`, `Aelvor.cpp`, `Door.cpp`, `VaelenRunModule.cpp`; the eight
obligations of rule 6 met (kernel_modules.txt, root CMake, Build.cs, its own
CMake listing sources, the `Public/Vaelen/Run/` tree, one Module.cpp, the
uproject, both targets); `Tests/Run` with `Run.Aelvor`, `Run.Door`,
`Run.Registry`, `Run.Shuffled`.

**The wiring.** `Aelvor::Kernel`'s constructor is the Atlas wiring verbatim -
fifteen types in Atlas order, fifteen systems - and the frozen pair says so:
`Run::Aelvor` with nothing set is `frame abc5a5767c6cf9dd, ground
8f7f4948f49b6e86` at 128/120, the ADR-0135 pair, asserted by `Run.Aelvor`
beside `Atlas.Frozen128`. Behind `Options{Size, PreHistory=300, Years=120,
Seed, Colony, Play, Lively}`: Colony adds `ColonyTypes` and `MiningSystem`
after Polity as the Atlas does; Play adds the six Phase 10 type sets after
Colony and `PlayerDaySystem`, `PlayerOrderSystem`, `RegardSystem`, `Doings`
and the `LifeChronicle` listener, as `Test_PlayerGate` wires them; Lively
adds Phase 12's living, repute, fame and judgement as `Test_GameplayGate`
does, and implies Play. **The idle claim holds**: a Run with Play and nobody
taken up has the same frame and ground digests as one without (`Run.Aelvor`,
at 96), which is what every Play option rests on.

**The door.** `Door::Mean` stamps `Issued = Now()` whatever the caller wrote
(asserted with 0 and with 12345), submits, and records `Recorded{Now(), C,
Verdict}`; `Door::Day` records `DayTurned{Now()}` at the tick it is turned on
and then turns it; when the played person is no longer alive afterwards they
are released and another is taken up, recorded as `TakenUp`. `Replay(Fresh,
stream)` is bounded by the records: for each `DayTurned`, submit every
`Recorded` due, turn the day, take up every `TakenUp` due (releasing whoever
was played, so a replay never has to know why a life ended); what was
recorded after the last day turn is submitted last. `Run.Door`: a year of
the eight verbs in a round at 96 plus five empty days, encoded, decoded and
replayed into a fresh Run - `Wrong=0`, `Days=365`, `Answered=360`, every
`ByKind` 45, and equal state, log and life digests, the life word for word;
**the same stream minus its last `DayTurned` replays to another state** -
the record is load-bearing, which is ADR-0138's claim. A stream of another
seed, of another size, into a Run without Play or before `Begin()` is
refused with `Refused=1` and nothing replayed.

**What a replay cannot know**, named in `Door.h` rather than hidden: a
played person who died with nobody left to take up. The Door releases them
and records nothing; the replay keeps the dead mark, and the commands after
it are refused `Dead` where the original said `NoPlayer`. A record kind for
it is a later task if a stream ever meets it. And `StartRules` are the host's
configuration, not an input: a replay must be given the same ones, and the
tests are; `Tools/Atlas --replay` uses the defaults.

**Measured, not asserted (ADR-0109)**: `run: day mean 1.2 ms, max 33.8 ms at
256 with the colony` on the container that runs the tests, release gcc, one
region detailed, thirty days played. The world was built in 1.3 s. What the
editor's day budget is read against.

**The tool.** `Tools/Atlas --replay FILE` reads a stream, builds the Run from
the stream's header (the world is the stream's, not the command line's; the
colony and the start rules are the host's), replays it, and writes a REPLAY
document - `"kind":"replay"`, the run, the stream's counts, the report with
its three digests, and the three view digests after it - exit 1 when the
stream is of another world or any answer differed, so a CTest entry can hold
a stream to its world (14.10's `Replay.Played`). `--empty` is the same with
no stream: the Play wiring with nobody taken up, the baseline a played stream
is compared against; CTest `Atlas.Empty` runs it at 48. The default path is
untouched: `Atlas.Frozen128` green.

**The guard.** `check_world_wiring.py` compares four wirings and three lists:
declarations, systems, and now LISTENERS (`X->Attach()`, resolved through the
same `make_unique` map), because a chronicle is not a system and the `Add`
pattern never saw the three that sat in `OPTIONAL_SYSTEMS` doing nothing. The
optional lists are extended by name, each with "Run::Aelvor only, behind
Play/Lively": `[wiring] 4 wirings of AELVOR agree (13 declarations, 8 systems
and 4 listeners deliberately optional, each named)`. Its first run on the new
file found SIXTEEN declarations in `Aelvor.cpp`: the header comment had
spelled out the idioms it matches, and the regex read the comment. Reworded,
and the comment now says why. Mutation-tested: a non-optional class attached
in one wiring only, an `Attach()` with no `make_unique` to resolve it, and
`BondageSystem` added after `PolitySystem` in the Run alone are each caught
at their row; the control agrees. **What it does not catch, by design**: an
OPTIONAL type declared out of place (`StartTypes` before `Polity` passed the
guard), because optional names are removed before the orders are compared.
What would catch that is the frozen pair, if the shifted ids change the
world - and nothing, if they do not. Named as a limit rather than widened
into a pattern; the fix, if it is ever needed, is a position rule per
optional name.

**Counts.** `[purity] 210 files, 0 violations` and thirteen `[purity] module`
lines - the row said 206, derived from 201 before 14.01 and 14.02 added five
files; the `parse` job now prints the module count and fails unless it is 13.
167 CTest entries (162 + `Run.Aelvor`, `Run.Door`, `Run.Registry`,
`Run.Shuffled`, `Atlas.Empty`). `VaelenRun.Build.cs` and `VaelenRunModule.cpp`
are UNVERIFIED until 14.08's build on the engine machine, and say so.

**The review** (three lenses, seven findings, two refuters each; two
confirmed, five refuted as documented limits or intended contract) and the
second commit it produced. Confirmed: `Door::Mean` recorded a command
answered `NoPlayer` - the world untouched - and on one tick the text form and
`Replay` put takings before commands, so a host whose input fired before its
first `TakeUp` on the same tick would have its refused command replayed
AFTER the taking, accepted and executed: `Wrong=1`, another state. A
`NoPlayer` answer is not a record now, `Door.h` says why, and `Run.Door` has
the case (a command meant before anybody is played, then a taking, ten
played days, replayed to the same world). And the `parse` job's "Kernel
modules" step piped the purity checker through `tee | tail`, which under
GitHub's `bash -e` without `pipefail` replaced the checker's exit status with
tail's 0: the step printed the violation count as if it enforced it, and
enforced only the module count. `set -o pipefail` now. Also taken from the
refuted set, cheaply: `Aelvor::Day()` before `Begin()` turns nothing rather
than failing a check in the history. Left as documented: the day-turn records
are a count, not ticks checked against the clock; `Colony` and the start
rules are the host's configuration and not in the stream header; the row's
`Days=360` is `Days=365` in the test - 360 played days plus the five empty
ones the truncated-stream clause needs.

STATUS: PROTOTYPE. Next: 14.04, `LifeView`.

### 14.04 done - the life, as the screen that shows it needs it

`Vaelen/View/Life.h`, the seventh leaf: `LifeView`, 1608 bytes, no padding,
trivially copyable, standard layout - who and where (index, region, alive,
age, two names), what the life was at its first moment (region, holder, year,
bond kind, the holder's name), the day (awake, spent, left, days lived,
missed), the body (food, health, rest, hunger), the queue (held, taken,
refused, dropped, the last refusal, the first eight waiting as
`PlayerCommand`s), the regard (repute, known, kindnesses, wrongs, eight
`KnownView`s with names), `Cost[16]` from `OrderRules::HoursOf`, `Near[8]` -
the neighbours a Move will NOT be refused for, which is adjacent AND detailed
(`Doings.cpp`'s Move rule, not adjacency alone as the row said) - and
`Company[16]` of the alive in the same region, lowest index first, with
names. Every name is fixed `char[32]`, NUL-terminated, printable ASCII, and
nothing after the NUL: `MeasureLifeView` hashes every byte of the struct.
`Life.h` includes CoreTypes, Hash, ViewApi and `Player/Intent.h`; the probe
now includes it, and a kernel include regrown in it (`Player/Commands.h`,
`Sim/Regions.h`) fails the probe; its `-E` output carries no `Commands.h`,
`Vaelen/Sim/` or `Vaelen/Population/` marker.

`TakeLifeView(const World&, const ViewSources&, RegionGraphCache& Ways,
LifeView&)` is in `Take.h`, mirroring `TakeViewFor`: the graph is the
caller's. `ViewSources` gains `HasLife` and the Hour, Order, Regard, Start,
Need and Family types - so `Take.h`, not `Life.cpp` alone, names those
headers; the row's "the one TU including them" holds for the `.cpp` and could
not for the type declarations. `Run::Aelvor::Sources()` fills them under
Play. One walk of the person pool per take for every name (a `PersonIndex`),
0.03 ms at 96.

`View.Life`, 5 tests, 343 checks, on a played 96-map through `Run::Aelvor`
(the first View test to take somebody up rather than declare the Player types
by hand; `Tests/View` links `Vaelen::Run`): flat and padding-free; nobody
played is an empty life with the clock and the costs still there, and no
graph built; every number equals the kernel accessor - `FindPerson`,
`NamePerson`, `NameRegion`, the needs pool, `StartOf`, `HoursOf`/`HoursLeft`,
`OrdersOf` and its ring, `RegardOf`/`ReputeOf` (absent on a one-day-old life,
and then the view says zero), `OrderRules{}.HoursOf`, every `Near` entry
`AreAdjacent` and `IsDetailed`, the company alive, there, ordered, named;
sixty takes leave the state and log digests unchanged and `Ways.Builds() ==
1`; the view outlives the world. `VAELEN_VIEWGATE_FROZEN_VIEW` and `_STATE`
unmoved. The fifth test enters the two name branches the fixture never did:
somebody KNOWN (a Speak aimed at `Company[0]`, three days, `Known[0].Name`
equals `NamePerson`) and somebody who HOLDS the played person (a bound start
on AELVOR at 128/120, `HolderName` equals `NamePerson`; the 96-map offers
none, and the test says so rather than assuming). A neighbour is detailed the kernel's way - `RequestDetail` and the
days until the yearly bridge honours it; `PromoteRegion` materialises people
without the mark the Move rule reads, which is why the first draft of the
test saw `0 near`.

What the review of this commit found (seven findings, two refuters each; two
confirmed, five refuted as documented limits): ARCHITECTURE's VaelenView row
said the leaves include Core and `ViewApi.h` only, and `Life.h` includes
`Player/Intent.h` too - the row says so now; and the queue was compared by
`Kind` alone with no Known or Holder name ever asserted - the whole
`PlayerCommand` is compared now (`memcmp`), the company's `Years` too, and
the fifth test above exists. Fixing it found a trap worth a line in
`Aelvor.h`: `Aelvor::Submit` stamps nothing, so a command with `Issued` left
at 0 is `Stale` (`OrderRules::StaleAfter`, 720 ticks) by the time the day
reads it - the first draft's Work and Speak were both refused that way and
the test had not noticed, since it only asked for numbers equal to the
kernel's. `Door::Mean` stamps `Now()`; a host that goes through `Aelvor`
directly must. The test now asserts the Work was taken and the Speak at
nobody refused.

Not done here, by name: `ChronicleView` (14.05) and the panel (14.06); the
holder's CURRENT bond (`Society::BondOf`) - the view carries the start's.
The row's line references drifted (`HoursOf` is `Commands.h:81`, `TakeViewFor`
is in `Take.h` since 14.02); the code is as described.

STATUS: PROTOTYPE. Next: 14.05, `ChronicleView`.

### 14.05 done - the last lines of the life, as the screen that shows them needs them

`Vaelen/View/Chronicle.h`, the eighth leaf: `ChronicleView`, 7816 bytes, no
padding, trivially copyable, standard layout - the last 32 lines about the
played person as ONE flat `char` buffer (`Text[6144]`, each line
NUL-terminated and packed from the front, oldest first) with a `LineView`
per line, plus the why of the newest thing that happened because of them
(`Why[2]`, `WhyText[512]`: the thing, then `  because ...` - two steps;
deeper is Phase 17). `LineView` is the row's `{Tick, Year, Kind, Begin,
Length}` and TWO more, `Verb` and `Refused`: `Kind` is a `LineKind` (Acted,
Refused, Walked, Born, Died - the five kinds `LifeTimeline` keeps), `Verb`
the `Intent` of an act or a refusal and `Refused` the `Refusal` of a
refusal, so 14.06's panel can grey or colour a row by code without parsing
its text. `Chronicle.h` includes CoreTypes, Hash and ViewApi, as the row
says and nothing more - `Verb` and `Refused` are the numbers of `Intent` and
`Refusal`, carried as `uint32`; a panel that wants their names includes
`Intent.h` itself. The probe includes it (`chronicle 7816 bytes`).

`TakeChronicleView(const World&, const ViewSources&, ChronicleView&)` is in
`Take.h` and INCREMENTAL, as the row asked: `ChronicleView::Since` is the
count of log events already read, the next take reads `Log.At(i)` from there
to `Count()`, keeps the ones about the played person by the same five kinds
`LifeTimeline` keeps (the filter is written out here rather than calling
`LifeTimeline`, which walks the whole log every call - the one thing an
incremental take must not do), describes them with `DescribeLifeEvent` (so
the words are `ExportLife`'s), appends each line dropping the oldest until
it fits (`Dropped` counts them), and returns the events read. A change of
played person starts the view over, and so does another world: a log shorter
than what was read, or one whose event at `Since - 1` does not hash
(`Event::Hash()`, kept as `LastHash`) to the one this view read last - two
worlds of the same seed that diverged before the cursor are told apart by
that, and a view resumed into either is that world's fresh view. The why: any new event whose `Cause`
is an event about the played person (`FindEvent`, a binary search) becomes
`WhyOf`, the newest wins, and its text is retaken only when `WhyOf` moved -
the first two lines of `ExportWhyWithLife`, which is the block `ExportLife`
prints after `why:`. The tail of every buffer is zero, so `MeasureChronicleView`
hashes the whole struct and a view grown take by take is the fresh one.

`ViewSources` gains `HasGoods`, `Economy::MarketTypes Markets` and
`Society::OrganizationTypes Organizations` (ADR-0137, widened again), so
that the `LifeContext` the take describes with is the one `Run::Aelvor::Life()`
describes with - `Goods` set, the economy speaking for the society and the
person layers - and a line about a stock caused by a Give is worded by its
own layer, to the byte, rather than the plainer person sentence. Without them
(`HasGoods` false) the person layer speaks; that is a plainer chronicle, not
an error. `Run::Aelvor::Sources()` fills them.

The row's `RecordRefusals=1` is applied where it lives: `Run::Aelvor`'s
`LifeChronicle` is built with `LifeChronicleRules{RecordRefusals = 1}`
(`Aelvor.cpp`), so a played life's refusals are chronicle RECORDS (what
`ExportChronicleWithLife` and `CheckLifeChronicle` read) as well as sentences.
The view itself never needed the rule: `LifeTimeline` reads the event log,
where `PlayerRefusedEvent` is always published (`Commands.cpp`), which is why
a refusal was a sentence in `Aelvor::Life()` before this task. The unplayed
digests cannot move (the listener is attached under Play only) and the Door's
replays compare a world with itself; Run.Door and Run.Aelvor green as before.

The row's "a line containing `Nothing`": the sentence is `could not ate:
nothing to do it with.` - `RefusalName(Refusal::Nothing)`, lower case, as
`PlayerHistory.cpp` writes it (`VAELEN_LIFE_FROZEN_TEXT` holds those words,
and this task does not touch them). The test asserts the name and, beside
it, `LineView::Refused == Refusal::Nothing`: the code IS the `Nothing`.

`View.Chronicle`, 7 tests, 5999 checks, on the 96-map through `Run::Aelvor`
and `Run::Door`: flat and padding-free (`sizeof 7816 <= 8192`); nobody played
is an empty chronicle with the clock on it and nothing read; the year of the
eight verbs of Run.Door with the view taken after every day, then once from
nothing - the same 7816 bytes; 32 lines held, 329 dropped, 1908 bytes of
text, `Truncated 0`, `NonAscii 0`; every line held equals the matching line of
`ExportLife` (every act, `MaxActs = 0`) word for word and its `Tick`, `Kind`,
`Verb` and `Refused` are the log's for the last 32 events about them; both
why lines equal `ExportLife`'s and the cause is an Acted line; every
`Refused` line contains `could not` and its `RefusalName`, no other does; the
tail is zero and every line terminated where `Length` says; without the
goods (`HasGoods` false) the lines and the `WhyOf` are the same; sixty takes
with nothing happening read 0 events and move nothing; released, the view is
empty, and taken up again it is the new life's from its first event. A walk
- a neighbour detailed the kernel's way, then a Move - is two lines the same
tick, `walked to Aigi` (Acted, Move) and `walked from Iakhas to Aigi`
(Walked), and the why is those two the other way round, `ExportLife`'s to the
byte. Another world - the same seed, five days of Work against five of Wait,
or a world at its first moment - starts the view over into that world's
fresh view. A refused Eat with no grain - the grain within reach taken away
by `Economy::AddStock`, the kernel's own, 55 units on that world, and none
left where `Doings::HasGoods` looks - is `Year 300, age of era 4: Eikha could
not ate: nothing to do it with.` with `Verb Eat` and `Refused Nothing`. The
view outlives the world. `VAELEN_VIEWGATE_FROZEN_VIEW`, `_STATE` and the
ADR-0135 pair unmoved.

Measured, not asserted: events read per day on the round-robin, min 1, max
79821 (the first day, which crosses the year: everything the yearly systems
publish is in the log, and the incremental take walks past it once; the
next eleven days read `2 1 2 1 1 1 1 1 2 1 2`), mean 223; the buffer is full
on day 31; the fresh take reads the 80270 events of the 60-year world in
1.5 ms (release gcc).

What the review of this commit found (five readers, two refuters per
finding): the why's truncation path could underflow `WhyTextBytes - 1 -
WhyUsed` once a why line had filled the buffer, and `Truncated` counted a
why cut again at every retake, so a grown view could differ from a fresh
one - the why now has its own `WhyTruncated`, reset with the why, a room
that cannot go below zero, and `MeasureChronicleView` sums both; the
`Since > Count` restart returned a wrapped count - it returns the events
read from zero; a world of the same seed that had diverged was resumed into
silently - `LastHash`, above; `Chronicle.h` included `Intent.h` it did not
use - removed; `View::ChronicleStats` shared its name with
`History::ChronicleStats` - `ChronicleViewStats`; and the tests asserted
neither the line numbers against the log, nor a Walked line, nor the
restart, nor the goods-less path, nor that the grain was gone - the 7 tests
above do. Admitted rather than fixed: `RecordRefusals = 1` in `Run::Aelvor`
lets a played life's refusals count against `MaxRecordsPerYear` (12) with
its doings - the records are 10.07's, the row asked for the rule, and a life
that is refused twelve times a year has its record say so; a Died line is
never produced by these tests (nobody dies to order); `EventsRead per day`
is printed for the first twelve days and summarised for the rest.

Not done here, by name: the panel (14.06) that composes this with `LifeView`
and `WorldView`; a why deeper than two steps (Phase 17); the record-side
chronicle (`ExportChronicleWithLife`) in the view - the screen shows the
life, the records are 10.07's. A line longer than the whole buffer is cut and
counted (`Truncated`); nothing the kernel writes is that long, and the cut
runs under no test.

STATUS: PROTOTYPE. Next: 14.06, the panel.

### 14.06 done - the first screen, composed kernel-side

`Vaelen/View/Panel.h`, the ninth leaf: `PanelView`, 4064 bytes, no padding,
trivially copyable - the PAGE, as rows of printable ASCII. `TakePanel(const
WorldView&, const LifeView&, const ChronicleView&, PanelView&)` takes no
World, no `ViewSources` and nothing from `Take.h`: three views in, rows out,
no allocation. The page is the row's: the date and the world, who they are
and who held them (`bound to <holder> since year N`), the body, `hours left X
of Y`, the queue with its last refusal, the eight verbs with cost, key and
whether the page offers them, the neighbours a Move will not be refused for
with the people in each, who is here, the last sixteen chronicle lines, and
as its LAST row the page's own digest in hex.

`RowView{Kind, Verb, Begin, Length}` per row and `VerbView{Verb, Cost,
Offered, Foreseen, Key}` per verb, so a widget greys a row or binds a key by
number without parsing text; the keys are the letters 14.09 binds - `T` wait,
`W` work, `R` rest, `E` eat, `M` move, `S` speak, `G` give, `K` take - and
they live in the view, so the UI binds what the page says rather than
agreeing with it by hand; `Lines(const PanelView&, char* Out, uint32
Bytes)` writes the rows joined by newlines - the row's signature plus the
buffer's size, because a writer that cannot be told how much room it has is
not a writer this project ships. `MeasurePanel` reports Rows, Bytes,
TextBytes, NonAscii, Truncated, Dropped, Offered and the digest, RECOMPUTED
from the text rather than read back from the field, so a page whose digest
row lies fails rather than agrees with itself. `Truncated` counts a row that
began and ran out of bytes; `Dropped` a row there was no room to begin at
all - a page that quietly stopped would be a page whose reader cannot tell
it stopped, and what does not fit is the NEWEST of the life.

What the page foresees, from the three views and nothing else: `NoPlayer`,
`Dead`, `Full` (the queue at 8), `Costly` (a verb costing more hours than the
whole day gives - the kernel's own rule, `Commands.cpp`), `TooFar` (a Move
with no detailed neighbour) and `NoOne` (a Speak, Give or Take with nobody
there). The one obstacle that is not a refusal is the hours left: what today
cannot pay for waits for tomorrow rather than being refused, so the page
writes `- not today` and `Press` answers `Costly`, the nearest true word the
enum has; the header says so. What the page CANNOT foresee - no grain to eat,
somebody who dies before the hour comes - is the world's answer and arrives
as a chronicle line. `Press` fills Kind/Target/Amount with `Issued = 0` (the
door stamps that) and answers `Unknown` for anything but the eight.

Two pages frozen, and the second is the first screen itself:
`VAELEN_PANEL_FROZEN_EMPTY 0x54787451e65766c1` on the unplayed 128/120 Run -
which is also, byte for byte, the empty play (the Play wiring with nobody
taken up), asserted here and printed by `Tools/Atlas --empty --size 128
--years 120 --panel` - and `VAELEN_PANEL_FROZEN_PLAYED 0x26ef4024725cd4aa` on
the 96-map round-robin after thirty days. Two CTest entries hold the tool's
page and not one: `Atlas.PanelEmpty` is its exit code and `Atlas.PanelFrozen`
its digest row, because a CMake test with `PASS_REGULAR_EXPRESSION` passes on
the match alone and never looks at the exit code. Both reproduce on linux-gcc-release and
linux-clang-debug here; the other six legs are the CI's to answer, and the
row claims eight.

`View.Panel`, 9 tests, 3493 checks: flat and 4064 bytes; three DEFAULT views
- a world nobody has looked at - still compose a page, which is the claim
that the screen needs no world; the two frozen pages; every verb's cost
against `OrderRules::HoursOf` and `LifeView::Cost`, its key, its Offered rule
and the two refusals the page foresees from the life; a queue filled to 8/8
greys every verb with `Full`, and a `Press` then leaves the kernel's own
`Refused` and `Taken` counts unmoved - the page refused it and the world
never saw it; `Lines` is the rows joined by newlines and nothing else, a
buffer too small writes only a terminator, every row is printable ASCII
terminated where its `Length` says, the tail of the text is zero; one changed
byte moves the digest and `MeasurePanel` then disagrees with the field; the
page outlives the world and `Press` still answers on it.

And every ROW, word for word, from three views written by hand in the test -
no world at all, which is what a pure function allows: the date, `Eikha of
Iakhas, 25`, `bound to Ordihumen since year 275`, `food 243  health 255
rest 185`, `hours left 2 of 16`, the queue, `last: too far to walk`,
`near: 27(412) 31(88)`, `here: Ukit, Aifus, ... and 852 more`, the chronicle
line, and each verb row - `[T] wait      1h  ok`, `[W] work      4h  -  not
today` when the hours are short, `[W] work      4h  -  longer than a day`
when the whole day is, `[M] move      3h  -  too far to walk` with no
neighbour, `[G] give      1h  -  nobody there` with nobody here. A free
person has no bound row; a dead one is offered nothing and says `, dead`. A
page that runs out counts what it lost: sixteen chronicle lines of two
hundred letters drop one row (`Dropped 1`, `Truncated 0`), one line of three
thousand is cut (`Truncated 1`), and both keep the digest row.

Measured, not asserted: the 256 world with the colony, in `Run.Aelvor`'s
already-logged day - `panel: bytes 1419, lines 32, truncated 0, digest
9c2551479f7ea9cb, taken in 2.71 ms` (release gcc; the take of the three views
is most of that, the page itself 0.04 ms at 128). The empty page is 16 rows
and 514 bytes; the played one 33 rows and 1458 bytes.

What the review of this commit found (five readers, two refuters each) and
what it changed: the verb keys were `1`..`8` while 14.09 binds letters - the
page now carries the letters, which moved both frozen digests before any UI
was written against them; `Lines` wrote a PARTIAL page into a buffer that
held some rows but not all, which its own header called impossible - it is
all of the page or none of it now; the "the world never saw it" proof
compared `Refused` and `Taken`, which a real `Submit` would not have moved
either, and compares `Held` now, which it would; `Press` on a page nobody
composed answered `Costly` for a verb that is not a verb, because slot zero
matched `Intent::None` - empty slots are skipped now; the chronicle row was
read as a C string from a `Begin` the view itself gave, and is read by
`Length` within the buffer now, as `MeasurePanel` reads its rows; and a row
that did not fit was simply gone, which `Dropped` now counts. The frozen
pages are the pages after all of that. The findings the refuters killed: a
forged `PanelView` (a struct no `TakePanel` wrote) is not a case this module
defends against beyond not reading past its own buffers, and the digest is
of the page's ROWS by design - the numbers beside them are asserted against
the kernel in the verbs test rather than hashed.

The tally, for the record, because the refuters finished after the
corrections were already pushed: ELEVEN findings survived them, collapsing to
the seven distinct defects above - `Lines` was found by three dimensions at
once and the missing text coverage by four. The commit that fixed them said
"six", counting the defects it had read rather than the findings that
returned; this is the number the review actually gave.

Not done here, by name: the UE side (14.08, 14.09) and the include fence that
reads this header (14.07); a region NAME for a neighbour, which no view
carries - the page prints the index and how many live there; the holder's
CURRENT bond, which is 14.04's limit still. A `PanelView` that no `TakePanel` wrote is read no
further than its own buffers and is not otherwise trusted.

STATUS: PROTOTYPE. Next: 14.07, the UI's includes read by the CI that cannot
build the UI.

### 14.07 done - the UI's includes, read by the CI that cannot build the UI

The fence exists before the code it guards, which is the whole point of doing
this task before 14.08 and 14.09: the UI is written UNDER it rather than
checked after. What it guards until those tasks write `Source/VaelenGame` and
`Source/VaelenUI` is `Tools/UiWitness`, a miniature of each - a world
subsystem holding the run, a HUD drawing `Lines()`, a controller binding the
page's own keys, a game mode naming the two classes - built by nothing, in
`Vaelen.uproject` nowhere, parsed and fenced like the real thing. Its README
says what each file is there to prove. When the real modules land, the tools
prefer them (a module with a `Private` directory under `Source` always wins)
and the witness stays as what the self-tests mutate, which is what keeps the
mutations off the real code.

`Tools/check_ui_fence.py`, CTest `Kernel.UiFence` and `Kernel.UiFenceSelfTest`,
a step of the `parse` job, and a line of `verify_fast`: every
`#include "Vaelen/..."` under the UI's roots is a view leaf, the command
surface (`Intent`, `Stream`, `PlayerApi`) or `Vaelen/Core/`; the TRANSITIVE
closure of each of those is too, so an allowed header that grows a kernel
include stops being allowed - which is exactly what `Frame.h` had done before
14.02; and thirteen word-bounded regexes refuse what an allowed include still
permits: `Submit(`, `ViewSources`, `Vaelen::World`, `TakeView*(`,
`TakeLifeView(`, `TakeChronicleView(`, `BeginEnslaved(`, `Vaelen::Run`,
`FPlatformTime`, `FDateTime`, `DeltaSeconds`, `Tick(`, `rand(`. `TakePanel` is
deliberately absent: it takes views. Word-bounded and not substrings, and the
self-test's CONTROL is a file saying "the operand" and "Ticker" - a checker
that cries wolf is a checker somebody turns off. 16 mutations, the closure and
the unnamed folder, each caught, control clean. (Eleven regexes and 12
mutations is what this paragraph said until the corrections below; the row at
14.07 asks for eleven and puts `ViewSources` among the refused prefixes,
the implementation moved it into the token list and added `TakeChronicleView`,
and four of the thirteen had no mutation at all.)

`VaelenGame` is read everywhere but its `Private` directory, and that
exemption is the seam: the module that owns the world may name `Vaelen::Run`
and `Take.h` in its `.cpp` and may not in its header. What the fence reads is
the MODULE and what it skips is named out loud, because UnrealBuildTool
compiles the whole module tree and a list of two subdirectory names left the
rest of it unread - see the corrections below. The witness proved both halves
compile that way.

`Tools/parse_engine_modules.py` gained the four things the row asked and one
the witness forced. `ENGINE_MODULES` has four names now; `stub_generated_headers`
runs ONCE over all of them into ONE directory made before the loop, because
14.09's HUD includes 14.08's subsystem header and a per-module scan leaves
that header's generated stub "file not found" on the UI's first line - which
is now a REVERSE CONTROL in the self-test: the healthy copy parsed with
`--stub-scope own` must FAIL, and does. `includes` is per-module: every module
keeps `[SHIM, generated, public] + kernel_include_dirs()` except `VaelenUI`,
which gets a LITERAL list - SHIM, generated, its own Public, VaelenGame's
Public, and only VaelenView, VaelenCore and VaelenPlayer of the kernel - so
`Take.h`, `Commands.h` and `Sim/World.h` are `file not found` from a real
front end and `Vaelen::World` is an unknown name. A literal list on purpose:
`kernel_include_dirs()` enumerates every `Source/*/Public` there is, so a
filter over it would widen silently the day a module is added. `module_root`
is the fifth thing: a module is read from `Source` when it has a `Private`
there and from the witness otherwise, and `--verbose` says which.

`Tools/EngineShim` gained ten headers - `GameFramework/{HUD,PlayerController,
GameModeBase}.h`, `Engine/{Canvas,GameInstance}.h`, `Components/InputComponent.h`,
`InputCoreTypes.h`, `Subsystems/GameInstanceSubsystem.h`, `Misc/{FileHelper,
Paths}.h` - plus `UWorld::GetGameInstance()`, `UEngine::GetSmallFont()`,
`UObject::StaticClass()` and `TSubclassOf`. Every new base class carries its
own `using Super = <itself>;`, which `GameFramework/Actor.h` had warned about
in prose since 13.06 ("the day one derives from another"): without it
`Super::DrawHUD()` in a HUD resolves to `AActor` and either fails to compile
or finds something else that does. And the four conversion macros stopped
throwing their argument away - `ANSI_TO_TCHAR(x)` was `(L"")`, which
typechecks anything at all, and is now a call that takes a `const char*`.

What the fence found before it was even finished, which is the answer to
whether it earns its keep: the first parse of the witness failed on
`Panel.h`'s own `static_assert(sizeof(PanelView) <= 4096)`. That was a stale
object file rather than a portability defect - it did not reproduce - but the
chase is what a front end over the UI is for, and it cost minutes rather than
a UE build on another machine.

The honest limit, in both docstrings and here: `VaelenView.Build.cs:29` is
`PublicDependencyModuleNames`, so under UnrealBuildTool every kernel include
path is TRANSITIVE into anything depending on VaelenView - the UI included.
The restricted include set is a CI-only simulation of a fence; the closure
check in `check_ui_fence.py` is the only thing standing between the UI and
`Commands.h` in a real build. Both files say so.

174 CTest entries green on linux-gcc-release (172 plus the two new Kernel
ones); 10 translation units parse (3 + 3 + 1 + 3), 13 shim mutations all
caught, the
control and the reverse control both as they must be; `Kernel.UiFence` and
`Kernel.UiFenceSelfTest` green on every leg. The `parse` job also compares the
stubs written against the `.generated.h` includes the tree actually has - two
readers, one number, six today - and the grep is `-E '[A-Za-z]+'` rather than
`'[A-Za-z]*'` because the star form counts a sentence mentioning the extension,
which is the lesson `check_world_wiring.py` learned the same way.

Not one STATUS line moved. STATUS: VALIDATED for the scripts, which is all
they claim - the UI they fence is a witness until 14.08 and 14.09 write the
real one, and the shim's new headers are UNVERIFIED until an editor has built
against the real Unreal ones.

STATUS: VALIDATED (the scripts). Next: 14.08, `VaelenGame`.

### 14.07-14.09 corrected - what the adversarial review found

Five confirmed findings over the fence, VaelenGame and VaelenUI; two of the
five were the same doc error counted twice, so four distinct things. Each was
written by one reviewer and then put to two refuters; these are what survived.

**A generated destructor nobody wrote** (`VaelenWorldSubsystem.h`). The class
declared no constructor and no destructor, so UnrealHeaderTool writes both
into `VaelenWorldSubsystem.gen.cpp` - a translation unit that includes the
public header and never the `.cpp`. `TUniquePtr<FVaelenHeld>` would then be
destroyed where `FVaelenHeld` is a forward declaration, which the engine's
`TDefaultDelete` refuses by `static_assert(sizeof(T) > 0)`. The owner's build
would have stopped on a file nobody in this repository wrote, and the pimpl
that makes the header world-free is exactly what causes it. Both are now
declared in the header and defined in the `.cpp`, where `FVaelenHeld` is
whole. Reproduced before and after by compiling the `.gen.cpp` UHT would
write: before, `invalid application of 'sizeof' to an incomplete type
'FVaelenHeld'`; after, clean. `Tools/EngineShim`'s `TUniquePtr` now carries
the same `static_assert` as the engine's, which is what makes that
reproduction say the engine's own words - though it still cannot catch the
real case, because there is no UnrealHeaderTool in the shim and so no
generated translation unit to parse. The comment in the header says that.

**A folder the fence did not know about** (`check_ui_fence.py`,
`parse_engine_modules.py`). Both tools read `Public` and `Private` and nothing
else; UnrealBuildTool compiles every `.cpp` UNDER a module. A
`Source/VaelenUI/Widgets/` - or `Classes/`, or a subdirectory of `Private` -
was therefore built by UBT and read by neither tool. Demonstrated on a copy:
a `Widgets/SVaelenWorldPanel.cpp` including `Vaelen/Sim/World.h` and calling
`Submit()` on a `World` passed the fence at `c9de288` with rc=0. Both tools
now walk the module tree, and what is NOT read is named: `ROOTS` lists the
exempt subdirectories (`VaelenGame/Private`, and nothing else) rather than the
included ones, so the default is to read and a new folder is fenced the day it
appears. The self-test gained that case as its own mutation.

**Four rules nothing proved fire** (`check_ui_fence.py`). Thirteen token
regexes, twelve mutations, and the four without one were `FDateTime` - the
case the 14.07 row names by name - `TakeView*(`, `TakeChronicleView(` and
`BeginEnslaved(`. Delete any of the four from `TOKENS` and the self-test still
said "all caught" and exited 0. Four mutations added, and each verified the
only way that means anything: with its own rule deleted from `TOKENS`, the
self-test fails naming that mutation.

**Eleven that were thirteen** (this file). The 14.07 done section said
"eleven word-bounded regexes" and then listed thirteen in the same sentence.
Corrected above, with what moved and why.

Twenty-two other findings were refuted by both of their refuters. One of them
- the `parse` job's "Generated stubs" step globbing the deleted
`Tools/UiWitness` - was real, and was found and fixed independently at
`c9de288` while the review was still running; the refuters were wrong about
that one, which is worth writing down next to the rest.

STATUS: VALIDATED for the scripts. 14.08 and 14.09 stay UNVERIFIED: the
destructor fix is the strongest reason yet to want the owner's build, and it
is still the owner's build that decides.

### 14.10 - a month played by hand, replayed by no hand

On 2026-09-16, in UE 5.6 on the owner's machine, somebody played eighty-three
days of a life in AELVOR at 256 and pressed `Vaelen.Stream.Write`. The engine
printed this:

```
LogVaelenPlay: AELVOR 256 seed 41454c564f52: played Dukem (person 15019, region 37) 83 days, 99 intents (76 taken, 23 refused by the world, 0 dropped at the door), state 2ffed5236a593c1b, log 1fdd4211695649bc, life 654e2de6455d8b7a, panel 7de2c5faf3cc1813
LogVaelenPlay: verbs work 34 rest 1 eat 2 wait 2 speak 5 give 2 take 26 move 27
```

`Tools/Atlas --replay Tests/Run/Streams/aelvor256-2026-09-16.stream --panel
--want-bound 0`, on Linux, from a world rebuilt out of the stream's header
alone, prints those two lines back **byte for byte** - compared as files, same
MD5, nothing eyeballed - above `replay: 99 of 99 answered identically, 83 of 83
days`.

That is ADR-0136 met in the only way that counts. Ninety-nine verdicts recorded
by the engine, ninety-nine reproduced by a kernel that has never seen Unreal;
eighty-three day turns, eighty-three replayed; and the four digests of the
world - its state, its event log, the life in words, the page on screen -
identical. **A played life is a function of what came through the door, and of
nothing else the engine did.**

The `panel` digest `7de2c5faf3cc1813` is also the last line of the screenshot
taken that day: the log and the pixels agree, which is what putting the page's
digest inside the page was for.

**Against the row's clauses.** (a) the format verbatim, 99 intents where it asks
for 30 or more, 23 refusals by the WORLD where it asks for one, and every one of
the eight verb counts at least 1.

The row also asks that a Move be among the TAKEN, and the verbs line does not
say that: it counts what was MEANT, whatever the world made of it
(`VaelenPlayCommands.cpp`, the tally walks `Tape.Commands`). Two things settle
it anyway, and both are in the line itself. By arithmetic: 27 Moves were meant,
23 intents in total were refused and 0 dropped, so at least four Moves were
taken whatever the other verbs did. And by where the man ends up: a fresh
`Vaelen.Play 256 100` of this seed takes person 15019 up in **region 42** - the
engine printed it, and the headless probe of the same seed agrees - while the
played line ends `person 15019, region 37`. A person changes region only by a
Move the world took. The verb ADR-0139 made playable the day before was played,
and it carried him five regions over.

(b) byte-identical after the prefix, above. (d) the screenshot carries
`last: too far to walk`, and that is a WORLD refusal rather than a foreseen one:
`Refusal::TooFar` is returned by `Player/Doings.cpp` when the day runs the
order, the page's own foresight would have kept the command from ever being
submitted, and the line says 0 dropped at the door. The panel digest is its last
row. (c) **MET** - see below, and it took four readings.

**Clause (c), measured on 2026-09-16, and what it did and did not settle.**

*The ms per `Vaelen.Day` at 256, reported and not gated:* the engine printed
246.4 ms for the first day turn and then 1.5 to 2.0 ms for each of the next
nine, over a world of 110 048 people in 126 regions. Both numbers were then
reproduced headlessly - gcc release, this container - and the reproduction
decomposes them, which the engine's single figure could not:

| | day 1 | day 2 and after |
|---|---|---|
| nobody taken up | 78.3 ms | 0.01 ms |
| somebody taken up | 339.8 ms | 0.10 ms |
| the five `Take*` calls the subsystem makes per day | 11.1 ms | 1.6 ms |

So the steady 1.6 ms the engine reports is **not the simulation**: the day
itself is 0.10 ms and the view-taking around it is 1.6 ms, sixteen times the
world it describes. And the first-day spike is real but has a name: 78 ms of it
is the detail promotion asked for at `Begin`, and the other 262 ms is the
promotion of the played person's neighbours that ADR-0139 asks for at `TakeUp`,
applied on the first tick after it. Turning a day in AELVOR at 256 costs a tenth
of a millisecond; promoting a region to person-by-person detail costs about 65.
That is Phase 15's subject stated as a measurement rather than a worry.

*The frame rate.* Four readings were needed, because the first one measured
something else. In order, and each one is a `stat unit` on the owner's T400:

| | Frame | Game | Draw | GPU | Prims |
|---|---|---|---|---|---|
| the template map, HUD up, nothing drawn | 28.15 | - | - | - | - |
| **the 13.09 scene** (`Vaelen.View 256 100`), no HUD | **8.33** | 0.09 | 3.40 | 8.16 | 511.2 K |
| PIE, camera at ground level, HUD **off** | 30.56 | 7.06 | 30.41 | 30.43 | 953.9 K |
| PIE, camera at ground level, HUD **on** | 30.47 | 7.58 | 30.34 | 30.42 | 954.8 K |

The first reading, 35.52 fps, is the one that looked like a failure. It was
taken over a template map with the Phase 13 actor absent - `Vaelen.Play` holds a
world for the HUD to draw, it does not draw AELVOR; `Vaelen.View` does, and it
had not been run. So that reading timed a sky.

The last two are the measurement that decides, and they were taken in one PIE
session with one camera and one command between them (`showhud`), so the HUD is
the only variable: **it costs 0.52 ms on the game thread, two draw calls, and
nothing at all on the GPU.** Fifteen `Canvas->DrawText` rows and a 4 KB page
composed per frame, which is what the code says it should cost.

**Clause (c) is therefore MET, and the arithmetic is worth writing out because
no single reading contains it.** Frame time is the slowest of the three threads.
Over the 13.09 scene the GPU asks 8.16 ms and the render thread 3.40 ms; the
game thread with the HUD up costs 7.58 ms *in total* - measured, in PIE, in the
third and fourth readings. The slowest of 7.58, ~3.5 and 8.16 is the GPU at
8.16 ms, so the 13.09 scene with the HUD over it runs at about **122 fps**, and
the clause asks for 90. The HUD cannot change that: it adds nothing to the term
that sets the frame. The one honest caveat is that the 13.09 reading was taken
in the editor viewport rather than PIE, which is why the game thread's PIE cost
is used above rather than that reading's 0.09 ms - the substitution is against
this project's own case, and it still passes with 30 % of headroom.

**And a finding worth more than the clause.** At ground level the same scene
costs 30.4 ms and is GPU-bound on 954.8 K primitives: the player pawn spawns
inside a mesh built to be read from above, and every tile near the camera is
drawn at full detail because nothing decides otherwise. That is not a Phase 14
defect - the HUD is innocent and the clause is met - it is the first measured
statement of what Phase 15 is for. Together with the 65 ms a region costs to
promote to person-by-person detail, Phase 15 now opens with two numbers instead
of an intention.

**What the phase cost in defects, and where they were found.** Three stood
between the code and the owner's first build, and not one was visible on six
Linux presets, on macOS, on the Windows CI leg or in the clang parse against the
shim: UnrealHeaderTool's generated destructor on an incomplete pimpl; then
`DEFINE_VTABLE_PTR_HELPER_CTOR`, which instantiates that same destructor
whatever the class declares, and which defeated the first fix; then
`RegionGraphCache`, exported whole while emitting neither of its implicit
special members, because nothing inside VaelenSim ever constructs one - a defect
that needs DLLs to exist at all, and CMake builds static libraries. The parse
job caught renamed members and wrong argument counts for four tasks running. It
cannot catch what a code generator writes, and it cannot catch what a linker
wants. That is the honest measure of it.

**The machinery, and the stand-in that is gone.** `Tools/Atlas` gained
`--want-bound N` (the host's `StartRules`, deliberately not in the stream, so a
replay must be told them - replaying this month with `--want-bound 1` takes up a
different person and diverges on all four digests), the three printed lines, and
`--stand`, which writes a month played by nobody. The stand-in stood for one day
so that the replay, the two lines, the four pinned digests and the CTest entry
were green before a human was available - and it is how ADR-0139's defect was
found, because `--stand` refused to write a file without a Move. It is deleted;
`--stand` stays.

CTest `Replay.Played` now replays the owner's month through a `-P` driver that
holds both the exit code and the three lines, and refuses an expectation too
short to contain a digest - the guard the review's own finding earned, when
unquoted parentheses cut `-DPLAYED` at its first `(` and the entry pinned
nothing at all. `TIMEOUT 1800`, `COST 4000`.

STATUS: 14.10 clauses (a), (b) and (d) MET. (c) - the frame rate with the HUD
over the 13.09 scene - is the one figure outstanding, and the phase closes when
it is written down.

### 14.08 written - VaelenGame, the one place a world is held

Written, parsed and fenced here; NOT built. The engine half is the owner's
machine and this section will say so until the two lines are pasted.

`UVaelenWorldSubsystem : UGameInstanceSubsystem` owns `Run::Aelvor`,
`Run::Door` and one `WorldGen::RegionGraphCache` behind a pimpl -
`FVaelenHeld`, declared in the header and defined in the `.cpp` - so the
PUBLIC header includes `CoreMinimal.h`,
`Subsystems/GameInstanceSubsystem.h`, `Vaelen/Core/Hash.h`, the view leaves,
`Player/Intent.h`, `Player/Stream.h` and its own `.generated.h`, and names no
World, no Run and no Take. `Begin(size, years)` generates AELVOR, takes
somebody up through the door and takes the five views; `AdvanceDay(n)` turns
n days, each a recorded `DayTurned`, then retakes them all; `Mean(command)`
goes through `Door::Mean` - which stamps `Issued` with the world's clock -
and retakes the LIFE and the PAGE after every call, so a refusal is never
read off a stale page; `Stream()` is everything the host has done, in order;
`Digests()` is the three a replay is compared by plus the page's own;
`WriteStream()` writes `Saved/Vaelen/<seed>-<size>.stream` through
`FFileHelper`. NO `Tick`: ADR-0138's day turn is a recorded input, and a
world that also moved on the frame clock could not be replayed from a key
sequence.

Four console commands, each resolving the subsystem through the command's own
`UWorld` -> `GetGameInstance()` -> `GetSubsystem<>()` and each printing
`LogVaelenPlay: no game instance - run with -game` and stopping when there is
none. `Vaelen.Do` goes through `Press()` with a fresh page, as a key press
will: what the page does not offer costs the world nothing.
`Vaelen.Stream.Write` prints the two lines this row quotes, which 14.10's
checker compares with what `Tools/Atlas --replay` prints of the same stream.

One reading settled, because the row can be read two ways: the second line,
`verbs work W rest R eat E wait T speak S give G take K move M`, carries
COUNTS and not key letters. The letters are placeholders that happen to be
the keys 14.09 binds - a mnemonic, not the output - and 14.10's own clause
(a) is what settles it: "the `LogVaelenPlay: verbs ...` line with every count
>= 1 and `move M` counted among the taken". The code prints
`verbs work %u rest %u eat %u wait %u speak %u give %u take %u move %u`,
counted from the stream's own records: what was MEANT, whatever the world
made of it.

What the fence and the parse caught on the day they were needed: the witness
UI of 14.07 had been written against a made-up API (`Page()`,
`TurnTheDay()`), and the FIRST parse after the real header appeared failed on
both - `parse_engine_modules.py` prefers a module with a `Private` directory
under `Source/`, so the witness UI is compiled against the real thing. The
witness's own `VaelenGame` is deleted, superseded by the real module, and the
cross-module `.generated.h` that the shared stub directory exists for is now
that module's. `test_engine_shim.py`'s Press mutation went stale on the
rename and said so.

`Tools/EngineShim`'s `FString` gained `Reset()` and `operator==`, both narrow
and both real in Unreal: the parse must be a SUBSET of the truth, never a
superset.

Headless, as the row asks: the `parse` job reports 4 engine modules and 12
translation units, 0 errors; `Kernel.UiFence`, `Kernel.UiFenceSelfTest` and
`Kernel.WorldWiring` green; 13 shim mutations caught with the control and the
reverse control; purity 216 files. Nothing of the headless kernel was
touched, and `VaelenPresentation` was not either.

STATUS: UNVERIFIED. What is missing is exactly one thing and it is named:
UnrealBuildTool `Result: Succeeded` on the owner's machine, then

    UnrealEditor-Cmd.exe Vaelen.uproject -game -nullrhi -unattended -log \
      -ExecCmds="Vaelen.Play 256 100, Vaelen.Do work, Vaelen.Day 30, Vaelen.Stream.Write, quit"

printing the two `LogVaelenPlay:` lines with D = 30 and N = 1, and
`Tools/Atlas --replay Saved/Vaelen/41454c564f52-256.stream --panel` printing
the same two lines here after the prefix. If the failure line appears
instead, the fallback the row names is a `UEngineSubsystem` reached through
`GEngine->GetEngineSubsystem<>()`, the same public header, one more build.

Next: 14.09, `VaelenUI`.

### 14.09 written - VaelenUI, the first screen

Written, parsed under the restricted include set and fenced; NOT built. The
screen itself is the owner's machine and this section says so until it has
been seen.

`AVaelenHUD::DrawHUD` writes the subsystem's cached `PanelView` through
`Vaelen::View::Lines` into a `char[PanelTextBytes]` and draws each row with
`Canvas->DrawText`, verbatim: the bytes on screen are the bytes `View.Panel`
digests, which is what makes a screenshot checkable against a number. A page
that does not fit whole is not drawn at all - `Lines` returns 0 - so a screen
is never a half page with its digest row missing.

`AVaelenPlayerController` binds the letters the PAGE carries (`VerbView::Key`,
14.06): W work, R rest, E eat, T wait, S speak, G give, K take, M move, Tab
cycles what the next Speak, Give, Take or Move is aimed at - through the
page's own `Company` and `Near` - Space is `AdvanceDay(1)`, one recorded
`DayTurned` (ADR-0138), F9 writes the stream. Every verb goes through
`Press()` first: what the page does not offer is logged as the refusal the
page foresaw and costs the world nothing, and what it offers becomes the
`PlayerCommand` handed to `Mean` - the module's single write. Each press logs
`LogVaelenUI: <verb> -> queued` or the door's refusal, which the row asks
for.

`AVaelenGameMode` names the HUD and the controller in C++, and
`Config/DefaultEngine.ini` names the mode:
`GlobalDefaultGameMode=/Script/VaelenUI.VaelenGameMode`. No asset, no
Blueprint, no UMG - and that one line is the only part of this task the parse
job cannot see, which is said here rather than discovered later.

The witness of 14.07 is gone, its whole job done: it existed so that the
fence and the parse were green BEFORE the real modules, and both real modules
now exist. It caught two things while it lived - a UI written against an API
the real subsystem did not have, and, through `test_engine_shim.py`'s STALE
check, a mutation that stopped testing anything when that API was corrected.
`Tools/check_ui_fence.py` now reads `Source/VaelenUI` and
`Source/VaelenGame/Public`, 8 files, and the shim's mutations name the real
files.

`Tools/EngineShim` gained `EKeys::Tab` and `EKeys::F9` - the parse refused to
bind keys the stand-in did not carry, which is the stand-in doing its job.

Headless: 13 translation units across four engine modules parse, 0 errors, of
which 4 are VaelenUI's under the restricted set; `Kernel.UiFence` and its
self-test green (12 mutations and the closure); 13 shim mutations caught.

STATUS: UNVERIFIED. What is missing: UBT `Result: Succeeded` in the same
build as 14.08, and the log carrying `LogVaelenUI: <verb> -> queued` for each
of the eight verbs pressed once in PIE. The screenshot is 14.10's.

Next: 14.10, the gate.

---

## 21. Phase 15 - STREAMING & LOD: task breakdown

Planned on 2026-09-16 by four readers over the existing machinery, three
competing breakdowns and three judges. Two judges picked the headless-first
shape, one picked the player-first shape; the section below is the headless-first
one with the grafts all three judges demanded, in the order they demanded them.

**What the planning found before a line of it existed.** Five things, every one
confirmed against the code here rather than taken on the planners' word.

1. **`ReleaseDetail` has no caller outside the tests.** `RequestDetail`
   (`Population/Lod.cpp:75`) only appends and returns false for good once
   `LodState::MaxWanted = 8` is reached. `Aelvor::NearDetail` asks for up to
   three neighbours on every taking; `Door::Day` takes somebody new up whenever
   the played person dies. One region at `Begin`, three per taking: after
   roughly three deaths the list saturates, `near:` goes empty and the world
   refuses every Move as `TooFar` for the rest of that world's life. **ADR-0139's
   fix expires.** The checked-in month has exactly one taking, which is why CI is
   green and why the Phase 14 gate is honest - and is exactly the shape of defect
   a stream with one taking cannot catch.

2. **`DemoteRegion` would destroy the played person.** `Persons.cpp:371-386`
   walks every entity whose `PersonInfo.Region` matches and destroys it, with no
   `PersonHeld` check anywhere in the file, while the crossings in `Lod.cpp`
   honour `PersonHeld` explicitly. Unreachable today precisely because nothing
   releases - and reachable the moment (1) is fixed. **So (1) is not the first
   task.**

3. **The detail bridge fires once per simulated year.** `LodSystem::GetLod()`
   returns `SimLod::World` (`Lod.h:166`) and `LodSchedule::Period[4] = 8640`
   ticks (`Sim/System.h`), against a calendar of 1 tick = 1 hour and 8640 hours
   to the year. So a region requested mid-year is not detailed until up to a
   year later, and any phase gate asking for N promotions inside thirty days
   cannot be satisfied by any implementation. ADR-0139 works at all today only
   because `Begin` ends exactly on a year boundary, so the bridge's next firing
   lands on the first day turn. That is also where 14.10's measured 340 ms first
   day comes from.

4. **A believer vanishes on a crossing into a full neighbour.** `Lod.cpp:304-310`
   checks `Dest.Add(Culture, 1)` and breaks when it fails, then calls
   `DestFaith->Add(Religion, 1)` and ignores its answer -
   `RegionFaith::Add` returns false when none of its `MaxFaiths = 4` slots is
   free (`Sim/Religion.h:78`). The person joins the head count and leaves the
   faith count. A conservation leak with no instrument watching it.

5. **`RegionLod::Level` is a boolean wearing a five-value costume.**
   `Sim/Population.h:73-78` declares `uint32 Level = 4` with
   `DetailedLevel = 2`; the only value ever written is 2 (`Persons.cpp:218`),
   and the component is added at 2 and removed, never re-levelled. Levels 0, 1
   and 3 are unreachable. The phase's own row says "simulation LOD 0-4". The
   phase must deliver gradations or rewrite the row; inheriting the number is
   how Phase 16 inherits the confusion.

**The design rule for the whole phase, and what makes ten tasks survivable.**
Phase 15 declares no new component type and changes no stored layout. Residency
lives in a host-side object rebuilt from the recorded inputs, so nothing enters
the state digest and a replay loses nothing. Every new behaviour sits behind
`Options::Stream = false`. The consequence is clause (f) of the gate rather than
a hope: **of the frozen constants of fourteen closed phases, zero move.**

| Task | What | Test | Where |
|---|---|---|---|
| 15.01 | A held person pins its region. `LodSystem` phase 1 skips a region holding a played person, AND `DemoteRegion` refuses when called directly - two layers, because the system should decide and the function should not be trickable. Publishes `RegionPinnedEvent`. No field added to `LodState`. | `Population.Pinned`: demotion attempted on the held region, refused both ways; the played entity still alive and `Population::IsConsistent` true; `PopulationGate` and `GameplayGate` digests unmoved (no world in them holds anybody). | **VALIDATED headless 2026-09-16** — CTest `Lod.APinnedPersonSurvivesTheDemotionTheBridgeWanted`, 28 checks, ADR-0140 |
| 15.02 | The cadence. Detail decisions move off `SimLod::World` (8640 ticks) onto a pass at `SimLod::Aggregate` (24 ticks, one day), so a requested region is walkable the next day rather than up to a year later. Costed before it is written: the pass runs ~36 000 times over a 100-year pre-history, and ADR-0119 records CI with no headroom, so the TIMEOUT/COST estimate and the `-E` branch are decided in the task, not after it. | `Run.Cadence`: a region requested on day 1 is detailed by day 2, not day 361. The 83-day month still replays to its four digests. | **VALIDATED headless 2026-09-16** — CTest `Lod.DetailIsDecidedOnADayAndTheCrossingsKeepTheirYear`, ADR-0141; 45 suites including every gate pass unchanged |
| 15.03 | `ReleaseDetail` gets its first production caller. `NearDetail` releases the previous taking's neighbours before asking for the new ones, never releasing the played person's own region or `Begin`'s, and reads the `LodRules` the `LodSystem` was built with rather than a fresh default. | `Run.Detail`: **the saturation itself as the control** - twelve takings reach `Wanted == 8` and refuse before the fix, stay under it after, and no taking ever finds `near:` empty. `Replay.Played` and `Atlas.Frozen128` unmoved. | **VALIDATED headless 2026-09-16** — CTest `Aelvor.TwelveTakingsDoNotSaturateTheWantedListAndNearIsNeverEmpty`, 31 checks, ADR-0142 |
| 15.04 | The conservation audit: `Population::Audit`, recomputed and never accumulated, declaring no component - heads, cultures and faiths counted both ways across every promote, demote and crossing. The instrument the next three tasks are measured by. | `Population.Audit`, with reverse controls: a seventh-culture mutation must make `Dropped >= 1` and a full-faith mutation must make the faith gap `>= 1`, or the audit measures nothing. | **VALIDATED headless 2026-09-16** — CTest `Lod.TheAuditCountsBothGrainsAndCanBeMadeToSayNo`, 28 checks, three reverse controls |
| 15.05 | The vanishing believer of finding 4, and any sibling the audit turns up. | The audit's faith gap is 0 over a 120-year world that crosses into full neighbours; the mutation that removes the fix makes it non-zero. | **VALIDATED headless 2026-09-16** — CTest `Lod.AFifthFaithIsRefusedAndTheCrossingNowReadsTheAnswer`, 25 checks. The staging needed a harness: the two systems that write `RegionFaith` (`Religions`, `Disasters`) are taken out of the scheduler once the region is detailed, because they rewrite faith tables BEFORE the crossing reads them inside the same year — three earlier stagings died there. The control was run for real rather than reasoned about: with the fix reverted, 181 believers cross into a destination with no room for their faith and the test FAILS; with it, 0 |
| 15.06 | `Attention{Region, Reach, Most}` as a leaf of `VaelenRun` - deliberately NOT `View::Eye`, because the eye is an input to the VIEW and this is an input to the RUN. `Looked{Tick, Region, Reach}` becomes a fourth stream record; `Door::Look` stamps the tick from the world's clock as `Door::Mean` stamps `Issued`. A version-1 stream simply carries no `Looked` line, so the checked-in month still loads. | `Run.Attention`: a stream of 400 `Looked` records replays to the same digests; the same stream minus its last record does not. `Replay.Played` unmoved, proving the decoder still reads a stream written before the record existed. | **VALIDATED headless 2026-09-16** — CTest `Door.ALookIsARecordedInputAndAStreamWithoutOneStillReads`, 35 checks, ADR-0143; 28 suites green including `Replay.Played`, which is the proof a version-1 stream with no `l` lines still reads |
| 15.07 | The warden: the residency policy itself, a deterministic function from attention to detail requests, host-side, writing requests and never promotions, so ADR-0037 holds by construction. | `Run.Warden`: the same attention sequence yields the same requests on all eight legs; hysteresis proved by a walk back and forth over one border producing a bounded number of changes. | **VALIDATED headless 2026-09-16** — CTest `Door.TheWardenTurnsALookIntoRequestsAndDoesNotThrashOnABorder`, 18 checks, ADR-0144; ten crossings of one border change the watched set zero times |
| 15.08 | The promotion hitch, bounded as a COUNT and not a wall clock, which is what ADR-0109 actually forbids. At most one region may be promoted on any single day turn, and the heads materialised per day turn are capped. | `Run.Hitch`: a 100-day walk crossing borders never promotes twice in one day turn; the cap is asserted, and removing it makes the test fail. | **VALIDATED headless 2026-09-16** — CTest `Door.NoDayTurnPromotesTwiceWhileTheCameraWalks`, ADR-0145. Control run: cap off, worst day turn promotes 3 regions and the test fails; cap on, 1 |
| 15.09 | `RegionLod::Level`: either the gradations the number promises, or the row rewritten to say two grains and an ADR saying why. Decided in the open, not inherited. | **VALIDATED headless 2026-09-16** — the row was rewritten, not the type. CTest `Lod.ARegionHasTwoGrainsAndNotFive` pins that every mark in a world reads `DetailedLevel` and that the five-rung ladder is `SimLod`, rung by rung. ADR-0146 |
| 15.10 | The engine half, one build and one sitting: the camera's region and reach handed to `Door::Look` each day, and a stream written with `Vaelen.Stream.Write` that carries `Looked` records. | **VALIDATED 2026-09-21 — THE GATE IS MET.** Built by UnrealBuildTool (UE 5.6, MSVC 19.51, Win64 Development Editor) and PLAYED: 142 day turns, 138 looks, 4 takings and 2 Speak intents at the keyboard, with the daily cadence on. `VaelenAtlas --gate` replays it on gcc/Linux to the same four digests - `state 609253a29361ec5f, log a2839792e0837328, life 0a4babc60f6e4d90, panel c4ddbe971538c59c` - and keeps all six clauses: 15 pins published while held, 0 wrong, both grains agreeing on every one of the 142 days, no day turn promoting twice, somewhere to walk within one day turn of each taking. CTest `Replay.Lived` and `Run.Gate.Lived` pin it. | engine |

**The gate.** A walk, recorded on the engine machine and replayed here, that
proves all of: (a) the stream carries `Looked` records and at least one region
was pinned while held - the fence fired rather than merely existed; (b) the
replay reproduces the walk's four digests byte for byte, as 14.10's did;
(c) over the walk, the audit's head, culture and faith gaps are all 0;
(d) no day turn promoted more than one region; (e) across at least four takings, somewhere to walk
arrived within four days of each - "never empty" was NOT a clause any
implementation could meet, because the neighbours a taking asks for are requests
the bridge answers on its next daily pass and 15.08 caps that pass at one
promotion a day, so the third neighbour cannot be detailed before the third day.
The stand-in walk measures the real figure: ONE day turn - `--walk` reads it at
the start of a day and `--gate` at the end of one, and since 2026-09-18 both say
1. An earlier version of this paragraph said two, which was the figure of the
walk written BEFORE the Phase 15 review regenerated it, and neither instrument
has said two since. That is the property ADR-0139 claimed, stated at a rate the
world can keep; (f) every frozen constant of Phases 00-14
unmoved, checked rather than asserted. Any one missing and the phase stays open.

**How the sitting goes.** In the editor, with `-game`:

```
Vaelen.Play 128 100 1     the daily cadence; without the 1 the world ignores the
                          camera, and the controller then records no look at all
(fly, and press Space twenty-five times)
Vaelen.TakeUp             let this one go, take somebody else up
(fly, Space twenty-five more)   and again, and again - four lives in all
F9  or  Vaelen.Stream.Write
```

**WHAT THE SITTING ACTUALLY TAUGHT, 2026-09-21.** The instructions above were
followed and the gate REFUSED the first walk on clause (a): 99 looks, 0 pins.
The walk was not at fault and neither was the code - the instruction was. It
said "stay near the centre and move a little", and the camera sat on region 54
for seventy-five days while the played person lived in region 9, forty-five
metres away.

A pin fires when a detailed region stops being wanted AND holds somebody. The
camera is what makes a region wanted. So **the fence only fires when the camera
passes over the played person's own region and then leaves it** - which the
stand-in walk does by accident every ninth day, because it changes region daily
over sixty of them. Isolated by experiment on the recorded walk rather than
argued: raising the reach from 0 to 1 changed nothing (0 pins), making the
camera wander changed everything (57 pins), and alternating between two regions
that are NOT the played one gave 0 again while alternating with the played one
gave 38.

The correction needed no new session: thirty more day turns alternating
`Vaelen.Look 9` and `Vaelen.Look 54` through `Vaelen.Day 5` were appended to the
same PIE, and the gate passed. Note `Vaelen.Day` does not consult the camera -
it uses the remembered look - so a whole walk can be driven from the console
with exact region numbers, which is how the last thirty days of the checked-in
walk were made.

**`Vaelen.TakeUp` is not optional, and finding out why cost a measurement.** The
gate asks for at least four takings, and the door takes somebody up on its own
only when the played person dies. They do not: four thousand day turns at
AELVOR 128 - eleven years of play - and the first person taken up was still
alive at the end. Without that verb a sitting records exactly ONE taking, and
the clause was unmeetable at the keyboard for want of a verb rather than for
want of a world. `Door.TheHostsOwnSittingMeetsTheGate` drives this exact
sequence headlessly and asks the gate's clauses of it, so the recipe cannot rot
in silence.

`Vaelen.Look <region>` is there for a host with no camera over the played world.
It is NOT a step of the sitting: `AVaelenPlayerController::TurnTheDay` overwrites
it from the camera on the very next press of Space, so a look typed with
`bLookFromCamera` on lives until the next keypress and no longer.

`Vaelen.Stream.Write` prints three lines. The first two are Phase 14's, quoted
verbatim in 14.10's checker; the third says how many looks the stream carries,
which cadence the world was begun under, and the command to run. Then, on the
file it names:

```
VaelenAtlas --gate <file> --want-bound 0 --expect "state ..., log ..., life ..., panel ..."
```

taking the four digests from the first line `Vaelen.Stream.Write` printed. It
reports one PASS/FAIL line per clause for (a), (b), (c), (d) and (e), plus one
for the digests - `--expect` is what makes that one a judgement rather than a
line to read, and without it the command says so instead of claiming a pass.
Clause (f) is the rest of the suite.

The walk wants **at least four takings**, so the sitting is long enough for the
played person to die four times over. The gate refuses a walk with no day turns
and refuses one whose last taking was still waiting for somewhere to walk when
the records ran out.

Next: 15.01, the fence that has to exist before anything else is allowed to
demote.

## 22. Phase 16 - SAVE/PERSISTENCE: task breakdown

Planned on 2026-09-21 by two readers who measured before they proposed, four
competing breakdowns and two judges. One judge picked the evidence-first shape
("determinism"), the other the smallest-safe shape ("minimal"); both picked the
same load-bearing rule, so the section below is the smallest-safe skeleton with
the evidence discipline of the other grafted into its gate, plus the four things
the remaining two plans saw and nobody else did. Where a task came from another
plan it says so.

**What Phase 16 is actually about.** The roadmap row promises "serialisation of
the whole world state", and that shipped in Phase 01: a played AELVOR 128
snapshots to 21,980,402 bytes, restores to an identical state digest, is a
byte-for-byte fixed point across load-then-re-save, and — with nobody looking —
continues for ten day turns to the same digest as the world it was copied from
(`cff08aea2e255220` on both sides). The world is saved. What is not saved is the
RUN: `Run::Aelvor`'s own members live outside the image, so the moment anybody
LOOKS, a restored world makes different detail decisions from the same records
and the two worlds part company (`6d4b82134edfcbf6` against `02c94cdaf3367b43`).
The whole gap is `Near_` and `Watched_` and about thirty bytes — one part in
700,000 of the image — and it was isolated by experiment: `Near_` alone gives
`a0e2fa35c353c33e`, `Watched_` alone `a6aaae6acbf09d00`, both together reproduce
the original bit for bit, and at the fullest wiring over sixty days the LOG
digest matches too. Worse than the divergence: a world loaded into a
`Run::Aelvor` that never ran `Begin()` is inert and permanently so — `TakeUp`
returns 0, `LookAt` skips the warden, `Day()` advances nothing, and `Begin()` is
refused forever because the clock has moved — so "load a save" today costs a
full world generation (722 ms at 128, 4,615 ms at 256, 52 s for a 500-year 256)
to produce a world the image immediately overwrites. So Phase 16 is not a
serialisation phase. It is: a save function that can refuse, a load that cannot
half-apply, a CONTAINER around the untouched image carrying the thirty bytes and
the provenance, a verb by which a run adopts a world it did not create, a file
on disk that cannot destroy the last good one, and a migration chain keyed on
something that actually moves when the format breaks.

**The design rule for the whole phase, and it is the same rule Phase 15 shipped
on.** Nothing is ever added to the image. `SaveSnapshot`'s bytes and
`ComputeStateDigest`'s value do not change, because the trailer digest is
computed over bytes that include the version, the flags word, the layout digest
and the seed (`Snapshot.cpp:255`) and `ComputeStateDigest` returns that trailer
(`Snapshot.cpp:305-312`). Every frozen phase-gate digest hangs off it. Everything
this phase needs to add — extents, host options, run state, provenance,
migration version, section digests — lives in a container OUTSIDE the image,
where it costs nothing. The consequence is clause (b) of the gate rather than a
hope: **of the frozen constants of sixteen closed phases, zero move.**

### What the planning found before a line of it existed

Twelve defects, every one confirmed against the code in this tree rather than
taken on a planner's word.

1. **`SaveSnapshot` cannot fail, and cannot say so.** It returns `void`, drops
   the body's result into `[[maybe_unused]]` and checks it with `VAELEN_CHECKF`
   (`Snapshot.cpp:253-254`), which `Assert.h:154` compiles to `((void)0)` when
   `VAELEN_ASSERTS_ENABLED` is 0 — which `Assert.h:33-43` sets for `NDEBUG` and
   for `UE_BUILD_SHIPPING`/`UE_BUILD_TEST`, exactly the builds a player runs.
   A failing body writes a short image; line 255 then hashes those short bytes
   and appends a trailer over them, so the file is well-formed, wrong, and
   validates on load. The same macro is the only guard on
   "cannot snapshot while dispatching events" (`Snapshot.cpp:185`). This is the
   single live data-loss defect in the round, and it costs zero digests to fix.

2. **A failed load leaves a running chimera, and `Run::Aelvor` cannot discard
   it.** Measured: world B at `3b4b2bfece864fb4` was fed a 60%-truncated,
   RESEALED image of world A. `LoadSnapshot` honestly returned `Truncated`. B was
   left at `a981acc1ba3607cf` — neither world — with A's clock (tick 2851440) and
   B's event log, then turned ten more day turns with nothing firing, reached
   `4e95d9a61dad4c76`, and answered `Life()` with a person's name.
   `Snapshot.h:57-59` tells callers to discard such a world; `Aelvor.h:210-211`
   holds the world inside a `unique_ptr<Kernel>` with no reset, `World` is
   non-copyable, and the kernel is built in the constructor. The contract is
   unsatisfiable by the only real caller. This needs no malice, only a short
   read — and a resealed image is precisely what the migration tooling this
   phase promises produces by definition. (Found by "adversary", by nobody else.)

3. **The image's integrity protection is inverted.** On a 7,015,163-byte
   AELVOR 64 image: trailer intact, 400 of 400 single-byte flips are caught.
   Trailer RECOMPUTED, the log band is still 60 of 60 refused, because
   `EventLog::ReadFrom` re-derives the per-event chain
   (`EventBus.cpp:60-68`) — while the pools-and-map band is 60 of 60 ACCEPTED
   with a silently wrong state digest, and the clock/ids/entities band 52 of 60.
   The 84-93% that is chronicle is checked twice; the 9% that is the actual
   simulation has no interior check at all.

4. **The header `Flags u32` is written 0 and never read.** `Snapshot.cpp:249`
   writes it, `:283` reads it into a local, and no branch anywhere looks at that
   local. Measured: an image with `Flags = 0xDEADBEEF`, trailer recomputed,
   loads `Ok` with an identical digest. There is no channel by which an image
   can tell an older reader "I am not what you assume", which is exactly what a
   truncated log or a new section would have to say.

5. **The host's declared world is never checked against the save.** A host
   declaring `Options::Size = 64` loading a 128 image into a never-generated
   world returns `Ok`: `WorldMap::LayoutDigest` (`WorldMap.cpp:29-37`) folds
   layer name hashes and element sizes and no extents, and on load `Reset(Config)`
   takes width and height from the image (`WorldMap.cpp:40-53`) and overwrites
   the target's. The world that comes out is coherent; what is discarded in
   silence is the host's statement about which world it thought it was opening.
   `Options::Stream` is invisible to every existing guard because it declares no
   component type — and `Aelvor::Header()` then writes a `StreamHeader` from
   `Given_.Size`, naming a world that does not exist.

6. **Six structurally different causes return one word.** A build that added a
   type, an older build reading a save with an extra type, the same types
   registered in the other order, a field added to a component (ADR-0090), a type
   renamed, and a different seed — all six return `LayoutMismatch`.
   `MissingPool` fires for none of them; its only reachable path is a registered
   type with no pool, which no production wiring produces, and the reconciliation
   check at `Snapshot.cpp:164` is dead behind the equality gate at `:137`. A seed
   mismatch is reported as `LayoutMismatch` (`Snapshot.cpp:291-296`), telling a
   player their components changed when what happened is they opened another
   world's save. Three of the six are routine evolution and one is "this is not
   your world". A migration keyed on a diagnosis that cannot tell them apart is
   keyed on nothing.

7. **Pools are matched by position, not by name.** The image already writes
   `NameHash` and `ElementSize` per pool; the loader checks `TypeId == Id`
   instead. So a pure registration-order refactor, in which nothing about the
   world changed, makes every existing save unloadable, and a build that adds one
   component type cannot read yesterday's save. Every phase from 03 to 15 added
   component types; the full AELVOR wiring registers 55. (Found by "migration".)

8. **The save-format version does not move when the save format breaks.**
   `VAELEN_SAVE_FORMAT_VERSION` is 3 and its own comment dates that to Phase 02
   ("3: 32 world-gen parameters (02.03)"), while Phases 03-15 invalidated every
   prior save repeatedly without touching it. `Version.h:10-11` has claimed since
   Phase 00 that "Persistence/Migration keys its upgraders on this number";
   `grep -rn Migration Source/` returns only population migration. And the
   roadmap line cannot be satisfied literally, because the version sits inside
   the bytes the trailer hashes: bumping it moves `ComputeStateDigest` for every
   world and every frozen gate digest with it. Migrations must key on the
   container.

9. **The layout digest cannot see a field.** `ComponentType.cpp:65-74` folds
   `NameHash` and `Size << 32 | Alignment` and nothing else.
   Reordering two same-sized fields, or retyping a `uint32` to a `float`, moves
   no digest, passes every check the loader has, and loads old bytes into new
   fields with no result code and no symptom. ADR-0090 prices ADDING a field;
   nothing prices CHANGING one, and a save format is exactly where that bill
   arrives. See OUT OF SCOPE: this phase does not close it.

10. **`Run::Aelvor` has no way to adopt a world it did not create.** `Begun_` is
    false after a load and nothing can set it: `TakeUp` returns 0 on `!Begun_`
    (`Aelvor.cpp:617`), `LookAt` skips the warden (`:461`), `Day()` ticks nothing
    (`:782`), and `Begin()` is refused forever because `PreHistory::Generate`
    rejects a world whose clock has left `StartTick` (`PreHistory.cpp:73`).
    Measured: a constructed-but-never-`Begin()`-ed world accepts the image in
    70 ms and reaches the saved digest and the correct +10-day digest — the
    722 ms of generation is not required at the WORLD layer, and is 93-96% of
    restore time. The brief's finding 5 ("a load costs a world generation") is
    wrong about the world and right about the run.

11. **`Aelvor.h` documents an invariant the save path breaks, and the wrong
    sentence is why the divergence survived two phases.** `Aelvor.h:221-232` says
    of `Near_` and `Watched_`: "a replay rebuilds it by applying the same takings
    in the same order, so nothing here enters a digest". The premise is true; the
    conclusion silently assumes replay is the only way back. `Reside` releases
    only a region it remembers asking for (`Aelvor.cpp:530`) and refuses to
    release anything in `Near_` or equal to `Detail_` (`:532`, `:566`), so a
    restored world remembers nothing, releases nothing, re-requests what is
    already wanted, and the 8-slot `LodState::Wanted` leaks from
    `[26 2 9 10 15]` to `[26 14 15 19 23 24]` and saturates at `MaxWanted`.
    Different regions get detailed, so different people exist.

12. **`Tools/run_gates.sh` does not run the Phase 14/15 gates.** Its loop
    (`:26-28`) iterates eleven entries and contains none of `Run.Gate`,
    `Run.Gate.Lived`, `Replay.Played`, `Replay.Walked`, `Replay.Lived` — the
    newest code, in the phase most likely to move a digest, checked by the script
    whose own header comment (`:8-13`) describes that exact accident happening
    before. Two lines. (Found by "determinism".)

Two more, recorded and not acted on. `<fstream>` and `<filesystem>` are banned in
all thirteen modules of `Tools/kernel_modules.txt` — `VaelenRun` included — the
latter with the reason "OS file-system access does not belong in the simulation
kernel" (`check_kernel_purity.py:119-146`), and the only file I/O in the project
is `std::fopen` in `Tools/Atlas`. So "checkpoints on disk" cannot be built where
the roadmap line implies, and 16.07 decides that in the open rather than
discovering it. And `DiplomacySystem::Graph`/`GraphRegions` (`Diplomacy.cpp:72`)
is keyed on region COUNT alone; it is stale-proof today only because
`LoadSnapshot` refuses a different seed and the map is a pure function of seed
and config. It is the one cache that would not notice a genuinely different map,
and the first thing to break if anything ever loads a save into a world generated
differently. 16.10 records it. It does not fix it.

One claimed danger that does NOT exist, so that no task is spent on it:
`EventLog::ReadFrom`'s `Size != 16 + Count * sizeof(Event)` check cannot be
defeated by integer overflow. 112's odd part is 7, invertible modulo 2^64, so
exactly one `Count` satisfies the congruence for a given `Size` and it is the
honest one; solved explicitly in the probe, which recovered the true event count
and no wrapping solution.

### Tasks

Fourteen, which is above Phase 15's ten, and the reason is that four separate
things are missing rather than one. They are ordered so that no commit can take
the tree red for a reason the task did not intend: 16.01 freezes the instruments,
16.02-16.03 make the two existing functions honest, 16.04-16.07 build outward
from the image without touching it, and the evidence tasks come last because they
are what the earlier ones are measured by. 16.14 is outside the gate.

| Task | Deliverable | Test (with its control) | Why |
|---|---|---|---|
| 16.01 | **The instruments, before anything moves.** (a) `Tests/Sim/Golden/` holding three images written by TODAY's v3 build at 867a129 — bare 16-tile, full-wiring 16-tile, full-wiring 32-tile/5y (about 21 KB, 22 KB, 104 KB) — each with a README line recording the exact `Run::Options`, the writing commit, the format version, the layout digest, the state digest and the byte count, plus the command that regenerates them. Never AELVOR: a 128 golden is 22 MB against a 5.97 MB tracked repo. (b) `Tools/run_gates.sh`'s loop extended to sixteen entries with `Run.Gate`, `Run.Gate.Lived`, `Replay.Played`, `Replay.Walked`, `Replay.Lived`. | `Golden.V3RoundTrips`: each golden loads into a freshly constructed world of its recorded `Options`, returns `Ok`, matches its recorded state digest, and re-saves to identical bytes. CONTROL: `Tools/run_gates.sh` is run once with a deliberately moved digest in `Run.Gate` and must report `GATES-DONE 1 failing` — a gate list that cannot go red is not a gate list. | From "migration" 16.01 and "determinism"'s defect list. This is a one-way door: 16.09 changes what the container means and 16.08 changes what a refusal means, and after them no build in existence can write an honest v3 file again. Nothing older than this phase would then exist for the migration chain to migrate. Half a day, and it must be the first commit. **DONE 2026-09-21**, and the sizes inside this row were wrong: the third golden was costed at "about 104 KB" and the world it names weighs 22,568,335 bytes - two hundred and sixteen times. The corpus is three YOUNG worlds instead (77478, 78036, 173414 bytes; 329 KB in all), because the event log is 75% of any image that has a history and a 16-tile world goes from 78 KB to 11.4 MB between ten years of pre-history and thirty. The gate list is seventeen, not sixteen. `Tests/Run/Golden/README.md` carries the table, and the corpus proved defect 5 on the day it was written: `full-16` and `full-32` share the layout digest `5ab1a2f994715f25`, so `Golden.TheLayoutDigestCannotTellTwoMapSizesApart` pins the defect until 16.08 fixes it, at which point that test must fail and be rewritten. |
| 16.02 | **`SaveSnapshot` can fail, and says so.** `SnapshotResult SaveSnapshot(const World&, std::vector<uint8>&)` replaces the `void` signature; on failure `Out` is truncated back to its entry size so a caller's buffer is never half-written. The `VAELEN_CHECKF(!W.Events().IsDispatching(), ...)` at `Snapshot.cpp:185` becomes a returned `Inconsistent`. No image byte and no header field changes. | `Snapshot.SaveRefusesRatherThanLies`: a world holding one pool whose `Serialize` returns false — non-`Ok` returned, `Out.size()` equal to what it was before the call; the same world dispatching returns `Inconsistent`. Built and run under `-DNDEBUG` as well as with asserts on, because the defect only exists where the assert is gone. CONTROL: restore the `void` signature and re-run — the image is written short, the trailer is computed over the short bytes and validates, and the test fails on `Out.size()`. That is the silent corruption. That is the silent corruption. | Defect 1, and from "minimal" 16.01, which put it first and was right to. Nothing goes on disk until the function that produces the bytes can refuse. **DONE 2026-09-21, and the defect is real but was described wrongly.** A resealed SHORT image does NOT validate on load: measured at four cut points, `LoadSnapshot` answers `Truncated` every time, because the reader runs out of a section. So the old code did not corrupt a loaded world - it told the caller nothing at the moment of WRITING, and the player learned their save was not one only when they opened it, with the world it came from gone. `Snapshot.AShortImageIsRefusedButTheWriterNeverKnew` says so in its own name. Both new tests ran with `VAELEN_ASSERTS_ENABLED = 0`, which is the build the defect lived in. NOT `[[nodiscard]]`: ninety call sites ignore the result today and putting ninety mechanical edits in the commit that fixes a data-loss path is how a mistake hides; the header says so. |
| 16.03 | **A load that fails changes nothing.** `LoadSnapshot` splits into a validate pass over the whole image — cheap, because 16.04's section table gives every section its own length and digest — and an apply pass that runs only after validate returns `Ok`. On any refusal the target world is bit-identical to what it was. `Snapshot.h:57-59` loses "the world's state is unspecified (callers discard it)". | `Snapshot.AllOrNothing`: take world B to digest X, then feed it in turn a truncated-and-resealed image, a resealed flip in the entity section, an image with a pool's element size altered, and one with a wrong log digest. After each: `B.StateDigest() == X`, and ten further day turns reach the digest B would have reached with no failed load at all. PAIRED ARM, and it is the control: B then loads a VALID image and must change to the image's digest and continue as the original — a stub that makes `LoadSnapshot` a no-op passes the four failure arms and fails this one. | Defects 2 and 3, from "adversary" 16.03, which is the only plan that found the chimera. It belongs before 16.06, because `Adopt` is the exact caller that owns its world and cannot discard it. Shipping `Adopt` without this ships a documented-impossible contract into the path that most needs it. |

**16.03 AS BUILT, 2026-09-21, and it is NOT the mechanism this row planned.** The plan was a validate pass over the whole image followed by an apply pass, and it was cheap only because 16.04's section table would give every section its own length and digest. That table does not exist yet, and a validate pass without it cannot prove a pool or a layer will deserialise - those failures are data-dependent, so a structural pre-pass would have been a validate that does not validate. Building 16.03 on it would have meant either reordering the phase or shipping a weaker guarantee under a stronger name.

What shipped instead needs nothing from 16.04 and does not touch the image: **the target is kept with `SaveSnapshot` before the first byte of it is overwritten, and put back on any refusal.** The two halves are the writer and the reader that already test each other, so the rollback path is the best-covered code in the file rather than a second implementation to get wrong. It costs one image of the target, and only past the header - a bad trailer, magic, version, layout or seed still refuses for free. Measured: 1.3 MB kept in 8.7 ms, restored exactly; ~6.6 ms/MB, so ~145 ms for the 22 MB played AELVOR 128.

The planned test name `Snapshot.AllOrNothing` became `Snapshot.ARefusedLoadLeavesTheWorldItRefusedToChange`, plus `Snapshot.AWorldThatCannotBeKeptIsNotOverwritten` for the case the plan did not foresee: the rollback is taken with `SaveSnapshot`, so a world `SaveSnapshot` refuses cannot be put back and must therefore not be taken apart at all. `SnapshotResult` gains `RollbackFailed` so that the one case where the promise could not be kept is not reported as one where it was.

THE PAIRED ARM THE ROW ASKED FOR WAS RUN AND IT EARNED ITS KEEP TWICE. Against a faithful pre-16.03 `LoadSnapshot`: 11 of 13 Snapshot tests still pass and exactly the two new ones fail, on the clauses that matter - `Intact` and `Usable` - while `Refused == Cuts` passes throughout, which is the proof that the defect was never dishonesty but the wreckage left behind. It also caught a test of mine that measured NOTHING: `AWorldThatCannotBeKeptIsNotOverwritten` first compared `ComputeStateDigest` from inside the dispatch, where 16.02 makes it return its zero sentinel - zero equalled zero and it passed against the unfixed loader too. It now measures from outside the dispatch against a control world that lived through the same event. And an earlier, sloppier control - one that disabled the rollback but left the restore code reachable with an empty buffer - SEGFAULTED, which I nearly wrote down as the pre-fix behaviour; it was an artefact of the control, not a measurement.
| 16.04 | **The container: bytes, and not a file.** `Vaelen/Run/Checkpoint.h`/`.cpp` in `VaelenRun`: magic `VAELENCP`, a container version independent of `VAELEN_SAVE_FORMAT_VERSION`, the inner format version copied out of the image, seed, tick, and a section table of `{kind u16, flags u32, offset u64, length u64, digest u64}` — then the sections: STATE (the output of `SaveSnapshot` VERBATIM, never re-encoded), RUN, HOST, STREAM. Flags split: low 16 bits must-understand, an unknown set bit refuses by name; high 16 bits may-ignore, carried through a read-then-write untouched. An FNV-1a trailer over everything preceding. `BuildCheckpoint(const Aelvor&, std::vector<uint8>&)` and `ReadCheckpoint(...)`. No path, no `<fstream>`, no `<filesystem>`. The section table records the log's event count and byte length, and `Atlas --save --sections` prints each section's share. | `Save.ContainerRoundTrip`: build at three wirings, read back, every header field equal, and `memcmp` of the STATE section against a fresh `SaveSnapshot` of the same world returns 0. `Save.SectionTableIsHonest`: at three wirings the section lengths sum to the container length with delta 0. `Save.UnknownRequiredFlagIsRefused`: bit 5 of the low half set → refused naming bit 5; bit 5 of the high half → `Ok`, and a re-save carries it back out unchanged. CONTROL, from "adversary" 16.02: sweep one flipped byte across 180 offsets spanning three bands, recompute the container trailer after each, and require all 180 refused; the same sweep with per-section digests disabled must report at least 100 silent acceptances — today the pools-and-map band reports 60 of 60. Second control: log one extra event between building the header and writing the sections, and the sum must stop matching. | Defects 3, 4 and 8. The container is the load-bearing decision of the phase and both judges picked it: the image cannot grow, because a section inside it moves every frozen digest at once. Per-section digests go in the container's SECTION TABLE rather than inside the image, which is what keeps that graft at zero digest cost. The must-understand half is the channel the image's own `Flags` word was reserved for and never given. |

**16.04 AS BUILT, 2026-09-21.** `Vaelen/Run/Checkpoint.h`/`.cpp` in `VaelenRun`, plus `Tests/Run/Test_Checkpoint.cpp` (CTest `Run.Checkpoint`) and `Atlas --save --sections`. No path, no `<fstream>`, no `<filesystem>` - the container is bytes and WHERE they go is the host's business.

**ONE SECTION IS WRITTEN, NOT FOUR, AND THAT IS THE DEPENDENCY ORDER RATHER THAN A SHORTCUT.** STATE carries `SaveSnapshot`'s output verbatim. RUN's payload is 16.05's `RunState`, which does not exist yet; HOST and STREAM are 16.07, and one of them turns on a question still open for the owner (does a save carry its recorded tape). Emitting them empty would have been fake content in a table whose whole job is to describe bytes honestly. The kinds are declared in the enum with the task that fills each, and the table is variable-length by design, so 16.05 adds a row without a container version bump.

**THE VERBATIM CLAUSE IS CHECKED BY `memcmp` AND NOT BY ARGUMENT**: a fresh `SaveSnapshot` of the same world is byte-identical to the STATE section at all three wirings. A container that re-encoded the image - even losslessly - would pass every other check on the page.

**THE CONTROL CAME OUT STRONGER THAN THIS ROW PREDICTED.** The row asked for 180 flipped bytes across three bands, each with the container trailer RECOMPUTED so the file agrees with its own damage, and for the digest-less arm to report "at least 100 silent acceptances". Measured: **180 of 180 refused with per-section digests, 0 of 180 refused without** - not a hundred silent acceptances but a hundred and eighty. The section digest is doing all of the work and the trailer none of it, which is exactly what putting the digests in the TABLE rather than in the image was supposed to buy, at zero frozen-digest cost.

Two guards the row did not name were added because the table is only worth having if it cannot lie: sections must be in order and must not overlap (an overlapping table is how one run of bytes is made to be read twice with two meanings), and every byte between the table and the trailer must be claimed by some section (unclaimed bytes are where a second meaning hides). `InnerVersionMismatch` is a separate result from `VersionMismatch` because the entire point of a container is that the two versions move independently, so a reader must be able to say WHICH one it disagrees with.

`Atlas --save --sections` confirmed 16.01's measurement by a second route: at 32 tiles and 20+20 years the container is 818,041 bytes and the log is 626,656 of them - **76.6%**, against the ~75% measured in 16.01 off the image alone.
| 16.05 | **The run travels with the world.** One `RunState` struct — `Begun_`, `Detail_`, `Dug_`, `Near_`, `Watched_` — that `Aelvor::Begin()` fills and the RUN section carries, so a field cannot be added to one without being added to the other. `Aelvor::GetRunState()/SetRunState()`. `Ways_` is deliberately not saved and the reason is asserted rather than assumed: `WorldMap::Serialize` calls `Reset` which does `++Replaced` (`WorldMap.cpp:53`) and every `RegionGraphCache` keys on `HashCombine(Map.Revision(), W, H)` (`Regions.cpp:417`), so a load invalidates it by construction. `Eyes_` rides along although it is written and never read, because it costs 12 bytes and the next person to read it should find it right. The comment at `Aelvor.h:221-232` is corrected. | `Run.RestoredWalkContinues`: `Options{128, Play, Stream, Lively, Colony}`, take up, 20 day turns with looks, checkpoint, then 60 more with the same looks on both sides — state digest AND log digest equal. CONTROLS, three, because the probes proved no single one is sufficient: `Watched_` withheld must diverge, `Near_` withheld must diverge, both withheld must reach the already-measured `plain` value. The 128 Play+Stream cell must reproduce `6d4b82134edfcbf6` against `02c94cdaf3367b43` and `Wanted(5)` against `Wanted(6)`. | Defect 11, measured twice independently. `Detail_` and `Dug_` are saved rather than derived: "migration" proposed recovering them from components, and the probes could NOT vary them in isolation to check that, so the phase copies eight bytes instead of trusting an unverified derivation. Robust before simple. |

**16.05 AS BUILT, 2026-09-21, AND IT CORRECTS THIS ROW'S OWN PREMISE.** `Aelvor::RunState` {Begun, Detail, Dug, Eyes, Near, Watched} REPLACES the six members rather than mirroring them, so a field cannot be added to the run without being added to what is saved - the row asked for that and it is the whole reason it is one object. `GetRunState`/`SetRunState`, carried as `SectionKind::Run` in the 16.04 container: the table is variable-length, so it cost no container version, which is what the table was for.

**"A restored world diverges the moment anybody LOOKS" IS THE WRONG WAY ROUND.** Measured: looking is what HEALS it. A look fills `Watched` in; with `Options::Stream` the warden that acts on it runs on a DAY TURN. A restored world looked at six times before any day passes rebuilds its own run state and converges exactly. What diverges it is a day turn taken before the looks have caught up - and since a real host turns a day per look, that is every restore: measured at 0, 1, 2 and 3 silent day turns, **without-run DIVERGED in all four and with-run MATCHED in all four**.

**TWO WAYS OF MEASURING THIS PROVE NOTHING, AND I WROTE BOTH BEFORE FINDING OUT.** (1) Comparing a restore against ANOTHER RESTORE: both lack the run, both are wrong the same way, and they agree at every one of twelve day turns. The comparison must be against the CONTINUING SOURCE. (2) Comparing a world that looks against one that does not - they part because a look requests detail, which has nothing to do with the run. Only the run may differ between the two sides. Both wrong shapes are kept in `Checkpoint.TheRunTravelsWithTheWorldAndTheComparisonMustBeTheSource` as named arms, because a test that shows only the working case cannot tell the next reader why the two obvious experiments lie.

`SetRunState` REFUSES rather than clamps - a region past this map's `RegionCount()` means the state came from a world this one is not, and clamping would restore a run that looks right and watches somewhere that does not exist. Bounded through the same `Ways_` cache `LookAt` uses, so a state it accepts is one the warden can act on.

`Ways_` is not saved, and the row's reason was CHECKED rather than repeated: `WorldMap::Reset` does `++Replaced` (WorldMap.cpp:53, and the Serialize path at :109) and `RegionGraphCache` keys on `HashCombine(HashUInt64(Map.Revision()), ...)` (Regions.cpp:421), so a load invalidates it by construction. `SetRunState` therefore does nothing about it and says so.

Run.Checkpoint is 6 tests and 119 checks; 11 CTest entries under Run. and Replay. green in DEBUG with asserts on. The 128 Play+Stream cell and the `6d4b82134edfcbf6`/`02c94cdaf3367b43` pair this row names are NOT yet reproduced - that is a several-minute world and belongs with 16.06's `Adopt`, where the restore route is the thing under test.
| 16.06 | **Adopt: a world `Aelvor` did not generate.** `Aelvor::Adopt(const uint8*, usize)` — verify the container header against `Given_`, validate then apply the STATE section (16.03), `SetRunState`, `Begun_ = true`. `Begin()` after an `Adopt`, and `Adopt` into a world already begun, are each refused with a distinct named reason rather than a silent `false`. | `Run.AdoptCostsNoGeneration`: adopt a 256 checkpoint into an `Aelvor` that has only been CONSTRUCTED — `Ok`, state digest equals the save, `Played()` is the saved person, `Begun()` true, `Day()` advances `Now()` by 24, `Release()` then `TakeUp()` returns a person, `LookAt()` changes `Watching()`, then 60 day turns with the recorded looks reach the original's digest. A counter inside `PreHistory::Generate` reads 0 for the whole path; the test PRINTS milliseconds and asserts on none of them (ADR-0109). CONTROLS: the same assertions against a world that was only `LoadSnapshot`-ed must all fail in the measured way — `Begun=0`, `TakeUp->0`, `Detail=0`, `Watching` 0 regions, `Day()` 3456480 → 3456480; and a world adopted with `Detail_` left at 0 must diverge, because the warden is then free to release the world's floor. Second route, from "adversary": a world that was `Begin()`-ed and then loaded must reach the SAME digest over the same 30 turns — if the two restore routes disagree, one of them is wrong and the test says which. | Defect 10, and the prize of the phase: the one thing a 3,911-byte recorded stream cannot do is restore without re-running the world. Without this task the image route costs 785 ms at 128 against the stream route's 825 ms, and twenty-two megabytes buy a one-percent saving. `Begin()`'s only non-world outputs are `Detail_`, `Dug_` and `Begun_` — everything else it writes goes through `Ages.Generate`/`Ages.Run` into the image — which is what makes 16.05's struct complete TODAY. See RISKS. |
| 16.07 | **The store, and a write that cannot destroy the last good save.** `ICheckpointStore { bool Write(name, const uint8*, usize); bool Read(name, std::vector<uint8>&); std::vector<std::string> List(); }` declared in `VaelenRun` with no OS header at all, and two implementations outside the kernel: a stdio one for the headless tests and `Tools/Atlas` (`--save <path>`, `--load <path>`), and the Unreal one in 16.14. Writes go to a temporary name in the same directory, are flushed, then renamed into place. A ring of N checkpoints with a manifest naming each one's tick, digest and container version. Every failure is a returned result: a full disk, an unwritable path, a short read. | `Save.AtomicAndComplete`: write, read, `memcmp` 0; a file truncated at 10/50/90% is refused by name and never crashes; an unwritable path returns a failure. From "adversary" 16.08, the sharper arm: write into a directory too small for the image, assert `DiskFull`, that NO file exists at the final path, and that the previous checkpoint there is still byte-identical and still adopts to its recorded digest. CONTROL: replace temp-then-rename with a direct truncating open and re-run — the previous checkpoint is now half-overwritten and the second half of the test fails. | `ILogSink` and `AssertHandler` are the precedent: a kernel interface with a host implementation. The fence is real — `VaelenRun` is in `kernel_modules.txt` and `<filesystem>` is banned there with a stated reason — so the roadmap's "checkpoints on disk" is decided here in the open rather than discovered during implementation, and no `PURITY-ALLOW` exemption is added to reach it. Atomicity is not a format feature; it is the difference between a save system and a way to lose a game. |
| 16.08 | **Six causes, six answers — and pools matched by name.** `SnapshotResult` replaced by a diagnosis that names the measured causes apart: `NotASave`, `FormatTooNew`, `FormatTooOld`, `UnknownRequiredFlag`, `SeedMismatch`, `WorldShapeDiffers`, `TypeAdded`, `TypeRemoved`, `TypeResized`, `TypeRenamed`, `TypesReordered`, `PoolMissing`, `Truncated`, `Corrupt`, `Inconsistent` — carrying, for every type outcome, which types differ and how — plus a `Describe()` writing one human line. `LayoutMismatch` deleted. The load path reconciles the image's pools against the registry by `NameHash`, so a type at a different `TypeId` loads into the right pool. **A type the image lacks, or has and the world does not, is still a REFUSAL by name unless 16.09 has a registered upgrader for it.** The dead `Written != PoolCount` check (`Snapshot.cpp:164`) and the `PoolCount` equality gate (`:137`) go. | `Snapshot.SixCausesSixAnswers`: rebuild the six worlds of the probe — type added, type removed, reordered, field added, renamed, other seed — and assert six DISTINCT outcomes, each naming the type it is about; an unmodified load stays `Ok` with an empty difference list. `Snapshot.ReorderedTypesLoad`: a world that reorders two types loads a golden to `Ok` with every component in the correct pool and the same state digest as a world built in the image's own order. CONTROL: `grep -rn LayoutMismatch Source/ Tests/` prints nothing outside a retirement note; and a type whose `ElementSize` differs with no upgrader must return `TypeResized` naming the type and BOTH sizes, never a partial load. | Defects 6 and 7. Name-keying is from "migration" 16.05 and is what makes 16.09 worth building at all: a migration chain over an all-or-nothing image has nothing it can repair, and Phase 17 adding a component type would still brick every save. Its empty-pool partial load is explicitly NOT taken — one judge rejected it outright and was right: a world running with an empty pool where the save had data is the chimera of defect 2, blessed as a feature and reached by a routine refactor. |
| 16.09 | **Migrations keyed on the container, and a corpus with a forge.** `Migrate(std::vector<uint8>&, uint32 From) -> MigrateResult` applying a registered table of N→N+1 upgraders in strictly ascending order; a container from a FUTURE version is refused and never migrated. Per-type upgraders registered beside the type, keyed on `(NameHash, ElementSize) -> (NameHash, ElementSize)` with a `Why` string. `Tests/Save/Corpus/` holds one container of every version this project has shipped, each beside a `.digest` file, built from the ABSTRACT test world of `Tests/Sim/Test_Snapshot.cpp` and not from AELVOR. Plus `Tools/forge_save`: given the section table, synthesise an older-shaped container from a current one. | `Save.CorpusMigrates`: every file in the directory migrates to the current version and reaches the digest recorded beside it. `Save.MigrationChain`: three test-only upgraders run in order and exactly once; a table with a gap returns `NoUpgrader` at the gap rather than skipping it; a future version is refused untouched. `Migration.ForgeReproducesTheRealV3Golden`: a v3 image forged from a current save equals, byte for byte, the golden a real v3 build wrote at 16.01. CONTROLS: remove one upgrader and the test must fail naming the file it could no longer migrate; truncate a corpus file by one byte and it must fail as `Truncated`, not as a wrong digest; register the same upgrader twice and the "exactly once" assertion must fail. | Defect 8. The forge is from "migration" 16.08 and is the only honest answer any plan gave to "how do you test migrations for a format with no old files": a forge alone can be wrong in the same way the reader is wrong, a golden alone runs out the second time the schema moves, and a forge CHECKED against a real golden survives the disappearance of the last build that could write one. The corpus is not hypothetical — 16.10 bumps the container, so the version-1 files this phase writes are obsolete inside this phase. |
| 16.10 | **The host's declared world is checked against the save.** `Adopt` and `ReadCheckpoint` compare the container's Size, PreHistory, Years, Seed and the four wiring bools against the host's `Options` and refuse a mismatch by name, leaving the target untouched. `Player::StreamHeader` gains the same wiring bitfield, so a stream and a save agree on what world they describe. A DECISIONS entry naming `DiplomacySystem::Graph`/`GraphRegions` as the one cache keyed on region count alone, safe today only because the seed check forbids a different map. | `Save.WrongWorldIsRefused`: a 128 checkpoint offered to a host declaring `Size = 64` is refused by name, and the host's world still has its own tick, extents and digest; the same for a different `Years`, a different seed (now `SeedMismatch`), and each of the four wiring bools — six distinct results, none of them about component layout. CONTROL: take the extents check out and the 64-host call returns `Ok` again, which is today's measured behaviour; and the `Stream=false` arm must, with the check removed, produce a different state digest within ten day turns. | Defect 5, and it is not hypothetical downstream: `Aelvor::Header()` builds the `StreamHeader` from `Given_.Size`, so a walk recorded after such a load carries a header naming a world that does not exist and `Player::SameWorld` sends the replay to build the wrong one. `Options::Stream` is invisible to every existing guard because it declares no component type. |
| 16.11 | **Provenance: the stream and the host's rules travel with the save.** A STREAM section carrying `Door::Tape` and `Door::Start` (`StartRules`), and the full `Options` in HOST. Container version 2. `Door` gains a constructor that takes a tape back, so a loaded session keeps recording into the same stream rather than starting a new one. | `Save.ProvenanceReplays`: from the FILE ALONE, build a fresh `Aelvor` out of the container's own HOST section, replay the carried stream into it, and reach the same state, log and life digests as adopting the STATE section — two routes, one answer — then continue both by ten more recorded days and assert they stay equal. CONTROL, already calibrated: construct the fresh world from `Options{Play}` alone, which is what `Tools/Atlas --replay` does today, and the replay must come back `Wrong=4, WrongTakings=4` on both checked-in 128 walks. | Measured: the two checked-in 128 walks replay clean only with `Options::Stream = true`; without it all four takings pick a different person, because the daily detail cadence is a different world. `StreamHeader` is 24 bytes of Seed/Size/PreHistory/Years/Version (`Stream.h:110-118`) with no channel for the wiring, and `InputStream`'s own comment claims it is "everything a replay needs". A save format that claims to restore a PLAYED world cannot leave out the configuration that decides which world the records belong to — and a restored world that cannot be replayed has lost the Phase 14/15 audit route that is this project's whole determinism story. Four kilobytes against twenty-two megabytes. |
| 16.12 | **Every day is a save point.** (a) `Tests/Run/Test_SaveContinue.cpp` and a ctest entry `Run.SaveContinue`: a matrix of wirings {plain, Play+Stream, Play+Stream+Lively+Colony} × sizes {64, 128} × save points {early, mid, late}; for each cell, save at N, adopt into a fresh run, feed the SAME remaining records to M, and assert state, log and life digests equal against a straight run to M. (b) `Tools/Atlas --savefuzz <stream> --points K --seed S`: K seeded save points over a stream's day turns, plus restore-run-save-restore, three consecutive adopts into one dirty world, and the re-save fixed point. (c) A horizon in which the played person DIES mid-horizon, saved before, on and after the death, checked into `Tests/Run/Streams/` with its README line. | The tests ARE the deliverable; the controls are what make them evidence. `Run.SaveContinue.Control`, a sibling ctest entry, runs the identical matrix with the RUN section withheld and PASSES ONLY WHEN every `Stream=true` cell DIVERGES and every `Stream=false` cell still agrees. `--withhold-run` must report a non-zero mismatch count whose first mismatch day is at or after the stream's first look; `--corrupt-byte N` must be refused by a section digest rather than loaded. `Run.SaveDeath` asserts its own PRECONDITION — `PlayedAlive()` went false at a recorded tick and the stream carries a second `TakenUp` — and FAILS if no death occurred, so it can never pass vacuously. | From "determinism" 16.04-16.06, and this discipline is why one judge made it the spine: a green main test with a green control closes nothing. One cell at one wiring proved the divergence; only a matrix proves the fix. A death exercises `Release`, a fresh `TakeUp`, `NearDetail`'s give-back path (`Aelvor.cpp:694`) and a recorded `TakenUp` — the exact machinery `Near_` feeds, and the most likely place for a fourth piece of run state to be hiding. 15.03's history says what happens when a give-back path is never exercised: saturation at `MaxWanted`, every Move refused `TooFar`, silently, for the rest of that world's life. |
| 16.13 | **A state digest that does not write the world.** A `HashingWriter` implementing `IArchive` that folds bytes as they are produced and allocates nothing, and `ComputeStateDigest` rewritten onto it. The returned `Hash64` must be BIT-IDENTICAL to today's for every world: this is a performance task with a zero-change contract. Lands LAST of the headless tasks, after 16.12 exists to run it against both implementations. | `Sim.DigestParity`: over the whole 16.12 matrix, old and new return the same `Hash64`, and the new path's allocation counter reads 0 bytes for the image; the test prints the bytes no longer allocated (22 MB at 128, 61 MB at 256). CONTROL: advance one world by a single tick and require BOTH implementations to move to the same NEW value — a hasher returning a constant, or ignoring a section, passes a parity test that only ever compares two readings of the same world. | Measured: `ComputeStateDigest` IS a full cold `SaveSnapshot` — 20.5 ms/11 MB at 64, 75.6 ms/22 MB at 128, 192.0 ms/61 MB at 256, allocated and thrown away to read eight bytes off the end. `Aelvor::StateDigest()` is that call, and it is what every gate and every per-day check uses; 16.12's matrix and fuzzer will call it tens of thousands of times. Both judges flagged the hazard, and it is real: this re-implements the number every frozen gate asserts, inside the phase whose first rule is that the number must not move. Hence last, and hence the both-must-move arm. |
| 16.14 | **The engine half, and the sitting. OUTSIDE THE GATE.** `Vaelen.Save <name>` and `Vaelen.Load <name>` in `VaelenGame`, an `ICheckpointStore` over Unreal's `IFileManager` under the project's saved directory, and `FVaelenHeld` retaking its presentation state after an adopt. One sitting on the owner's Windows machine: play, save, CLOSE the editor, reopen, load, keep playing, write the stream out. STATUS says `UNVERIFIED (engine)` until that sitting happens. | `Run.Gate.Saved`: the checkpoint written in the engine is carried here, `VaelenAtlas --load` adopts it to the same state digest, tick and played person the engine printed, and then replays the checkpoint's own STREAM section to the digest the engine printed at the end of the sitting. CONTROL: offer it the same checkpoint with its RUN section zeroed and the continuation must diverge — 16.05's control, run at the end on real bytes from a real machine. | A checkpoint that has never survived a process exit has been round-tripped in memory, not tested, and Phase 15 closed on a sitting that taught it the instruction rather than the code was wrong. But Phase 15 was also gated for days by that machine, and a gate that cannot be run here is not a gate. So this is the phase's closing confirmation and not one of its clauses. Everything in 16.01-16.13 must be green headless before the sitting is scheduled, not during it. |

### The gate

Every clause is runnable on this machine, and each names the command that decides
it. Any one missing and Phase 16 stays open.

- **(a)** `Tools/run_gates.sh linux-clang-debug` prints `GATES-DONE 0 failing
  (of 17)`: the eleven it ran before 16.01 plus `Run.Golden`, `Replay.Played`,
  `Replay.Walked`, `Replay.Lived`, `Run.Gate` and `Run.Gate.Lived`. Seventeen
  and not the sixteen this section first said: `Run.Golden` was added on top of
  the planning's five, because the corpus is the instrument the rest of the
  phase is measured by and a gate list that omits it measures nothing about it.
  `Tools/run_gates.sh linux-clang-debug --self-test` prints `SELF-TEST OK`.
- **(b)** Not one frozen digest moved.
  `git diff 867a129..HEAD -- Tests/ Source/ Tools/ Docs/ROADMAP.md | grep -E '^-.*\b[0-9a-f]{16}\b'`
  prints nothing. Checked by diff, not asserted by eye.
- **(c)** `ctest --preset linux-clang-debug` and `ctest --preset linux-gcc-release`
  each report 100% passed, and `ctest -R '^Kernel\.(Purity|WorldWiring|UiFence)$'`
  is green — no kernel module includes `<fstream>` or `<filesystem>`, and no
  `PURITY-ALLOW` exemption was added to reach this gate.
- **(d)** `ctest -R '^Run\.SaveContinue$'` passes over at least 18 cells, each
  asserting state, log and life digests equal between "saved at N, adopted, run
  to M" and "run straight to M". The test prints the cell count and every digest.
- **(e)** THE CONTROL FIRES. `ctest -R '^Run\.SaveContinue\.Control$'` passes,
  where passing means every `Stream=true` cell DIVERGES with the RUN section
  withheld and every `Stream=false` cell still agrees, the 128 Play+Stream cell
  reproducing `6d4b82134edfcbf6` against `02c94cdaf3367b43`.
- **(f)** `ctest -R '^Run\.SaveDeath$'` passes and prints the tick of the death
  and of the second `TakenUp`; it fails, loudly, if no death occurred.
- **(g)** `Tools/Atlas --savefuzz <stream> --points 64 --seed 1` reports
  `0 mismatches` on each of the three streams in `Tests/Run/Streams/`; the same
  command with `--withhold-run` reports a non-zero count and names the first
  mismatching day, and with `--corrupt-byte` reports a section-digest refusal
  rather than a load.
- **(h)** `ctest -R '^Run\.AdoptCostsNoGeneration$'` passes: a 256 checkpoint
  adopted into a CONSTRUCTED-only `Run::Aelvor` reaches the saved digest, is
  playable (`Day()` advances, `TakeUp` returns a person, `LookAt` changes
  `Watching()`), and calls `PreHistory::Generate` zero times, proven by a call
  counter and never by a stopwatch (ADR-0109). Milliseconds are printed and
  nothing asserts on them.
- **(i)** Every refusal names itself and leaves the target bit-identical.
  `ctest -R '^Snapshot\.AllOrNothing$|^Snapshot\.SixCausesSixAnswers$|^Save\.WrongWorldIsRefused$|^Save\.MigrationChain$'`
  passes; `Snapshot.AllOrNothing` prints the target's digest before and after
  every refusal and they are equal; and running it with 16.03's validate pass
  disabled makes at least one line read CLOBBERED.
- **(j)** `ctest -R '^Golden\.V3RoundTrips$|^Save\.CorpusMigrates$|^Migration\.ForgeReproducesTheRealV3Golden$'`
  passes on `linux-clang-debug`, `linux-gcc-release` AND on the existing Windows
  MSVC leg of `kernel-ci.yml` (`ctest --preset windows-msvc-debug -E Shuffled`)
  and the macOS leg. That leg exists; nothing in this planning has ever run
  there, and the corpus is the first thing that makes the cross-compiler claim a
  measurement rather than an argument from `static_assert`.
- **(k)** `ctest -R '^Save\.AtomicAndComplete$|^Atlas\.SaveThenLoadFromDisk$'`
  passes: Atlas saves, a second PROCESS loads and runs ten days to the
  one-process digest; a write into a directory too small for the image reports
  `DiskFull` with no file at the final path, and the previous checkpoint there
  still reads and still adopts to its recorded digest.
- **(l)** `ctest -R '^Save\.ProvenanceReplays$'` passes: the STREAM section
  replays into a world built ONLY from the container's HOST section and reaches
  the checkpoint's state, log and life digests with `Wrong = 0`.
- **(m)** `ctest -R '^Save\.ContainerRoundTrip$|^Save\.SectionTableIsHonest$|^Save\.UnknownRequiredFlagIsRefused$'`
  passes: STATE `memcmp`s to a bare `SaveSnapshot`, section lengths sum to the
  container length with delta 0, and the 180-offset resealed-flip sweep reports
  0 silent acceptances while the same sweep with section digests off reports at
  least 100. Both numbers printed.
- **(n)** `ctest -R '^Sim\.DigestParity$'` passes: identical `Hash64` old and
  new for every world in the 16.12 matrix, both moving together when one world
  advances a tick, 0 bytes allocated for the image.
- **(o)** No fake done. Every STATUS comment this phase touched says VALIDATED
  only where the code was built AND run here; 16.14 reads `UNVERIFIED (engine)`
  until the owner's machine produces its line; and 16.12's log finding — a
  divergence found, or a null result with the horizons and wirings it was
  searched over — is written into DECISIONS whichever way it came out.

### Out of scope for Phase 16, and where it belongs

- **Log truncation.** All four plans measured the same thing — the log is 83.6%
  of the image at 256, 90.9% at 128, 93.5% at 64 and 90.6-95.6% of a 500-year
  world; it grows 49.2/151.0/285.5 kB per simulated year without bound; only the
  last simulated year is ever read by a running system, and that tail is 0.65%
  of the events at 128 and 0.80% at 256, a 10.4× / 5.9× image reduction. All four
  are also right that the evidence it can be dropped is TWO worlds run 400 day
  turns each with the log cleared, against five systems that demonstrably read
  `W.Log().All()` inside `Tick` — `ProductionSystem` (`Production.cpp:91`),
  `MarketSystem` (`Markets.cpp:138`), `DecaySystem` (`Decay.cpp:56`),
  `FactionSystem` (`Factions.cpp:284`), `RegardSystem` (`Regard.cpp:95`).
  Truncating also moves every frozen digest, because the log is inside the image
  the trailer hashes. **Phase 16 truncates nothing, compresses nothing, and
  builds no truncation machinery — not even switched off.** It records the
  numbers and the question. The SEARCH for a divergence, with the injected-event
  control that proves the search can see the log at all, is folded into 16.12's
  fuzzer as a reported measurement, not a shipped policy. The decision belongs to
  **Phase 18 STRESS TEST**, which is where a 500-year AELVOR 256 — a 182 MB save
  that takes 52 s to generate — is exercised at all, and to a phase that OPENS
  with it rather than one that stumbles into it while shipping a save.
- **Severing `ComputeStateDigest` from the image trailer, and any project-wide
  re-freeze.** "adversary" 16.04 and "migration" 16.02 both proposed it, by
  different means. Nothing in this phase requires it once the container carries
  the new state, and doing it mid-phase destroys the only instrument that could
  catch this phase's own regressions: afterwards the eleven gates assert new
  numbers produced by the same changed code. If it is ever wanted, it belongs to
  a later phase that opens with it, and migration's version/flags substitution is
  the cheaper of the two forms.
- **A per-field layout digest** (defect 9). "determinism" 16.09 proposed deriving
  it "from `PlainData.h`'s existing traits"; `PlainDataTraits` carries exactly one
  member, `static constexpr bool NoPadding`, and there is no member enumeration
  anywhere to derive offsets or type tags from. A real fix needs a member-listing
  macro applied across ~55 component types and every map layer and the `Event`
  payload set. That is its own job and belongs with **Phase 19 MODDING**'s stable
  ids and APIs. Until then the hole stands and this phase says so: a same-size
  field reorder or retype is not caught by anything 16.08 adds.
- **Loading a save into a world generated differently.** Explicitly refused by
  16.10. The `DiplomacySystem` region-count cache is recorded as the thing that
  breaks first if that refusal is ever loosened.
- **A save browser, thumbnails, named slots, autosave UI.** `Phase 17 DEBUG
  TOOLS` for the inspector side, `Phase 20 POLISH` for the player-facing side.
  16.04's section table and manifest are what makes listing saves without
  generating a world possible; this phase stops there.
- **Compression.** If it comes, it comes as an optional section under 16.04's
  may-ignore flag rule, in a later phase.

### Open questions only the owner can answer

1. **Where do saves live on the Windows machine, and what are the verbs called?**
   16.14 assumes `Vaelen.Save <name>` / `Vaelen.Load <name>` under the project's
   saved directory. If the sitting should instead use a key like F9 (as
   `Vaelen.Stream.Write` does), say so before 16.07 fixes the store's naming.
2. **Does a save carry its recorded tape by default?** 16.11 says yes: four
   kilobytes against twenty-two megabytes, and without it a restored world cannot
   be audited or replayed. If saves are ever to be shared, that tape is a record
   of what somebody did, and that is the owner's call and not the code's.
3. **How many checkpoints in the ring, and is there an autosave?** 16.07 builds
   the ring; nothing decides N, and nothing decides whether the game writes one
   on its own. If it does, the cadence must be counted in DAY TURNS and never in
   wall clock (ADR-0109).
4. **Is a save ever to be loadable into a world generated differently?** Today
   the seed check forbids it, and that is the only thing making
   `DiplomacySystem`'s region-count cache safe. Answering "yes, one day" makes
   fixing that cache a Phase 17 task rather than a recorded note.
5. **What is an acceptable save size at the far end?** A 500-year AELVOR 256 is
   182 MB today and unbounded thereafter. Phase 16 ships that and calls it
   correct. If it is not acceptable, Phase 18 needs to open with the log question
   rather than inherit it.
6. **Does the MSVC leg get the corpus?** Clause (j) says yes, and the leg exists
   (`kernel-ci.yml`, `windows-2022`, `ctest --preset windows-msvc-debug -E Shuffled`).
   It adds a few seconds and it is the only cross-compiler witness the format
   will ever have. Confirm it should not be excluded.

### What the probes could NOT settle, and the risks that follow

- **A death across a save was never reached.** The longest horizon measured is 60
  day turns at the fullest wiring with the played person alive throughout. A
  death exercises `Release`, a fresh `TakeUp`, `NearDetail`'s give-back path and
  a recorded `TakenUp` — the machinery `Near_` feeds — and is the most likely
  place for a fourth piece of run state to be hiding. 16.12(c) asks for one and
  asserts its own precondition; if no workable seed reaches a death in a
  workable horizon, the task must SAY so in its STATUS line rather than let a
  gate that never met a death imply it did.
- **`Detail_` and `Dug_` could not be varied in isolation.** They are private and
  only `Begin()` writes them, so the argument for saving them is from the code
  (`Reside` reads `Detail_` at `:532` and `:566`) and from the never-`Begin()`-ed
  run where both read 0. Confirming needs a setter, which is a task and not a
  probe. This is why 16.05 copies them rather than deriving them.
- **Everything measured is gcc on Linux.** `sizeof(Event) == 112`, the
  `static_assert`s on padding, and `WorldGenConfig` being written as raw bytes
  through one `SerializeBytes` call are an ARGUMENT that MSVC and AppleClang
  agree, not a run. Clause (j) is the only thing that turns it into a
  measurement, which is why the corpus is small enough to live in git.
- **16.06's `Adopt` sets `Begun_` without `Begin()` running.** That is provably
  complete today — `Begin()`'s only non-world outputs are `Detail_`, `Dug_` and
  `Begun_` — and that completeness is a property of today's `Begin()`, not of the
  design. 16.05's single struct filled by both paths is the guard; if a later
  phase adds a line to `Begin()` that writes a member and not the struct, a
  restored world diverges silently again and 16.12's matrix is the only
  instrument that would catch it.
- **The two probes disagree by 27,565 bytes on the same wiring** (21,980,402
  against 22,007,967 at AELVOR 128, Play+Stream, 20 played days). The walks
  differed by a few hundred logged events. Same world, same conclusion, and
  neither number should be quoted as a constant.
- **The count of frozen digest literals is not a constant either.** The planners
  quoted 249 and 252; a bare `[0-9a-f]{16}` grep over `Tests/`, `Source/` and
  `Docs/ROADMAP.md` here returns 178. Clause (b) checks the DIFF, not a count, for
  that reason.
- **Fourteen tasks is above this project's range**, and 16.12 is the biggest of
  them. If the phase has to shed weight, 16.13 goes first (it is a performance
  task with a zero-change contract) and 16.09's forge second (the corpus alone
  still migrates, it just stops being a witness once the schema moves twice).
  16.01 through 16.06 are not sheddable: they are the save.

Next: 16.01, the goldens and the gate list, because both are one-way doors and
neither can be recovered once another task has landed.
