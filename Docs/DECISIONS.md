# VAELEN Architecture Decision Records

STATUS: VALIDATED (Phase 00) - every statement about current code was checked against
the sources on branch `claude/vaelen-master-prompt-aw7zqj` on 2026-09-05; the builds
and test runs listed in [Verification record](#verification-record) were executed for
this document. Statements about alternatives that were not chosen are engineering
rationale, not verified code.

Purpose: one record per architecture decision that later phases must not silently
undo. Each record has five parts: Context (the forces), Decision (what is in the code),
Alternatives and decision rule (what was rejected and which criterion of the project's
decision rule decided: robustness, then architectural simplicity, performance,
evolvability, determinism), Consequences (what it costs and what it enables), Status
(accepted / superseded, and the validation state of the implementation).

**Three records in this file are PROPOSED and await the project owner's
decision: ADR-0111, ADR-0118 and ADR-0120.** They are defects found, measured
and deliberately not fixed, because fixing them changes what the world is.
`Docs/OPEN_QUESTIONS.md` states each as a choice - what it costs to leave it,
what it costs to change it, and the one sentence that unblocks it - so that
reading the three records in full is optional.

Rules for this file:

- Numbering is append-only. A record is never deleted or renumbered; a reversed decision
  gets a new record and the old one is marked `Superseded by ADR-nnnn`.
- Every record names the files that embody it, so a reader can check it against the code.
- A change that breaks a frozen value (random sequences, hashes, id layout, save format)
  requires a new record and a `VAELEN_SAVE_FORMAT_VERSION` bump (see `Version.h`).

| ADR | Title | Status |
|---|---|---|
| [0001](#adr-0001-engine-agnostic-simulation-kernel-with-dual-build-ubt--cmake) | Engine-agnostic simulation kernel with dual build (UBT + CMake) | Accepted; headless VALIDATED, engine side UNVERIFIED |
| [0002](#adr-0002-no-exceptions-no-rtti-in-the-kernel) | No exceptions, no RTTI in the kernel | Accepted; VALIDATED |
| [0003](#adr-0003-xoshiro256-seeded-by-splitmix64-hierarchical-named-random-streams) | xoshiro256** seeded by SplitMix64, hierarchical named random streams | Accepted; VALIDATED |
| [0004](#adr-0004-persistentid-856-bit-layout-monotonic-never-reused-serials-allocator-state-in-world-state) | PersistentId 8/56-bit layout, monotonic never-reused serials, allocator state in world state | Accepted; VALIDATED |
| [0005](#adr-0005-printf-style-logging-instead-of-stdformat) | printf-style logging instead of std::format | Accepted; VALIDATED |
| [0006](#adr-0006-dependency-free-in-house-test-harness) | Dependency-free in-house test harness | Accepted; VALIDATED |
| [0007](#adr-0007-bitmask-with-rejection-for-unbiased-integer-ranges) | Bitmask-with-rejection for unbiased integer ranges | Accepted; VALIDATED |
| [0008](#adr-0008-kernel-purity-enforced-by-a-ctest) | Kernel purity enforced by a CTest | Accepted; VALIDATED |
| [0009](#adr-0009-floating-point-policy-no-contraction-integers-for-authoritative-state) | Floating-point policy: no contraction, integers for authoritative state | Accepted; headless VALIDATED, engine side UNVERIFIED |
| [0010](#adr-0010-runtime-entity-handles-with-generations-dense-registry-lifo-slot-reuse) | Runtime entity handles with generations, dense registry, LIFO slot reuse | Accepted; headless VALIDATED, engine side UNVERIFIED |
| [0011](#adr-0011-components-are-plain-data-in-typed-sparse-sets-registered-explicitly) | Components are plain data in typed sparse sets, registered explicitly | Accepted; headless VALIDATED, engine side UNVERIFIED |
| [0012](#adr-0012-systems-are-ordered-by-declared-dependencies-with-a-name-hash-tie-break-each-gets-a-per-tick-derived-stream) | Systems ordered by declared dependencies with a name-hash tie-break; per-tick derived streams | Accepted; headless VALIDATED, engine side UNVERIFIED |
| [0013](#adr-0013-simulation-time-is-an-integer-tick-count-the-calendar-is-data-derived-from-it) | Simulation time is an integer tick count; the calendar is data derived from it | Accepted; headless VALIDATED, engine side UNVERIFIED |
| [0014](#adr-0014-events-are-plain-112-byte-records-with-a-cause-delivered-next-tick-in-publish-order-the-log-is-append-only-with-a-running-digest) | Events are plain records with a cause, delivered next tick; append-only log with running digest | Accepted; headless VALIDATED, engine side UNVERIFIED |
| [0015](#adr-0015-one-world-object-owns-the-state-snapshots-are-a-symmetric-versioned-digest-checked-byte-image-stored-types-carry-no-padding) | One World object owns the state; snapshots are a symmetric, versioned, digest-checked byte image; stored types carry no padding | Accepted; headless VALIDATED, engine side UNVERIFIED |
| [0016](#adr-0016-tiles-are-dense-typed-layers-not-entities-the-world-map-is-a-state-block-with-a-code-declared-layer-set) | Tiles are dense typed layers, not entities; the world map is a state block with a code-declared layer set | Accepted; headless VALIDATED, engine side UNVERIFIED |
| [0017](#adr-0017-world-generation-uses-q3232-fixed-point-and-integer-lattice-noise-no-floating-point-no-libm) | World generation uses Q32.32 fixed point and integer lattice noise; no floating point, no libm | Accepted; headless VALIDATED, engine side UNVERIFIED |
| [0018](#adr-0018-world-generation-is-a-pipeline-of-pure-stages-with-derived-seeds-a-32-slot-parameter-block-and-a-sea-bounded-continent) | World generation is a pipeline of pure stages with derived seeds, a 32-slot parameter block and a sea-bounded continent | Accepted; headless VALIDATED, engine side UNVERIFIED |
| [0019](#adr-0019-climate-is-a-row-wise-advection-model-with-resolution-independent-decay-and-a-threshold-biome-table) | Climate is a row-wise advection model with resolution-independent decay and a threshold biome table | Accepted; headless VALIDATED, engine side UNVERIFIED |
| [0020](#adr-0020-hydrology-fills-depressions-by-priority-flood-fills-shallow-basins-with-sediment-and-keeps-deep-ones-as-lakes-rivers-and-lakes-are-entities) | Hydrology fills depressions by priority flood, fills shallow basins with sediment and keeps deep ones as lakes; rivers and lakes are entities | Accepted; headless VALIDATED, engine side UNVERIFIED |
| [0021](#adr-0021-regions-grow-from-lattice-seeds-by-terrain-cost-with-a-merge-floor-the-adjacency-graph-is-derived-not-stored) | Regions grow from lattice seeds by terrain cost with a merge floor; the adjacency graph is derived, not stored | Accepted; headless VALIDATED, engine side UNVERIFIED |
| [0022](#adr-0022-deposits-come-from-an-explicit-suitability-table-hashed-draws-and-one-per-kind-per-cell-spacing) | Deposits come from an explicit suitability table, hashed draws and one-per-kind-per-cell spacing | Accepted; headless VALIDATED, engine side UNVERIFIED |
| [0023](#adr-0023-the-world-is-a-function-of-seed-and-config-through-one-pipeline-call-frozen-as-whole-world-digests-at-three-sizes) | The world is a function of seed and config through one pipeline call, frozen as whole-world digests at three sizes | Accepted; headless VALIDATED, engine side UNVERIFIED |
| [0024](#adr-0024-history-is-eras-opened-by-span-or-caused-request-plus-a-chronicle-of-record-entities-with-every-piece-of-state-in-components) | History is eras opened by span or caused request plus a chronicle of Record entities, with every piece of state in components | Accepted; headless VALIDATED, engine side UNVERIFIED |
| [0025](#adr-0025-population-is-coarse-per-region-cultures-are-entities-and-a-culture-splits-by-graph-distance-with-lineage-spacing) | Population is coarse per region, cultures are entities, and a culture splits by graph distance with lineage spacing | Accepted; headless VALIDATED, engine side UNVERIFIED |
| [0026](#adr-0026-names-are-built-from-a-per-language-phonology-pronounceable-by-construction-unique-per-scope-and-stored-as-fixed-size-components) | Names are built from a per-language phonology, pronounceable by construction, unique per scope, and stored as fixed-size components | Accepted; headless VALIDATED, engine side UNVERIFIED |
| [0027](#adr-0027-religions-are-entities-born-from-a-founding-event-with-believers-per-region-bounded-by-its-people-spreading-along-the-graph-and-with-migration) | Religions are entities born from a founding event, with believers per region bounded by its people, spreading along the graph and with migration | Accepted; headless VALIDATED, engine side UNVERIFIED |
| [0028](#adr-0028-disasters-are-drawn-from-world-hazards-announced-by-an-omen-a-year-ahead-and-caused-by-it-with-consequences-through-the-existing-request-doors) | Disasters are drawn from world hazards, announced by an omen a year ahead and caused by it, with consequences through the existing request doors | Accepted; headless VALIDATED, engine side UNVERIFIED |
| [0029](#adr-0029-the-pre-history-is-one-object-that-owns-the-phase-03-systems-and-one-call-on-a-fresh-world-frozen-per-century-as-the-starting-state) | The pre-history is one object that owns the Phase 03 systems and one call on a fresh world, frozen per century as the starting state | Accepted; headless VALIDATED, engine side UNVERIFIED |
| [0030](#adr-0030-history-is-queried-through-the-log-and-the-records-and-read-as-text-built-from-names-with-deterministic-fallbacks) | History is queried through the log and the records, and read as text built from names with deterministic fallbacks | Accepted; headless VALIDATED, engine side UNVERIFIED |
| [0031](#adr-0031-believers-never-exceed-the-living-at-any-tick-boundary-carries-round-up-and-deaths-take-believers-first) | Believers never exceed the living at any tick boundary: carries round up and deaths take believers first | Accepted; headless VALIDATED, engine side UNVERIFIED |
| [0032](#adr-0032-population-has-two-grains-and-one-truth-persons-exist-only-in-detailed-regions-and-always-sum-to-the-coarse-counts) | Population has two grains and one truth: persons exist only in detailed regions and always sum to the coarse counts | Accepted; headless VALIDATED, engine side UNVERIFIED |
| [0033](#adr-0033-in-a-detailed-region-the-persons-drive-the-counts-and-the-coarse-systems-observe-an-opt-in-lod-marker) | In a detailed region the persons drive the counts and the coarse systems observe an opt-in LOD marker | Accepted; headless VALIDATED, engine side UNVERIFIED |
| [0034](#adr-0034-families-are-entities-founded-by-grooms-lineage-is-read-from-the-parent-links-and-children-are-born-to-couples) | Families are entities founded by grooms, lineage is read from the parent links, and children are born to couples | Accepted; headless VALIDATED, engine side UNVERIFIED |
| [0035](#adr-0035-needs-are-yearly-integers-fed-by-the-regions-ration-and-disasters-reach-persons-through-the-event-log) | Needs are yearly integers fed by the region's ration, and disasters reach persons through the event log | Accepted; headless VALIDATED, engine side UNVERIFIED |
| [0036](#adr-0036-traits-are-drawn-from-the-identity-and-the-parents-skills-are-earned-year-by-year-and-persons-are-named-by-their-language) | Traits are drawn from the identity and the parents, skills are earned year by year, and persons are named by their language | Accepted; headless VALIDATED, engine side UNVERIFIED |
| [0037](#adr-0037-detail-is-requested-not-decided-by-the-kernel-and-people-cross-the-grain-border-as-events) | Detail is requested, not decided by the kernel, and people cross the grain border as events | Accepted; headless VALIDATED, engine side UNVERIFIED |
| [0038](#adr-0038-persons-enter-the-chronicle-through-a-capped-listener-that-decides-what-matters-at-dispatch) | Persons enter the chronicle through a capped listener that decides what matters at dispatch | Accepted; headless VALIDATED, engine side UNVERIFIED |
| [0039](#adr-0039-the-phase-04-gate-runs-every-population-system-over-the-256-pre-history-and-freezes-the-state-at-250-and-500-years) | The Phase 04 gate runs every population system over the 256 pre-history and freezes the state at 250 and 500 years | Accepted; headless VALIDATED, engine side UNVERIFIED |
| [0040](#adr-0040-organisations-are-entities-seated-in-a-region-filled-from-its-persons-and-kept-as-counts-when-the-region-is-coarse) | Organisations are entities seated in a region, filled from its persons, and kept as counts when the region is coarse | Accepted; headless VALIDATED, engine side UNVERIFIED |
| [0041](#adr-0041-standing-is-recomputed-every-year-from-what-the-world-already-knows-and-tiers-are-shares-of-a-regions-adults) | Standing is recomputed every year from what the world already knows, and tiers are shares of a region's adults | Accepted; headless VALIDATED, engine side UNVERIFIED |
| [0042](#adr-0042-norms-live-on-the-culture-and-reach-the-lower-modules-through-a-small-mirror-they-choose-to-observe) | Norms live on the culture and reach the lower modules through a small mirror they choose to observe | Accepted; headless VALIDATED, engine side UNVERIFIED |
| [0043](#adr-0043-bondage-is-a-state-on-the-person-with-a-living-holder-among-the-elite-and-a-count-per-region) | Bondage is a state on the person with a living holder among the elite, and a count per region | Accepted; headless VALIDATED, engine side UNVERIFIED |
| [0044](#adr-0044-organisations-act-through-one-yearly-system-whose-effects-reach-the-lower-modules-as-state-they-observe-or-as-events-they-ignore) | Organisations act through one yearly system whose effects reach the lower modules as state they observe or as events they ignore | Accepted; headless VALIDATED, engine side UNVERIFIED |
| [0045](#adr-0045-the-social-shape-of-a-region-is-a-count-that-outlives-its-persons-and-binds-the-next-ones) | The social shape of a region is a count that outlives its persons and binds the next ones | Accepted; headless VALIDATED, engine side UNVERIFIED |
| [0046](#adr-0046-each-module-chronicles-its-own-events-through-its-own-capped-listener-into-the-one-record-store) | Each module chronicles its own events through its own capped listener into the one record store | Accepted; headless VALIDATED, engine side UNVERIFIED |
| [0047](#adr-0047-the-phase-05-gate-runs-every-population-and-society-system-over-the-256-pre-history-and-freezes-the-state-the-log-and-the-text) | The Phase 05 gate runs every population and society system over the 256 pre-history and freezes the state, the log and the text | Accepted; headless VALIDATED, engine side UNVERIFIED |
| [0048](#adr-0048-goods-are-counted-kinds-held-in-common-by-regions-and-by-the-houses-of-a-detailed-region-and-conserved-across-the-grains) | Goods are counted kinds, held in common by regions and by the houses of a detailed region, and conserved across the grains | Accepted; headless VALIDATED, engine side UNVERIFIED |
| [0049](#adr-0049-the-harvest-is-made-in-both-grains-by-the-same-rule-and-hunger-follows-the-grain-through-a-ration-the-need-system-observes) | The harvest is made in both grains by the same rule, and hunger follows the grain through a ration the need system observes | Accepted; headless VALIDATED, engine side UNVERIFIED |
| [0050](#adr-0050-a-price-is-an-integer-on-the-region-from-what-it-wants-over-what-it-holds-within-a-floor-and-a-ceiling) | A price is an integer on the region, from what it wants over what it holds, within a floor and a ceiling | Accepted; headless VALIDATED, engine side UNVERIFIED |
| [0051](#adr-0051-a-route-is-an-entity-between-two-neighbouring-markets-opened-by-a-price-gap-a-want-and-a-surplus-carrying-goods-one-way-and-closed-when-idle) | A route is an entity between two neighbouring markets, opened by a price gap, a want and a surplus, carrying goods one way, and closed when idle | Accepted; headless VALIDATED, engine side UNVERIFIED |
| [0052](#adr-0052-wealth-reaches-standing-as-a-rank-not-an-amount-and-an-extinct-houses-goods-follow-the-cultures-descent-custom) | Wealth reaches standing as a rank, not an amount, and an extinct house's goods follow the culture's descent custom | Accepted; headless VALIDATED, engine side UNVERIFIED |
| [0053](#adr-0053-nothing-of-the-fine-grain-outlives-it-a-coarse-region-keeps-counts-and-stocks-and-nothing-else) | Nothing of the fine grain outlives it: a coarse region keeps counts and stocks, and nothing else | Accepted; headless VALIDATED, engine side UNVERIFIED |
| [0054](#adr-0054-the-chronicle-records-what-lasts-a-road-built-a-town-risen-a-price-at-its-bound-a-fortune-moved-not-the-churn-beneath-them) | The chronicle records what lasts: a road built, a town risen, a price at its bound, a fortune moved, not the churn beneath them | Accepted; headless VALIDATED, engine side UNVERIFIED |
| [0055](#adr-0055-when-two-validated-orderings-cannot-both-hold-the-chain-that-carries-the-grain-wins-and-the-describer-of-the-topmost-layer-speaks-for-all-of-them) | When two validated orderings cannot both hold, the chain that carries the grain wins, and the describer of the topmost layer speaks for all of them | Accepted; headless VALIDATED, engine side UNVERIFIED |
| [0056](#adr-0056-a-polity-is-an-entity-seated-in-a-council-and-a-region-remembers-whose-it-is) | A polity is an entity seated in a council, and a region remembers whose it is | Accepted; headless VALIDATED, engine side UNVERIFIED |
| [0057](#adr-0057-a-law-is-one-number-on-the-polity-and-a-plain-struct-on-the-region) | A law is one number on the polity and a plain struct on the region | Accepted; headless VALIDATED |
| [0058](#adr-0058-authority-is-written-on-the-region-and-falls-with-the-walk-from-the-seat) | Authority is written on the region and falls with the walk from the seat | Accepted; headless VALIDATED |
| [0059](#adr-0059-succession-is-judged-not-decided) | Succession is judged, not decided | Accepted; headless VALIDATED |
| [0060](#adr-0060-a-faction-takes-ground-never-the-throne) | A faction takes ground, never the throne | Accepted; headless VALIDATED |
| [0061](#adr-0061-a-relation-is-a-fact-of-the-ground-and-a-war-is-a-permission-not-an-order) | A relation is a fact of the ground, and a war is a permission, not an order | Accepted; headless VALIDATED |
| [0062](#adr-0062-history-is-narrower-than-the-log-and-the-topmost-describer-speaks-for-every-layer) | History is narrower than the log, and the topmost describer speaks for every layer | Accepted; headless VALIDATED |
| [0063](#adr-0063-a-seat-cannot-be-taken) | A seat cannot be taken | Accepted; headless VALIDATED |
| [0064](#adr-0064-an-army-is-people-taken-out-of-regions) | An army is people taken out of regions | Accepted; headless VALIDATED |
| [0065](#adr-0065-a-host-walks-the-region-graph-and-eats-what-it-stands-on) | A host walks the region graph, and eats what it stands on | Accepted; headless VALIDATED |
| [0066](#adr-0066-a-battle-is-settled-in-one-year-by-numbers-ground-and-a-stream) | A battle is settled in one year by numbers, ground and a stream | Accepted; headless VALIDATED |
| [0067](#adr-0067-a-seat-is-taken-only-by-sitting-in-front-of-it) | A seat is taken only by sitting in front of it | Accepted; headless VALIDATED |
| [0068](#adr-0068-the-war-is-the-thing-and-the-stance-follows-it) | The war is the thing, and the stance follows it | Accepted; headless VALIDATED |
| [0069](#adr-0069-a-derived-cache-is-keyed-on-what-it-was-derived-from) | A derived cache is keyed on what it was derived from | Accepted; headless VALIDATED |
| [0070](#adr-0070-a-person-index-is-never-handed-out-twice) | A person index is never handed out twice | Accepted; headless VALIDATED |
| [0071](#adr-0071-the-numbers-land-on-people) | The numbers land on people | Accepted; headless VALIDATED |
| [0072](#adr-0072-a-chronicle-is-the-small-part-a-century-keeps) | A chronicle is the small part a century keeps | Accepted; headless VALIDATED |
| [0073](#adr-0073-a-gate-is-where-the-phase-finds-out-what-it-got-wrong) | A gate is where the phase finds out what it got wrong | Accepted; headless VALIDATED |

---

## ADR-0001: Engine-agnostic simulation kernel with dual build (UBT + CMake)

### Context

VAELEN is a UE5 project (`Vaelen.uproject`, engine association 5.6) whose value is a
deterministic living-world simulation, not engine features. The project layering is
SIMULATION -> WORLD STATE -> GAMEPLAY -> PRESENTATION -> DIALOGUE/NARRATION, with the
simulation as the single source of truth and no write access from rendering, dialogue or
any LLM. The simulation needs unit, determinism, edge-case and long-duration tests, plus
stress tests, on every commit. An Unreal build needs a licensed engine installation, a
large runner and minutes to hours per build; none of that exists for this repository
(no engine-backed CI runner, see `Docs/ARCHITECTURE.md` section 7.2).

### Decision

The simulation kernel is written in pure standard C++20 and compiled twice from the same
files:

- by Unreal Build Tool as the runtime module `VaelenCore` (`Source/VaelenCore/VaelenCore.Build.cs`:
  `PCHUsage = NoPCHs`, `bUseUnity = false`, `bEnableExceptions = false`, `bUseRTTI = false`,
  `CppStandardVersion.Cpp20`, dependency on `Core` only, `PublicDefinitions` adds
  `VAELEN_UNREAL_BUILD=1`);
- by CMake as the static library `VaelenCore` / `Vaelen::Core` (`/CMakeLists.txt`,
  `Source/VaelenCore/CMakeLists.txt`), with `VAELEN_HEADLESS_BUILD=1` and
  `VAELEN_ASSERTS_ENABLED` set from the option `VAELEN_ENABLE_ASSERTS`.

The export macro is owned by the kernel: `CoreTypes.h` defines `VAELEN_CORE_API` from
`VAELEN_CORE_EXPORTS` (private definition of the module in modular builds) and
`VAELEN_CORE_IMPORTS` (public definition for dependants), empty otherwise. UBT's
generated `VAELENCORE_API=DLLEXPORT` is never used, because `DLLEXPORT` is only defined
by `HAL/Platform.h`, which the kernel never includes. In `VaelenCore.Build.cs` the
assertion switch and the compile-time log floor are likewise defined from the target
configuration, since UBT defines `NDEBUG` in every non-debug-CRT configuration.

Kernel files include only `"Vaelen/..."` headers and non-banned standard headers
(`Tools/check_kernel_purity.py`, rule R1). The single Unreal-facing translation unit of a
kernel module is `<Module>Module.cpp` (`Source/VaelenCore/Private/VaelenCoreModule.cpp`,
`IMPLEMENT_MODULE`), listed nowhere in CMake and skipped by the purity checker. Everything
that touches the engine lives in Unreal-only modules (`Source/Vaelen`: `FVaelenModule`
installs a kernel log sink and assertion handler). The kernel provides its own
replacements for the engine facilities it cannot use: `CoreTypes.h` (fixed-width
aliases), `Log.h`, `Assert.h`, `Hash.h`, `Random.h`, `Ids.h`, `Version.h`.

The layering rule is enforced structurally: a kernel module cannot include an upper
layer (R1 admits only `Vaelen/` and standard headers), so nothing above the simulation
can be a compile-time dependency of it; upper layers reach the simulation only through
the kernel's public API (command interface PLANNED, Phase 01/10).

### Consequences

- Tests and CI run without an engine: `.github/workflows/kernel-ci.yml` builds and runs
  `ctest` on Linux (clang and gcc, Debug and RelWithDebInfo), Windows (MSVC) and macOS
  (AppleClang) from `CMakePresets.json`. Headless stress and long-duration runs are
  possible with the same binaries (none exist yet; PLANNED, Phases 01 and 18).
- Every kernel feature is written once and must satisfy both toolchains: no Unreal
  containers, strings, logging, assertions, math or serialisation inside the kernel;
  the kernel's own primitives (ADR-0003 to ADR-0005) are the only ones available.
- Two build descriptions must be kept in sync by hand: `Source/VaelenCore/CMakeLists.txt`
  lists sources explicitly; UBT globs the module directory. A file added to one and not
  the other compiles in one build only.
- The engine side has no automated verification. `VaelenCoreModule.cpp`, `Source/Vaelen/**`,
  `*.Build.cs`, `*.Target.cs` and `Vaelen.uproject` are labelled `STATUS: UNVERIFIED` and
  have never been compiled in this repository's history.
- Data handed to Unreal (positions, names, ids) will need a bridge with explicit
  conversions in the Unreal modules (PLANNED, Phase 13). The kernel never depends on
  the presentation's data model.

### Alternatives and decision rule

- A kernel that includes Unreal headers and is tested only through Unreal Automation:
  rejected for robustness and simplicity (every test needs an engine install, no CI
  without a licensed runner, no headless stress tests).
- A separately built third-party library linked into UE as `PublicAdditionalLibraries`:
  rejected for simplicity (per-platform prebuilt binaries to maintain). Compiling the
  same sources twice keeps one source of truth.
- Decided by robustness (testability without the engine), then simplicity.

### Status

Accepted. Headless build VALIDATED: clang++ 18.1.3 and g++ 13.3.0, six presets,
14/14 CTest entries, 133/133 tests (108/108 without assertions), 0 warnings (see
[Verification record](#verification-record)). Engine build UNVERIFIED. Verified against:
`/CMakeLists.txt`, `Source/VaelenCore/CMakeLists.txt`, `Source/VaelenCore/VaelenCore.Build.cs`,
`Source/Vaelen/Vaelen.Build.cs`, `Source/Vaelen/Private/Vaelen.cpp`, `Vaelen.uproject`,
`.github/workflows/kernel-ci.yml`, `CMakePresets.json`.

---

## ADR-0002: No exceptions, no RTTI in the kernel

### Context

Unreal Engine compiles game code without C++ exceptions and without RTTI; the module
rules in `VaelenCore.Build.cs` set `bEnableExceptions = false` and `bUseRTTI = false`.
Code that is compiled both by UBT and headless (ADR-0001) must have one semantics, so the
headless build cannot rely on facilities the engine build does not have. Exceptions also
introduce control flow that is hard to make deterministic and hard to test in a build
where the same `throw` is a compile error.

### Decision

Both builds disable exceptions and RTTI: GCC/Clang `-fno-exceptions -fno-rtti`; MSVC
`/GR-`, `_HAS_EXCEPTIONS=0`, `/EHsc` stripped from `CMAKE_CXX_FLAGS` and replaced by the
explicit `/EHs-c-` (plus `/wd4577`, since MSVC otherwise warns on every `noexcept`)
(`/CMakeLists.txt`, `vaelen_build_flags`); UBT flags as above. The purity checker rejects `throw`, `try {`,
`catch (` (R2), `dynamic_cast`, `typeid` (R3) and the headers `<exception>`,
`<stdexcept>`, `<typeinfo>`, `<typeindex>`, `<csetjmp>`, `<setjmp.h>` (R1).

Error handling in the kernel therefore uses three mechanisms only (all present in the
code):

1. Assertions for programming errors: `VAELEN_CHECK`, `VAELEN_CHECKF`, `VAELEN_VERIFY`
   (`Assert.h`), reported through a pluggable handler and fatal by default
   (`Assert.cpp`, `DefaultHandler` writes to stderr, logs, then calls
   `Detail::AbortProcess`, which flushes stdio and calls `std::abort()`, SIGABRT).
2. "Check, then guard": after a failed check the function still returns a safe value,
   because a handler may return (the tests install one). `IdAllocator::Allocate`
   returns `PersistentId::Invalid()`; `RandomStream::Below(0)` returns 0;
   `IdAllocator::ReserveUpTo` returns `false`.
3. Return values for expected failures: `bool` (`Log::AddSink`, `Log::RemoveSink`,
   `IdAllocator::ReserveUpTo`), `VAELEN_ENSURE` yielding its boolean, sentinel values
   (`PersistentId::Invalid()`).

Polymorphism uses explicit interfaces with virtual destructors (`ILogSink`); type
identification uses explicit enums (`IdKind`, `AssertKind`, `LogLevel`).

### Consequences

- Identical behaviour in both builds; no hidden unwinding paths; smaller binaries.
- Standard-library operations that would throw (allocation failure, `std::vector::at`)
  terminate the process instead. The kernel must validate inputs before calling them.
- Third-party test frameworks built around exceptions cannot be used as-is (ADR-0006);
  `VT_REQUIRE` returns from the test function instead of throwing.
- The abort itself cannot be unit-tested in-process: `VAELEN_UNREACHABLE()` is proven to
  compile and to terminate control flow (`Test_Assert.cpp`, `SignOf`); the default
  handler's reporting path is tested through an `Ensure` report
  (`Assert.DefaultHandlerLogsEnsureAndContinues`).
- Every fallible kernel API documents its failure value in its header comment; callers
  must check it. No `errno`-style globals, no `std::optional` in public APIs yet (allowed).

### Alternatives and decision rule

- Exceptions and RTTI enabled in the headless build only: rejected for robustness (two
  semantics for the same code, `throw` a compile error in one build).
- `std::expected`/error codes for programming errors: rejected for simplicity; they are
  used for expected failures, assertions for invariants.
- Decided by robustness (identical behaviour in both builds).

### Status

Accepted; VALIDATED. Verified against: `/CMakeLists.txt`, `Source/VaelenCore/VaelenCore.Build.cs`,
`Source/VaelenCore/Public/Vaelen/Core/Assert.h`, `Source/VaelenCore/Private/Assert.cpp`,
`Source/VaelenCore/Private/Ids.cpp`, `Source/VaelenCore/Private/Random.cpp`,
`Tools/check_kernel_purity.py` (R1-R3), `Tests/Core/Test_Assert.cpp` (33 tests with
assertions enabled, 23 with assertions disabled).

---

## ADR-0003: xoshiro256** seeded by SplitMix64, hierarchical named random streams

### Context

The determinism rule (same seed + same inputs = same result) applies to a world with
dozens of simulation systems and thousands of per-region or per-entity random consumers,
across Linux, Windows and macOS compilers and across the engine and headless builds.
Adding a random draw to one system must not change any other system's results; stream
state must be saved with the world; floating-point draws must not depend on the FPU or
the C library; and the reference algorithm must be simple enough to re-implement
independently in a test.

### Decision

`Vaelen::RandomStream` (`Random.h`, `Random.cpp`):

- Generator: xoshiro256** (Blackman and Vigna). `NextU64` is the published update; state
  is `RandomStreamState{uint64 Seed; uint64 S[4]; uint64 DrawCount}` (48 bytes), saved
  and restored verbatim (`GetState`/`SetState`, constructor from state).
- Seeding: `Reseed(Seed)` fills `S[0..3]` with four consecutive `SplitMix64Next` outputs
  and guards against the all-zero state; the state constructor and `SetState` apply
  the same guard and report a violated invariant with `VAELEN_ENSURE`.
- Hierarchy by name: `Derive(std::string_view Name)` = `Derive(HashString(Name))` =
  `RandomStream(HashCombine(HashCombine(Seed, DeriveByNameSalt), NameHash))`, salt
  `0x5641454c454e2d4e` ("VAELEN-N"). Hierarchy by index: `Fork(uint64 Index)` with salt
  `0x5641454c454e2d49` ("VAELEN-I"). Both depend only on the parent's root seed and never
  advance the parent. Names hash at compile time with `"hydrology"_vhash` (`Hash.h`).
- `Jump()` advances the generator by 2^128 draws (published jump polynomial) for cases
  where derivation does not apply.
- Draws: `NextU64`, `NextU32` (high half), `Below` (ADR-0007), `RangeInclusive[U]`,
  `NextDouble` (top 53 bits times 2^-53), `NextFloat` (top 24 bits), `RangeDouble`,
  `Chance` (no draw at p <= 0 or p >= 1), `NextNormal` (Marsaglia polar, no cached
  second variate, so the draw count is a pure function of the calls).

Rationale for the generator (engineering judgement, not code):

- Not `std::mt19937` / `mt19937_64`: 624 x 32-bit words of state (about 2.5 KB) per
  stream, which matters when every system, region and entity may own a stream that is
  saved with the world; documented statistical weaknesses (linearity) in the TestU01
  literature; and while the engine's raw output is specified by the standard, the
  `<random>` distributions are not, so `std::uniform_int_distribution` gives different
  numbers on libstdc++, libc++ and MSVC. `<random>` is banned in the kernel (R1).
- Not PCG: the 64-bit-output PCG variants use 128-bit state and multiplication, which
  needs `__int128` or emulation on MSVC; the 32-bit-output variant halves the width of
  every draw. xoshiro256** has 32 bytes of state, a period of 2^256 - 1, a published
  jump function and public-domain reference code that `Test_Random.cpp` transcribes
  independently to verify the kernel.

Rationale for names rather than indices: an index ties a system's sequence to a
registration order or an enum position, so inserting one system changes every world
generated afterwards; a name hashes to the same 64-bit value forever (FNV-1a 64, frozen by
`Test_Hash.cpp`), is readable in logs and saves, and composes hierarchically
(`Derive("a").Derive("b")` differs from `Derive("b").Derive("a")` and from
`Derive("ab")`, tested). `Fork(Index)` is kept for instances whose index is itself world
state (region number, entity serial).

### Consequences

- A draw added or removed in one system cannot perturb another system's sequence, and a
  stream can be reconstructed from the root seed plus a path of names.
- The exact sequence for a given seed is frozen. Changing the generator, the seeding, a
  salt, `HashString`, `HashCombine` or `Mix64` changes every generated world and every
  derived stream; `Test_Hash.HashCombineFrozenValues` and the known-answer tests in
  `Test_Random.cpp` fail on purpose in that case, and the only legitimate fix is a new
  save-format version with a migration.
- `DrawCount` counts internal `NextU64` calls, not logical operations: `Jump()` advances
  it by 256, rejection sampling (ADR-0007) and `NextNormal` consume a variable number
  per call. It is a diagnostic, not a position.
- `NextDouble`/`NextFloat` are bit-identical everywhere (integer construction).
  `NextNormal` uses `std::log`/`std::sqrt`; cross-platform bit-exactness is not promised
  for it (`Docs/CONVENTIONS.md` section 6.9).
- Name collisions are not detected at runtime: two systems deriving the same name share a
  stream silently. `Test_Hash.DistinctNamesGiveDistinctHashes` covers a fixed list only.
- A default-constructed stream is seed 0: valid, but simulation code must always pass an
  explicit seed (header comment).

### Status

Accepted; VALIDATED. Verified against: `Source/VaelenCore/Public/Vaelen/Core/Random.h`,
`Source/VaelenCore/Private/Random.cpp`, `Source/VaelenCore/Public/Vaelen/Core/Hash.h`,
`Tests/Core/Test_Random.cpp` (23 tests: SplitMix64 and xoshiro256** known answers
against independent reference code, 1000-draw agreement, jump agreement, derivation and
fork properties, state round trip), `Tests/Core/Test_Hash.cpp` (15 tests). Note the
header/implementation inconsistency recorded in ADR-0007.

---

## ADR-0004: PersistentId 8/56-bit layout, monotonic never-reused serials, allocator state in world state

### Context

Persons, families, settlements, items, events and documents must be addressable across
ticks, across save and load, and after they are dead or destroyed: history, records,
inheritance and dialogue refer to things that no longer exist ("HISTORY BELONGS TO NO
ONE" applies to identifiers as well). Identifiers appear in the event log and in saves,
so they must be deterministic (same seed and inputs give the same ids), compact,
comparable, hashable and self-describing enough to debug.

### Decision

`Vaelen::PersistentId` (`Ids.h`): a `uint64` with the layout
`[8 bits IdKind][56 bits serial]`, `KindBits = 8`, `SerialBits = 56`,
`MaxSerial = 2^56 - 1`, serial 0 reserved so that `Value == 0` is `Invalid()`; kind
`None` is invalid whatever the serial. `constexpr`, trivially copyable, standard layout,
8 bytes (`static_assert`), `operator<=>` on the raw value (kind is the major key),
`Hash()` = `Mix64(Value)` and a `std::hash` specialisation. `IdKind : uint8` values are
part of the save format: append only, never renumbered, ranges reserved per future
module (1-2 core, 10-13 world, 20-25 history and society, 30-34 economy and
infrastructure, 40-43 politics and military, 50-51 knowledge).

`Vaelen::IdAllocator` (`Ids.h`, `Ids.cpp`): one monotonic counter per kind,
`State{std::array<uint64, 256> NextSerial}` (2 KB), initialised to 1, exposed with
`GetState`/`SetState` so it is saved and loaded with the world. `Allocate(Kind)` returns
`Make(Kind, Next++)`; on kind `None` or when `Next > MaxSerial` it reports a Check
failure and returns `Invalid()` without touching the counter, so serials never wrap and
no id is ever reused. `ReserveUpTo` raises a counter above imported serials; `PeekNext`
and `GetAllocatedCount` are read-only; `SetState` sanitises zero counters to 1; `Reset`
restarts every counter. Not thread-safe by design: allocation happens on the simulation
thread in a deterministic order.

Alternatives considered (rationale, not code):

- Pointers or pointer-based handles: not stable across save/load, not deterministic
  (allocator behaviour, address-space randomisation), unable to refer to destroyed
  objects, and unsafe.
- Slot index plus generation counter: fast for dense component storage but slots are
  reused and generations wrap, so a handle is meaningless in a historical record. It is
  kept as a *separate runtime concept* for Phase 01 (`Ids.h` header comment), mapped to
  and from `PersistentId` by a registry.
- 128-bit GUIDs: twice the size, need a random source (either non-deterministic or an
  extra stream for no benefit), carry no kind and no creation order, and cannot be
  counted per kind.

### Consequences

- 8-byte ids that serialise as raw bytes, work as map keys (`std::unordered_map<PersistentId, ...>`
  tested), sort by kind then by creation order, and print as kind name plus serial
  (`IdKindToString`).
- Determinism by construction: ids depend only on the allocator state and the order of
  `Allocate` calls, which must itself be deterministic (single simulation thread, or a
  fixed order when systems run in parallel, PLANNED).
- 2^56 ids per kind is practically unlimited; exhaustion is nevertheless handled
  explicitly and tested (`Ids.AllocatorExhaustionTriggersCheck`).
- At most 255 usable kinds; the enum is a save-format contract, so a kind can be
  deprecated but never removed.
- An id does not locate storage. A registry mapping ids to runtime slots is required
  (PLANNED, Phase 01, task 01.01).
- The allocator state must be part of every snapshot and checkpoint (PLANNED, task
  01.06); a snapshot without it would reuse ids after restore.

### Status

Accepted; VALIDATED. Verified against: `Source/VaelenCore/Public/Vaelen/Core/Ids.h`,
`Source/VaelenCore/Private/Ids.cpp`, `Tests/Core/Test_Ids.cpp` (17 tests: layout,
masking, invalid ids, ordering, hashing and map keys, kind names, sequential and
independent counters, peek, reserve, reset, state round trip, sanitising, determinism
over 5000 allocations, None and exhaustion assertion paths).

---

## ADR-0005: printf-style logging instead of std::format

### Context

The kernel needs logging and formatted assertion messages that compile under UBT and
headless (ADR-0001) on MSVC, GCC, Clang and AppleClang. `Log.h` records the constraint
that motivated the decision: the libc++ bundled with Unreal's Linux toolchain does not
guarantee `std::format`, and the three CI compilers had to agree on the same formatting
code. Logging is also on the path of assertion failures, where allocation and heavy
machinery are undesirable.

### Decision

Logging is printf-style (`Log.h`, `Log.cpp`):

- `Log::Write(const LogCategory&, LogLevel, File, Line, const char* Format, ...)` formats
  with `vsnprintf` into a 2048-byte stack buffer, builds a `LogRecord` (category, level,
  message, file, line) and delivers it to every registered sink under one mutex.
- Format strings are checked at compile time on GCC/Clang through
  `VAELEN_PRINTF_ATTR(5, 6)` (`__attribute__((format(printf, ...)))`, defined in
  `Assert.h`) together with `-Wformat=2 -Werror`; on MSVC the attribute expands to
  nothing.
- Macros `VAELEN_LOG_TRACE/DEBUG/INFO/WARNING/ERROR/FATAL(Category, Format, ...)` apply a
  compile-time floor (`VAELEN_LOG_COMPILED_MIN_LEVEL`), the category threshold and the
  global minimum before formatting, so arguments are evaluated only for delivered records.
- Assertion messages use the same scheme: `Detail::ReportAssertF(..., Format, ...)` with
  a 1024-byte buffer (`Assert.cpp`).
- Sinks (`ILogSink`) receive fully formatted text; the Unreal module forwards it to
  `UE_LOG`, the test runner to stdout/stderr (`StdioLogSink`).

### Consequences

- Works on every toolchain in the matrix without `<format>`; no allocation on the logging
  or assertion path; records are plain `const char*` for any sink.
- Compile-time format checking exists on GCC/Clang only. A format error that only MSVC
  would see is not caught; the Linux CI legs are the first line of defence, and the
  Linux build must stay green before the Windows one is trusted.
- Fixed-width integers cross the boundary through `static_cast<long long>` / `%lld` and
  `static_cast<unsigned long long>` / `%llu` (the only place bare `long` is allowed,
  purity rule R7).
- Messages longer than 2047 (log) or 1023 (assert) characters are truncated silently;
  both limits are tested (`Log.LongMessageIsTruncatedSafely`,
  `Assert.LongMessageIsTruncatedSafely`).
- No formatting of user-defined types: a `PersistentId` is printed as
  `IdKindToString(Kind)` plus its serial by the caller; no helper exists yet.
- Moving to `std::format` later would change every call site; it is only worth doing once
  every toolchain in the matrix and the engine's Linux toolchain guarantee it.

### Alternatives and decision rule

- `std::format`: rejected for robustness (not guaranteed in the libc++ shipped with
  Unreal's Linux toolchain at the time of the decision) and portability parity across
  MSVC, GCC and Clang.
- A custom type-safe formatter: rejected for simplicity in Phase 00; printf formats are
  compile-time checked (`-Wformat=2`) and, independently of compiler flags, forced to be
  string literals by the macros.
- Decided by robustness, then simplicity.

### Status

Accepted; VALIDATED. Verified against: `Source/VaelenCore/Public/Vaelen/Core/Log.h`,
`Source/VaelenCore/Private/Log.cpp`, `Source/VaelenCore/Public/Vaelen/Core/Assert.h`,
`Source/VaelenCore/Private/Assert.cpp`, `Tests/Core/Test_Log.cpp` (20 tests, including
an 8-thread dispatch test), `Tests/Core/Test_Assert.cpp`. The claim about the engine's
Linux toolchain is the rationale recorded in `Log.h`; it was not re-verified against an
engine installation (none is available here).

---

## ADR-0006: Dependency-free in-house test harness

### Context

Tests must compile under exactly the kernel's flags (`-Werror` with the full warning set,
`-fno-exceptions -fno-rtti`, MSVC `/WX /GR-`) on four toolchains, without a package
manager, and must be able to intercept the kernel's assertion handler. GoogleTest and
Catch2 are designed around exceptions for their fatal-assertion path and bring their own
build systems and warning profiles; carrying either through UBT and through the four
headless toolchains would mean maintaining a third-party build for a small feature set.
The project also wants one test binary per kernel module with precise CTest attribution
and, later, the option to run the same tests from Unreal Automation (noted in
`Config/DefaultEditor.ini`).

### Decision

`Tests/Harness/VaelenTest.h` and `Tests/Harness/TestMain.cpp`, no external dependency:

- `VAELEN_TEST(Suite, Name)` defines a function and registers a `TestCase` through a
  static `Registrar` into an intrusive singly linked list, preserving declaration order
  within a translation unit.
- Checks: `VT_CHECK`, `VT_CHECK_MSG`, `VT_REQUIRE` (records and `return`s),
  `VT_CHECK_EQ`, `VT_REQUIRE_EQ`, `VT_CHECK_NE`, `VT_CHECK_STREQ`, `VT_CHECK_NEAR`;
  failures print file, line, expression and both values.
- `VaelenTest::ScopedAssertCapture` installs a kernel assertion handler that counts
  Check and Ensure reports and records the last expression and message, so assertion
  paths are testable without aborting; its destructor calls `SetAssertHandler(nullptr)`.
- Runner: `VaelenCoreTests [--suite Name] [--filter Substring] [--list] [--verbose]
  [--quiet-log]`; exit codes 0 (all passed), 1 (failures), 2 (usage), 3 (no test
  matched); kernel log output is silenced unless `--verbose`.
- CTest mapping: `Tests/Core/CMakeLists.txt` globs `Test_*.cpp` and registers
  `Core.<Suite>` running `--suite <Suite>`; `Tests/CMakeLists.txt` adds `Kernel.Purity`.

### Consequences

- Zero dependencies; the harness compiles under the kernel flags (Linux clang and gcc
  observed; the other legs are CI-defined) and is about 300 lines that the project owns.
- Integrated assertion capture; a single fast binary per module; one CTest entry per
  suite so a failure names the suite directly.
- Missing on purpose: fixtures, parametrised tests, matchers, death tests, timeouts,
  XML/JUnit output, test sharding. Death tests are impossible anyway (ADR-0002).
- Tests must be independent and restore every global they touch; registration order
  across translation units is unspecified. `Test_Log.cpp` restores sinks and the global
  level by hand.
- A suite name that does not match its file stem yields a CTest entry that matches zero
  tests; this is detected only at `ctest` time (exit 3).
- `Test_Harness.cpp` is not guarded by `VAELEN_ASSERTS_ENABLED`: with
  `-DVAELEN_ENABLE_ASSERTS=OFF` its `AssertCaptureDoesNotAbort` test fails (observed, see
  [Verification record](#verification-record)). No CI preset builds that configuration.

### Alternatives and decision rule

- GoogleTest or Catch2: rejected for robustness (both assume exceptions or RTTI for
  their default configurations, and add a dependency to the UBT-side build).
- Unreal Automation tests only: rejected for the same reason as ADR-0001.
- Decided by robustness (no exceptions, single binary, no dependency), then simplicity.

### Status

Accepted; VALIDATED for the configurations CI builds (assertions enabled). Verified
against: `Tests/Harness/VaelenTest.h`, `Tests/Harness/TestMain.cpp`,
`Tests/Core/CMakeLists.txt`, `Tests/CMakeLists.txt`, `Tests/Core/Test_Harness.cpp`
(2 self-tests); used by the 110 other tests.

---

## ADR-0007: Bitmask-with-rejection for unbiased integer ranges

### Context

`RandomStream::Below(Count)` must return a uniform integer in `[0, Count)` with no bias
and with the same result on every platform. `NextU64() % Count` is biased. Lemire's
nearly-divisionless method is the usual fast unbiased choice but needs a 64 x 64 -> 128
bit multiply, which is not a native type on MSVC (it requires intrinsics that differ per
architecture) and would make the draw sequence depend on how that multiply is emulated.
Floating-point scaling loses precision above 2^53.

### Decision

`Random.cpp`, `RandomStream::Below`:

1. `Count == 0` is a Check failure; `Count <= 1` returns 0 without consuming a draw.
2. Build `Mask` = the smallest `2^k - 1` >= `Count - 1` by or-folding.
3. Loop: `X = NextU64() & Mask`; return `X` when `X < Count`, otherwise draw again.

`RangeInclusiveU(Min, Max)` and `RangeInclusive(Min, Max)` are `Min + Below(Span + 1)`
with a single-draw shortcut when the span is the full 64-bit range, and a Check on
`Min <= Max`.

### Consequences

- Unbiased by construction (every accepted value has the same probability) and portable:
  64-bit integer operations only. Chi-square and per-bucket checks pass on fixed seeds
  (`Random.BelowDistribution`, up to 1,000,000 draws).
- Powers of two take exactly one draw and return the masked output
  (`Random.BelowPowerOfTwoPath`); the expected number of draws is below 2, the worst
  case (`Count = 2^k + 1`) accepts about half of the candidates (`Random.BelowBounds`
  measures fewer than 25,000 draws for 10,000 calls).
- The number of draws per call varies, so `DrawCount` is not a function of the number of
  `Below` calls (see ADR-0003). Replay is unaffected because the same calls on the same
  state produce the same rejections.
- The loop is unbounded in theory; the probability of `n` consecutive rejections is at
  most 2^-n.
- Switching to another method later changes every generated sequence and therefore the
  save format.

### Status

Accepted; VALIDATED. Verified against: `Source/VaelenCore/Private/Random.cpp`,
`Source/VaelenCore/Public/Vaelen/Core/Random.h`, `Tests/Core/Test_Random.cpp`
(`BelowBounds`, `BelowDistribution`, `BelowPowerOfTwoPath`, `RangeInclusiveEdges`,
`RangeInclusiveUFullRange`, `AssertBelowZero`, `AssertRangeInclusiveInverted`,
`AssertRangeInclusiveUInverted`).

---

## ADR-0008: Kernel purity enforced by a CTest

### Context

ADR-0001 to ADR-0003 are conventions until something checks them. The compiler catches
part of it in the headless build (`throw` and `dynamic_cast` on polymorphic types are
errors under `-fno-exceptions`/`-fno-rtti`), but not an engine header that happens to be
on the include path under UBT, not `<random>`, `<chrono>`, `rand()` or `time()`, not a
`TODO` inside a file labelled VALIDATED, not a bare `long`, and not a missing STATUS line.
The check must run on every CI leg with no tooling beyond what the runners already have.

### Decision

`Tools/check_kernel_purity.py` (Python 3, standard library only), registered by
`Tests/CMakeLists.txt` as the CTest entry `Kernel.Purity` whenever `find_package(Python3)`
succeeds (a CMake warning otherwise). It scans every module named in
`Tools/kernel_modules.txt` (today `VaelenCore`): `Public/**/*.h,*.inl` and
`Private/**/*.cpp,*.h,*.inl`, skipping the single `*Module.cpp`. A lexer blanks comments
and string/character literals (preserving line numbers) before the token rules run.

Rules: R0 structure (at most one `*Module.cpp`; well-formed exemptions; not exemptable),
R1 include whitelist (`"Vaelen/..."` or a standard header not in the ban list:
`<random>`, `<chrono>`, `<ctime>`, `<time.h>`, stream and locale headers, `<exception>`,
`<stdexcept>`, `<typeinfo>`, `<typeindex>`, `<csetjmp>`, `<setjmp.h>`, `<filesystem>`),
R2 no exceptions, R3 no RTTI, R4 deterministic randomness (`rand`/`srand`,
`random_device`, `mt19937`, other `<random>` engines, `std::chrono`, clocks, `time()`,
`clock()`, OS timers), R5 header hygiene (`#pragma once` in `.h`; `// STATUS:` with one
of VALIDATED, PROTOTYPE, INCOMPLETE, UNVERIFIED in `.h`/`.inl`, validated in `.cpp` when
present), R6 no fake done (no `TODO`, `FIXME`, "implement later" in a VALIDATED file),
R7 fixed-width (no bare `long` family outside `static_cast<...>`). Exemption:
`// PURITY-ALLOW(Rn[, Rm]): reason` on the offending line (file-level R5 on any line).
Exit codes: 0 clean, 1 violations (`path:line: Rn rule-name: message`), 2 configuration
error. `--self-test` builds a synthetic repository in a temporary directory and checks
every rule, every exemption form, the lexer corner cases and the command line (36 checks).

### Consequences

- A purity violation fails `ctest` on every CI leg with a file and line; the rules and
  their reasons live in one script and are printed with each finding.
- The checker checks itself (`--self-test`) and the current kernel is clean
  (12 files, 0 violations).
- The check is textual and heuristic: it cannot see through macros, and a non-literal
  `#include` is reported as unverifiable rather than resolved. False positives are
  handled with a documented exemption; unused exemptions are reported under `--verbose`.
- Scope is the kernel only: tests, tools and Unreal modules are not scanned (by design;
  tests may use `<thread>`, `<unordered_map>` and so on).
- Without Python 3 the entry is silently absent from `ctest` (only a configure-time
  warning). The Linux CI job installs `python3`; the Windows and macOS jobs rely on the
  runner image, which has not been observed from this repository.
- The checker accepts UNVERIFIED as a fourth STATUS value beyond the three project labels,
  for engine-facing files the headless pipeline cannot compile.
- R5 requires a STATUS line in headers only; four of the five kernel `.cpp` files
  (`Assert.cpp`, `Ids.cpp`, `Random.cpp`, `Version.cpp`) have none, although the project
  rule asks for one in every kernel file (reported, not fixed here).

### Alternatives and decision rule

- clang-tidy / include-what-you-use: rejected for robustness and simplicity (an extra
  toolchain on four CI images, no rule for STATUS lines or Unreal includes).
- Relying on the headless build failing when an Unreal header is included: rejected as
  incomplete (it would not catch `<random>`, `throw` in dead code, missing STATUS lines,
  or an empty module scanned vacuously).
- Decided by robustness (self-tested, no dependency), then simplicity.

### Status

Accepted; VALIDATED. Verified against: `Tools/check_kernel_purity.py`,
`Tools/kernel_modules.txt`, `Tests/CMakeLists.txt`, `.github/workflows/kernel-ci.yml`;
runs executed: `--self-test` (36 checks, 0 failed), `--root /home/user/vaelen --verbose`
(12 files, 0 violations, 0 exemptions), `ctest` entry `Kernel.Purity` passed in all four
Linux configurations below.

---

## ADR-0009: Floating-point policy: no contraction, integers for authoritative state

### Context

The master prompt (§35) requires that the same seed and the same inputs produce the same
world, and the kernel is compiled by three compilers (Clang, GCC, MSVC) for the headless
build and by Unreal's own Clang/MSVC toolchains for the engine build. IEEE-754 basic
operations are deterministic, but compilers may legally fuse `a * b + c` into one FMA
instruction (`-ffp-contract`), which changes the rounding of the result. GCC contracts by
default in GNU mode, Clang contracts within expressions by default, MSVC does not under
`/fp:precise`. `RandomStream::RangeDouble` (`Min + (Max - Min) * NextDouble()`) is exactly
such an expression. `std::log`/`std::sqrt` (used by `NextNormal`) are additionally
implementation-defined across C runtimes.

### Decision

- The headless build compiles every kernel target with `-ffp-contract=off` (GCC/Clang)
  and `/fp:precise` (MSVC) through the `vaelen_build_flags` interface target in
  `/CMakeLists.txt`. `CMAKE_CXX_EXTENSIONS` is `OFF` (strict `-std=c++20`).
- Integer draws (`NextU64`, `Below`, `RangeInclusive*`) and the integer-derived uniforms
  (`NextDouble`, `NextFloat`) are the only random primitives promised bit-identical across
  platforms and compilers; `NextNormal`, `RangeDouble` and `Chance` are documented as
  deterministic per toolchain only (`Random.h`).
- Authoritative simulation state (anything saved, replayed or compared between runs)
  prefers integer or fixed-point representations. Floating point is allowed in derived,
  non-authoritative values and in presentation. A system that must accumulate floating
  point across ticks documents why and how the error is bounded (`Docs/CONVENTIONS.md`
  §6.9).

### Consequences

- The kernel's floating-point results are identical between the Linux Clang and GCC
  builds in the CI matrix; the `*-noasserts` presets exercise the optimised code paths.
- The Unreal build must apply the same contraction setting (UBT exposes it per module)
  before any floating-point result is called cross-compiler deterministic; until an
  engine-backed build exists this side is UNVERIFIED.
- A small performance cost on targets where FMA would otherwise be emitted; accepted, the
  kernel is integer-heavy by design.

### Alternatives and decision rule

- Build flags only (`-ffp-contract=off`): rejected for robustness, since the Unreal
  build does not see the CMake flags; the in-source pragmas make the kernel
  self-protecting under every toolchain.
- Fixed-point everywhere: kept as the policy for authoritative state, but not imposed on
  the random primitives, whose floating-point outputs are derived from integer draws.
- Decided by determinism and robustness.

### Status

Accepted 2026-09-05. Files: `/CMakeLists.txt`, `Source/VaelenCore/Public/Vaelen/Core/Random.h`,
`Docs/CONVENTIONS.md`. Headless VALIDATED (both compilers, six presets); engine side
UNVERIFIED.

---

## ADR-0010: Runtime entity handles with generations, dense registry, LIFO slot reuse

### Context

Simulation systems touch hundreds of thousands of entities per tick. `PersistentId`
(ADR-0004) is the right identity for anything saved or referenced across time, but it is
a 64-bit key into a hash map, not an index into dense component arrays, and it cannot
tell a caller cheaply whether the entity still exists. The runtime needs an accessor
that is (a) an array index, (b) safe against use after destruction, and (c) reproducible:
two runs with the same operations must hand out the same handles, because handles end up
in per-tick ordering decisions.

### Decision

`Vaelen::EntityHandle` (`Source/VaelenSim/Public/Vaelen/Sim/EntityHandle.h`) is 64 bits:
the high 32 bits are a generation, the low 32 bits a slot index; value 0 is the null
handle and live generations start at 1. `Vaelen::EntityRegistry` (`EntityRegistry.h/.cpp`)
keeps a dense slot table, bumps the slot generation on `Destroy`, recycles free slots
through a LIFO free list, retires a slot whose generation reaches `MaxGeneration`, and
maps `PersistentId` to slot through an `unordered_map` that is only ever used for lookups.
Iteration and snapshot state are in slot-index order. Handles are not persisted: a
snapshot restores the slot table (`State`), which reproduces every handle exactly, and
`SetState` validates the state (unique ids, consistent free list and counters) before
accepting it.

### Alternatives and decision rule

- Raw pointers or bare indices: rejected for robustness (use after destruction is
  undetectable).
- `PersistentId` everywhere with hash-map lookups: rejected for performance and for
  determinism of iteration (hash-map order is not stable).
- 128-bit handles or separate generation arrays: rejected for simplicity; 32+32 bits with
  slot retirement gives 4 billion slots and 4 billion generations per slot.
- FIFO free list (delays reuse): rejected for simplicity; LIFO is equally deterministic
  and generations already guarantee stale-handle detection.
- Decided by robustness, then determinism and performance.

### Consequences

- Every component store of 01.02 can be indexed by `EntityHandle::Index()` and verified by
  the registry in debug builds.
- Handles must never be saved or compared across snapshots taken from different
  histories; `PersistentId` remains the identity in events, saves and references.
- A retired slot is never reused (bounded, documented leak of one slot per 2^32 - 1
  destructions of the same slot).

### Status

Accepted 2026-09-05. Files: `Source/VaelenSim/Public/Vaelen/Sim/EntityHandle.h`,
`EntityRegistry.h`, `Source/VaelenSim/Private/EntityRegistry.cpp`,
`Tests/Sim/Test_EntityHandle.cpp`, `Tests/Sim/Test_EntityRegistry.cpp` (16 tests, one
million create/destroy cycles). Headless VALIDATED on the six Linux presets; engine side
UNVERIFIED.

---

## ADR-0011: Components are plain data in typed sparse sets, registered explicitly

### Context

Systems must iterate the entities that carry a given component quickly, in an order that
is reproducible, and the whole component state must be snapshottable and comparable
between two runs (01.06, 01.07). Component types must be identified without RTTI
(ADR-0002) and without relying on static-initialisation order, which differs between
link orders and would silently change type ids between builds.

### Decision

- `ComponentTypeRegistry` (`Source/VaelenSim/Public/Vaelen/Sim/ComponentType.h`): a world
  registers its component types explicitly, in a fixed order, at setup. The id is the
  registration index (`ComponentTypeId`, 16-bit), the FNV-1a hash of the name is the
  stable identity used by snapshots and mods; `LayoutDigest()` summarises names, sizes
  and alignments in order. `ComponentType<T>` carries the type at compile time so pool
  lookups need neither RTTI nor a repeated type argument.
- Components are trivially copyable, default constructible plain data (enforced by
  `static_assert`): no pointers to other components, no owning resources. References
  between entities are `PersistentId`s.
- `ComponentPool<T>` (`ComponentPool.h`) is a sparse set: dense `std::vector<T>`, dense
  `std::vector<EntityHandle>` (full handles, so stale generations never match), sparse
  index by slot. Removal swaps the last entry into the hole. Snapshot state is the two
  dense arrays; `SetState` rebuilds the sparse index and rejects inconsistent input.
- `ComponentStore` (`ComponentStore.h`) owns one pool per created type and removes every
  component of an entity in type-id order (`RemoveAll`), which the owner calls before
  destroying an entity.

### Alternatives and decision rule

- Archetype storage (grouping entities by component set, as in Unreal Mass or flecs):
  rejected for simplicity in Phase 01; sparse sets are simpler, iteration by single
  component is optimal, and archetypes can be introduced behind the same `ComponentStore`
  interface if the Phase 18 stress tests demand it.
- Type ids from a template instantiation counter or `__COUNTER__`: rejected for
  determinism (depends on translation-unit and link order).
- Slot-ordered dense arrays (sorted insertion): rejected for performance; determinism
  only requires the order to be a function of the operation sequence, which swap-remove
  satisfies.
- Non-trivial components with constructors and owning members: rejected for robustness
  of persistence and replay comparison.
- Decided by determinism and robustness, then simplicity.

### Consequences

- Dense iteration order is not slot order; a system that needs a canonical order sorts
  by `PersistentId` or iterates the registry.
- A component pool can be serialised as raw bytes (01.06) and hashed for replay
  comparison (01.07).
- Adding a component to a new generation of a slot whose stale entry was not removed is
  reported (`VAELEN_ENSURE`) and repaired, never silently wrong.

### Status

Accepted 2026-09-05. Files: `Source/VaelenSim/Public/Vaelen/Sim/ComponentType.h`,
`ComponentPool.h`, `ComponentStore.h`, `Source/VaelenSim/Private/ComponentType.cpp`,
`ComponentStore.cpp`, `Tests/Sim/Test_ComponentType.cpp`, `Test_ComponentPool.cpp`,
`Test_ComponentStore.cpp` (15 tests, one million operations against a live registry).
Headless VALIDATED on the six Linux presets; engine side UNVERIFIED.

---

## ADR-0012: Systems are ordered by declared dependencies with a name-hash tie-break; each gets a per-tick derived stream

### Context

The master prompt requires that the world evolve the same way for the same seed and
inputs (section 35), that systems be schedulable at several levels of detail (section
36), and that adding a random draw in one system never perturbs another (ADR-0003).
Execution order must therefore be a property of the set of systems, not of the order
in which code happened to register them or of memory addresses, and it must stay valid
when systems are later spread over threads.

### Decision

`Scheduler` (`Source/VaelenSim/Public/Vaelen/Sim/System.h`, `Private/Scheduler.cpp`):

- Systems declare their name and the names of the systems that must run before them
  (`ISystem::GetDependencies`). `Build()` runs Kahn's algorithm and, among the ready
  systems, always picks the smallest name hash (FNV-1a, then the name itself on a
  collision). Unknown dependencies, cycles (including self-dependencies), duplicate
  names and invalid LOD schedules are build errors; the scheduler refuses to run.
- Simulation LOD: every system has a `SimLod` 0-4; the `LodSchedule` gives one tick
  period per level (defaults 1, 4, 24, 720, 8640 ticks: every tick, every 4 hours, daily,
  monthly, yearly). A system ticks when `tick % period == 0`.
- Random streams: on every tick the scheduler hands each system
  `WorldStream.Derive(nameHash).Fork(tick)`. The stream is a function of the world seed,
  the system's name and the tick only, so it is unaffected by other systems, by the
  order of execution and by how many draws the system made on earlier ticks.
- `RunTick` runs the due systems in order with a `TickContext` (tick, clock, registry,
  components, stream, event bus) and then advances the clock by one tick; the scheduler
  is the only caller of `SimClock::Advance` in normal operation.

### Alternatives and decision rule

- Registration order as execution order: rejected for determinism (results would depend
  on module load order and code layout).
- Priorities (integers) instead of dependencies: rejected for evolvability (priorities
  need global coordination; dependencies are local statements that compose).
- One stream per system advanced across ticks: rejected for robustness of replay and
  partial re-simulation (a system's sequence would depend on its own history of draws;
  per-tick forking makes every tick self-contained).
- Variable LOD periods per entity rather than per system: deferred; per-system periods
  are the simple hook the master prompt asks for in Phase 01, per-entity LOD is Phase 15.
- Decided by determinism, then evolvability and simplicity.

### Consequences

- Renaming a system changes its execution position among independent systems and its
  random sequence: a name is part of the save-format contract of a world.
- The order is a total order today (single-threaded). The declared dependencies are
  exactly the information a parallel scheduler needs later; results must not change.
- Systems must not hold pointers into the `TickContext` beyond the call.

### Status

Accepted 2026-09-05. Files: `Source/VaelenSim/Public/Vaelen/Sim/System.h`,
`Source/VaelenSim/Private/Scheduler.cpp`, `Tests/Sim/Test_Scheduler.cpp` (8 tests).
Headless VALIDATED on the six Linux presets; engine side UNVERIFIED.

---

## ADR-0013: Simulation time is an integer tick count; the calendar is data derived from it

### Context

Master prompt section 33: time is continuous for the player (days, seasons, years,
generations; wait, sleep, travel, accelerate, pause) and the world keeps running while
unobserved. Section 35 forbids anything that makes a run depend on the machine. A
floating-point delta time would make results depend on frame rate and on accumulated
rounding; the wall clock is already banned by the purity rules (R1/R4).

### Decision

`SimClock.h` (header-only, constexpr):

- `SimTick` is a `uint64` count of fixed-duration ticks since the world epoch. The clock
  only moves by `Advance()` (one tick) and `Restore(tick)` (snapshots). Accelerating
  time means executing more ticks per real second; a tick never changes length.
- `CalendarRules` is data (ticks per hour, hours per day, days per month, months per
  year, months per season) with regular defaults: 1 tick = 1 hour, 24-hour days, 30-day
  months, 12 months, 4 seasons of 3 months, a 360-day AELVOR year. `Calendar::ToDate` and
  `ToTick` are exact inverses over the whole `uint64` range.
- Irregular rules (leap days, intercalary months, per-culture calendars) are a data
  decision for the world-generation phases; the kernel provides the regular skeleton and
  the extension point, not a historical calendar.

### Alternatives and decision rule

- Floating-point simulation time (seconds as `double`): rejected for determinism.
- Fixed real-world calendar (Gregorian rules): rejected for evolvability; AELVOR is not
  Earth and cultures will carry their own calendars.
- Variable tick length per LOD: rejected for simplicity; LOD is expressed as tick
  periods (ADR-0012), not as time dilation.
- Decided by determinism, then simplicity.

### Consequences

- 1 tick = 1 hour at the default rules; a year is 8640 ticks; `uint64` covers about
  2 x 10^15 years, so overflow is not a practical concern and every tick value is valid.
- Every duration in the simulation (gestation, travel, seasons) is expressed in ticks or
  in calendar units converted through `CalendarRules`, never in real seconds.

### Status

Accepted 2026-09-05. Files: `Source/VaelenSim/Public/Vaelen/Sim/SimClock.h`,
`Tests/Sim/Test_SimClock.cpp` (4 tests). Headless VALIDATED on the six Linux presets;
engine side UNVERIFIED.

---

## ADR-0014: Events are plain 112-byte records with a cause; delivered next tick in publish order; the log is append-only with a running digest

### Context

The master prompt demands that every event be caused by the systems (section 2), that
the game answer "why did this happen" with a causal chain (section 58), that event logs
support replay (section 35), and that history stay addressable centuries later. The
kernel therefore needs an event record that is cheap, comparable between runs and
serialisable as bytes, plus a delivery discipline that cannot depend on scheduling.

### Decision

- `Event` (`Source/VaelenSim/Public/Vaelen/Sim/Event.h`): 112 bytes, no padding, every
  byte defined: `Id` (PersistentId of kind Event, monotonic from the world allocator),
  `Tick`, `TypeHash` (FNV-1a of the type name, `EventType<T>`), `Cause` (id of the event
  that caused it, Invalid for root causes), `Subject` (the persistent id it is about),
  `PayloadSize` and 64 payload bytes holding a trivially copyable `T`. The cause field is
  the edge of the causal graph that Phase 17 tooling walks.
- `EventLog`: append-only; `Digest()` is a running `HashCombine` over each event's raw
  bytes in order, so equal digests mean identical histories; byte image `[count]
  [digest][events]` whose digest is recomputed and checked on load. Unbounded in Phase
  01 (the log is the history); tiering and compaction belong to Phases 16/17.
- `EventBus`: `Publish(tick, type, payload, subject, cause)` logs the event immediately
  and queues it; `Dispatch(tick)` delivers every event published before `tick`, in
  publish order, to the listeners of its type ordered by listener-name hash; events
  published while dispatching wait for the next tick. `Scheduler::RunTick` dispatches
  before running the systems of the tick, so a system sees the events of the previous
  tick, never those of the current one.

### Alternatives and decision rule

- Immediate (synchronous) delivery: rejected for determinism and evolvability (results
  would depend on which system published first within a tick and would break under a
  parallel scheduler).
- Variable-size payloads (byte spans, heap allocation): rejected for simplicity and
  robustness of hashing and serialisation; 64 bytes hold ids, counts and small structs,
  and large data belongs in components referenced by `Subject`.
- Listeners in subscription order: rejected for determinism (same reasoning as system
  ordering, ADR-0012).
- Decided by determinism, then robustness and simplicity.

### Consequences

- One-tick latency between cause and reaction is the rule; a chain of n reactions takes
  n ticks (one hour each at the default calendar). Systems that need same-tick effects
  use component state, not events.
- Renaming an event type or listener changes hashes and delivery order: names are part
  of a world's save-format contract.
- Every published event is logged even when nobody listens: the log is the history, not
  a message queue.

### Status

Accepted 2026-09-05. Files: `Source/VaelenSim/Public/Vaelen/Sim/Event.h`, `EventBus.h`,
`Source/VaelenSim/Private/EventBus.cpp`, `Scheduler.cpp`, `Tests/Sim/Test_Event.cpp`,
`Test_EventLog.cpp`, `Test_EventBus.cpp` (10 tests). Headless VALIDATED on the six Linux
presets; engine side UNVERIFIED.

---

## ADR-0015: One World object owns the state; snapshots are a symmetric, versioned, digest-checked byte image; stored types carry no padding

### Context

The master prompt requires checkpoints, deterministic replay and "same seed + same
inputs = same result" (sections 3 and 35). Phase 01 needs a way to capture the whole
simulation state, restore it into a fresh process and continue as if nothing had
happened, and a way to compare two worlds byte for byte. Until 01.06 the state blocks
(id allocator, random stream, clock, registry, pools, event bus, log) were assembled
by hand in every test.

### Decision

1. `World` is the single owner of state. It holds the id allocator, the root random
   stream, the clock, the entity registry, the component store, the event bus and the
   event log, and references the code that acts on them (component type registrations,
   systems, listeners). The code is not saved: the same setup function runs on both
   sides of a restore, and the snapshot verifies that it did (component layout digest,
   seed, per-pool type id, name hash and element size).
2. Serialisation is symmetric. `IArchive` exposes `IsLoading()` and `SerializeBytes`;
   one routine per type both writes and reads, so save and load cannot drift. The
   memory reader never throws: a read past the end sets a sticky error flag and
   zero-fills the destination; vector counts are bounded before allocation.
3. The image is versioned and digest-checked. Header: magic `VAELENSN`,
   `VAELEN_SAVE_FORMAT_VERSION`, flags, component layout digest, seed. Body: clock,
   root stream state, 256 id counters, entity slots, pools in type-id order, pending
   events, event log. Trailer: FNV-1a digest of every preceding byte, verified before
   any state is touched. Every rejection is a named `SnapshotResult`
   (`VersionMismatch`, `BadMagic`, `LayoutMismatch`, `MissingPool`, `Truncated`,
   `Corrupt`, `Inconsistent`); a wrong version is never migrated silently.
4. Stored types carry no padding. Components and event payloads must satisfy
   `IsPlainData<T>`: trivially copyable and either empty, provably unique in
   representation (`std::has_unique_object_representations_v`) or declared padding-free
   through `PlainDataTraits<T>::NoPadding` (needed for floating-point members, which the
   compiler cannot prove). Structs with padding inside the kernel (the registry slot)
   are written field by field.
5. Only the root random stream is saved. Per-system per-tick streams are derived from
   the root seed, the system name and the tick (ADR-0012), so they need no state.
6. Systems hold no state of their own. Anything a system needs across ticks lives in
   components or events; otherwise a restored world could not continue identically.

### Alternatives and decision rule

- Separate writer and reader code paths: rejected; the round-trip tests found that
  the raw registry slot image was non-deterministic (padding bytes), and a symmetric
  routine makes such asymmetries impossible by construction.
- Raw `memcpy` of state structs, padding included: rejected; two identical worlds
  would differ in bytes nobody wrote, defeating byte-identical snapshots.
- Automatic migration of older format versions: deferred to Phase 16 with on-disk
  files; the kernel rejects mismatches explicitly instead.
- Saving derived per-system streams: rejected; they are a pure function of saved data.
- Decided by robustness (explicit rejection, digest before mutation), then
  determinism, then simplicity.

### Consequences

- Adding a component field, renaming a type or changing the seed changes the layout
  digest or the identity check: old images are rejected until Phase 16 provides
  migration.
- `World` is the unit of testing from 01.07 on; tests no longer assemble the blocks by
  hand.
- Types with floating-point members must declare `PlainDataTraits<T>::NoPadding` and
  their authors are responsible for the field layout.
- Loading into a world leaves that world unspecified on failure; callers discard it.

### Status

Accepted 2026-09-05. Files: `Source/VaelenSim/Public/Vaelen/Sim/PlainData.h`,
`Archive.h`, `World.h`, `Snapshot.h`, `Source/VaelenSim/Private/Archive.cpp`,
`World.cpp`, `Snapshot.cpp`, `Tests/Sim/Test_Archive.cpp`, `Test_World.cpp`,
`Test_Snapshot.cpp` (15 tests). Headless VALIDATED on the six Linux presets; engine
side UNVERIFIED.

---

## ADR-0016: Tiles are dense typed layers, not entities; the world map is a state block with a code-declared layer set

### Context

Phase 02 derives AELVOR from the seed: a grid of up to 4096 x 4096 tiles (16.7 million)
with elevation, climate, hydrology and more per tile. The Phase 01 entity model (a
persistent id, a registry slot and sparse-set components per entity) costs tens of
bytes of bookkeeping per entity and hashes pool by pool; regions, rivers and deposits
number in the thousands and fit it, tiles do not. The map must still be snapshotted,
restored, hashed and replayed exactly like every other state block (ADR-0015).

### Decision

1. Tiles are addressed by coordinate or row-major index on a `WorldGrid`; per-tile
   values live in `TileLayer<T>` (one dense vector per layer, plain data under the
   ADR-0015 rule, name-seeded digest). No tile has an id, a slot or a component.
2. The neighbour order is fixed (N, NE, E, SE, S, SW, W, NW) and border-clipped, so
   every algorithm that walks neighbours is deterministic by construction.
3. `WorldMap` is a state block of `World`: its config, grid and layer contents are
   state; its layer set (names, element sizes, order) is code declared by the setup
   function, folded into the snapshot header's layout digest next to the component
   layout, and verified layer by layer on load.
4. The snapshot format is versioned by this change (`VAELEN_SAVE_FORMAT_VERSION` 2):
   format-1 images are rejected explicitly, never migrated silently.
5. Regions, rivers, lakes and deposits will be entities with components; they reference
   tiles by index.

### Alternatives and decision rule

- Tiles as entities: rejected; 16.7 million registry slots and sparse indices for data
  that is never created or destroyed individually, and a snapshot dominated by handles.
- A fixed struct per tile: rejected; every stage would change one struct shared by all,
  and hashing or serialising one field would touch all fields. Layers add and hash
  independently per stage.
- Layers registered at runtime by name from data: rejected for Phase 02; the set of
  layers is part of the save-format contract and belongs to code, like component types.
- Decided by robustness (explicit layout check, explicit version rejection), then
  simplicity, then performance (dense arrays), consistent with determinism.

### Consequences

- Adding a layer or changing an element size changes the layout digest: old images are
  rejected until Phase 16 provides migration.
- Grid size is state: the same code can generate 64 x 64 test worlds and the 1024 x 1024
  default, and a snapshot carries its own size.
- The frozen state digests of the Phase 01 references changed with the format version;
  their log digests did not.

### Status

Accepted 2026-09-05. Files: `Source/VaelenSim/Public/Vaelen/Sim/TileGrid.h`,
`WorldMap.h`, `Source/VaelenSim/Private/WorldMap.cpp`, `Snapshot.cpp`,
`Source/VaelenCore/Public/Vaelen/Core/Version.h`, `Tests/Sim/Test_TileGrid.cpp`,
`Test_WorldMap.cpp` (10 tests). Headless VALIDATED on the six Linux presets; engine
side UNVERIFIED.

---

## ADR-0017: World generation uses Q32.32 fixed point and integer lattice noise; no floating point, no libm

### Context

The world of AELVOR is derived from the seed and must hash identically on clang, gcc,
MSVC and AppleClang (Phase 01 proved the simulation does). ADR-0009 makes floating
point bit-stable inside one toolchain by forbidding contraction, but terrain
generation needs noise, interpolation, roots and later trigonometry-like curves, and
`sin`, `exp`, `pow` and friends are implemented differently by every libm: the same
source would generate different worlds on different platforms.

### Decision

1. World generation computes in `Fix64`, a Q32.32 fixed-point number in a signed
   64-bit raw value (range [-2^31, 2^31), resolution 2^-32). Every operation is
   constexpr and defined for every input: wrapping arithmetic on unsigned values, a
   saturating zero divisor, zero for roots of negatives.
2. The 128-bit intermediates of multiplication and division are built from 32-bit
   halves and bit-by-bit long division, not from `__int128` or compiler intrinsics,
   so MSVC and the others agree bit for bit and the code stays constexpr.
3. Noise is lattice-based: a SplitMix-style mixer of (seed, x, y) gives lattice values
   and gradient directions; interpolation uses SmoothStep weights in Fix64; fractal
   sums derive one seed per octave from the base seed; domain warping derives two more.
4. `<cmath>` and floating-point types are banned from the world-generation files;
   tests may use doubles as references with an explicit tolerance argument.
5. Frozen values guard the noise at fixed points and over a field: a change of any
   constant is a deliberate change of every generated world.

### Alternatives and decision rule

- Floating point with a private, deterministic math library (own sin/exp): rejected;
  the rounding of every intermediate would still have to be reasoned about per
  compiler, and fixed point makes exactness provable by construction.
- 32-bit fixed point (Q16.16): rejected; not enough range for elevations, distances
  and accumulated flows on a 4096-wide grid at sub-metre resolution.
- Simplex or open-simplex noise: deferred; gradient noise on a square lattice is
  simpler to make exact and its directional artefacts are hidden by fractal sums and
  warping; the noise API keeps the door open.
- Decided by determinism across platforms first, then robustness (defined
  everywhere), then simplicity.

### Consequences

- World-generation code cannot use `float`/`double`; anything that needs a curve
  gets a fixed-point implementation with a frozen test.
- Division and square root cost a 64- or 128-step loop; generation stages must use
  them per tile, not per neighbour pair, and the 1024 x 1024 baseline (02.08) records
  the cost.
- The simulation proper (Phases 03+) keeps ADR-0009 floats where it needs them; only
  the seed-to-world pipeline is fixed point.

### Status

Accepted 2026-09-05. Files: `Source/VaelenSim/Public/Vaelen/Sim/FixedPoint.h`,
`Noise.h`, `Source/VaelenSim/Private/Noise.cpp`, `Tests/Sim/Test_FixedPoint.cpp`,
`Test_Noise.cpp` (9 tests). Headless VALIDATED on the six Linux presets; engine side
UNVERIFIED.

---

## ADR-0018: World generation is a pipeline of pure stages with derived seeds, a 32-slot parameter block and a sea-bounded continent

### Context

02.03 is the first stage that turns the seed into terrain. The way it is shaped
decides how every later stage (climate, hydrology, regions, deposits) plugs in, how
parameters travel through snapshots, and what kind of world AELVOR is.

### Decision

1. Each stage is a pure function of (seed, config, earlier layers). It derives its own
   seeds from the world seed and its stage name through the lattice hash, writes only
   its own layers, and has a frozen digest, so a change is confined to that stage and
   what follows it.
2. Stage parameters live in `WorldGenConfig::Params`, 32 raw Q32.32 / integer slots
   addressed by named indices; zero means "use the stage's default". Adding a parameter
   does not change the config's layout or the save format. The slots replaced the
   reserved words of 02.01 (save format 3).
3. The world is a sea-bounded continent: a warped low-frequency mask plus a bias makes
   the land, an edge falloff sinks everything near the border, fractal relief adds
   detail everywhere and cubed ridge noise raises mountains only where the mask is
   solid. Sea level is a config value; classification (land, coast, shore, border) and
   slope derive from elevation and are recomputed by `ClassifyTerrain` whenever a later
   stage edits elevation.
4. Inspection is by numbers and ASCII: `MeasureElevation` (land fraction, largest
   landmass, coast tiles, border land, extremes) and `ExportAscii`, both used by the
   tests and readable on a phone.

### Alternatives and decision rule

- One monolithic generator: rejected; a frozen digest per stage localises changes and
  lets later phases regenerate a single layer.
- Typed parameter structs per stage inside the config: rejected for now; every new
  stage would change the config layout and the save format.
- A wrapping (toroidal) world or coast-to-edge land: rejected; a sea-bounded continent
  keeps every later algorithm (flow, regions, routes) free of edge cases and matches a
  single-continent AELVOR; the parameters can still shrink the falloff later.
- Decided by evolvability (stages, slots) and robustness (bounded world), consistent
  with determinism (derived seeds, integer math per ADR-0017).

### Consequences

- Later stages append to `WorldLayers::Declare` and `ParamIndex`; they never edit
  another stage's layer except elevation through documented calls followed by
  `ClassifyTerrain`.
- The elevation digest depends on the parameter defaults: changing a default is a
  deliberate change of every world and of the frozen tests.
- Generation time is dominated by three fractal evaluations per tile (0.74 s at
  1024 x 1024 in release); the 02.08 baseline records the whole pipeline.

### Status

Accepted 2026-09-05. Files: `Source/VaelenSim/Public/Vaelen/Sim/WorldGen.h`,
`WorldMap.h`, `Source/VaelenSim/Private/WorldGen.cpp`,
`Source/VaelenCore/Public/Vaelen/Core/Version.h`, `Tests/Sim/Test_WorldGen.cpp`
(6 tests). Headless VALIDATED on the six Linux presets; engine side UNVERIFIED.

---

## ADR-0019: Climate is a row-wise advection model with resolution-independent decay and a threshold biome table

### Context

Biomes drive every later phase (deposits, settlement, agriculture, culture). The
climate must be believable enough to give rain shadows, dry interiors and wet coasts,
cheap enough to run per tile at 1024 x 1024, deterministic across platforms
(ADR-0017), and independent of the grid resolution so the 64, 256 and 1024 worlds share
one set of parameters.

### Decision

1. Temperature is a latitude band (equator at the middle row, poles at the top and
   bottom rows) minus an altitude lapse per 1000 elevation units, plus bounded local
   noise. Seasons are a separate offset function of latitude and season index that later
   phases add on top of the annual mean; the layer stores the mean only.
2. Moisture comes from a humidity parcel advected along each row by the prevailing
   wind of that latitude (trade easterlies, westerlies, polar easterlies). Over land it
   rains a base fraction per tile plus an orographic share of any climb; over sea it
   recovers. The base fraction is one over the decay distance, and the decay distance
   is a fraction of the map width, so the same parameters give the same climate at
   every resolution. A rational sea-proximity term (from a multi-source BFS distance)
   is blended in so coasts are never dry and interiors never reach zero.
3. Biomes are a threshold table over (temperature, moisture, elevation above sea,
   land): twelve entries with names and glyphs, ordered so every branch is reachable
   and tested.
4. Winds are per row, not per tile: no advection across rows and no global circulation.

### Alternatives and decision rule

- A 2D moisture diffusion or a global circulation model: rejected for Phase 02; an
  order of magnitude more cost for no test that could distinguish it, and harder to
  keep deterministic and resolution independent.
- Moisture from sea distance only: rejected; no rain shadow, which the deposits and
  cultures of later phases lean on.
- Per-tile decay constants: rejected after the first version (a fixed 1/12 per tile
  made the interior of a 256-wide continent a desert and would make a 1024-wide one
  uniformly dry).
- Decided by robustness across resolutions, then simplicity, consistent with
  determinism.

### Consequences

- Every climate constant is a parameter slot (`ParamIndex` 9 to 18) with a default;
  the frozen digests at 256 pin the defaults.
- Later stages that edit elevation (hydrology's depression filling) rerun
  `ClassifyTerrain` and `GenerateClimate` in that order.
- Seasons are not stored: whoever needs a seasonal temperature calls
  `SeasonalOffset` with the calendar's season.

### Status

Accepted 2026-09-05. Files: `Source/VaelenSim/Public/Vaelen/Sim/WorldGen.h`,
`Source/VaelenSim/Private/WorldGen.cpp`, `Tests/Sim/Test_Climate.cpp` (6 tests).
Headless VALIDATED on the six Linux presets; engine side UNVERIFIED.

---

## ADR-0020: Hydrology fills depressions by priority flood, fills shallow basins with sediment and keeps deep ones as lakes; rivers and lakes are entities

### Context

Rivers and lakes shape settlement, trade and borders in every later phase. The
fractal relief of 02.03 has closed depressions everywhere; a drainage model has to
decide what becomes a lake, what water simply crosses, and how to do that
deterministically on a million tiles.

### Decision

1. Depressions are filled by priority flood + epsilon seeded from every sea tile, with
   ties broken by tile index. After filling every land tile has a strictly lower
   neighbour, so D8 flow never stalls and never cycles.
2. Flow direction is the steepest descent on the filled surface, diagonals scaled by
   181/256, ties resolved by the fixed neighbour order; accumulation is computed in
   decreasing (filled, index) order.
3. Raised tiles form basins (4-connected). A basin whose deepest fill is below
   `LakeMinDepth` (160 units) or that has fewer than `LakeMinTiles` (12) tiles is filled
   with sediment: its elevation is raised to the water surface and it becomes a plain.
   A deeper, larger basin is a lake: the elevation stays as the lake bed, the filled
   level is the surface. The stage therefore edits the elevation layer and reruns
   `ClassifyTerrain`, and runs before the climate stage.
4. Rivers are tiles whose accumulation exceeds a fraction of the tile count, outside
   lakes, traced from their sources in scan order; each trace of at least
   `MinRiverLength` tiles is one entity with a `RiverInfo` component; each lake is one
   entity with a `LakeInfo` component; index layers point back to them.
5. The stage destroys its previous entities before running, so it can be re-run, but
   ids are fresh each time: regeneration reproduces a world only from a fresh world.

### Alternatives and decision rule

- Filling only (Barnes' priority flood as is): rejected after measurement; 375 lakes
  covering a tenth of the land and rivers cut to ten tiles at 256.
- Breaching (carving channels through rims): deferred; more code and parameters for a
  result the sediment rule approximates, and it can be added later as a second
  basin outcome without changing the layers.
- Rivers as tile flags only: rejected; later phases need to name a river, follow it
  and hang history on it, which the entity model provides.
- Decided by robustness (drainage guaranteed by construction), then simplicity,
  consistent with determinism (index tie-breaks everywhere).

### Consequences

- Elevation after hydrology differs from the 02.03 elevation on filled basins; the
  02.03 frozen digests are taken before hydrology and stay valid.
- Rivers end where they meet a lake or another river; a lake's outlet tile starts a
  new river when its flow is high enough.
- The 32-slot parameter block gains four hydrology slots (19 to 22).

### Status

Accepted 2026-09-05. Files: `Source/VaelenSim/Public/Vaelen/Sim/Hydrology.h`,
`Source/VaelenSim/Private/Hydrology.cpp`, `Source/VaelenCore/Public/Vaelen/Core/Ids.h`,
`Tests/Sim/Test_Hydrology.cpp` (5 tests). Headless VALIDATED on the six Linux presets;
engine side UNVERIFIED.

---

## ADR-0021: Regions grow from lattice seeds by terrain cost with a merge floor; the adjacency graph is derived, not stored

### Context

Every later phase addresses the world by region: settlement, polities, routes, wars,
maps. Regions must cover the land exactly, be contiguous, follow the terrain, have a
usable size, be entities with ids, and expose their neighbours - all deterministically
and cheaply at 1024 x 1024.

### Decision

1. Seeds come from a jittered lattice over the land (the land tile nearest to a hashed
   offset in each cell) plus one seed for every landmass that received none, so every
   island has a region.
2. Regions grow by a multi-source least-cost search on the 4-neighbourhood whose step
   cost rises with elevation change and with stepping onto a river tile; ties are
   broken by tile index. Ridges and rivers therefore become borders without any
   explicit watershed computation.
3. Regions below a size floor merge into the neighbour they share the longest border
   with, smallest first; an island below the floor keeps its own region. Indices are
   compacted in seed order and one entity with a `RegionInfo` component is created per
   region.
4. The adjacency graph (sorted neighbour lists with shared-border lengths) is derived
   from the region-index layer on demand and never stored: the layer is the single
   source of truth and the snapshot carries no redundant structure.

### Alternatives and decision rule

- Strict watershed regions (one per river basin): rejected; basins vary from a few
  tiles to a quarter of the continent, and the size band matters more to later
  phases than hydrological purity. Rivers still shape borders through the cost.
- Voronoi cells on plain distance: rejected; borders would cut across mountains and
  rivers, which is what regions exist to avoid.
- Lloyd relaxation of the seeds: deferred; the lattice plus merging gives a usable
  band (126 regions at 256, 166 at 1024) without iteration.
- Storing neighbour lists in the component: rejected; a fixed-size list caps the
  degree and duplicates the layer, and rebuilding is cheap.
- Decided by robustness (exact cover, contiguity, floor) and simplicity, consistent
  with determinism.

### Consequences

- Region ids, like river and lake ids, are fresh on every generation; a world is
  reproduced only from a fresh world.
- Later phases that need the graph call `BuildRegionGraph` once and keep the result
  for as long as the layer is unchanged.
- The parameter block gains four region slots (23 to 26).

### Status

Accepted 2026-09-05. Files: `Source/VaelenSim/Public/Vaelen/Sim/Regions.h`,
`Source/VaelenSim/Private/Regions.cpp`, `Tests/Sim/Test_Regions.cpp` (5 tests).
Headless VALIDATED on the six Linux presets; engine side UNVERIFIED.

---

## ADR-0022: Deposits come from an explicit suitability table, hashed draws and one-per-kind-per-cell spacing

### Context

Resources drive the economy, settlement and conflict of later phases. The master
prompt forbids hand placement: deposits must follow from the seed and the terrain,
be explainable ("why is there iron here"), be spread rather than clumped, and stay
deterministic and cheap.

### Decision

1. Suitability is an explicit, public, pure function of (kind, land, coast, biome,
   elevation above sea, slope, river, lake adjacency, river adjacency) returning 0 to
   1000. Every rule is a line in one table and every line has a test.
2. Placement is a per-tile, per-kind hashed draw: chance = base chance of the kind
   times suitability times density. The hash is the lattice hash of the stage seed,
   the kind and the coordinates, so a tile's outcome never depends on scan order.
3. Spacing: one deposit per kind per cell of `DepositSpacing` tiles, the cell keeping
   the best (suitability, hash) draw; one deposit per tile, first kind wins.
4. Richness is seven tenths suitability plus a hashed share; the tier is the kind's
   base tier (common, uncommon, rare) raised for the richest draws. Each deposit is an
   entity of kind ResourceDeposit carrying its region index.

### Alternatives and decision rule

- Poisson-disc sampling per kind: rejected; the cell rule gives a comparable spread
  with no iteration and an obvious determinism argument.
- Suitability by learned or noise-driven fields: rejected; explainability and
  testability of every rule matter more than variety, and the hash already adds it.
- Placing deposits per region (quotas): deferred to the economy phases, which can
  still add region-level resources on top of tile deposits.
- Decided by robustness (explicit rules, frozen counts) and simplicity, consistent
  with determinism.

### Consequences

- Balancing is a table edit plus a refreeze; the frozen counts at 256 make every
  balance change deliberate.
- The parameter block gains two deposit slots (27 and 28); slots 29 to 31 remain.
- A deposit's `Region` is a snapshot of the region layer at generation time; if
  regions are regenerated, deposits must be regenerated after them.

### Status

Accepted 2026-09-05. Files: `Source/VaelenSim/Public/Vaelen/Sim/Deposits.h`,
`Source/VaelenSim/Private/Deposits.cpp`, `Tests/Sim/Test_Deposits.cpp` (5 tests).
Headless VALIDATED on the six Linux presets; engine side UNVERIFIED.

---

## ADR-0023: The world is a function of seed and config through one pipeline call, frozen as whole-world digests at three sizes

### Context

Seven stages exist with their own layers, entities, parameters and frozen digests.
Later phases and the engine need one way to produce a world, one way to check that a
build still produces the same world, and one setup routine that every side of a
snapshot runs identically.

### Decision

1. `WorldSetup::Declare` declares every Phase 02 layer and component type in a fixed
   order; it is the setup routine of the world's save-format contract (ADR-0015).
2. `GenerateWorld(World, Setup, Config, Last)` runs Reset, elevation, hydrology,
   climate, regions and deposits, stoppable after any stage; an invalid config or
   stage is refused with a report. Re-running replaces the generated entities.
3. The world is a pure function of (seed, config): the tests freeze the digest of the
   whole world state - layers and entities - at 64, 256 and 1024 for the AELVOR seed,
   and every compiler and platform in CI must reproduce them. Two fresh worlds give
   byte-identical snapshot images.
4. Degenerate inputs are legal: a drowned world and a 1 x 1 map succeed with zero
   entities; a non-square map is ordinary.

### Alternatives and decision rule

- Leaving stage composition to callers: rejected; the order carries constraints
  (hydrology edits elevation before climate, deposits read regions) that one function
  should own.
- Freezing only per-stage digests: rejected; the whole-world digest also covers the
  entity ids, the id allocator and the component pools, which is what a saved game
  depends on.
- A separate world-generation executable for CI comparison: rejected; the frozen
  constants inside the test binary already run on all four compilers.
- Decided by robustness (one owner of the order, whole-state freeze) and simplicity.

### Consequences

- Any change to a stage, a default parameter or the save format changes the three
  whole-world digests: refreezing them is the deliberate act that ships the change.
- The 1024 x 1024 world takes 25 s in a debug build with assertions; CI runs it on
  every push, which is acceptable now and to be watched.
- Phase 03 starts from `GenerateWorld` and adds its own setup and stages after it.

### Status

Accepted 2026-09-05. Files: `Source/VaelenSim/Public/Vaelen/Sim/WorldGenPipeline.h`,
`Source/VaelenSim/Private/WorldGenPipeline.cpp`, `Tests/Sim/Test_WorldPipeline.cpp`
(4 tests). Headless VALIDATED on the six Linux presets; engine side UNVERIFIED.

---

## ADR-0024: History is eras opened by span or caused request plus a chronicle of Record entities, with every piece of state in components

### Context

Phase 03 simulates the past. The master prompt wants every historical fact to be an
event with a cause, history to be queryable ("why did this happen"), and the world to
be addressable by era. The kernel already has an append-only event log with cause
links (ADR-0014) and stateless systems over components (ADR-0015).

### Decision

1. Eras are entities. A yearly system founds the first era, closes the open one when
   it reaches its span or when a request is pending, and opens the next; both
   transitions are events whose subject is the era and whose cause is the request's
   event. Systems and listeners request eras through `RequestEra(cause)`; the first
   cause wins until the yearly tick.
2. The chronicle is a listener over chosen event types: each such event becomes a
   Record entity carrying the era at its tick and the region of its subject when the
   subject is a region. Records are the historical record's index; the event log stays
   the source.
3. The era system's own state (pending request, open era, counts) lives in one
   `HistoryState` component on a history entity created once by `InitializeHistory`
   on a fresh world - never by setup code, so a restored world keeps its own.
4. Queries are functions over the log and the components: era at a tick, event by id
   (binary search on monotonic ids), cause chain to the root, events in an era, events
   about a subject.

### Alternatives and decision rule

- Eras as a fixed calendar (every N years): rejected; the prompt wants eras to be
  caused (a collapse, a founding) and the span rule stays as the fallback.
- Records as plain log annotations: rejected; later phases hang names, documents and
  maps on records, which entities with persistent ids support.
- Keeping the pending request inside the system object: rejected by ADR-0015 rule 6
  and proven by the snapshot test taken three ticks before the yearly tick.
- Decided by robustness (state in components, causes mandatory on requests) and
  simplicity.

### Consequences

- Era boundaries fall on yearly ticks: a request made mid-year opens the era at the
  next yearly tick, and its cause chain still points at the mid-year event.
- Every chronicled event costs one entity; later phases choose which types to
  chronicle and may summarise instead of recording each event.
- `IdKind::Era` is added; records use `IdKind::Document`.

### Status

Accepted 2026-09-05. Files: `Source/VaelenSim/Public/Vaelen/Sim/History.h`,
`Source/VaelenSim/Private/History.cpp`, `Source/VaelenCore/Public/Vaelen/Core/Ids.h`,
`Tests/Sim/Test_History.cpp` (3 tests). Headless VALIDATED on the six Linux presets;
engine side UNVERIFIED.

---

## ADR-0025: Population is coarse per region, cultures are entities, and a culture splits by graph distance with lineage spacing

### Context

Phase 03 needs people before persons exist (Phase 04): enough demography for cultures
to spread, mix and split over centuries of pre-history, at a cost that lets a 500-year
run finish in seconds and stay deterministic across compilers. Phase 02 already gives
regions, their adjacency graph, biomes, rivers and fertile deposits.

### Decision

1. Population is one `RegionPopulation` component per region: up to six culture slots
   with integer counts, a total, a capacity derived from the region's tiles (biome
   table, river tiles, fertile deposits), the majority and the years settled. No
   individuals, no fractions, no floats.
2. Cultures are entities (`CultureInfo`: home region, parent, generation, founding tick,
   identity hash). Founding, splitting, settling, abandoning and migrating are events;
   a settlement caused by a wave carries the wave as its cause.
3. Growth is yearly and logistic in integers (`Total * Growth * Room / Capacity / 1000`),
   decline above capacity is proportional to the excess, minorities below the
   assimilation share join the majority. Migration is monthly: a majority above the
   crowding threshold sends a share to the least crowded neighbour, every move decided
   on the start-of-tick state in region order, so the outcome is independent of pool
   iteration order and conserves people.
4. A culture splits when a region has been settled for `SplitYears` and its majority's
   home lies at least `SplitDistance` away in the region graph. The due region drags
   its connected far component of the same culture with it. The block joins the
   nearest sibling culture (same parent) whose home is closer than `SplitDistance`;
   otherwise it founds a culture at the block's lowest region. Homes of one lineage are
   therefore at least `SplitDistance` apart and the culture count is bounded by the
   graph, not by the order in which regions happen to become due.
5. All thresholds live in a public `PopulationRules` table with the defaults used by
   the frozen test; later phases (disasters, religions) read and change population only
   through the component and events.

### Alternatives and decision rule

- Per-tile population layers: rejected; regions are the unit history reasons about and
  a per-tile layer costs a 1024 x 1024 pass per year for nothing Phase 03 needs.
- One culture per splitting region: tried first and rejected; the frontier of an
  expanding culture becomes due one region at a time, which produced more than forty
  cultures in a century on a 99-region continent. Grouping the regions due in the same
  year was not enough either (41); the far-component and lineage-spacing rules bring
  the same run to 18 cultures with contiguous territories.
- Migration decided during pool iteration: rejected; conservation and determinism
  depend on decisions taken on a frozen view of the tick.
- Decided by robustness (integer bookkeeping with exact helpers, conservation proven by
  running the migration system by hand) and simplicity (one rule table, no floats).

### Consequences

- Six culture slots per region: a seventh culture cannot enter a region until
  assimilation frees a slot, and migration keeps the people where they are in that case.
- Capacity is recomputed from the map each year; a later phase that changes deposits or
  biomes changes capacity without touching this code.
- The frozen 500-year run at 128 (`f2afaa068c0f717d`, 47 587 people, 18 cultures) is
  the reference for every later change of the rules; a deliberate change re-freezes it.

### Status

Accepted 2026-09-05. Files: `Source/VaelenSim/Public/Vaelen/Sim/Population.h`,
`Source/VaelenSim/Private/Population.cpp`, `Tests/Sim/Test_Population.cpp` (5 tests).
Headless VALIDATED on the six Linux presets; engine side UNVERIFIED.

---

## ADR-0026: Names are built from a per-language phonology, pronounceable by construction, unique per scope, and stored as fixed-size components

### Context

From Phase 03 on, everything the player reads has a name: regions, rivers, cultures,
eras, later persons and documents. The master prompt wants names to feel like they
belong to a people, to change as peoples split, and to be reproducible from the seed.
The kernel forbids floats, external data and non-deterministic randomness, and every
piece of state must survive snapshots byte for byte.

### Decision

1. A language is an entity per culture with a `Phonology`: bit inventories over fixed
   onset, coda and vowel tables, four syllable-shape weights and a syllable range, 20
   bytes with no padding. A root language is derived from the culture's identity hash;
   a child culture's language is the parent's phonology with one sound change; every
   `DriftTicks` (150 years) a language changes one more sound. Names already given
   keep their text; the generation at naming time is recorded on the name.
2. A name is a pure function of (phonology, scope, key, salt): a hash stream draws the
   syllables; the construction rules (no vowel across a boundary, single consonant
   after a single coda and never the same letter twice, vowel after a cluster coda, no
   repeated syllable, two-syllable floor without suffix, stem cap) guarantee the
   `IsPronounceable` invariant, which the generator asserts on every result.
3. Names are `NameInfo` components on the named entity: language, scope, key, salt,
   generation and a 24-byte NUL-terminated text. Uniqueness is per scope: the yearly
   system retries the salt until the text is unused, so two regions never share a name
   while a river and a region may.
4. The `LanguageSystem` runs after Population, founds languages, drifts them, then
   names cultures, languages, settled regions, rivers and lakes whose source region is
   settled, and eras in the language of the largest culture. Every naming is a Named
   event with the entity as subject, so the chronicle can record it.

### Alternatives and decision rule

- Word lists or syllable data files: rejected; the kernel carries no external data and
  fixed tables in code keep names identical on every platform.
- Names as std::string fields: rejected; components are plain data (ADR-0011) and a
  fixed 24-byte text serialises without a length prefix.
- Global uniqueness across scopes: rejected; scope-local uniqueness keeps salts low
  (at most 1 on AELVOR 128) and lets a river carry its region's name.
- Decided by robustness (the invariant is asserted, not hoped for) and determinism
  (hash stream, integer only); simplicity over linguistic realism.

### Consequences

- The sound tables are part of the frozen contract: moving an entry changes every
  name, so additions go at the end and any change re-freezes the naming digest.
- `MaxSalt` bounds the uniqueness search; a scope with more entities than the
  phonology can name distinctly leaves the rest nameless rather than duplicating.
- Persons (Phase 04) use `NameScope::Person` with the same generator.

### Status

Accepted 2026-09-05. Files: `Source/VaelenSim/Public/Vaelen/Sim/Naming.h`,
`Source/VaelenSim/Private/Naming.cpp`, `Tests/Sim/Test_Naming.cpp` (5 tests). Headless
VALIDATED on the six Linux presets; engine side UNVERIFIED.

---

## ADR-0027: Religions are entities born from a founding event, with believers per region bounded by its people, spreading along the graph and with migration

### Context

The master prompt wants religions to be born from cultures and events, to spread along
migration, to split, and to carry tenets that later phases (politics, persons, dialogue)
read. Phase 03 already has cultures and coarse population per region (ADR-0025), the
region graph (ADR-0021), eras with causes (ADR-0024) and languages (ADR-0026). Disasters
and omens arrive in 03.05 and must be able to found religions too.

### Decision

1. A religion is an entity whose component records the founding event id, and that id
   is never zero: foundings come only through a request queue that carries the causing
   event. Two sources exist today (a new era, a culture split where a faith is held) and
   `RequestFounding(region, cause, kind)` is the door for every later source.
2. Believers are counted per region in a `RegionFaith` component (four slots) and are
   bounded by the region's people: the yearly step clamps first, every conversion is
   capped by the live room, and a migration wave carries believers in the source's
   proportions and never beyond the destination's people. The bound is exact once the
   waves of the last tick are delivered (the bus dispatches at the start of the next
   tick).
3. Spread is local: the majority faith of a region converts a share of that region's
   unconverted and a smaller share of its neighbours' unconverted, decided on the
   start-of-year state in region order. Faith reaches a new region only from an
   adjacent majority or with a wave, so it follows the graph by construction.
4. A schism is a founding whose parent is the majority faith of the region; its tenets
   are the parent's with one or two axes moved. Tenets are eight bytes derived from the
   identity, exposed as data and not interpreted here.
5. Pending requests live in a singleton `FaithState` component (eight slots, first
   request per region wins) so a snapshot taken between a request and the yearly tick
   restores the request; the listener and the system hold no state.

### Alternatives and decision rule

- One religion per culture at founding: rejected; the prompt wants religions caused by
  events, and a culture without a faith is a valid state.
- Believers per person: not possible before Phase 04; per region matches the
  population model and keeps 500 years at 128 in a fraction of a second.
- Spread as a global diffusion: rejected; the graph-local rule is what the test can
  prove (a region gains a faith only next to a converted region) and what the world
  reads as history.
- Decided by robustness (bound enforced at every entry point, requests bounded and
  refused with a counter) and simplicity (one rule table, integer only).

### Consequences

- Four faith slots per region: a fifth faith cannot enter until one fades below the
  fade share; the refused counter and the unchanged state make this visible.
- A founding request in a region with nobody in it is refused at the yearly tick, not
  when requested, so the request's cause is still recorded in `Refused`.
- Religion names depend on the language of the founding culture existing; a religion
  founded before its culture's language is named the following year.

### Status

Accepted 2026-09-05. Files: `Source/VaelenSim/Public/Vaelen/Sim/Religion.h`,
`Source/VaelenSim/Private/Religion.cpp`, `Source/VaelenSim/Public/Vaelen/Sim/Naming.h`
(`NameScope::Religion`), `Tests/Sim/Test_Religion.cpp` (5 tests). Headless VALIDATED on
the six Linux presets; engine side UNVERIFIED.

---

## ADR-0028: Disasters are drawn from world hazards, announced by an omen a year ahead and caused by it, with consequences through the existing request doors

### Context

The prompt asks for yearly random events tied to the world (drought from moisture,
flood from rivers, eruption from mountains, plague from population density) with causal
consequences in population and religion, and for every historical fact to be an event
with a cause. Phase 03 already offers two request doors: `EraSystem::RequestEra(cause)`
and `ReligionSystem::RequestFounding(region, cause, kind)`.

### Decision

1. Hazards are derived, not stored: one pass over the tiles gives each region its mean
   moisture, river share and mountain share (tiles at least `MountainElevation` above
   the sea; the Alpine biome starts at 2 500 m and is absent at small sizes), cached by
   the region layer digest. Plague risk is computed each year from people per tile.
2. An omen is an event about the region carrying the kind and the risk; it is queued in
   a singleton component (32 slots, overflow counted). The next year the omen strikes
   with a flat chance and a severity that escalates with the risk, and the disaster is
   an event whose cause is the omen. A record entity keeps kind, region, severity,
   deaths and the omen id, so "why did this happen" always resolves.
3. Deaths are taken per culture in proportion through `RegionPopulation::Remove`, so the
   population bookkeeping stays exact; the majority faith loses a share of its
   believers; a disaster at or above `FoundingSeverity` in a faithless region requests a
   founding; one at or above `EraSeverity` with at least `EraDeaths` deaths requests an
   era. Consequences go through the doors, never by touching the other systems' state.
4. Randomness is the system's own stream (a function of world seed, system name and
   tick), so replay and snapshots reproduce every omen and strike.

### Alternatives and decision rule

- Striking in the same tick as the omen: rejected; the year of warning is what makes
  omens readable history and gives 03.07 something to narrate.
- Unbounded omen queue: rejected; a bounded queue keeps the state a fixed-size
  component, and the dropped counter makes the bound visible (2 144 dropped in the
  cursed test, 0 in the reference run).
- Hazards as components: rejected; they are a pure function of the map and would only
  duplicate state that the snapshot already carries.
- Decided by robustness (every disaster provably caused and placed) and simplicity
  (one rule table, one pass over the tiles).

### Consequences

- Frequencies are set by the rule table (about 250 disasters in 500 years on the
  99-region reference continent, a dozen of them severe, a few eras and faiths born of
  them); a balance change re-freezes the disaster digest.
- Disasters kill only within the struck region; famine spreading to neighbours or
  plague travelling with migration are left to later phases.
- The DisasterInfo entity uses `IdKind::Entity`; a dedicated kind can be added without
  changing the record.

### Status

Accepted 2026-09-05. Files: `Source/VaelenSim/Public/Vaelen/Sim/Disasters.h`,
`Source/VaelenSim/Private/Disasters.cpp`, `Tests/Sim/Test_Disasters.cpp` (5 tests).
Headless VALIDATED on the six Linux presets; engine side UNVERIFIED.

---

## ADR-0029: The pre-history is one object that owns the Phase 03 systems and one call on a fresh world, frozen per century as the starting state

### Context

Phase 04 needs a starting state: regions with peoples, faiths and names, a chronicle
with causes, named eras. Six systems and two listeners from 03.01 to 03.05 produce it,
each with its own types, rules and wiring; every test so far assembled them by hand.
The prompt wants "a `GeneratePreHistory` call" and a reference run frozen per century.

### Decision

1. `PreHistory` is the one place that assembles Phase 03: it declares every type,
   owns the systems and listeners, wires the optional couplings (era and religion
   naming, faith shaken by disasters, eras opened by catastrophes, the chronicle's
   event types) and adds the systems before `World::Build`. The systems stay separate
   classes with separate rules; the object only composes them.
2. `Generate` works on a fresh world only (no history state, clock at its start) and
   refuses otherwise without touching anything; a restored snapshot is continued with
   `Run`. So a world has exactly one origin: generated once, or loaded.
3. The reference is the AELVOR 256 run with one state digest per century and the log
   digest at 500 years, and it must be reached identically in one call or century by
   century; the test also proves a mid-history snapshot continues to the same digests.
4. The report is a struct of the existing measures plus digests; its text form is
   deterministic so later tooling can diff runs.

### Alternatives and decision rule

- A free function that creates systems on the heap and leaks them into the world:
  rejected; ownership must be explicit for snapshots, tests and the engine wrapper.
- Freezing only the final digest: rejected; per-century digests localise a divergence
  to a century when a compiler or a rule changes.
- Running at a coarser LOD than the systems declare: not needed; the world LOD
  already runs the yearly systems once a year and migration monthly.
- Decided by robustness (one origin, refusals without side effects) and simplicity.

### Consequences

- The chronicle records era, culture, settlement, language, religion and disaster
  events, not omens, waves or conversions: a 500-year run keeps a few hundred records.
- The frozen century digests change whenever any Phase 03 rule or system changes; the
  test names the century that moved.
- Phase 04 receives the world through the same object, so its systems are added the
  same way after Phase 03's.

### Status

Accepted 2026-09-06. Files: `Source/VaelenSim/Public/Vaelen/Sim/PreHistory.h`,
`Source/VaelenSim/Private/PreHistory.cpp`, `Tests/Sim/Test_PreHistory.cpp` (5 tests).
Headless VALIDATED on the six Linux presets; engine side UNVERIFIED.

---

## ADR-0030: History is queried through the log and the records, and read as text built from names with deterministic fallbacks

### Context

The prompt wants "why did this happen" answered from any entity or event, a timeline
per region, and the chronicle readable as text. The kernel has the event log with
cause links (ADR-0014), Record entities per chronicled event with era and region
(ADR-0024) and names as components (ADR-0026). The text must be the same on every
platform and after a snapshot, and must never depend on the presentation layer.

### Decision

1. Queries are pure functions over the log and the components. An event explains
   itself by `CauseChain`; an entity explains itself through its origin, the earliest
   record about it, so a religion leads to its founding event and from there to the
   disaster or era that caused it. Every step carries the era at its tick and the
   region of its subject.
2. Timelines are the records about a region in tick then id order; they partition the
   placed records, so a region's story and the chronicle agree.
3. Text is built in the kernel from fixed English templates per event type, the names
   given in 03.03 and deterministic fallbacks ("region 12", "entity 0") when a thing
   has no name yet. Unknown event types get a generic line rather than nothing, so a
   later phase's events are never silently dropped from the chronicle. The text of
   AELVOR 128 after 300 years is frozen as an FNV digest.
4. Disaster kinds read as common nouns inside a sentence; names keep their capital.

### Alternatives and decision rule

- Localised or data-driven templates: deferred to the presentation and modding
  phases; the kernel text is a reference and a debugging tool, not the final prose.
- Storing text on records: rejected; text is derived, names can change (drift) and
  the log plus the components are enough to rebuild it.
- Decided by determinism (a frozen text digest across compilers) and simplicity (one
  function per query, no state).

### Consequences

- A change in any template, name rule or chronicled event set re-freezes the text
  digest; the test names the line count too, so a missing record is visible.
- `NameEntity` walks the component pools in a fixed order to classify an unnamed
  entity; a new entity kind gets its fallback by adding one case.
- Persons and documents (Phases 04 and 12) reuse `DescribeEvent` by adding cases.

### Status

Accepted 2026-09-06. Files: `Source/VaelenSim/Public/Vaelen/Sim/HistoryText.h`,
`Source/VaelenSim/Private/HistoryText.cpp`, `Tests/Sim/Test_HistoryText.cpp` (5
tests). Headless VALIDATED on the six Linux presets; engine side UNVERIFIED.

---

## ADR-0031: Believers never exceed the living at any tick boundary: carries round up and deaths take believers first

### Context

ADR-0027 promised that believers never exceed a region's people "once the waves of the
last tick are delivered". The Phase 03 gate checked that bound every decade over 2000
years and found two leaks: the migration carry rounded the believers down, so a source
region kept a few more than its share after every wave until the yearly clamp; and a
disaster killed people in a system that may run after the yearly clamp, leaving the
believers above the living for a year.

### Decision

1. The carry is rounded up and never put back: the source loses at least its
   proportional share of believers, and what the destination cannot hold (no slot, no
   room) is not counted anywhere. With believers <= people before a wave, the bound
   holds after it.
2. A disaster takes the dead from the believers of the struck region, faith by faith in
   proportion, before shaking the majority faith.
3. The bound is therefore exact at every tick boundary except for a source region whose
   wave of the last tick is still travelling; the gate test exempts exactly those.

### Alternatives and decision rule

- Ordering the systems so the clamp runs last: rejected; system order is by name hash
  and a rule that depends on it would be fragile.
- Conserving believers across a refused carry: rejected; conservation was never
  promised, the bound was.
- Decided by robustness: an invariant checked over 200 decades beats one argued for.

### Consequences

- The religion, disaster and pre-history digests from year 300 on are refrozen; the
  chronicle text digest is unchanged.
- The reference continent after 2000 years: 68 cultures, 33 religions (31 schisms),
  1 137 disasters, 32 eras, 1 453 records, people at 91 percent of capacity.

### Status

Accepted 2026-09-06. Files: `Source/VaelenSim/Private/Religion.cpp`,
`Source/VaelenSim/Private/Disasters.cpp`, `Tests/Sim/Test_HistoryGate.cpp` (2 tests).
Headless VALIDATED on the six Linux presets; engine side UNVERIFIED.

---

## ADR-0032: Population has two grains and one truth: persons exist only in detailed regions and always sum to the coarse counts

### Context

Phase 04 brings persons. The prompt wants simulation LOD 0-4: full detail where the
player is, statistics far away. Phase 03 already keeps integer counts per culture and
believers per faith on every region, frozen across compilers, and every later system
(economy, politics) will read those counts. Instantiating a person for every one of
the two million people of a 1024 world is neither needed nor affordable.

### Decision

1. The coarse counts of Phase 03 remain the truth everywhere. A region can be promoted
   to detail: one `PersonInfo` entity per counted person, culture by culture, faith
   handed out in slot order, sex and age drawn from a hash stream of the world seed, the
   region and the tick. A detailed region carries a `RegionDetail` component.
2. Demotion folds the living persons back into the counts (counts per culture and
   believers per faith become what the persons say) and destroys every person of the
   region. A promote / demote round trip without life events leaves the counts as they
   were; with deaths in between, the counts follow the persons.
3. `IsConsistent` states the invariant between the two grains for a detailed region,
   and `MeasureDetail` counts the regions where it fails, so the life-cycle systems of
   04.02 onwards are checked against it every year.
4. `VaelenPopulation` is a third kernel module with the same rules as `VaelenSim`
   (pure C++20, dual build, purity checked) and depends on it; persons use
   `IdKind::Person` and a 64-byte padding-free component.

### Alternatives and decision rule

- Persons everywhere: rejected; a 1024 world holds two million people and the coarse
  systems already give the far world its history.
- Persons as a view over the counts (no entities): rejected; persons need identity,
  lineage and names that survive snapshots, which entities with persistent ids give.
- Decided by robustness (one truth, an invariant that can be checked) and evolvability
  (the LOD bridge of 04.06 only has to move regions between the two grains).

### Consequences

- The tick of a promotion is part of the draw: promoting the same region a tick later
  materialises different persons, on purpose, so replay stays exact.
- Until 04.02, the coarse systems keep moving the counts of a detailed region while its
  persons stand still; `IsConsistent` then reports the drift, which is what 04.02 must
  close.
- The persons digest of AELVOR 128 after 300 years (19 781 persons) is frozen and
  reproduced on four compilers through CI.

### Status

Accepted 2026-09-06. Files: `Source/VaelenPopulation/*` (module rules, CMake, API
header, `Persons.h/.cpp`, `VaelenPopulationModule.cpp`), `Tests/Population/*`. Headless
VALIDATED on the six Linux presets; engine side UNVERIFIED.

---

## ADR-0033: In a detailed region the persons drive the counts and the coarse systems observe an opt-in LOD marker

### Context

ADR-0032 left the coarse systems of Phase 03 moving the counts of a detailed region
while its persons stood still. Two drivers of one number cannot both be right: either
the counts follow the persons or the persons follow the counts, and the choice must
not change a single Phase 03 digest, because those are frozen on four compilers.

### Decision

1. In a detailed region the persons are the truth. Every year the `LifeSystem` ages
   them, kills by age band, lets couples have children and then rewrites the region's
   counts per culture and believers per faith from the living. Capacity stays the
   region's.
2. The coarse systems leave detailed regions alone. `RegionLod` is a marker component
   whose type is declared by the population module, not by the pre-history; the
   population, migration and disaster systems learn it through `ObserveLod` and skip
   marked regions (no growth, decline, assimilation, abandonment or split; no wave in
   or out; disasters strike but do not kill there until the needs of 04.04 handle
   persons). A world that never declares the marker behaves exactly as before, so every
   Phase 03 digest is unchanged.
3. Births scale with the room left in the region (480 per mille per fertile woman at
   full room, 50 at capacity), which settles a detailed region near 80 percent of its
   capacity, where the coarse logistic model keeps its regions. A promotion or a
   demotion therefore changes the grain, not the size of the population.
4. Every birth and death is an event about the person (PersonBorn, PersonDied), so
   04.07 can chronicle the ones that matter and 04.04 can attach causes.

### Alternatives and decision rule

- Persons following the counts (materialising the coarse growth, killing to match the
  coarse deaths): rejected; the coarse rules know nothing of age or couples, and the
  persons would carry no history of their own.
- A flag inside `RegionPopulation`: rejected; a layout change would bump the save
  format and refreeze every Phase 03 digest for a bit the pre-history never sets.
- Ordering the systems so a reconciliation always runs last: rejected; system order is
  by name hash and the marker makes the order irrelevant.
- Decided by determinism (opt-in, digests untouched) and robustness (one driver per
  number, an invariant checked every year).

### Consequences

- Migration waves never reach or leave a detailed region; emigration and immigration
  of persons belong to the LOD bridge of 04.06.
- Disasters in detailed regions publish their events but kill nobody until 04.04 ties
  needs, famine and disease to persons; the gate of 04.08 will check that no such gap
  remains.
- The life tables are a public rule table; a balance change refreezes the lives digest.

### Status

Accepted 2026-09-06. Files: `Source/VaelenPopulation/Public/Vaelen/Population/Lives.h`,
`Source/VaelenPopulation/Private/Lives.cpp`, `Persons.h/.cpp`,
`Source/VaelenSim/Public/Vaelen/Sim/Population.h`, `Private/Population.cpp`,
`Disasters.h/.cpp`, `PreHistory.h`, `Tests/Population/Test_Lives.cpp` (5 tests). Headless
VALIDATED on the six Linux presets; engine side UNVERIFIED.

---

## ADR-0034: Families are entities founded by grooms, lineage is read from the parent links, and children are born to couples

### Context

The prompt wants lineage: who descends from whom, which family a person belongs to,
who inherits. Persons (ADR-0032) carry mother and father indices; the life system
(ADR-0033) picked any eligible man as father. Families need an identity that outlives
their members, and kinship must be answerable without storing a tree.

### Decision

1. A family is an entity: culture, home region, founder, head, generation, founding
   and extinction ticks. A groom without a family founds one on his marriage; the bride
   joins it; children are born into their mother's family, which is her husband's.
   Descent is therefore patrilineal by default, and the rule table can change who
   founds and who joins without touching the queries.
2. Marriages are drawn yearly by the grooms in index order: the n-th eligible bride of
   the same region, culture and faith, of age and within the age gap, not kin within
   two generations. The dead release their spouses at the next yearly tick. Every
   marriage, founding and extinction is an event.
3. Lineage is a set of pure queries over the parent links (ancestors, descendants,
   siblings, kinship within a depth, living members of a family), each building one
   index of the persons; the family system builds that index once per tick, so the
   kinship test inside the marriage loop is a lookup, not a pool scan.
4. `PersonInfo` gains `Spouse` inside its reserved tail (the layout stays 64 bytes and
   padding-free), and `LifeRules.SpouseRequired` makes births couples-only in worlds
   with families while staying off by default, so the 04.02 digest is unchanged.

### Alternatives and decision rule

- Storing children lists on persons or families: rejected; the parent links already
  define the tree and lists would have to be kept consistent through every death and
  demotion.
- Matrilineal or bilateral families: not chosen as default but not excluded; the
  founder and joiner are decided in one place.
- Decided by robustness (no derived structure to keep in sync) and simplicity.

### Consequences

- Widows and widowers remarry; a person can appear in several marriage events, and
  the family a child is born into is the one at the time of birth.
- Families go extinct and stay in the world as history; the head of a living family is
  always a living member, replaced by the eldest when the head dies.
- Kinship within two generations forbids marrying a sibling, a parent or a
  grandparent's line; cousins are allowed. A stricter rule is one number away.

### Status

Accepted 2026-09-06. Files: `Source/VaelenPopulation/Public/Vaelen/Population/Families.h`,
`Source/VaelenPopulation/Private/Families.cpp`, `Persons.h`, `Lives.h/.cpp`,
`Tests/Population/Test_Families.cpp` (5 tests). Headless VALIDATED on the six Linux
presets; engine side UNVERIFIED.

---

## ADR-0035: Needs are yearly integers fed by the region's ration, and disasters reach persons through the event log

### Context

Since 04.02 the coarse disaster system kills nobody in a detailed region: the persons
must feel the droughts and plagues themselves, with the death traceable to the
disaster's event (the prompt's causality). Persons also need a body - food and health -
that later phases (traits, the player, economy) can read and push.

### Decision

1. `PersonNeeds` is a small padding-free component (food, health, a rest slot, hungry
   years) added lazily to every living person of a detailed region by the need system;
   a demotion drops it with the person. Values are 0..255 integers moved once a year.
2. The ration of a region is its capacity over its living, cut by the droughts that
   struck it during the year; food refills by the ration and burns by a constant, hunger
   under a year's worth wears health down by a base plus a draw that grows with the
   deficit, and a fed year restores it; a plague strikes a random share of the persons
   for a draw bounded by the severity; infants and elders take more. Zero health is
   death, so a single bad year weakens and a second one kills.
3. The need system reads the DisasterStruck events of the last year from the event log
   rather than subscribing: the log is the source of truth, replay-safe, and the
   coarse record (deaths 0 in a detailed region) stays as written. Each death carries
   its cause code in the payload and the DisasterStruck id as the event cause, so
   why-chains reach from a person's death to the disaster and its omen.
4. Hunger without a drought is starvation (a region past its capacity), with no cause
   id: the world, not an event, is to blame.

### Alternatives and decision rule

- A FaithListener-style listener on DisasterStruck: rejected; the coarse system runs
  in the same yearly tick and dispatch is next-tick, the log read is simpler and
  replayable.
- Continuous (per-tick) needs: rejected for Phase 04; the yearly grain matches the
  life system and the disaster system. Phase 10 (the player) will refine rest and
  daily needs where the player is.
- Decided by robustness and determinism (one ordered pass over persons by index, one
  random draw per person per plague).

### Consequences

- Famine and disease now reduce the counts of detailed regions through the persons,
  by the same reconciliation as every other death; a detailed region is no longer
  spared by its detail.
- The 04.01-04.03 frozen digests hold because their worlds carry no need system; a
  world with needs has its own frozen digest.
- Rest is a reserved slot until Phase 10.

### Status

Accepted 2026-09-06. Files: `Source/VaelenPopulation/Public/Vaelen/Population/Needs.h`,
`Source/VaelenPopulation/Private/Needs.cpp`, `Tests/Population/Test_Needs.cpp` (6 tests).
Headless VALIDATED on the six Linux presets; engine side UNVERIFIED.

---

## ADR-0036: Traits are drawn from the identity and the parents, skills are earned year by year, and persons are named by their language

### Context

Later phases (traits driving choices, the player meeting persons, dialogue) need
persons who differ from one another in a stable, explainable way, and who can be
called by a name in the tongue of their people. Persons already carry an identity
hash (ADR-0032) and a language (04.01); Phase 03 has the phonologies and the naming
function.

### Decision

1. Six traits, 0..255 with 128 ordinary, drawn once from the identity: three bytes of
   a lattice draw averaged give a bell with real tails, without floating point. A child's
   draw is pulled toward the mean of its parents by a heritability rule (half by default),
   so lineages have a temper without a genetics model.
2. Four skills, 0..255, that start at zero and move once a year in detailed regions: a
   draw scaled by the trait behind the skill from 8 to 45, an apprenticeship bonus from a
   parent who knows the trade until 15, a cap from the trait, and a slow fading from 60.
   Skills are earned in the simulation, never drawn.
3. Names are NameInfo components of scope Person on the person entity, built by
   `GenerateName` from the phonology of the person's language (or its culture's latest),
   keyed by the identity; namesakes are allowed, no salt loop and no Named event, so
   naming thousands of persons costs one draw each and does not swell the log.
4. Traits and names live in their own components: the persons digest of a world without
   the trait system is unchanged, and a demotion drops them with the person.

### Alternatives and decision rule

- A random draw per trait from the tick's stream: rejected; traits would then depend on
  the tick order of creation rather than on the person, and a replay from a snapshot
  could not re-derive them.
- Unique person names per language: rejected; real populations share names, and the
  uniqueness scan is quadratic in the persons of a region.
- Decided by determinism (identity-derived) and simplicity.

### Consequences

- Traits are queryable and explainable: "she has her mother's will" is a computation.
- The trait behind each skill (vigour, wit, boldness, piety) is a table; Phase 05 and
  later can add skills without touching the growth loop.
- Names are not unique; the family (04.03) and the index tell namesakes apart.

### Status

Accepted 2026-09-06. Files: `Source/VaelenPopulation/Public/Vaelen/Population/Traits.h`,
`Source/VaelenPopulation/Private/Traits.cpp`, `Tests/Population/Test_Traits.cpp` (5 tests).
Headless VALIDATED on the six Linux presets; engine side UNVERIFIED.

---

## ADR-0037: Detail is requested, not decided by the kernel, and people cross the grain border as events

### Context

Persons exist only in detailed regions (ADR-0032). Something has to decide which
regions are detailed - the player's region and its neighbours in the game, a focus of
interest in a headless run - and the two grains must exchange people, or a detailed
region would be an island the coarse migrations flow around (04.02 made the coarse
systems leave it alone).

### Decision

1. The kernel does not decide: a singleton `LodState` lists the regions the world
   wants detailed (up to 8, in request order) through `RequestDetail` / `ReleaseDetail`,
   and the yearly `LodSystem` applies it - demotions first, then promotions up to a
   rule's limit, an empty region refused and counted. Presentation and gameplay only
   ever write requests; the promotion itself stays a kernel operation with a tick.
2. People cross the border in the bridge, not in the coarse migration: a crowded
   detailed region (above three quarters of its capacity) sends a share of its crowd -
   unmarried adults of 16 to 40, in index order - to the coarse neighbour with the most
   room (below two thirds); their persons are destroyed and the destination's counts and
   faith are raised. A crowded coarse neighbour sends a share of its crowd into a
   detailed region with room as new persons of its majority culture and faith, with an age and sex drawn from the tick's
   stream and an identity from the world seed. Each crossing is a PersonLeft or
   PersonArrived event about the person, with the other region in the payload.
3. After the crossings the region is reconciled, so the counts stay the aggregate of
   the persons, and a promote / demote cycle without a tick conserves the counts and
   the faith slots exactly (the persons carry culture and faith one by one).

### Alternatives and decision rule

- Letting the coarse migration waves touch detailed regions and materialising the
  movers: rejected; the wave logic is tuned for counts and would have to know about
  persons, families and ages.
- Keeping emigrants as persons in a coarse region: rejected; a person without a
  detailed region has no life system, and half-alive persons would break the
  invariant that persons and counts agree.
- Decided by robustness (one owner of the border) and by the layering rule (the
  kernel never guesses what the presentation wants).

### Consequences

- Emigrants lose their detail; if their region is detailed again later they are new
  persons. Their departure is in the log, so history can still tell it.
- The lines sit where the coarse world lives (regions settle near three quarters of
  their capacity), so emigration flows in ordinary centuries while immigration needs a
  detailed region emptied by famine, plague or a widened capacity.
- The state is a component: a snapshot restores the wanted list and the tallies, and
  the 500-year alternation test replays identically from its year 250 snapshot.
- Phase 10 will request the player's region and neighbours; Phase 07 can request a
  region for a chronicle's focus.

### Status

Accepted 2026-09-06. Files: `Source/VaelenPopulation/Public/Vaelen/Population/Lod.h`,
`Source/VaelenPopulation/Private/Lod.cpp`, `Tests/Population/Test_Lod.cpp` (5 tests).
Headless VALIDATED on the six Linux presets; engine side UNVERIFIED.

---

## ADR-0038: Persons enter the chronicle through a capped listener that decides what matters at dispatch

### Context

A detailed region publishes thousands of person events a century (ADR-0033): births,
deaths, marriages, houses. The chronicle (ADR-0029, Phase 03) records every event of
its subscribed types as a Record entity; subscribing it to the person events would
drown the history of peoples and faiths under the lives of one region, yet the prompt
wants persons in history - a famine's dead, a house that died out, the why of a death.

### Decision

1. A second listener, `PersonChronicle`, subscribes to the person event types and
   decides at dispatch, against the world, what matters: a house founded or died out,
   a death with a cause id (famine, plague), the death or marriage of a head of house,
   the chronicle's focus moving; crossings are off by default. Rules switch each.
2. What matters becomes the same `RecordInfo` entity the Phase 03 chronicle writes,
   with the region taken from the payload (a person is not a region entity) and the
   era at the tick, so every Phase 03 query (timelines, why-chains, the chronicle
   export) sees person records without change.
3. A cap per year and region bounds the records: the first N that matter are kept, the
   rest counted as dropped in a snapshot-safe state component. The events themselves
   stay in the log; only the chronicle is selective.
4. Text is a pure function of the log and the state: one sentence per person event with
   the Phase 03 prefix, names through the naming components, fallbacks that never
   fail; a person's story is its timeline followed by the "because" lines of its
   death's cause chain.

### Alternatives and decision rule

- Recording every person event: rejected; the chronicle is meant to be read.
- Deciding what matters at publish time in each system: rejected; the systems would
  each need the chronicle's rules, and the listener sees the whole world at dispatch.
- Decided by simplicity (one owner of what matters) and evolvability (rules and
  cap are numbers; Phase 07 can add its own kinds of "matters").

### Consequences

- The cap makes a crowded year lossy for the chronicle, never for the log; the
  dropped count says how lossy.
- Heads are recognised at dispatch, one tick after the event: the family system
  replaces a dead head at its next yearly tick, so the head is still on its family.
- Person lines depend on names (04.05); without the trait system they fall back to
  "person N" and stay deterministic.
- A person who leaves a detailed region is kept as `LifeState::Gone` rather than
  destroyed (a change to 04.06): history can still name the one who left; a demotion
  of the region drops them with the rest.

### Status

Accepted 2026-09-06. Files: `Source/VaelenPopulation/Public/Vaelen/Population/PersonHistory.h`,
`Source/VaelenPopulation/Private/PersonHistory.cpp`, `Tests/Population/Test_PersonHistory.cpp`
(5 tests). Headless VALIDATED on the six Linux presets; engine side UNVERIFIED.

---

## ADR-0039: The Phase 04 gate runs every population system over the 256 pre-history and freezes the state at 250 and 500 years

### Context

Phase 04 added six systems and a listener on top of the Phase 03 world. Each task
froze its own reference in a world that holds only the systems it needed; nothing had
yet run all of them together for centuries over the reference world, and the exit
criteria (ROADMAP section 2) ask for a long-duration test and frozen values that guard
the save format.

### Decision

1. The gate is one reference run: AELVOR 256 after 300 years of pre-history, the busiest
   region requested through the bridge, 500 years with lives (births to couples),
   families, needs, traits and names, the bridge and the person chronicle, every yearly
   system in its scheduler order.
2. Every Phase 04 invariant is checked every decade on the live state, each with its own
   message: one detailed region and the two grains in agreement, needs, traits and a
   name on every living person, ages bounded, spouse links whole, heads alive and in
   their house, caused deaths pointing at a disaster of their region, coarse
   bookkeeping exact and believers bounded, the chronicle resolving with every person
   record described.
3. The state digest at 250 and 500 years, the persons digest and the log digest are
   frozen and reproduced by clang, gcc, MSVC and AppleClang through CI; a snapshot at
   year 250 restored into a fresh object continues to the same year 500.
4. The gate is the last task of the phase; a deliberate rule change in any Phase 04
   system refreezes the gate's digests, and the refreeze is recorded in the task's
   docs. A world that runs needs and the bridge orders the family system after them
   (`FamilySystem::RunAfter`), so every head and spouse link is whole at the end of
   every yearly tick; the dependency is declared by the world, never assumed by the
   system, because a system may only depend on what exists. Emigration flows in ordinary centuries; immigration into a detailed region
   needs a shock (famine, plague, a widened capacity), which the run records.

### Alternatives and decision rule

- Freezing only per-task digests: rejected; the interplay (families under famine,
  names of arrivals, records of houses dying out) is what the phase promises.
- Two thousand years like the Phase 03 gate: rejected for now; persons cost more per
  year and the 500-year run already covers twenty generations; Phase 16 (performance)
  will revisit the length.
- Decided by robustness (every invariant, every decade) and determinism.

### Consequences

- The gate takes the longest of the Population suites; its CTest entry keeps the
  default timeout on Linux and the Shuffled entry keeps its 1200 s allowance.
- A red gate on one compiler with green per-task suites points at the interplay; the
  per-decade messages say which invariant and when.

### Status

Accepted 2026-09-06. Files: `Tests/Population/Test_PopulationGate.cpp` (1 test).
Headless VALIDATED on the six Linux presets; engine side UNVERIFIED.

---

## ADR-0040: Organisations are entities seated in a region, filled from its persons, and kept as counts when the region is coarse

### Context

Phase 05 opens with organisations: the prompt wants councils, temples, guilds and
warbands that persons found, join and leave, that survive their members and that the
later phases (politics, war, economy) can act through. Persons exist only in detailed
regions (ADR-0032); an organisation must outlive a demotion.

### Decision

1. An organisation is an entity of kind `Organization` with a seat region, a kind, a
   culture, a faith for a temple, a head, a member count, a number of seats, founding
   and disbanding ticks and an identity from the world seed. The kinds are a table;
   05.01 fills councils and temples, 05.05 the guilds, warbands and clans.
2. Membership is a component on the person (one organisation per person in 05.01, with
   a role), so a person's affiliation is read where the person is, and a demotion drops
   memberships with the persons while the organisation keeps its last member count.
3. Seats are filled by rules of the kind from the region's living, in a deterministic
   order: a council by the heads of the largest houses (house size, then person index),
   a temple by the most pious of its faith (piety, then index); free persons of age
   only. Seats are kept until death or departure; a head is seated when none of the
   members is the head. The empty are disbanded after a few years in a detailed region,
   never in a coarse one, and the world keeps the disbanded as history.
4. The system runs after Families and, where they exist, after Needs and Lod
   (`RunAfter`), so every year's deaths and departures are seen before the seats are
   judged. The society module depends on the population module; nothing in the
   population module knows about organisations.

### Alternatives and decision rule

- Organisations as counts only (coarse everywhere): rejected; persons must sit in them
  for offices, standing and the chronicle to mean anything.
- Members stored on the organisation: rejected; a list would need rewriting on every
  death and demotion, and a person's affiliation would need a scan.
- Decided by robustness (one owner of each link) and evolvability (kinds and rules are
  tables; the fill order is one comparator per kind).

### Consequences

- A coarse organisation's member count is stale by construction until the next
  promotion; it is a memory, not a truth, and is never judged against persons.
- The organisations digest hashes every organisation in index order; heads and counts
  make it sensitive to every seat decision.
- 05.02 reads memberships and roles as offices for standing.

### Status

Accepted 2026-09-06. Files: `Source/VaelenSociety/*`, `Tests/Society/Test_Organizations.cpp`
(5 tests). Headless VALIDATED on the six Linux presets; engine side UNVERIFIED.

---

## ADR-0041: Standing is recomputed every year from what the world already knows, and tiers are shares of a region's adults

### Context

Social structure needs a notion of who stands above whom: for offices, for the
chronicle, for the player's dealings (Phase 10) and for bondage (05.04). The world
already carries what standing comes from - houses and their heads (04.03), traits and
skills (04.05), seats and their heads (05.01). Storing standing as an independent truth
would create a second source that drifts from the first.

### Decision

1. Standing is a yearly computation, not a stored truth: a score from the size of the
   house and its headship, the age band, charm and will, the best skill and the offices
   held, with every weight in a rule table and the score function pure and tested point
   by point. The component that holds score, rank, tier and offices is a cache of that
   computation, dropped for the dead, the gone, the young and the coarse.
2. Rank is the position in the region's order of living adults, scaled to 0..255, ties
   broken by person index; tiers are shares of the region (5 percent elite, 15 percent
   notable), so every region has an elite however poor or rich, and a region's elite is
   a query (`EliteOf`) rather than a list to maintain.
3. The system runs after Organizations, so the year's seats and heads count; nothing
   downstream writes standing back.

### Alternatives and decision rule

- Absolute thresholds for the tiers: rejected; scores grow with the rules and with
  house sizes, and the game needs "the elite of this region" whatever its scale.
- Wealth as a component of standing: deferred to Phase 06 (economy), where wealth
  will exist; the score function takes what exists and the rule table will gain a
  weight.
- Decided by robustness (one source of truth, a pure function) and evolvability
  (weights and shares are numbers).

### Consequences

- Standing changes when houses, seats, ages or skills change, never on its own.
- The standing digest hashes every standing in person index order; it is sensitive to
  every weight and to every upstream rule.
- 05.04 will read the tier for who may hold whom; 05.07 will chronicle rises and
  falls between tiers.

### Status

Accepted 2026-09-06. Files: `Source/VaelenSociety/Public/Vaelen/Society/Standing.h`,
`Source/VaelenSociety/Private/Standing.cpp`, `Tests/Society/Test_Standing.cpp` (4 tests).
Headless VALIDATED on the six Linux presets; engine side UNVERIFIED.

---

## ADR-0042: Norms live on the culture and reach the lower modules through a small mirror they choose to observe

### Context

The prompt wants cultures that differ in how their people marry, descend, tolerate
and hold others, and that change with their history. The rules that govern marriage
sit in the population module (ADR-0034) as one table per world; the society module
sits above it and may not be included by it.

### Decision

1. A culture's customs are one component (`NormSet`) on the culture entity, drawn once
   from the culture's identity so that a seed gives the same customs on every machine,
   and inherited by a split culture from its parent with one custom of its own. Drifts
   come from events read from the log - a schism hardens the faith, a great disaster
   loosens the people - and each drift is an event with before and after.
2. The lower module defines the small struct it understands (`MarriageNorms`: marrying
   ages, gap, faith, eagerness) and an opt-in (`FamilySystem::ObserveNorms`); the society
   module mirrors its marriage customs into that struct on the same culture entity and
   keeps the mirror equal. Dependencies stay one-way: the population module knows a
   struct and a type, never the society.
3. A world that does not observe the norms is unchanged (the 04.03 digest holds); a
   world that does marries every culture by its own customs, with the family rules as
   the customs of any culture that carries none.

### Alternatives and decision rule

- Passing per-culture rules into the family system's constructor: rejected; customs
  change over time and are born with cultures the constructor never sees.
- The society module rewriting the family rules: rejected; one owner per truth, and the
  rules are a world constant.
- Decided by the layering rule and by evolvability (the mirror pattern extends to
  descent in 05.04 and to any later reader).

### Consequences

- Norms drift by rules of the society module; the lower module sees only the mirror.
- The norms digest hashes every NormSet in culture order; it is sensitive to every
  draw and every drift.
- Descent, tolerance, mobility and the bondage allowed are ready for 05.04 to 05.06.

### Status

Accepted 2026-09-06. Files: `Source/VaelenSociety/Public/Vaelen/Society/Norms.h`,
`Source/VaelenSociety/Private/Norms.cpp`, `Source/VaelenPopulation/Public/Vaelen/Population/Families.h`,
`Source/VaelenPopulation/Private/Families.cpp`, `Tests/Society/Test_Norms.cpp` (4 tests).
Headless VALIDATED on the six Linux presets; engine side UNVERIFIED.

---

## ADR-0043: Bondage is a state on the person with a living holder among the elite, and a count per region

### Context

The prompt names bondage and slavery as institutions of the world, to be simulated
without euphemism and without glorification: who falls into them, who holds whom, how
people leave them, and how a region's shape survives the grain. The customs (ADR-0042)
say which institutions a culture allows; standing (ADR-0041) says who the elite are.

### Decision

1. A bond is a component on the person: its kind (bonded, enslaved), how it was entered
   (debt, birth; capture waits for the wars of Phase 08), who holds it and since when.
   Free persons carry nothing. The holder is a living member of the region's elite
   who is not bound, chosen as the one with the fewest held; a holder's death frees
   the bonded (the debt dies with the creditor) and passes the enslaved on.
2. Entries and exits are events about the person with the reason in the payload and
   the birth event or the death event as the cause where one exists, so the chronicle
   (05.07) can say why. Hardening from bondage to slavery is a second entry event.
3. Each region carries its strata as counts, written from the living while the region
   is detailed and kept while it is coarse; 05.06 conserves them across a promotion.
4. Every rate is a rule; every gate is a custom of the person's own culture: a culture
   that forbids debt bondage has no debtors bound, one that forbids birth bondage has
   nobody born enslaved, and the world without customs has no bondage at all.

### Alternatives and decision rule

- Holders as organisations or regions rather than persons: rejected for now; a held
  person must be someone's for freedom by the holder's death to exist; the holder field
  admits 0 for a later institutional holder.
- Bondage as a tier of standing: rejected; standing is a rank among the free, bondage
  a state that removes one from it.
- Decided by robustness (one component, one owner, causes in the log) and by the
  prompt's insistence that institutions be explicit.

### Consequences

- The bondage digest hashes every bond then every strata; it is sensitive to every
  draw and every holder.
- The bound are still persons: they marry, work, are ranked and chronicled like the
  free; nothing in the lower modules knows about bonds.
- Phase 08 will add capture as an entry and flight across the border as an exit.

### Status

Accepted 2026-09-06. Files: `Source/VaelenSociety/Public/Vaelen/Society/Bondage.h`,
`Source/VaelenSociety/Private/Bondage.cpp`, `Tests/Society/Test_Bondage.cpp` (4 tests).
Headless VALIDATED on the six Linux presets; engine side UNVERIFIED.

---

## ADR-0044: Organisations act through one yearly system whose effects reach the lower modules as state they observe or as events they ignore

### Context

Organisations (ADR-0040) existed but did nothing. The prompt wants them to matter to
the simulation - to famine, to faith, to skills, to the wars to come - without the
lower modules (needs, faith, traits) knowing about them.

### Decision

1. One system, `Decisions`, runs after Organizations and makes every kind's decision
   in organisation index order, each a DecisionMade event with a value and a cause where
   one exists. The kinds are a table; each decision is a few lines against state the
   world already has.
2. Effects reach downward in two ways only. Through state the lower module chooses to
   observe: a council's grain is a `RegionStores` component on the region, defined in
   the population module, that the need system reads when told to observe the type
   (the mirror pattern of ADR-0042); a temple's converts are persons whose faith the
   temple sets and counts the population module reconciles. Or through events the lower
   modules ignore: a warband's raid is a RaidPlanned event for Phase 08, and a guild's
   training writes the skill it trains.
3. Guilds and warbands are founded and seated by the organisation system like councils
   and temples, from the skilled (craft, fighting) with the most skilled as head; the
   05.01 and 05.02 references are refrozen because those organisations now exist.

### Alternatives and decision rule

- A system per organisation kind: rejected; the order of decisions across kinds must
  be one deterministic order, and the kinds share the seat and cause machinery.
- Decisions as component flags read by the lower systems: rejected for effects with a
  natural home (grain on the region, faith on the person); the flag would duplicate it.
- Decided by the layering rule and by determinism (one order, index by index).

### Consequences

- The stores digest hashes every region's grain; famine deaths in a cursed run are
  fewer with a council than without.
- Preaching moves persons between faiths and the counts follow through the same
  reconciliation as births and deaths.
- Phase 08 consumes RaidPlanned; until then raids are history without consequence.

### Status

Accepted 2026-09-06. Files: `Source/VaelenSociety/Public/Vaelen/Society/Decisions.h`,
`Source/VaelenSociety/Private/Decisions.cpp`, `Source/VaelenSociety/Public/Vaelen/Society/Organizations.h`,
`Source/VaelenSociety/Private/Organizations.cpp`, `Source/VaelenPopulation/Public/Vaelen/Population/Needs.h`,
`Source/VaelenPopulation/Private/Needs.cpp`, `Tests/Society/Test_Decisions.cpp` (4 tests).
Headless VALIDATED on the six Linux presets; engine side UNVERIFIED.

---

## ADR-0045: The social shape of a region is a count that outlives its persons and binds the next ones

### Context

Persons exist only while a region is detailed (ADR-0032); the bonds on them (ADR-0043)
vanish with a demotion. A region that held a hundred enslaved should not be free the
day the player looks away and back.

### Decision

1. The strata (free, bonded, enslaved) are counts on the region, written from the
   living while the region is detailed and kept while it is coarse; a promote /
   demote cycle without a tick leaves them and the bondage digest untouched.
2. At a promotion the bondage system, running after the bridge, sees a region whose
   living carry no bond while its strata count some, and binds as many of its common
   adults again in index order - the enslaved first, each to the holder with the
   fewest held - with an entry of reason Promotion, so history knows these bonds were
   inherited from the count, not entered by a person's debt or birth.
3. A person who leaves over the border (ADR-0037) leaves the bond with a Departure
   exit, never a death; the destination's coarse strata are not changed (the count
   there is a memory of its last detail).

### Alternatives and decision rule

- Persisting the bound as persons through a demotion: rejected; the region's persons
  are folded into counts as a whole, and bonds without persons are meaningless.
- Binding at random rather than by index order: rejected; the promotion must be
  reproducible and the bound must be common adults, which the order already gives.
- Decided by determinism and by the two-grain rule (counts are the truth of the
  coarse grain).

### Consequences

- The set of persons bound after a promotion differs from the set before the demotion
  (the persons are new); the count is conserved up to the year's deaths and the
  holders' room, and the test bounds the difference.
- Organisations' member counts and standings already follow the persons; the strata
  were the last social count without a road back.

### Status

Accepted 2026-09-06. Files: `Source/VaelenSociety/Public/Vaelen/Society/Bondage.h`,
`Source/VaelenSociety/Private/Bondage.cpp`, `Tests/Society/Test_Strata.cpp` (3 tests).
Headless VALIDATED on the six Linux presets; engine side UNVERIFIED.

---

## ADR-0046: Each module chronicles its own events through its own capped listener into the one record store

### Context

The Phase 03 chronicle (ADR-0029) records the world's events, the Phase 04 person
chronicle (ADR-0038) the persons' that matter. The society module adds foundings,
decisions, customs and bonds, and the chronicle must stay one readable story with one
record store, one export and one why.

### Decision

1. The society module has its own listener with its own rules and cap, writing the same
   `RecordInfo` entities into the same pool; the history state's record count grows
   with all three. A custom changed is a record without a region (a culture is not a
   place); a raid is recorded through its RaidPlanned event, not its decision; a bond
   entered by a promotion is not recorded (the count was, at the demotion).
2. Text composes downward: the society text names organisations, cultures and faiths
   and hands every other event to the person text, which hands the rest to the history
   text, so one call describes any event and the prefix is the same at every level.
3. The why of an event is the Phase 03 cause chain rendered with the richest text
   available, so a council's grain explains itself by the drought that struck.

### Alternatives and decision rule

- One listener per world that knows every module's events: rejected; it would live in
  the highest module and every lower module would have to be re-described there.
- Records in a per-module pool: rejected; one export in tick order needs one pool, and
  the Phase 03 queries (timelines, checks) would miss the rest.
- Decided by the layering rule and by simplicity (one pattern, three uses).

### Consequences

- The chronicle text digest depends on every module's text; a wording change anywhere
  refreezes it.
- Phase 06 and later add their listener and their text layer the same way.

### Status

Accepted 2026-09-06. Files: `Source/VaelenSociety/Public/Vaelen/Society/SocietyHistory.h`,
`Source/VaelenSociety/Private/SocietyHistory.cpp`, `Tests/Society/Test_SocietyHistory.cpp` (3 tests).
Headless VALIDATED on the six Linux presets; engine side UNVERIFIED.

---

## ADR-0047: The Phase 05 gate runs every population and society system over the 256 pre-history and freezes the state, the log and the text

### Context

Phase 05 added five systems and a listener on top of Phase 04, several of them
reaching into the lower modules through observed state (customs, stores). Each task
froze its own reference; nothing had run every system together for centuries over the
reference world, and the exit criteria ask for it.

### Decision

1. The gate is one reference run: AELVOR 256 after 300 years of pre-history, the
   busiest region requested through the bridge, 500 years with every Phase 04 and 05
   system wired as the game will wire them - the family system observing the customs
   and running after needs, the bridge and norms; the need system observing the
   stores; the organisation and bondage systems running after the bridge - and both
   person and society chronicles attached.
2. Every invariant of both phases is checked every decade with its own message; the
   three chronicles are checked every fifty years; the state, the log and the chronicle
   text are frozen at 250 and 500 years and reproduced by four compilers; a snapshot at
   year 250 continues to the same year 500 with the same text.
3. As in ADR-0039, a deliberate rule change in any Phase 04 or 05 system refreezes the
   gate and is recorded in the task's docs; the gate keeps 1800 s of CTest time and the
   Society.Shuffled entry 5400 s for the MSVC debug runner.

### Alternatives and decision rule

- Reusing the Phase 04 gate with the society systems added: rejected; the Phase 04 gate
  stays the reference of Phase 04 alone, so a Phase 05 rule change cannot hide behind a
  Phase 04 refreeze.
- Decided by robustness and determinism.

### Consequences

- The chronicle text of the reference run is frozen; a wording change in any module's
  text refreezes it, which is the point: the story the player reads is a tested output.
- The two gates together take about ten minutes of clang debug time per preset.

### Status

Accepted 2026-09-06. Files: `Tests/Society/Test_SocietyGate.cpp` (1 test).
Headless VALIDATED on the six Linux presets; engine side UNVERIFIED.

---

## ADR-0048: Goods are counted kinds, held in common by regions and by the houses of a detailed region, and conserved across the grains

### Context

Phase 06 gives the world a material side: famine, standing, the decisions of Phase 05
and the wars of Phase 08 need goods that exist somewhere, that persons and houses can
own, and that survive a region being promoted and demoted (ADR-0032). Phase 09 will make
named things; Phase 06 must count.

### Decision

1. A good is a kind in a table (grain, cloth, tools, ore, timber, salt, luxuries), never
   an entity. A stock is a small array of counts by kind, 32 bytes, padding free.
2. Two owners of stock, one per grain: `RegionStock` on the region entity is what the
   region holds in common, and its whole stock while it is coarse; `HouseStock` on the
   family entity is what a house holds while its home region is detailed. Persons hold
   nothing in Phase 06 (items are Phase 09).
3. The land endows every region exactly once - grain from its capacity, timber, ore, salt
   and luxuries from the richness of its deposits - so that every later system starts
   from a stock that follows the map (Phase 02) and the demography (Phase 03). Production
   (06.02) then moves the counts; the endowment is never repeated.
4. A promotion splits a share of the common stock among the region's living houses by
   their living members, the rest staying in common; a demotion folds the houses' goods
   back; an extinct house returns its goods to the common stock; a house founded later
   starts with nothing. So the whole of a region - common and houses - is the same before
   and after every grain change: nothing is made or lost by the level of detail, which is
   the invariant the tests hold for a century.
5. Every change is an event about the region or the house; goods moved by hand
   (`AddStock`) are clamped at zero and at the top, return the units actually moved and
   carry a cause, so that the chronicle (06.07) and the why can follow them.

### Alternatives and decision rule

- Stocks on persons from the start: rejected; a person's share would have to be folded
  at every death and departure, and Phase 09 items are the right owner of what a person
  carries.
- A single world-wide ledger entity: rejected; the region is the coarse owner everywhere
  else (population, faith, strata) and a snapshot must carry stocks with their regions.
- Splitting all of the common stock at a promotion: rejected; the commons keep the
  region's reserve (the stores of 05.05 become real grain in 06.02) and the split is a
  rule, not a constant.
- Decided by robustness (one owner per grain, a conservation invariant checked yearly)
  and evolvability (kinds and endowment are tables; production, markets and trade add
  movements without changing the owners).

### Consequences

- The stocks digest hashes every common stock in region order, then every house stock in
  family order; any change in the split, the fold or the endowment changes it.
- The economy module depends on the society module (guilds and stores in 06.02, wealth in
  standing in 06.05); nothing below knows about goods.
- Saturation at the top is a rule of the counts, not a story: 06.03 prices and 06.04 trade
  will keep stocks far from it.

### Status

Accepted 2026-09-07. Files: `Source/VaelenEconomy/*`, `Tests/Economy/Test_Stocks.cpp`
(4 tests). Headless VALIDATED on the six Linux presets; engine side UNVERIFIED.

---

## ADR-0049: The harvest is made in both grains by the same rule, and hunger follows the grain through a ration the need system observes

### Context

06.01 gave every region a stock; nothing moved it. Production must feed the needs of
04.04 (which so far rationed a region by its capacity over its living, cut by droughts
and softened by a council's stores), work in coarse and detailed regions alike so that
a promotion does not change what a region eats, and leave the Phase 04 and 05 frozen
runs untouched.

### Decision

1. One rule for the harvest at both grains: workers on the land within the capacity, a
   fixed harvest per worker at ordinary farming. A coarse region counts seven tenths of
   its people on the land as workers; a detailed region counts its persons of age, each
   yielding 850 to 1150 per mille by farming skill, the land taking no more of them than
   the capacity's share - so the two grains agree within a few tenths, and the tests hold
   them within three.
2. Where the grain goes follows the owner of 06.01: a coarse region's harvest to the
   common stock, a detailed region's to the workers' houses, a share to the common stock
   where a council keeps grain (05.05). The meals come the same way back: a house eats
   from its own stock, then from the common one; the unhoused from the common one.
3. Hunger follows the grain through one number: the ration - what the region could feed
   over what it needed - written on the region as `RegionRation`, a Population type the
   need system observes (`ObserveRation`) and that replaces the land's ration, droughts
   included, since the harvest already took the drought. Without an observer the need
   system keeps its Phase 04 rule, so nothing frozen moves.
4. Droughts cut the harvest, not the meals, and are the harvest's cause; the shortfall is
   an event with the same cause, so a famine's why reaches the drought through the grain.
5. Grain in store spoils a tenth a year: the store of a region at peace converges to a
   dozen harvests rather than growing without bound. The other goods follow the people
   (timber burnt, salt used, cloth and tools made and worn, tools from ore) in the
   common stock; luxuries wait for trade.

### Alternatives and decision rule

- Persons farming individually in a detailed region (a harvest per person entity):
  rejected; the house is the owner of 06.01 and the unit that eats; a per-person split
  would fold at every death.
- The need system reading the stocks itself: rejected; Population must not know about
  goods (layering); a ration is the smallest truthful interface, as the stores were.
- Decided by robustness (one rule, one number, the Phase 04 behaviour kept where nothing
  observes), then simplicity.

### Consequences

- A region past its capacity starves at both grains unless its store carries it; the
  need system, as validated in 04.04, empties a hungry region within a few years - the
  tests read the deaths after three.
- The rations digest hashes every ration in region order; it and the stocks digest
  freeze 06.02.
- 06.03 markets read stock over need, the same numbers this task computes.

### Status

Accepted 2026-09-07. Files: `Source/VaelenEconomy/Public/Vaelen/Economy/Production.h`,
`Private/Production.cpp`, `Source/VaelenPopulation/.../Needs.h/.cpp`,
`Tests/Economy/Test_Production.cpp` (4 tests). Headless VALIDATED on the six Linux
presets; engine side UNVERIFIED.

---

## ADR-0050: A price is an integer on the region, from what it wants over what it holds, within a floor and a ceiling

### Context

Trade (06.04) needs a reason for goods to move, wealth (06.05) needs a value for a stock,
and the chronicle needs prices that mean something to a reader. The kernel is integer
and deterministic; a market must be a few numbers on the region that both grains
compute alike.

### Decision

1. A market is a component on the region entity, one integer price per good, written
   yearly after production for every region holding a stock. No market entity, no order
   book, no money supply: a price is a ratio.
2. The ratio is what the region wants over what it holds. What it wants comes from the
   same consumption rules the production uses (two years of its people's grain, three of
   every used good, a little luxury), so the market and the harvest never disagree on
   need; what it holds is the common stock and the houses' together, so a promotion does
   not move a price by moving grain between owners.
3. The price is the base price of the good times that ratio, clamped between a quarter
   and eight times the base: a floor because a good never becomes free, a ceiling because
   an empty market is not infinitely dear, and bounds because every later rule (trade
   thresholds, wealth) can count on a finite range.
4. A price that moved by a quarter or more is an event about the region, and for grain
   its cause is the year's harvest, whose cause is the drought that cut it: the why of a
   dear loaf is three events deep and follows the existing chain.
5. `ValueOf` prices a stock at a market; it is the one function wealth will use.

### Alternatives and decision rule

- Prices from transactions (a clearing between buyers and sellers): rejected for Phase
  06; there are no buyers yet, and the ratio gives the same direction with one number.
- A world price per good: rejected; the region is the owner everywhere else, and trade
  needs the differences between regions.
- Decided by simplicity within robustness (bounded integers, one rule, one owner) and
  evolvability (06.04 opens routes on price gaps; 06.05 values houses; a clearing can
  replace the ratio later without changing the component).

### Consequences

- Prices in a coarse and a detailed region differ only as their stocks and people do;
  the tests hold them within a factor of two after five years.
- Price events are sparse (a quarter's move), so a century of a world adds about a
  thousand of them; the markets digest hashes every market in region order.
- Base prices are a table in the rules; balancing them is data, not code.

### Status

Accepted 2026-09-07. Files: `Source/VaelenEconomy/Public/Vaelen/Economy/Markets.h`,
`Private/Markets.cpp`, `Tests/Economy/Test_Markets.cpp` (4 tests). Headless VALIDATED
on the six Linux presets; engine side UNVERIFIED.

---

## ADR-0051: A route is an entity between two neighbouring markets, opened by a price gap, a want and a surplus, carrying goods one way, and closed when idle

### Context

06.03 gave every region prices; goods still never left their region. Trade must move
goods where they are wanted, along the region graph of Phase 02, leave traces the
chronicle and the later phases (infrastructure, war) can use - roads, settlements - and
stay bounded in entities and events over centuries.

### Decision

1. A route is an entity of kind Route between two adjacent regions (in index order), a
   settlement an entity of kind Settlement on a region. Both are records with a life:
   opened and closed, founded and abandoned, so that a road or a town of the past is
   history, not a deleted row.
2. A route opens when, for some good, the price on one side is at least twice the
   other's, the dear side holds less than it wants and the cheap side more: a gap alone
   opened roads that carried nothing (the first draft churned five thousand of them in
   three centuries); with the want and the surplus a road opens only where goods will
   move. A region takes four routes at most; a road once built is reopened rather than
   built again, so the entities are bounded by the graph's edges.
3. Every year, after the markets have priced, every open route carries a quarter of the
   cheap side's surplus of every good, up to what the dear side wants and a yearly
   limit, from the one common stock to the other. Goods move one way and nothing is paid:
   money and payment belong to wealth (06.05); in Phase 06 a route is a flow, not a
   contract. A route that carried nothing for five years closes.
4. A settlement is founded where a region's routes carried fifty units in a year, and
   abandoned after ten years without traffic; it remembers its routes and traffic of the
   last year, and its identity comes from the world seed for the naming to come.
5. Every opening, closing, carry (one event a route a year, with the units), founding and
   abandoning is an event about the route or the settlement.

### Alternatives and decision rule

- Trade by houses or merchants (persons carrying goods): rejected for Phase 06; the
  common stocks are the owners at both grains and a flow between them is what the
  markets can price; merchants are Phase 09 people with items.
- Routes as graph edges kept in a table without entities: rejected; a road must have an
  id for events, names and the chronicle, and its life is state.
- Payment in goods of equal value: deferred to 06.05 with wealth; one-way flows keep this
  task's invariant simple (goods conserved, only moved).
- Decided by robustness (bounded entities, one open route per pair, one event a carry)
  and evolvability (the rules are numbers; a payment can be added to the carry).

### Consequences

- Trade lowers the ore ceiling where no deposit is: the tests count fewer markets at the
  ceiling with trade than without.
- A flooded world closes every road and empties every settlement within the rule's
  years; a drained one reopens them with new settlement indices and the same road
  indices.
- The trade digest hashes every route then every settlement in index order, closed and
  abandoned included; 500 years at 64 freeze it.

### Status

Accepted 2026-09-07. Files: `Source/VaelenEconomy/Public/Vaelen/Economy/Trade.h`,
`Private/Trade.cpp`, `Tests/Economy/Test_Trade.cpp` (4 tests). Headless VALIDATED on the
six Linux presets; engine side UNVERIFIED.

---

## ADR-0052: Wealth reaches standing as a rank, not an amount, and an extinct house's goods follow the culture's descent custom

### Context

Phase 06 must give money a social weight and let fortunes pass between generations,
without the society module (Phase 05) learning what a price is, and without changing the
frozen behaviour of any earlier task. Standing (05.02) already scores persons; stocks
(06.01) already return an extinct house's goods to the common stock.

### Decision

1. A house's wealth is its goods at its own region's prices (`ValueOf`, 06.03), computed
   yearly for every living house of a detailed region.
2. Wealth reaches standing as a **rank** among the region's houses (0 to 255), never as an
   amount. A rank is comparable across regions, bounded, and independent of the price
   scale, so a standing score cannot be inflated by a bout of scarcity, and the society
   module needs no notion of goods or money. `HouseWealth` lives in `Standing.h`, is
   declared by the economy and observed by the standing system, exactly as the stores of
   05.05 and the ration of 06.02 are observed by the systems below them. Without an
   observer the Phase 05 score is unchanged.
3. The heir of a house is the eldest living child of its head who carries the line by the
   culture's descent custom (05.03) - sons where patrilineal, daughters where matrilineal -
   and who has a house of their own. The name is written on the house as `HouseHeir` and
   honoured by the stock system when the house dies out (`ObserveHeirs`): the goods pass to
   the heir, and only a house whose line has failed loses them to the commons. Without the
   hook, 06.01's behaviour and digests hold.
4. Wealth and heirs are truths about the living: a house that dies out, or whose region
   goes coarse, keeps neither.

### Consequences, observed

Because a bride joins her husband's house while a groom who already has one keeps it
(04.03), a matrilineal world names many times more heirs than a patrilineal one: under
patrilineal descent the sons hold the house itself, so a house that dies out has usually
had no son to leave it to, and the commons take all. The tests measure that gap (two
houses with an heir against a hundred and twenty-four in the same world and year) rather
than assume symmetry; it is a legible consequence of the customs, not a defect.

### Alternatives and decision rule

- Wealth as points from the amount: rejected; prices move with scarcity and the score
  would swing with a bad harvest.
- Inheritance handled by the family system: rejected; Population must not know about
  goods. The name is written by the economy and read by the economy; only the custom
  comes from Society.
- Splitting the goods among all children: rejected for Phase 06; one heir keeps the
  invariant simple and matches how a house's stock is a single owner.
- Decided by robustness (one owner, one number, earlier behaviour preserved without an
  observer) and layering.

### Status

Accepted 2026-09-07. Files: `Source/VaelenEconomy/Public/Vaelen/Economy/Wealth.h`,
`Private/Wealth.cpp`, `Source/VaelenSociety/.../Standing.h/.cpp`,
`Source/VaelenEconomy/.../Stocks.h/.cpp`, `Tests/Economy/Test_Wealth.cpp` (5 tests).
Headless VALIDATED on the six Linux presets; engine side UNVERIFIED.

---

## ADR-0053: Nothing of the fine grain outlives it: a coarse region keeps counts and stocks, and nothing else

### Context

Five phases have added state that exists only while a region is detailed - persons,
families' members, memberships, standing, bonds, house stocks, wealth, heirs - and each
added its own rule for what a demotion keeps. By 06.05 the economy had three owners of
fine-grained state (house stock, house wealth, house heir) written by two different
systems, and only the first was cleared on demotion. A house of a coarse region kept a
rank among houses that no longer existed and an heir nobody could inherit from.

### Decision

1. One rule for the whole economy: what is read from a detailed region dies with it. The
   wealth system sweeps, every year and even when no region is detailed at all, every
   house that died out or whose region went coarse, and removes its wealth and its heir;
   the stock system had already folded its goods the same tick.
2. What a coarse region keeps is only what it can keep truthfully: counts (population,
   faith, strata, organisation seats) and stocks, which are the region's own, not a
   reading of its persons. Everything else is recomputed at the next promotion.
3. The invariant is proved, not asserted: a still world - the owners of goods and nothing
   else - keeps every unit of every good, to the unit, through five hundred years of
   promotions and demotions. A living world with every system running holds the weaker but
   complete set of invariants over the same span.

### Alternatives and decision rule

- Keeping wealth ranks through a demotion as a memory (as organisation seats are kept):
  rejected; a seat count is about the organisation, which survives, while a rank is about
  a comparison between houses whose members no longer exist.
- Clearing on the demotion event itself rather than sweeping: rejected; the sweep is one
  rule that also catches a house that dies out while its region stays detailed, and it
  needs no event ordering between two modules.
- Decided by robustness (one rule, checked every year) over performance (a sweep of the
  family pool a year is nothing beside the tick that precedes it).

### Consequences

- `MeasureWealth` counts a purse or an heir on a dead house as stale, and every long run
  reports zero.
- A region promoted after centuries coarse is valued from scratch: its houses' ranks and
  heirs are this year's, not the ones it had before.
- The two five-hundred-year digests freeze the grain behaviour of the whole economy; any
  future system that writes fine-grained state must clear it the same way or they move.

### Status

Accepted 2026-09-07. Files: `Source/VaelenEconomy/Private/Wealth.cpp`,
`Public/Vaelen/Economy/Wealth.h`, `Tests/Economy/Test_Grains.cpp` (2 tests). Headless
VALIDATED on the six Linux presets; engine side UNVERIFIED.

---

## ADR-0054: The chronicle records what lasts: a road built, a town risen, a price at its bound, a fortune moved, not the churn beneath them

### Context

The economy publishes far more events than the society did: a harvest and a price for every
region every year, a carry for every open road, a split at every promotion. A chronicle
that recorded them would bury the reader and the world's history state under thousands of
records a century, and the first draft did exactly that - 3794 records in ten years, of
which 2935 were roads opening and closing.

### Decision

1. A record is written only for what a reader would still care about a century later: a
   road built (its first opening), a road that carried something being abandoned, a town
   risen or abandoned, a price that reached its floor or its ceiling, a region gone short
   of grain past a threshold, a fortune that moved half the range of ranks, an inheritance.
   Harvests, carries, endowments, splits, folds and ordinary price moves have lines but no
   records: they are read from the log when wanted.
2. Every economic event still gets a sentence. `DescribeEconomyEvent` covers all seventeen
   of them, so the chronicle, the why and any later reader speak of the whole economy even
   though only a part of it is recorded.
3. Two things the drafting proved were bugs rather than noise, and were fixed at the
   source rather than filtered here: a market's first pricing published seven price-change
   events (a market appearing is not a price changing), and a road reopened on a passing
   price gap published the same event as a road built (`RouteInfo` now counts its
   openings). Filtering either in the listener would have left the log itself lying.

### Alternatives and decision rule

- Recording everything and paging the chronicle: rejected; the record count is the world's
  history state, which snapshots and replays, and it must stay proportionate to what
  happened rather than to how often systems ran.
- A relevance score per event: rejected as unpredictable; thresholds in a rules struct are
  readable, tunable and testable.
- Decided by robustness (the log stays the whole truth, the chronicle a bounded reading of
  it) and by fixing causes rather than symptoms.

### Consequences

- The frozen counts of two earlier tasks move deliberately: 06.03's price changes and
  06.04's trade digest, both recorded in the task's entry.
- A record's region is the region the event happened in, so the per-region-per-year cap
  applies where a reader would notice a flood.
- 06.08's gate counts records among its invariants.

### Status

Accepted 2026-09-07. Files: `Source/VaelenEconomy/Public/Vaelen/Economy/EconomyHistory.h`,
`Private/EconomyHistory.cpp`, `Private/Markets.cpp`, `Public/Vaelen/Economy/Trade.h`,
`Private/Trade.cpp`, `Tests/Economy/Test_EconomyHistory.cpp` (3 tests). Headless VALIDATED
on the six Linux presets; engine side UNVERIFIED.

---

## ADR-0055: When two validated orderings cannot both hold, the chain that carries the grain wins, and the describer of the topmost layer speaks for all of them

### Context

The Phase 06 gate is the first test to run every Phase 04, 05 and 06 system in one world.
It refused to start: `Scheduler::Build` found a cycle. Phase 05 had validated
`Houses->RunAfter("Needs")` - a family forms after this year's hunger - and Phase 06 had
validated `Body->RunAfter("Production")` - the needs read this year's ration - while the
stock system runs after Families. Together: Families -> Needs -> Production -> Stocks ->
Families. Both edges were reasonable when added; neither task could see the other.

### Decision

1. The chain that carries the grain wins. The economy's order (stocks, then the harvest,
   then the meals) is the one that must hold within a tick, because a ration computed
   from last year's harvest would make hunger lie. `Houses->RunAfter("Needs")` is dropped
   where the economy runs: the family system forms its marriages on the living of the tick
   before this year's hunger, a one-tick difference in who is available to marry, and no
   invariant depends on it.
2. The describer of the topmost layer speaks for every layer under it. The economy's
   chronicle was describing society events through the person text, so a founding or a
   raid lost its sentence in the world's own chronicle. `EconomyContext` now carries an
   optional `SocietyContext`, and with it `DescribeEconomyEvent` delegates to
   `DescribeSocietyEvent`, which itself delegates to the person text. Without it the
   behaviour is unchanged, so 06.07's own tests and digests hold.
3. Both were found only by running everything at once. A gate that assembles the whole
   world is worth more than the sum of the task tests, and it is the reason the roadmap
   ends every phase with one.

### Alternatives and decision rule

- Keeping both edges by making Stocks run before Families: rejected; the stock system
  splits a promoted region's commons among its houses and must see the houses that exist.
- Letting the economy chronicle omit society lines: rejected; a chronicle that speaks of
  a world must speak of all of it, and the omission was invisible until a gate exported
  the whole text.
- Decided by robustness (no lying ration, no mute layer) over keeping an earlier decision
  untouched.

### Consequences

- The Phase 05 gate keeps its own ordering and its own frozen digests: it runs no economy,
  so no cycle exists there. The two gates order differently on purpose, and each says so.
- Any later phase adding a yearly system must check its `RunAfter` edges against the whole
  set, not only against its own phase.
- The Phase 06 gate's four digests freeze the whole world - state, log and text - at 256
  over 500 years.

### Status

Accepted 2026-09-07. Files: `Tests/Economy/Test_EconomyGate.cpp`,
`Source/VaelenEconomy/Public/Vaelen/Economy/EconomyHistory.h`,
`Private/EconomyHistory.cpp`, `Tests/Economy/CMakeLists.txt` (1 test). Headless VALIDATED
on the six Linux presets; engine side UNVERIFIED.

---

## ADR-0056: A polity is an entity seated in a council, and a region remembers whose it is

### Context

Phase 07 opens with authority. The prompt wants polities, laws, succession and diplomacy;
Phase 05 already has councils with seats and heads, and Phase 04 has persons with standing
and offices. The question is whether power gets its own hierarchy or borrows the one that
exists.

### Decision

1. A polity is an entity of kind Polity with a seat region, a culture, a council and a
   ruler. It is founded where a detailed region holds a council of enough members and
   enough people and belongs to nobody yet.
2. Its ruler IS the council's head - the same person the organisation system already
   seats, alive and of age. No new kind of person, no parallel election, no office that
   exists only in politics: authority is exercised through the organisations of Phase 05,
   and when the council seats a new head the polity has a new ruler the same year.
3. Belonging is a component on the region (`RegionRule`), not a list on the polity. A
   region knows whose it is whether it is simulated person by person or kept as counts,
   a demotion never loses it, and the polity's count of regions is recomputed from the
   regions themselves rather than trusted.
4. A polity that has no council of its own, or nothing left to rule, is dissolved after
   its first years and stays in the world as history with its founding intact. Its
   regions are freed the same tick - and a seat whose council still sits is founded anew
   immediately, because a council governing a peopled region is exactly the condition for
   a polity to exist.

### Alternatives and decision rule

- A polity holding a list of its regions: rejected; the list would need rewriting at
  every demotion and could disagree with the regions, and 07.03 will move regions between
  polities where one owner of the truth matters most.
- A ruler chosen by the polity itself (an election, a strongest claimant): rejected for
  07.01; that is 07.04 succession and 07.05 factions, and both will move the council's
  head rather than bypass it.
- Founding on a region rather than a council: rejected; a region with no organisation has
  nobody to rule through, and the council threshold is what makes a polity a thing persons
  belong to.

### Consequences

- A coarse region keeps its rule and its polity keeps standing, but its ruler falls to
  none: the council's head is a memory of counts while nobody is simulated, and the seat
  fills again at the next promotion.
- The polities digest hashes every polity in index order then every rule in region order,
  so a change in founding, dissolution or belonging moves it.
- 07.02 hangs law on the polity as components the lower systems observe, the way the need
  system observes a ration.

### Status

Accepted 2026-09-07. Files: `Source/VaelenPolitics/*`, `Tests/Politics/Test_Polities.cpp`
(4 tests). Headless VALIDATED on the six Linux presets; engine side UNVERIFIED.

---

## ADR-0057: A law is one number on the polity and a plain struct on the region

### Context

07.02 wants a polity's rules to be felt by the layers under it, and taxes taken in
goods. The economy is a lower module: it must not learn what a polity is. But the
harvest is where a share of the grain has to be assessed, and the harvest belongs
to the economy.

### Decision

1. A law lives on the polity (`PolityLaw`), and is *proclaimed* onto every region
   the polity rules as `RegionDues` - a struct **the economy declares**, in
   `Stocks.h`, next to the stocks it belongs with. The politics module writes it;
   the economy's production system observes it, exactly as the population's
   `RegionRation` is declared by the population and written by the economy (06.02).
   The economy never includes a politics header and never learns why anything is
   owed.
2. The share is assessed on the harvest of the year, in the year it is reaped, and
   the grain does not move: it stays in the region's common stock until a collector
   comes. A bad year owes less, and a region that is stripped simply cannot pay.
3. The collector is the politics module's own system, running after the harvest. It
   takes what the region can pay into the polity's treasury and leaves the rest as
   arrears on the region. Nothing is spent yet, so what the log says was paid, what
   the law says was taken, and what the treasury holds are the same number - an
   invariant the tests hold to.
4. A law moves by itself, between a floor and a ceiling, by what the treasury holds
   against what the polity wants for the people it rules, and relents after years of
   short collection. **It does not move in the year it is written**: the founding
   year is the law's own, so what the log first records a polity demanding is exactly
   what the rules say a new polity demands.

### Alternatives and decision rule

- The politics module reaching into `RegionStock` at the harvest: rejected; it would
  have to run inside the economy's tick to catch the harvest, and the economy would
  own an ordering it does not know about.
- The economy moving the grain into a "tax" pot of its own: rejected; a pot with no
  owner is state nobody is responsible for, and 07.03 will spend the treasury.
- A law read by the lower systems through a query into the politics module:
  rejected outright - it inverts the layering.
- The law moving in its founding year: rejected; the first year would then report a
  share no rule ever asked for, and a test of the rules could not name a number.

### Consequences

- The dues of a demoted region survive the demotion, since they sit on the region.
- A seat whose council still sits is refounded the same tick it is freed (07.01), so
  a region rarely stays unruled for long; its arrears wait for whoever comes.
- 07.03 spends the treasury: reach, garrisons and what a polity's word costs to carry.

### Status

Accepted 2026-09-07. Files: `Source/VaelenPolitics/Public/Vaelen/Politics/Law.h`,
`Private/Law.cpp`, `Source/VaelenEconomy/.../Stocks.h` (`RegionDues`), `Production.h`
and `Private/Production.cpp` (`ObserveDues`), `Tests/Politics/Test_Law.cpp` (4 tests).
Headless VALIDATED on the six Linux presets.

---

## ADR-0058: Authority is written on the region and falls with the walk from the seat

### Context

07.03 asks how far a polity's word carries. The easy answer is a radius in tiles
around the seat. The world already has a better graph than distance in tiles: the
region adjacency of 02.06, which knows that two regions across a mountain are far
apart even when their centroids are near.

### Decision

1. Authority is a component on the region (`RegionAuthority`), not a number the
   polity owns: the region knows whose word runs in it, from how far, and how
   firmly. The same reasoning as ADR-0056 - the ground remembers, not the ruler.
2. The distance is hops on the region graph **walked only through the polity's own
   ground**. A region it cannot walk to from its own seat is held not at all, so an
   enclave that loses its corridor loses its master; nothing had to be written to
   make that happen.
3. Hold falls by a fixed step per hop. A region under the floor **slips free** - the
   polity does not decide to release it, and no event asks it to. Losing ground is
   what happens when a polity overreaches, not a choice it makes.
4. Carrying a word costs grain a year, per region, growing with distance, paid out
   of the treasury 07.02 fills. What cannot be paid costs hold everywhere, and the
   far edge crosses the floor first: a polity in trouble contracts from its border.
5. Reach - how far a *new* region may be taken - is bought by the treasury and
   capped. A polity takes the unruled edge in region order, and **writes the taken
   region's authority in the same tick it takes it**.

### Alternatives and decision rule

- A radius in tiles from the seat: rejected; it ignores the terrain the region
  graph already encodes, and it would let a polity hold across a sea.
- Authority as one number on the polity, with distance applied at the point of use:
  rejected; every consumer would have to recompute the walk, and 07.05's factions
  need to ask a region how firmly it is held.
- The polity choosing to release a region it cannot afford: rejected; it makes
  losing ground a decision, and a decision needs a decider - which is 07.05, not
  this task.
- Writing the taken region's authority on the next tick: rejected, and this was a
  real defect found by the tests. A region ruled by a polity but carrying nobody's
  authority is a lie for a whole year, and the invariant "the regions held equal
  the regions ruled" could not hold.

### Consequences

- A polity's shape is emergent: it grows along the graph while grain lasts and
  contracts from the edge when it does not. Nothing draws a border.
- The seat never slips. A polity with no seat is 07.01's business (it dissolves),
  not this system's.
- 07.04 seats a new ruler on the same council; the hold is unaffected by who rules,
  which is deliberate - 07.05 is where a weak ruler will cost hold.

### Status

Accepted 2026-09-07. Files: `Source/VaelenPolitics/Public/Vaelen/Politics/Reach.h`,
`Private/Reach.cpp`, `Tests/Politics/Test_Reach.cpp` (4 tests). Headless VALIDATED on
the six Linux presets.

---

## ADR-0059: Succession is judged, not decided

### Context

07.01 made a polity's ruler the head of its council: authority runs through the
organisations of Phase 05 rather than beside them. 07.04 has to add succession
without taking that back. The obvious design - a succession system that picks the
next ruler - would give the polity a second way to seat someone, and two systems
seating rulers is two truths.

### Decision

1. This system never seats anyone. The council seats its head, 07.01 makes that
   head the ruler, and succession only **watches the seat and compares**.
2. Every year it names the claimant the culture's `Descent` custom points at: the
   eldest living child of age of whoever sits, on the patrilineal or matrilineal
   line the culture keeps. It is a prediction, not an instruction.
3. When the seat changes hands, the passing is settled if the person who took it
   is the one that was named (or nobody was named), and disputed otherwise.
4. A vacancy and a disputed succession both add **unrest** to the polity: a number
   that fades year by year, which the reach system takes off the hold of every
   region. That is the whole consequence - no rebellion, no civil war, no faction.
   Those are 07.05.
5. The claimant is found in **one walk of the persons per year** for all polities
   at once, not one walk per polity, and ties between children born the same tick
   go to the lower person index so pool order can never decide a succession.

### Alternatives and decision rule

- A succession system that seats the heir: rejected; it would fight the
  organisation system for the council's head, and the loser would be whichever ran
  second in the schedule.
- Unrest as a component on each region: rejected; the cause is one event at the
  centre, and 07.03 already owns the per-region number. A polity-level number that
  reach subtracts keeps one writer per field.
- Succession failing silently when the custom names nobody: kept, deliberately. A
  ruler with no living child of age has no claimant, so no passing can be disputed
  - a young dynasty is fragile in reach but not in legitimacy.

### Consequences

- A polity's stability is now legible in one number, and it moves for reasons the
  chronicle of 07.07 can state: "the seat fell empty", "the custom named another".
- Because unrest costs hold everywhere at once, a bad succession contracts a large
  polity more than a small one - overreach is punished by its own geometry.
- 07.05 will give the passed-over claimant somewhere to put their claim.

### Status

Accepted 2026-09-07. Files: `Source/VaelenPolitics/Public/Vaelen/Politics/Succession.h`,
`Private/Succession.cpp`, `Reach.h`/`Private/Reach.cpp` (`ObserveLine`),
`Tests/Politics/Test_Succession.cpp` (4 tests). Headless VALIDATED on the six Linux presets.

---

## ADR-0060: A faction takes ground, never the throne

### Context

07.01 made the ruler the council's head and 07.04 refused to seat anyone. 07.05
has to give the passed-over claimant and the neglected province somewhere to put
their grievance, without giving the world a third way to fill a seat.

### Decision

1. A faction is an entity of its own kind (`IdKind::Faction`, new in this task)
   with a polity, a **region**, a strength, and sometimes a person. It never
   changes who sits. What it does is take its region out of the polity.
2. Two grievances form one: a claimant the custom named and the council passed
   over, in the region they live in; and a region held under a threshold for
   several years running, which wants nobody in particular.
3. A seat is not a province. No faction rises in the region a polity rules from -
   neither by neglect (it cannot be neglected by itself) nor by a claimant who
   lives there (they have nothing to take; their grievance is already in the
   polity's unrest from 07.04). This keeps the seat inviolable in all three
   systems that could otherwise take it.
4. A faction gathers strength while its grievance stands and loses it when the
   grievance is answered. At its threshold the region leaves and the faction
   **ends**: it wanted that, and it has it. There is no permanent rebel state.
5. While it stands it adds to the polity's unrest, which 07.03 takes off every
   hold - so a faction in one province weakens the whole. It runs last of the
   politics systems, so that unrest is felt the year after: a faction is not news
   the day it forms.
6. A polity bears one faction at a time. A second grievance waits rather than
   piling on, so the strength of a rebellion is never the sum of unrelated
   complaints.

### Alternatives and decision rule

- A faction that seats its claimant: rejected; it is a third writer of the seat
  after the organisation system and 07.01, and the schedule would decide who wins.
- A faction as a component on the polity: rejected; a grievance has a place, and
  07.07 must be able to name the region a revolt happened in.
- A faction that persists after taking its region: rejected for 07.05; a standing
  rebel polity is what 07.01 already makes when a council governs free ground, so
  the region simply becomes ungoverned and may be founded upon again.
- Strength as a count of people rather than a per mille: deferred; the population
  of a coarse region is a count, and a per mille keeps the rule readable at both
  grains.

### Consequences

- Overreach now has a second cost. 07.03 makes a far province cheap to lose; 07.05
  makes it *want* to be lost, and the wanting spreads unrest to the whole.
- A polity that pays its way and settles its successions is never troubled: every
  grievance in the model has a cause the polity could have removed.
- 07.06 gives two polities something to be to each other; a region freed by a
  revolt is exactly the kind of ground a neighbour will want.

### Status

Accepted 2026-09-07. Files: `Source/VaelenPolitics/Public/Vaelen/Politics/Factions.h`,
`Private/Factions.cpp`, `Source/VaelenCore/.../Ids.h` and `Private/Ids.cpp`
(`IdKind::Faction`), `Tests/Politics/Test_Factions.cpp` (4 tests). Headless VALIDATED
on the six Linux presets.

---

## ADR-0061: A relation is a fact of the ground, and a war is a permission, not an order

### Context

07.06 has to make two polities mean something to each other without inventing a
diplomatic apparatus - envoys, offers, treaties negotiated turn by turn - that
nothing else in the model would support, and without a war system, which is
Phase 08.

### Decision

1. **Contact is a fact of the region graph.** Two polities know each other when
   their ground touches; nobody decides to meet. A relation is created on the
   first touch and lives as an entity of its own kind (`IdKind::Treaty`), with
   A always the lower index so a pair has exactly one relation whichever side
   is asked.
2. **The warmth drifts from what the world already has**: a shared culture, the
   roads of 06.04 crossing the border, the length of that border, and the gap
   in size between the two. No opinion is invented; every term is a number some
   other phase already maintains.
3. **The stance follows the warmth with hysteresis.** A stance holds until the
   warmth has passed the edge of its band by a margin, so a pact is not lost to
   one bad year and a war is not declared by a rounding.
4. **A war is a permission, not an order.** Diplomacy marks the weaker side's
   border regions as `RegionInPlay` - a struct **`Reach.h` declares** and
   diplomacy writes - and stops there. The reach system, which could only ever
   take unruled ground, may now take a contested region at a war price. It
   never learns what a war is; it learns that this ground is takeable and
   dearer. Whether anything is taken depends on the treasury, exactly as
   claiming empty ground does.
5. **A war is fought at the border, not from the capital.** Empty ground is
   claimed within the seat's reach; ground in play is taken from wherever the
   polity already stands. Without this a large polity could never annex
   anything, since its own border is past its reach - which the first run
   showed plainly.

### Alternatives and decision rule

- A war system with armies and battles: that is Phase 08. Putting it here would
  duplicate what 08 must own.
- Diplomacy moving the regions itself: rejected; two systems writing `RegionRule`
  is two truths, and the reach system already owns the cost, the ordering and the
  bookkeeping of taking ground.
- Relations as a matrix on the polity: rejected; a relation has a history the
  chronicle of 07.07 must be able to name, and an entity is what carries one.
- A stance chosen by a ruler's traits: deferred; the traits of 04.05 belong to a
  person, and a polity outlives its rulers. 07.04's unrest is where a ruler's
  fortune already reaches the map.

### Consequences

- Conquest is emergent: two powers grow into each other, the border lengthens,
  the warmth falls, the stance turns, ground goes in play, and the richer side
  takes it - and a polity reduced to nothing dissolves by 07.01's own rule.
- Everything taken in war is held like anything else: it costs upkeep, it is held
  more loosely the further it is, and it can be neglected into a faction. There is
  no special status for conquered ground.
- Phase 08 will replace the permission with a contest: armies, sieges and losses
  between the mark and the taking.

### Status

Accepted 2026-09-07. Files: `Source/VaelenPolitics/Public/Vaelen/Politics/Diplomacy.h`,
`Private/Diplomacy.cpp`, `Reach.h` and `Private/Reach.cpp` (`RegionInPlay`,
`ObserveContest`, `RegionAnnexed`), `Source/VaelenCore/.../Ids.h` (`IdKind::Treaty`),
`Tests/Politics/Test_Diplomacy.cpp` (4 tests). Headless VALIDATED on the six Linux presets.

---

## ADR-0062: History is narrower than the log, and the topmost describer speaks for every layer

### Context

07.07 is the fourth chronicle layer (03.06 the world's, 04.07 the person's, 05.07
the society's, 06.07 the economy's). The pattern was settled by then; what this
task had to settle was *what a political event has to be to be history*, and how
a world with four layers of describers tells one story rather than four.

### Decision

1. **The narrowness is the design.** A politics log of forty years holds 1838
   events; the chronicle keeps 30. The rule for each kind is the same one 06.07
   used for prices - record the ends, not the motion:
   - a settled succession is not history (the council seated its head, as it
     does); an empty seat and a succession the custom did not name are;
   - a tax moving a notch is not history; a tax at its floor or its ceiling is,
     because the polity has run out of room in one direction;
   - a faction forming or fading is not history; a province throwing off its
     master is;
   - a stance drifting through rivalry is not history; a pact sworn and a war
     begun are.
2. **Every event still has a sentence**, recorded or not. `DescribePoliticsEvent`
   covers all twenty-one, so anything read out of the log - by a test, by the why
   chain, by a later tool - reads as the world's own words, and nothing ever
   falls back to "event 12 of type 0x...".
3. **The topmost describer speaks for every layer under it.** `PoliticsContext`
   carries an optional `EconomyContext`, which carries a `SocietyContext`, which
   knows the person layer. One call describes any event in the world at the
   fullest words available. Without this, a chronicle of a whole world would lose
   the words of its middle - the mistake 06.08 found and fixed for the economy.

### Alternatives and decision rule

- Recording every political event: rejected; a century would leave tens of
  thousands of records and the chronicle would be the log with extra steps.
- A separate describer per layer, chosen by the caller: rejected; the caller
  would have to know which layer an event came from, which is exactly what the
  layering is meant to hide.
- Recording law changes at every step: rejected; the bound is the moment the rule
  itself becomes visible, and a polity pinned at its ceiling is a fact about the
  polity, not about the year.

### Consequences

- The chronicle of a world with two powers reads as a history: foundings, seats
  taken, taxes demanded, dues paid, provinces thrown off, wars, annexations.
- 07.08's gate can freeze the chronicle text as a digest, which catches any
  change in wording, ordering or narrowness at once.
- A fifth layer (Phase 08's wars) will carry a `PoliticsContext` the same way.

### Status

Accepted 2026-09-07. Files: `Source/VaelenPolitics/Public/Vaelen/Politics/PoliticsHistory.h`,
`Private/PoliticsHistory.cpp`, `Tests/Politics/Test_PoliticsHistory.cpp` (3 tests).
Headless VALIDATED on the six Linux presets.

---

## ADR-0063: A seat cannot be taken

### Context

The Phase 07 gate ran five centuries of two powers at 256 with every system of
Phases 04 to 07 and found what no task test could: a polity whose capital had
been annexed went on standing, ruling through a council that now sat inside
another realm. Three systems can take ground - the reach system lets a region
slip (07.03), a faction takes its own region (07.05), and a war puts ground in
play (07.06) - and each had its own answer for capitals. Two said no; one said
nothing.

### Decision

1. **A seat cannot be taken.** The rule is now the same in all three places: a
   seat does not slip for want of hold, no faction rises in it, and no war puts
   it in play. A polity ends when its council ends or when it has nothing left,
   never by having its capital seized.
2. Taking a capital is a siege, and a siege is Phase 08. When 08.04 adds it, it
   will be the only way, and it will go through the polity system rather than
   around it.
3. Belt and braces: a polity that somehow loses its seat is dissolved **at
   once**, without the founding grace. Losing a capital is not a failure to
   grow, and a young polity ruling from another realm's ground is a
   contradiction rather than a state that needs time.
4. Whoever takes ground away cancels the demand on it. The law system (07.02)
   runs before the systems that free ground, so without this a freed region was
   assessed for a whole year on behalf of a master it no longer had. What it
   already owes it still owes: the arrears wait for whoever comes next.

### Alternatives and decision rule

- A polity that moves its seat when it loses one: rejected for Phase 07; the
  seat is where the council sits, and a council does not move. It may return in
  Phase 08 as a term of surrender.
- Dissolving a polity the moment its capital falls, taken by war: that is what
  the first fix did, and it worked - but it left the three systems disagreeing
  about whether a seat is takeable at all. One rule stated in one place beats
  three rules that happen to agree.
- Tolerating the inconsistency in the invariant: rejected. An invariant that
  tolerates a lie for a year is not an invariant; a polity ruling from enemy
  ground is exactly the kind of thing the gate exists to catch.

### Consequences

- Conquest can take every province and stops at the wall. A polity reduced to
  its capital is a rump that may grow again - which is a better world than one
  where a lucky annexation ends a state.
- The three "the seat is inviolable" rules can now be stated as one line in the
  documentation and tested as one property.
- 08.04 has a clear brief: the siege is the only door, and it opens through
  07.01.

### Status

Accepted 2026-09-07. Files: `Source/VaelenPolitics/Private/Polities.cpp`,
`Private/Reach.cpp`, `Private/Factions.cpp`, `Private/Diplomacy.cpp`,
`Public/Vaelen/Politics/Factions.h`, `Tests/Politics/Test_PoliticsGate.cpp` (1 test).
Headless VALIDATED on the six Linux presets with every gate run.

---

## ADR-0064: An army is people taken out of regions

### Context

Phase 08 opens with armies. The cheap version is a number on the polity: a
strength that grows when it pays and shrinks when it fights. The world already
has people, regions, hold and a treasury, and a number would be answerable to
none of them.

### Decision

1. **An army is people taken out of regions.** Every man under arms is recorded
   on the region he came from (`RegionLevy`), and the invariant the tests hold
   to every year is that the men the regions say are away are **exactly** the
   men under arms. Nothing is invented and nothing is lost - the same
   conservation rule the economy holds for grain (06.06).
2. **A levy is obedience.** A region gives men in proportion to its people and
   to how firmly it is held, and one held under a floor gives none. 07.03
   already measures obedience as a number on the region; a levy simply reads
   it. A polity that overreached cannot call up the ground it barely holds.
3. **A host is raised only where there is a reason** - a war of 07.06 - and one
   at a time. When the war ends or the polity ends, the host goes home and the
   men return to the regions that gave them.
4. **What cannot be fed melts.** An army eats grain out of the treasury 07.02
   fills. A polity that cannot pay does not receive an order to disband: the
   host loses men year by year, and what it loses walks home. Three hungry
   years, or a strength under the raising floor, and the rest go too.

### Alternatives and decision rule

- Strength as a number on the polity: rejected; nothing would connect it to the
  people, and 08.06 has to be able to say which regions lost their men.
- Men drawn from a polity's total population rather than region by region:
  rejected; it would ignore hold, which is the whole point - and a polity's
  population is not a number it owns either.
- An army disbanded by decision when the treasury runs dry: rejected, for the
  same reason a region slips rather than being released (ADR-0058). Losing an
  army is what happens to a polity that overreaches, not a choice it makes.

### Consequences

- War is now paid for twice: in grain, out of the same treasury that carries
  the polity's word (07.03), and in people, out of the regions that must still
  bring in a harvest. 08.06 will make the second cost visible.
- A polity at peace has no army at all. There is no standing force to maintain,
  which is right for the scale the world is modelled at.
- 08.02 marches the host: the region graph is already there, and the levy is
  already accounted region by region.

### Status

Accepted 2026-09-08. Files: `Source/VaelenMilitary/*`,
`Tests/Military/Test_Armies.cpp` (4 tests). Headless VALIDATED on the six Linux
presets.

---

## ADR-0065: A host walks the region graph, and eats what it stands on

### Context

08.01 raises hosts and leaves them at the seat. Getting them anywhere needs a
notion of distance and a notion of speed, and the world offers two: the tile
grid, which is where things actually are, and the region graph of 02.05, which
is how the world is partitioned into places with names, rulers and people.

### Decision

1. **A host moves on the region graph, not on the tile grid.** A hop is a move
   to a neighbouring region. Distance is counted in hops, so the nearest enemy
   ground is the one fewest regions away rather than the one fewest miles away.
   Everything a war touches - rule, hold, dues, people - is written per region;
   moving per tile would give a precision nothing else in the simulation has.
2. **One hop a season, no faster, whatever the distance.** Speed is a rule, not
   a function of terrain or supply. A host four hops from its aim takes a year.
3. **The aim is chosen fresh every year**, by a breadth-first walk from where
   the host stands: the nearest region ruled by somebody its polity is at war
   with, ties going to the lower region index so that the road a host takes is
   the same on every run of the same seed. A front that moves changes the aim,
   which is right: a host does not march on a city that has already fallen.
4. **A host eats off the ground it ends the year on**, out of the region's
   common stock (06.01), and this is separate from the grain the treasury pays
   it (08.01). Marching is a cost the land bears, not the crown.
5. **A host standing on somebody else's ground loosens their grip on it**, and
   nothing more. It does not take the region: that is 08.04's business. But
   07.03 already turns a grip that keeps loosening into ground that slips, so
   an army parked on a province is a slow political fact before it is a
   military one.

### Alternatives and decision rule

- Movement on the tile grid with terrain costs: rejected as precision the rest
  of the simulation cannot answer. Robust over performant: a region graph walk
  is a handful of hops on a graph of a hundred nodes, and every neighbour of it
  - rule, hold, stock - is already indexed by region.
- A path computed once and followed to the end: rejected; the world changes
  under a marching army, and a host that keeps walking towards a region its
  own side has since taken is a bug the tests would have to tolerate.
- Taking the region on arrival: rejected. Standing somewhere and holding it are
  different things, and collapsing them would leave 08.03 and 08.04 nothing to
  do.
- Foraging out of the treasury instead of the land: rejected; it would make war
  a purely fiscal event and leave no mark on the people 08.06 has to account
  for.

### Consequences

- An enemy host is now a reason a province slips, through the hold of 07.03,
  with no new machinery.
- Two hosts can stand in the same region. Nothing resolves that yet; 08.03
  does.
- The frozen march digest is the first that depends on the region graph's
  adjacency, so a change to region partitioning will show up here first.

### Status

Accepted 2026-09-08, amended the same day by ADR-0066: point 3 as first written
sent every host to the nearest ground an enemy ruled, which is the province on
its own side of the border, and two powers were at war for two hundred and forty
years without their hosts meeting once. A host now marches on the nearest ground
an enemy host is standing on and falls back on nearest enemy ground only when no
host is in reach. Files: `Source/VaelenMilitary/Public/Vaelen/Military/March.h`,
`Source/VaelenMilitary/Private/March.cpp`, `Tests/Military/Test_March.cpp`
(4 tests). Headless VALIDATED on the six Linux presets.

---

## ADR-0066: A battle is settled in one year by numbers, ground and a stream

### Context

08.02 puts hosts in motion, and sooner or later two of them, of polities at
war, end a year on the same region. Something has to settle that, and the range
of possible answers is wide: a tactical model with terrain, formations and
rounds at one end, a coin flip at the other.

### Decision

1. **A battle is settled in one year, once.** No manoeuvring, no second round,
   no reinforcement. The simulation's finest step at this LOD is the year, and
   a model with rounds inside a year would be inventing detail the rest of the
   world cannot answer.
2. **Three things decide it and nothing else**: how many men each side has,
   whose ground it is, and a draw from a stream fixed by the world seed and the
   tick. The ground counts because 07.03 already measures how firmly a polity
   holds a region: a host fighting where the people obey its own ruler is not
   fighting the same battle as one deep in a stranger's province. The stream is
   what keeps the bigger host from always winning without making the outcome
   arbitrary - the swing is bounded, and the same seed always fights the same
   battle.
3. **The defender is the side whose ground it is**, and on nobody's ground the
   host raised first. A tie goes to the defender, because the side that did not
   have to come is the side that keeps the field.
4. **The fallen leave the levies of the regions that gave them.** Whether they
   died or scattered is 08.06's to say; the levy only knows they are no longer
   under arms, which is what keeps the men the regions say are away exactly
   equal to the men under arms (ADR-0064).
5. **A host left far weaker than the one that beat it is gone**; one that merely
   lost falls back on the nearest neighbouring region its own polity rules, and
   loses its marching orders, because a beaten army is under no orders until it
   is given new ones.
6. **A battle does not take ground.** Standing somewhere and holding it are
   different things; ground changes hands at a seat, before walls, in 08.04.

### Alternatives and decision rule

- Rounds within a year, with morale and formations: rejected. Robust over
  performant and evolvable over detailed - none of the inputs such a model
  wants (supply lines, doctrine, terrain within a region) exist, and inventing
  them would put weight on numbers nothing else in the world produces.
- Losses as a fraction of the difference in strength: rejected as a second
  free parameter doing the work of the first; a flat share of each side, with
  the loser's share the greater, is legible and gives 08.06 a clean number.
- A battle taking the region: rejected. It would collapse 08.03 and 08.04 into
  one and leave a capital falling to a single field engagement.

### Consequences

- Two defects surfaced the moment battles could happen, and both are closed
  here. 08.02's aim rule sent every host to the nearest ground an enemy ruled,
  which is the province on its own side of the border: two powers were at war
  for two hundred and forty years and never met. A host now marches on an enemy
  host before it marches on enemy ground. And 08.01 let a region whose men were
  already away be levied again by whoever next took the ground, which wrote over
  the first claim and left men no army accounted for once that first army was
  destroyed; such a region now gives nobody, and every release of a levy is
  checked to the man.
- War now costs men where it is fought as well as grain where it stands. AELVOR
  128 over a hundred years fights seventy-five battles and loses two thousand
  men to them, out of hosts of a few hundred: hosts are destroyed and raised
  again rather than grinding on.
- 08.04 has what it needs: a host can now stand before a seat having beaten
  what was defending it.

### Status

Accepted 2026-09-08. Files: `Source/VaelenMilitary/Public/Vaelen/Military/Battle.h`,
`Source/VaelenMilitary/Private/Battle.cpp`, `Tests/Military/Test_Battle.cpp`
(4 tests), `Source/VaelenCore/.../Ids.h` (`IdKind::Battle`). Headless VALIDATED
on the six Linux presets.

---

## ADR-0067: A seat is taken only by sitting in front of it

### Context

By 08.03 the phase can raise armies, march them, and destroy them, and it has
taken nothing. Ground does change hands already - a province slips to a
neighbour when its ruler's grip fails (07.03), and a faction can carry one off
(07.05) - but neither of those can end a polity, because 07.01 ends a polity
only when it loses its seat. Conquest needs a way to take a seat.

### Decision

1. **A seat changes hands only under siege.** Not by battle, not by a grip that
   failed, not by a claim. A host must stand on it, alone, for years.
2. **A siege is a wall coming down at the rate of the men before it.** A whole
   wall is a thousand per mille; a hundred men bring down two hundred and fifty
   of them in a year; nothing at all comes down in the year the host arrives,
   because a seat does not fall in a season. A host too small to shut a seat in
   never invests one however long it stands there.
3. **A seat nobody is sitting before is mended**, a little every year. A
   besieger that is beaten off or marches away loses its work slowly rather
   than at once, so a war fought in fits and starts can still take a capital -
   but a besieger that is driven away for a decade begins again.
4. **The wall belongs to the region, not to the siege.** It is written on the
   seat and outlives both besieger and besieged: a wall that has been breached
   stays breached, and the next army to come inherits the work.
5. **Taking a seat writes belonging on the region and nothing else.** The
   military layer does not reach into a polity to end it. It sets the region's
   rule to the taker and its authority to nothing - a stormed seat obeys
   nobody yet, and 07.03 makes the new holder earn its grip back - and the
   polity that held it finds out next year from 07.01's own rule that a polity
   without its seat is not a polity.

### Alternatives and decision rule

- A seat taken by winning a battle on it: rejected. It would make a capital
  worth exactly one field engagement, and it would collapse 08.03 and 08.04
  into one rule.
- A siege as a component on the army rather than the region: rejected. Two
  armies besiege the same seat over a century, and the wall is a property of
  the place; putting it on the host would lose it the moment the host did.
- The military layer dissolving the polity directly: rejected as a layering
  violation in spirit if not in letter. 07.01 already owns what makes a polity
  stand or fall, and it needs no help: writing the region's rule is enough.
- A garrison strength defending the seat: rejected for now as a second army
  with no levy behind it. The defender's own host is free to march back and
  raise the siege, which is the same drama with the pieces that already exist.

### Consequences

- The phase now runs end to end. On AELVOR 128 a levy is called, marches on the
  enemy host, fights it, the beaten one falls back or is destroyed, the winner
  sits down before a capital, and in the eighty-first year of the war the
  capital falls and a power ends - after which the world founds another, because
  07.01 keeps founding polities on councils.
- A seat that has changed hands starts at no authority at all, so 07.03 may let
  it slip again before the taker has it firmly. Conquest is not settlement.
- 08.05 has what it needs to give a war an end: something has actually been won.

### Status

Accepted 2026-09-08. Files: `Source/VaelenMilitary/Public/Vaelen/Military/Siege.h`,
`Source/VaelenMilitary/Private/Siege.cpp`, `Tests/Military/Test_Siege.cpp`
(4 tests). Headless VALIDATED on the six Linux presets.

---

## ADR-0068: The war is the thing, and the stance follows it

### Context

Through 08.04 a war was a stance. 07.06 cooled a relation past a threshold and
everything downstream - levies, marches, battles, sieges - read "at war" off
that one field. It starts a war well enough. It cannot end one, because the
thing that would end a war is two powers deciding that what it has cost them is
more than what it might win, and a stance has no memory of what it has cost.

The review found the other half of the problem. A relation outlives the
polities in it, by design: 07.06 keeps it as a record. But 07.06 only ever
revisits pairs whose ground still touches, so a relation left at war over a
polity that has been dissolved is never revisited and never turns. The survivor
read that frozen record as a live war for ever after: it kept a host raised and
fed it out of the treasury, year on year, against nobody - draining the same
treasury 07.03 spends on holding its provinces.

### Decision

1. **A war is an entity**, of kind War, opened when a relation turns to war and
   never destroyed: a war that has ended is a thing the world remembers, with
   the years it ran, the men each side lost, and who won it.
2. **The stance follows the war.** While a war is open its relation is held at
   war whatever the warmth has done, because a war is not called off by a good
   harvest; when the war ends the stance is written back to peace at a warmth
   that will hold for a while and then not. 07.06 still decides when a war
   *begins*: cooling into war is a political fact, and the military layer has
   no business deciding who falls out with whom.
3. **What ends a war is exhaustion**, kept per side: a little every year for
   the war simply going on, more for every hundred men lost, a great deal for a
   capital lost. A side worn past forfeiting will take any terms; two sides
   worn past willing make a white peace. Nothing ends in the three years it
   began.
4. **A war ends when a side does.** A polity dissolved ends its wars, with the
   other side the winner, and a war stance standing over a polity that is gone
   is written back to peace even where no war was ever opened for it.
5. **Terms are what has already happened.** Whoever holds ground at the end
   keeps it, and what the war put in play (07.06) stops being in play. There is
   no separate negotiation: the fighting already moved what was going to move,
   and a treaty clause that handed over a province nobody had taken would be a
   number with no history behind it.

### Alternatives and decision rule

- War aims declared at the outbreak, with the peace judged against them:
  rejected for now. An aim is a claim on a specific province, and nothing in
  07.06 produces one - the relation cools because of borders, size and culture,
  not because of a grievance with a place. 07.05 has grievances with places;
  when a faction can carry one into a war, aims will have something real to be
  made of.
- Exhaustion kept on the polity rather than on the war: rejected. A polity in
  two wars is worn differently by each, and a polity that made peace should
  recover from that war rather than from all of them at once.
- Ending a war by letting 07.06's warmth drift back up: rejected. It is what
  the code already did by accident, and it produced wars that ended because the
  harvest was good and wars that never ended at all.

### Consequences

- Wars now recur rather than running for ever: on AELVOR 128 two powers fight
  five wars in a century, most of them white peaces of fifteen to eighteen
  years, and the decided ones are decided by a capital falling.
- A siege has something to interrupt it. A war that ends lifts every siege in
  it, and the wall mends during the peace, so taking a capital needs a war long
  enough to finish the job - which is why the wars that are won are the ones
  where a seat fell.
- 08.06 has the number it needs: the men each side lost in each war, kept on
  the war itself.

### Status

Accepted 2026-09-08. Files: `Source/VaelenMilitary/Public/Vaelen/Military/War.h`,
`Source/VaelenMilitary/Private/War.cpp`, `Tests/Military/Test_War.cpp`
(4 tests). Headless VALIDATED on the six Linux presets.

---

## ADR-0069: A derived cache is keyed on what it was derived from

### Context

The region adjacency graph is derived from the map. Three systems walk it every
year - authority and reach (07.03), marching (08.02) and battles (08.03) - and
building it every tick would be wasteful, so each of them kept it and rebuilt
it when the count of regions changed. The count was chosen because it is cheap
and because, after generation, it never changes.

An adversarial review found and reproduced what that misses. `LoadSnapshot`
replaces the world's map wholesale, and the snapshot checks validate the seed
and the layout of the layers, not the map's dimensions or contents. AELVOR has
ninety-nine regions at 128 tiles a side and ninety-nine at 112. Load a 128
image into a world that has already ticked on a 112 map and every system keeps
the old adjacency: every distance, hold, upkeep, slip, claim, march and retreat
of that tick runs on the graph of a world that no longer exists, silently. The
same image gives two different worlds, which is the one thing the simulation
promises never to do.

### Decision

1. **A cache of a derived thing is keyed on the thing it was derived from**, not
   on any property of it. `WorldMap` counts the times its shape or contents have
   been replaced wholesale - by generation, or by a snapshot loaded over it -
   and exposes that as `Revision()`.
2. **One `RegionGraphCache`, in VaelenSim**, keyed on that revision and the
   grid, used by all three systems. Three copies of a caching rule is three
   chances to get it wrong, and this one was wrong three times.
3. **The count of regions is never a key.** It is a property of the map, and a
   property that two different maps can share is not an identity.

### Alternatives and decision rule

- Rebuilding the graph every tick: correct and simple, and rejected on the
  decision rule only after the cheap exact key was available. A 256-square
  world at five hundred years with three systems walking it is sixty-five
  thousand tiles a rebuild, three times a year.
- Hashing the region layer as the key: correct, and O(tiles) every tick for
  every system - the same cost as the rebuild it is meant to avoid.
- Validating the map's dimensions in `LoadSnapshot` and refusing a mismatch:
  rejected as the wrong fix in the wrong place. Loading a save of another world
  size is a reasonable thing to do; the bug is that a derived cache did not
  notice.

### Consequences

- Every frozen digest is unchanged. The fix changes nothing in a world that was
  never mishandled, which is what a fix for a stale cache should look like.
- Two tests hold it: one at the Sim level, that the cache rebuilds when the map
  under it changes though the count does not (and it records the 128/112
  collision that makes the old key wrong); one at the Politics level, that a
  snapshot loaded into a world which has already run fifty years on another map
  gives exactly the world the image came from.
- Anything else derived from the map and kept across ticks now has a key to use.

### Status

Accepted 2026-09-08. Files: `Source/VaelenSim/Public/Vaelen/Sim/Regions.h`,
`Source/VaelenSim/Private/Regions.cpp`, `Source/VaelenSim/Public/Vaelen/Sim/WorldMap.h`,
`Source/VaelenSim/Private/WorldMap.cpp`, `Source/VaelenPolitics/.../Reach.*`,
`Source/VaelenMilitary/.../March.*`, `.../Battle.*`, `Tests/Sim/Test_Regions.cpp`,
`Tests/Politics/Test_Reach.cpp`. Headless VALIDATED on the six Linux presets.

---

## ADR-0070: A person index is never handed out twice

### Context

`PersonInfo::Index` is the compact key everything outside the population uses
to name a person: a council's head (05.01), a polity's ruler (07.01), a line's
sitting ruler and its claimant (07.04), a faction's claimant (07.05). All three
places that made people - promotion, birth, and the LOD bridge - allocated the
next index as one past the highest person alive.

That is not a counter. Population LOD demotes a region by destroying every
person in it, which lowers the highest alive, and the next promotion or birth
hands the same band of indices out again. An adversarial review reproduced what
follows: the council of a demoted region keeps a `Head` pointing at a destroyed
person, `FindPerson` correctly returns nothing for it - until the index is
recycled onto a live adult in another region, at which point 07.01 seats that
stranger as the polity's ruler and 07.04 names their children as claimants to a
throne they have never heard of. Nothing detects it, because every check that
could have is written in terms of the index.

### Decision

1. **One counter, in world state.** A `PersonCounter` singleton component lives
   on one entity of the world, made with the first index ever taken, seeded past
   anything that already exists so that a world built before it keeps its
   people. It only ever goes up, it is carried in a snapshot like any other
   state, and it is destroyed by nothing.
2. **Every person takes its index from `TakePersonIndices`.** All three
   allocation sites go through it. There is no other way to get one.
3. **The fix belongs in the allocator, not in the readers.** Teaching 07.01 to
   check that its ruler still sits on the council it was seated from would fix
   one reader; there are four, and the next one written would not know.

### Alternatives and decision rule

- Deriving the index from the entity id, whose allocator is already monotonic:
  rejected. It would make indices sparse, and several places size an array by
  the highest index they have seen.
- Clearing every reference when a region is demoted: rejected. The population
  would have to know about councils, polities, lines and factions - the exact
  inversion of the layering the project is built on.
- Never destroying people on demotion: rejected; that is what LOD is for.

### Consequences

- Four frozen state digests moved - the population gate, the society gate, the
  economy gate and the politics gate - because in a run where a region is
  demoted the indices genuinely differ now. Every invariant those gates check is
  unchanged; only the digests are, and they are refrozen with the reason written
  beside them.
- The world carries one more entity, for the counter, from the first person it
  ever makes.
- Anything else that hands out a dense index from "one past the highest alive"
  has the same defect. Nothing else does today.

### Status

Accepted 2026-09-08. Files: `Source/VaelenPopulation/Public/Vaelen/Population/Persons.h`,
`Source/VaelenPopulation/Private/Persons.cpp`, `Private/Lives.cpp`,
`Private/Lod.cpp`, `Tests/Population/Test_Persons.cpp`. Headless VALIDATED on
the six Linux presets, with every phase gate re-run at 256 over 500 years.

---

## ADR-0071: The numbers land on people

### Context

Through 08.05 the phase moves numbers. A levy is men taken out of regions, a
battle is men taken out of a levy, a siege is a wall coming down. At no point
does anybody die: the count of men away comes back down, and the people of the
region are exactly as many as they were before the war. That is the difference
between a wargame and a world, and the world is the thing being built.

### Decision

1. **The levy says why men left it.** `ReleaseLevy` takes a `LevyEnd` - home,
   melted, fallen - and publishes one record per region it touched. The levy
   itself still does not care: it only knows the men are no longer under arms.
   A higher layer reads the reason and decides what it meant.
2. **The fallen are dead where they came from.** Where a region is simulated
   person by person (04.06) they are persons, struck out in index order so the
   same men die on every run of a seed. Where it is not, they are people taken
   off the count. Either way the region that gave them is the region that loses
   them: that is what makes a levy a cost rather than a number.
3. **What men bring home is standing.** 05.02 grants standing for what other
   people know about you, and a man who marched and came back is granted
   something a man who stayed is not - a fixed amount per war, up to a cap,
   because the fourth war buys nothing the third did not. `PersonService` is
   declared in society, beside the standing it weighs, and written by the
   military: the same pattern as a house's wealth.
4. **People leave ground an army will not get off.** A region a host has stood
   on for years loses a share of its people to a neighbour its own ruler still
   holds - not to another region under a host. This is how a province empties
   without a battle being fought on it. It is coarse only: moving a simulated
   person between regions is migration, and migration is not this task.
5. **Nothing here decides anything.** The system reads what the war did - the
   levies released, the ground foraged - and writes what it cost. Every rule
   about who fights whom, and where, lives upstream.

### Alternatives and decision rule

- Killing people inside `ReleaseLevy`: rejected. The military module would be
  reaching into persons, standing and population counts from the middle of an
  accounting routine, and the same call is made when men walk home unharmed.
- Deaths as a share of the region's population rather than the men it gave:
  rejected. The whole point of ADR-0064 is that an army is *these* people from
  *these* regions; killing a proportion of the wrong region would throw that
  away for nothing.
- Standing computed from a war record rather than a component: rejected.
  `StandingScore` is a pure function of what a person is, and service is
  something a person is.

### Consequences

- A hundred years of two powers on AELVOR 128 kills 1847 men across 39 regions,
  sends 884 home, moves 113 people off ground they would not stay on, and
  leaves 90 people carrying a war on their standing. War is now visible in the
  population, not only in the military's own books.
- Declaring `PersonService` changes the type registry, which changes the state
  digest of every world that declares the standing types - three phase gates
  were refrozen for it, with every invariant they check unchanged.
- 08.07 has what the chronicle needs: a death has a region and a number, a
  flight has a where-from and a where-to.

### Status

Accepted 2026-09-08. Files: `Source/VaelenMilitary/Public/Vaelen/Military/Toll.h`,
`Source/VaelenMilitary/Private/Toll.cpp`, `Source/VaelenMilitary/.../Armies.h`,
`Source/VaelenSociety/.../Standing.h`, `Tests/Military/Test_Toll.cpp` (4 tests).
Headless VALIDATED on the six Linux presets.

---

## ADR-0072: A chronicle is the small part a century keeps

### Context

Six tasks of Phase 08 make war happen and none of them say so. The event log
holds all of it - every hop of every march, every measure of grain foraged,
every man released from every levy - which is precisely what nobody remembers.
07.07 already settled the shape for politics; this is the same shape one layer
up, and the same argument.

### Decision

1. **The chronicle keeps a few kinds and drops the rest.** A levy called, a
   host melting for want of grain, a battle, a host broken, a siege laid, a
   capital stormed, a war begun, a war ended - and a toll only past a mark. A
   region losing two or three men is not history; a region losing eight in a
   year is the year it is remembered for.
2. **Every military event has a sentence, kept or not.** The describer is not
   the listener: anything the log holds can be read out, which is what the why
   export and any future interface need. It falls through to the politics text
   for everything else, so a military chronicle can tell you about a harvest.
3. **The line is stamped in the same hand as every layer below** - the year and
   the age first. A chronicle whose lines do not agree on their own format is
   not a chronicle.
4. **A polity is named by its seat where the politics context is at hand.** The
   military layer knows a polity is a thing with a number; only 07.01 knows it
   is a thing with a seat. Where the context is absent the number is used, and
   where a polity has been dissolved the number is all there is - which is the
   honest answer, not a failure.
5. **Causes are chained where one thing really did cause another.** A battle
   publishes its own event first and hands that id to the levy releases it
   caused and to the breaking or the falling back that followed; a burial names
   the release that reported the fallen. That, and not a new data structure, is
   what makes "the why of a lost province" a chain: three steps from a village
   burying a man to the battle that killed him.

### Alternatives and decision rule

- Recording every military event: rejected. The chronicle would be the log with
  extra steps, and a century of two powers would produce tens of thousands of
  records nobody could read.
- A separate "war record" entity summarising each war: rejected as a second
  representation of what `WarInfo` already is. The chronicle's job is the
  sentence, not the state.
- Storing cause ids on the components (a siege remembering the event that laid
  it): rejected for now. It would grow three structs past their size asserts to
  deepen chains that are already three steps, and the ones that matter - a
  burial back to its battle - are chained without it.

### Consequences

- A hundred years of AELVOR 128 with two powers writes 241 military records,
  and two worlds of one seed write them word for word: the chronicle text is
  now a frozen digest like any other.
- The seventeen military events all read as sentences, which is what 08.08's
  gate will check across five centuries and what a Phase 12 interface will
  show.
- Chaining causes cost a reordering in the battle - it publishes before it
  releases - and nothing else.

### Status

Accepted 2026-09-08. Files: `Source/VaelenMilitary/Public/Vaelen/Military/MilitaryHistory.h`,
`Source/VaelenMilitary/Private/MilitaryHistory.cpp`, `Tests/Military/Test_MilitaryHistory.cpp`
(4 tests). Headless VALIDATED on the six Linux presets.

---

## ADR-0073: A gate is where the phase finds out what it got wrong

### Context

Every task of Phase 08 shipped green: its own tests passed, the six presets
passed, the frozen digests held. The gate runs five centuries at 256 with every
Phase 04 to 08 system at once and checks every invariant of every one of them
each decade. It failed immediately, four different ways.

That is not a failure of the tasks. It is what a gate is for, and the four are
worth recording because they are all the same shape: a system tested against
its own module behaves; the same system tested against every module it stands
on does not.

### Decision

1. **The gate checks the invariants of every module beneath the phase**, not
   only the phase's own. Three of the four defects were found by checking the
   Phase 07 measures inside a Phase 08 world.
2. **A measure's `Bad` means incoherence, and nothing else.** `MeasurePolities`
   counted a polity whose seat had just been stormed, and one whose ruler had
   just been killed, as bad. Neither is: 08.04 can take a seat and 08.06 can
   kill anybody, and 07.01 answers both on its next tick. They are now `Doomed`
   and `Bereft` - counted, visible to a gate, and not confused with a world
   that does not add up. A measure written before a phase existed will call that
   phase's normal outcomes wrong; the fix is to name them, not to widen `Bad`.
3. **A cross-layer write clears what it invalidates, in the same tick.** A host
   marching on an enemy that has been beaten and fallen back, destroyed, or
   whose capital has changed hands, is under an order to march on nothing. The
   battle, the siege and the war each now clear the orders they invalidate,
   rather than leaving a stale one for the next year's march to notice.
4. **A snapshot replayed must reach the world it was taken from**, over the
   whole length of the gate and not a decade of it. That check found a
   divergence at 250 years that nothing shorter had.

### Alternatives and decision rule

- Tolerating the one-tick transients in the gate (allowing `Bad <= 1` in a year
  a seat was stormed): rejected. It would have hidden the third defect, which
  was a real stale order and not a transient at all.
- Leaving stale orders for the next year's march to overwrite: rejected. It
  works, and it means the world spends a tick in a state its own measures call
  wrong, which is exactly the state a gate cannot distinguish from a bug.

### Consequences

- Phase 08 is closed: six Linux presets green with every gate, 92 CTest entries
  each, purity 136 files 0 violations, every file of the phase VALIDATED.
- The four frozen digests of the gate - state at 250 and 500, the event log, and
  the chronicle as text - are the strongest guard the project has: any change to
  any system of any phase from 04 to 08 that alters the world will move one.
- Phase 09 inherits a world where war is a thing that happens to people.

### Status

Accepted 2026-09-08. Files: `Tests/Military/Test_MilitaryGate.cpp`,
`Source/VaelenPolitics/Private/Polities.cpp`, `Source/VaelenMilitary/Private/Battle.cpp`,
`Private/Siege.cpp`, `Private/War.cpp`. Headless VALIDATED on the six Linux
presets with every gate.

---

## Verification record

Executed on 2026-09-05 after the Phase 00 review pass (clang++ 18.1.3, g++ 13.3.0, CMake 3.28.3, Ninja 1.11.1, Python 3.11.15, clang-format 18.1.3, Linux x86_64), with the checked-in
presets into `out/build/<preset>`:

| Preset | Build | `ctest` | `VaelenCoreTests` |
|---|---|---|---|
| linux-clang-debug | 0 warnings | 14/14 passed | 133 run, 133 passed, 21914 checks |
| linux-gcc-debug | 0 warnings | 14/14 passed | 133 run, 133 passed, 21914 checks |
| linux-clang-release | 0 warnings | 14/14 passed | 133 run, 133 passed, 21914 checks |
| linux-gcc-release | 0 warnings | 14/14 passed | 133 run, 133 passed, 21914 checks |
| linux-clang-noasserts | 0 warnings | 14/14 passed | 108 run, 108 passed, 21701 checks |
| linux-gcc-noasserts | 0 warnings | 14/14 passed | 108 run, 108 passed, 21701 checks |

Per-suite counts: Assert 33, CoreTypes 1, Harness 5, Hash 15, Ids 19, Log 23, LogFloor 1, Random 29, Version 7 (133 tests with assertions, 108 without). Purity: `python3 Tools/check_kernel_purity.py --self-test`
-> 36 checks, 0 failed; `--root . --verbose` -> 12 files, 0 violations, 2 exemptions.
clang-format 18 dry run: 0 drift. GitHub Actions run 5 (commit `71bad2d`, https://github.com/Thomas10112/vaelen/actions/runs/33977296696): all 9 jobs green - six Linux presets, clang-format 18, Windows MSVC 19.44 (`windows-msvc-debug`, 14/14 CTest entries), macOS 15 AppleClang (`macos-debug`, 14/14). The engine (UBT) build was not executed.

## ADR-0074: A building is a thing that holds, not a number the simulation reads

### Context

Phase 09 gives the world infrastructure. The easy version of that is a number:
`RegionGranary`, read by the famine code, raised by whoever wants it lower. Two
phases already show why that ends badly. 05.05 gave councils a granary as a
number on a region, and nothing in the world can point at it; 06.04 gave the
world routes as artefacts of trade, and a route is not a road anybody built.
Both are useful and neither is a thing.

### Decision

1. **A building is an entity of kind Building standing in a region**, with what
   it is, how much of it there is, how sound it still is, and what it cost. It
   can be pointed at, saved, named, ruined and inherited by a later phase.
2. **A building is never a number the simulation reads instead of the world.**
   A granary does not make a famine less likely; it holds grain, and the famine
   of 06.02 finds the grain there. Anything that cannot be expressed as a thing
   that holds, lowers or raises something an earlier phase already computes does
   not belong in this phase (09.02 is where every kind gets its effect).
3. **It is raised out of what the region holds IN COMMON, never out of a house's
   own goods.** A granary is not one family's. So a coarse region builds out of
   its common stock and a detailed one out of what a council put by (05.05),
   which is the same rule read from the other end - and a region that cannot
   feed the builders does not get the granary, however many people it has.
4. **A region keeps at most one work of each kind and makes it bigger.** A
   granary of three is one building enlarged twice, not three granaries. It
   keeps the region's summary a small fixed table, gives 09.05 one thing to let
   fall rather than a heap, and makes "how big is the granary here" a single
   number - `KeptSize` - which is the only thing 09.02 will read.
5. **The summary on the region is a cache, and the buildings are the truth.**
   `RegionWorks` exists so the layers below need not walk every building in the
   world; `MeasureBuildings` recomputes it from the buildings and counts any
   disagreement as `Bad`, so the cache cannot drift in silence.

### Consequences

The cost is real and it is paid in the log: the raising is published first and
every unit of timber, tools and grain is taken with that event as its cause, so
"what did this granary cost" is a query, not a comment. A test proves the sum of
what the raisings took equals the sum of what the buildings say they cost.

Nothing built does anything yet. That is deliberate: 09.01 makes the thing exist
and be paid for, 09.02 gives every kind its effect on the layers below, and 09.05
lets what is not kept fall down.


## ADR-0075: A building is the reason a number moves, and it owns its own field

### Context

ADR-0074 says a building is never a number the simulation reads instead of the
world. 09.02 has to make good on that: four kinds of work, four things they do,
and none of them a new rule about famine, harvest or siege.

There is a second problem underneath it. A region's granary already has a
number: 05.05 writes `RegionStores::GrainPerMille` when a council decides to put
grain by after a drought. If 09.02 wrote the same field, two systems would take
turns overwriting each other every year and the last one in the schedule would
win.

### Decision

1. **Every kind of work is turned into the one number an earlier phase already
   reads, and that phase is not touched.**

   | work | number | who reads it |
   |---|---|---|
   | granary | `RegionStores::BuiltPerMille` | 04.04 softens the drought's cut |
   | mill | `RegionWorkshops::FieldsPerMille` | 06.02 reaps more from the same fields |
   | smithy | `RegionWorkshops::CraftPerMille` | 06.02 makes more cloth and tools |
   | wall | `RegionWall::Extra` | 08.04 brings less of it down in a year |

   Not one of those four systems learned anything about buildings. Each reads a
   component it already knew how to read, and a world without infrastructure
   reads a factor of one - to the unit, which is why every frozen digest of
   Phases 04 to 08 survived this task unchanged.

2. **Two writers never share a field.** `RegionStores` now carries a council's
   share and a granary's share side by side, and 04.04 adds them. A council
   deciding to store grain and a region having somewhere to put it are two
   different facts about the same year, and the world should hold both.

3. **The numbers are recomputed from what stands, every year, and never added
   to.** A work that falls (09.05) takes its effect with it the same year,
   because nothing anywhere remembers to subtract it - the value is a pure
   function of the region's summary. That is also why `MeasureWorks` can check
   every written number against the buildings that justify it and count any
   disagreement as `Bad`.

4. **A cap per kind, in the rules.** A region that builds granaries for four
   centuries does not become immune to drought. The cap is the statement that
   infrastructure changes the odds and never the rules.

### Consequences

The test that matters is not that the number is written but that the layer below
behaves differently: two worlds of one seed that build the same things for the
same price, with the works worth nothing in one of them, reap 27 221 041 grain
against 26 243 333. And a seat's wall, played twice from the same snapshot in
the same tick, comes down by 112 behind a wall of five, by 148 behind a wall of
one, and by 176 behind none.

The wall is the one whose exact factor a test does not assert, only its
monotonicity and its direction: what a besieger brings down in a year depends on
its strength at the moment the siege system runs, which a test cannot read back
after the tick. The three-way comparison from one snapshot is the honest form of
that claim.


## ADR-0076: A settlement is a fact about trade; a place is a thing on the map

### Context

06.04 already founds settlements, and they are not places. A settlement of 06.04
is a fact about trade: goods changed hands on this region often enough and long
enough that somebody stayed. It has a region, a traffic and a count of routes -
and no position, no size and nothing standing in it. That is exactly right for
what it was for, and not enough to build in, draw, or stand in.

The obvious move is to add a position field to `SettlementInfo` and be done. It
is the wrong one: the economy would then carry a fact it has no use for and no
way to check, and 06.04's frozen digests would move every time 09.03 changed its
mind about where towns go.

### Decision

1. **A place is a second component on the same settlement entity**, not a wider
   `SettlementInfo`. Trade knows why the town is there; infrastructure knows
   where it is and how big. Neither can corrupt the other's answer, and 06.04's
   digests did not move.
2. **A place stands on ONE TILE, chosen once and never moved.** It is the first
   thing on this map smaller than a region, which is what everything after it
   needs: 09.04 to run a road to it, Phase 13 to draw it, a player to stand in
   it. Moving it later would invalidate all three, so it is fixed at founding.
3. **Where it goes is a rule about the world, not a draw.** On water where the
   region has water, at the region's heart where it has none, taking the tile
   nearest the centroid in both cases, and never a tile another place already
   holds - a ruin included, because a ruin holds its ground.
4. **A size from the people around it and the goods through it**, capped, with a
   `Grown` that never falls. A town is not a settlement's traffic under another
   name: it is how many live in it rather than on the land around it, plus what
   passes through, and the cap is the statement that this world does not make
   cities.
5. **A region's works stand in its town where it has one**, on the same tile, and
   in the countryside where it has not. `BuildingPlace` is the building's own
   component so that 09.01 need not know towns exist.

### Consequences

A region holds several places over the centuries and most of them are ruins:
120 places over 120 years at 128, of which 32 stand and 88 emptied. So "the
place in this region" has to mean the one still standing, and only fall back to
the oldest ruin when nothing stands. Getting that wrong is what the first run of
the test caught: buildings were being placed in towns that had been abandoned
two centuries earlier.

A place's size is zero exactly when its settlement is abandoned, which is the
invariant that keeps the two components honest with each other, and
`MeasurePlaces` checks it along with the tile being inside its own region, one
place to a tile, and the town's own count of works agreeing with the buildings
placed there.


## ADR-0077: A road is what somebody made of a route, and it falls back to one

### Context

A route of 06.04 is not a road. It is a fact about prices: grain was dearer
there than here, often enough and long enough that somebody carried it. It opens
because a gap opened and closes because nothing crossed it, and nobody ever
built it. 09.04 has to let the world build one without turning the route into
something the economy no longer recognises.

### Decision

1. **A road is a second component on the route entity**, the same shape as
   09.03's places on settlements. Trade knows the route exists and why;
   infrastructure knows what has been made of it. No digest of Phase 06 moved.
2. **It is cut out of the two ends together, half each.** A road is never one
   region's: both common stocks pay the timber and both must have the hands to
   spare. A region rich in timber beside a poor one still gets no road.
3. **Only where enough already crosses.** A road is not a wager on trade that
   does not exist yet: the route must already have carried enough over its life
   to be worth the timber, and a further grade needs that much again in a single
   year. What a road does is let more of the trade that already wanted to happen
   get across - it creates none.
4. **What it does is one number, where trade already reads it.**
   `RouteEase::CarryPerMille` raises both the share of a surplus carried and the
   yearly cap, and no road is a factor of one to the unit. Same contract as
   ADR-0075's four hooks.
5. **Falling back is not destruction.** Unkept, a road wears; with nothing left
   to wear it loses a grade; at grade zero it is a track again - which is what a
   route was before anybody touched it. The entity lives, the route lives, and
   regions with timber can cut it again. Nothing in this phase deletes anything.

### Consequences

Most roads outlive the trade that made them and then go: 22 roads cut over a
century at 128, of which 3 are still made and 19 have gone back to tracks. The
reason is worth stating plainly - a road on a route trade has closed cannot be
kept, because keeping is a thing the two ends do for a route that is still
carrying. A road outliving its reason for a decade and then fading is the
behaviour wanted, not a defect.

The effect on trade is real and small: 321 307 units carried with roads against
321 110 with roads worth nothing, over the same century of the same world,
built the same way and paid for the same. Small is correct here. A road that
doubled trade would be a road that created it.


## ADR-0078: What is not kept falls, and what falls stays on the ground

### Context

09.01 raises things and 09.02 makes them matter. Neither takes anything away,
which means a world that runs long enough is a world where every region has
every work at its cap and nothing has ever been lost. That is not a living
world; it is an inventory. And the first two hundred and fifty year run said so
plainly: 129 works standing, none of them ever lost.

### Decision

1. **A work wears at a rate set by what it is.** A granary of wood goes at 70
   per mille a year, a wall of stone at 25. Against that the region pays a
   little timber out of its common stock every year to mend what it has. What it
   cannot pay for wears; what wears to nothing falls.
2. **The year takes its toll before the region mends.** A work kept every year
   reaches full repair and stays there; one that is not falls at the rate of
   what it is. The other order gives a world where nothing is ever quite sound
   and nothing ever quite falls.
3. **The weather and the war are extra wear on the same number**, never a
   separate rule: a flood or an eruption of 03.05 (never a drought or a plague -
   those kill people, not walls), a foreign host standing on the region, a siege
   before its seat. The system runs after `Disasters` so that a flood is felt by
   the walls in the year it struck rather than a year late.
4. **It runs BEFORE Buildings**, so the region sees what it actually has before
   it decides what to raise or enlarge. A world where the decision is made on
   last year's stock is a world that builds what it already lost.
5. **A fallen work is not deleted.** It stays on the ground as a ruin, and
   raising that kind again on ground that already holds its ruin costs less -
   the stone is there. That is the whole of what a ruin does, and it is enough
   to make where a world has already been matter to where it goes next.

### Consequences

The world stops being an inventory. Two hundred and fifty years at 128: 58 works
standing of which 49 sound and 9 worn, 40 fallen, 3 raised back on their own
ruins. And the gift of goods the other tests hand every region every year had to
go from this one - a region handed timber every year keeps everything it has
ever built, which is exactly the world this task exists to prevent.

The count of falls is a poor instrument for the weather: a storm knocks a work
down but the region mends it next year, so over centuries the same set of works
falls either way. The honest proof is to play one year twice from a single
snapshot, as 09.02 did for the wall - region 31's granary, in the year a flood
struck it, went 1000 to 1000 with the storm worth nothing and 1000 to 650 with
it worth five hundred per mille: the 500 of the storm and the 70 of the year
taken off, and the 220 of the mending put back.

**A disclosed gap:** the war side of the same rule (a foreign host, a siege) is
wired identically and is not proven by a test of its own here, because the world
this suite runs did not put a host on ground with works in the years it was
watched. It is exercised at the Phase 09 gate (09.08), which runs the military
systems at 256 for five centuries.


## ADR-0079: One number on the ground, two systems that walk it

### Context

09.04 made roads and gave them one job: more of the trade that already wanted to
happen gets across. That is the economy's side of a road, and it is not what a
road was ever mainly for. An army marches on it. A courier, a tax collector and
a garrison go up it. Both of those already exist - 08.02 walks the region graph
hop by hop, 07.03 lets a polity's hold fall away with the hops from its seat -
and neither has any idea what a road is.

### Decision

1. **One number on the region, two readers.** `Politics::RegionWays::EasePerMille`
   is declared in the lowest module that needs it (Politics), and both the reach
   of 07.03 and the marching of 08.02 observe the same component. Two parallel
   structs saying the same thing would drift the first time one of them was
   tuned.
2. **Four uses, all of the same shape as ADR-0075's hooks.** A host gets further
   in a year along made ground and eats less beside it; a polity's hold falls
   away more slowly along it and the upkeep of carrying a word that far costs
   less. Every one of them is an existing number multiplied by a factor that is
   exactly one where nothing has been built - which is why every frozen digest
   of Phases 07 and 08 came through this task unchanged.
3. **A region is served by its BEST road, never by the sum of them.** Four
   tracks meeting at a village do not make a highway. Adding them up would make
   the busiest crossroads unconquerable by arithmetic rather than by anything
   anybody did.
4. **Written every year from what stands, never added to.** A road falling back
   to a track (09.04) takes its worth off the ground the same year, and
   `MeasureLogistics` recomputes the whole table and counts any disagreement as
   `Bad`.

### Consequences

The effect on a polity is large and that is right: at 128, region 19 sat two
hops from its seat on ground its best road made 200 per mille easier, and after
two years it was held at 274 with the road and had **slipped free entirely**
without it. A road is how an empire holds ground it could not otherwise reach,
and this is the first thing in the project that says so.

The effect on a host is exact: on the same tick from one snapshot, a host on
region 52 took 410 grain off the field with no road beside it and 256 with one -
the same appetite divided by 1600 over 1000, which is what the rule says. What
it needs comes up the way rather than off the ground it is standing in.


## ADR-0080: The chronicle of a phase speaks for every phase under it

### Context

Phase 09 has spent six tasks building things and none of it saying so. The event
log holds every unit of timber taken and every year a road was mended, which is
exactly what nobody remembers. This is the third time the project has hit the
same problem (07.07, 08.07), and the third time it is solved the same way, which
is the point of recording it once more.

### Decision

1. **The same shape as 08.07, one layer up.** A listener turns the few events
   that matter into records; a describer gives every infrastructure event a
   sentence whether the chronicle kept it or not; two exports give the whole
   chronicle in order and the why of one event walked back to its root.
2. **The describer of the topmost layer speaks for every layer under it.**
   `DescribeWorksEvent` falls through to the military text, which falls through
   to the politics text, and so down to the person. An infrastructure chronicle
   can tell you about a harvest, and it does:
   `Year 0, age of Divik: Edavaken harvested 994 of grain.`
3. **A road is filed at the end it runs from**, the way 08.07 files a war at the
   seat of the side its record names. A thing that belongs to two regions still
   has to be findable from one of them.
4. **What is history and what is not is a rule, not a judgement.** A raising is
   history and an enlargement from two to three is not (`RecordEnlargements`
   defaults to zero); a town growing by one is not, and a town reaching a size
   worth remembering is.
5. **A fall names the blow that finished it.** `BuildingFellEvent` is published
   with the flood or eruption of the year as its cause where there was one, so
   that the why is a chain and not a line.

### Consequences

The sentence this phase existed to be able to write:

```
Year 431, age of Ekut: the granary of Osvin fell in.
  because a flood struck Osvin and 23 died.
  because omens of flood were seen over Osvin.
```

Two hundred and fifty years at 128 keep 737 records of 1613 infrastructure
events - 98 raisings, 41 falls, 74 roads and 524 towns - and two worlds of one
seed write all 1138 lines of it word for word.

The why of a fall the years alone brought about is one line, and that is the
truth of it: nothing caused it but time, and a chronicle that invented a cause
there would be worse than one that says so.


## ADR-0081: A gate that passes first time is a claim about the seven tasks before it

### Context

The Phase 08 gate failed immediately, four different ways, and ADR-0073 records
what that taught: a system tested against its own module behaves; the same
system tested against every module it stands on does not. The Phase 09 gate
passed on its first run - fifty decades, every invariant of all six Phase 09
measures and of the Phase 07 and 08 ones under them, a snapshot of year 250
replayed to exactly the year 500 the first world reached.

That is a good outcome and a suspicious one, and it is worth writing down which
of the two it is.

### Decision

Record it as earned, and say what earned it. Three habits came out of Phase 08
and were applied to every task of Phase 09 from 09.01:

1. **Every cross-layer effect is one number an earlier phase already reads, and
   is exactly one where nothing is built** (ADR-0075, ADR-0077, ADR-0079). Seven
   hooks were added to five systems across Phases 04, 06, 07 and 08, and not one
   frozen digest of those phases moved. The gate could not find an interaction
   defect in code that provably does nothing when it is not used.
2. **Every derived number is recomputed from what stands, never added to**
   (ADR-0075, ADR-0078, ADR-0079). The class of defect the Phase 08 gate found -
   a stale order left behind by a change somewhere else - cannot occur in a value
   that is a pure function of the current world.
3. **Every measure recomputes what it checks and counts disagreement as `Bad`**,
   so a cache that drifts is a test failure in the task that introduced it rather
   than a mystery at the gate.

And one thing the gate did settle that no task suite could: 09.05's war wear.
That was disclosed in the 09.05 commit as wired-but-unproven, because the world
that suite runs never put a host on ground anybody had built on. The gate does,
and the arithmetic is exact.

### Consequences

Five centuries at 256 in 506 seconds with assertions on: 200 works raised, 389
enlarged, 112 fallen, 10 raised back on their own ruins, 67 towns standing, 210
roads cut and 165 lost, 23 regions still served by a made road, 5537 chronicle
records.

Read across the decades, the world builds up and then wears down to a working
equilibrium rather than to either extreme: 89 works standing at year 100 and 88
at year 500, with the ruins growing from 15 to 112 underneath. Roads peak at 51
made around year 200 and fall to 18 by year 500 - most roads outlive the trade
that made them and then go, which is what ADR-0077 said would happen and is the
first time it has been seen over five centuries.

A gate that passes first time is not a reason to trust the next one less. Phase
10 gets the same gate.


## ADR-0082: The player is a mark on somebody the world already had

### Context

Nine phases have built a world of people who are born, eat, work, are bound and
freed, marry, hold office, march and die. Phase 10 has to put a player in it,
and the obvious shape - a Player entity with its own position, its own needs and
its own rules - is the one that quietly ends the project. A player that is not a
person is a second simulation running beside the first, and every system after
this one has to be written twice.

### Decision

1. **The player is a component on an existing person entity**, and nothing else.
   No position of its own, no needs of its own, no rules of its own. The person
   was already being simulated by 04.01 through 09.06 and goes on being.
2. **The mark does nothing at all in 10.01.** It does not tick, publish, or read
   anything. That is not an unfinished task; it is the foundation the rest of
   the phase is built on, and it is tested before anything is built on it: a
   world with a player in it and the same world without one run to the same state
   digest for fifty years. Two worlds of one seed, one marked and one not:
   2550e13a5efc68c6 both.
3. **One player to a world.** Taking a second is refused rather than leaving the
   first behind with no way to say which was meant.
4. **Only a living person of a detailed region.** A coarse region has no persons
   to be, only a count of them - so `TakePlayer` on somebody there fails, and
   there is nothing to be confused about later.

### Consequences

Everything the player will do in 10.04 and 10.05 has to go through a system that
already owns that part of the world, because there is no other way in: the mark
carries no state to change. That is the constraint that makes a played life
replayable from a recorded command stream, which is what 10.08 will test and
what this project is for.

The rule to hold to as the phase goes on: if a command cannot be expressed as
something a person in that world could do, applied by the system that already
owns it, it does not belong in Phase 10.


## ADR-0083: A start is found, not written

### Context

The player begins bound. The obvious way to arrange that is to bind somebody at
the moment the game starts - pick a person, set their `BondState` to Enslaved,
begin. It takes four lines and it breaks ADR-0082 completely: it writes state
into the world from outside the simulation, and a world that can be written to
from outside cannot be replayed.

### Decision

1. **The start looks for a life the world already made, and the search is
   allowed to come back empty.** 05.04 binds people every year - for debt, at
   birth, by capture, by being on the wrong side of a promotion - and 10.02
   takes one of them. A world whose detailed regions hold nobody bound returns
   zero and writes nothing at all.
2. **Ore ground is a preference, not a requirement, and the record says which it
   got.** The ground a mining colony would stand on and the ground the most
   people live on are not the same ground: at AELVOR 128 the two busiest
   regions - the only ones simulated person by person - have no ore under them
   at all. A start that refused to happen over that would be a start that never
   happens. `PlayerStart::OnOre` records the truth either way, and Phase 11 will
   have to reconcile the colony with where the people actually are.
3. **`PlayerStart` is a record and never a rule.** Nothing reads it to decide
   anything; it says what the life WAS at its first moment - the region, the
   bond and who held it, the family, the standing 05.02 had already given them,
   their age - and it does not change as the life goes on. The world runs the
   same whether it is there or not.
4. **The lowest person index among those offered**, so that the same world hands
   over the same life twice, which is what a replay needs.

### Consequences

The start is honest about the world it landed in: at AELVOR 128 after sixty
years of detail there are 288 bound people to choose from, and the one handed
over is person 3821 of region 26 - bonded, held by person 720, of family 767,
with the standing 05.02 had already given them, aged 40. Not a character sheet:
a place in a world, with a holder who exists and a family who exist.

And the failure mode is a real one that a test asserts rather than a hypothetical
one: a world configured never to bind anybody offers nobody, and the start says
so instead of manufacturing a life.


## ADR-0084: The first system in nine phases to want a grain finer than the year

### Context

Every system built so far runs at `SimLod::World`: once every 8640 ticks, which
the calendar of 01.04 calls a year. That is right for a harvest, a levy and a
polity, and it is useless to somebody living a life. A person eats today, works
today and sleeps tonight.

The scheduler has had the finer grains since 01.03 and nothing has used them:
`Period[]` is `{1, 4, 24, 720, 8640}`, so the tick IS the hour and
`SimLod::Aggregate` is the day. 10.03 is the first thing in the project to run
at one of them.

### Decision

1. **It runs at the day, and for exactly one person.** `PlayerDaySystem` is
   `SimLod::Aggregate`, so it fires 360 times a year against every other
   system's once, and its first act is to look for the mark: with nobody played
   it returns immediately and costs a pointer chase.
2. **What it does is grant time, and nothing else.** A day gives the person
   waking hours; 10.04's commands will spend them. `SpendHours` is a budget and
   never an overdraft - asking for more than is left gives what is left.
3. **It publishes nothing.** That is the claim that lets a person live an hour
   at a time inside a world that runs at the year, and it is testable exactly:
   a world with somebody played and a world without, both carrying the system,
   write the SAME event log over twenty years - e4b3a487cb40e4af both, over 7200
   days lived. The state digests differ, because the mark and the day are state;
   the history does not, because the day makes none.
4. **A day that cannot turn is counted, not skipped silently.** If the played
   person dies or goes back to the coarse grain, `Missed` grows and the record
   says how many days went by without them.

### Consequences

The cost is a system tick 360 times a year for one person, which is what a
playable life costs and is the cheapest form of it: the world around them keeps
its yearly grain, and only the played person pays for the finer one. Phase 11's
mining colony will want the same treatment for a whole region, and this is the
shape it will take.

The rule to hold to: anything the fine grain writes must be the player's own
record. The moment it writes something the world owns, the log digests diverge
and the test in `Test_Hours.cpp` fails - which is the intended alarm, not an
inconvenience.

## ADR-0085: What arrives from outside the simulation is a struct in a queue

### Context

The rule of Phase 10 is that the player is a person the world already had. It is
easy to write down and easy to break: the shortest path from a keypress to a
changed world is to reach into a component and set a field, and every project
that does that loses replay in the same afternoon.

The world of nine phases is already fully determined by its seed. A played
person adds a second source of input - what that person decided to do - and the
whole question of the task is what shape that input has, because the answer
decides whether a life can be replayed at all.

### Decision

Intent is data, and only the simulation acts on it.

1. **What arrives from outside is a `PlayerCommand`.** A flat 24-byte struct: a
   kind, a target, an amount, an hour cost, and the tick it was meant on.
   `Submit` puts it in a ring on the played person and does NOTHING else - no
   store moves, no standing changes, no hour is spent, no event is published.
   The test that keeps that honest compares the event log digest and the hours
   left across a submission: both unchanged.
2. **`PlayerOrderSystem` is the only thing that acts on one.** It runs inside
   the simulation at the day, after `PlayerDay`, and it does the deciding: what
   the world refuses, what the day can pay for, what waits for tomorrow.
3. **A refusal is an answer, not a silence.** `Refusal` names why - nobody
   played, the person dead, an intent with no name, longer than any day, a queue
   already full, an intent that waited past the month it was meant in - it is
   kept on the command, counted on the queue and published as an event, because
   "nothing happened" is not something a player can act on and not something
   10.07 can put in a chronicle.
4. **The day is the budget, and the queue carries the rest.** Intents are taken
   in the order they were meant until the hours run out; what is left waits. A
   person who means eight things at dawn does not do all eight at dawn.
5. **What no day can pay for is refused rather than left in.** An intent costing
   more than a whole day would otherwise sit at the head of the queue forever
   and block everything meant after it.
6. **The queue is world state.** It is a component, it is in the state digest
   and it is in the snapshot: what a person means is part of the world, not of
   the program that ran it.

### Consequences

Replay is now a test rather than an argument. A recording keeps every submission
with the tick it was made on - including the ones the door turned away, because
a stream that keeps only what was accepted is a summary of the input and not the
input. Replayed blind into a fresh world of the same seed, all 1200 intents got
the same verdict at the door, 789 were taken and 21 refused as in the first life,
the queue held the same intents in the same slots, and the event log was the same
log.

The cost is that nothing can take a shortcut. 10.05 hangs the effect of each kind
inside this system's loop, and every one of them has to go through the system
that already owns that part of the world - work through 06.01, eating through
04.04, moving through the region graph - rather than writing what it wants
directly. That is more work per verb and it is the whole reason the phase can
promise a replayable life.

The rule to hold to: if a thing the player does cannot be expressed as a command
applied by the system that owns that change, it does not belong in this project.
A player who can write to the world from outside it cannot be replayed, and a
world that cannot be replayed is not this project.

## ADR-0086: A verb the player has is a call into somebody else's module

### Context

10.04 gave intent a shape and put a system inside the simulation in charge of
acting on it, and deliberately left the acting empty: an intent cost its hours
and changed nothing. This is the task that fills it in, and it is the task where
the architecture of the whole project is easiest to lose.

The shortest way to make a player work, eat and walk is to write the three
fields: add to the region's stock, raise the person's food, set their region.
Three lines each, no dependencies, works immediately. It also ends the project,
in two ways. The stock, the food and the region are numbers other systems own
and maintain invariants over - a person's region is a fact the coarse counts of
04.06 have to agree with, and setting it behind their back leaves a world whose
two grains disagree. And a verb that writes what it wants cannot be replayed by
re-running the simulation: it has to be replayed by re-running the verb, and the
two drift the first time the economy changes.

### Decision

A doing is a call into the module that owns that change, and nothing else.

1. **PlayerOrderSystem does not know how to do anything.** It owns the queue,
   the hours and the refusals. For the doing itself it asks an `IDoing`, and
   10.05 supplies one. A world that was never handed the verbs is exactly the
   world 10.04 left, which is what keeps that task's tests honest.
2. **Each verb is somebody else's function.** Work and giving and taking go
   through `Economy::AddStock` (06.01); eating goes through `AddStock` for the
   grain and `Population::FeedPerson` (04.04) for the meal; resting goes through
   `Population::RestPerson`, into the `Rest` field 04.04 had reserved for Phase
   10 since it was written; walking goes through `Population::MovePerson`.
3. **`MovePerson` is new, and it belongs to 04.06 rather than here.** Moving a
   person between regions means the coarse counts of both have to be made to
   agree with the persons in them again, so it reconciles both, refuses any
   region the world is not simulating person by person, and publishes what
   happened. The player walks through it like anything else that ever moves a
   person will.
4. **A day of work costs the body.** It burns food and spends rest through
   04.04, which is what makes eating and resting worth doing at all. The yearly
   ration still tops everybody up once a year: a hungry day inside a fed year is
   levelled out there, and that is the yearly system doing its job rather than
   this one being undone.
5. **Refusal comes before payment.** Every doing is asked whether the world
   allows it BEFORE a single hour is spent, so an attempt the world turns down -
   nothing to eat, nobody there, too far to walk - costs the person nothing.
6. **Everything a doing moves carries the act as its cause.** The act's own
   event id is passed down into `AddStock` and `MovePerson`, so the grain that
   moved and the walk that happened point back at the intent that caused them.
   That is the chain 10.07 walks back through every layer under the player.
7. **Speaking does nothing.** It is in the log with who it was aimed at, and
   10.06 builds what the people around the player make of them out of exactly
   that. A verb that wrote an opinion somewhere would be a dialogue tree with
   extra steps.

### Consequences

The verbs are slower to write and there are fewer places for them to be wrong.
The test that matters is not that work adds grain: it is that sixty days of a
life, recorded as a stream of intents and replayed blind into a fresh world of
the same seed, give the same state digest with the economy and the people in it,
not merely the same queue. That test passes, and it could not pass if any verb
wrote a number directly, because nothing in a replay would have re-run the verb.

The rule to hold to: if a thing the player wants to do has no owner to call, the
answer is to give that change an owner - a function in the module whose
invariants it touches - and not to write the field from here. `MovePerson` is
what that looks like: it is used by the player today and it is where anything
that ever moves a person will go.

## ADR-0087: An opinion is a reading of the log, not a number a menu moves

### Context

Every game that has ever had a reputation system has built it the same way: a
dialogue tree with a number hanging off it. The player picks line 2 rather than
line 1, somebody's approval goes up four, and what the world thinks of them is a
record of which buttons were pressed. It is easy, it is what players expect, and
it is a different project from this one - it makes the opinion the primary thing
and the world a decoration on it.

Nine phases have built the other order: the world is what happened, and the
event log is what it remembers. 10.04 and 10.05 put every act of the player in
that log with who did it, what kind it was and who it was aimed at.

### Decision

An opinion is read out of the log, and out of nothing else.

1. **RegardSystem walks the acts of the day.** It looks at the tail of the log -
   the events of this tick - and stops there. It never reads the life, it never
   reads a menu, and there is no dialogue anywhere in the module.
2. **Only what was done to somebody counts.** Working and eating are nobody
   else's business; speaking, giving and taking are aimed at a person and are
   what make one. A refused doing is not an act at all, so nobody saw it: the
   world has no opinion about what the player tried.
3. **The standing of 05.02 enters in one place and one way.** An opinion is
   worth what its holder is worth, so the repute of the player at large is the
   opinions weighted by the RANK 05.02 gives whoever holds them. The head of a
   house thinking well of you counts for more than a field hand doing the same,
   and the unranked - the bound, the young - still count for something rather
   than nothing.
4. **It is a read of 05.02 and never a write to it.** The played person's own
   standing is what StandingSystem says it is, from the house, the office, the
   traits and the years, exactly as for everybody else. Nothing here touches it,
   and a test holds two worlds to that.
5. **A person is known to the handful they have dealt with.** Eight opinions at
   once; when a ninth person is dealt with, the faintest and oldest of them is
   who stops thinking about the player. Nobody is remembered by a region.
6. **The world forgets.** An opinion drifts back towards nothing at a rate a
   year can be measured in, stopping at nothing rather than souring into a
   grudge. A kindness done once is not a claim on somebody for ever.

### Consequences

What the world makes of the player cannot be authored, only earned, and the test
that shows the design is doing something is not that a gift raises a number. It
is two worlds of one seed doing the same gift and the same theft to the same two
people the other way round: the deeds are worth exactly the same to the people
who received them, and the repute comes out +35 against -63, because the place
hears the higher-ranked one louder.

The cost is that a designer cannot hand-place a reaction, and Phase 12's
dialogue will have to be written against this rather than around it: what
somebody says to the player is a function of an opinion earned in the log, and
if the writing wants a scene the world has not earned, the answer is to give the
player a way to earn it rather than to set the number.

The rule to hold to: nothing writes an opinion except a reading of what is in
the log. The moment something sets one directly, the number stops being a fact
about the world and the whole of 10.06 becomes decoration.

## ADR-0088: The chronicle of a life is the chronicle of a world with one person in it

### Context

Six phases have each ended with a chronicle task, and each of them said the same
thing one layer higher: the event log holds everything, which is exactly what
nobody remembers, so a chronicle is the small part a century keeps. 10.07 is the
seventh and the last one before the gate, and the layer it sits on is a person
rather than a region, a polity or a war.

That difference is the whole question. A player's log is denser than a
century's - a life at the day rather than a world at the year - and the
temptation is to keep all of it, because it is the player's and the player is
interested. That way lies a ledger of every meal.

### Decision

1. **What is kept is what touched somebody else.** Giving, taking and walking
   are records; working, eating, resting and waiting are not, by default. It is
   the same line 10.06 draws for opinions and it is drawn for the same reason:
   those are the acts the world has any view about. `RecordSmallDoings` turns
   the rest on for a session that wants every hour, and the default is off.
2. **A refusal is not history.** What the world would not let somebody do is
   worth having while playing and is not what a life was, so `RecordRefusals` is
   off by default too. It is still an event, and it still has a sentence.
3. **Every event has a sentence whether it was kept or not**, stamped with the
   year in the same hand as every layer below, falling through to the works text
   (and so to the military, and so to the person) or, in a lighter world, to the
   economy text. The describer of the topmost layer speaks for every layer under
   it - ADR-0080's rule, one layer further up.
4. **The why runs the other way and leaves the player behind.** 10.05 passes the
   act's own event as the cause of everything a doing moves, so the grain that
   left a house points at the giving. Walk further and the chain is the world's:
   a famine that took their family is the drought of 03.05 through the stores of
   04.04 through the granary of 09.02 that fell in and was never rebuilt.
5. **`ExportLife` is the whole of it in one call**: who they are in the world's
   own naming, what they did in order, who knows them and what those people make
   of them, and the why of the last thing that happened because of them.

### Consequences

A life can be read without the program that ran it, which is what a chronicle is
for, and the text is frozen like every other text in the project: twenty days of
this life are 12 records and the digest 3727cebce1fc782c on every compiler that
runs it.

The deeper consequence is the one worth stating plainly. The player's chronicle
is not a special document; it is the world's chronicle with one person's acts in
it, written by the same describer, kept in the same records, walked by the same
why. Phase 12's dialogue and Phase 17's causal graph both read this, and neither
needs to know that a player exists.

The rule to hold to: if the player's chronicle ever needs a mechanism the other
six do not have, the thing to suspect is the mechanism, not the six.

## ADR-0089: A played life is an input, and a life owns its records

### Context

Phase 10 spent seven tasks arguing that a played life is a life of this world and
not a second simulation beside it. The gate is where that stopped being an
argument, and it took six runs to get right - not because the simulation was
wrong, but because each run showed that the shape of the claim was not yet
stated properly.

The first run passed. Reading its numbers rather than its verdict showed that
the person taken was forty years old, died in year ten of the forty, and that
three quarters of the gate was intents refused on behalf of a corpse. A green
test measuring almost nothing.

The second, meant to fix that by taking somebody under twenty-five, lost them in
year THREE. A younger person dying sooner is backwards for mortality, and that
was the tell: they were not dying. The crossings of 04.06 send unmarried adults
of sixteen to forty out of a crowded region and turn them into counts, and the
played person is exactly that profile. A played life was ending with nobody
dead.

### Decision

1. **A person can be held in the fine grain, and 04.06 owns that.** `PersonHeld`
   and `HoldPerson` / `FreePerson` / `IsHeld` live where the crossings live. The
   crossings skip a held person; with nobody held the code is inert, so nothing
   nine phases froze can move. `TakePlayer` holds their person when it is given
   the LOD types, and lets them go on release.
2. **A world with mortality in it is played across several lives.** A bound
   person of a crowded region at 256 does not live forty years, and a gate that
   pretended otherwise would be measuring a world that does not exist. The gate
   plays the forty years across as many people as the world's mortality demands:
   a life ends, the mark comes off, another of this world's people is taken up.
3. **The input to a replay is the seed, the takings, and the intents.** Taking
   somebody up is the other thing the outside world does, so it is recorded with
   its tick and reproduced. That is a stronger claim than the intents alone ever
   made: a replay that rebuilt a different world would have nobody to hand over
   at the right moment, and the gate would fail on the taking rather than on a
   digest.
4. **A life owns its records and they end with it.** The mark, the queue, the
   day, the start and the opinions all go when the life does - `EndOrders`,
   `EndStart`, `EndHours`, `EndRegard` - while what the person DID stays in the
   world's history, which is where it belongs and what the chronicle reads. The
   invariants that say one day and one queue to a world are right, and the way
   to satisfy them across several lives is to end each one properly rather than
   to weaken them.

### Consequences

The gate passes: forty years at 256 with every system of Phases 04 to 10
running, played across three lives out of 14400 intents and 3 takings, every
invariant of every phase checked each decade, and the whole thing replayed blind
into a fresh world of the same seed to the same state digest, the same event log
and the same life word for word.

Two consequences worth carrying forward. Anything that measures a life - days
lived, acts taken, opinions held - measures the life being lived and not the
years, so what spans a span of years is the sum over the lives in it; a caller
that wants the latter has to add them up, and the gate does. And 04.06 now has a
way to be told what it must not move, which Phase 11's colony will need for a
whole region rather than one person.

The rule to hold to: when a long test disagrees with the simulation, read its
numbers before its verdict. Five of the six runs of this gate found something,
and only one of the five was a defect in the test alone.

## ADR-0090: A colony is a fact with a date, not a setting of the run

### Context

11.03 gives a colony its work: hands on the ore seams of 02.07, the ore credited
through the stocks of 06.01, and the seam running out as it is taken.

Reading 06.02 before writing anything showed that most of that already existed.
`Production.cpp` sums the richness of a region's iron and copper deposits and
adds five per mille of it to the common stock every year. Two things it does not
do, and they turned out to be the whole task: the seam never depletes, so a
deposit is infinite; and the lift is written straight into `RegionStock::Amount`
rather than through `AddStock`, so it is in no event log, has no cause, and no
chronicle can say where the ore came from.

That left one design question: how does 06.02 know to stop reaping and
extracting on ground a colony works? Counting the ore twice - once by the year
and once by the colony's day - would be a defect of the most ordinary kind.

The first answer was a rule, `ProductionRules::MinedRegion`, on the precedent of
11.02's `NeedRules::DailyRegion`: rules are in no digest, so a rule costs no
closed phase anything. It compiled, the tests passed, and the numbers were
nonsense. The colony had seven people in it.

The rules of a system are fixed when the system is built, which is before
`Generate` runs three hundred years of pre-history. So the region had reaped
nothing since the world began. The colony was not starving in the test; it had
starved for three centuries before the test began, and the mining figures were a
reading of a corpse. The test passed on the way through.

### Decision

1. **The mark is a component, because a colony BEGINS.** `Economy::RegionMined`
   sits on a region entity and says: this ground is worked for what is under it.
   `FoundColony` puts it there, at a tick, and the world before that call is a
   world with no colony in it. A rule could not express that, because a rule has
   no date.

2. **Declared by the module that needs it, observed by the one below.** The mark
   is registered by `Economy::DeclareMined(World&)`, which only `VaelenColony`
   calls, and `ProductionSystem::ObserveMined` reads it. A world that never
   founds a colony has exactly the components it had before 11.03 existed, so no
   frozen digest of Phases 06 to 10 moves. This is ADR-0086's rule applied
   upwards: 11.01 paid for it with six phases' digests, and this is the second
   time it has been the answer.

3. **One mark, three consequences, all from the same fact.** The hands are on
   the rock and not in the fields, so: nothing is reaped there, no ore is
   extracted there by the year, and the people still eat out of the common
   stock. That last one is not an extra rule - it is what makes a colony a place
   that eats what it does not grow, and what 11.06's road exists to answer.

4. **Depletion is the colony's own component.** `DepositTaken` sits on the
   deposit entity and is declared by `VaelenColony` alone. `DepositInfo` has two
   reserved words that would have held the count; using them was rejected for
   the same reason as the rule - it is a Phase 02 VALIDATED struct, and writing
   Phase 11 state into it makes the world-generation digest a function of
   Phase 11.

5. **No `Work::Mine`, and no `Skill::Mining`.** `Buildings.cpp` raises one
   building of every kind per region per year, so a fifth `Work` would raise
   mines in every world ever made. `Skill::Count` sizes `PersonTraits`, whose
   size is asserted at sixteen bytes, so a sixth skill would move the component
   digest of every world ever made. The colony's hands use the craft 06.02
   already turns ore into tools with.

### Consequences

- A colony can be founded at any moment of a world's life, and the world before
  it is untouched. That is what 11.07 needs to put the player's start on the
  colony's ground at a date rather than at the beginning of time.
- `VaelenColony` is the tenth kernel module, above Infrastructure and below
  Player, because 10.02's start stands on ground this module owns.
- The tests measure the mine and not a famine, which cost one wrong design and
  two wrong readings to arrive at. Both are written into the tests where the
  next person will meet them.

## ADR-0091: A colony is founded bound, and holds itself

### Context

11.04 asks who holds a colony. Phase 05 already had bondage: people enter it
through debt, birth, capture or a promotion, leave it through manumission,
flight, a holder's death or their own, and a holder takes no more than
`BondageRules::MaxHeldPerHolder` - twelve. None of that was ever tested at the
density this phase needs, because Phase 05 never had a colony.

So the task began with a measurement rather than a design
(`Tests/Colony/Test_Holders.cpp`). Thirty years on the busiest region of AELVOR
128 gave:

    1484 alive, 114 bound (7.7%), 114 held by a person, 0 by the region,
    48 elites (capacity 576), fullest holder 3

Two things fell out of it, and both contradicted what the task had assumed.

**Bondage cannot grow into a colony.** At 05.04's own rates - debt at fifteen
per mille a year against manumission at twenty-five and flight at eight -
a region settles near a twelfth of its people bound. Waiting longer does not
help; that is an equilibrium, not a ramp. A colony where nearly everybody is
bound has to be FOUNDED that way, which is what the fiction always said: the
people are sent there.

**And the holders were never the limit until then.** Forty-eight elites held a
hundred and fourteen people between them, a fifth of what they could carry.
They become the limit only once a colony is founded: a thousand hands want
eighty-four holders at twelve each, and the region has forty-eight.

Reading how the shortfall is handled found a defect. `Bondage.cpp` chose a
holder with `NextHolder()`, which returns 0 when every elite is full, and then:

    const uint32 Holder = NextHolder();
    if (Holder == 0) { break; }

At capacity the loop stopped. People the region's strata said were bound were
silently left free, so `RegionStrata` and the actual bonds disagreed and nothing
said so. It had never shown because no test had ever filled the elite.

### Decision

1. **The region is the holder of last resort.** `BondState::Holder == 0` has
   meant "the region itself" since 05.04, and the enslaved whose holder dies
   already pass through `NextHolder()` and can land there. The promotion path
   now uses it too instead of breaking. This fixes the silent shortfall for
   every world, not only for colonies, and it is what a colony IS: one holding
   with hundreds in it rather than a place of masters with twelve each.

2. **Two missing verbs, in the module that owns each.** `Society::BindPerson`
   binds somebody the way the system binds one itself, and
   `Society::FoundOrganization` makes an organisation the way the yearly system
   makes one. Both existed only as private lambdas inside their systems. A
   founding needs them, and nothing outside a layer may write that layer's
   state, so they belong there rather than in whoever founds a colony.

3. **`OrganizationKind::Overseers`, and why an enum was safe here when it was
   not in 11.03.** 11.03 rejected `Work::Mine` because `Buildings.cpp` iterates
   `K < WorkCount` and raises one building of every kind per region per year, so
   a fifth kind would appear in every world ever made. `OrganizationKind` is not
   like that: `Organizations.cpp` founds each kind BY NAME, never by walking the
   enum, and `OrganizationKind::Count` sizes only `OrganizationStats::PerKind`,
   which is a stats struct and in no digest. **The danger is not adding a value
   to an enum. It is adding a value to an enum that something iterates to create
   things.** Check the loops before the layout.

4. **Standing was not flattened; it was removed.** 05.02 takes the rank off
   anybody bound and leaves them out of the ranking. So in a colony standing
   very nearly ceases to exist: nine hundred and thirty-five people were ranked
   on that ground, and eighty-two are, with nobody dead. Among the free few
   nothing moved at all - a hundred and ninety-eight per mille stood above the
   common run before, a hundred and ninety-five after. "Everybody's rank is the
   same work" is literally true in this model, and the first version of the test
   passed for the wrong reason by checking only that fewer people stood high.

5. **An overseer who falls into bondage keeps their seat, and the test says so.**
   `OrganizationSystem::ObserveBonds` keeps the bound out of the candidates, so
   nobody is ever SET to hold a colony that holds them. But 05.01 does not take
   a seat back when a sitting member is bound later, and three years on two of
   twenty-four overseers were held. Whether an institution should unseat a bound
   member is a Phase 05 question and not a colony's, so the test measures the
   drift instead of asserting it away.

### Consequences

- `BindColony` binds the colony's commoners to the colony itself, sparing the
  elite so that somebody is left to be an overseer at all. Every bond goes
  through `Society::BindPerson`, so 05.04 owns every bond in the world as it
  always has and the colony owns none of them.
- Once founded, a colony is not a special case: five years of 05.04 took eleven
  hundred and fifteen bound down to nine hundred and eleven through manumission
  and flight, exactly as it would anywhere else.
- The overseers outlive the people in them, because the yearly system re-seats
  them as it re-seats a guild. That is what makes them an institution rather
  than a list of owners.

## ADR-0092: The bond does not shorten a life; the ground does

### Context

Phase 10's gate read that a bound person dies young, and 11.05 asks whether that
is the world being harsh or the model being wrong.

Reading the code before measuring anything said there is no mechanism: `LifeRules`
has no bondage term and mortality is by age band alone; death by health has a
single path (`Needs.cpp`, health reaching zero) and health falls only from hunger
and plague; and being fed last is a question of family (`HouseAt`) rather than of
the bond. So the expected answer was "the ground, not the bond".

Getting a measurement that could say otherwise took three attempts, and each
wrong one passed:

1. Everybody who died bound against everybody who died free: the free died at
   twenty-three and the bound at sixty-five. Not about bondage at all - the free
   group was full of children born over the sixty years and the bound were the
   founding adults.
2. Restricted to the founding cohort: sixty-five against forty-seven, still
   confounded, because `BindColony` spares the under-twelves and the free were
   again the young.
3. Restricted to one age band as well: **the free group was empty.** In a colony
   there is no free control of working age, by construction - every adult but
   the elite is bound. The control had to come from an ordinary region.

### Decision

1. **The comparison is made in an ordinary region, one cohort, one age band.**
   AELVOR's busiest region settles near a twelfth of its people bound by 05.04's
   own rates, which gives both groups on one ground. Twenty to forty at the
   taking, followed for seventy years:

       61 of the bound died at a mean age of 66, 396 of the free at 65

   One year. **The bond itself does not shorten a life**, and the Phase 10
   reading was about the region. The test says so and refuses a wider tolerance:
   if it ever fails there is a mechanism nobody documented, and finding it is
   the task.

2. **What a colony costs is measured against the same ground left to farm.** One
   seed, one region, the founding the only difference, sixty years, both fed the
   same endowment far past what they can eat:

       farming: 1460 people became 1497 (natural 1355, famine 0, starved 0)
       mining:  1460 people became   39 (natural 1007, famine 0, starved 1941)

   Not famine, which needs a drought, and not plague. Starvation, with a full
   granary behind them until the year there is not one.

3. **A colony does not weaken; it holds and then falls.** Both worlds drain the
   endowment at the same rate. The farm lives because it REAPS when the pile
   runs out. The colony reaps nothing (11.03), so it is fine while the store
   covers the year's need and collapses inside a decade of the year it stops:

       year 10   20     30     40     50    60
       farm    1430  1432   1447   1481   1488  1497
       mine    1430  1432   1447   1481    333    39

   An endowment only moves the date. That is the number 11.06's road has to
   beat, and it is why the answer is a continuous inflow rather than a bigger
   pile.

### Consequences

- 11.05 changes no kernel file. It is measurement, and the finding is that the
  model was right: nothing needed fixing in 04.04, 05.04 or 06.02.
- A method note that belongs to 11.01 and cost this task an hour:
  `LodRules::Held` is a rule fixed when the system is built, so it holds a region
  through the whole of `Generate`'s pre-history. With the full economy running,
  three hundred years of that EMPTIES the region - 1460 people in a world without
  the hold, none in the world with it. The dated way is `RequestDetail` (04.06),
  which protects a region from demotion exactly as the rule does
  (`Lod.cpp:170`) but from the tick it is asked. This is ADR-0090's lesson again:
  a colony is a fact with a date, and so is the grain it is simulated at.

## ADR-0093: A colony feeds itself, because the world has nothing to send it

### Context

11.06 asks what a colony sends out, what it needs in, and what the roads of
09.04 carry. The number to beat came from 11.05: a colony with a one-off
endowment held while the store covered the year's need and then collapsed -
1460 people became 39 in sixty years, by 1941 deaths from starvation.

The arithmetic said roads could just about do it. 06.04 carries at most
`CarryMax` (1000) units of a good a year on a route, times
`(1000 + RouteEase::CarryPerMille) / 1000`, and 09.04's best road is grade 3 at
`EasePerGrade` 200, so 1600. A region holds `MaxRoutesPerRegion` (4). That is
6400 a year against the 5924 a colony of 1481 eats. It fits, and only just.

The measurement said the arithmetic was irrelevant. Post-founding inflow over
sixty years, two colonies of different sizes on one seed:

    region 75: 1432 people eat 5728 a year, the roads brought 115 a year
    region 79:  421 people eat 1684 a year, the roads brought  32 a year

Two per cent of what they eat, and the SMALL colony died as surely as the big
one. (A first reading of mine said 3150 a year and fifty-five per cent of need.
That was wrong: it divided a CUMULATIVE total which included the twenty years
before the founding, when the region traded normally as an ordinary farm.)

`Trade.cpp` says why. A carry is bounded by
`(HeldS - WantS) * CarryPerMille / 1000 * Easier / 1000` - a quarter of the
seller's surplus ABOVE ITS OWN WANT. AELVOR's regions live at subsistence:
06.02 has them eat what they reap and spoil a tenth of what is left every year.
Nobody has spare food, so there is nothing to send, and `CarryMax` and the roads
never bind at all.

### Decision

1. **The rule of 11.03 was too absolute, and that was the whole cause.**
   `RegionMined` zeroed a mined region's harvest entirely, on the ground that
   its hands are on the rock. But 11.04 established that the elite are spared
   and the founding binds only those twelve and over, so a colony has free
   people who are not on the rock. `ProductionSystem::ObserveBonds` now excludes
   the BOUND from the farm workers and from what the fields yield, and leaves
   everybody else in them. A colony reaps in proportion to the people it has
   left over.

2. **This supersedes the collapse of ADR-0092 point 2 and 3.** Those figures
   were true of the model as 11.03 first wrote it, and 11.05 reported them
   faithfully. With the corrected rule the same experiment gives 1460 people
   becoming 1497 on both grounds, with zero deaths from starvation on either.
   What a colony costs its people is a store that drains faster - 1939 grain
   left against 9196 after sixty years - and not a graveyard.

3. **Nothing in 06.04 or 09.04 was touched.** Raising `CarryMax`,
   `MaxRoutesPerRegion` or the road ease would have made a colony survive and
   would have changed every world since Phase 06 to paper over a rule of Phase
   11. The measurement says they are not the binding constraint, so they stay
   as they are.

### Consequences

- **A colony is fed by the colony.** The roads bring about a fiftieth of what it
  eats, and that is not a defect of the roads: it is what a subsistence world
  has to spare. If a colony is ever to be supplied from outside, the mechanism
  will have to be a polity provisioning it through the dues of 07.02 - grain
  taken by authority rather than carried by a price gap - and that is a Phase 12
  question, not this one.
- **A colony founded bound does not stay bound.** Measured over sixty years,
  1116 bound at the founding become 795, 578, 426, 335, 291 and 247 as 05.04's
  manumission and flight take them out, with a persistent core because bondage
  unredeemed hardens into slavery. So a penal colony nobody resupplies with
  convicts becomes, in two generations, a free mining town that farms again -
  which is why its harvest recovers, and it is the world saying something the
  design never wrote down.

## ADR-0094: A thing the world does with no event behind it cannot be remembered

### Context

11.07 puts the colony in the chronicle. Every layer of this project already has
the same shape for that - a listener that turns the events which matter into
records, and a sentence for each - so the task looked like filling in a template.

It was not, because `FoundColony` published nothing at all. A colony began, the
mined mark went on the region, a thousand people were bound, and the world's
history said nothing had happened. There was no record to write because there
was no event to write it from.

Writing the chronicle then found a second thing. `MiningRules::Region` named the
colony's region as a rule fixed when the system is built - and a test harness
cannot know that region before it builds its systems, because the colony is
founded afterwards. The chronicle test lifted an existing harness, mined nothing
at all, and the reason was that the rule was zero.

That is the third time in one phase. ADR-0090 removed
`ProductionRules::MinedRegion` for it; 11.05 found `LodRules::Held` applies
through the whole of `Generate`'s pre-history and empties the region; and now
this.

### Decision

1. **`ColonyFoundedEvent`, published by `FoundColony`.** Nothing the world does
   is remembered unless an event says it happened. This is the rule the phase
   should have followed from 11.03 and did not.

2. **`MiningSystem` reads the colony pool, not a rule.** There is no
   `MiningRules::Region` any more. The system mines every colony the world holds,
   which is also why it can run daily at all: the colony pool has as many
   entries as there are colonies, and walking it is walking one entity.

3. **The general rule, stated once for the phase.** *A rule is fixed when the
   system is built; a fact has a date. Anything that begins during a world's
   life must be a component, and the system must find it by looking.* Three
   tasks paid for this separately before it was written down.

4. **A colony's binding is one record, not a thousand.** `DescribeBinding` takes
   the count and says it in one sentence. A thousand records saying the same
   thing on the same day is a ledger, not a chronicle - and the same judgement
   governs the lifts, which are told by the year rather than by the day.

### Consequences

- The frozen mining digest moves from `a7b00a23e7072fbb` to `e737ebcd1c65e709`,
  for two reasons that are both real changes to the world it measures: 11.06
  stopped a mined region reaping nothing, and this task replaced
  `LodRules::Held` with `RequestDetail` in the harnesses so the hold begins at a
  tick. Neither is a determinism failure - the same seed still gives the same
  colony twice over, and the tests that prove it are the ones above the frozen
  check.
- The sentences read as the rest of the chronicle does: "the ground of Einzu was
  given over to what lay under it", "464 people were bound to the colony of
  Einzu", "a seam under Einzu gave up the last of itself".

## ADR-0095: A gate has to be full of what it claims to measure

### Context

11.08 is the Phase 11 gate: a century at 256 with the mining colony at full
detail while the world around it keeps the year, every invariant of every phase
each decade, frozen digests, and a life played inside the colony replayed into a
fresh world of the same seed.

It passed on its second run and the first one is the reason this ADR exists.

The first run founded the colony on the ore-richest ground in AELVOR, which is
what a mining colony wants, and the ground turned out to hold thirty-six people.
Then the played life ended in its third year and `BeginEnslaved` found nobody
else, because the start insists on somebody bound and a colony's bondage erodes -
11.06 measured 1116 becoming 247 over sixty years, and at that size it reaches
zero inside a decade. So the gate lived ninety of its hundred years with nobody
played, recorded 1081 intents where there should have been 14400, and reported
a century in a second and a half.

It also failed, and the failure was the useful part. The replay's state digest
differed. The recording released the mark when the life ended -
`ReleasePlayer`, `EndOrders`, `EndStart`, `EndHours`, `EndRegard` - and the
replay only does that when it takes somebody NEXT. With one life and no
successor, the recording ended with the mark off a dead person and the replay
ended with it on. A gate that had been full would never have shown it.

### Decision

1. **The colony is founded where the ore AND the people are.** The ranking is
   the product of a region's ore and its population, so ground that is only rich
   or only peopled falls away. That is also truer to the fiction: a colony is
   sent somewhere worth sending people to.

2. **The start takes whoever the colony has, bound or not.** A colony's bondage
   erodes by 05.04's own rates, so a gate that insists on a bound life measures
   nothing after the first one. What the gate is about is the colony, and 11.04
   already proves the binding separately.

3. **The colony's founding is an INPUT to the world, like a taking.** The replay
   founds it on the same tick, binds the same number and raises the same
   overseers, and the gate checks the tick as well as the counts. A world that
   is not given the same inputs is not the same world, and the state digest
   would say so - which is exactly how the first run's defect surfaced.

4. **ADR-0089's rule again, stated for gates in general.** *A gate has to be
   full of what it claims to measure.* The Phase 10 gate spent three quarters of
   itself refusing intents at a corpse; this one spent nine tenths with nobody
   played. Both passed their first run. The number to look at is not the verdict
   but the volume: 14400 intents over forty years is what a life lived a day at
   a time comes to, and anything less is a gate measuring its own silence.

### Consequences

- The gate now founds a colony of 4007 bound people on 4463 units of seam, works
  every seam out inside the century, lives five lives and 14400 intents through
  it, and replays all of them into the same world - same state digest, same
  event log, same mining digest, and no intent answered differently.
- Frozen at `half=78997c9e11f162a5`, `end=2862e6e238d2c1fd`,
  `log=abb41cdb4e931d65`, in 27.8 seconds.

## ADR-0096: The verbs that create are already done; the verbs that only move are done by nobody

### Context

12.01 gives a person nobody is playing a way to act. The roadmap called it
"promoting the verbs of 10.05 from the player's own to anybody's", and reading
the code before writing showed there is nothing to promote: `Doings::Do` takes a
PERSON INDEX, not "the player". The verbs have always been anybody's. What is
the player's is the plumbing - the intent queue and `PlayerOrderSystem`.

So the question became which verbs an unplayed person may use, and that is not a
matter of taste. Reading what each one does splits them exactly, and the line is
CONSERVATION:

    Wait       nothing at all
    Speak      nothing moves; the act is in the log and 10.06 reads it
    Give/Take  move goods between two houses
    Move       MovePerson, which reconciles both grains
    Work       AddStock(+WorkYield) - CREATES goods
    Eat        AddStock(-EatGrain) and FeedPerson - DESTROYS and fills
    Rest       RestPerson - fills

06.02 already harvests for everybody and 04.04 already rations everybody, in
aggregate. A person who works and eats individually is therefore counted twice.
For one played person that is negligible, and 10.03 accepted it knowingly; for a
region of unplayed people the economy would roughly double.

### Decision

1. **An unplayed person does only what nothing else does.** Speak and give -
   conservative acts, through `Doings::Do` unchanged, so a played person and an
   unplayed one do the same thing by the same call. That is the whole claim of
   the task and it needed no new verb.

2. **`RegionLively` is a component, not a rule.** Ground becomes lively at a
   tick. ADR-0090, and Phase 11 paid for it three times.

3. **What decides the intent is deliberately thin, and says so in place.**
   Somebody nearby, something to spare. 12.02 is the answer to that question - a
   person acts on what they BELIEVE - and nobody believes anything yet.

### Consequences

- Conservation is measured against a CONTROL rather than against zero, and the
  reason is a finding of its own. The first version compared a region's holdings
  before and after a hundred days of acts and found grain up by 168. It was not
  the giving: the same world with `ActPerMille = 0` - nobody acting at all -
  moves by exactly the same 168, because the window began on a year boundary and
  the tick AFTER a whole year is the tick the yearly systems run in. Comparing
  lively ground against still ground isolates what the acts did, and by that
  measure 22228 acts (15918 spoken, 6310 given) leave the region holding exactly
  what stillness leaves.
- That drift is left named rather than explained: something moves a region's
  holdings across a year boundary without publishing an event that the window
  catches. It is not this task's, it is not the acts, and it is worth finding
  before 12.08 freezes anything.
- A world with no lively ground writes a history of its own, and two such worlds
  of one seed are identical - the 10.03 claim, one layer up.

## ADR-0097: A reputation is what a world gets when something reaches a third person

### Context

12.02 was planned as "knowledge as a filter over the world's log", on the
finding that thirty-one files read `Log().All()` and two of them decide what
somebody believes. That premise was wrong, and correcting it is most of this
decision.

`Regard.cpp` does open its loop on `W.Log().All()`, and reading only that line is
what produced the claim. Reading the lines under it says the opposite: it walks
back to the CURRENT TICK and stops - its own comment is "it never reads the
life" - takes only acts AIMED at somebody, skipping work and eating as "nobody
else's business", and records the opinion on the person who was acted upon. That
is already exactly what reached them. `Decisions.cpp` reads droughts inside a
memory window, which is a council knowing the weather in its own region.

So nobody in AELVOR was ever omniscient. Two other things were true instead, and
neither had been noticed:

- An opinion exists only ABOUT THE PLAYED PERSON. `PlayerRegard` is a component
  on the played person, so in a world with nobody played, nobody thinks anything
  of anybody. 12.01 had just given unplayed people acts of their own.
- Nothing TRAVELS. An opinion moves only between the two people involved.
  Nobody in twelve phases has ever learned anything they did not suffer.

### Decision

1. **`PersonRepute` on any person, the mirror of 10.06's `PlayerRegard`.** The
   same 224 bytes, the same handful of slots, the same weights, so one world has
   one scale. What 10.06 does for the one played person, this does for everybody.

2. **Hearsay, and it is the whole of the task.** A speaking may carry the
   speaker's opinion of a THIRD person to whoever they spoke to, at
   `HeardPerMille` (300) of its weight. A story is worth less than a scar, and
   it is worth something. `HeardOfEvent` puts it in the log, so the chronicle of
   12.07 can say who told whom what.

3. **The same rule 10.06 set is kept, not relaxed.** This system reads the
   current tick's acts and stops. Nothing here reads a history, and nobody
   learns anything except by being there or being told.

### Consequences

- Sixty days of one lively region: 1137 people thought of, 9063 opinions, of
  which 7343 are first hand and **1720 reached somebody it never happened to**.
  2821 tellings. Two worlds of one seed think exactly the same things of exactly
  the same people.
- The measured repute has no negative side yet - `worst 0` - because 12.01's
  unplayed people only speak and give. Taking is the verb that makes an enemy
  and nobody does it. That is a limit of 12.01 and not of this task, and 12.06
  is where a repute has to cost somebody something.
- Reading one line and not the ten under it cost a wrong premise in a pushed
  roadmap. The correction is its own commit, because the premise was public.

## ADR-0098: A document is hearsay that outlives the teller

### Context

12.02 let one person tell another what they think of a third, and that story
lives exactly as long as the people in it: a teller can be asked again, can
change their mind, and dies. 12.03 asks what a world gets when a claim stops
depending on anybody being alive to make it.

### Decision

1. **A document freezes an opinion, with a date on it.** `DocumentInfo` holds
   who wrote it, who it speaks of, and `Says` - the regard its writer held AT
   THE MOMENT OF WRITING. Nothing ever changes `Says`. The writer's own opinion
   moves on, and the two come apart; that gap is the whole of what makes a
   document interesting rather than a slower kind of truth.

2. **A copy says exactly what the original said.** `Writer`, `About`, `Says` and
   the date are carried over unchanged, and only `From` and the holder differ.
   A copy is dangerous rather than harmless precisely because it does not know
   it has aged.

3. **Reading is worth less than being told, which is worth less than being
   there.** `ReadPerMille` (200) against 12.02's `HeardPerMille` (300) against a
   first-hand act at full weight. A page cannot be asked what it meant.

4. **A lost document says nothing and cannot be copied.** What it said is gone;
   what it caused is not, because what it caused is in the event log.

### Consequences

- Measured: a writer set down 15 about somebody; a year of the region living
  later they think 0, and the page still says 15. And a person who had never met
  the subject at all came to think 3 of them off that page.
- **A limit worth naming: `MostThoughtOf` is eight.** Only eight people can hold
  an opinion about anybody at once, oldest evicted. A reputation bounded at
  eight holders is a village's reputation and not a polity's, and 12.05 - repute
  travelling on the roads of 09.04 - is where that bound has to be faced rather
  than raised quietly.
- One defect of mine, and the reason `SlotFor` is public now: `ReadDocument`
  carried its own copy of the slot logic and left out the eviction, so a page
  could teach nobody anything about a person already thought of by the full
  handful - which is every person worth writing about. Two copies of one rule is
  one copy too many; there is one now, and both callers use it.

## ADR-0099: A map is the one document the world can check

### Context

12.03's documents say what their writer thought of somebody, and nothing can
ever say whether that was right - an opinion has no truth to be measured
against. 12.04 asks what changes when a claim is about GROUND, because the
ground is right there: 02.06 built the region graph and `AreAdjacent` answers
for anybody.

### Decision

1. **`PersonGround` holds what somebody can name, and which of it they stood
   on.** A person who read a region off a page names it exactly as readily as
   one who walked there; the single bit that separates them is `Walked`, and
   nothing in the world tells the person which of their own beliefs it is.

2. **A map of walked ground is true by construction, and only of CONSECUTIVE
   crossings.** A first version claimed every pair of walked regions, which
   would have drawn a false map out of an honest walk - going from A to C by
   way of B does not put A next to C. Known is in arrival order and the claims
   are the crossings actually made.

3. **`ForgeClaim` is the only way a map becomes wrong.** Ground does not move,
   so a map is not wrong because the world changed under it; it is wrong because
   somebody drew it wrong. That is deliberate and it is what makes the check
   worth having.

4. **`CheckMap` is the world reading the map back.** Nothing else in twelve
   phases can be asked whether it is true.

### Consequences

- Measured: a map of ground walked from region 26 to region 2 gives one claim,
  one true, none false. One forged road later it gives two claims, one true and
  one false, and the world says exactly which. A reader who has never left their
  own region can then name both roads, and has walked neither - the only thing
  that tells them apart is a bit they cannot see.
- **A limit: walked ground pushes read ground out of a full head, and never the
  reverse.** Standing somewhere is worth more than reading about it, and a
  person holds eight regions. A world where somebody can be talked out of what
  they have seen is a different design and would need saying so.
- Maps do not decay and nothing forges them by itself. Both are for whoever
  needs a forger - 12.06's consequences, or a scenario - and this task gives the
  verb rather than the motive.

## ADR-0100: A name is held by a place, and it is built on deeds rather than on opinions

### Context

ADR-0098 named the limit 12.05 was to face rather than raise quietly:
`MostThoughtOf` is eight, so a person is thought of by eight people at a time
and the ninth evicts the oldest. A reputation bounded at eight holders is a
village's reputation and not a polity's.

### Decision

1. **The bound was the wrong SHAPE, not the wrong number.** Nobody in the next
   valley has a private opinion of a man they will never meet; the place has
   one. So fame lives on the region - `RegionNames`, a handful of names to a
   region - and not on the person. Raising eight to eighty would have been the
   same model with a bigger cache.

2. **A name travels on the routes of 06.04 and not across the map.** A place
   with no road to you has never heard of you, however close it stands. That is
   the whole of why this task waits for trade rather than reading the region
   graph, and it is measured: with roads, eight places carry sixty-four names,
   fifty-six of them from abroad, reaching three roads out; with the identical
   world and the trade system left out, one place carries eight names and
   nothing is abroad at all.

3. **A name is built on what somebody has DONE, counted over a whole life.**
   Not on 12.02's `Repute`, and this is the part that had to be measured before
   it could be decided. `PersonRepute::Repute` is the average of the opinions of
   the at most eight people who last dealt with them - and in a region of 1428
   people, 1163 of them sit between 80 and 93 on a scale to a thousand, the
   loudest three tied exactly. The eight names that average picks out are
   COMPLETELY DIFFERENT every year (p715, p1100 → p168, p321 → p782, p965 →
   p514, p1175): a lottery, carried perfectly. `Kindnesses` and `Wrongs` are
   cumulative and never evicted, and they persist - the same two people led the
   region in four consecutive years. Fame is `Kindnesses * PerDeed +
   Wrongs * PerWrong`, clamped to a life's worth.

4. **There is no gradual fading, and the omission is measured rather than
   assumed.** A `ForgetPerYear` decay was written and could not be reached.
   While the people who earned a name are alive it is re-told across every road
   every year at a value that only grows, so no place is ever a year out of
   date. Shutting all sixty-eight roads by hand left MORE names abroad two years
   later, not fewer, because 06.04 re-opens a route the year the prices warrant
   it. Killing every person in the source region did not do it either: the
   level-of-detail bridge materialises the region's people again from the
   aggregate, and the loudness recovered from 13191 to 31851. Set to 0, 120 and
   400 a year, the rule gave byte-identical name lists and byte-identical
   loudness, differing only in a timestamp. **A rule that changes nothing is not
   shipped.** What a place does is keep the loudest handful it is told about;
   a name it is no longer told, it stops saying, that year.

5. **What a place keeps across years is `Since` - the year it first heard the
   name.** That is what its memory amounts to, and unlike the decay it is live
   data.

### Consequences

- The eight-holder bound of 12.02 is unchanged and no longer binding on
  reputation: an opinion is still held by the eight people who last dealt with
  somebody, and a name is now held by every place that has heard it.
- **A limit worth naming, and it is not this task's to fix: AELVOR has no
  famous people.** Every person in 12.01 acts at the same rate under the same
  rules, so the cumulative record is a tight cluster - a thousand people within
  a factor of two of each other - and which eight a place names is decided at
  the margin. Fame here is persistent and it is not yet EARNED. 12.06 is where
  a repute costs or gains somebody something, and that is what would spread the
  distribution.
- `MostNames` is eight per place and it saturates: a region trading with four
  neighbours hears up to thirty-two names and may say eight. Which is why
  dropping, not fading, is what removes a name.

## ADR-0101: A repute costs somebody something, and character is what makes it unequal

### Context

Twelve phases had produced a reputation that changed nobody's life. 12.02 gave
people opinions of each other, 12.03 wrote them down, 12.05 sent them along the
roads - and at the end of it the worst-named man in the world ate the same
dinner as the best. ADR-0100 also named the reason it could not be otherwise:
every person in 12.01 acts at the same rate under the same rules, so the
cumulative record is one tight cluster and nobody is famous for anything.

### Decision

1. **Take is restored to the verbs an unplayed person may use.** 12.01 shipped
   with Speak and Give, and ADR-0096's rule was that the verbs which CREATE or
   DESTROY are already done in aggregate while the ones that only MOVE or SAY
   are done by nobody. Take only moves. Leaving it out cost the phase its whole
   negative half: 12.02's `worst` was 0 because nobody in AELVOR had ever
   wronged anybody.

2. **Character decides who takes, not want - and this was measured, not
   preferred.** Want was tried first and does not work, for a reason worth
   writing down: **hunger in AELVOR is not individual.** 06.02 works out ONE
   ration for a whole region and 04.04 moves every person's Food by it, so
   everybody in a place is exactly as fed as everybody else. Measured: all 1428
   people of the world's best-fed region sit at 255 of 255, and a land lean
   enough to push anybody under the hunger line (HarvestPerWorker 3 instead of
   7) leaves **5 of the 1428 alive**. There is no window in which some people
   are hungry and others are not.
   `Trait::Boldness` has one. It is 04.05's, 0..255 with 128 ordinary, and
   heritable at half - 59 of the 1428 are over 200. So who is thought ill of in
   this world runs in families rather than being redrawn by the dice each
   morning.

3. **The consequence is bondage, and only bondage.** The bodies that act on an
   INDIVIDUAL in this project are few: 05.04 binds and frees. 07.02's dues are
   laid on a REGION and not on a person, so a polity cannot charge a thief more
   without inventing an axis this world does not have; that is left alone
   deliberately rather than invented. `Society::FreePerson` is added as the
   mirror of `BindPerson` for the same reason that one exists: nothing may write
   a bond from outside the layer that owns one. `BondEntry::Judgement` is a new
   reason, and `BondageStats::Entered` grew from five to six so it is counted as
   itself rather than folded into None.

4. **A place judges on what IT says, never on the person's own record.** This is
   the part worth reading twice. A place carries eight names (12.05); somebody
   whose name is not among them is not judged at all. Measured: 56 people had
   wronged somebody and **42 of them were never answered for it** - not spared
   by a clause, simply never spoken of. That is a consequence of 12.05's shape
   rather than a rule invented here, and it is what makes a reputation worth
   having or worth hiding.

5. **A name travels; a body does not.** Judgement acts only on people standing
   in the place that carries the name. Seven of the eight places in the measured
   world carry names and hold nobody at all, and they passed **0** judgements.

### Consequences

- Measured, over six years of one lively region: blind to character, 478682 acts
  and 0 takings and a worst repute of 0; seeing it, 472864 acts, 5467 takings
  and a worst repute of **-82**. With no line to cross, 0 condemned; with one,
  **15 condemned and 14 still bound**, every one of them on a name their place
  was carrying.
- **Two defects this task exposed in closed phases, both fixed where they
  live.** `Player::MeasureDoings` (10.05) searched every act linearly for every
  event in the log - fine for one played person's few hundred acts, quadratic
  for a region's half million. Sorted and binary-searched, the suite went from
  over fifty minutes to five and a half. And a first version of the hunger read
  walked the whole person pool a second time every day with a component lookup
  per entry; folded into the walk that was already there.
- A limit: bondage is the only consequence, so the world can punish and cannot
  reward beyond letting somebody out of a bond it imposed. A good name buys
  freedom and nothing else.

## ADR-0102: A chronicle of what was believed, not of what was true

### Context

Phase 12's rule is that a person acts on what they believe and the world acts
on what it has heard. 03.07 has answered "why did this happen" since Phase 03
by walking a cause chain - but every link in that chain was a FACT. 12.06 began
binding people for their names, and a bondage whose cause chain says nothing is
a punishment with no recorded reason.

### Decision

1. **A name remembers the event that brought it.** `Fame` grew from 24 bytes to
   32 to carry a `PersistentId First` - the telling that put this name in this
   place. It is state and not a local because everything a place DOES about a
   name is caused by it: `NameTravelled` is published with the previous
   telling as its cause, and `Condemned`, `Pardoned` and the bond itself are
   published with the local telling as theirs. So `History::CauseChain` - the
   same one 03.07 has always used - now walks belief rather than fact, without
   a second mechanism.

2. **`WhyBelieved` is 03.07's `Why` with the sentences of this layer.** A chain
   of length one is a place acting on what it saw; a chain of length four is a
   place acting on a rumour four valleys old. Nothing new is invented to make
   that readable - the chain is the chain.

3. **`WhatWasKnown` answers for a PLACE, not for the world.** After ADR-0100
   that is where knowing lives, so "what was known, by whom, and when" is a
   question a region answers: what it says of somebody, how many roads the
   saying crossed, and the year it first heard it.

### Consequences

- Measured: 116675 events of belief in a six-year world, every one of them with
  a sentence and none falling through to a generic line; the chronicle digest is
  identical across two runs of one seed. All 64 names any place carries can be
  asked about and answered; a person nobody has heard of returns nothing rather
  than an empty sentence.
- The chronicle reads: *"Imhugundild was bound by Edavaken for a name it spoke
  ill of (-707) where it was earned"*, and behind it *"Edavaken came to speak
  ill of (-707) Imhugundild"*. And of a name that travelled: *"Ekdu speaks ill
  of (-600) Assinek, 1 road away, and has since year 304."*
- **A limit, and it is the phase's own thesis only half proved.** Every chain
  measured is two steps long and every judgement says *where it was earned*,
  because 12.06 binds only people standing in the place that carries the name
  and **nobody in AELVOR ever moves**. `Player::Intent::Move` exists and 12.01
  does not use it. Until somebody can earn a name in one place and be standing
  in another, no one is ever judged on a rumour - the machinery for it is built,
  wired and chronicled, and the world gives it nothing to carry. That is the
  next real question this phase asks, and it is named here rather than implied
  by a passing test.

## ADR-0103: The Phase 12 gate, and the second hollow gate this project has built

### Context

ADR-0095 was written when the Phase 11 gate passed its first run on 1081 played
intents where it should have had 14400: every check it made was true, and it was
measuring a world in which the played person had died and nobody had replaced
them. The rule taken from it was **a gate has to be full of what it claims to
measure - read the volume before the verdict.**

### Decision

1. **The Phase 12 gate is the Phase 11 gate with the phase on top of it.** The
   same century at 256, the same colony founded where the ore and the people
   both are, the same forty years played a day at a time - and the colony's
   people now living lives of their own: speaking, giving, taking, thinking
   things of each other, telling each other, and the places around them coming
   to carry names that cost somebody their freedom.

2. **The volumes are asserted before the digests, out loud.** Not as a comment
   about care but as checks the gate fails: a played intent on every one of the
   14400 days, somebody taking something, somebody thought ill of, a name
   crossing a road, a name costing a freedom, and every act of belief in a
   century carrying a sentence.

3. **It happened again, and the assertions caught it.** The first run of this
   gate produced `0 taken`, `worst 0`, `0 condemned` and `13680 acts` - and
   every invariant held, because there is nothing wrong with a world in which
   nobody does anything. The cause was one missing line: the colony's ground was
   never marked lively, so `LivingSystem` had no ground to walk and the whole of
   Phase 12 measured zero inside a gate named for it. `MakeLively` is an INPUT
   now, alongside the founding and the binding, and the replay performs it on
   the same tick.

4. **Two invariants of mine were wrong and the gate was right to fail them.**
   `Opinions >= Heard` - `Heard` is a running total of everything ever told
   about somebody while `Opinions` is how many of their eight slots are in use,
   so a person told about a hundred times holds eight opinions and the hundred
   is not an error. And `Pardoned <= Condemned` - a pardon frees anybody a place
   thinks well enough of, and most bound people in a colony were bound by
   05.04's debt or 11.04's founding, so a good name buying somebody out of a
   bondage they were born into is the system working.

5. **`Colony.ColonyGate` and `Gameplay.GameplayGate` are in `run_gates.sh` now.**
   They were missing for the whole of Phases 11 and 12 - the two slowest gates
   in the project and the two covering the newest code, and the script whose own
   header explains that a local loop excluding the gates is how six phases'
   frozen digests moved at once without anybody noticing.

### Consequences

- Measured, and this is what full looks like: 14400 played intents over two
  lives; 327267 acts in all, 67769 given and 1175 taken; 6939 people thought of
  and 55074 opinions held, of which 81911 tellings; best 175, worst -92; 96
  names carried in 12 places, 88 of them from abroad and three roads at the
  furthest; 21 condemned, 1 pardoned, 10 still bound; 82647 events of belief,
  every one with a sentence.
- Replayed into a fresh world of the same seed from the founding, the binding,
  the lively ground, the takings and the intents alone: 14400 of 14400 intents
  answered identically, 2 of 2 takings, and the state digest, the event log, the
  mining digest, the fame digest, the repute digest, the judgement digest and
  the belief digest all identical.
- The gate takes 12m53s under gcc-debug, which makes it the second-slowest thing
  in the project after the Colony gate. That is the price of a gate that is full.

## ADR-0104: The view is a block of numbers, so that read-only stops being a promise

### Context

The layering rule of this project has said since Phase 00 that PRESENTATION
reads WORLD STATE and does not touch it. For thirteen phases nothing enforced
that, because nothing rendered anything: it was a rule kept by everybody
remembering it, and the first time a renderer exists it will be kept by whoever
writes the renderer remembering it too.

### Decision

1. **A `WorldView` holds no pointer, no handle, no component type and no
   reference to the world it came from.** A renderer that has one cannot reach
   the simulation even by mistake, because there is nothing in its hand to reach
   with. That is the whole design and everything else follows: the view is taken
   by value, copied, kept, compared with the previous one, and handed to another
   thread without the world caring.

2. **The compiler checks it, not a comment.** The test asserts
   `std::is_trivially_copyable<RegionView>` and `std::is_standard_layout`, so the
   day somebody adds an `EntityHandle` or a `std::string` to a view struct, the
   build stops. A rule that only a reviewer can enforce is the rule that gets
   broken in Phase 16.

3. **`TakeView` takes a `const World&`.** The signature is the promise, and the
   test proves the other half of it that a const reference cannot: sixty frames
   taken in a row leave the state digest, the event log digest and the event
   count all identical.

4. **Every source past the map and the people is optional.** A world with no
   trade gives a view that says 0 roads rather than refusing to be looked at.
   For most of this project's life the world was half-built, and a renderer that
   only works against a finished one is a renderer nobody can develop against.

### Consequences

- Measured: a world of 99 regions and 19781 people is 5600 bytes of view - the
  whole frame, everything a renderer needs. Sixty frames left 13747 events
  before and 13747 after.
- The regions come out in index order every time, never in pool order, so a
  renderer can keep its own array in step with the view's across frames.
- A region the world simulates person by person reports the heads actually
  standing there; one it does not reports the aggregate count of 04.02. A
  renderer must never draw an empty region because the simulation stopped
  tracking heads, and the view says which of the two it is.
- **This is not a snapshot and not a query.** 03.06 owns snapshots and they are
  the whole world including everything nobody can see; 03.07 answers questions.
  This is one frame's worth of what can be drawn, small enough to take sixty
  times a second.

## ADR-0105: A delta nobody can replay is a delta nobody should believe - and the padding that proved it

### Context

13.01 takes a whole view every frame. For ninety-nine regions that is 5600
bytes and nobody cares; AELVOR at 256 has thousands, and a renderer re-reading
all of them sixty times a second to find the four that moved is a poll, not a
renderer.

### Decision

1. **The delta is defined by what can be replayed from it, not by what it
   contains.** `Apply(older, Diff(older, newer))` must give `newer` back byte
   for byte, and the test checks it by digest on every single frame of a
   sixty-day run rather than once at the end. A difference that cannot rebuild
   the thing it is a difference of is a compression scheme with no decoder.

2. **An empty previous view means the whole thing, and says so.** `Whole = 1`
   rather than a delta that quietly assumes the screen already matches. A
   renderer that has drawn nothing needs everything.

3. **Ground appearing and disappearing is handled and tested, though no world
   does it yet.** The branches exist, and an untested branch is a guess.

### Consequences

- Measured: five years moved 27 of 99 regions - 1600 bytes against 5600 for the
  full frame. Sixty days cost 6792 bytes of difference against 336000 bytes of
  frames, **twenty per mille**, with 59 of the 60 days moving nothing a renderer
  can see at all.
- **The defect this task found, and it is the reason ROADMAP section 18 now
  demands three presets.** `RegionView` had eleven 32-bit fields after an
  `int64` - 52 bytes, which the compiler rounds to 56 for the struct's 8-byte
  alignment, leaving four bytes nobody ever wrote. `MeasureView` hashes these
  structs and `Diff` memcmps them, so those four bytes decided whether two
  identical regions compared equal. They happened to be zero under gcc and not
  under clang-release: the suite was green in two presets and failed in the
  third, on the same source.
  The fix is a twelfth 32-bit field to make the count even, and an assertion
  that compares the size against the SUM OF THE FIELDS rather than against a
  number I typed. `static_assert(sizeof(T) == 56)` passes happily on a padded
  struct, which is exactly how this got in.
- The rule generalises past this struct: anything hashed or memcmp'd byte-wise
  needs the field-sum assertion, not a size assertion. The kernel's component
  types get this from `PlainData`'s no-padding check; a view struct is not a
  component and got nothing until now.

## ADR-0106: How finely the world thinks and how finely it is shown are different questions

### Context

01.03's `SimLod` says how much computation a corner of the world is worth: Full,
Detailed, Aggregate, Statistic, World, on periods of 1, 4, 24, 720 and 8640
ticks. It is tempting to reuse it for drawing, because it is already there and
it already says "detail".

### Decision

1. **They are not the same question and must not share an answer.** How finely a
   place is SHOWN depends on where somebody is looking, and the world has no
   idea where anybody is looking. Measured on one world at one moment: a region
   the simulation runs person by person, nine borders from the eye, left out of
   the frame entirely; and a region the simulation only counts, directly under
   the eye and drawn finely. If those two ever agreed, `SimLod` would have been
   enough and this task would not exist.

2. **The eye is an INPUT to the view, never a property of the world.** Nothing
   in 13.03 writes anything. Two people looking from two places get two frames
   and the world stays one world - checked by state digest and log digest, not
   asserted.

3. **The graph cache belongs to the caller.** Building the region graph is a
   walk of the whole map and a frame is taken sixty times a second, so the cost
   is put where somebody can see it rather than hidden in a static - the same
   choice 10.05's `Doings` made for the same reason.

4. **Grain first, budget second.** A budget applied before the grain was worked
   out would drop near ground to keep far ground it happened to reach first.
   Within a ring the cut is by index, so a budget that bites mid-ring bites the
   same way every frame and the screen does not flicker between two regions at
   equal distance.

5. **`Unreached` is not "far away".** An island with no chain of borders to the
   eye is not a long way off, it is not reachable, and a renderer wants the
   difference.

### Consequences

- Measured: looking from region 88 with a reach of one, 3 regions drawn of 99 -
  224 bytes against 5600 for the whole world. Reach 0 through 4 gives 1, 9, 20,
  34 and 50 regions, and exactly one region is the subject at every reach.
- The frame's header counts what is in the FRAME and not what is in the world. A
  renderer showing 40000 people while drawing four regions would be lying about
  both.
- An eye looking nowhere in particular gets the whole world, which is what 13.01
  gave and stays the default. A renderer that does not care about level of
  detail never has to know this task happened.

## ADR-0107: A frame taken across a save must not tear

### Context

03.06 puts a world on disk and brings it back. 13.01 says a view is a pure
function of world state. Those two claims together have a consequence nobody had
checked: a view taken before a save and a view taken after the reload must be
the same view. If they ever differed it would mean the view reads something a
snapshot does not carry - which is the one way a renderer could show a loaded
game that never existed.

### Decision

The consequence is checked rather than assumed, in three directions:

1. **The frame survives.** The digest of a view taken before the save equals the
   digest of one taken from the reloaded world.

2. **A delta made ACROSS a save still applies.** This is the half a renderer
   actually lives on: it holds yesterday's frame, the game is saved and reloaded
   in between, and the difference from yesterday's frame to today's must still
   rebuild today's exactly. A renderer must not have to know a save happened.

3. **The eye does not move either.** A framed view is a pure function of the
   world AND the eye, so neither half may notice a save.

And the 13.01 claim again with a save in the middle: thirty frames taken from
the saved world and thirty from the loaded one leave both state digests
unmoved - and the world that has been looked at thirty times saves to **byte
for byte the same snapshot** it did before anybody looked at it.

### Consequences

- Measured: a 2.5 MB snapshot of a 99-region world of 20255 people; identical
  frames either side of it; a delta across the save and four further years
  carrying 27 changed regions in 1600 bytes with `Whole = 0`, meaning the save
  did not tear the ground.
- The last check is the strongest one in the module and the cheapest to have
  skipped: comparing the two snapshots as bytes catches a view that writes
  through a `const` reference by some route the digest happens not to cover.

## ADR-0108: A delta is checked every frame, not at the end of the year

### Context

13.02 established that the difference between two views can rebuild the newer
one. The Phase 13 kernel gate has to decide how often to check that over a long
run, and the tempting answer - build a screen out of deltas for a year, compare
it once at the end - is the wrong one.

### Decision

**The screen is compared against a fresh frame on every single day of the
watched year.** A delta that is right 359 times and wrong once produces a
renderer that shows the wrong world on the 360th day and never recovers, because
every later delta is applied to a screen that already drifted. Checking once at
the end can only tell you that the drift happened to cancel out.

The gate also asserts its volumes before its verdict, per ADR-0095, for the
third time in three phases: the Phase 11 gate passed on 1081 intents instead of
14400, the Phase 12 gate passed with the whole phase measuring zero, and a gate
that watched a year of a world in which nothing moved would have been the third.
So it fails unless the world moved during the year it watched, unless at least
one frame carried real ground, and unless watching by difference actually cost
less than re-reading.

### Consequences

- Measured: a century at 256 taking the world from 55177 people to 110474; then
  360 frames watched a day at a time, 12 of them carrying anything at all, 45
  regions in the busiest one, and **35432 bytes of delta against 2560320 bytes
  of frames** - fourteen per thousand. The screen matched a fresh frame on all
  360 days, and the screen built out of nothing but differences equalled the
  world taken whole at the end.
- 50.9 seconds under gcc-debug, which makes this the cheapest gate in the
  project by a wide margin - the view layer does no simulation, it only reads.
- `View.ViewGate` is in `run_gates.sh`, added at the same time as the gate
  itself rather than two phases later, which is what happened to Colony and
  Gameplay.

## ADR-0109: A correctness suite does not assert on the wall clock

### Context

CI run 128 failed one job of nine - `linux-clang-release` - on
`Population.Lod`, a Phase 04 test nothing in Phases 12 or 13 had touched. The
failing line was:

```
VT_CHECK(With > Without * 1.05)   "holding a region at the fine grain costs something real"
```

It measures how many wall-clock seconds twenty years take with a region held at
the fine grain against the same twenty years without, and requires the held
world to be at least five per cent slower. It passes on an idle machine and
reproduces green locally. It failed on a runner with a hundred and forty-one
other tests on it.

### Decision

**The claim was right and the instrument was wrong.** The test's own comment
says what it means: *"the people of the held region are simulated one by one for
twenty years, so the world with it must do more work than the world without"*.
That is a statement about WORK, and seconds are only a proxy for work - a proxy
that on shared CI measures how busy the machine is.

So the assertion counts events instead: the held world's event log against the
coarse world's over the same twenty years. Measured, **686 events coarse against
18860 holding it** - a factor of twenty-seven, deterministic, and identical
under all three presets. The five per cent margin was never the real signal; it
was a proxy so weak that machine load could swamp it.

**The seconds stay in the log line.** "What does the fine grain cost" is worth
having on the record, and 11.01 was right to measure it. It is just not
something to fail a build over.

### Consequences

- Checked across the suite: this was the only wall-clock assertion in the
  project. Every other use of `steady_clock` - the seven phase gates - only
  prints how long a century took, which is reporting and not judging.
- The rule generalises: a test may MEASURE time and print it; it may not decide
  pass or fail by it. Anything a build fails on has to be a property of what the
  code did, not of what else the machine was doing.
- This is the second defect in three days found only because a job differed
  between presets, after `RegionView`'s padding (ADR-0105). The difference
  between "green on my two presets" and "green on nine jobs" keeps being where
  the real defects live.

## ADR-0110: Use the idiom the other seven gates use, and MSVC will not have to explain why

### Context

The Phase 13 kernel gate froze its digests as `constexpr Hash64` values in the
anonymous namespace and guarded the comparison with a runtime `if`:

```cpp
constexpr Hash64 FrozenView = 0xd95d1257...;
if (FrozenView != 0x0ull) { VT_CHECK_EQ(LastFrame, FrozenView); }
```

Every one of the seven gates before it uses `#define` at file scope and `#if`.
I used the newer-looking form because it looked cleaner. The Windows leg of CI
failed the build: **C4127, "conditional expression is constant"**, which that
job treats as an error.

### Decision

Use `#define` and `#if`, like the seven gates that already work on all nine CI
jobs. MSVC is right on the substance - a runtime branch on a compile-time
constant is exactly what `#if` is for, and a reader of the gate wants to know at
a glance whether the digests are frozen, which a preprocessor guard says and a
`constexpr` plus `if` only implies.

### Consequences

- The general rule, and it is the cheap lesson of the two: **when a project has
  done a thing seven times, the eighth is not the place to improve on it.** If
  the idiom is wrong, change all eight deliberately; do not diverge in one file
  and discover why on a platform that is not on this machine.
- This is the third defect in this session found only because the CI matrix is
  wider than the local one, after `RegionView`'s padding (ADR-0105) and the
  wall-clock assertion (ADR-0109). Three for three, the difference between
  "green here" and "green everywhere" is where the real defects were.
- Windows and macOS remain the two legs no local preset can stand in for. There
  is no fix for that except pushing and reading the result.

## ADR-0111: The chronicle can say what a region grew, and not what it ate

### Context

11.03 established the rule and the project paid for it there: a colony's ore is
credited through `Economy::AddStock` **and nothing else**, "so that the
chronicle can say where it came from". 01.05's causal edge and 03.07's `Why`
both work only on what the event log holds.

12.01 hit the other end of that rule without naming it. A conservation test had
to be written against a control world rather than against the log, because the
log could not account for a region's grain. That observation was left open when
Phase 12 closed. This is it, measured.

### The measurement

`Tests/Economy/Test_Ledger.cpp`, one ordinary year of the busiest region of a
128 world at year 303, 1460 people:

```
grain   the stores end +6;   the log names +7329;  7323 units moved unnamed
cloth   the stores end +15;  the log names +0;       15 units moved unnamed
tools   the stores end +8;   the log names +0;        8 units moved unnamed

the log names 7329 units in 28 events, and 7346 units moved that it does not
name at all
```

**A region harvested 7329 units of grain and its stores rose by six.** The log
records the 7329. It records nothing whatever about where the other 7323 went.

`ProductionSystem::Tick` writes `RegionStock::Amount` and `HouseStock::Amount`
directly in a dozen places and publishes exactly two events - `Harvest` and
`Shortfall`. Spoilage, meals, timber and salt burning, cloth and tools made and
worn out: all of it moves by direct assignment. `Shortfall` reports what could
NOT be fed, which is not the same as what was eaten.

### Decision

**Record it; do not fix it unilaterally.** The fix is one a person should choose,
because of what it costs rather than because it is hard:

- Routing 06.02's consumption through `AddStock` would add roughly four hundred
  events a year at 256 - cheap in itself.
- But it moves the **event log digest**, and that digest is frozen in eleven
  gates: History, Population, Society, Economy, Politics, Military,
  Infrastructure, Player, Colony, Gameplay and View. Every one would have to be
  re-recorded in a single deliberate pass.
- ADR-0095 exists because six phases' frozen digests once moved at once without
  anybody noticing. Doing that on purpose is fine; doing it at five in the
  morning on my own judgement is not.

### Consequences

- The test is permanent and asserts `Dark > 0` - that unnamed movement exists -
  rather than asserting it away. The day 06.02 publishes its meals, that line
  fails and says by exactly how much the world got more honest.
- **A fourth payment on the year-boundary trap.** The first version of this test
  reported "0% explained", which was true of the window and false of the world:
  the yearly systems run ON the boundary tick, so a window `(From, To]` whose
  `From` sits on one excludes that year's harvest while the `Before` snapshot
  was taken after it had already run. One tick off the boundary and the real
  numbers appeared. The comment in the test says so, because this is the fourth
  time.


---

### Revised by the chronicle (13.08e): the world cannot say anything was eaten

The original entry measured the gap in units: a region harvested 7329 of grain,
its stores rose by six, and the log names none of the other 7323. With 13.08d's
chronicle and 13.08e's cause chains readable, the same gap can be stated in the
world's own words, and it is worse than an accounting hole.

This is a complete explanation, as AELVOR gives it:

```
Year 202: Grain could not be had in Iarist.
   because  Iarist harvested 1085 of grain.
```

A famine explained by a harvest, and the chain stops there - because the events
between the two were never published. Counted over everything the world
remembers and every cause behind those memories, 8595 sentences in all:

```
harvest              53   (only ever as a cause, never as a record)
could not be had    159
went N short         58
eaten / consumed / spoiled / rationed / stored     0
```

**Not one sentence in the memory of a four-century world says anything was
eaten.** The two apparent matches are "great" and the generated names Feda and
Fedel; searched for as whole words, both are zero.

So the choice in this ADR is not only about a digest. Routing 06.02's spoilage
and meals through `AddStock` is what would let the chronicle say where a harvest
went, and without it the causal chain the README promises stops one link short
of every famine this world has ever had - offering, as the reason people
starved, the fact that they grew food.

## ADR-0112 — A build target names every module, and the uproject is what makes it build

**Date:** 2026-09-10
**Status:** Accepted
**Phase:** 13.06 — first UBT build

### Context

Thirteen kernel modules exist. Twelve are simulation; the thirteenth, `Vaelen`,
is the engine bridge. Both build targets said this:

```csharp
ExtraModuleNames.AddRange(new string[] { "VaelenCore", "VaelenSim",
    "VaelenPopulation", "VaelenSociety", "VaelenEconomy", "VaelenPolitics",
    "VaelenMilitary", "Vaelen" });
```

Seven simulation modules of twelve. `VaelenInfrastructure`, `VaelenColony`,
`VaelenPlayer`, `VaelenGameplay` and `VaelenView` — everything built in Phases 09
through 13 — were named nowhere, and no edge in the module graph leads to them
either: `Vaelen.Build.cs` depends on the kernel only as far as `VaelenPolitics`.

The list was correct when it was written, at the end of Phase 07. Every phase
since added a module and left the targets alone, because nothing on this machine
reads them: CMake enumerates `Tools/kernel_modules.txt`, the purity checker walks
the directory tree, and the nine CI jobs are all CMake.

### What I predicted, and what actually happened

I predicted a hollow build: UBT compiles what the target names plus that
closure, prints `Build succeeded`, and never opens the other five modules —
13.06 answering "yes" about five twelfths of a kernel it never read.

**The build was run with the stale targets, and that is not what happened.**

```
[107/157] Link [x64] UnrealEditor-VaelenInfrastructure.dll
[117/157] Link [x64] UnrealEditor-VaelenColony.dll
[129/157] Link [x64] UnrealEditor-VaelenPlayer.dll
[144/157] Link [x64] UnrealEditor-VaelenGameplay.dll
[149/157] Link [x64] UnrealEditor-VaelenView.dll
```

All five compiled and linked. UnrealBuildTool builds the modules a project
descriptor declares, not only the target's `ExtraModuleNames`, and
`Vaelen.uproject` has listed all twelve since each was written. The gate was
full; the list I was reading was not the one holding it up.

### Decision

**Both targets name all thirteen modules anyway**, and the reason is honesty
rather than necessity: two lists that describe the same set should agree, and a
reader who opens `VaelenEditor.Target.cs` to learn what the editor builds should
not be told seven when the answer is thirteen. `ExtraModuleNames` is the
mechanism for "build this though nothing needs it yet", so it is the right place
— not `Vaelen.Build.cs` gaining dependencies it does not use, which it will earn
in 13.07 when `VaelenPresentation` actually consumes `VaelenView`.

### Consequences

- A module added in a later phase is declared in **four** places, and the
  checklist in ROADMAP section 18 says so: `Tools/kernel_modules.txt`, its
  `CMakeLists.txt` entry, `Vaelen.uproject`, and both `.Target.cs` files. Only
  the third is load-bearing for UBT; the fourth is documentation that a target
  should not lie.
- **The lesson is about the claim, not the code.** I reasoned from one file to a
  failure mode, wrote it up as fact, and the toolchain I could not run disagreed
  within the hour. ADR-0095 says a gate must be full of what it claims to
  measure; this is the mirror of it — a defect report must be full of what it
  claims to have found. Reading the file UBT reads is evidence about the file.
  Only running UBT is evidence about the build.
- The correction cost nothing because the build was run before the change was
  pushed. It would have cost a false entry in this document forever if it had
  not been.

---

## ADR-0113 — C4251 is what exporting a kernel class costs, and 13.06 is not where it is paid

**Date:** 2026-09-10
**Status:** Accepted
**Phase:** 13.06 — first UBT build

### Context

The first UnrealBuildTool build of the whole kernel succeeded — thirteen modules,
157 actions, ninety-two seconds, MSVC 14.51 under UE 5.6, `Result: Succeeded`.
It produced no errors and several thousand lines of one warning:

```
Vaelen\Sim\EventBus.h(56,22): warning C4251:
    'Vaelen::EventLog::Events': 'std::vector<Vaelen::Event,...>'
    must have dll-interface to be used by clients of 'Vaelen::EventLog'
```

Every kernel module owns its export macro (`VAELEN_SIM_API` and its eleven
siblings), and each `Build.cs` turns it on only for a modular link:

```csharp
if (Target.LinkType == TargetLinkType.Modular)
{
    PrivateDefinitions.Add("VAELEN_SIM_EXPORTS=1");
    PublicDefinitions.Add("VAELEN_SIM_IMPORTS=1");
}
```

Seventy-nine classes carry that macro across the twelve modules — 24 in
VaelenSim alone — and most hold a `std::vector`, a `std::unique_ptr`, a
`std::string` or a `std::string_view`. MSVC warns once per such member per
translation unit that sees it, which is where the thousands of lines come from.

### What the warning does and does not mean

It is not noise about nothing. Exporting a class whose layout contains a
`std::vector` means a client's inlined code manipulates a container the DLL
allocated. That is safe exactly when both sides share one STL and one CRT, and
it corrupts the heap when they do not.

Unreal builds every module of a target with one toolchain and one CRT, so this
is safe today by construction rather than by luck. A monolithic target
(Shipping, Test) never defines the macros at all and the warning does not exist
there. It is a modular-editor-build phenomenon.

### Decision

**Record it; do not silence it in 13.06, and do not refactor it either.**

- Silencing C4251 per module is what Unreal's own code does and it would remove
  three thousand lines of noise. It would also remove the one signal that says
  which classes cross a DLL boundary by value — and 13.07 is the first task with
  a real cross-module consumer (`VaelenPresentation` reading `VaelenView`).
- Refactoring seventy-nine classes to export free functions over opaque handles
  is a real answer to a problem nobody has yet, and it would move headers that
  eleven CI gates depend on.
- 13.06 asked one question — does the kernel compile under UBT — and the answer
  is yes. Answering a second question badly is not a bonus.

So it goes to 13.07, where a Windows build exists in the same pass that would
apply the change, which is the only place the change can be verified. Anything
decided here would be decided blind: this container has no Unreal, so a
`#pragma warning(disable:4251)` guarded by `_MSC_VER` compiles to nothing under
gcc and clang and the local gates would prove exactly nothing about it.

### Consequences

- ROADMAP section 19 carries it as a named limit of 13.07 with the two candidate
  fixes and the measurement above.
- The eleven `*.Build.cs` files and twelve `*Module.cpp` files lose `STATUS:
  UNVERIFIED`. They have now been read by the toolchain they were written for.
  Thirteen phases of engine-side files carried that label on the promise that
  Phase 13 would pay the debt; this is the payment.
- What the build did NOT verify stays unverified: nothing ran. `Result:
  Succeeded` says the kernel compiles and links as twelve DLLs under MSVC. It
  says nothing about a world ticking inside the editor, which is 13.09's gate.

---

## ADR-0114 — The kernel runs inside the editor, and that is a different claim from 13.06's

**Date:** 2026-09-10
**Status:** Accepted
**Phase:** 13 — between 13.06 and 13.07

### What happened

13.06 established that the kernel COMPILES and LINKS under UnrealBuildTool, and
said explicitly that nothing had run. Twenty minutes later something ran. From
the editor's console, on a machine with UE 5.6:

```
Cmd: Vaelen.Atlas
LogVaelenAtlas: material BasicShapeMaterial: 1 vector parameter(s) — Color
LogVaelenAtlas: AELVOR 128x128, seed 0x41454c564f52: year 420,
                6459 land tiles, 65 regions peopled, 36374 living,
                43 towns, 85 roads, 1 polities standing
                (region 26 simulated person by person).
                Simulated in 1.00 s.
```

And earlier in the same log, at module startup:

```
LogVaelen: VAELEN 0.0.1 - kernel save format v3 - kernel asserts on - module started
```

### What this is evidence of, precisely

- **Twelve kernel modules load into a running editor.** `LogPluginManager` found
  the target receipt, the DLLs mounted, `FVaelenModule::StartupModule` ran and
  installed the log sink and the assertion handler. The line above is the
  kernel's own logging arriving through Unreal's, which is the whole point of
  `VaelenLogSink`.
- **A world generates, lives four hundred and twenty years, and reports.** Three
  hundred of pre-history and a hundred and twenty with every system: population,
  houses, needs, traits, LOD, organisations, norms, stocks, production, markets,
  trade, wealth, standing, polities. Thirty-six thousand people alive at the end.
- **With assertions ON.** `kernel asserts on` is in the startup line. This was
  not an indulgent build that skipped its own checks.
- **In one second.** 420 years of a 36k-person world, most of it at coarse LOD,
  inside the editor process.

### What it is NOT evidence of

Nothing was SEEN. The report is a log line; the plate was laid out into a level
whose camera was eighty-five thousand units away, on top of the Open World
template's own Landscape. Whether AELVOR is legible on a screen is still 13.07b
and 13.09, and this ADR does not anticipate them.

Nor is it a frame rate. "Simulated in 1.00 s" is the cost of building the world
once, not the cost of a frame, and 13.09's gate asks the second question.

### Why this is worth an ADR rather than a line in the roadmap

Because 13.06's commit message went out of its way to say "Nothing RAN" and to
put that limit on fourteen files. Twenty minutes later the limit was gone. A
project that writes down what it has not yet proved has to be equally quick to
write down when it proves it, or the labels drift into pessimism and stop
meaning anything - which is the same failure as ADR-0112's, pointed the other
way.

The fourteen files keep their VALIDATED label unchanged: it says "compiled and
linked by UnrealBuildTool in 13.06; not run in the editor". That is still an
accurate statement about what 13.06 verified. This ADR is what the editor added.

## ADR-0115 — Presentation cannot stop reading the world until the view carries the ground

**Date:** 2026-09-10
**Status:** Accepted
**Phase:** 13 — task 13.07a

### The rule, and the thing the rule could not have

Since Phase 00 the layering rule has said PRESENTATION reads WORLD STATE and does
not touch it. 13.01 made it structural: a `WorldView` is a flat block of numbers
with no pointer, no handle and no component type, so a renderer holding one has
nothing to reach the simulation with.

And `AVaelenAtlasActor` reads `Map.GetLayer(T.World.Layers.Biome)` directly,
tile by tile, along with the terrain layer, the elevation layer, the river and
lake layers. ADR-0112's task notes called this the thing 13.07 must fix.

Reading both sides together says something sharper than "the actor is
non-compliant". **A `WorldView` is regions.** Ninety-nine of them on AELVOR at
128, fifty-five at 256. The coastline the actor draws — six thousand four
hundred and fifty-nine land tiles against ten thousand of sea — is not in the
view and never was. The actor reads the world because the world is where the
coast is.

### The decision

Give the view the ground: `Vaelen/View/Land.h`, a `MapView` of `TileView`s, one
per tile, carrying elevation, biome, region and a byte of ground flags. Same
promise as 13.01 one level down — eight bytes of plain numbers, no handle, no
way back — and the same test (`Land.TheGroundOutlivesTheWorldItCameFrom`) that
13.01's design property makes possible.

The general form of the finding, which is worth more than the file:

> **A layering rule with nothing behind it is a rule everybody breaks.** Before
> asking a consumer to stop reaching past a boundary, look at what it is
> reaching for. If the boundary does not carry it, the consumer is not
> undisciplined — the boundary is incomplete.

### What it costs, said out loud

A `WorldView` weighs 5600 bytes on AELVOR at 128 and is meant to be taken sixty
times a second. A `MapView` weighs 131120 bytes at 128 and 512 KB at 256, and
**is not a per-frame structure**. The ground changes when the world is generated;
take it then, keep it, and take a frame for the year-by-year things. The header
says so where somebody reaching for it will read it.

### Two smaller decisions inside it

- **`GroundFlag` mirrors `WorldGen::TerrainFlag` and `Land.cpp` asserts it.** The
  first four bits are copied so a renderer needs one byte instead of three
  layers; copied values drift, asserted values cannot. If Phase 02 renumbers a
  flag the file stops compiling instead of quietly painting the coast inland.
- **Elevation is kept in Q16.16, not Fix64 raw.** Shifted right sixteen and
  saturated, so a tile is eight bytes rather than sixteen, and a mountain past
  32767 units is clamped rather than wrapped into a trench. The saturation is
  deliberate and the header says the scale.

### Verified

`Tests/View/Test_Land.cpp`, five suites, thirty-three checks: no way back into
the world (compiler-checked), the ground matches the world tile for tile across
every one of 9216 tiles, the ground outlives its world, one seed gives one
ground and two seeds do not, and a world with no map says so instead of reading
an empty layer by tile index.

## ADR-0116 — The world gets looked at without an engine, and Unreal stays the target

**Date:** 2026-09-10
**Status:** Accepted
**Phase:** 13 — task 13.07a

### The situation this answers

13.06 and ADR-0114 bought something real: the kernel compiles under UBT and runs
inside UE 5.6. The first image of AELVOR came out of that editor. The image was
also, honestly, a plate of coloured cubes and a map — thirteen phases of
simulation and no art, because `Content/` is empty and no phase has yet been
about pixels.

And the cost of getting to it, on the machine that has UE: a fifty-seven-second
editor startup, a graphics driver from 2021 that the engine warns about on every
launch, four gigabytes of VRAM, and a build of the C++ project before anything
new can be tried at all. Every question about the world — does the coast look
like a coast, does the terrain have ranges or just noise, what happened in year
300 — costs that.

### The decision

**Unreal stays the target. It stops being the daily loop.**

The daily loop becomes `Tools/Atlas`: a headless executable, built by all nine CI
jobs, that generates AELVOR, runs its centuries, takes a `WorldView` and a
`MapView`, and writes both out as JSON. What draws them is a separate concern —
and can be anything, because what it is handed is numbers.

### Why this is available at all

Because of 13.01's design property and nothing else. A view holds no pointer, no
handle and no reference into the world, so **it outlives the world**, serialises
without a serialiser and crosses any boundary — a thread, a process, a file, a
browser. That was written down in `Frame.h` as a layering guarantee. It turns out
to be a portability guarantee as well, and this is the first task that spends it.

### What Unreal is still for, so this is not a retreat

- The game. Everything Phase 14 and after is about — a person on the ground, a
  camera at eye height, the colony the player wakes up owned in — is engine work,
  and 13.06 is what makes it possible.
- The 13.09 gate: the editor open on AELVOR at 256, a century running, a frame
  rate written down. That needs the engine and a person at a screen.
- The moment art exists. A tool that writes numbers cannot show a face.

### The measurement that made the call, rather than the mood

Taken on the machine with UE 5.6 (i5-10400, T400 4 GB), Development Editor:

| Map | Land tiles | Living | Simulated |
|---|---|---|---|
| 128 x 128 | 6459 | 36374 | 1.00 s |
| 256 x 256 | 25842 | 123600 | 6.05 s (6.24 / 6.40 / 6.52 on repeats) |

Four times the tiles and 3.4 times the people cost six times the seconds: it
grows a little faster than linearly and it stays inside what a gate can hold.
13.09 is reachable. This ADR is about where the day is spent, not about whether
the engine can carry the world.

### The cross-check that came free

`Tools/Atlas` on Linux with GCC against the editor on Windows with MSVC, same
seed, both sizes:

| Map | Land tiles | Peopled | Living | Year | UE 5.6 | headless (release) |
|---|---|---|---|---|---|---|
| 128 x 128 | 6459 | 65 | 36374 | 420 | 1.00 s | — |
| 256 x 256 | 25842 | 55 | 123600 | 420 | 6.05 s | 5.65 s |

Every number of the world identical on both sides. Two toolchains, two operating
systems, two build systems, one world. Determinism has been asserted by tests
since Phase 00; this is the first time it has been observed across the engine
boundary.

## ADR-0117 — The first thing to draw AELVOR is not the engine, and that is the point

**Date:** 2026-09-10
**Status:** Accepted
**Phase:** 13 — task 13.07b

### The decision

`Tools/Viewer/Atlas.html`: the world drawn from the JSON of 13.07a and from
nothing else. Not `VaelenPresentation`, not Unreal, not a module — a page, in
the repository, built by a script that refuses to build it from a file the
checker rejects.

### Why a page rather than the engine module the roadmap named

Two reasons, and the first is the one that matters.

**It is the strongest possible test of 13.01's claim.** The layering rule says
PRESENTATION reads a view and cannot reach the simulation. A UE actor that obeys
the rule is a UE actor that could have disobeyed it — the World is right there,
one `GetWorld()` away, and the discipline is the programmer's. A web page cannot
disobey. It has no World, no kernel header, no process in common with the
simulation, and the world that produced its numbers was destroyed before the
file was written. If the view were missing something, this page could not draw
it, and no amount of care would help. **It drew it.**

**And it is the loop.** Every question about the world — does the coast look like
a coast, does the terrain have ranges, where did the towns go — now costs one
command and a browser tab, on any machine, instead of a fifty-seven-second
editor start on the one machine that has UE.

### What it settled

A question open since the first screenshot of AELVOR: whether the terrain has
shape or is flat noise with colour on it. Slope shading, light from the
north-west, answers it — there are ranges, and they run. The generator is not
the problem, and 13.07c can be about rendering rather than about world
generation. That is a task's worth of work not spent.

### What it did NOT settle, said plainly

- **`VaelenPresentation` still does not exist.** The roadmap's 13.07b is now
  13.07c and stays engine-side. Drawing AELVOR in a browser proves the view
  carries enough to draw from; it proves nothing about Unreal.
- **ADR-0113's C4251 decision stays open.** It resolves where MSVC compiles a
  cross-module consumer, and MSVC never reads this page. Calling a warning dealt
  with because a different compiler never emitted it is exactly the kind of fake
  green this project spends its ADRs avoiding.
- **This is a map, not a game.** Nobody is on the ground, there is no camera at
  eye height and there is no art. Phase 14 and after are where that lives.

### The palette, and why it is copied rather than chosen

The page uses `AVaelenAtlasActor::PaintColour`'s exact values. A viewer that
invented its own colours would show a world that looks different from the one
the engine shows, and every comparison between the two would then be an argument
about palettes. Copied values drift, so the reason is written here: when the
engine's palette changes, this changes with it, deliberately and by hand.

## ADR-0118 — Six towns stand where nobody lives, and the rule that should close them counts the wrong thing

**Date:** 2026-09-10
**Status:** Proposed — the change is the project owner's call, not mine
**Phase:** 13 — found by 13.07b, one hour after the viewer existed

### How it was found

By looking. `Tools/Viewer/Atlas.html` had been open for under an hour when a
count of settlements against region population came back with six settlements
whose region holds zero people. Thirteen phases of tests never asked that
question, because no test knew it was a question.

### The measurement, taken from the world and not from the view

A probe linked against the kernel, reading `RegionPopulation::Total` directly on
the region entity — so this is not a gap in `TakeView`:

```
settl.   region   people    tiles  traffic  routes   quiet
#100         75        0      351        0       1       4
#112         14        0      398       12       3       0
#115        122        0      105        1       1       0
#117         20        0      246        4       1       0
#119         33        0      256        7       2       0
#121         34        0       59      117       1       0
```

`Quiet` is the counter that leads to abandonment. Five of the six sit at **zero**
and are reset every year. Only #100, which receives nothing, is on its way out —
and that one is the rule working correctly.

The same probe, over every region with no inhabitants, on the goods held in
common there:

```
region  85: 9065 units    region  89: 6816    region  80: 5952
region  47: 5485          region 100: 5474    region 105: 3763
region  24: 3978          region  45: 3994    region 115: 3921
```

Nine thousand units of grain, timber, ore and salt sitting in a region where
nobody is left to eat, burn, forge or salt anything.

### The mechanism

`TradeSystem` (06.04) abandons a settlement after `AbandonAfterQuietYears` years
without traffic:

```cpp
Live->Quiet = Traffic[Live->Region] > 0 ? 0u : Live->Quiet + 1u;
if (Live->Quiet >= Rules.AbandonAfterQuietYears) { Live->Abandoned = Context.Tick; }
```

and traffic is credited to **both** ends of any route that moved anything:

```cpp
if (A < N) { Traffic[A] += Units; }
if (B < N) { Traffic[B] += Units; }
```

So a region that only ever RECEIVES goods has its `Quiet` reset every year it is
delivered to. Put plainly:

> **The rule that decides whether a settlement still exists never asks whether
> anybody lives in it.** It asks whether anything was delivered there. A place
> nobody lives in, that is still being shipped to, is immortal.

### Why nothing has been changed

Same reason as ADR-0111, and the same person's call. Any of the fixes below
changes how many settlements stand, which changes the settlement count in the
frame, which moves the **event-log digest frozen in eleven gates**. That is a
deliberate single-pass re-freeze and somebody's decision rather than mine.

### The four answers, and they are genuinely different

1. **Abandon when the region empties.** One line. Most obviously "correct", and
   it makes a settlement mean "a place people live".
2. **Require traffic AND people.** Keeps the quiet rule and adds the missing
   half. Slowest to abandon, least disruptive to existing behaviour.
3. **Count only OUTBOUND traffic as a sign of life.** A place that only consumes
   is being kept alive from outside; a place that sends something is alive. This
   is the most interesting economically and the hardest to reason about.
4. **Change nothing, and rename it.** A depot with no inhabitants, kept alive by
   the trade passing through it, is not obviously a bug in a world whose whole
   premise is that systems cause events nobody wrote. It may be a *story* — the
   warehouse settlement, the caravan stop, the granary that outlived its town —
   and the actual defect may be that the word for it is "settlement".

My own reading: (4) is the one worth thinking about before reaching for (1). But
this is a decision about what VAELEN's world MEANS, and that is not mine to take.

### The finding that is not about settlements

The viewer paid for itself in an hour. Thirteen phases of unit, integration,
determinism, edge and long-duration tests are all tests of things somebody
already suspected. **Nobody had ever looked at the world**, and the first look
produced an anomaly no existing gate could have caught, because every gate
compares the world to what the world did last time.

### Revised by the chronicle (13.08e): the six were the residue, not the defect

The reasoning above asked why six settlements could not DIE. With 13.08d's
chronicle readable, the world answers a question nobody had put to it - how they
were BORN:

```
Year 405: The town of region 14 rose on the traffic of its roads.
Year 413: The town of region 20 rose on the traffic of its roads.
```

They were not towns that emptied. They were founded in regions that were already
empty, by traffic alone. Counted over the whole run of AELVOR at 256, against the
kept frames of 13.08c:

```
towns founded over four centuries                          121
  rose in a region with NO inhabitants at the frame before  100
  empty both before AND after the founding                   75
  rose where anybody at all lived                            15
regions where a town "rose" more than once                   44
```

**One hundred of a hundred and twenty-one towns in this world were founded where
nobody lived**, and forty-four regions had a town rise more than once - one of
them four times.

So the founding rule has the same blindness as the abandonment rule, and it is
the larger half:

```cpp
if (Settled[R] != 0 || Traffic[R] < Rules.SettleFromTraffic || RegionHandles[R].IsNull()) { continue; }
```

Traffic founds a town. Traffic keeps it. Nobody is asked at either end. The six
settlements standing in empty regions at year 420 are not the anomaly - they are
what is left at the end of a process that has been doing this for four hundred
years, and the towns on the map of AELVOR are mostly, and always were, empty.

### Corrected an hour later, by looking further (13.08e, second pass)

The paragraph above ended with a sentence that was not true, and the correction
matters more than the sentence: **"the towns on the map of AELVOR are mostly, and
always were, empty."**

Every measurement behind that claim stopped at year 420, because every gate in
this project stops around there. Run the same world to 1500 and:

```
year   50   16 towns, 12 of them empty    4 regions peopled
year  100    2 towns,  1 of them empty    4 regions peopled
year  250   36 towns, 15 of them empty   22 regions peopled
year 1500   65 towns,  0 of them empty   84 regions peopled
```

**At year 1500 not one town stands in an empty region.** The abandonment rule
does clear them - it just takes centuries, and year 420 happened to catch six in
flight. What survives of the finding is narrower and still real:

- TRUE: 100 of the 121 foundings between year 0 and 420 happened in regions with
  no inhabitants. Traffic founds a town without asking whether anyone lives there.
- TRUE: at year 420, six towns stood in empty regions.
- FALSE: that this is the steady state. It is a transient, and the world resolves
  it on its own over hundreds of years.

So the defect is real and **self-limiting**, which weakens answer 4 (renaming a
thing the world eventually corrects) and takes the urgency out of 1 and 2. What
it leaves is a different question nobody has asked: sixteen towns at year 50 and
two at year 100 is violent churn, and whether THAT is a world worth having is a
design question rather than a defect report.

### The lesson, which is ADR-0125's, one hour later and on my own claim

A new instrument showed a pattern; the pattern was real; and the sentence
generalised it from one snapshot to "always". The instrument that found the
overreach is the same one - run longer and look again. **Every gate in this
project stops at four or five centuries, so every claim made from them is a
claim about a young world**, and that limit is not visible from inside the data.

That does not decide the four answers below; it changes what they are worth. If
a settlement is a place trade passes through, VAELEN spends centuries building
caravan stops and then tidying them away, and the map might simply say so.

## ADR-0119 — A CI budget with no headroom is not a passing job, it is a lucky one

**Date:** 2026-09-10
**Status:** Accepted
**Phase:** 13 — after 13.07b

### What happened

Run 140 on `284cfb3`: **seven of nine jobs green**, including the two that
matter most for a change to `VaelenView` — Windows MSVC (60 min of tests) and
macOS AppleClang (83 min). The two debug legs were **cancelled at 117 minutes**
by `timeout-minutes: 120`.

### What it was not

The obvious suspect is the work 13.07a added. The log refutes it:

```
145/151 Test #148: View.Land ...........................   Passed    1.69 sec
```

One and seven tenths of a second, plus five Atlas entries of a few seconds each.
That is not two hours.

### What it was

By the time the axe fell, **150 of 151 tests had passed.** The job was killed
waiting on one: `Gameplay.Shuffled`, which ctest picked up at 11:04:59 — ninety-
five minutes into a hundred-and-twenty-minute budget — and which needs about
forty-five minutes of its own, because by construction it re-runs every suite in
its module in one process.

The other long poles, measured in the same log:

```
Infrastructure.Shuffled   2326 s
Gameplay.GameplayGate     1150 s
Player.Shuffled           1028 s
Gameplay.Fame              545 s
Gameplay.Judgement         535 s
Player.PlayerGate          495 s
```

ctest runs four at a time. **The wall clock of a leg is set by when its longest
test STARTS, not by how much work there is in total.** A forty-five-minute test
picked up last adds forty-five minutes to the job no matter what else has
finished. Adding six short tests did not add two hours of work; it changed the
packing, and the packing is what decided the job.

So the honest statement of the old state: **gcc-debug finishing at 114 of 120
minutes was never a passing job. It was a job that passed when the packing was
lucky**, and nobody had noticed because it had always been lucky.

### The decision, in two parts

1. **The long poles are declared.** ctest orders by descending `COST` when it has
   one, and has none on a fresh checkout. Every `<Module>.Shuffled` now carries
   `COST 10000` and every gate `COST 5000`, so the things that cannot be
   parallelised start first instead of last. This attacks the makespan rather
   than the symptom.
2. **The budget gets real headroom.** 120 → 180 minutes on the Linux matrix, and
   on Windows and macOS too, which carry the same suite and the same growth.

Raising the timeout alone would have been the fudge — it hides the packing
problem until the next phase re-creates it. Setting COST alone would have been
the gamble — it improves the expected makespan without proving the worst case
fits. Both, or neither.

### The rule this leaves behind

> A test budget is not a limit on how long the suite may take. It is a claim
> about the worst packing. When a job routinely finishes within five per cent of
> its timeout, it is already failing — it just has not been unlucky yet.

### And then the ordering was measured, and there is nothing left in it

Run 144 is the first run that was never cancelled, so it is the first real
timing this project has. `linux-clang-debug`: **5394.65 s**, 152 tests, `-j4`.

The obvious next move was to give every long test a `COST`, not just the
`Shuffled` and `Gate` ones — three Gameplay tests of five minutes each started
at minute eighty-five, which looks exactly like the defect this ADR fixed.

Simulated against the measured times before touching anything:

```
serial work            21192 s   (353 min)
longest single test     2276 s   Gameplay.Shuffled
floor, 4 workers        5298 s   (88 min)

measured on CI          5395 s   (90 min)
simulated, as it is     5298 s
simulated, cost = time  5299 s
```

**The win is zero.** The schedule is already at the floor and the two per cent
above it is process start-up, not packing. Those three late tests are not waste:
with 21192 s of work over four workers, something has to run at minute
eighty-five, and the scheduler is choosing well.

So: **do not add more `COST` properties.** The makespan is bound by
`total work / workers`, not by ordering, and the only two levers left are more
workers (a GitHub `ubuntu-24.04` runner is 4 vCPU, so `-j4` is already right —
a larger runner would roughly halve it) or less work in the tests themselves.

Recorded because the next person to look at a ninety-minute CI leg will have
the same idea I had, and the measurement takes ten minutes while the change
would take an hour and buy nothing.

## ADR-0120 — A pair of regions does not identify a road, and 06.04 meant it to

**Date:** 2026-09-10
**Status:** Proposed — the fix is the project owner's call, not mine
**Phase:** 13 — found by 13.08a

### How it was found

By writing the obvious test. `Test_Net.cpp` looked each route up by its pair of
regions and compared it to the world. It failed on **96 of 254 routes**, and the
first four mismatches said the whole thing:

```
world #120 19->23 carried     0 idle 5 open 0
view  #21  19->23 carried 13468 idle 0 open 1
```

Two route entities, one pair of regions. On AELVOR at 256 it is **105 of 275**.

### The mechanism, and it is four lines

`TradeSystem::Tick` sorts every route into `Open` or `Closed` **once, at the top
of the tick**:

```cpp
(R.Closed == 0 ? Open : Closed).push_back(Route{H, R});
```

Step 1 then closes the routes that have gone idle, mutating the copy in place:

```cpp
Live->Closed = Context.Tick;
Rt.Info.Closed = Context.Tick;   // in `Open`, which is where it stays
```

Step 2 reopens roads, and looks for the old entity in `Closed` only:

```cpp
for (const Route& Old : Closed) { if (Old.Info.From == A && Old.Info.To == B) { ... } }
```

A road closed in tick T and warranted again in tick T is therefore not found —
so a **new entity** is created for a pair that already has one. The comment
directly above that loop states the intent the code misses:

> *A road once built is reopened rather than built again.*

### Why nobody saw it

`RouteStats::Twice` counts pairs carrying two OPEN routes, and that has never
fired — correctly, because `IsOpen` does prevent it. Every twin is one open road
and one closed one, which is precisely the case the only check does not look at.
It took reading the routes out of the world and comparing them one by one.

### What it costs

- Entities and memory, in proportion: 38 per cent of routes are twins.
- **The history of a road.** `Openings` counts how many times a road has been
  reopened; a twin starts again from one, so a road opened five times reads as
  five different roads and no chronicle can say "the road to X opened again".
- **`Identity` stops identifying.** It is `LatticeHash(seed, A, B)`, so both
  twins carry the same one, and a hash meant to name a thing names two.

### The fix, which is small

Either move the entry from `Open` into `Closed` when it is closed, or search
both lists for the reuse. One line and a comment. It is not applied here for the
usual reason: it changes the route count, which moves the event-log digest
frozen in eleven gates. Same class as ADR-0111 and ADR-0118, same person's call.

### What the view does in the meantime

It reports the world as it is. `RouteView::Index` is the identity, `RouteOf`
looks a route up by it, and `RouteBetween` returns the OPEN road for a pair when
there is one - because that is the road that exists. A view that quietly kept
one twin and dropped the other would have made the defect invisible again, and
`Test_Net.cpp` has a suite whose whole job is to fail if anyone tries.

### What the fix actually costs, measured rather than estimated (added 2026-09-10)

The three paragraphs above say the fix is small and that applying it "moves the
event-log digest frozen in eleven gates". That was an estimate. It has now been
measured: the fix applied to the working tree, the whole suite run, the world
written out before and after, and the tree reverted to the byte. **Nothing was
committed** - the decision is still the project owner's. What follows is so the
decision can be taken on numbers.

**The fix that was measured** searches `Open` as well as `Closed` for the road
to reuse, requiring `Closed != 0` on the match. Nineteen lines with the comment.
`IsOpen` has already ruled out an open route for the pair, so a match in `Open`
can only be a road that closed during this very tick - the case the defect is.

**What it does to the world**, AELVOR at 256, 300 years of pre-history and 420
years run:

| | before | after |
|---|---|---|
| route entities | 317 | **184** |
| pairs of regions with a road | 186 | 184 |
| pairs carrying twins | 131 | **0** |
| roads open at the end | 83 | 84 |
| chronicled first openings | 317 | **184** |
| roads told they opened for the first time more than once | **131** | **0** |
| "fell out of use" | 686 | 1395 |
| records in all | 18723 | 19309 |
| living | 206710 | 206710 |
| freed / enslaved / died / married | 6275 / 5379 / 2466 / 1327 | identical |

Every twinned pair carried exactly two entities, never three. `RouteStats::Twice`
is 0 in both columns, which is the point of ADR-0120: the only check that
existed could not see any of this.

**It is not people-neutral, and the 256 column is misleading on that.** At 128
the same run gives 45535 living before and **45544 after** - nine people. So the
right statement is that the fix changes what the world's roads are, and the
change reaches the living, faintly. At 256 it happened to cancel; that is not a
property to rely on.

**What it does to the suite**, `linux-gcc-release`, 155 tests:

- baseline **155/155**, 645 s
- with the fix, **26 fail**

Twenty-four of the twenty-six are frozen constants - digests and record counts -
and are mechanical to re-freeze. **Two are not**, and they are the reason this
measurement was worth taking:

```
Tests/Economy/Test_EconomyHistory.cpp:494  VT_CHECK(S.Records < Harvests / 4)
Tests/Player/Test_PlayerGate.cpp:1574      VT_CHECK(Lives > 1)
```

**The first says the fix is necessary but not sufficient.** 06.07 records a
road's opening only when `Openings <= 1` - a first building is history, a
reopening is not. With twins, every reopening was a NEW entity, so it read as a
first opening and got recorded; falsely, but symmetrically with the closings.
Fix the twins and that symmetry goes: a road is now said to open **once** and to
fall out of use **1395 times**. Records at 128 go 1147 -> 1615 and cross the
threshold `a harvest a region a year is not history` was defending. So applying
ADR-0120 also asks 06.07 a question it has never been asked: **is a road
reopening history?** Right now the answer is no, and after the fix that answer
makes the chronicle of the roads lopsided.

**The second is sharper.** `PlayerGate` asserts *"and the world's mortality
really did end a life and start another"*. With the fix it does not: one played
life spans the whole forty years. Nothing about mortality changed - the roads
changed, so the food changed, so the bound person in a crowded region lived.
That assertion is either an invariant of the design or an accident of this seed,
and nobody has had to decide which until now.

**The blast radius is exactly the layering.** The gates that move are ECONOMY,
POLITICS, MILITARY, INFRASTRUCTURE, COLONY, PLAYER, GAMEPLAY and VIEW. The gates
that do not are HISTORY, POPULATION and SOCIETY - every phase below 06. A change
in `TradeSystem` reaches everything above it and nothing beneath it. That is the
layering rule of the whole project holding under a real change rather than in a
diagram, and it is the first time it has been put to the test this way.

**So the cost, exactly:** 24 constants to re-freeze across 8 gates, each of which
must be justified rather than pasted, plus two decisions - what 06.07 should
record about a road that reopens, and whether `PlayerGate` should assert that a
played life ends. It is not a one-line change. It is a one-line change and an
afternoon of deciding what the world is supposed to say about its roads.

**Reproducing it:** the working-tree patch and the measurement scripts are not
in the repository; they were scratch. The patch is the `Open` search quoted
above, in `TradeSystem::Tick`'s reuse pass in `Source/VaelenEconomy/Private/Trade.cpp`.

### What it costs, revised upward — evidence from the chronicle (added in 13.08e)

The first version of this ADR counted the cost as entities, memory and a lost
`Openings` history. With 13.08d's chronicle readable, the real cost is worse:
**the history of the world is wrong, and wrong in sentences.**

06.07 records a road's opening only when `Openings <= 1` - a first building is
history, a reopening is not. A twin is a NEW entity, so it starts at one, so its
opening is chronicled as a first. Everything the world says about one such road:

```
Year 0:   The road from Miogu to Yiotur was opened.
Year 385: The road from Miogu to Yiotur was opened.
```

Opened for the first time twice, three hundred and eighty-five years apart, with
nothing said in between about it ever closing - because what closed was the other
entity, and `NameRoute` calls a route that is gone "road N" rather than by its
towns.

Counted across AELVOR at 256:

```
the chronicle claims  275 roads opened for the first time
the world contains    170 pairs of regions ever linked
therefore             105 of those claims are false
```

**Thirty-eight per cent of the road history of this world is untrue.** Not
missing, not approximate - a sentence that says a thing happened for the first
time, about a thing that had happened before. For a project whose README says
the simulation is the source of truth and whose chronicle is meant to be
readable as history, that is a different order of defect from a wasted entity,
and it is the reason this ADR is worth acting on rather than filing.

## ADR-0121 — The network is not part of the frame, because the frame is diffed

**Date:** 2026-09-10
**Status:** Accepted
**Phase:** 13 — task 13.08a

### What was added

`Vaelen/View/Net.h`: `RouteView` (32 bytes) and `ColonyView` (16), a `NetView`
holding both in stable order, `TakeNetView`, `RouteOf`, `RouteBetween`,
`ColonyIn` and `MeasureNetView`. Same promise as 13.01 and 13.07a - flat numbers,
no handle, no way back, and it outlives the world it was taken from.

Why it was needed: `RegionView::Roads` is a COUNT. It says a region is touched by
three routes and not which three, so a map drawn from a frame has fifty-two towns
on it and not one line between them. The whole economy of the world was
undrawable.

### The decision, and it is the interesting part

**It is not a field on `WorldView`.** Adding `std::vector<RouteView> Routes` to
the frame would have been the obvious move and would have been a trap:

> 13.02's `Delta` diffs a `WorldView` **region by region**. A vector of routes
> living inside that struct would be carried by the view, ignored by the diff,
> and silently missing from every screen rebuilt from a delta - right on the
> first frame and stale forever after.

The general rule, which is worth more than this file:

> **A structure the diff does not know about must not live inside the thing the
> diff claims to describe.** Either the diff learns it, or it lives outside where
> its absence is visible.

13.05's gate would not have caught it either: it compares a rebuilt screen to a
fresh frame and both would carry routes from the same source.

### The colony, and why the tool asks before founding one

VAELEN opens with the player owned, inside a mining colony. A view that cannot
say where the colony is cannot draw the place the game begins, so `ColonyView`
is here.

`Tools/Atlas` founds one only under `--colony`. Without the flag it declares no
colony types and adds no mining system, so the world it simulates is the world
every run before this change simulated - **verified, not asserted**: the default
run at 256 still reports frame `0xd0407d9684ab4f5a` and ground
`0x1faebda9e6c61a5b`, the digests published before this task existed.

### Two things measurement caught that reading would not have

- **A colony needs rock.** The first `--colony` run reported one colony, region
  42, zero hands, zero lifted. `MiningSystem` works the ORE seams of its region
  and that region had none worth taking, so the tool now puts the colony on the
  busiest region that has ore under it. With that: 5689 hands, 990 units lifted.
- **A system nobody added does nothing.** The run before that reported zero
  hands because the `MiningSystem` was never constructed - an edit whose anchor
  had moved, applied without an assertion that it had matched. The build was
  green, the tests were green, and the number was zero. Only running it said so.

## ADR-0122 — The years are the same years; what changes a run is when detail is asked for

**Date:** 2026-09-10
**Status:** Accepted
**Phase:** 13 — task 13.08c

### What was added

`--every N` on `Tools/Atlas`: keep a `WorldView` and a `NetView` every N years and
write them into the file, and a slider on the page that reads every number from
the chosen year. AELVOR at 256 with `--every 10` is 42 frames, 1.2 MB, and the
arc it shows is the point of the whole exercise:

```
year  10        992 alive     8 roads open     4 regions peopled
year 210     24 223 alive    22 roads open
year 250     31 872 alive    55 roads open
year 420    123 600 alive    78 roads open    55 regions peopled
```

### The size decision

A frame carries regions and routes and **not the ground**. The ground does not
change and a copy of 65536 tiles a frame would be the file. Regions and routes go
in as flat arrays with a documented stride rather than an object apiece: at a
frame a decade over four centuries that is the difference between a file a
browser opens and one it thinks about.

### The finding, which is worth more than the feature

The pre-history ran **inside** `PreHistory::Generate`, so the timeline could not
see into the first three hundred years - the most interesting three hundred, when
a world of nine hundred people becomes a world of twenty thousand. The fix looks
trivial: seed with `Generate(Config, 0, false)` and put every year through `Run`,
where a frame can be taken between them.

The first attempt did exactly that and **produced a different world**: 27564
alive against 36374, 46 regions peopled against 65, and no region simulated
person by person at all. Nothing about the years had changed. What had changed
was that `RequestDetail` now fired at year zero, where nobody has spread yet and
`Busiest()` finds no region worth detailing.

> **A run is not defined by which call ticks the clock. It is defined by when the
> world is asked to pay attention.** Move the tick and nothing happens; move the
> request and you get another world.

With detail requested after exactly `PreHistory` years, as before, the
restructured run reproduces the old one to the bit - frame `0x1ad9b6c934b65257`,
ground `0x8f7f4948f49b6e86`, network `0x7f0e8fbdef66b895` at 128, all three
unmoved. That is what makes it a restructure and not a change.

### Why the timeline cannot lie about the colony

The kept frames carry regions and routes. They do not carry colonies, so the only
year this page knows there is one is the year the run ended. The marker is drawn
in that year alone and the panel labels the row `Colony (at year 420)` - because
a mine drawn on the map at year 10, three centuries before anybody dug it, is
exactly the kind of confident wrong picture this whole layer exists to avoid.

### Verified, not assumed

Taking a view is const: the world does not know it happened. A run with `--every`
therefore simulates the run without it, and the digests say so rather than the
argument. `Tools/check_atlas_output.py` gained ten rules over the timeline - the
frames are in year order, the last one is the year the run ended, no row breaks
its stride, every frame's regions and routes are ones the document has, and each
frame's own totals add up - each proved to fire by `--self-test`, now 42 checks.
The CTest runs use `--every 5 --colony`, so CI exercises the tool the way it is
actually used rather than only its simplest shape.

## ADR-0123 — The world already knew how to tell its own history; nothing had ever asked it

**Date:** 2026-09-10
**Status:** Accepted
**Phase:** 13 — task 13.08d

### What was found

Phases 04 to 11 each built a chronicle listener and a describer: `PersonChronicle`,
`SocietyChronicle`, `EconomyChronicle`, `PoliticsChronicle`, `MilitaryChronicle`,
`LifeChronicle`, `ColonyChronicle`. Every one turns the events that matter into
`RecordInfo` documents and every one can put a sentence on them **in the words of
the world**. They are covered by tests, they are green in eleven gates, and until
today nothing outside a test had ever read one.

The wiring is three listeners and one describer, because the topmost layer speaks
for every layer under it (06.07). What comes out:

```
Year 0, age of Divik: the Oldegedim first settled Edavaken.
Year 21, age of Ubu: a great eruption struck Miogu.
Year 21, age of Ubu: The road from Kiodanam to Entam was opened.
Year 419, age of Okerdun: Kudihumho was enslaved by debt in Edavaken.
Year 419, age of Okerdun: Arord was freed by manumission in Edavaken.
```

Named cultures, named places, named people, named eras, dated. AELVOR at 256
remembers **8158 things** over four centuries.

### What the tally says about the world

```
freed 1697 · died 1628 · enslaved 1473 · married 1114 · roads 937 · towns 71 · settled 59
```

After death, **bondage is the most recorded fact of this world** - three thousand
one hundred and seventy entries about people being owned and people ceasing to
be owned. Nobody wrote that. The README's premise ("the player starts enslaved")
is not a story laid over the simulation; it is what the simulation does most.

### The decisions

- **`--chronicle` is opt-in, and that is not caution.** The listeners create
  `RecordInfo` entities: a world nobody asked to remember carries no memory. With
  the flag off, the digests are the digests of every run before this - frame
  `0x1ad9b6c934b65257`, ground `0x8f7f4948f49b6e86`, network `0x7f0e8fbdef66b895`
  at 128, checked rather than assumed. With it ON they are also unchanged, which
  is the stronger result: **the chronicle observes without touching.**
- **The tool walks the records rather than calling `ExportChronicleWithEconomy`.**
  The kernel's exporter writes the same sentences as one block of text, and a
  page that puts a line on a timeline needs the year and the region of each. Same
  describer, one level lower.
- **Economy is the top describer, not Politics.** `PoliticsContext` needs law,
  reach, succession, faction and diplomacy types this tool does not declare.
  Roads and towns therefore get their own sentences and a polity's rise gets the
  plainer one - stated here rather than left as a silent limit.
- **JSON strings are escaped by the rules.** The chronicle is written by the
  world, so it holds whatever the namer of a place put in it. `Json::Str` escapes
  quotes, backslashes and control bytes and passes UTF-8 through.

### What the page says out loud

Person-level events exist only where the world simulates person by person - one
region of a hundred and twenty-six - so the last decades of the panel are full of
one town's marriages and extinct houses. That is not a hole in the record; it is
the shape of the LOD design, and the page carries a line saying so rather than
letting the reader conclude the rest of the world is empty.

### The rule this leaves behind

> Eleven phases of capability were built and verified and never used. A test
> proves a thing works; only a reader proves it is worth anything. **Ask the
> systems you already have what they know before building another one.**

## ADR-0124 — An age of the world ended because a volcano killed four hundred and eighty-nine people

**Date:** 2026-09-10
**Status:** Accepted
**Phase:** 13 — task 13.08e

### The claim, and where it had been sitting

The README says the simulation is the source of truth and that **systems cause
events**. Every event carries a `Cause`, `History::CauseChain` has walked those
causes since Phase 03, and `ExportWhyWithEconomy` has been able to write the
chain out in words since 06.07. Nothing had ever asked.

`--why` asks. For every chronicled record it walks the chain and describes each
link with the same describer the line itself used:

```
Year 342: Grain could not be had in Zakru.
   because  Zakru harvested 102 of grain.
     because  a drought struck Zakru.
       because  omens of drought were seen over Zakru.

Year 325: the age of Oldiss ended.
   because  a terrible eruption struck Wadumfu and 489 died.
     because  omens of eruption were seen over Wadumfu.
```

Four systems wrote those four lines and none of them knew about the others. That
is the whole thesis of the project, and it was in the event log unread.

### Why the chain is walked over the LOG and not the chronicle

`CauseChain` reads `World::Log()`. A cause can therefore be an event **nobody
thought worth remembering** - the harvest behind the hunger is not history, the
drought behind the harvest is. Walking the records instead would give a chain
with holes in exactly the places that explain anything.

### What it costs, measured

At 128, 356 of 3295 records have a cause and the deepest chain is three links;
the file grows from 637 KB to 727 KB. At 256, 388 of 8158, and 2.2 MB in total.
Depth is capped at 8 - a cycle in the causes would otherwise be a hang, and a
cap that is never reached costs nothing.

### A duplicate that was not one

The chronicle showed pairs at year 0: *"the Oldegedim first settled Edavaken"*
and *"the Oldegedim settled Edavaken"*, and 14 later lines that read exactly the
same as another line of the same year and region. Both looked like ADR-0120's
twin roads.

Emitting each record's event id settled it: **8158 ids for 8158 lines, all
distinct.** No event is recorded twice. The year-0 pairs are two events, and the
cause chain says so itself - the second is *because* the first. The 14 others are
genuinely different events that read alike because two people in a region of
seven thousand were given the same generated name.

> The cost of checking was one field in a JSON file. The cost of not checking
> would have been an ADR proposing a fix to something that was never broken -
> and this session had already written two ADRs about real duplicates, which is
> exactly the state of mind in which the third one gets invented.

### Verified

`Tools/check_atlas_output.py` gained three rules: no event id repeats, and a
`because` that is present is a non-empty list of non-empty lines - because an
empty cause list is a worse lie than a missing one, saying "this was asked and
nothing came back". Each proved to fire by `--self-test`, now 50 checks. The
CTest atlas runs use `--why`, so CI asks the world why on every push.

## ADR-0125 — Two suspected defects in a row, and neither was one

**Date:** 2026-09-10
**Status:** Accepted (a note on method, not a change)
**Phase:** 13 — while gathering evidence for ADR-0120

### What happened

Reading the chronicle for evidence about the twin roads, two things looked wrong
within ten minutes of each other. Neither was.

**"The chronicle never records a road closing."** 275 lines saying a road was
opened, zero saying one was closed, and 49 of the closed routes had carried more
than the hundred units that makes a closing history. It looked like the
settlement half of a rule working and the road half not.

The describer says **"fell out of use"**, not "closed". There are 541 of them -
twice as many as the openings. The grep was wrong, not the world.

**"Roads open in year 0, before anyone has spread."** 115 chronicle lines dated
year 0, many of them roads. On a world whose first year holds nine hundred people
in four regions, that reads as a clock that has not started.

It has. `Year = Tick / TicksPerYear`, so every tick of the first year is year 0,
and the cultures are seeded with stock, so price gaps and therefore roads exist
in the first year. The date is right and the intuition about it was wrong.

### Why it is worth an ADR

Because of what was in the way of noticing. This session had already found and
written up two genuine duplicate-entity defects - ADR-0118's settlements that
cannot be abandoned and ADR-0120's twin roads. **That is the exact state of mind
in which a third one gets invented**: the pattern is fresh, the tooling is new,
the data is unfamiliar, and every oddity looks like a member of a family you have
just learned to recognise.

Both were settled in under a minute - one by reading the describer, one by
dividing by the ticks in a year. The cost of checking was nothing. The cost of
not checking would have been two ADRs proposing fixes to working code, in a
document whose whole value is that its entries are true.

> A new instrument shows you things you have never seen. Most of them were always
> there and are fine. **Before reporting what an instrument shows, find out what
> the instrument does** - and the more recently you were right about something
> that looked the same, the more carefully you should look.

The one thing that survived the checking is in ADR-0120: 105 of 275 chronicled
first-openings are false, because a twin entity starts its `Openings` count at
one. That one is real, and it is real because it was checked the same way.

## ADR-0126 — AELVOR reaches an equilibrium and holds it for nine hundred years

**Date:** 2026-09-10
**Status:** Accepted
**Phase:** 13 — a long-duration look, done while CI ran

### The question nobody had asked

Every gate in this project stops at four or five centuries. Nothing had ever run
this world further, so nothing knew whether it settles, starves or runs away.
`Tools/Atlas` made asking cost one command: AELVOR at 256 for **1500 years**.

```
year   50     2 405 alive    4 regions peopled   16 towns   14 roads open
year  250    31 872 alive   22                   36         55
year  500   177 957 alive   82                   60         83
year  600   202 373 alive   84                   60         82
year  900   210 105 alive   84                   66         86
year 1200   208 336 alive   84                   65         90
year 1500   203 791 alive   84                   65         91
```

**It settles.** Population climbs for six centuries, reaches about two hundred
and five thousand, and then holds within three per cent of that for the next nine
hundred years. Regions peopled stops at 84 of 126 and never moves again. Towns
sit between 60 and 66. Roads breathe between 79 and 91.

Nothing was capped to make that happen. It is what the systems do when left
alone: a carrying capacity emerging from land, harvests, hunger and death rather
than from a rule that says two hundred thousand.

### What it is evidence for, and what it is not

- **It is evidence of stability.** Fifteen centuries with assertions compiled in,
  43267 chronicle records, no crash, no runaway, no collapse, 275 seconds.
- **It is one seed.** A world that settles on `0x41454c564f52` says nothing about
  a world that would not on another. This is not a gate and is not written down
  as one.
- **It is the first thing this project has learned about its own long run**, and
  it corrected an error inside the hour - see ADR-0118, where a claim built
  entirely on measurements taken at year 420 turned out to describe a transient.

### The rule it leaves

> Every gate in this project stops at four or five centuries, so **every claim
> made from a gate is a claim about a young world** - and nothing inside the data
> says so. When a finding is about what the world IS rather than what it did once,
> run it further before writing it down.

## ADR-0127 — "126 regions" overstates the world by forty-two slivers

**Date:** 2026-09-10
**Status:** Accepted (an observation about the generator, not a change)
**Phase:** 13 — from the fifteen-century run of ADR-0126

### The question

AELVOR's equilibrium settles at **84 regions peopled of 126**, and never moves
again in nine hundred years. A third of the world permanently empty invites the
conclusion that a third of the land is uninhabitable, or that something stops
people spreading into it.

### The answer is size, not land

```
regions never peopled in 1500 years   42     median size    3 tiles
regions peopled at some point         84     median size  301 tiles
```

A hundredfold difference. Biome is a red herring: boreal forest is 55 per cent of
the never-peopled set, but it is also 15 per cent of the peopled one - boreal
regions that are BIG get people. The forty-two are offcuts of the region
partition, three tiles apiece, mostly along a coastline where a partition of the
land naturally leaves fragments.

Nothing is wrong. A Voronoi-shaped partition produces a tail of slivers, and a
sliver cannot hold a population that needs to eat.

### What it does mean, and it touches the page

**Any statistic taken per region is diluted by forty-two fragments.** "55 regions
peopled of 126" reads as *most of this world is empty*, when what it says is *55
of the 84 regions that could ever hold anyone*. The viewer reports the world's
own numbers and is right to; a reader who does not know about the tail will draw
the wrong conclusion from them anyway.

No threshold is being introduced here. "A region under N tiles is not real" would
be a rule invented to make a number read better, and this project has spent
enough ADRs on the difference between what the world says and what would be
convenient. The observation is written down instead, where the next person
reading a per-region statistic can find it.

### Where it came from

Nobody asked this question in thirteen phases, because the answer only becomes
visible when a world is run long enough to STOP changing. At year 420 the count
was still climbing and 55 of 126 looked like a world filling up. It had already
finished.

---

## ADR-0128 — A thousand lines of viewer that nothing compiled

**Status:** ACCEPTED · **Date:** Phase 13 · **Task:** 13.07b (retrofit)

### The situation

`Tools/Viewer/Atlas.html` is 1044 lines: markup, CSS, and a script that paints
tiles, draws roads, and walks cause chains. Every other line in this repository
is either compiled by two compilers and run by CTest, or checked by a Python
script that CTest runs. The page was neither. It was checked by me opening it
and looking, which is how it broke three times in one day:

- a name assigned but declared nowhere (`toldEl`), silent until that panel opened;
- a `getElementById` for an id the markup spelled differently, likewise silent;
- a literal U+2009 written into the script where an HTML entity belonged,
  which is the mojibake the user saw and reported.

None of the three is a hard problem. All three are invisible until a human
opens the page, notices, and says so. That is the definition of a check that
should not be a human's job.

### The decision

`Tools/check_viewer.py` reads the page without a browser. Seven rules, chosen
because each one names a class of failure that actually happened or would
plainly happen next:

1. the page has its title and its data placeholder;
2. `node --check` parses the extracted script;
3. every `getElementById("X")` has a matching `id="X"` in the markup;
4. `CONTROLS` keys and `show` keys agree, in both directions;
5. nothing the script *renders* is non-ASCII (comments are exempt);
6. no assignment to a name declared nowhere in the file;
7. no CSS `var(--x)` whose token is undefined on bare `:root`.

Rule 7 is not from a bug we hit; it is from ADR-0113's theme discipline — a
token defined only inside a `@media` or `[data-theme]` block renders one
theme's text on the other theme's ground, and that failure is invisible to
whoever is not using that theme.

Three CTest entries: `Viewer.Page` checks the template, `Viewer.SelfTest`
proves each rule still fires against ten deliberate breaks, and `Viewer.Built`
runs `build_viewer.py` over a world the atlas wrote and checks the *result* —
because a sound template can still be inlined into a broken page, and that is
the one failure the first two cannot see.

### What it does not catch, which matters more than what it does

Rule 6 finds a name declared *nowhere*. It does not find a name declared in
another function's scope and used outside it. That is exactly the `html is not
defined` bug, and I claimed this checker would have caught it. It would not.

Catching it needs scope analysis, which needs a real JavaScript parser. Adding
one to guard a thousand-line page is a larger dependency than the page. So the
limit stands and is written first in the file's own header, where the next
person reads it before trusting the green.

Nor does any of this prove the page *looks* right: the checker never renders,
never measures a layout, never runs the script against a document. A page can
pass all seven rules and paint nothing. What the rules buy is that the three
classes of silent breakage that cost real time today cannot recur unnoticed.

### Considered

**Headless browser (Playwright).** Would catch scope errors, would catch a
blank canvas, would catch layout. Also pulls a browser into a kernel repo whose
entire point is that it builds with a compiler and CMake and nothing else, and
whose CI already runs 9 jobs. Rejected on cost, not on merit — if the viewer
grows into something the project depends on, this is the upgrade path.

**Leave it unchecked.** Honest for a throwaway. The page is not a throwaway:
it is how a person sees AELVOR without an engine, and it is the argument that
`WorldView` holding no pointer was worth the trouble.

### Every one of these was watched failing

A rule nobody has seen fire is a rule nobody has. So:

- each of the ten self-test breaks is **aimed at its own rule**, and being
  caught by a different one is its own failure - proved by mis-aiming one on
  purpose, which reported *"syntax error: caught, but by the wrong rule"*;
- the parse rule is skipped, not faked, when `node` is absent, and both the
  summary line and the self-test count say so;
- `Viewer.Built`: the built page from a template with one mis-spelled `id`
  fails the check with exit 1, and the CMake wrapper exits 1 when the build
  step itself fails. The wrapper's second failure path is the same three lines
  as the first and was not separately exercised - said here rather than left to
  be assumed.
