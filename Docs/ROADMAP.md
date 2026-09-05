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
   and `CMakePresets.json`: Linux clang and gcc in Debug and RelWithDebInfo, Windows
   MSVC Debug, macOS AppleClang Debug, including the `Kernel.Purity` entry.
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
| 00 | FOUNDATION | Engine-agnostic kernel skeleton with dual build, core primitives (types, ids, hashing, random streams, logging, assertions, versions), test harness, purity check, CI and documentation. | INCOMPLETE (00.01-00.04 VALIDATED headless; 00.05 in progress; engine build UNVERIFIED) |
| 01 | CORE SIMULATION | Entities, components, systems and a deterministic tick scheduler; simulation clock and calendar; event bus and event log; snapshot interfaces; deterministic replay; simulation LOD 0-4 hooks. | PLANNED |
| 02 | WORLD | Procedural world of AELVOR derived from the seed: regions, tiles, terrain, climate, hydrology, resource deposits. | PLANNED |
| 03 | HISTORY | Simulated pre-history that everything later inherits: eras, cultures, languages, religions, migrations, the historical record. | PLANNED |
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
running them (section 8). Test counts are from `VaelenCoreTests --list`.

### 00.01 Project architecture - VALIDATED (headless) / UNVERIFIED (engine)

Deliverables present:

- Unreal project: `Vaelen.uproject` (UE 5.6; modules `VaelenCore` PreDefault, `Vaelen`
  Default), `Source/Vaelen.Target.cs`, `Source/VaelenEditor.Target.cs`,
  `Source/VaelenCore/VaelenCore.Build.cs`, `Source/Vaelen/Vaelen.Build.cs`,
  `Config/Default{Engine,Game,Editor,Input}.ini`.
- Engine bridge module `Source/Vaelen`: `FVaelenModule` installs `FVaelenLogSink`
  (kernel records to `UE_LOG`) and the kernel assertion handler.
- Headless build: `/CMakeLists.txt` (options, warning set, `-fno-exceptions -fno-rtti`,
  MSVC equivalents, defines), `Source/VaelenCore/CMakeLists.txt` (explicit source list),
  `Tests/CMakeLists.txt`, `Tests/Core/CMakeLists.txt`, `CMakePresets.json`.
- CI: `.github/workflows/kernel-ci.yml` (Linux clang/gcc x Debug/RelWithDebInfo, Windows
  MSVC, macOS).
- Style: `.clang-format`, `.editorconfig`, `.gitignore`.
- Layering and module plan: `Docs/ARCHITECTURE.md` sections 1-4.

Verified: headless configure, build and `ctest` with clang++ and g++ in Debug and
RelWithDebInfo (section 8). Not verified: any UBT build; every engine-facing file is
labelled UNVERIFIED and has never been compiled in this repository. Known gap: the
checked-in `.clang-format` does not reproduce the checked-in code
(`Docs/CONVENTIONS.md` section 2).

### 00.02 Core primitives: ids, hash, random - VALIDATED

| Piece | Files | Tests (suite: count) |
|---|---|---|
| Fixed-width types, helpers | `CoreTypes.h` | indirectly by every suite |
| Versions | `Version.h`, `Version.cpp` | Version: 7 |
| Hashing (FNV-1a 64, `Mix64`, `HashCombine`, `_vhash`) | `Hash.h` | Hash: 15 |
| Random streams (xoshiro256**, SplitMix64, `Derive`/`Fork`, `Jump`, draws) | `Random.h`, `Random.cpp` | Random: 23 |
| Persistent ids and allocator | `Ids.h`, `Ids.cpp` | Ids: 17 |

All headers carry `STATUS: VALIDATED (Phase 00)`. Tests include known answers against
independent reference implementations, frozen regression values for `HashCombine`,
determinism and state round trips, edge cases (full 64-bit ranges, serial exhaustion) and
assertion paths. Decisions: ADR-0003, ADR-0004, ADR-0007. Known discrepancies: `Random.h`
names Lemire's method where the code uses bitmask-with-rejection; `Ids.cpp`,
`Random.cpp`, `Version.cpp` have no STATUS line.

### 00.03 Logging and assertions - VALIDATED

