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

# Where the UI lives. VaelenGame is policed in its PUBLIC directory only: it is
# the module that holds the world, so its .cpp may name Run and Take.h, and its
# header may not - that is the seam. Until 14.08 and 14.09 write the real
# modules, the witness of 14.07 stands in for them (Tools/UiWitness/README.md).
ROOTS = [
    ("VaelenUI", ("Public", "Private")),
    ("VaelenGame", ("Public",)),
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
    """Every directory this fence reads, in the order it reads them."""
    out = []
    for module, subs in ROOTS:
        under = Path(root) / "Source" / module
        where = under if (under / "Private").is_dir() or (under / "Public").is_dir() else None
        if where is None and witness:
            wit = Path(root) / "Tools" / "UiWitness" / module
            where = wit if wit.is_dir() else None
        if where is None:
            continue
        for sub in subs:
            if (where / sub).is_dir():
                out.append(where / sub)
    return out


def sources(folder):
    return sorted(p for p in Path(folder).rglob("*") if p.suffix in (".h", ".cpp"))


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
    for folder in folders:
        for path in sources(folder):
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

    if failures:
        print("self-test: %d case(s) failed" % failures, file=sys.stderr)
        return 1
    print("self-test: %d mutations and the closure, all caught, control clean" % len(MUTATIONS))
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
    files = sum(len(sources(f)) for f in folders)
    print("[ui-fence] %d file(s) under %s: every Vaelen include is a view leaf, the command "
          "surface or Core, and nothing names a world"
          % (files, ", ".join(os.path.relpath(f, args.root) for f in folders)))
    print("[ui-fence] this is TEXT, not a compiler: VaelenView.Build.cs makes every kernel "
          "include path transitive under UBT, so this is the only thing standing between the "
          "UI and Commands.h")
    return 0


if __name__ == "__main__":
    sys.exit(main())
