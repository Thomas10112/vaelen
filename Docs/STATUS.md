# VAELEN — Build status

STATUS: VALIDATED for the state it reports, checked on 2026-09-07 against the sources on
branch `claude/vaelen-master-prompt-aw7zqj` after the Phase 00 review pass. This is the
living status document: it is refreshed at the end of every task (section "How to
refresh").

## BUILD STATUS

```
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
VAELEN BUILD STATUS
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

PHASE       : 09 — INFRASTRUCTURE — IN PROGRESS
TASK        : 09.04 — ROADS
STATUS      : PROTOTYPE (headless) / UNVERIFIED (engine)

PROGRESS
███████████████████████░ 89%

CURRENTLY
→ A route of 06.04 is not a road. It is a fact about prices: grain was dearer there than here, often
  enough and long enough that somebody carried it. It opens because a gap opened and closes because
  nothing crossed it, and nobody ever built it.

  09.04 lets the regions at its two ends make something of it. A road is cut out of both common
  stocks, half each, and out of the hands both can spare - a road is never one region's - and only
  where enough already crosses to be worth the timber. What it does is let more of the trade that
  already wanted to happen get across: one number, RouteEase::CarryPerMille, where trade already
  reads, and no road is a factor of one to the unit.

  And it has to be kept. Unkept it wears; with nothing left to wear it loses a grade; at grade zero
  it is a track again - which is what a route was before anybody touched it. Nothing is destroyed and
  no entity dies: it falls back to what the economy always had, and can be cut again.

  Most roads outlive the trade that made them and then go: 22 cut over a century, 3 still made, 19
  back to tracks. A road on a route trade has closed cannot be kept, because keeping is a thing the
  two ends do for a route that is still carrying. That is the behaviour wanted, not a defect.

WHAT PHASE 09 IS
→ 06.04 gave the world routes as artefacts of trade and 05.05 gave councils a granary as a number on
  a region. Phase 09 does not replace either: it gives the world the things people actually build and
  keep - buildings that cost goods and labour and decay when nobody minds them - and makes those
  routes and those granaries the first two examples of it.

COMPLETED
✓ Phases 00-07 (headless, Phase 07 closed)
✓ Phase 08 MILITARY closed
✓ CI run 82 green on all nine jobs, on the tree carrying 09.01 and 09.02
✓ 09.01 buildings · 09.02 what a building does · 09.03 settlements as places
✓ 09.04 roads — RoadInfo, RouteEase, RoadSystem, MeasureRoads, 5 tests

NEXT
→ 09.05 — decay and ruins: everything built falls down unless it is kept

TESTS (a task inside a phase runs two presets; six at the gate)
✓ linux-clang-debug and linux-gcc-release green, ctest 91/91 each (gates and shuffled excluded)
✓ VaelenInfrastructureTests 18 run, 18 passed, on both presets
✓ AELVOR 128 at year 420: 22 roads cut, 3 still made, 19 tracks, 4320 timber, digest 7ede5ba455016378
✓ Two worlds of one seed, roads cut and paid for the same, worth nothing in one: 321 307 units
  carried against 321 110
✓ A road nobody can ever keep loses every grade and ends a track, and every road left standing was
  cut too recently to have fallen
✓ Purity: 145 files, 0 violations; clang-format: 0 files need formatting

EXIT CRITERIA (roadmap section 2)
◻ 1. Six Linux presets with every gate — at the Phase 09 gate (09.08)
✓ 2. Determinism tests for 09.01 to 09.04: same seed, snapshot round trip and replay, frozen digests
◻ 3. The files of the phase are PROTOTYPE until the Phase 09 gate; engine files stay UNVERIFIED
✓ 4. Unit, integration, deterministic and edge tests for all four systems; long-duration at 09.08
✓ 5. ARCHITECTURE, DECISIONS, ROADMAP and STATUS updated; ADR-0074 to ADR-0077

BLOCKERS
∅ (engine-side files of the module stay UNVERIFIED until the next UE 5.6 build)
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

```

## Phase 00 task breakdown (canonical numbering: `Docs/ROADMAP.md` section 4)

| Task | Content | Status |
|---|---|---|
| 00.01 | Project architecture: `Vaelen.uproject`, targets, modules, CMake dual build, presets, CI, conventions | VALIDATED (headless) / VALIDATED (engine: first UBT build 2026-09-07, ARCHITECTURE section 8) |
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
| 02.06 | Regions: jittered-lattice seeds, terrain-cost growth, size-floor merging, entities, adjacency graph, the graph cache | VALIDATED: Regions 6 tests |
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
| 04.01 | `VaelenPopulation` module, `PersonInfo`, promotion of a region into persons and demotion back, consistency, the index counter | VALIDATED: Persons 6 tests |
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
| 05.06 | Strata honoured at a promotion (Promotion entry), departures release bonds, 500 years of alternation | VALIDATED: Strata 3 tests |
| 05.07 | SocietyChronicle listener, society lines, unified chronicle, the why of a decision | VALIDATED: SocietyHistory 3 tests |
| 05.08 | Phase 05 gate: 500 years at 256 with a detailed region and every Phase 04 and 05 system, invariants every decade, frozen at 250 / 500 with the log and the text, snapshot at year 250 | VALIDATED: SocietyGate 1 test |

Phase 05 against the exit criteria (`Docs/ROADMAP.md` section 2): (1) CI matrix green
for 05.01 to 05.07 (runs 42 to 48), 05.08 by its own run; (2) determinism tests for every
system, frozen digests per task and for the gate at 250 and 500 years, reproduced by
clang, gcc, MSVC and AppleClang; (3) no INCOMPLETE file, engine files UNVERIFIED; (4)
unit, integration, deterministic, edge-case and long-duration categories present (500
years at 256 with a detailed region, 500 years of alternation at 64 with every system);
(5) ADR-0040 to ADR-0047, docs updated. Verdict: **Phase 05 VALIDATED on the headless
side, UNVERIFIED on the engine side until the first UE 5.6 build.**

## Phase 06 task breakdown (canonical numbering: `Docs/ROADMAP.md` section 10)

| Task | Content | Status |
|---|---|---|
| 06.01 | `VaelenEconomy` module, goods as kinds, RegionStock and HouseStock, the endowment, split and fold across the grains, AddStock | VALIDATED: Stocks 4 tests |
| 06.02 | ProductionSystem: harvest by workers and farming cut by droughts, spoilage, meals by house, the ration observed by the needs, deposits, craft | VALIDATED: Production 4 tests |
| 06.03 | MarketSystem: a market per region, integer prices from wanted over held within a floor and a ceiling, price events with the harvest as cause, ValueOf | VALIDATED: Markets 4 tests |
| 06.04 | TradeSystem: routes between neighbouring markets on price gaps, goods carried cheap to dear, idle routes closed, settlements founded and abandoned | VALIDATED: Trade 4 tests |
| 06.05 | WealthSystem: houses valued and ranked at their market, the rank weighing in standing, heirs by descent custom, goods passing to the heir | VALIDATED: Wealth 5 tests |
| 06.06 | The economy across the grains: wealth and heirs cleared with the grain, conservation to the unit over 500 years, every invariant in a living world | VALIDATED: Grains 2 tests |
| 06.07 | EconomyChronicle: roads, towns, prices at their bounds, shortfalls, fortunes and inheritances recorded; a line for every economic event; the why of a dear loaf | VALIDATED: EconomyHistory 3 tests |
| 06.08 | Phase 06 gate: 500 years at 256 with every Phase 04, 05 and 06 system, invariants every decade, frozen at 250 and 500 with the log and the text | VALIDATED: EconomyGate 1 test |

## Phase 07 task breakdown (canonical numbering: `Docs/ROADMAP.md` section 11)

| Task | Content | Status |
|---|---|---|
| 07.01 | `VaelenPolitics` module, polities as entities founded on councils, the seat and the ruler, belonging written on the region itself | VALIDATED: Polities 4 tests |
| 07.02 | Law as one number on the polity, written onto the regions as dues the economy observes; the grain taken into a treasury | VALIDATED: Law 4 tests |
| 07.03 | Authority written on each region, falling with distance from the seat; the upkeep of a word; ground taken and ground that slips | VALIDATED: Reach 5 tests |
| 07.04 | The line a polity remembers, the claimant the descent custom names, and the unrest a seat taken by anyone else costs its hold | VALIDATED: Succession 4 tests |
| 07.05 | Factions as entities of their own kind: a grievance with a place and sometimes a person, gathering while unanswered and taking the ground at its threshold | VALIDATED: Factions 4 tests |
| 07.06 | Relations as entities of their own kind, warmed and cooled by the world two polities share; a war puts the weaker border in play and the reach system takes it | VALIDATED: Diplomacy 4 tests |
| 07.07 | A sentence for every political event and a record only for what a century would remember | VALIDATED: PoliticsHistory 3 tests |
| 07.08 | Phase 07 gate: 500 years at 256 with two powers and every Phase 04 to 07 system, invariants every decade, frozen at 250 and 500 with the log and the text | VALIDATED: PoliticsGate 1 test |

Phase 07 against the exit criteria (`Docs/ROADMAP.md` section 2): (1) CI matrix green for 07.01 to 07.07 (runs 60 to 67; runs 60 and 65 were superseded on the same headless tree), 07.08 by its own run; (2) determinism tests for every system, frozen digests per task and for the gate at 250 and 500 years, reproduced by clang, gcc, MSVC and AppleClang; (3) no INCOMPLETE file, engine files UNVERIFIED; (4) unit, integration, deterministic, edge-case and long-duration categories present (500 years at 256 with two detailed regions); (5) ADR-0056 to ADR-0063, docs updated. Verdict: **Phase 07 VALIDATED on the headless side, UNVERIFIED on the engine side until the next UE 5.6 build.**

## Phase 08 task breakdown (canonical numbering: `Docs/ROADMAP.md` section 12)

| Task | Content | Status |
|---|---|---|
| 08.01 | `VaelenMilitary` module; a levy raised from the regions a polity holds and fed out of its treasury; armies as entities of kind Army | VALIDATED: Armies 4 tests |
| 08.02 | Marching: a host walks the region graph towards the nearest enemy ground, one hop a season, eats off what it stands on and loosens an enemy ruler's grip | VALIDATED: March 4 tests |
| 08.03 | Battle: two hosts of powers at war on one region settled in a year by strength, whose ground it is, and a stream fixed by the world seed | VALIDATED: Battle 4 tests |
| 08.04 | Siege: a host sitting before an enemy seat, the only way a capital ever changes hands, and the polity that loses one ends | VALIDATED: Siege 4 tests |
| 08.05 | War as a thing with a beginning and an end: opened by a relation turning, held open while it runs, closed by exhaustion, with the stance of 07.06 following it | VALIDATED: War 4 tests |
| 08.06 | What war costs the living: the dead where they came from, the standing of those who came back, the people who would not stay | VALIDATED: Toll 4 tests |
| 08.07 | War in the chronicle: a sentence for every military event, the few a century keeps, and the why of a lost province walked back to the battle | VALIDATED: MilitaryHistory 4 tests |
| 08.08 | Phase 08 gate: five centuries at 256 with every Phase 04 to 08 system, every invariant each decade, a snapshot replayed, four frozen digests | VALIDATED: MilitaryGate 1 test |

## Phase 09 task breakdown (canonical numbering: `Docs/ROADMAP.md` section 13)

| Task | Content | Status |
|---|---|---|
| 09.01 | `VaelenInfrastructure` module; buildings as entities of kind Building raised out of a region's common stock and the hands it can spare, what they cost taken in the log | PROTOTYPE: Buildings 4 tests |
| 09.02 | What a building does: the granary of 05.05, the harvest of 06.02, the wall of 08.04, each as the one number that phase already reads | PROTOTYPE: Works 5 tests |
| 09.03 | Settlements as places: a tile of their own, a size from the people and the traffic, and the region's works standing in them | PROTOTYPE: Places 4 tests |
| 09.04 | Roads: a route of 06.04 cut out of the two ends together, carrying more, worn when nobody keeps it, fallen back to a track | PROTOTYPE: Roads 5 tests |
| 09.05 | Decay and ruins | PLANNED |
| 09.06 | Logistics: what a road is worth to 08.02 and to 07.03 | PLANNED |
| 09.07 | Infrastructure in the chronicle | PLANNED |
| 09.08 | Phase 09 gate | PLANNED |

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
| `VaelenCore/Private/VaelenCoreModule.cpp` | VALIDATED (UE 5.6, 2026-09-07) |
| `VaelenCore/VaelenCore.Build.cs` | VALIDATED (UE 5.6, 2026-09-07) |
| `Vaelen.Target.cs`, `VaelenEditor.Target.cs` | no STATUS line; `VaelenEditor` built 2026-09-07, the game target only had its rules assembly compiled |
| `Vaelen/Vaelen.Build.cs`, `Vaelen/Public/Vaelen.h`, `Vaelen/Private/Vaelen.cpp`, `VaelenLogSink.h/.cpp` | VALIDATED (UE 5.6, 2026-09-07) |

### Source/VaelenSim (Phase 01)

| File | STATUS |
|---|---|
| `Public/Vaelen/Sim/SimApi.h`, `PlainData.h`, `EntityHandle.h`, `EntityRegistry.h`, `ComponentType.h`, `ComponentPool.h`, `ComponentStore.h`, `SimClock.h`, `System.h`, `Event.h`, `EventBus.h`, `Archive.h`, `World.h`, `Snapshot.h`, `Private/EntityRegistry.cpp`, `ComponentType.cpp`, `ComponentStore.cpp`, `Scheduler.cpp`, `EventBus.cpp`, `Archive.cpp`, `World.cpp`, `Snapshot.cpp` | VALIDATED (Phase 01) — integration and long-duration tests arrive with 01.07 / 01.08 |
| `Private/VaelenSimModule.cpp`, `VaelenSim.Build.cs` | VALIDATED (UE 5.6, 2026-09-07) |
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
| `VaelenPopulation.Build.cs`, `Private/VaelenPopulationModule.cpp` | VALIDATED (UE 5.6, 2026-09-07) — engine-side, not compiled headless |
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
| `VaelenSociety.Build.cs`, `Private/VaelenSocietyModule.cpp` | VALIDATED (UE 5.6, 2026-09-07) — engine-side, not compiled headless |
| `CMakeLists.txt` | VALIDATED (Phase 05) |
| `Public/Vaelen/Society/SocietyApi.h` | VALIDATED (Phase 05) |
| `Public/Vaelen/Society/Organizations.h`, `Private/Organizations.cpp` | VALIDATED (Phase 05) — covered by `Tests/Society/Test_Organizations.cpp` |
| `Public/Vaelen/Society/Standing.h`, `Private/Standing.cpp` | VALIDATED (Phase 05) — covered by `Tests/Society/Test_Standing.cpp` |
| `Public/Vaelen/Society/Norms.h`, `Private/Norms.cpp` | VALIDATED (Phase 05) — covered by `Tests/Society/Test_Norms.cpp` |
| `Public/Vaelen/Society/BondState.h`, `Public/Vaelen/Society/Bondage.h`, `Private/Bondage.cpp` | VALIDATED (Phase 05) — covered by `Tests/Society/Test_Bondage.cpp` |
| `Public/Vaelen/Society/Decisions.h`, `Private/Decisions.cpp` | VALIDATED (Phase 05) — covered by `Tests/Society/Test_Decisions.cpp` |
| `Public/Vaelen/Society/SocietyHistory.h`, `Private/SocietyHistory.cpp` | VALIDATED (Phase 05) — covered by `Tests/Society/Test_SocietyHistory.cpp` |

### `Source/VaelenEconomy` (Phase 06)

| File | Status |
|---|---|
| `VaelenEconomy.Build.cs`, `Private/VaelenEconomyModule.cpp` | VALIDATED (UE 5.6, 2026-09-07) — engine-side, not compiled headless |
| `CMakeLists.txt` | VALIDATED (Phase 06) |
| `Public/Vaelen/Economy/EconomyApi.h` | VALIDATED (Phase 06) |
| `Public/Vaelen/Economy/Stocks.h`, `Private/Stocks.cpp` | VALIDATED (Phase 06) — covered by `Tests/Economy/Test_Stocks.cpp` |
| `Public/Vaelen/Economy/Production.h`, `Private/Production.cpp` | VALIDATED (Phase 06) — covered by `Tests/Economy/Test_Production.cpp` |
| `Public/Vaelen/Economy/Markets.h`, `Private/Markets.cpp` | VALIDATED (Phase 06) — covered by `Tests/Economy/Test_Markets.cpp` |
| `Public/Vaelen/Economy/Trade.h`, `Private/Trade.cpp` | VALIDATED (Phase 06) — covered by `Tests/Economy/Test_Trade.cpp` |
| `Public/Vaelen/Economy/Wealth.h`, `Private/Wealth.cpp` | VALIDATED (Phase 06) — covered by `Tests/Economy/Test_Wealth.cpp` |
| `Public/Vaelen/Economy/EconomyHistory.h`, `Private/EconomyHistory.cpp` | VALIDATED (Phase 06) — covered by `Tests/Economy/Test_EconomyHistory.cpp` |

### `Source/VaelenMilitary` (Phase 08)

| File | Status |
|---|---|
| `VaelenMilitary.Build.cs`, `Private/VaelenMilitaryModule.cpp` | UNVERIFIED — engine-side, not compiled headless, and newer than the first Unreal build |
| `CMakeLists.txt` | VALIDATED (Phase 08) |
| `Public/Vaelen/Military/MilitaryApi.h` | VALIDATED (Phase 08) |
| `Public/Vaelen/Military/Armies.h`, `Private/Armies.cpp` | VALIDATED (Phase 08) — covered by `Tests/Military/Test_Armies.cpp` |
| `Public/Vaelen/Military/March.h`, `Private/March.cpp` | VALIDATED (Phase 08) — covered by `Tests/Military/Test_March.cpp` |
| `Public/Vaelen/Military/Battle.h`, `Private/Battle.cpp` | VALIDATED (Phase 08) — covered by `Tests/Military/Test_Battle.cpp` |
| `Public/Vaelen/Military/Siege.h`, `Private/Siege.cpp` | VALIDATED (Phase 08) — covered by `Tests/Military/Test_Siege.cpp` |
| `Public/Vaelen/Military/War.h`, `Private/War.cpp` | VALIDATED (Phase 08) — covered by `Tests/Military/Test_War.cpp` |
| `Public/Vaelen/Military/Toll.h`, `Private/Toll.cpp` | VALIDATED (Phase 08) — covered by `Tests/Military/Test_Toll.cpp` |
| `Public/Vaelen/Military/MilitaryHistory.h`, `Private/MilitaryHistory.cpp` | VALIDATED (Phase 08) — covered by `Tests/Military/Test_MilitaryHistory.cpp` |

### `Source/Vaelen` (engine bridge and presentation)

| File | Status |
|---|---|
| `Vaelen.Build.cs`, `Private/Vaelen.cpp`, `Private/VaelenLogSink.h/.cpp`, `Public/Vaelen.h` | VALIDATED under UBT (UE 5.6, 2026-09-07, editor) |
| `Public/VaelenAtlasActor.h`, `Private/VaelenAtlasActor.cpp` | VALIDATED under UBT (UE 5.6, 2026-09-08) — compiled clean and run through the `Vaelen.Atlas` console command, which generated AELVOR 128 and reported it |

### `Source/Vaelen` (engine bridge and presentation)

| File | Status |
|---|---|
| `Vaelen.Build.cs`, `Private/Vaelen.cpp`, `Private/VaelenLogSink.h/.cpp`, `Public/Vaelen.h` | VALIDATED under UBT (UE 5.6, 2026-09-07, editor) |
| `Public/VaelenAtlasActor.h`, `Private/VaelenAtlasActor.cpp` | UNVERIFIED — engine-side, not compiled headless, and newer than the first Unreal build |

### `Source/VaelenPolitics` (Phase 07)

| File | Status |
|---|---|
| `VaelenPolitics.Build.cs`, `Private/VaelenPoliticsModule.cpp` | UNVERIFIED — engine-side, not compiled headless, and newer than the first Unreal build |
| `CMakeLists.txt` | VALIDATED (Phase 07) |
| `Public/Vaelen/Politics/PoliticsApi.h` | VALIDATED (Phase 07) |
| `Public/Vaelen/Politics/Polities.h`, `Private/Polities.cpp` | VALIDATED (Phase 07) — covered by `Tests/Politics/Test_Polities.cpp` |
| `Public/Vaelen/Politics/Law.h`, `Private/Law.cpp` | VALIDATED (Phase 07) — covered by `Tests/Politics/Test_Law.cpp` |
| `Public/Vaelen/Politics/Reach.h`, `Private/Reach.cpp` | VALIDATED (Phase 07) — covered by `Tests/Politics/Test_Reach.cpp` |
| `Public/Vaelen/Politics/Succession.h`, `Private/Succession.cpp` | VALIDATED (Phase 07) — covered by `Tests/Politics/Test_Succession.cpp` |
| `Public/Vaelen/Politics/Factions.h`, `Private/Factions.cpp` | VALIDATED (Phase 07) — covered by `Tests/Politics/Test_Factions.cpp` |
| `Public/Vaelen/Politics/Diplomacy.h`, `Private/Diplomacy.cpp` | VALIDATED (Phase 07) — covered by `Tests/Politics/Test_Diplomacy.cpp` |
| `Public/Vaelen/Politics/PoliticsHistory.h`, `Private/PoliticsHistory.cpp` | VALIDATED (Phase 07) — covered by `Tests/Politics/Test_PoliticsHistory.cpp` |
| `Public/Vaelen/Politics/Law.h`, `Private/Law.cpp` | VALIDATED (Phase 07) — covered by `Tests/Politics/Test_Law.cpp` |
| `Public/Vaelen/Politics/Law.h`, `Private/Law.cpp` | VALIDATED (Phase 07) — covered by `Tests/Politics/Test_Law.cpp` |

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
| `Sim/Test_Regions.cpp` (Phase 02) | VALIDATED | 6 |
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
| `Population/Test_Persons.cpp` (Phase 04) | VALIDATED | 6 |
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
| `Society/Test_Strata.cpp` (Phase 05) | VALIDATED | 3 |
| `Society/Test_SocietyHistory.cpp` (Phase 05) | VALIDATED | 3 |
| `Society/Test_SocietyGate.cpp` (Phase 05) | VALIDATED | 1 |
| `Economy/Test_Stocks.cpp` (Phase 06) | VALIDATED | 4 |
| `Economy/Test_Production.cpp` (Phase 06) | VALIDATED | 4 |
| `Economy/Test_Markets.cpp` (Phase 06) | VALIDATED | 4 |
| `Economy/Test_Trade.cpp` (Phase 06) | VALIDATED | 4 |
| `Economy/Test_Wealth.cpp` (Phase 06) | VALIDATED | 5 |
| `Economy/Test_Grains.cpp` (Phase 06) | VALIDATED | 2 |
| `Economy/Test_EconomyHistory.cpp` (Phase 06) | VALIDATED | 3 |
| `Economy/Test_EconomyGate.cpp` (Phase 06) | VALIDATED | 1 |
| `Politics/Test_Polities.cpp` (Phase 07) | VALIDATED | 4 |
| `Politics/Test_Law.cpp` (Phase 07) | VALIDATED | 4 |
| `Politics/Test_Reach.cpp` (Phase 07) | VALIDATED | 5 |
| `Politics/Test_Succession.cpp` (Phase 07) | VALIDATED | 4 |
| `Politics/Test_Factions.cpp` (Phase 07) | VALIDATED | 4 |
| `Politics/Test_Diplomacy.cpp` (Phase 07) | VALIDATED | 4 |
| `Politics/Test_PoliticsHistory.cpp` (Phase 07) | VALIDATED | 3 |
| `Politics/Test_PoliticsGate.cpp` (Phase 07) | VALIDATED | 1 |
| `Military/Test_Armies.cpp` (Phase 08) | VALIDATED | 4 |
| `Military/Test_March.cpp` (Phase 08) | VALIDATED | 4 |
| `Military/Test_Battle.cpp` (Phase 08) | VALIDATED | 4 |
| `Military/Test_Siege.cpp` (Phase 08) | VALIDATED | 4 |
| `Military/Test_War.cpp` (Phase 08) | VALIDATED | 4 |
| `Military/Test_Toll.cpp` (Phase 08) | VALIDATED | 4 |
| `Military/Test_MilitaryHistory.cpp` (Phase 08) | VALIDATED | 4 |
| `Military/Test_MilitaryGate.cpp` (Phase 08) | VALIDATED | 1 |
| `Infrastructure/Test_Buildings.cpp` (Phase 09) | PROTOTYPE | 4 |
| `Infrastructure/Test_Works.cpp` (Phase 09) | PROTOTYPE | 5 |
| `Infrastructure/Test_Places.cpp` (Phase 09) | PROTOTYPE | 4 |
| `Infrastructure/Test_Roads.cpp` (Phase 09) | PROTOTYPE | 5 |

Per-suite counts: Assert 33, CoreTypes 1, Harness 5, Hash 15, Ids 19, Log 23, LogFloor 1, Random 29, Version 7 (133 tests with assertions, 108 without). CTest entries: `Kernel.Purity`, `Kernel.PuritySelfTest`, `Core.Assert`, `Core.CoreTypes`, `Core.Harness`, `Core.Hash`, `Core.Ids`, `Core.Log`, `Core.LogFloor`, `Core.Random`, `Core.Version`, `Core.Registry`, `Core.Shuffled`, `Core.Reversed` (14 entries). Sim suites: EntityHandle 3, EntityRegistry 13, ComponentType 4, ComponentPool 8, ComponentStore 3, SimClock 4, Scheduler 8, Event 2, EventLog 2, EventBus 6, Archive 4, World 3, Snapshot 8, Replay 5, MiniWorld 4, TileGrid 4, WorldMap 6, FixedPoint 4, Noise 5, WorldGen 6, Climate 6, Hydrology 5, Regions 6, Deposits 5, WorldPipeline 4, History 3, Population 5, Naming 5, Religion 5, Disasters 5, PreHistory 5, HistoryText 5, HistoryGate 2 (161 tests; 158 tests without assertions); CTest entries `Sim.EntityHandle`, `Sim.EntityRegistry`, `Sim.ComponentType`, `Sim.ComponentPool`, `Sim.ComponentStore`, `Sim.SimClock`, `Sim.Scheduler`, `Sim.Event`, `Sim.EventLog`, `Sim.EventBus`, `Sim.Archive`, `Sim.World`, `Sim.Snapshot`, `Sim.Replay`, `Sim.MiniWorld`, `Sim.TileGrid`, `Sim.WorldMap`, `Sim.FixedPoint`, `Sim.Noise`, `Sim.WorldGen`, `Sim.Climate`, `Sim.Hydrology`, `Sim.Regions`, `Sim.Deposits`, `Sim.WorldPipeline`, `Sim.History`, `Sim.Population`, `Sim.Naming`, `Sim.Religion`, `Sim.Disasters`, `Sim.PreHistory`, `Sim.HistoryText`, `Sim.HistoryGate`, `Sim.Registry`, `Sim.Shuffled` (42 entries in total). Population suites: Persons 6, Lives 5, Families 5, Needs 6, Traits 5, Lod 5, PersonHistory 5, PopulationGate 1 (38 tests; 38 without assertions); CTest entries `Population.Persons`, `Population.Lives`, `Population.Families`, `Population.Needs`, `Population.Traits`, `Population.Lod`, `Population.PersonHistory`, `Population.PopulationGate`, `Population.Registry`, `Population.Shuffled` (10 entries). Society suites: Organizations 5, Standing 4, Norms 4, Bondage 4, Decisions 4, Strata 3, SocietyHistory 3, SocietyGate 1 (28 tests; 28 without assertions); CTest entries `Society.Organizations`, `Society.Standing`, `Society.Norms`, `Society.Bondage`, `Society.Decisions`, `Society.Strata`, `Society.SocietyHistory`, `Society.SocietyGate`, `Society.Registry`, `Society.Shuffled` (10 entries). Economy suites: Stocks 4, Production 4, Markets 4, Trade 4, Wealth 5, Grains 2, EconomyHistory 3, EconomyGate 1 (27 tests; 27 without assertions); CTest entries `Economy.Stocks`, `Economy.Production`, `Economy.Markets`, `Economy.Trade`, `Economy.Wealth`, `Economy.Grains`, `Economy.EconomyHistory`, `Economy.EconomyGate`, `Economy.Registry`, `Economy.Shuffled` (10 entries). Politics suites: Polities 4, Law 4, Reach 5, Succession 4, Factions 4, Diplomacy 4, PoliticsHistory 3, PoliticsGate 1 (29 tests; 29 without assertions); CTest entries `Politics.Diplomacy`, `Politics.Factions`, `Politics.Law`, `Politics.Polities`, `Politics.PoliticsGate`, `Politics.PoliticsHistory`, `Politics.Reach`, `Politics.Succession`, `Politics.Registry`, `Politics.Shuffled` (10 entries). Military suites: Armies 4, Battle 4, March 4, MilitaryGate 1, MilitaryHistory 4, Siege 4, Toll 4, War 4 (29 tests; 29 without assertions); CTest entries `Military.Armies`, `Military.Battle`, `Military.March`, `Military.MilitaryGate`, `Military.MilitaryHistory`, `Military.Siege`, `Military.Toll`, `Military.War`, `Military.Registry`, `Military.Shuffled` (10 entries). Infrastructure suites: Buildings 4, Works 5, Places 4, Roads 5 (18 tests; 18 without assertions); CTest entries `Infrastructure.Buildings`, `Infrastructure.Works`, `Infrastructure.Places`, `Infrastructure.Roads`, `Infrastructure.Registry`, `Infrastructure.Shuffled` (6 entries).

### Tools/ and CI

| File | STATUS |
|---|---|
| `Tools/check_kernel_purity.py` | VALIDATED (Phase 00): self-test 36 checks, kernel scan 12 files, 0 violations, 2 exemptions (the `long long` aliases) |
| `Tools/kernel_modules.txt` | lists `VaelenCore` |
| `.github/workflows/kernel-ci.yml` | All 9 jobs green on GitHub (run 5): six Linux presets, `format`, Windows MSVC, macOS |

### `Source/VaelenInfrastructure` (Phase 09)

| File | Status |
|---|---|
| `VaelenInfrastructure.Build.cs`, `Private/VaelenInfrastructureModule.cpp` | UNVERIFIED — engine-side, not compiled headless, and newer than the first Unreal build |
| `CMakeLists.txt` | PROTOTYPE (Phase 09) |
| `Public/Vaelen/Infrastructure/InfrastructureApi.h` | PROTOTYPE (Phase 09) |
| `Public/Vaelen/Infrastructure/Buildings.h`, `Private/Buildings.cpp` | PROTOTYPE (Phase 09) — covered by `Tests/Infrastructure/Test_Buildings.cpp` |
| `Public/Vaelen/Infrastructure/Works.h`, `Private/Works.cpp` | PROTOTYPE (Phase 09) — covered by `Tests/Infrastructure/Test_Works.cpp` |
| `Public/Vaelen/Infrastructure/Places.h`, `Private/Places.cpp` | PROTOTYPE (Phase 09) — covered by `Tests/Infrastructure/Test_Places.cpp` |
| `Public/Vaelen/Infrastructure/Roads.h`, `Private/Roads.cpp` | PROTOTYPE (Phase 09) — covered by `Tests/Infrastructure/Test_Roads.cpp` |

## Verified here

Toolchain: clang++ 18.1.3, g++ 13.3.0, CMake 3.28.3, Ninja 1.11.1, Python 3.11.15, clang-format 18.1.3, Linux x86_64. Every preset was configured, built and tested with
`cmake --preset`, `cmake --build --preset`, `ctest --preset` into `out/build/<preset>`:

| Preset | Build | `ctest` | `VaelenCoreTests` | `VaelenSimTests` | `VaelenPopulationTests` | `VaelenSocietyTests` | `VaelenEconomyTests` | `VaelenPoliticsTests` | `VaelenMilitaryTests` |
|---|---|---|---|---|---|---|---|---|---|
| linux-clang-debug | 0 warnings | 92/92 passed | 133 run, 133 passed, 22427 checks | 161 run, 161 passed | 37 run, 37 passed | 27 run, 27 passed | 26 run, 26 passed | 29 run, 29 passed | 29 run, 29 passed |
| linux-gcc-debug | 0 warnings | 92/92 passed | 133 run, 133 passed, 22427 checks | 161 run, 161 passed | 37 run, 37 passed | 27 run, 27 passed | 26 run, 26 passed | 29 run, 29 passed | 29 run, 29 passed |
| linux-clang-release | 0 warnings | 92/92 passed | 133 run, 133 passed, 22427 checks | 161 run, 161 passed | 37 run, 37 passed | 27 run, 27 passed | 26 run, 26 passed | 29 run, 29 passed | 29 run, 29 passed |
| linux-gcc-release | 0 warnings | 92/92 passed | 133 run, 133 passed, 22427 checks | 161 run, 161 passed | 37 run, 37 passed | 27 run, 27 passed | 26 run, 26 passed | 29 run, 29 passed | 29 run, 29 passed |
| linux-clang-noasserts | 0 warnings | 92/92 passed | 108 run, 108 passed, 22214 checks | 158 run, 158 passed | 37 run, 37 passed | 27 run, 27 passed | 26 run, 26 passed | 29 run, 29 passed | 29 run, 29 passed |
| linux-gcc-noasserts | 0 warnings | 92/92 passed | 108 run, 108 passed, 22214 checks | 158 run, 158 passed | 37 run, 37 passed | 27 run, 27 passed | 26 run, 26 passed | 29 run, 29 passed | 29 run, 29 passed |

Since Phase 08 closed, a task inside a phase is verified on the two presets that actually find things - `linux-clang-debug` (assertions on) and `linux-gcc-release` (optimised, `-Werror`) - plus the purity checker and clang-format; the six-preset matrix above is re-run in full at the phase gate. **09.01**: both presets green, 88/88 CTest entries each (gates and shuffled re-runs excluded), `VaelenInfrastructureTests` 4 run / 4 passed / 90 checks on both, purity 139 files 0 violations, 0 formatting drift. **09.02**: both presets green, 89/89 each, `VaelenInfrastructureTests` 9 run / 9 passed, purity 141 files 0 violations, 0 formatting drift - and every frozen digest of Phases 04 to 08 unchanged, which is the point: the four hooks read a factor of one where nothing is built. **09.03**: both presets green, 90/90 each, `VaelenInfrastructureTests` 13 run / 13 passed, purity 143 files 0 violations, 0 formatting drift. **09.04**: both presets green, 91/91 each, `VaelenInfrastructureTests` 18 run / 18 passed, purity 145 files 0 violations, 0 formatting drift.

Mini-world baseline (100 000 ticks, 41 entities, 305 027 events, 34 168 227-byte snapshot), logged by `Sim.MiniWorld`, not asserted: clang debug 0.39 s (255 k ticks/s), gcc debug 0.40 s, clang release 0.135 s (739 k ticks/s), gcc release without assertions 0.127 s (790 k ticks/s); snapshot 0.09-0.14 s.

GitHub Actions runs 13 to 28 (01.06 through 03.03): all 9 jobs green each; run 29 (03.04) red on Windows MSVC only (a dangling pool pointer in the faith listener changed the religion digest, and `Sim.Shuffled` exceeded its 300 s CTest timeout), both fixed in the 03.05 commit and green again in runs 30 to 48 (03.05 to 05.07); run 49 (05.08) was cancelled by the job timeouts - the Phase 05 gate took the serial debug test runs past 30 minutes on Linux and 45 on Windows (every Linux release and no-assert job green) - so from 06.01 CTest runs 4 jobs in every preset (`execution.jobs` in `CMakePresets.json`, the stdio capture entries serialised by a resource lock), which brings a debug run under ten minutes - green again in runs 50 to 68 (06.01 to 07.08 and the atlas actor; the first Unreal build at run 54 included; runs 56 and 60 were superseded by 57 and 61 on the same headless tree); so the frozen replay, mini-world and snapshot values hold on Windows MSVC and macOS AppleClang as well. Phase 00 record - run 5 (commit `71bad2d`, https://github.com/Thomas10112/vaelen/actions/runs/33977296696): all 9 jobs green - six Linux presets, clang-format 18, Windows MSVC 19.44 (`windows-msvc-debug`, 14/14 CTest entries), macOS 15 AppleClang (`macos-debug`, 14/14).

Also run locally: `python3 Tools/check_kernel_purity.py --self-test` (36 checks, 0 failed),
`python3 Tools/check_kernel_purity.py --root . --verbose` (12 files, 0 violations),
`clang-format --style=file --dry-run -Werror` on every kernel and test source (0 drift).

### Engine (development PC, 2026-09-07)

Toolchain: UE 5.6.1 (`5.6.1-44394996+++UE5+Release-5.6`), MSVC 14.51.36256 from Visual
Studio 2026 Community (the only x64 toolchain installed; UBT calls it "Visual Studio 2022
compiler version 14.51.36256 is not a preferred version" and uses it anyway, so no
`BuildConfiguration.xml` override was needed), Windows SDK 10.0.26100.0, Windows 11
Enterprise 10.0.26200, .NET 8.0.300 bundled with the engine.

| Step | Result |
|---|---|
| `Build.bat -projectfiles` | Succeeded, 14 s |
| `Build.bat VaelenEditor Win64 Development` (first) | Failed: `C2280` on `ComponentStore`, then `LNK2019/LNK2001` on 7 kernel symbols |
| `Rebuild.bat VaelenEditor Win64 Development` (after the fixes) | Succeeded, 77 actions, 65 s, 0 errors, 654 C4251 + 42 C4996 warnings |
| `Binaries/Win64` | `UnrealEditor-Vaelen{Core,Sim,Population,Society,Economy}.dll` + `UnrealEditor-Vaelen.dll` |
| Editor opened on `Vaelen.uproject` | `LogVaelen: VAELEN 0.0.1 - kernel save format v3 - kernel asserts on - module started` |
| `check_kernel_purity.py` after the fixes (UE's bundled Python 3) | 102 files, 0 violations |
| `clang-format 22.1.3 -i --style=file` on every edited source | 0 drift |

## Unverified

- The `Vaelen` game target: only `VaelenEditor` was built on 2026-09-07, so the
  monolithic path (every `VAELEN_<MODULE>_API` empty) has never been linked. Nor was any
  configuration other than Development: the `VAELEN_ASSERTS_ENABLED=0` and
  `VAELEN_LOG_COMPILED_MIN_LEVEL=2` branches of the `Build.cs` files stay unexercised
  under UBT. `Docs/ARCHITECTURE.md` section 8.3 keeps the list.
- Engine-backed CI: none. The first engine build was run by hand on the development PC,
  once; nothing re-runs it.
- The headless suites against the four kernel headers edited for that build
  (`ComponentStore.h`, `Population.h`, `Religion.h`, `Regions.h`): the development PC has
  no CMake, no Ninja and no system Python 3, so only the purity checker was re-run there
  (102 files, 0 violations, through the Python bundled with UE). The GitHub matrix is
  what confirms the 240+ tests and the frozen digests.
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
