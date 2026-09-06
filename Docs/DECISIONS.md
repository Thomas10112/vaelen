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
