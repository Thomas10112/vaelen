# VAELEN Build Status

Living document. Every number below was produced by the commands listed in
[Verified here](#verified-here) on 2026-09-05, on this repository's working tree (branch
`claude/vaelen-master-prompt-aw7zqj`). Update it whenever a task changes state; never record a
result that was not observed.

Labels: `VALIDATED` / `PROTOTYPE` / `INCOMPLETE` are the project's labels; `UNVERIFIED` marks
engine-only files that the headless pipeline cannot compile.

## BUILD STATUS

```
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
VAELEN BUILD STATUS
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

PHASE       : 00 — FOUNDATION
TASK        : 00.04 — DOCUMENTATION
STATUS      : IN PROGRESS

PROGRESS
███████████████████░░░░░ 80%

CURRENTLY
→ README.md and Docs/STATUS.md

COMPLETED
✓ 00.01 Project architecture: UE 5.6 skeleton, headless CMake build, presets, CI workflow, test harness
✓ 00.02 Core test suites: 7 suites, 112 tests, 21 785 checks, 8/8 CTest entries (clang 18, gcc 13)
✓ 00.03 Kernel purity checker: 12 files, 0 violations; self-test 32 checks, 0 failed
✓ Docs/ARCHITECTURE.md, Docs/CONVENTIONS.md, Docs/DECISIONS.md

NEXT
→ 00.05 Adversarial review of Phase 00, fixes, validated commit (planned)
→ Phase 01 — CORE SIMULATION (planned)

FILES
+ README.md
+ Docs/STATUS.md

TESTS
✓ Core.* 112/112, clang++ 18.1.3, Debug, assertions on
✓ Core.* 112/112, g++ 13.3.0, Debug, assertions on
✓ Core.* 112/112, clang++ and g++, RelWithDebInfo
✓ Kernel.Purity: 12 files, 0 violations
⚠ VAELEN_ENABLE_ASSERTS=OFF (not a CI configuration): Harness.AssertCaptureDoesNotAbort fails, 88/89
⚠ Unreal Build Tool (UE 5.6): not run — every Unreal-facing file is UNVERIFIED

BLOCKERS
None (no UE 5.6 installation here: the Unreal build cannot be verified in this environment)
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
```

## Phase 00 task breakdown

Task numbers follow the breakdown used for this phase in the project's task tracking.

| Task | Content | State |
|---|---|---|
| 00.01 | `Vaelen.uproject`, targets, modules `VaelenCore` and `Vaelen`, root `CMakeLists.txt`, `CMakePresets.json`, `kernel-ci.yml`, `Tests/Harness` | Done: headless build VALIDATED (clang 18 + gcc 13); UBT side UNVERIFIED |
| 00.02 | `Tests/Core/Test_{Assert,Harness,Hash,Ids,Log,Random,Version}.cpp` | Done: 112/112 on both compilers |
| 00.03 | `Tools/check_kernel_purity.py`, `Tools/kernel_modules.txt`, CTest entry `Kernel.Purity` | Done: 0 violations, self-test passes |
| 00.04 | `Docs/ARCHITECTURE.md`, `Docs/CONVENTIONS.md`, `Docs/DECISIONS.md`, `README.md`, `Docs/STATUS.md` | In progress (this document) |
| 00.05 | Adversarial review of Phase 00, fixes, validated commit | Planned |

## File status

STATUS labels are copied verbatim from each file's header comment. "none" means the file has no
`// STATUS:` line (allowed for `.cpp`, build scripts and CMake files; required for headers by
purity rule R5).

### Source/

