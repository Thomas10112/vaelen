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

PHASE       : 05 — SOCIETY
TASK        : 05.05 — ORGANISATIONS ACTING
STATUS      : VALIDATED (headless) / UNVERIFIED (engine)

PROGRESS
███████████░░░░░░░░░░░░░ 48%

CURRENTLY
→ 05.05 closed: guilds and warbands founded and seated by the organisation system from the skilled;
  the yearly DecisionSystem - a council lays in grain after a drought (RegionStores that the need
  system observes, softening the next drought's cut), a temple preaches and converts a share of the
  region's people of other faiths (persons first, counts reconciled), a guild trains its members'
  craft, a warband plans a raid on the most peopled neighbour every five years (RaidPlanned for
  Phase 08); DecisionMade events with the drought or the raid as cause

COMPLETED
✓ Phases 00-04 (headless) ; 05.01-05.04 (CI 45)
✓ 05.05 Organisations acting (4 tests: Decisions)

NEXT
→ 05.06 Social shape across the grains (strata shares kept through demotion, honoured at promotion)
→ 05.07 Society in history
→ Monday: first UE 5.6 build on the PC (ARCHITECTURE section 8 checklist)

FILES
+ Source/VaelenSociety/Public/Vaelen/Society/Decisions.h, Private/Decisions.cpp
+ Tests/Society/Test_Decisions.cpp
~ Needs.h/.cpp (RegionStores, NeedSystem::ObserveStores), Organizations.h/.cpp (guilds and warbands: founding
  thresholds, seats by craft and fighting, heads by skill), Test_Organizations.cpp and Test_Standing.cpp
  and Test_Bondage.cpp (refrozen: guilds and warbands now exist), Source/VaelenSociety/CMakeLists.txt

TESTS
✓ Core 133 (108 without asserts) + Sim 162 (159 without asserts) + Population 37 (37 without asserts)
  + Society 21 (21 without asserts); ctest 66/66 in all six Linux presets; Phase 03 and 04 digests unchanged
✓ Region 26 of AELVOR 128 for 100 years: 155 decisions by 4 organisations; frozen stores digest d99d3be56bcbfe4f;
  organisations refrozen 8ce703a8fa934de0, standing refrozen ddad5d79565a46f8, bondage refrozen a1a92018c62bc034
✓ Purity: 91 files, 0 violations

BLOCKERS
∅ (engine-side files stay UNVERIFIED until the first UE 5.6 build)
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
| 03.05 | Disasters and omens tied to the world, with deaths, faith and era consequences | VALIDATED: Disasters 5 tests |
| 03.06 | PreHistory object and one-call run, per-century frozen reference at 256, mid-history snapshot, 1024 baseline | VALIDATED: PreHistory 5 tests |
| 03.07 | Why-queries, region timelines, the chronicle as deterministic text | VALIDATED: HistoryText 5 tests |
| 03.08 | Phase 03 gate: 2000 years at 256 with invariants every decade, frozen at 1000 / 1500 / 2000, snapshot at year 1000 | VALIDATED: HistoryGate 2 tests |

Phase 03 against the exit criteria (`Docs/ROADMAP.md` section 2): (1) CI matrix green
for 03.01 to 03.07 (runs 26 to 32, run 29 red on MSVC and fixed in 03.05), 03.08 by its own
run; (2) determinism tests for every system, frozen digests per task and per century,
reproduced by clang, gcc, MSVC and AppleClang; (3) no INCOMPLETE file, engine files
UNVERIFIED; (4) unit, integration, deterministic, edge-case and long-duration categories
present (2000 years at 256, 500 years at 1024); (5) ADR-0024 to ADR-0031, docs updated.
Verdict: **Phase 03 VALIDATED on the headless side, UNVERIFIED on the engine side until
the first UE 5.6 build.**

## Phase 04 task breakdown (canonical numbering: `Docs/ROADMAP.md` section 8)

| Task | Content | Status |
|---|---|---|
| 04.01 | `VaelenPopulation` module, `PersonInfo`, promotion of a region into persons and demotion back, consistency | VALIDATED: Persons 5 tests |
| 04.02 | LifeSystem (ageing, mortality, fertility, reconciliation), RegionLod marker observed by the coarse systems | VALIDATED: Lives 5 tests |
| 04.03 | FamilySystem (marriages, families, heads, extinction), lineage queries, births to couples | VALIDATED: Families 5 tests |
| 04.04 | PersonNeeds and NeedSystem (rations, famine from drought, disease from plague, deaths with causes) | VALIDATED: Needs 6 tests |
| 04.05 | PersonTraits and TraitSystem (traits from identity and parents, skills through life, names in the person's language) | VALIDATED: Traits 5 tests |
| 04.06 | LodState, LodSystem (requests, promotions, demotions, crossings both ways) | VALIDATED: Lod 5 tests |
| 04.07 | PersonChronicle listener, person lines, unified chronicle, stories and the why of a death | VALIDATED: PersonHistory 5 tests |
| 04.08 | Phase 04 gate: 500 years at 256 with a detailed region and every Phase 04 system, invariants every decade, frozen at 250 / 500, snapshot at year 250 | VALIDATED: PopulationGate 1 test |

Phase 04 against the exit criteria (`Docs/ROADMAP.md` section 2): (1) CI matrix green
for 04.01 to 04.07 (runs 34 to 39), 04.08 by its own run; (2) determinism tests for every
system, frozen digests per task and for the gate at 250 and 500 years, reproduced by
clang, gcc, MSVC and AppleClang; (3) no INCOMPLETE file, engine files UNVERIFIED; (4)
unit, integration, deterministic, edge-case and long-duration categories present (500
years at 256 with a detailed region, 500 years of LOD alternation at 64); (5) ADR-0032 to
ADR-0039, docs updated. Verdict: **Phase 04 VALIDATED on the headless side, UNVERIFIED on
the engine side until the first UE 5.6 build.**

## Phase 05 task breakdown (canonical numbering: `Docs/ROADMAP.md` section 9)

| Task | Content | Status |
|---|---|---|
| 05.01 | `VaelenSociety` module, organisations as entities (councils, temples), seats, heads, coarse counts | VALIDATED: Organizations 5 tests |
| 05.02 | PersonStanding and StandingSystem (score from house, age, traits, skills and offices; ranks and tiers; the elite of a region) | VALIDATED: Standing 4 tests |
| 05.03 | NormSet per culture and NormSystem (customs from identity and parent, drifts on schisms and disasters), MarriageNorms observed by the family system | VALIDATED: Norms 4 tests |
| 05.04 | BondState, RegionStrata and BondageSystem (debt and birth entries, hardening, manumission, flight, holders, strata per region) | VALIDATED: Bondage 4 tests |
| 05.05 | DecisionSystem (grain against drought, preaching, training, raids planned), guilds and warbands, RegionStores observed by the need system | VALIDATED: Decisions 4 tests |
| 05.06-05.08 | Social shape across the grains, society in history, gate | PLANNED |

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
| `Public/Vaelen/Sim/Disasters.h`, `Private/Disasters.cpp` | VALIDATED (Phase 03) — covered by `Tests/Sim/Test_Disasters.cpp` |
| `Public/Vaelen/Sim/PreHistory.h`, `Private/PreHistory.cpp` | VALIDATED (Phase 03) — covered by `Tests/Sim/Test_PreHistory.cpp` |
| `Public/Vaelen/Sim/HistoryText.h`, `Private/HistoryText.cpp` | VALIDATED (Phase 03) — covered by `Tests/Sim/Test_HistoryText.cpp` |

### Source/VaelenPopulation (Phase 04)

| File | STATUS |
|---|---|
| `VaelenPopulation.Build.cs`, `Private/VaelenPopulationModule.cpp` | UNVERIFIED — engine-side, not compiled headless |
| `CMakeLists.txt` | VALIDATED — six Linux presets |
| `Public/Vaelen/Population/PopulationApi.h` | VALIDATED (Phase 04) |
| `Public/Vaelen/Population/Persons.h`, `Private/Persons.cpp` | VALIDATED (Phase 04) — covered by `Tests/Population/Test_Persons.cpp` |
| `Public/Vaelen/Population/Lives.h`, `Private/Lives.cpp` | VALIDATED (Phase 04) — covered by `Tests/Population/Test_Lives.cpp` |
| `Public/Vaelen/Population/Families.h`, `Private/Families.cpp` | VALIDATED (Phase 04) — covered by `Tests/Population/Test_Families.cpp` |
| `Public/Vaelen/Population/Needs.h`, `Private/Needs.cpp` | VALIDATED (Phase 04) — covered by `Tests/Population/Test_Needs.cpp` |
| `Public/Vaelen/Population/Traits.h`, `Private/Traits.cpp` | VALIDATED (Phase 04) — covered by `Tests/Population/Test_Traits.cpp` |
| `Public/Vaelen/Population/Lod.h`, `Private/Lod.cpp` | VALIDATED (Phase 04) — covered by `Tests/Population/Test_Lod.cpp` |
| `Public/Vaelen/Population/PersonHistory.h`, `Private/PersonHistory.cpp` | VALIDATED (Phase 04) — covered by `Tests/Population/Test_PersonHistory.cpp` |

### `Source/VaelenSociety` (Phase 05)

| File | Status |
|---|---|
| `VaelenSociety.Build.cs`, `Private/VaelenSocietyModule.cpp` | UNVERIFIED — engine-side, not compiled headless |
| `CMakeLists.txt` | VALIDATED (Phase 05) |
| `Public/Vaelen/Society/SocietyApi.h` | VALIDATED (Phase 05) |
| `Public/Vaelen/Society/Organizations.h`, `Private/Organizations.cpp` | VALIDATED (Phase 05) — covered by `Tests/Society/Test_Organizations.cpp` |
| `Public/Vaelen/Society/Standing.h`, `Private/Standing.cpp` | VALIDATED (Phase 05) — covered by `Tests/Society/Test_Standing.cpp` |
| `Public/Vaelen/Society/Norms.h`, `Private/Norms.cpp` | VALIDATED (Phase 05) — covered by `Tests/Society/Test_Norms.cpp` |
| `Public/Vaelen/Society/BondState.h`, `Public/Vaelen/Society/Bondage.h`, `Private/Bondage.cpp` | VALIDATED (Phase 05) — covered by `Tests/Society/Test_Bondage.cpp` |
| `Public/Vaelen/Society/Decisions.h`, `Private/Decisions.cpp` | VALIDATED (Phase 05) — covered by `Tests/Society/Test_Decisions.cpp` |

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
| `Sim/Test_Disasters.cpp` (Phase 03) | VALIDATED | 5 |
| `Sim/Test_PreHistory.cpp` (Phase 03) | VALIDATED | 5 |
| `Sim/Test_HistoryText.cpp` (Phase 03) | VALIDATED | 5 |
| `Sim/Test_HistoryGate.cpp` (Phase 03 gate) | VALIDATED | 2 |
| `Population/Test_Persons.cpp` (Phase 04) | VALIDATED | 5 |
| `Population/Test_Lives.cpp` (Phase 04) | VALIDATED | 5 |
| `Population/Test_Families.cpp` (Phase 04) | VALIDATED | 5 |
| `Population/Test_Needs.cpp` (Phase 04) | VALIDATED | 6 |
| `Population/Test_Traits.cpp` (Phase 04) | VALIDATED | 5 |
| `Population/Test_Lod.cpp` (Phase 04) | VALIDATED | 5 |
| `Population/Test_PersonHistory.cpp` (Phase 04) | VALIDATED | 5 |
| `Population/Test_PopulationGate.cpp` (Phase 04) | VALIDATED | 1 |
| `Society/Test_Organizations.cpp` (Phase 05) | VALIDATED | 5 |
| `Society/Test_Standing.cpp` (Phase 05) | VALIDATED | 4 |
| `Society/Test_Norms.cpp` (Phase 05) | VALIDATED | 4 |
| `Society/Test_Bondage.cpp` (Phase 05) | VALIDATED | 4 |
| `Society/Test_Decisions.cpp` (Phase 05) | VALIDATED | 4 |

Per-suite counts: Assert 33, CoreTypes 1, Harness 5, Hash 15, Ids 19, Log 23, LogFloor 1, Random 29, Version 7 (133 tests with assertions, 108 without). CTest entries: `Kernel.Purity`, `Kernel.PuritySelfTest`, `Core.Assert`, `Core.CoreTypes`, `Core.Harness`, `Core.Hash`, `Core.Ids`, `Core.Log`, `Core.LogFloor`, `Core.Random`, `Core.Version`, `Core.Registry`, `Core.Shuffled`, `Core.Reversed` (14 entries). Sim suites: EntityHandle 3, EntityRegistry 13, ComponentType 4, ComponentPool 8, ComponentStore 3, SimClock 4, Scheduler 8, Event 2, EventLog 2, EventBus 6, Archive 4, World 3, Snapshot 8, Replay 5, MiniWorld 4, TileGrid 4, WorldMap 6, FixedPoint 4, Noise 5, WorldGen 6, Climate 6, Hydrology 5, Regions 5, Deposits 5, WorldPipeline 4, History 3, Population 5, Naming 5, Religion 5, Disasters 5, PreHistory 5, HistoryText 5, HistoryGate 2 (162 tests; 159 tests without assertions); CTest entries `Sim.EntityHandle`, `Sim.EntityRegistry`, `Sim.ComponentType`, `Sim.ComponentPool`, `Sim.ComponentStore`, `Sim.SimClock`, `Sim.Scheduler`, `Sim.Event`, `Sim.EventLog`, `Sim.EventBus`, `Sim.Archive`, `Sim.World`, `Sim.Snapshot`, `Sim.Replay`, `Sim.MiniWorld`, `Sim.TileGrid`, `Sim.WorldMap`, `Sim.FixedPoint`, `Sim.Noise`, `Sim.WorldGen`, `Sim.Climate`, `Sim.Hydrology`, `Sim.Regions`, `Sim.Deposits`, `Sim.WorldPipeline`, `Sim.History`, `Sim.Population`, `Sim.Naming`, `Sim.Religion`, `Sim.Disasters`, `Sim.PreHistory`, `Sim.HistoryText`, `Sim.HistoryGate`, `Sim.Registry`, `Sim.Shuffled` (42 entries in total). Population suites: Persons 5, Lives 5, Families 5, Needs 6, Traits 5, Lod 5, PersonHistory 5, PopulationGate 1 (37 tests; 37 without assertions); CTest entries `Population.Persons`, `Population.Lives`, `Population.Families`, `Population.Needs`, `Population.Traits`, `Population.Lod`, `Population.PersonHistory`, `Population.PopulationGate`, `Population.Registry`, `Population.Shuffled` (10 entries). Society suites: Organizations 5, Standing 4, Norms 4, Bondage 4, Decisions 4 (21 tests; 21 without assertions); CTest entries `Society.Organizations`, `Society.Standing`, `Society.Norms`, `Society.Bondage`, `Society.Decisions`, `Society.Registry`, `Society.Shuffled` (7 entries).

### Tools/ and CI

| File | STATUS |
|---|---|
| `Tools/check_kernel_purity.py` | VALIDATED (Phase 00): self-test 36 checks, kernel scan 12 files, 0 violations, 2 exemptions (the `long long` aliases) |
| `Tools/kernel_modules.txt` | lists `VaelenCore` |
| `.github/workflows/kernel-ci.yml` | All 9 jobs green on GitHub (run 5): six Linux presets, `format`, Windows MSVC, macOS |

## Verified here

Toolchain: clang++ 18.1.3, g++ 13.3.0, CMake 3.28.3, Ninja 1.11.1, Python 3.11.15, clang-format 18.1.3, Linux x86_64. Every preset was configured, built and tested with
`cmake --preset`, `cmake --build --preset`, `ctest --preset` into `out/build/<preset>`:

| Preset | Build | `ctest` | `VaelenCoreTests` | `VaelenSimTests` | `VaelenPopulationTests` | `VaelenSocietyTests` |
|---|---|---|---|---|---|---|
| linux-clang-debug | 0 warnings | 66/66 passed | 133 run, 133 passed, 22097 checks | 162 run, 162 passed | 37 run, 37 passed | 21 run, 21 passed |
| linux-gcc-debug | 0 warnings | 66/66 passed | 133 run, 133 passed, 22097 checks | 162 run, 162 passed | 37 run, 37 passed | 21 run, 21 passed |
| linux-clang-release | 0 warnings | 66/66 passed | 133 run, 133 passed, 22097 checks | 162 run, 162 passed | 37 run, 37 passed | 21 run, 21 passed |
| linux-gcc-release | 0 warnings | 66/66 passed | 133 run, 133 passed, 22097 checks | 162 run, 162 passed | 37 run, 37 passed | 21 run, 21 passed |
| linux-clang-noasserts | 0 warnings | 66/66 passed | 108 run, 108 passed, 21884 checks | 159 run, 159 passed | 37 run, 37 passed | 21 run, 21 passed |
| linux-gcc-noasserts | 0 warnings | 66/66 passed | 108 run, 108 passed, 21884 checks | 159 run, 159 passed | 37 run, 37 passed | 21 run, 21 passed |

Mini-world baseline (100 000 ticks, 41 entities, 305 027 events, 34 168 227-byte snapshot), logged by `Sim.MiniWorld`, not asserted: clang debug 0.39 s (255 k ticks/s), gcc debug 0.40 s, clang release 0.135 s (739 k ticks/s), gcc release without assertions 0.127 s (790 k ticks/s); snapshot 0.09-0.14 s.

GitHub Actions runs 13 to 28 (01.06 through 03.03): all 9 jobs green each; run 29 (03.04) red on Windows MSVC only (a dangling pool pointer in the faith listener changed the religion digest, and `Sim.Shuffled` exceeded its 300 s CTest timeout), both fixed in the 03.05 commit and green again in runs 30 to 45 (03.05 to 05.04), so the frozen replay, mini-world and snapshot values hold on Windows MSVC and macOS AppleClang as well. Phase 00 record - run 5 (commit `71bad2d`, https://github.com/Thomas10112/vaelen/actions/runs/33977296696): all 9 jobs green - six Linux presets, clang-format 18, Windows MSVC 19.44 (`windows-msvc-debug`, 14/14 CTest entries), macOS 15 AppleClang (`macos-debug`, 14/14).

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
