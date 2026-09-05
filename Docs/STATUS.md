# VAELEN — Build status

STATUS: VALIDATED for the state it reports, checked on 2026-09-05 against the sources on
branch `claude/vaelen-master-prompt-aw7zqj` after the Phase 00 review pass. This is the
living status document: it is refreshed at the end of every task (section "How to
refresh").

## BUILD STATUS

```
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
VAELEN BUILD STATUS
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

PHASE       : 02 — WORLD
TASK        : 02.05 — HYDROLOGY
STATUS      : VALIDATED (headless) / UNVERIFIED (engine)

PROGRESS
███████████████░░░░░░░░░ 62%

CURRENTLY
→ 02.05 closed: priority-flood+epsilon depression filling, D8 flow, accumulation,
  sediment-filled shallow basins vs deep lakes, rivers and lakes as entities

COMPLETED
✓ Phase 00 — FOUNDATION ; Phase 01 — CORE SIMULATION (CI 9/9 on every run)
✓ 02.01 Grid and layers (CI 18) ; 02.02 Fixed point and noise (CI 19)
✓ 02.03 Elevation and coastline (CI 20) ; 02.04 Climate and biomes (CI 21)
✓ 02.05 Hydrology (5 tests: Hydrology)

NEXT
→ 02.06 Regions (watershed-seeded partition of the land, region entities and adjacency)
→ 02.07 Deposits ; 02.08 Phase 02 gate (frozen full pipeline at 64/256/1024, baseline)
→ Monday: first UE 5.6 build on the PC (ARCHITECTURE section 8 checklist)

FILES
+ Source/VaelenSim/Public/Vaelen/Sim/Hydrology.h, Private/Hydrology.cpp
~ Ids.h/.cpp (IdKind::Lake = 14)
+ Tests/Sim/Test_Hydrology.cpp

TESTS
✓ Core 133 (108 without asserts) + Sim 113 (110 without asserts); ctest 38/38 in all six Linux presets
✓ AELVOR at 256: every land tile drains to the sea, 40 rivers (longest 46 tiles), 22 lakes; frozen
  flow256 7b6f14f39284c7a0, acc256 09daae8f9829deed, river256 00f52817ba9dcda8 (clang = gcc)
✓ Baseline 1024 x 1024 hydrology: 1.5 s debug, 0.36 s release
✓ Purity: 44 files, 0 violations

BLOCKERS
None
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
```

## Phase 00 task breakdown (canonical numbering: `Docs/ROADMAP.md` section 4)

| Task | Content | Status |
|---|---|---|
| 00.01 | Project architecture: `Vaelen.uproject`, targets, modules, CMake dual build, presets, CI, conventions | VALIDATED (headless) / UNVERIFIED (engine files never compiled by UBT) |
| 00.02 | Core primitives: `CoreTypes`, `Version`, `Hash`, `Random`, `Ids` | VALIDATED: Hash 15, Random 29, Ids 19, Version 7, CoreTypes 1 tests |
| 00.03 | Logging and assertions: `Log`, `Assert` | VALIDATED: Log 23, LogFloor 1, Assert 33 (23 of them in the assertions-off build) |
| 00.04 | Test harness, runner, kernel purity checker | VALIDATED: Harness 5 tests, runner registry/shuffle/reverse entries, purity self-test 36 checks |
| 00.05 | Adversarial review, fixes, documentation | VALIDATED (headless): 8 review lenses, 189 findings, fixes committed in `7d41751`; docs refreshed |

## Phase 01 task breakdown (canonical numbering: `Docs/ROADMAP.md` section 5)

| Task | Content | Status |
|---|---|---|
| 01.01 | Entity handles and registry | VALIDATED: EntityHandle 3, EntityRegistry 13 tests |
| 01.02 | Component type registry, sparse-set pools, store | VALIDATED: ComponentType 4, ComponentPool 8, ComponentStore 3 |
| 01.03 | Systems, deterministic scheduler, LOD periods | VALIDATED: Scheduler 8 |
| 01.04 | Simulation clock and calendar | VALIDATED: SimClock 4 |
| 01.05 | Events, append-only log, next-tick bus | VALIDATED: Event 2, EventLog 2, EventBus 6 |
| 01.06 | World, archives, versioned snapshot, plain-data rule | VALIDATED: Archive 4, World 3, Snapshot 8 |
| 01.07 | Deterministic replay gate | VALIDATED: Replay 5 (frozen hashes reproduced by clang, gcc, MSVC, AppleClang) |
| 01.08 | Abstract mini-world long-duration gate | VALIDATED: MiniWorld 4 (100 000 ticks, invariants, periodic replays, frozen end state) |

Phase 01 against the exit criteria of `Docs/ROADMAP.md` section 2: (1) green on the
whole CI matrix (runs 13 and 14 for 01.06 and 01.07: six Linux presets, clang-format,
Windows MSVC, macOS AppleClang; 01.08 is checked by the run of its own commit); (2)
determinism tests exist for every system: identical runs give identical digests, state
round-trips through snapshots, frozen values guard the RNG (Phase 00), the replay
reference and the mini-world end state; (3) no INCOMPLETE file; engine files UNVERIFIED;
(4) unit, integration (Scheduler, EventBus, Snapshot, Replay), deterministic, edge-case and
long-duration (MiniWorld) categories present; (5) ADR-0010 to ADR-0015 cover the phase's
decisions, docs updated. Verdict: **Phase 01 VALIDATED on the headless side, UNVERIFIED
on the engine side until the first UE 5.6 build.**

## Phase 02 task breakdown (canonical numbering: `Docs/ROADMAP.md` section 6)

| Task | Content | Status |
|---|---|---|
| 02.01 | Grid, tile layers, world-gen config, WorldMap state block, save format 2 | VALIDATED: TileGrid 4, WorldMap 6 tests |
| 02.02 | Fix64 Q32.32 fixed point, deterministic lattice noise (value, gradient, fractal, warp) | VALIDATED: FixedPoint 4, Noise 5 tests |
| 02.03 | Elevation and coastline: continental mask, relief, ridges, edge sea, terrain flags, slope, ASCII export | VALIDATED: WorldGen 6 tests |
| 02.04 | Climate and biomes: sea distance, latitude and lapse, advected moisture with rain shadow, biome table, seasons hook | VALIDATED: Climate 6 tests |
| 02.05 | Hydrology: depression filling, D8 flow, accumulation, sediment fill vs lakes, rivers and lakes as entities | VALIDATED: Hydrology 5 tests |
| 02.06-02.08 | Regions, deposits, gate | PLANNED |

## File status

Every file under `Source/`, `Tests/` and `Tools/` carries a `// STATUS:` line (rule R5 of
the purity checker, applied to headers and sources).

### Source/

| File | STATUS |
|---|---|
| `VaelenCore/Public/Vaelen/Core/CoreTypes.h` | VALIDATED (Phase 00) — integration and long-duration tests deferred to Phase 01 |
| `VaelenCore/Public/Vaelen/Core/Version.h` | VALIDATED (Phase 00) — same note |
| `VaelenCore/Public/Vaelen/Core/Assert.h` | VALIDATED (Phase 00) — same note |
| `VaelenCore/Public/Vaelen/Core/Log.h` | VALIDATED (Phase 00) — same note |
| `VaelenCore/Public/Vaelen/Core/Hash.h` | VALIDATED (Phase 00) — same note |
| `VaelenCore/Public/Vaelen/Core/Random.h` | VALIDATED (Phase 00) — same note |
| `VaelenCore/Public/Vaelen/Core/Ids.h` | VALIDATED (Phase 00) — same note |
| `VaelenCore/Private/Assert.cpp`, `Log.cpp`, `Random.cpp`, `Ids.cpp`, `Version.cpp` | VALIDATED (Phase 00) — covered by the matching `Tests/Core/Test_*.cpp` |
| `VaelenCore/Private/VaelenCoreModule.cpp` | UNVERIFIED (requires UE5) |
| `VaelenCore/VaelenCore.Build.cs`, `Vaelen.Target.cs`, `VaelenEditor.Target.cs` | UNVERIFIED (requires UE5) |
| `Vaelen/Vaelen.Build.cs`, `Vaelen/Public/Vaelen.h`, `Vaelen/Private/Vaelen.cpp`, `VaelenLogSink.h/.cpp` | UNVERIFIED (requires UE5) |

### Source/VaelenSim (Phase 01)

| File | STATUS |
|---|---|
| `Public/Vaelen/Sim/SimApi.h`, `PlainData.h`, `EntityHandle.h`, `EntityRegistry.h`, `ComponentType.h`, `ComponentPool.h`, `ComponentStore.h`, `SimClock.h`, `System.h`, `Event.h`, `EventBus.h`, `Archive.h`, `World.h`, `Snapshot.h`, `Private/EntityRegistry.cpp`, `ComponentType.cpp`, `ComponentStore.cpp`, `Scheduler.cpp`, `EventBus.cpp`, `Archive.cpp`, `World.cpp`, `Snapshot.cpp` | VALIDATED (Phase 01) — integration and long-duration tests arrive with 01.07 / 01.08 |
| `Private/VaelenSimModule.cpp`, `VaelenSim.Build.cs` | UNVERIFIED (requires UE5) |
| `Public/Vaelen/Sim/TileGrid.h`, `WorldMap.h`, `Private/WorldMap.cpp` | VALIDATED (Phase 02) — covered by `Tests/Sim/Test_TileGrid.cpp`, `Test_WorldMap.cpp` |
| `Public/Vaelen/Sim/FixedPoint.h`, `Noise.h`, `Private/Noise.cpp` | VALIDATED (Phase 02) — covered by `Tests/Sim/Test_FixedPoint.cpp`, `Test_Noise.cpp` |
| `Public/Vaelen/Sim/WorldGen.h`, `Private/WorldGen.cpp` | VALIDATED (Phase 02) — covered by `Tests/Sim/Test_WorldGen.cpp`, `Test_Climate.cpp` |
| `Public/Vaelen/Sim/Hydrology.h`, `Private/Hydrology.cpp` | VALIDATED (Phase 02) — covered by `Tests/Sim/Test_Hydrology.cpp` |

### Tests/

| File | STATUS | Tests |
|---|---|---|
| `Harness/VaelenTest.h`, `Harness/TestMain.cpp` | VALIDATED (Phase 00) | — |
| `Core/Test_Assert.cpp` | VALIDATED | 33 (23 build-independent) |
| `Core/Test_CoreTypes.cpp` | VALIDATED | 1 + compile-time asserts |
| `Core/Test_Harness.cpp` | VALIDATED | 5 |
| `Core/Test_Hash.cpp` | VALIDATED | 15 |
| `Core/Test_Ids.cpp` | VALIDATED | 19 |
| `Core/Test_Log.cpp` | VALIDATED | 23 |
| `Core/Test_LogFloor.cpp` | VALIDATED | 1 |
| `Core/Test_Random.cpp` | VALIDATED | 29 |
| `Core/Test_Version.cpp` | VALIDATED | 7 |
| `Sim/Test_EntityHandle.cpp`, `Test_EntityRegistry.cpp` | VALIDATED | 3, 13 |
| `Sim/Test_ComponentType.cpp`, `Test_ComponentPool.cpp`, `Test_ComponentStore.cpp` | VALIDATED | 4, 8, 3 |
| `Sim/Test_Scheduler.cpp`, `Test_SimClock.cpp` | VALIDATED | 8, 4 |
| `Sim/Test_Event.cpp`, `Test_EventLog.cpp`, `Test_EventBus.cpp` | VALIDATED | 2, 2, 6 |
| `Sim/Test_Archive.cpp`, `Test_World.cpp`, `Test_Snapshot.cpp` | VALIDATED | 4, 3, 8 |
| `Sim/Test_Replay.cpp` (deterministic + integration gate) | VALIDATED | 5 |
| `Sim/Test_MiniWorld.cpp` (long-duration gate) | VALIDATED | 4 |
| `Sim/Test_TileGrid.cpp`, `Test_WorldMap.cpp` (Phase 02) | VALIDATED | 4, 6 |
| `Sim/Test_FixedPoint.cpp`, `Test_Noise.cpp` (Phase 02) | VALIDATED | 4, 5 |
| `Sim/Test_WorldGen.cpp` (Phase 02) | VALIDATED | 6 |
| `Sim/Test_Climate.cpp` (Phase 02) | VALIDATED | 6 |
| `Sim/Test_Hydrology.cpp` (Phase 02) | VALIDATED | 5 |

Per-suite counts: Assert 33, CoreTypes 1, Harness 5, Hash 15, Ids 19, Log 23, LogFloor 1, Random 29, Version 7 (133 tests with assertions, 108 without). CTest entries: `Kernel.Purity`, `Kernel.PuritySelfTest`, `Core.Assert`, `Core.CoreTypes`, `Core.Harness`, `Core.Hash`, `Core.Ids`, `Core.Log`, `Core.LogFloor`, `Core.Random`, `Core.Version`, `Core.Registry`, `Core.Shuffled`, `Core.Reversed` (14 entries). Sim suites: EntityHandle 3, EntityRegistry 13, ComponentType 4, ComponentPool 8, ComponentStore 3, SimClock 4, Scheduler 8, Event 2, EventLog 2, EventBus 6, Archive 4, World 3, Snapshot 8, Replay 5, MiniWorld 4, TileGrid 4, WorldMap 6, FixedPoint 4, Noise 5, WorldGen 6, Climate 6, Hydrology 5 (113 tests, 2 124 870 checks; 110 tests without assertions); CTest entries `Sim.EntityHandle`, `Sim.EntityRegistry`, `Sim.ComponentType`, `Sim.ComponentPool`, `Sim.ComponentStore`, `Sim.SimClock`, `Sim.Scheduler`, `Sim.Event`, `Sim.EventLog`, `Sim.EventBus`, `Sim.Archive`, `Sim.World`, `Sim.Snapshot`, `Sim.Replay`, `Sim.MiniWorld`, `Sim.TileGrid`, `Sim.WorldMap`, `Sim.FixedPoint`, `Sim.Noise`, `Sim.WorldGen`, `Sim.Climate`, `Sim.Hydrology`, `Sim.Registry`, `Sim.Shuffled` (38 entries in total).

### Tools/ and CI

| File | STATUS |
|---|---|
| `Tools/check_kernel_purity.py` | VALIDATED (Phase 00): self-test 36 checks, kernel scan 12 files, 0 violations, 2 exemptions (the `long long` aliases) |
| `Tools/kernel_modules.txt` | lists `VaelenCore` |
| `.github/workflows/kernel-ci.yml` | All 9 jobs green on GitHub (run 5): six Linux presets, `format`, Windows MSVC, macOS |

## Verified here

Toolchain: clang++ 18.1.3, g++ 13.3.0, CMake 3.28.3, Ninja 1.11.1, Python 3.11.15, clang-format 18.1.3, Linux x86_64. Every preset was configured, built and tested with
`cmake --preset`, `cmake --build --preset`, `ctest --preset` into `out/build/<preset>`:

| Preset | Build | `ctest` | `VaelenCoreTests` | `VaelenSimTests` |
|---|---|---|---|---|
| linux-clang-debug | 0 warnings | 38/38 passed | 133 run, 133 passed, 22005 checks | 113 run, 113 passed, 2 124 870 checks |
| linux-gcc-debug | 0 warnings | 38/38 passed | 133 run, 133 passed, 22005 checks | 113 run, 113 passed |
| linux-clang-release | 0 warnings | 38/38 passed | 133 run, 133 passed, 22005 checks | 113 run, 113 passed |
| linux-gcc-release | 0 warnings | 38/38 passed | 133 run, 133 passed, 22005 checks | 113 run, 113 passed |
| linux-clang-noasserts | 0 warnings | 38/38 passed | 108 run, 108 passed, 21792 checks | 110 run, 110 passed |
| linux-gcc-noasserts | 0 warnings | 38/38 passed | 108 run, 108 passed, 21792 checks | 110 run, 110 passed |

Mini-world baseline (100 000 ticks, 41 entities, 305 027 events, 34 168 227-byte snapshot), logged by `Sim.MiniWorld`, not asserted: clang debug 0.39 s (255 k ticks/s), gcc debug 0.40 s, clang release 0.135 s (739 k ticks/s), gcc release without assertions 0.127 s (790 k ticks/s); snapshot 0.09-0.14 s.

GitHub Actions runs 13 to 21 (01.06 through 02.04): all 9 jobs green each, so the frozen replay, mini-world and snapshot values hold on Windows MSVC and macOS AppleClang as well. Phase 00 record - run 5 (commit `71bad2d`, https://github.com/Thomas10112/vaelen/actions/runs/33977296696): all 9 jobs green - six Linux presets, clang-format 18, Windows MSVC 19.44 (`windows-msvc-debug`, 14/14 CTest entries), macOS 15 AppleClang (`macos-debug`, 14/14).

Also run locally: `python3 Tools/check_kernel_purity.py --self-test` (36 checks, 0 failed),
`python3 Tools/check_kernel_purity.py --root . --verbose` (12 files, 0 violations),
`clang-format --style=file --dry-run -Werror` on every kernel and test source (0 drift).

## Unverified

- The Unreal build: no UE 5.6 installation is available in this environment. Every
  engine-facing file is UNVERIFIED and was reviewed only by reasoning from the UE 5.6
  API (see `Docs/ARCHITECTURE.md` section 8 for what the first engine build must confirm).
- clang-cl on Windows: only Microsoft cl (MSVC 19.44) was exercised by CI; the
  `/clang:-ffp-contract=off` branch is untested.
- Long-duration and integration test categories: deferred to Phase 01 (ROADMAP 01.07,
  01.08); every kernel STATUS line says so.

## Discrepancies

None known between code, comments and documents after the 00.05 review pass. Findings of
that pass that were deliberately NOT applied: adding `FPSemantics` to `VaelenCore.Build.cs`
(property not confirmable without an engine; the in-source `fp contract(off)` pragmas
protect the kernel instead), pinning GitHub Actions to commit SHAs (major tags kept),
and the `IdKind` placeholders for Phases 02-12 (kept, now documented as provisional).

## How to refresh this document

```
for P in linux-clang-debug linux-gcc-debug linux-clang-release linux-gcc-release linux-clang-noasserts linux-gcc-noasserts; do
  cmake --preset $P && cmake --build --preset $P && ctest --preset $P
done
out/build/linux-clang-debug/Tests/Core/VaelenCoreTests --list | cut -d. -f1 | sort | uniq -c
python3 Tools/check_kernel_purity.py --self-test
python3 Tools/check_kernel_purity.py --root . --verbose
```

Then update the BUILD STATUS block, the task table and the numbers above.
