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

PHASE       : 03 — HISTORY
TASK        : 03.04 — RELIGIONS
STATUS      : VALIDATED (headless) / UNVERIFIED (engine)

PROGRESS
████████████░░░░░░░░░░░░ 50%

CURRENTLY
→ 03.04 closed: religions are entities born from an event (a new era, a culture split as
  schism, or a requested founding with its cause), believers counted per region and bounded
  by its people, faith spreading yearly along the region graph and travelling with migration
  waves, eight tenet axes as data, names in the founding culture's language

COMPLETED
✓ Phase 00 — FOUNDATION ; Phase 01 — CORE SIMULATION ; Phase 02 — WORLD (headless)
✓ 03.01 Eras (CI 26) ; 03.02 Cultures (CI 27) ; 03.03 Languages (CI 28) ; 03.04 Religions (5 tests: Religion)

NEXT
→ 03.05 Disasters and omens (drought, flood, eruption, plague with causal consequences)
→ 03.06 Pre-history run and the starting state ; 03.07 Queryable history ; 03.08 gate
→ Monday: first UE 5.6 build on the PC (ARCHITECTURE section 8 checklist)

FILES
+ Source/VaelenSim/Public/Vaelen/Sim/Religion.h, Private/Religion.cpp
~ Naming.h (NameScope::Religion)
+ Tests/Sim/Test_Religion.cpp

TESTS
✓ Core 133 (108 without asserts) + Sim 145 (142 without asserts); ctest 45/45 in all six Linux presets
✓ AELVOR 128 after 500 years: 10 religions (8 schisms), 45 480 believers of 47 587 people, 74/99 regions
  with a majority faith, every religion traced to its founding event; frozen state 3d9bf5c1c7732241
✓ Purity: 58 files, 0 violations

BLOCKERS
∅ (engine-side files stay UNVERIFIED until the first UE 5.6 build)
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

PHASE       : 03 — HISTORY
TASK        : 03.03 — LANGUAGES AND NAMING
STATUS      : VALIDATED (headless) / UNVERIFIED (engine)

PROGRESS
█████████░░░░░░░░░░░░░░░ 37%

