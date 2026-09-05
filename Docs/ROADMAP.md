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
running them (section 8). Test counts are from `VaelenCoreTests --list`. This numbering
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

Verified: headless configure, build and `ctest` for all six Linux presets (section 8),
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

## 5. Phase 01 - CORE SIMULATION: task breakdown (PLANNED)

Nothing in this section exists. Planned module: `VaelenSim` (kernel), directory
`Source/VaelenSim` with `Public/Vaelen/Sim/`, listed in `Tools/kernel_modules.txt`,
`/CMakeLists.txt`, `Vaelen.uproject` and both targets (`Docs/ARCHITECTURE.md` section 3.3,
rule 6). Tests in `Tests/Sim/Test_<Suite>.cpp` building `VaelenSimTests`. Each task ends
VALIDATED only with unit, deterministic and edge-case tests on both compilers; 01.07 and
01.08 supply the integration and long-duration categories for the whole phase. Every
design choice below that survives implementation gets an ADR (planned numbers 0010+).

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
TASK        : 00.05 — FOUNDATION VALIDATION
STATUS      : VALIDATED (headless) / UNVERIFIED (engine)

PROGRESS
████████████████████████ 100%

CURRENTLY
→ Phase 00 closed on the headless side
→ Engine-side files await the first UE 5.6 build (Docs/ARCHITECTURE.md § 8)

COMPLETED
✓ 00.01 Project architecture (UE5 project, dual build, CMake presets, CI)
✓ 00.02 Core primitives: ids, hash, random
✓ 00.03 Logging and assertions
✓ 00.04 Test harness and kernel purity check
✓ 00.05 Adversarial review (189 findings triaged, 60+ fixes), docs

NEXT
→ 01.01 Entity handles and registry (Phase 01 — CORE SIMULATION)
→ First engine build on a machine with UE 5.6 (validates the UNVERIFIED files)

FILES
Source/VaelenCore (7 headers, 5 sources, 1 UE module file)
Tests/Harness (2), Tests/Core (9 suites), Tools/check_kernel_purity.py
Docs/{ARCHITECTURE,CONVENTIONS,DECISIONS,ROADMAP,STATUS}.md, README.md

TESTS
✓ 133/133 tests, 21 914 checks: clang++ 18 and g++ 13, Debug and RelWithDebInfo
✓ 108/108 with assertions off (linux-*-noasserts), both compilers
✓ ctest 14/14 in all six Linux presets (incl. Kernel.Purity, PuritySelfTest 36 checks)
✓ clang-format 18: 0 drift on Source/VaelenCore and Tests
✓ GitHub CI run 5: Windows MSVC 19.44 and macOS 15 AppleClang, 14/14 each; all 9 jobs green
⚠ Unreal Build Tool (UE 5.6): never run, engine-facing files UNVERIFIED

BLOCKERS
None
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
```

## 8. Verification record

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
