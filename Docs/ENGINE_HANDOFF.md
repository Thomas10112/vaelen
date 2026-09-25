# Engine hand-off: what the machine with UE 5.6 does, and what it does not

**Who this is for.** The session running on a computer with Unreal Engine 5.6
installed. Everything in this repository is built and verified in a Linux
container with clang, gcc, cmake and ninja and **no engine at all**. That
container cannot build the engine-facing modules. This file is what it hands
over.

**Where this stands on 2026-09-16.** Phase 13 is CLOSED. Phase 14's two modules
are BUILT - `VaelenGame` and `VaelenUI` compiled and ran in the editor on
2026-09-16 - and its month is PLAYED: eighty-three days at the keyboard, and the
headless replay reproduced them byte for byte. One figure is outstanding and the
phase closes on it: the frame rate, clause (c) of 14.10, at the end of the
Phase 14 section below. Phases 13's sections are kept as the record of how the
last hand-off went.

**Where this stands on 2026-09-24.** Phases 15, 16 and 17 happened after the
paragraph above and none of them touched this file; that is corrected here in one
block, and the sections below Phase 14 stay the record they are.

- **Phase 15 (STREAMING & LOD) is CLOSED, 2026-09-21**, all six clauses. Its
  engine half, 15.10, is built and was run on this machine: the world subsystem
  hands the camera's region to `Run::Door::Look` when the day turns, and the walk
  played here on 2026-09-21 is `Replay.Lived` and `Run.Gate.Lived` in the frozen
  gate list — the only two entries that came off another computer.
- **Phase 16 (SAVE/PERSISTENCE): fourteen tasks built headless, its gate still
  open on two things.** One is headless (the migration half of clause (j),
  deferred to the v4 format bump). The other is yours: **16.14's engine half
  is WRITTEN and PARSED (2026-09-24), not built and not run.** It did not
  exist when the Phase 17 close read the tree, and it was written on the
  headless side first, like every engine-side file since 14.07. Its sitting
  has its own section below: what to type, and the lines to bring back.
- **Phase 18 (CLIMATE & SEASONS) is OPEN, planned in `Docs/ROADMAP.md`
  section 25.** Its first four tasks are headless and touch what the engine
  COPIES: `View::WorldView`, `RegionView` and `LifeView` renamed their reserved
  words (`Season`, `Climate`, `Degrees`, `Chill`, `Winter`) at the same size,
  and `Vaelen/View/Climate.h` is a new leaf (four bytes a tile) Phase 19 will
  colour the ground by. 18.05 added `Population::WarmthTypes` (one component,
  `PersonWarmth`, eight bytes, zero is warm), declared ONLY in a climate
  world, after `PolityTypes` and BEFORE `ColonyTypes` - the position both
  actors must take at the flip, because it shifts the Colony/Play/Lively type
  ids by one (ADR-0153); `Tools/check_world_wiring.py` names it optional until
  then. 18.06 added `Economy::WinterSystem` behind the same option, built
  after the stock system with `RunAfter("Stocks")`, `ObserveSettlements(
  Trade.Settlement)`, and the harvest told `RunAfter("Winter")` - the four
  lines both actors add at the flip. **18.10 FLIPPED IT.** Both actors now
  declare `WarmthTypes` after `PolityTypes`, build `WinterSystem` after the
  stocks, tell the harvest the season and the need system the winter, and the
  view actor's sources say `HasClimate` and `HasWarmth` - the page carries a
  weather row and the HUD draws it through `View::Lines` with no engine change.
  **The ADR-0135 pair moved with it:** at 128/120 the frame the engine must
  match is now `ec18241b89c3d246` (it was `abc5a5767c6cf9dd`, which the
  headless side still reproduces with `--no-climate`); the ground is unchanged,
  `8f7f4948f49b6e86`. Parsed against the shim, not built here: the actors are
  UNVERIFIED (engine) until the owner's next sitting prints the two lines.
  Months recorded before 18.10 are pre-climate worlds; replay them headless with
  `--no-climate` (the CTest drivers do).