CURRENTLY
→ 03.03 closed: one language per culture (phonology derived from the culture's identity or
  drifted from the parent's on a split, one sound change every 150 years), names built
  syllable by syllable and pronounceable by construction, unique per scope, given yearly to
  cultures, languages, settled regions, rivers, lakes and eras as components on the named entity

COMPLETED
✓ Phase 00 — FOUNDATION ; Phase 01 — CORE SIMULATION ; Phase 02 — WORLD (headless)
✓ 03.01 Eras (CI 26) ; 03.02 Cultures and population (CI 27) ; 03.03 Languages and naming (5 tests: Naming)

NEXT
→ 03.04 Religions (born from cultures and events, spread along migration, schisms)
→ 03.05 Disasters and omens ; 03.06 Pre-history run
→ Monday: first UE 5.6 build on the PC (ARCHITECTURE section 8 checklist)

FILES
+ Source/VaelenSim/Public/Vaelen/Sim/Naming.h, Private/Naming.cpp
+ Tests/Sim/Test_Naming.cpp

TESTS
✓ Core 133 (108 without asserts) + Sim 140 (137 without asserts); ctest 44/44 in all six Linux presets
✓ 14 336 generated names all pronounceable, 3-16 letters, at least 113/128 distinct per scope;
  AELVOR 128 after 500 years: 18 languages, 154 names, 0 duplicates, frozen state 871cdd11bea18906
✓ Purity: 56 files, 0 violations

BLOCKERS
∅ (engine-side files stay UNVERIFIED until the first UE 5.6 build)
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

PHASE       : 03 — HISTORY
TASK        : 03.02 — CULTURES AND COARSE POPULATION
STATUS      : VALIDATED (headless) / UNVERIFIED (engine)

PROGRESS
██████░░░░░░░░░░░░░░░░░░ 25%

CURRENTLY
→ 03.02 closed: Culture entities seeded on the best spread regions, coarse population per
  region (six culture slots), yearly growth bounded by biome, river and fertile-deposit
  capacity, monthly migration along the region graph, assimilation, abandonment and
  culture splits by graph distance with lineage spacing

COMPLETED
✓ Phase 00 — FOUNDATION ; Phase 01 — CORE SIMULATION ; Phase 02 — WORLD (headless)
✓ 03.01 Eras and the historical record (CI 26) ; 03.02 Cultures and population (5 tests: Population)

NEXT
→ 03.03 Languages and naming (Language entities per culture, deterministic names)
→ 03.04 Religions ; 03.05 Disasters and omens
→ Monday: first UE 5.6 build on the PC (ARCHITECTURE section 8 checklist)

FILES
+ Source/VaelenSim/Public/Vaelen/Sim/Population.h, Private/Population.cpp
+ Tests/Sim/Test_Population.cpp

TESTS
✓ Core 133 (108 without asserts) + Sim 135 (132 without asserts); ctest 43/43 in all six Linux presets
✓ AELVOR 128, 500 years: 47 587 people of 55 174 capacity, 18 cultures from 4 seeds, 74/99 regions
  settled; migration conserves people; frozen state f2afaa068c0f717d (clang = gcc)
✓ Purity: 54 files, 0 violations

BLOCKERS
∅ (engine-side files stay UNVERIFIED until the first UE 5.6 build)
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

World-generation baselines at 1024 x 1024 (AELVOR seed, logged and not asserted): elevation 0.74 s, hydrology 0.36 s, regions 0.35 s, deposits 0.30 s in release; full pipeline 25 s in debug with assertions.

| Task | Content | Status |
|---|---|---|
| 02.01 | Grid, tile layers, world-gen config, WorldMap state block, save format 2 | VALIDATED: TileGrid 4, WorldMap 6 tests |
| 02.02 | Fix64 Q32.32 fixed point, deterministic lattice noise (value, gradient, fractal, warp) | VALIDATED: FixedPoint 4, Noise 5 tests |
| 02.03 | Elevation and coastline: continental mask, relief, ridges, edge sea, terrain flags, slope, ASCII export | VALIDATED: WorldGen 6 tests |
| 02.04 | Climate and biomes: sea distance, latitude and lapse, advected moisture with rain shadow, biome table, seasons hook | VALIDATED: Climate 6 tests |
| 02.05 | Hydrology: depression filling, D8 flow, accumulation, sediment fill vs lakes, rivers and lakes as entities | VALIDATED: Hydrology 5 tests |
| 02.06 | Regions: jittered-lattice seeds, terrain-cost growth, size-floor merging, entities, adjacency graph | VALIDATED: Regions 5 tests |
| 02.07 | Resource deposits: suitability rules, hashed draws, spacing cells, richness and tiers, entities | VALIDATED: Deposits 5 tests |
| 02.08 | Phase 02 gate: one-call pipeline, frozen whole-world digests at three sizes, regeneration, snapshot, edge cases, simulation over a generated world | VALIDATED: WorldPipeline 4 tests |

Phase 02 against the exit criteria of `Docs/ROADMAP.md` section 2: (1) green on the
whole CI matrix for 02.01 through 02.07 (runs 18 to 24: six Linux presets, clang-format,
Windows MSVC, macOS AppleClang), 02.08 checked by its own run; (2) every stage has a
frozen digest at 256 and the whole world at 64, 256 and 1024, regeneration is byte
identical and snapshots re-hash identically; (3) no INCOMPLETE file; engine files
UNVERIFIED; (4) unit (helpers, rules, fixed point), integration (pipeline, snapshot of
generated worlds), deterministic (frozen values everywhere), edge-case (drowned world,
1 x 1, non-square, two islands, synthetic ridge and basin) and long-duration (1024 x
1024 baselines per stage and for the pipeline, simulation and replay over a generated
world) categories present; (5) ADR-0016 to ADR-0023 cover the phase's decisions, docs
updated. Verdict: **Phase 02 VALIDATED on the headless side, UNVERIFIED on the engine
side until the first UE 5.6 build.**

## Phase 03 task breakdown (canonical numbering: `Docs/ROADMAP.md` section 7)

| Task | Content | Status |
|---|---|---|
| 03.01 | Eras, era calendar, chronicle records, history queries | VALIDATED: History 3 tests |
| 03.02 | Culture entities, coarse population per region, growth, migration, assimilation, splits | VALIDATED: Population 5 tests |
| 03.03 | Languages per culture, phonology drift, pronounceable unique names for cultures, regions, rivers, lakes, eras | VALIDATED: Naming 5 tests |
| 03.04 | Religions born from events, believers per region, spread along the graph and with migration, schisms, tenets | VALIDATED: Religion 5 tests |
| 03.05-03.08 | Disasters, pre-history run, queries, gate | PLANNED |

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
| `Public/Vaelen/Sim/Regions.h`, `Private/Regions.cpp` | VALIDATED (Phase 02) — covered by `Tests/Sim/Test_Regions.cpp` |
| `Public/Vaelen/Sim/Deposits.h`, `Private/Deposits.cpp` | VALIDATED (Phase 02) — covered by `Tests/Sim/Test_Deposits.cpp` |
| `Public/Vaelen/Sim/WorldGenPipeline.h`, `Private/WorldGenPipeline.cpp` | VALIDATED (Phase 02) — covered by `Tests/Sim/Test_WorldPipeline.cpp` |
| `Public/Vaelen/Sim/History.h`, `Private/History.cpp` | VALIDATED (Phase 03) — covered by `Tests/Sim/Test_History.cpp` |
| `Public/Vaelen/Sim/Population.h`, `Private/Population.cpp` | VALIDATED (Phase 03) — covered by `Tests/Sim/Test_Population.cpp` |
| `Public/Vaelen/Sim/Naming.h`, `Private/Naming.cpp` | VALIDATED (Phase 03) — covered by `Tests/Sim/Test_Naming.cpp` |
| `Public/Vaelen/Sim/Religion.h`, `Private/Religion.cpp` | VALIDATED (Phase 03) — covered by `Tests/Sim/Test_Religion.cpp` |

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
| `Sim/Test_Regions.cpp` (Phase 02) | VALIDATED | 5 |
| `Sim/Test_Deposits.cpp` (Phase 02) | VALIDATED | 5 |
| `Sim/Test_WorldPipeline.cpp` (Phase 02 gate) | VALIDATED | 4 |
| `Sim/Test_History.cpp` (Phase 03) | VALIDATED | 3 |
| `Sim/Test_Population.cpp` (Phase 03) | VALIDATED | 5 |
| `Sim/Test_Naming.cpp` (Phase 03) | VALIDATED | 5 |
| `Sim/Test_Religion.cpp` (Phase 03) | VALIDATED | 5 |

Per-suite counts: Assert 33, CoreTypes 1, Harness 5, Hash 15, Ids 19, Log 23, LogFloor 1, Random 29, Version 7 (133 tests with assertions, 108 without). CTest entries: `Kernel.Purity`, `Kernel.PuritySelfTest`, `Core.Assert`, `Core.CoreTypes`, `Core.Harness`, `Core.Hash`, `Core.Ids`, `Core.Log`, `Core.LogFloor`, `Core.Random`, `Core.Version`, `Core.Registry`, `Core.Shuffled`, `Core.Reversed` (14 entries). Sim suites: EntityHandle 3, EntityRegistry 13, ComponentType 4, ComponentPool 8, ComponentStore 3, SimClock 4, Scheduler 8, Event 2, EventLog 2, EventBus 6, Archive 4, World 3, Snapshot 8, Replay 5, MiniWorld 4, TileGrid 4, WorldMap 6, FixedPoint 4, Noise 5, WorldGen 6, Climate 6, Hydrology 5, Regions 5, Deposits 5, WorldPipeline 4, History 3, Population 5, Naming 5, Religion 5 (145 tests; 142 tests without assertions); CTest entries `Sim.EntityHandle`, `Sim.EntityRegistry`, `Sim.ComponentType`, `Sim.ComponentPool`, `Sim.ComponentStore`, `Sim.SimClock`, `Sim.Scheduler`, `Sim.Event`, `Sim.EventLog`, `Sim.EventBus`, `Sim.Archive`, `Sim.World`, `Sim.Snapshot`, `Sim.Replay`, `Sim.MiniWorld`, `Sim.TileGrid`, `Sim.WorldMap`, `Sim.FixedPoint`, `Sim.Noise`, `Sim.WorldGen`, `Sim.Climate`, `Sim.Hydrology`, `Sim.Regions`, `Sim.Deposits`, `Sim.WorldPipeline`, `Sim.History`, `Sim.Population`, `Sim.Naming`, `Sim.Religion`, `Sim.Registry`, `Sim.Shuffled` (42 entries in total).

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
| linux-clang-debug | 0 warnings | 45/45 passed | 133 run, 133 passed, 22097 checks | 145 run, 145 passed |
| linux-gcc-debug | 0 warnings | 45/45 passed | 133 run, 133 passed, 22097 checks | 145 run, 145 passed |
| linux-clang-release | 0 warnings | 45/45 passed | 133 run, 133 passed, 22097 checks | 145 run, 145 passed |
| linux-gcc-release | 0 warnings | 45/45 passed | 133 run, 133 passed, 22097 checks | 145 run, 145 passed |
| linux-clang-noasserts | 0 warnings | 45/45 passed | 108 run, 108 passed, 21884 checks | 142 run, 142 passed |
| linux-gcc-noasserts | 0 warnings | 45/45 passed | 108 run, 108 passed, 21884 checks | 142 run, 142 passed |

Mini-world baseline (100 000 ticks, 41 entities, 305 027 events, 34 168 227-byte snapshot), logged by `Sim.MiniWorld`, not asserted: clang debug 0.39 s (255 k ticks/s), gcc debug 0.40 s, clang release 0.135 s (739 k ticks/s), gcc release without assertions 0.127 s (790 k ticks/s); snapshot 0.09-0.14 s.

GitHub Actions runs 13 to 28 (01.06 through 03.03): all 9 jobs green each, so the frozen replay, mini-world and snapshot values hold on Windows MSVC and macOS AppleClang as well. Phase 00 record - run 5 (commit `71bad2d`, https://github.com/Thomas10112/vaelen/actions/runs/33977296696): all 9 jobs green - six Linux presets, clang-format 18, Windows MSVC 19.44 (`windows-msvc-debug`, 14/14 CTest entries), macOS 15 AppleClang (`macos-debug`, 14/14).

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