| Piece | Files | Tests (suite: count) |
|---|---|---|
| Categories, levels, macros, sinks, thresholds | `Log.h`, `Log.cpp` | Log: 20 (includes an 8-thread dispatch test) |
| `VAELEN_CHECK/CHECKF/VERIFY/ENSURE/UNREACHABLE`, pluggable handler, failure counter | `Assert.h`, `Assert.cpp` | Assert: 28 with assertions enabled, 5 with them disabled |

Decisions: ADR-0002, ADR-0005. Known discrepancies: `Log.h` says the headless build
installs a stdout sink, but the runner does so only with `--verbose`; `Assert.cpp` has no
STATUS line; the assertions-disabled configuration is not part of CI (see 00.04).

### 00.04 Test harness and purity - VALIDATED

- Harness `Tests/Harness/VaelenTest.h`, runner `Tests/Harness/TestMain.cpp`, self-test
  `Tests/Core/Test_Harness.cpp` (Harness: 2). One CTest entry per `Test_<Suite>.cpp`.
- Purity checker `Tools/check_kernel_purity.py` (rules R0-R7, exemptions, `--self-test`:
  32 checks, 0 failed), module list `Tools/kernel_modules.txt`, CTest entry
  `Kernel.Purity` (12 files, 0 violations).

Decisions: ADR-0006, ADR-0008. Known gaps: `Test_Harness.cpp` is not guarded by
`VAELEN_ASSERTS_ENABLED`, so a build with `-DVAELEN_ENABLE_ASSERTS=OFF` fails one test
(observed: 89 run, 88 passed); no CI preset builds that configuration. The harness
comment says `ScopedAssertCapture` restores the previous handler; it installs the default
one.

### 00.05 Foundation validation and docs - INCOMPLETE

Done:

- `Docs/ARCHITECTURE.md` (layers, dual build, module map, primitives, tests, pipeline,
  known discrepancies) and `Docs/CONVENTIONS.md` (language, formatting, naming, purity,
  determinism, logging, assertions, labels, tests, process).
- `Docs/DECISIONS.md` (ADR-0001 to ADR-0008) and this roadmap.
- Cross-check of code against comments and documents; the discrepancies are listed in
  `Docs/ARCHITECTURE.md` section 11, in the ADR consequences, and in section 4 above.

Open:

- No `README.md` (referenced by `Config/DefaultEditor.ini`).
- The discrepancies listed above are reported, not fixed (`Random.h` wording, `Log.h`
  wording, `VaelenTest.h` wording, missing STATUS lines in four kernel `.cpp` files,
  `Test_Harness.cpp` under assertions off, `.clang-format` mismatch, the CI workflow
  comment that claims an engine-backed runner).
- Windows, macOS and RelWithDebInfo CI legs are defined but their results have not been
  observed from this repository; the engine build has no pipeline at all.
- Final review and the closing progress report of Phase 00.

Phase 00 against the exit criteria of section 2: (1) green on the Linux legs as run
locally, other legs unobserved; (2) met for Random, Ids, Hash; (3) met: no INCOMPLETE file
exists, engine files are UNVERIFIED; (4) unit, deterministic and edge-case present,
integration limited to cross-primitive use, long-duration not applicable yet; (5) docs
present except README, closing report pending.

## 5. Phase 01 - CORE SIMULATION: task breakdown (PLANNED)

Nothing in this section exists. Planned module: `VaelenSim` (kernel), directory
`Source/VaelenSim` with `Public/Vaelen/Sim/`, listed in `Tools/kernel_modules.txt`,
`/CMakeLists.txt`, `Vaelen.uproject` and both targets (`Docs/ARCHITECTURE.md` section 3.3,
rule 6). Tests in `Tests/Sim/Test_<Suite>.cpp` building `VaelenSimTests`. Each task ends
VALIDATED only with unit, deterministic and edge-case tests on both compilers; 01.07 and
01.08 supply the integration and long-duration categories for the whole phase. Every
design choice below that survives implementation gets an ADR (planned numbers 0009+).

### 01.01 Entity handles and registry