- **Phase 17 (DEBUG TOOLS) is CLOSED, 2026-09-24**, all eleven clauses, and it
  had no engine task. One thing it changed that the engine COPIES:
  `View::ChronicleView` (`Vaelen/View/Chronicle.h`) holds four why lines instead
  of two and carries a `WhyEnd`, so its `sizeof` went from 7816 to 8144 bytes.
  `UVaelenWorldSubsystem` keeps one by value, so the next UBT build recompiles
  it and nothing else is asked of you; the HUD draws `View::Lines` and never
  reads the struct.

## The standing rule, first, because it is the one that matters

**Do not fix anything inside these directories:**

```
Source/VaelenCore  Source/VaelenSim  Source/VaelenPopulation  Source/VaelenSociety
Source/VaelenEconomy  Source/VaelenPolitics  Source/VaelenMilitary
Source/VaelenInfrastructure  Source/VaelenColony  Source/VaelenPlayer
Source/VaelenGameplay  Source/VaelenView  Source/VaelenScene  Source/VaelenRun
```

They are validated by the headless CI: eighteen frozen gates (`Tools/run_gates.sh`; seventeen until 18.10 added `Replay.Climate`),
six Linux presets, a Windows MSVC leg and a macOS AppleClang leg, 219 CTest entries
(2026-09-24; it was fourteen, and 175, when this was first written). A fix applied on the engine machine
is a fix nothing in that matrix has seen, and the frozen digests of eighteen gates
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

The remaining four - `VaelenRun`'s rules file and Unreal-facing translation unit,
`Source/VaelenGame` and `Source/VaelenUI` - were built by UBT on 2026-09-16, in
the same editor configuration, and the modules were then RUN: a life was played
through `VaelenUI`'s eight keys for eighty-three days. The fifteen files that
carried `STATUS: UNVERIFIED` then said VALIDATED with the date and what was run;
getting there cost three defects that nothing headless could see, listed under
Phase 14 below. **That stopped being true after 15.10, and nothing noticed:** by
2026-09-25, 16.14 and 18.02/18.10 had put 410 non-comment lines into seven engine
files, five of which still said VALIDATED. Since 19.01 every engine file names the
build its STATUS rests on (`// BUILD: <id>`, a row of `Tools/engine_builds.txt`),
and `Kernel.EngineStatus` refuses a claim whose code moved past that build. The
list of what is UNVERIFIED is what that check accepts, not this paragraph.

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

## PHASE 14 - what happened on 2026-09-16, and the one figure still missing

Both modules are built and a month is played. What follows is the record, and
then the single thing still outstanding.

### 14.08 and 14.09 - `VaelenGame` and `VaelenUI`, BUILT

Both were compiled by UnrealBuildTool on 2026-09-16 (UE 5.6, MSVC 19.51, Win64
Development Editor) and run. It took three defects to get there, and **not one
of them was visible on this side**, which is the honest measure of what the
parse job buys:

1. **UnrealHeaderTool's generated destructor.** `UVaelenWorldSubsystem`
   declared neither constructor nor destructor, so UHT wrote both into
   `VaelenWorldSubsystem.gen.cpp` - a translation unit that includes the public
   header and never the `.cpp`, where the pimpl's type is only forward
   declared. `TDefaultDelete` refuses that by `static_assert`. There is no UHT
   in the shim, so the file that breaks does not exist to be parsed.
2. **`DEFINE_VTABLE_PTR_HELPER_CTOR`.** Declaring the two and defining them
   `= default` out of line did NOT fix it: MSVC treats an out-of-line `= default`
   special member of a dllexport class as implicitly defined and elides it, and
   UHT emits that macro whatever the class declares. The fix that held is
   `TPimplPtr` (`Templates/PimplPtr.h`), which binds its deleter at
   construction, in the `.cpp`, where the type is complete.
3. **`RegionGraphCache` LNK2019.** Exporting the class exported nothing usable;
   the fix is to stop exporting the class and export only `Of()`. Invisible
   headless because CMake builds static libraries, where a missing dllexport
   costs nothing.

Expect more of that shape, and **report rather than repair**.

### 14.10 - the month, and the two lines: DONE, byte for byte

