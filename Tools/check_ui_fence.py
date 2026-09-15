#!/usr/bin/env python3
"""What the UI may include, and what it may never name. 14.07, ADR-0136/0137.

WHY THIS EXISTS

14.02 made the view headers leaves and 14.06 made a page out of three of them,
so a UI can be written that cannot reach the simulation. Can be. Nothing stops
a hurried afternoon from adding one include and reaching it anyway, and the
compiler will not complain, because of one line:

    VaelenView.Build.cs:29  PublicDependencyModuleNames.AddRange(... VaelenSim,
                            VaelenPlayer, VaelenGameplay ...)

Under UnrealBuildTool a PUBLIC dependency's include paths are transitive. Any
module that depends on VaelenView - and the UI must - can compile
`#include "Vaelen/View/Take.h"`, `"Vaelen/Player/Commands.h"` and
`"Vaelen/Sim/World.h"` today. The headless CI cannot build the UI at all, and
Tools/parse_engine_modules.py's restricted include set is a SIMULATION of a
fence, not the fence: it proves the UI parses without those headers, not that
the real build refuses them.

So this file is the only thing standing between the UI and Commands.h, and it
says so in both docstrings, as the roadmap asked. It reads text. That is a
weaker tool than a compiler and an honest one: it is checked in, it runs on
every leg, and its self-test proves each rule fires.

WHAT IS CHECKED

    the INCLUDES    every `#include "Vaelen/..."` under the UI's roots is one
                    of the view leaves, the command surface, or Core
    the CLOSURE     and so is everything those headers include, transitively:
                    an allowed header that grows a kernel include stops being
                    allowed, which is exactly how Frame.h reached Commands.h
                    before 14.02
    the TOKENS      a short list of word-bounded regexes for the things a UI
                    must not do even with an allowed include: submit straight
                    to the world, name Vaelen::World or Vaelen::Run, take a
                    view itself, read a clock, or tick anything

WHAT IS NOT

Whether the UI is correct, whether it draws anything, or whether the real UBT
build would link. The parse job answers the first two in shape only; nothing
here answers the third.
"""

import argparse
import os
import re
import shutil
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

# Where the UI lives, and the ONE directory of each module this fence does not
# read. Everything else under the module is read, because everything else under
# the module is what UnrealBuildTool compiles: naming Public and Private and
# stopping there left Source/VaelenUI/Widgets/ - or any folder somebody adds -
# built by UBT and read by nobody, which is a fence with a gate in it. So the
# list is what is EXEMPT, and the default is to read.
#
# VaelenGame/Private is the exemption and the whole seam: it is the module that
# holds the world, so its .cpp may name Run and Take.h and its header may not.
# 14.07 wrote this against a witness under Tools/UiWitness because neither
# module existed yet; 14.08 and 14.09 wrote them, the witness is gone, and the
# --witness path below is what is left of it: a module of Source that is not
# there is not silently skipped, it is reported.
ROOTS = [
    ("VaelenUI", ()),
    ("VaelenGame", ("Private",)),
]

ALLOWED = set(
    ["Vaelen/View/%s.h" % n for n in
     ("Frame", "Land", "Net", "Folk", "Delta", "Eye", "Life", "Chronicle", "Panel", "ViewApi")]
    + ["Vaelen/Player/%s.h" % n for n in ("Intent", "Stream", "PlayerApi")]
)

# Anything under here is allowed too: Core is the kernel's own foundation and
# has no way back to a World (Vaelen/Core/*.h).
ALLOWED_PREFIX = "Vaelen/Core/"

# Named so that the failure says WHICH rule, rather than "not in the list".
REFUSED = [
    "Vaelen/View/Take.h",
    "Vaelen/Player/Commands.h",
    "Vaelen/Sim/",
    "Vaelen/Population/",
    "Vaelen/Society/",
    "Vaelen/Economy/",
    "Vaelen/Colony/",
    "Vaelen/Infrastructure/",
    "Vaelen/Gameplay/",
    "Vaelen/Run/",
]

