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

PHASE       : 01 — CORE SIMULATION
TASK        : 01.04 — SIMULATION CLOCK & CALENDAR
STATUS      : VALIDATED (headless) / UNVERIFIED (engine)

PROGRESS
████████████░░░░░░░░░░░░ 50%

CURRENTLY
→ 01.03 and 01.04 closed: ISystem, Scheduler (stable topological order, LOD periods,
  per-system streams), SimClock and the AELVOR calendar

COMPLETED
✓ Phase 00 — FOUNDATION (CI 9/9)
✓ 01.01 Entity handles & registry (16 tests)
✓ 01.02 Component storage (15 tests)
✓ 01.03 Systems & tick scheduler (8 tests)
✓ 01.04 Simulation clock & calendar (4 tests)

NEXT
→ 01.05 Event bus & event log (typed events, next-tick delivery, causal links)
→ 01.06 Persistence interfaces & snapshot

FILES
+ Source/VaelenSim/Public/Vaelen/Sim/{SimClock.h, System.h}
+ Source/VaelenSim/Private/Scheduler.cpp
+ Tests/Sim/{Test_SimClock.cpp, Test_Scheduler.cpp}

TESTS
✓ Core 133 (108 without asserts) + Sim 43 (41 without asserts); ctest 23/23 in all six Linux presets
✓ Purity: 24 files, 0 violations
⚠ Unreal Build Tool: VaelenSim engine files UNVERIFIED

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
| `Public/Vaelen/Sim/SimApi.h`, `EntityHandle.h`, `EntityRegistry.h`, `ComponentType.h`, `ComponentPool.h`, `ComponentStore.h`, `SimClock.h`, `System.h`, `Private/EntityRegistry.cpp`, `ComponentType.cpp`, `ComponentStore.cpp`, `Scheduler.cpp` | VALIDATED (Phase 01) — integration and long-duration tests arrive with 01.07 / 01.08 |
| `Private/VaelenSimModule.cpp`, `VaelenSim.Build.cs` | UNVERIFIED (requires UE5) |

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

Per-suite counts: Assert 33, CoreTypes 1, Harness 5, Hash 15, Ids 19, Log 23, LogFloor 1, Random 29, Version 7 (133 tests with assertions, 108 without). CTest entries: `Kernel.Purity`, `Kernel.PuritySelfTest`, `Core.Assert`, `Core.CoreTypes`, `Core.Harness`, `Core.Hash`, `Core.Ids`, `Core.Log`, `Core.LogFloor`, `Core.Random`, `Core.Version`, `Core.Registry`, `Core.Shuffled`, `Core.Reversed` (14 entries). Sim suites: EntityHandle 3, EntityRegistry 13, ComponentType 4, ComponentPool 8, ComponentStore 3, SimClock 4, Scheduler 8 (43 tests, 2 017 947 checks; 41 tests without assertions); CTest entries `Sim.EntityHandle`, `Sim.EntityRegistry`, `Sim.ComponentType`, `Sim.ComponentPool`, `Sim.ComponentStore`, `Sim.SimClock`, `Sim.Scheduler`, `Sim.Registry`, `Sim.Shuffled` (23 entries in total).

### Tools/ and CI

| File | STATUS |
|---|---|
| `Tools/check_kernel_purity.py` | VALIDATED (Phase 00): self-test 36 checks, kernel scan 12 files, 0 violations, 2 exemptions (the `long long` aliases) |
| `Tools/kernel_modules.txt` | lists `VaelenCore` |
| `.github/workflows/kernel-ci.yml` | All 9 jobs green on GitHub (run 5): six Linux presets, `format`, Windows MSVC, macOS |

## Verified here

Toolchain: clang++ 18.1.3, g++ 13.3.0, CMake 3.28.3, Ninja 1.11.1, Python 3.11.15, clang-format 18.1.3, Linux x86_64. Every preset was configured, built and tested with
`cmake --preset`, `cmake --build --preset`, `ctest --preset` into `out/build/<preset>`:

| Preset | Build | `ctest` | `VaelenCoreTests` |
|---|---|---|---|
| linux-clang-debug | 0 warnings | 14/14 passed | 133 run, 133 passed, 21914 checks |
| linux-gcc-debug | 0 warnings | 14/14 passed | 133 run, 133 passed, 21914 checks |
| linux-clang-release | 0 warnings | 14/14 passed | 133 run, 133 passed, 21914 checks |
| linux-gcc-release | 0 warnings | 14/14 passed | 133 run, 133 passed, 21914 checks |
| linux-clang-noasserts | 0 warnings | 14/14 passed | 108 run, 108 passed, 21701 checks |
| linux-gcc-noasserts | 0 warnings | 14/14 passed | 108 run, 108 passed, 21701 checks |

GitHub Actions run 5 (commit `71bad2d`, https://github.com/Thomas10112/vaelen/actions/runs/33977296696): all 9 jobs green - six Linux presets, clang-format 18, Windows MSVC 19.44 (`windows-msvc-debug`, 14/14 CTest entries), macOS 15 AppleClang (`macos-debug`, 14/14).

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