Eighty-three days played at the keyboard, ninety-nine intents, written out with
the console command `Vaelen.Stream.Write`. The engine printed:

```
LogVaelenPlay: AELVOR 256 seed 41454c564f52: played Dukem (person 15019, region 37) 83 days, 99 intents (76 taken, 23 refused by the world, 0 dropped at the door), state 2ffed5236a593c1b, log 1fdd4211695649bc, life 654e2de6455d8b7a, panel 7de2c5faf3cc1813
LogVaelenPlay: verbs work 34 rest 1 eat 2 wait 2 speak 5 give 2 take 26 move 27
```

`Tools/Atlas --replay Tests/Run/Streams/aelvor256-2026-09-16.stream --panel
--want-bound 0` printed those two lines back on Linux, from a world rebuilt out
of the stream's header alone. Compared as files rather than by eye: same MD5,
zero bytes differ, and above them `replay: 99 of 99 answered identically, 83 of
83 days`. CTest `Replay.Played` now pins exactly that. The panel digest
`7de2c5faf3cc1813` is also the last line of the screenshot taken that day, so
the log and the pixels agree.

Clauses (a) the format verbatim, (b) byte-identical, (d) the screenshot with the
digest as its last row: **met**. Clause (c) is not.

**The ms per `Vaelen.Day` is in.** 246.4 ms for the first day turn, then 1.5 to
2.0 ms for each of the next nine, at 256 with 110 048 alive. Reproduced
headlessly and decomposed there: the day itself is 0.10 ms, the five `Take*`
calls the subsystem makes around it are 1.6 ms, and the first day's spike is the
detail promotion of the played person's neighbours (ADR-0139) landing on the
first tick - about 65 ms per region promoted. Reported, not gated.

**The frame rate is not in, and one reading decides it.** 35.52 fps (28.15 ms)
was read with the HUD up on 2026-09-16, against a clause that asks for `>= 90`.
It fails as measured - but the screenshot shows nothing behind the HUD except
the template map's grid floor, sky and volumetric clouds: the Phase 13 actor's
72 649 instances are not in the picture, and 13.09 measured **100 fps on this
same machine with them drawn**. So the number may be timing a sky rather than
this project.

`DrawHUD` returns at its first `if` while no world is held, which gives a clean
before-and-after. **In the 13.09 level** (the one with the presentation actor in
it, not an empty template map):

1. `stat unit` and `stat fps` - read them with nothing running. The HUD draws
   nothing here, so this is the scene's own cost.
2. `Vaelen.Play 256 100`
3. `stat unit` and `stat fps` again.

Send both readings, all four figures each (Frame / Game / Draw / GPU, and fps).
The difference between them is the HUD's cost exactly, and `stat unit` says
which thread pays it. If the HUD costs ~17 ms, that is a defect on this side and
worth more than a green gate. If GPU sits at 28 ms in BOTH readings, the clause
was written against a scene nobody was measuring and it gets amended with an ADR.

Nothing else is asked of the engine machine for Phase 14.

### How the stream is written, for the next time

`F9` is NOT it. `F9` writes the input stream to `Saved/Vaelen/` and prints
nothing; the two `LogVaelenPlay:` lines come from the console command
**`Vaelen.Stream.Write`**, which writes the file AND prints the gate lines. Ask
for the lines and you are asking for that command. (The hand-off said `F9` here
until 2026-09-16, and the first `.stream` that reached this side was written at
day 8 because of it.)

`Vaelen.Play` needs a game instance: it is a `UGameInstanceSubsystem`, so it
runs under PIE or `-game`, and answers `no game instance` from an editor
commandlet.

## PHASE 16 - the sitting of 16.14: what to type, and what to bring back