# Word-bounded, not substrings. A substring ban on "rand" refuses "the operand"
# and one on "Tick" refuses "Ticker": the self-test carries a CONTROL with both
# words in it, because a checker that cries wolf is a checker somebody turns
# off. TakePanel is deliberately absent: it takes views, not a world.
TOKENS = [
    (r"\bSubmit\s*\(", "the UI submits straight to the world; it must go through the door (Mean)"),
    (r"\bViewSources\b", "ViewSources names every type set of the kernel"),
    (r"\bVaelen::World\b", "the UI names a World"),
    (r"\bTakeView\w*\s*\(", "the UI takes its own view; the module holding the world hands it one"),
    (r"\bTakeLifeView\s*\(", "the UI takes its own view; the module holding the world hands it one"),
    (r"\bTakeChronicleView\s*\(", "the UI takes its own view; the module holding the world hands it one"),
    (r"\bBeginEnslaved\s*\(", "the UI starts a life itself"),
    (r"\bVaelen::Run\b", "the UI names the run that owns the world"),
    (r"\bFPlatformTime\b", "a clock: the world's time is the simulation's tick, not the frame's"),
    (r"\bFDateTime\b", "a clock: the world's time is the simulation's tick, not the frame's"),
    (r"\bDeltaSeconds\b", "a frame's length: the day turns on a key, not on the wall clock"),
    (r"\bTick\s*\(", "the UI ticks something"),
    (r"\brand\s*\(", "randomness outside the kernel's own stream"),
]

INCLUDE = re.compile(r'^\s*#\s*include\s+"(Vaelen/[^"]+)"', re.MULTILINE)


def ui_dirs(root, witness=True):
    """Every module this fence reads, as (directory, exempt subdirectories).

    The directory is the MODULE, not a chosen subdirectory of it: what the
    fence skips is named in the second half of the pair and nowhere else.
    """
    out = []
    for module, exempt in ROOTS:
        under = Path(root) / "Source" / module
        where = under if under.is_dir() else None
        if where is None and witness:
            wit = Path(root) / "Tools" / "UiWitness" / module
            where = wit if wit.is_dir() else None
        if where is None:
            continue
        out.append((where, exempt))
    return out


def sources(folder, exempt=()):
    """Every header and .cpp under the module, minus the exempt subdirectories.

    rglob over the module, not over one subdirectory of it: UBT compiles the
    whole tree, so the whole tree is read unless this fence says out loud that
    it does not.
    """
    out = []
    for path in sorted(Path(folder).rglob("*")):
        if path.suffix not in (".h", ".cpp"):
            continue
        parts = path.relative_to(folder).parts
        if parts and parts[0] in exempt:
            continue
        out.append(path)
    return out


def allowed(include):
    return include in ALLOWED or include.startswith(ALLOWED_PREFIX)


def why_refused(include):
    for bad in REFUSED:
        if include == bad or include.startswith(bad):
            return bad
    return None


def closure(root, header, seen):
    """Every Vaelen/ header reachable from this one, through Source/*/Public."""
    if header in seen:
        return
    seen.add(header)
    for public in sorted((Path(root) / "Source").glob("*/Public")):
        path = public / header
        if not path.is_file():
            continue
        with open(path, "r", encoding="utf-8") as handle:
            for found in INCLUDE.findall(handle.read()):
                closure(root, found, seen)
        return


