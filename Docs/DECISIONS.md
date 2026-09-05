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