The engine half is written and parsed (2026-09-24), not built and not run.
`Source/VaelenGame/Private/VaelenCheckpointStore.h` and `.cpp` carry
`STATUS: UNVERIFIED (engine)` until this sitting, and since 19.01 so do
`VaelenWorldSubsystem` (`Save`, `Load`, `Saves`) and `VaelenPlayCommands`
(`Vaelen.Save`, `Vaelen.Load`, `Vaelen.Saves`) - until 19.01 this paragraph said
they did, and they still said VALIDATED; `Kernel.EngineStatus` now holds it. The standing rule holds: a
compile error in those files is reported, not repaired - and an error inside
`Source/VaelenRun` or below is reported and NOT repaired, whatever it says.
Three engine calls were parsed and never run, and are the likeliest to need
a report: `IFileManager::Move` with Replace, `FindFiles` over a directory
(the store strips paths to leaves either way), and `FFileHelper`'s
`SaveArrayToFile` / `LoadFileToArray`.

1. Build the editor as for 14.08. The console should list three new commands:
   `Vaelen.Save`, `Vaelen.Load`, `Vaelen.Saves`.
2. Under `-game` (a game instance is needed, as for `Vaelen.Play`):
   `Vaelen.Play 128 120 1`, a few `Vaelen.Day`, one or two `Vaelen.Do`, then
   `Vaelen.Save first`. Bring back the TWO lines it prints:
   ```
   LogVaelenPlay: saved first: state <16 hex>, log <16 hex>, life <16 hex>, panel <16 hex>; year Y day D, played P, daily cadence; <path>
   LogVaelenPlay: check it headless: VaelenAtlas --load-from "<path>" --size 128 --years 120 --prehistory 300 --then-days 0 --stream --climate
   ```
   (Since 19.03 the check names its era both ways, `--climate` or
   `--no-climate`: the Atlas's default flipped at 18.10.)
3. A few more `Vaelen.Day`, then `Vaelen.Stream.Write`; bring its lines too.
4. CLOSE THE EDITOR. Reopen under `-game` again and, WITHOUT `Vaelen.Play`:
   `Vaelen.Saves` (bring the listing), then `Vaelen.Load first`. Bring back
   its `loaded first:` line - its state digest must be the one `saved`
   printed - and the `day(s) on the tape` line after it.
5. `Vaelen.Day` the same number of times as in step 3, `Vaelen.Stream.Write`,
   and bring back that line and BOTH files: `Saved/Vaelen/first` (the
   container) and the stream it wrote.
6. If anything refuses, bring the exact line: every refusal names its reason
   (the store's, the container's, or `Adopt`'s), and the line is the report.

What the headless side does with it: runs the `check it headless` command and
requires `adopted at` the same state; adds `Run.Gate.Saved` over the container
exactly as `Run.Gate.Lived` was added over the walk of 2026-09-21; replays the
container's own STREAM section and requires the digests of step 5; and turns
the four `UNVERIFIED (engine)` lines to VALIDATED with the date.

## PHASE 19 - sitting S1 (task 19.03): the tree as it stands, built once

Before any Phase 19 engine line lands (ROADMAP section 26, ADR-0159). Since
the last build (15.10, 2026-09-21, commit `867a129`) 410 lines of engine code in
seven files have been written here and parsed, and never compiled: 16.14's
save and load, 18.02's host byte, 18.10's climate wiring in both actors, and
19.03's era flag. This sitting builds exactly that, so that a first error has
one cause. The standing rule above holds without exception: REPORT, do not
repair - above all nothing in `Source/VaelenCore` ... `VaelenMilitary`, nor in
`VaelenRun`, `VaelenView`, `VaelenScene` or below.

THE FIRST ATTEMPT, 2026-09-25, on 19.03's commit `c000399`, stopped at step 1
exactly as this sitting is meant to stop: `Result: Failed
(OtherCompilationError)`, the first error

    Source\VaelenRun\Public\Vaelen\Run\Aelvor.h(264,25): error C2487: 'Generations':
    le membre d'une classe d'interface dll ne peut pas être déclaré avec une interface dll

and the same for `AdoptResultToString`, `Adopt`, `GetRunState` and `SetRunState`
(lines 318, 331, 334, 338), in `Aelvor.cpp` and `VaelenCheckpointStore.cpp`.
Phase 16 had put `VAELEN_RUN_API` on five members of a class that is itself
`VAELEN_RUN_API`; the macro is `dllexport` only in the editor's DLL build, so
every headless leg, MSVC's included, compiled it. Repaired HERE, not on the
owner's machine (the standing rule held), by commit `19.03b`, which also adds
`Kernel.DllApi` (`Tools/check_dll_api.py`): it reads every header the way
that build does and finds those five lines, and only those, in `c000399`.

The retake builds `19.03b`. It is the branch's head of that moment, so it
carries 19.04 to 19.08 too - headless work in kernel modules the CI builds
on every leg, and one new module, `VaelenScene` (a `Build.cs`, a module file
and integer code). A first error in VaelenScene is therefore a possible
second cause: report which module a first error is in.

0. Check out THE COMMIT OF 19.03b, not the branch's head - the branch goes on
   with headless work that this build must not see:
   ```
   git fetch origin claude/vaelen-master-prompt-aw7zqj
   git checkout $(git log origin/claude/vaelen-master-prompt-aw7zqj --grep="^19.03b: " -1 --format=%H)
   git rev-parse HEAD
   ```
   The FIRST line of what you bring back is that `rev-parse` - the log is
   refused without it (`Tools/check_session.py`, `head`).
1. Build the editor as for 14.08. Bring back the UBT result line and, if it
   fails, the FIRST error verbatim, and stop there.
2. In the editor console: `Vaelen.View 128 120`. Bring back its
   `AELVOR digests:` line; it must say `frame ec18241b89c3d246, ground
   8f7f4948f49b6e86` - the headless frame since 18.10, never yet printed by an
   engine.
3. The 16.14 steps above, as written.
4. Under `-game`: `Vaelen.Play 128 120 1`, `Vaelen.Day` three times,
   `Vaelen.Stream.Write`. Bring back the `LogVaelenPlay:` lines, the stream
   file it names, and a SCREENSHOT of the page: its second row is the Weather
   row, which no engine screen has shown yet.
5. `stat unit` twice, the camera at ground level: over `Vaelen.View 128 120`
   and during `Vaelen.Play`. Bring back the Frame / Game / Draw / GPU figures.
   They are the baseline Phase 19's frame-time question is measured against.
6. THE CONTROL. Copy `Tests/Run/Containers/host24-16.container` from the
   repository into `Saved/Vaelen/`, then `Vaelen.Load host24-16`. Its `check
   it headless` line must end with `--no-climate`: the world before the winter,
   said by name.

Bring back the whole `Saved/Logs/Vaelen.log` as it is, with the rev-parse line
put first. It is committed as `Tests/Run/Sessions/s1-<date>.log` and re-read by
`Session.P19S1`: the lines of step 4 against `VaelenAtlas --gate <stream>
--want-bound 0`, byte for byte after the category. Afterwards
`Tools/engine_builds.txt` gains the row `s1` (RECORDED), and
`check_engine_status.py` moves the files this build compiled to VALIDATED.

### After S1: what the branch carries beyond 19.03b, for the build after it

S1 is pinned to `19.03b` and sees none of this. The next build (sitting S2,
19.06) will, and each is a possible first error of its own:

- 19.09 (kernel, CI-built): `Source/VaelenScene/Public/Vaelen/Scene/Sky.h`
  and `Private/Sky.cpp` - integers only, nothing Unreal.
- 19.10 (engine, parsed only): `UVaelenWorldSubsystem::Keys()` - a plain
  `Vaelen::View::PanelKeys` member of the UCLASS, NOT a UPROPERTY (its type
  is no USTRUCT); every page the host composes takes it, and the `check it
  headless` line gains ` --keys LLLLLLLL` only when the table is not 14.09's.
  `AVaelenPlayerController::SetupInputComponent` binds the eight verbs from
  that table through `FKey(FName(...))` instead of eight `EKeys::` literals -
  the belief to report if it does not compile is that `FKey` has a constructor
  from `FName` (InputCoreTypes.h) and that the key named "T" is `EKeys::T`.
  The table is DefaultKeys until 19.06, so the page and the four digests of
  every stream recorded so far are unchanged: if `Vaelen.Play` prints another
  `panel` digest than the Atlas replays to, the binding moved the page and
  that is the report.

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