def check(root, witness=True):
    """Every complaint, in reading order. Empty means the fence holds."""
    bad = []
    folders = ui_dirs(root, witness)
    if not folders:
        return ["no UI to check: neither Source/ nor Tools/UiWitness holds VaelenUI or VaelenGame"]
    for folder, exempt in folders:
        for path in sources(folder, exempt):
            with open(path, "r", encoding="utf-8") as handle:
                text = handle.read()
            where = os.path.relpath(path, root)
            for include in INCLUDE.findall(text):
                refused = why_refused(include)
                if refused is not None:
                    bad.append("%s includes %s (refused: %s)" % (where, include, refused))
                elif not allowed(include):
                    bad.append("%s includes %s, which the UI may not see" % (where, include))
                else:
                    # And everything IT includes, transitively.
                    seen = set()
                    closure(root, include, seen)
                    for reached in sorted(seen):
                        if not allowed(reached):
                            bad.append("%s includes %s, whose closure reaches %s"
                                       % (where, include, reached))
            for pattern, says in TOKENS:
                for found in re.finditer(pattern, text):
                    line = text.count("\n", 0, found.start()) + 1
                    bad.append("%s:%d: %s - %s" % (where, line, found.group(0).strip(), says))
    return bad


CONTROL = '''// A UI file that must PASS: every word below is one a substring ban would
// catch and a word-bounded one must not.
#include "Vaelen/View/Panel.h"
#include "Vaelen/Player/Intent.h"

// The operand of a press is the verb; a Ticker draws the page every frame.
void Draw(const Vaelen::View::PanelView& Page)
{
	Vaelen::Player::PlayerCommand What;
	Vaelen::View::Press(Page, Vaelen::Player::Intent::Work, 0, 1, What);
}
'''

MUTATIONS = [
    ("an include of Take.h", '#include "Vaelen/View/Take.h"\n'),
    ("an include of Commands.h", '#include "Vaelen/Player/Commands.h"\n'),
    ("an include of Sim/World.h", '#include "Vaelen/Sim/World.h"\n'),
    ("a World named", "Vaelen::World* W = nullptr;\n"),
    ("a Run named", "Vaelen::Run::Aelvor* A = nullptr;\n"),
    ("a Submit call", "void Go() { Submit(1); }\n"),
    ("a view taken", "void Go() { TakeLifeView(1, 2, 3, 4); }\n"),
    ("a clock read", "double Now() { return FPlatformTime::Seconds(); }\n"),
    # The four below exist because a rule with no mutation is a rule nothing
    # proves fires: delete or mistype any of these lines in TOKENS and, until
    # 14.09's review, the self-test still printed "all caught" and exited 0.
    # FDateTime is the one row 14.07 names in the roadmap by name.
    ("a date read", "int When() { return FDateTime::Now(); }\n"),
    ("the world's own view taken", "void Go() { TakeView(1, 2); }\n"),
    ("the chronicle taken", "void Go() { TakeChronicleView(1, 2, 3, 4); }\n"),
    ("a life begun", "void Go() { BeginEnslaved(1, 2); }\n"),
    ("a frame's length", "void Step(float DeltaSeconds) { (void)DeltaSeconds; }\n"),
    ("something ticked", "void Go() { Thing.Tick(0.1f); }\n"),
    ("randomness", "int Roll() { return rand(); }\n"),
    ("ViewSources named", "Vaelen::View::ViewSources From;\n"),
]