| File | STATUS line |
|---|---|
| `Source/Vaelen.Target.cs` | none (C# build script) |
| `Source/VaelenEditor.Target.cs` | none (C# build script) |
| `Source/VaelenCore/VaelenCore.Build.cs` | none (C# build script) |
| `Source/VaelenCore/CMakeLists.txt` | none |
| `Source/VaelenCore/Public/Vaelen/Core/CoreTypes.h` | VALIDATED (Phase 00) |
| `Source/VaelenCore/Public/Vaelen/Core/Version.h` | VALIDATED (Phase 00) |
| `Source/VaelenCore/Public/Vaelen/Core/Assert.h` | VALIDATED (Phase 00) |
| `Source/VaelenCore/Public/Vaelen/Core/Log.h` | VALIDATED (Phase 00) |
| `Source/VaelenCore/Public/Vaelen/Core/Hash.h` | VALIDATED (Phase 00) |
| `Source/VaelenCore/Public/Vaelen/Core/Random.h` | VALIDATED (Phase 00) |
| `Source/VaelenCore/Public/Vaelen/Core/Ids.h` | VALIDATED (Phase 00) |
| `Source/VaelenCore/Private/Assert.cpp` | none |
| `Source/VaelenCore/Private/Log.cpp` | VALIDATED (Phase 00) - covered by Tests/Core/Test_Log.cpp |
| `Source/VaelenCore/Private/Random.cpp` | none |
| `Source/VaelenCore/Private/Ids.cpp` | none |
| `Source/VaelenCore/Private/Version.cpp` | none |
| `Source/VaelenCore/Private/VaelenCoreModule.cpp` | UNVERIFIED - not compiled in the headless CI (requires UE5). |
| `Source/Vaelen/Vaelen.Build.cs` | none (C# build script) |
| `Source/Vaelen/Public/Vaelen.h` | UNVERIFIED - not compiled in the headless CI (requires UE5). |
| `Source/Vaelen/Private/Vaelen.cpp` | UNVERIFIED - not compiled in the headless CI (requires UE5). |
| `Source/Vaelen/Private/VaelenLogSink.h` | UNVERIFIED - not compiled in the headless CI (requires UE5). |
| `Source/Vaelen/Private/VaelenLogSink.cpp` | UNVERIFIED - not compiled in the headless CI (requires UE5). |

### Tests/

| File | STATUS line | Suite / tests |
|---|---|---|
| `Tests/CMakeLists.txt` | none | registers `Kernel.Purity` |
| `Tests/Core/CMakeLists.txt` | none | builds `VaelenCoreTests`, one `Core.<Suite>` entry per file |
| `Tests/Harness/VaelenTest.h` | VALIDATED (Phase 00) | - |
| `Tests/Harness/TestMain.cpp` | none | runner |
| `Tests/Core/Test_Assert.cpp` | VALIDATED | Assert, 28 tests (5 when assertions are disabled) |
| `Tests/Core/Test_Harness.cpp` | none | Harness, 2 tests |
| `Tests/Core/Test_Hash.cpp` | VALIDATED | Hash, 15 tests |
| `Tests/Core/Test_Ids.cpp` | VALIDATED | Ids, 17 tests |
| `Tests/Core/Test_Log.cpp` | VALIDATED | Log, 20 tests |
| `Tests/Core/Test_Random.cpp` | VALIDATED (Phase 00) - all tests pass warning-free with clang++ 18 and g++ 13. | Random, 23 tests |
| `Tests/Core/Test_Version.cpp` | VALIDATED | Version, 7 tests |

### Tools/ and CI

| File | STATUS line |
|---|---|
| `Tools/check_kernel_purity.py` | VALIDATED (Phase 00) - self-tested with --self-test; runs in CTest as Kernel.Purity. |
| `Tools/kernel_modules.txt` | none (lists `VaelenCore`) |
| `.github/workflows/kernel-ci.yml` | none; jobs: Linux matrix of the four `linux-*` presets, Windows `windows-msvc-debug`, macOS `macos-debug` |

Total: 112 tests, 21 785 checks; CTest entries `Kernel.Purity`, `Core.Assert`, `Core.Harness`,
`Core.Hash`, `Core.Ids`, `Core.Log`, `Core.Random`, `Core.Version` (8).

## Verified here

Environment: Linux x86_64 (kernel 6.18), clang++ 18.1.3, g++ 13.3.0, cmake 3.28.3, Ninja 1.11.1,
Python 3.11.15. All builds use the repository defaults: `-Werror` with the full warning list from
`CMakeLists.txt`, `-fno-exceptions -fno-rtti`, `VAELEN_BUILD_TESTS=ON`. Build directories are
private (`out/build/agent-doc-readme-*`) so the fixed preset directories `out/build/<preset>`
were not touched.

| Configuration | Commands | Result |
|---|---|---|
| clang++, Debug, `VAELEN_ENABLE_ASSERTS=ON` (default) | `cmake -S . -B out/build/agent-doc-readme-clang -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=clang++` / `cmake --build ...` / `VaelenCoreTests` / `ctest --test-dir ... --output-on-failure` | build 0 warnings; `112 run, 112 passed, 0 failed, 21785 checks`; ctest 8/8 |
| g++, Debug, asserts ON | same with `-DCMAKE_CXX_COMPILER=g++` into `agent-doc-readme-gcc` | build 0 warnings; 112/112, 21785 checks; ctest 8/8 |
| clang++, RelWithDebInfo | same with `-DCMAKE_BUILD_TYPE=RelWithDebInfo` into `agent-doc-readme-clang-rel` | build 0 warnings; 112/112; ctest 8/8 |
| g++, RelWithDebInfo | same into `agent-doc-readme-gcc-rel` | build 0 warnings; 112/112; ctest 8/8 |
| clang++, Debug, `-DVAELEN_ENABLE_ASSERTS=OFF` | into `agent-doc-readme-clang-noassert` | build 0 warnings; `89 run, 88 passed, 1 failed`; ctest 7/8 (`Core.Harness` fails, see discrepancy 1) |
| g++, Debug, asserts OFF | into `agent-doc-readme-gcc-noassert` | identical to clang: 88/89, ctest 7/8 |
| Purity checker | `python3 Tools/check_kernel_purity.py --root . --verbose` | `[purity] 12 files, 0 violations` (R0-R7 all 0, 0 exempted) |
| Purity self-test | `python3 Tools/check_kernel_purity.py --self-test` | `32 checks, 0 failed` |
| Preset names | `cmake --list-presets=all` | configure: `linux-clang-debug`, `linux-clang-release`, `linux-gcc-debug`, `linux-gcc-release`; build/test: those plus `macos-debug`, `windows-msvc-debug`, `windows-msvc-release` (host conditions hide the non-Linux configure presets) |

The explicit `cmake -S/-B` Debug commands use the same generator, build type, compiler and
default cache variables as the `linux-clang-debug` / `linux-gcc-debug` presets; the RelWithDebInfo
commands match the `linux-*-release` presets.

## Unverified

Not compiled, run or observed from this repository. Statements about these are claims of the
files themselves, not results.

- The preset flow as written (`cmake --preset X`, `cmake --build --preset X`, `ctest --preset X`)
  was not invoked here: presets configure into the shared `out/build/<preset>` directories.
  Only the equivalent explicit commands above were run.
- Unreal Build Tool build of `VaelenCore` and `Vaelen` with UE 5.6; Visual Studio 2022 project
  generation; launching the editor; the `VAELEN ... module started` log line. Every file marked
  `UNVERIFIED` above, plus `Vaelen.uproject`, the `.Target.cs` / `.Build.cs` rules and
  `Config/Default*.ini`, has been read but never compiled or executed.
- MSVC (`windows-msvc` presets, `/W4 /WX /GR- _HAS_EXCEPTIONS=0`) and AppleClang (`macos-debug`).
- Any run of `.github/workflows/kernel-ci.yml` on GitHub Actions (no run has been observed from
  this repository).
- Whether UBT's own warning settings accept the kernel sources, and whether the UBT-generated
  `VAELENCORE_API` is compatible with every use in the headers.
- Integration and long-duration tests: none exist (no simulation loop yet); planned from Phase 01.

## Discrepancies found while writing this document

Reported, not fixed (the files belong to other tasks).

1. `Tests/Core/Test_Harness.cpp` (`Harness.AssertCaptureDoesNotAbort`, lines 14-22) expects
   `VAELEN_ENSURE(1 == 2)` to report one Ensure failure, but with `VAELEN_ENABLE_ASSERTS=OFF`
   `VAELEN_ENSURE` expands to `Detail::EnsureResult(...)` and reports nothing (`Assert.h`
   line 146). The test is not guarded by `#if VAELEN_ASSERTS_ENABLED` and fails in that
   configuration with both compilers (observed: `actual: 0 expected: 1`). CI never builds with
   assertions off, so CI stays green.
2. `Tests/Core/Test_Harness.cpp` carries no `// STATUS:` line (every other test file does).
3. The seven discrepancies listed in `Docs/ARCHITECTURE.md` section 11 were re-checked against
   the sources and still hold, except item 2 (`Config/DefaultEditor.ini` refers to a README):
   `README.md` now exists.
4. `Content/` is an empty directory in this working tree; an empty directory is not carried by
   git, so a fresh clone will not contain it until Unreal content is added.

## How to refresh this document

```
cmake -S . -B out/build/<dir> -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=<clang++|g++>
cmake --build out/build/<dir>
out/build/<dir>/Tests/Core/VaelenCoreTests            # summary line: N run, P passed, F failed, C checks
out/build/<dir>/Tests/Core/VaelenCoreTests --list     # per-suite counts
ctest --test-dir out/build/<dir> --output-on-failure  # CTest entries
python3 Tools/check_kernel_purity.py --root . --verbose && python3 Tools/check_kernel_purity.py --self-test
grep -rn "STATUS:" Source Tests Tools --include=*.h --include=*.cpp --include=*.py   # file status table
```