- Goal: a runtime handle for dense storage plus the mapping to `PersistentId`
  (`IdKind::Entity`), as anticipated in `Ids.h` ("runtime slot handles with generation
  counters ... Phase 01").
- Planned: `EntityHandle{uint32 Slot; uint32 Generation}`; `EntityRegistry` with
  `Create() -> {PersistentId, EntityHandle}` (id from an `IdAllocator` owned by the
  world state), `Destroy(handle)` bumping the generation, `IsAlive`, `Resolve(id) ->
  handle`, `IdOf(handle)`; destroyed entities keep their `PersistentId` forever; slot
  reuse policy fixed and documented (free list in deterministic order).
- Tests: create/destroy/reuse with generation check, stale handle rejected, id <-> handle
  round trip, iteration order stable and independent of destruction history where
  specified, registry state round trip, determinism over a scripted create/destroy
  sequence, edge cases (empty registry, destroy twice, resolve of a never-created id).
- Exit: ADR for handle layout and slot reuse; VALIDATED.

### 01.02 Component storage

- Goal: typed, dense, deterministic storage of plain data per entity.
- Planned: component type ids without RTTI (compile-time name hash via `_vhash` or an
  explicit registry); `ComponentPool<T>` (structure of arrays or array of structs,
  decided by ADR) with sparse slot index, `Add/Remove/Get/Has`; components are trivially
  copyable so a pool can be snapshotted as bytes; iteration in slot order; removal by
  swap-and-pop with index fix-up.
- Tests: add/remove/has/get, iteration order after arbitrary removals, byte-snapshot
  round trip, growth to large counts, no dependence on allocation addresses (two pools
  built in different orders compare equal after the same operations).
- Exit: ADR for layout and type-id scheme; VALIDATED.

### 01.03 Systems and tick scheduler

- Goal: systems run in a fixed, explicit order with their own random stream and LOD
  hooks.
- Planned: `ISystem` (name, declared dependencies, `Tick(context)`), `Scheduler` that
  orders systems by explicit dependencies with a deterministic tie-break (name hash),
  hands each system `WorldStream.Derive(name)` (ADR-0003) and a `TickContext` (tick,
  clock, registry, event bus); per-system tick period per LOD level 0-4 (LOD 0 every
  tick, higher levels less often); registration order must not change results.
- Tests: two registration orders give the same execution order and identical world
  state; adding a system leaves other systems' streams untouched; dependency cycle
  detected and reported; LOD schedule counts per level; determinism across two worlds.
- Exit: ADR for ordering and LOD semantics; VALIDATED.

### 01.04 Simulation clock and calendar

- Goal: time as world state, never wall-clock.
- Planned: `SimTick` (`uint64`), fixed tick duration, `Calendar` converting tick to
  year / season / month / day / hour of AELVOR (constants data-defined, defaults in
  code), no use of `<chrono>` or `time()` (purity R1/R4 already forbid them).
- Tests: tick <-> calendar round trips, boundaries (tick 0, year rollover, `uint64`
  limits), leap or irregular rules if adopted, constexpr evaluation, determinism.
- Exit: VALIDATED.

### 01.05 Event bus and event log

- Goal: the historical record and the replay input.
- Planned: `Event{PersistentId Id (IdKind::Event); SimTick Tick; uint64 TypeHash;
  fixed-size payload or byte span}`; `EventBus` with publish/subscribe delivered in
  publish order within a tick; append-only `EventLog` with a running hash for
  determinism comparison; serialisation as bytes.
- Tests: delivery order, subscription filtering, log append-only and hash stable, log
  byte round trip, ids monotonic, edge cases (no subscribers, publish during dispatch,
  empty payload).
- Exit: ADR for event layout and log format; VALIDATED.

### 01.06 Persistence interfaces and snapshot

- Goal: capture and restore the whole simulation state in memory; on-disk files come in
  Phase 16.
- Planned: `IArchive` (write/read bytes, versioned by `VAELEN_SAVE_FORMAT_VERSION`),
  `Snapshot` covering registry, component pools, `IdAllocator::State`,
  `RandomStreamState` of every stream, clock, event-log position; version mismatch
  rejected explicitly; snapshots of identical runs are byte-identical.
- Tests: snapshot -> restore -> continue equals the uninterrupted run (state and event
  hash), snapshot equality across two identical worlds, wrong version rejected, empty
  world snapshot, large world snapshot.
- Exit: ADR for archive and snapshot format; VALIDATED.

### 01.07 Deterministic replay test

- Goal: prove the determinism rule end to end.
- Planned: record seed plus the command/input stream; replay gives an identical event
  log hash and snapshot; checkpoint at tick k, restore, run to tick N, compare with the
  uninterrupted run; the same hashes printed by the clang and gcc binaries and compared
  by a CTest script.
- Tests: this task is a test suite (`Test_Replay.cpp`); it is the phase's deterministic
  and integration gate.
- Exit: VALIDATED on both compilers; hashes recorded in the closing report.

### 01.08 Abstract mini-world end-to-end test

- Goal: exercise every Phase 01 piece together over a long run, without any AELVOR
  content.
- Planned: a toy world with three or four systems (a population counter, a resource
  pool, a random event producer, an LOD-sensitive system), run for a long duration
  (order of 100,000 ticks) with periodic snapshots and replays; invariants checked every
  N ticks; a performance baseline logged, not asserted.
- Tests: this task is a test suite (`Test_MiniWorld.cpp`); it is the phase's
  long-duration gate.
- Exit: VALIDATED; Phase 01 closed against section 2.

## 6. Phases 02-20: notes

No task breakdown exists yet for Phases 02-20; each is broken down when the previous
phase closes. Fixed points already in the code: `IdKind` values for Region, Tile, River,
ResourceDeposit (Phase 02), Culture, Language, Religion, Person, Family, Organization
(Phases 03-05), Item, Building, Settlement, Market, Route (Phases 06, 09), Polity, Law,
Army, War (Phases 07-08), Document, Map (Phase 12); `VAELEN_SAVE_FORMAT_VERSION`
(Phase 16); `Config/DefaultEngine.ini` and `DefaultInput.ini` note that the game engine
class and Enhanced Input mappings arrive in Phase 10.

## 7. Current BUILD STATUS

```
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
VAELEN BUILD STATUS
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

PHASE       : 00 — FOUNDATION
TASK        : 00.05 — FOUNDATION VALIDATION & DOCS
STATUS      : IN PROGRESS

PROGRESS
███████████████████░░░░░ 80%

CURRENTLY
→ Architecture decision records
→ Roadmap
→ Cross-check of code, comments and documents

COMPLETED
✓ 00.01 Project architecture (headless VALIDATED, engine UNVERIFIED)
✓ 00.02 Core primitives: ids, hash, random
✓ 00.03 Logging and assertions
✓ 00.04 Test harness and purity check

NEXT
→ README.md
→ Fix the reported discrepancies
→ Closing report of Phase 00, then 01.01

FILES
+ Docs/DECISIONS.md
+ Docs/ROADMAP.md

TESTS
✓ 112/112 clang++ 18 Debug, g++ 13 Debug
✓ 112/112 clang++ 18 RelWithDebInfo, g++ 13 RelWithDebInfo
✓ ctest 8/8 (incl. Kernel.Purity) in all four configurations
⚠ 88/89 with -DVAELEN_ENABLE_ASSERTS=OFF (not a CI configuration)

BLOCKERS
None
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
```

## 8. Verification record

Commands run on 2026-09-05 (Linux, clang++ 18.1.3, g++ 13.3.0, CMake 3.28.3, Ninja,
Python 3.11.15), each in a private build directory:

```
cmake -S . -B out/build/agent-doc-adr-clang -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=clang++
cmake --build out/build/agent-doc-adr-clang
ctest --test-dir out/build/agent-doc-adr-clang --output-on-failure   # 8/8 passed
out/build/agent-doc-adr-clang/Tests/Core/VaelenCoreTests             # 112 run, 112 passed, 21785 checks
# same with -DCMAKE_CXX_COMPILER=g++ into agent-doc-adr-gcc            # 8/8, 112/112
# same with -DCMAKE_BUILD_TYPE=RelWithDebInfo (clang++, g++)           # 8/8, 112/112 each
# same with -DVAELEN_ENABLE_ASSERTS=OFF (clang++, Debug)               # 89 run, 88 passed, 1 failed
python3 Tools/check_kernel_purity.py --self-test                       # 32 checks, 0 failed
python3 Tools/check_kernel_purity.py --root . --verbose                # 12 files, 0 violations
```

Not run: any UBT/engine build, the Windows and macOS CI legs.