def self_test(root):
    """The control first, then one mutation at a time. Each must be caught."""
    failures = 0
    with tempfile.TemporaryDirectory(prefix="vaelen-uifence-") as copy:
        source = Path(copy) / "Source"
        for module in ("VaelenSim", "VaelenCore", "VaelenView", "VaelenPlayer", "VaelenRun",
                       "VaelenPopulation", "VaelenSociety", "VaelenEconomy", "VaelenColony",
                       "VaelenInfrastructure", "VaelenGameplay", "VaelenPolitics", "VaelenMilitary"):
            there = Path(root) / "Source" / module / "Public"
            if there.is_dir():
                shutil.copytree(there, source / module / "Public")
        ui = source / "VaelenUI" / "Private"
        ui.mkdir(parents=True)
        (source / "VaelenUI" / "Public").mkdir(parents=True)
        page = ui / "VaelenPage.cpp"

        with open(page, "w", encoding="utf-8") as handle:
            handle.write(CONTROL)
        complaints = check(copy, witness=False)
        if complaints:
            print("self-test: the CONTROL was refused:", file=sys.stderr)
            for line in complaints:
                print("  " + line, file=sys.stderr)
            failures += 1
        else:
            print("self-test: the control passes (it says 'the operand' and 'Ticker')")

        for name, mutation in MUTATIONS:
            with open(page, "w", encoding="utf-8") as handle:
                handle.write(CONTROL + mutation)
            if not check(copy, witness=False):
                print("self-test: %s was NOT caught" % name, file=sys.stderr)
                failures += 1

        # And the closure: an ALLOWED header that grows a kernel include stops
        # being allowed. This is the rule no list of filenames can carry.
        with open(page, "w", encoding="utf-8") as handle:
            handle.write(CONTROL)
        leaf = source / "VaelenView" / "Public" / "Vaelen" / "View" / "Panel.h"
        with open(leaf, "r", encoding="utf-8") as handle:
            was = handle.read()
        with open(leaf, "w", encoding="utf-8") as handle:
            handle.write(was.replace('#include "Vaelen/Core/Hash.h"',
                                     '#include "Vaelen/Core/Hash.h"\n#include "Vaelen/Sim/World.h"', 1))
        if not check(copy, witness=False):
            print("self-test: a leaf that grew a kernel include was NOT caught", file=sys.stderr)
            failures += 1
        with open(leaf, "w", encoding="utf-8") as handle:
            handle.write(was)

        # And the folder nobody named. UnrealBuildTool compiles every .cpp
        # under a module, so a Widgets/ or Classes/ folder is built like the
        # rest; a fence that reads Public and Private and stops there lets one
        # hold the world in plain sight. Until 14.09's review, this passed.
        aside = source / "VaelenUI" / "Widgets"
        aside.mkdir(parents=True)
        with open(aside / "SVaelenPanel.cpp", "w", encoding="utf-8") as handle:
            handle.write('#include "Vaelen/Sim/World.h"\n'
                         "void Cheat(Vaelen::World& W) { Submit(W, 1); }\n")
        if not check(copy, witness=False):
            print("self-test: a file outside Public and Private was NOT read", file=sys.stderr)
            failures += 1
        shutil.rmtree(aside)

    if failures:
        print("self-test: %d case(s) failed" % failures, file=sys.stderr)
        return 1
    print("self-test: %d mutations, the closure and the unnamed folder, all caught, "
          "control clean" % len(MUTATIONS))
    return 0


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--root", default=str(ROOT))
    ap.add_argument("--self-test", action="store_true")
    ap.add_argument("--no-witness", action="store_true",
                    help="read only Source/, never Tools/UiWitness")
    args = ap.parse_args()
    if args.self_test:
        return self_test(args.root)
    bad = check(args.root, witness=not args.no_witness)
    for line in bad:
        print("[ui-fence] " + line, file=sys.stderr)
    if bad:
        print("[ui-fence] %d thing(s) the UI may not do" % len(bad), file=sys.stderr)
        return 1
    folders = ui_dirs(args.root, witness=not args.no_witness)
    files = sum(len(sources(f, e)) for f, e in folders)
    read = []
    for folder, exempt in folders:
        where = os.path.relpath(folder, args.root)
        read.append(where if not exempt else "%s (all but %s)" % (where, ", ".join(exempt)))
    print("[ui-fence] %d file(s) under %s: every Vaelen include is a view leaf, the command "
          "surface or Core, and nothing names a world" % (files, ", ".join(read)))
    print("[ui-fence] this is TEXT, not a compiler: VaelenView.Build.cs makes every kernel "
          "include path transitive under UBT, so this is the only thing standing between the "
          "UI and Commands.h")
    return 0


if __name__ == "__main__":
    sys.exit(main())
