# Engine hand-off: what the machine with UE 5.6 does, and what it does not

**Who this is for.** The session running on a computer with Unreal Engine 5.6
installed. Everything in this repository is built and verified in a Linux
container with clang, gcc, cmake and ninja and **no engine at all**. That
container cannot build the engine-facing modules. This file is what it hands
over.

**Where this stands on 2026-09-15.** Phase 13 is CLOSED - its gate passed on
2026-09-14, the editor open on AELVOR at 256 at 100 fps, every figure the engine
printed equal to the headless kernel's. What is handed over now is **Phase 14**:
two modules that have never been compiled, and the month of play that closes the
phase. Phase 13's sections are kept below as the record of how the last hand-off
went.

## The standing rule, first, because it is the one that matters

**Do not fix anything inside these directories:**

```
Source/VaelenCore  Source/VaelenSim  Source/VaelenPopulation  Source/VaelenSociety
Source/VaelenEconomy  Source/VaelenPolitics  Source/VaelenMilitary
Source/VaelenInfrastructure  Source/VaelenColony  Source/VaelenPlayer
Source/VaelenGameplay  Source/VaelenView  Source/VaelenRun
```

They are validated by the headless CI: fourteen phase gates, six Linux presets, a
Windows MSVC leg and a macOS AppleClang leg, 175 CTest entries. A fix applied on the engine machine
is a fix nothing in that matrix has seen, and the frozen digests of eleven gates
are exactly the kind of thing a well-meant edit moves.

**Report the errors. Do not repair them.** Paste the compiler output, name the
file and the line, and the headless side fixes it where the tests are.

The one thing the engine machine owns outright is `Source/Vaelen/` (the primary
game module), `Config/`, `Vaelen.uproject`, and whatever `VaelenPresentation`
turns out to be.

## What is actually there to build

Seventeen `*.Build.cs` files: thirteen kernel modules and four Unreal ones
(`Vaelen`, `VaelenPresentation`, `VaelenGame`, `VaelenUI`). `Vaelen.uproject`
declares them all, with `EngineAssociation 5.6`.

Thirteen of the seventeen were compiled and linked as DLLs by UBT on 2026-09-14
(task 13.06, `Result: Succeeded`, 157 actions, MSVC 19.51, Win64 Development
Editor) and the phase-13 gate ran the editor on AELVOR at 256 afterwards. Those
marks are paid for.

**What UBT has never seen** is exactly what carries `STATUS: UNVERIFIED` today,
and the list is short enough to print:

```
Source/VaelenRun/VaelenRun.Build.cs          Source/VaelenRun/Private/VaelenRunModule.cpp
Source/VaelenGame/   (Build.cs + 4 files)    Source/VaelenUI/   (Build.cs + 7 files)
```

`VaelenRun` is the thirteenth kernel module and was written after that build, so
its rules file and its one Unreal-facing translation unit have not been read by
UBT either - though everything under its `Public/` and `Private/` has compiled
under gcc, clang, MSVC and AppleClang through CMake and is covered by the CTest
suite. The mark is accurate rather than pessimistic: UBT has its own rules about
module dependencies, IWYU and what a `Build.cs` may say, and UnrealHeaderTool
generates code that no compiler here ever sees.

## 13.06 - the first UBT build of all twelve kernel modules

The whole task. Open the project in UE 5.6, or run the editor target from the
command line, and build.

**What to expect, and it is not "it just works".** UBT enforces things CMake
does not: dependency ordering declared in `Build.cs` files, include-what-you-use
on public headers, `PCHUsage`, and unity builds. Every kernel `Build.cs` sets
`PCHUsage = NoPCHs` and `bUseUnity = false` deliberately, so the likely failures
are missing entries in `PublicDependencyModuleNames` and headers that compile
only because CMake's include paths are flatter than UBT's.

**What to report back**, per failing module: the module name, the first error
verbatim with its file and line, and whether it is a missing dependency, a
missing include, or something else. A list of module names with no output is not
a report.

**What clears the mark.** When a module builds under UBT, its header's
`STATUS: UNVERIFIED` becomes `VALIDATED under UBT (UE 5.6, <date>, editor)` -
and only then. A file marked VALIDATED for having looked right is the one
mistake this phase must not make; thirteen phases of accurate UNVERIFIED marks
are worth more than one optimistic VALIDATED.

## 13.07, 13.08, 13.09 - the module and the picture

- **13.07** `VaelenPresentation`, and the world drawn as regions on a map at
  all. Verified by a screenshot.
- **13.08** a person, a colony and a road drawn from the view of 13.01. The data
  is already there and already tested: `View::TakeView` fills a `WorldView` of
  `RegionView`s with centroid tile, biome, elevation, head count, settlement,
  roads and how finely to draw it. Nothing needs inventing on the engine side
  except the drawing.
- **13.09** the editor open on AELVOR at 256, a century running, and the frame
  rate written down. Measured on the machine that has the engine.

## PHASE 14 - what is waiting for you now

Two modules and one month of play. Nothing else in the repository is blocked on
anything but these.

### 14.08 and 14.09 - build `VaelenGame` and `VaelenUI`

