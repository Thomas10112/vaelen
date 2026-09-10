# Engine hand-off: what the machine with UE 5.6 does, and what it does not

**Who this is for.** The session running on a computer with Unreal Engine 5.6
installed. Everything in this repository up to and including the kernel half of
Phase 13 was built and verified in a Linux container with clang, gcc, cmake and
ninja and **no engine at all**. That container cannot do the four remaining
tasks of Phase 13. This file is what it hands over.

## The standing rule, first, because it is the one that matters

**Do not fix anything inside these directories:**

```
Source/VaelenCore  Source/VaelenSim  Source/VaelenPopulation  Source/VaelenSociety
Source/VaelenEconomy  Source/VaelenPolitics  Source/VaelenMilitary
Source/VaelenInfrastructure  Source/VaelenColony  Source/VaelenPlayer
Source/VaelenGameplay  Source/VaelenView
```

They are validated by the headless CI: eleven phase gates, six Linux presets, a
Windows MSVC leg and a macOS AppleClang leg. A fix applied on the engine machine
is a fix nothing in that matrix has seen, and the frozen digests of eleven gates
are exactly the kind of thing a well-meant edit moves.

**Report the errors. Do not repair them.** Paste the compiler output, name the
file and the line, and the headless side fixes it where the tests are.

The one thing the engine machine owns outright is `Source/Vaelen/` (the primary
game module), `Config/`, `Vaelen.uproject`, and whatever `VaelenPresentation`
turns out to be.

## What is actually there to build

Thirteen `*.Build.cs` files and twelve `*Module.cpp` files. Twelve of the
modules are kernel modules that have compiled under gcc, clang, MSVC and
AppleClang through CMake, and have **never been read by UnrealBuildTool** except
for `VaelenCore`, `VaelenSim`, `VaelenPopulation`, `VaelenSociety` and
`VaelenEconomy`, which were built in the editor on 2026-09-07.

`Vaelen.uproject` declares all thirteen modules and `EngineAssociation 5.6`.

Everything newer than that first editor build carries `STATUS: UNVERIFIED` in
its header, and that mark is accurate rather than pessimistic: those files have
been read by a C++ compiler through CMake but never by UBT, which has its own
rules about module dependencies, IWYU and what a `Build.cs` may say.

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

## What the kernel half already hands you

| You need | It is called | Where |
|---|---|---|
| one frame's worth of the world | `View::TakeView(const World&, const ViewSources&, WorldView&)` | `Vaelen/View/Frame.h` |
| what changed since the last frame | `View::Diff` and `View::Apply` | `Vaelen/View/Delta.h` |
| only what is on screen | `View::TakeViewFor(..., const Eye&, ...)` | `Vaelen/View/Eye.h` |
| a sentence for anything that happened | `History::DescribeEvent`, `Gameplay::DescribeGameplayEvent` | `Vaelen/Sim/HistoryText.h`, `Vaelen/Gameplay/GameplayHistory.h` |
| why somebody was bound | `Gameplay::WhyBelieved` | `Vaelen/Gameplay/GameplayHistory.h` |

A `WorldView` holds no pointer, no handle and no component type (ADR-0104), so
it can be copied, kept between frames, compared, and handed to a rendering
thread. It cannot reach the simulation, and that is on purpose: the renderer
reads and never writes, and after 13.01 that is structural rather than a rule
somebody has to remember.