Both are written, both are parsed by CI against a stand-in for Unreal
(`Tools/EngineShim`), both are fenced, and **neither has ever been compiled by
UnrealBuildTool**. There is no asset, no Blueprint and no UMG: the game mode is
named in `Config/DefaultEngine.ini` and everything else is C++.

- `Source/VaelenGame` - `UVaelenWorldSubsystem`, the ONE place a world is held,
  plus four console commands. Its `.cpp` may name a `World`, a `Run` and
  `Take.h`; its public header may not, and a CI check enforces that.
- `Source/VaelenUI` - `AVaelenHUD` draws the page, `AVaelenPlayerController`
  binds eight letters, `AVaelenGameMode` names the two in C++.

**One defect to expect, already fixed, and the reason to build early.** The
adversarial review of 2026-09-15 found that `UVaelenWorldSubsystem` declared
neither constructor nor destructor, so UnrealHeaderTool writes both into
`VaelenWorldSubsystem.gen.cpp` - a translation unit that includes the public
header and never the `.cpp`, where the type held by the pimpl is only forward
declared. `TDefaultDelete` refuses that by `static_assert`, and your build would
have stopped on a file nobody here wrote. It is fixed. Nothing on this side
could have caught it: there is no UnrealHeaderTool in the shim, so the
translation unit that breaks does not exist to be parsed. Expect more of that
shape, and **report rather than repair**.

### 14.10 - the month, and the two lines

This is the phase gate, and four of its five clauses are yours.

1. Open the 13.09 level and run `Vaelen.Play 256 100`. It takes seconds, not a
   frame: it generates AELVOR, runs 300 years of pre-history and 100 of the
   world, and takes somebody up.
2. **Press `Space` once before anything else - and count it as the first of
   your thirty.** On the day somebody is taken up
   the hours of that day are already spent, so the page offers no verb at all
   (every row reads `- not today`) and `near:` is empty. One day turn fixes
   both. This is measured, not guessed: in the checked-in stand-in stream the
   taking is at tick 3456000 and the first command the page accepted is at
   3456024. If you press `W` first and nothing happens, this is why, and the
   build is fine.
3. Play the remaining **twenty-nine days** through the keys, so that the log
   says `30 days` and not 31: `W` work, `R` rest, `E` eat, `T` wait,
   `S` speak, `G` give, `K` take, `M` move; `Tab` changes what the next Speak,
   Give, Take or Move is aimed at; `Space` turns the day. Every verb at least
   once. Get one refusal to appear on screen, and one `M` to a neighbour the
   page lists under `near:` - if `near:` is ever empty, that is a finding worth
   reporting rather than a key to press harder.
4. Read `stat fps` with the HUD up, between day steps - not during one.
5. Take one screenshot whose LAST line is the page's own digest in hex.
6. Press `F9`. It writes the input stream to `Saved/Vaelen/`.

**What to send back**, and it is four things:

- the two `LogVaelenPlay:` lines from the log, verbatim, whole;
- the eight `LogVaelenUI: <verb> -> ...` lines, one per key;
- the fps figure and the ms per `Vaelen.Day` at 256;
- the screenshot, and the `.stream` file.

The stream is the point. This side replays it headlessly through
`Tools/Atlas --replay <file> --panel --want-bound 0` and prints the same two
lines; if the bytes after the log prefix differ, the engine and the kernel
disagree about a played life and the phase does not close. That comparison is
already wired and green against a stand-in stream
(`Tests/Run/Streams/README.md` says exactly what a stand-in is and is not), so
the only thing missing is a month somebody actually played.

**Do not fix `Source/VaelenRun`, `Source/VaelenView` or `Source/VaelenPlayer`
to make the numbers agree.** If they disagree, that is the finding, and it is
worth more than a green gate.

## What the kernel half already hands you

| You need | It is called | Where |
|---|---|---|
| one frame's worth of the world | `View::TakeView(const World&, const ViewSources&, WorldView&)` | `Vaelen/View/Frame.h` |
| what changed since the last frame | `View::Diff` and `View::Apply` | `Vaelen/View/Delta.h` |
| only what is on screen | `View::TakeViewFor(..., const Eye&, ...)` | `Vaelen/View/Eye.h` |
| a sentence for anything that happened | `History::DescribeEvent`, `Gameplay::DescribeGameplayEvent` | `Vaelen/Sim/HistoryText.h`, `Vaelen/Gameplay/GameplayHistory.h` |
| why somebody was bound | `Gameplay::WhyBelieved` | `Vaelen/Gameplay/GameplayHistory.h` |
| the played life in one view | `View::TakeLifeView` | `Vaelen/View/Take.h`, view in `Vaelen/View/Life.h` |
| the whole first screen, as rows of text | `View::TakePanel` then `View::Lines` | `Vaelen/View/Panel.h` |
| a key turned into an intent the world will accept | `View::Press` | `Vaelen/View/Panel.h` |
| one world, its door and its replay | `Run::Aelvor`, `Run::Door`, `Run::Replay` | `Vaelen/Run/Aelvor.h`, `Vaelen/Run/Door.h` |

A `WorldView` holds no pointer, no handle and no component type (ADR-0104), so
it can be copied, kept between frames, compared, and handed to a rendering
thread. It cannot reach the simulation, and that is on purpose: the renderer
reads and never writes, and after 13.01 that is structural rather than a rule
somebody has to remember.
