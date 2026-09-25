#!/usr/bin/env python3
"""Prove that Tools/parse_engine_modules.py actually catches things. ADR-0134.

A CHECKER THAT HAS NEVER CAUGHT ANYTHING IS A CLAIM, NOT A CAPABILITY.

The parse job is green on a healthy tree, and a green light is exactly what a
shim that quietly stopped working would also give. So this file breaks the
module on purpose, once per defect, and fails if the parser did not notice.

The first mutation is not invented: it is the defect that actually happened.
13.07c renamed ReliefScale to ReliefFraction in FVaelenDrawSettings and left
the actor assigning the old name. That shipped, and the project owner's machine
was the first compiler to read it - error C2039, a build, a report, a fix, a
second build. This file exists so that the next one costs a CI minute instead.

The last case is a CONTROL that must PASS. Without it a parser that failed on
everything - a broken shim, a missing compiler - would look like a perfect
detector, and every mutation below would be "caught" for the wrong reason.

Nothing here writes to the working tree: each mutation is applied to a COPY of
the module under a temporary directory, which the parser is pointed at.
"""

import os
import shutil
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "Tools"))
# The list of engine modules is the parser's, read from it rather than copied:
# the day a second module was added there, this file still copied one, the
# parser found "no translation units" for the other, and the CONTROL failed -
# which is the control doing its job, and the reason it runs first.
from parse_engine_modules import ENGINE_MODULES, module_root  # noqa: E402

MODULE = "VaelenPresentation"

ACTOR = os.path.join("Source", MODULE, "Private", "VaelenViewActor.cpp")
DRAWER = os.path.join("Source", MODULE, "Private", "VaelenViewDrawer.cpp")
HEADER = os.path.join("Source", MODULE, "Public", "VaelenViewDrawer.h")

# 14.07's half: the UI, which is parsed with a restricted include set. These
# were the witness's files until 14.09 wrote the real module; the copy below
# takes each module from wherever the parser finds it, so this did not have to
# change when they became real - only the lines the mutations name did.
HUD = os.path.join("Source", "VaelenUI", "Private", "VaelenHUD.cpp")
KEYS = os.path.join("Source", "VaelenUI", "Private", "VaelenPlayerController.cpp")
# 16.14's three files: the store over the engine's file manager, the host that
# saves and loads through it, and the commands that print the lines.
STORE = os.path.join("Source", "VaelenGame", "Private", "VaelenCheckpointStore.cpp")
HOST = os.path.join("Source", "VaelenGame", "Private", "VaelenWorldSubsystem.cpp")
PLAY = os.path.join("Source", "VaelenGame", "Private", "VaelenPlayCommands.cpp")

# (name, file, text to find, text to put there). A mutation whose "find" text
# is no longer present is a FAILURE of this file, not a pass: it means the
# source moved and this case stopped testing anything.
MUTATIONS = [
    (
        "the real 13.07c defect: a renamed setting the actor still assigns",
        ACTOR,
        "How.ReliefFraction = ReliefFraction;",
        "How.ReliefScale = ReliefScale;",
    ),
    (
        "an argument dropped from a drawer call",
        ACTOR,
        "DrawFolk(People, Map, How, Folk, Tally.SkippedFolk, Tally.FolkTiles)",
        "DrawFolk(People, Map, How, Folk, Tally.SkippedFolk)",
    ),
    (
        "a tally field renamed in the header and not at its use",
        HEADER,
        "int32 FolkTiles = 0;",
        "int32 FolkTileCount = 0;",
    ),
    (
        "UE_LOG naming a log category that was never declared",
        ACTOR,
        'UE_LOG(LogVaelenView, Display, TEXT("AELVOR ground',
        'UE_LOG(LogVaelenTypo, Display, TEXT("AELVOR ground',
    ),
    (
        "a log argument naming a member that is gone",
        ACTOR,
        "Counted.Oldest, Counted.Bytes,",
        "Counted.Eldest, Counted.Bytes,",
    ),
    (
        "a drawer handed the wrong view entirely",
        ACTOR,
        "VaelenViewDrawer::DrawFolk(People, Map, How",
        "VaelenViewDrawer::DrawFolk(Map, Map, How",
    ),
    (
        "a colour function called with the settings missing",
        DRAWER,
        "Paint.Add(ColourOfPerson(P, How));",
        "Paint.Add(ColourOfPerson(P));",
    ),
    (
        "a UI that includes the header of the world it may not see",
        HUD,
        '#include "VaelenWorldSubsystem.h"',
        '#include "VaelenWorldSubsystem.h"\n#include "Vaelen/View/Take.h"',
    ),
    (
        "a UI that includes the command surface of the kernel",
        HUD,
        '#include "Engine/Canvas.h"',
        '#include "Engine/Canvas.h"\n#include "Vaelen/Player/Commands.h"',
    ),
    (
        "a UI that includes the World itself",
        HUD,
        '#include "Engine/Engine.h"',
        '#include "Engine/Engine.h"\n#include "Vaelen/Sim/World.h"',
    ),
    (
        "a UI that names Vaelen::World",
        HUD,
        "void AVaelenHUD::DrawHUD()\n{",
        "void AVaelenHUD::DrawHUD()\n{\n\tVaelen::World* Reached = nullptr;\n\t(void)Reached;",
    ),
    (
        "a field of the view the UI reads and the view does not have",
        KEYS,
        "Aim < Life.CompanyCount",
        "Aim < Life.CompanyThere_",
    ),
    (
        "an argument dropped from Press",
        KEYS,
        "Press(Page, Kind, Target, 1, What)",
        "Press(Page, Kind, 1, What)",
    ),
    (
        "16.14: a store calling a file-manager verb the engine does not have",
        STORE,
        "Files.FindFiles(Found, *Directory, nullptr);",
        "Files.FindFile(Found, *Directory, nullptr);",
    ),
    (
        "16.14: a load that restores through a verb the run does not have",
        HOST,
        "Fresh->Adopt(Bytes.data(), Bytes.size())",
        "Fresh->Restore(Bytes.data(), Bytes.size())",
    ),
    (
        "16.14: a save command that forgot the check line the host hands back",
        PLAY,
        "World->Save(Args[0], Where, Check)",
        "World->Save(Args[0], Where)",
    ),
]


def parses(source_root, extra=()):
    done = subprocess.run(
        [sys.executable, os.path.join(ROOT, "Tools", "parse_engine_modules.py"),
         "--source-root", source_root, "--quiet", *extra],
        capture_output=True, text=True)
    return done.returncode == 0


def main():
    with tempfile.TemporaryDirectory(prefix="vaelen-shim-test-") as work:
        pristine = os.path.join(work, "pristine")
        os.makedirs(os.path.join(pristine, "Source"))
        for name in ENGINE_MODULES:
            # Wherever the parser finds it: Source for the modules that have
            # one, Tools/UiWitness for the two that 14.08 and 14.09 write. The
            # copy always looks like Source/, so a mutation below names a path
            # that will still be right the day the real modules land.
            shutil.copytree(module_root(ROOT, name), os.path.join(pristine, "Source", name))

        # THE CONTROL, FIRST. If an untouched copy does not parse, every result
        # below is meaningless and saying so now is the only honest option.
        if not parses(pristine):
            print("[shim-test] CONTROL FAILED: an untouched copy does not parse.", file=sys.stderr)
            print("[shim-test] every 'caught' below would be caught for the wrong reason.",
                  file=sys.stderr)
            subprocess.run([sys.executable, os.path.join(ROOT, "Tools", "parse_engine_modules.py"),
                            "--source-root", pristine])
            return 1
        print("[shim-test] control: an untouched copy parses")

        # THE REVERSE CONTROL. The same untouched copy, parsed with the stub
        # scan this file used to do - each module's own .generated.h and no
        # other - must FAIL, because 14.09's HUD includes 14.08's subsystem
        # header and that header's generated stub is written while VaelenGame
        # is scanned. If this ever passes, the shared stub directory has
        # stopped being load-bearing and the CONTROL above is proving less
        # than it says.
        if parses(pristine, ("--stub-scope", "own")):
            print("[shim-test] REVERSE CONTROL FAILED: the copy parses with per-module stubs,",
                  file=sys.stderr)
            print("[shim-test] so nothing here proves the shared stub directory is needed.",
                  file=sys.stderr)
            return 1
        print("[shim-test] reverse control: with per-module stubs it does NOT parse")

        missed = []
        stale = []
        for name, relative, find, put in MUTATIONS:
            broken = os.path.join(work, "broken")
            if os.path.isdir(broken):
                shutil.rmtree(broken)
            shutil.copytree(pristine, broken)

            path = os.path.join(broken, relative)
            with open(path, "r", encoding="utf-8") as f:
                text = f.read()
            if find not in text:
                stale.append(name)
                print(f"[shim-test] STALE   {name}", file=sys.stderr)
                print(f"[shim-test]         the text it mutates is no longer in {relative}",
                      file=sys.stderr)
                continue
            with open(path, "w", encoding="utf-8") as f:
                f.write(text.replace(find, put, 1))

            if parses(broken):
                missed.append(name)
                print(f"[shim-test] MISSED  {name}", file=sys.stderr)
            else:
                print(f"[shim-test] caught  {name}")

        if missed or stale:
            print(f"[shim-test] {len(missed)} missed, {len(stale)} stale of "
                  f"{len(MUTATIONS)} mutations", file=sys.stderr)
            return 1
        print(f"[shim-test] {len(MUTATIONS)} mutations, all caught, control clean")
        return 0


if __name__ == "__main__":
    sys.exit(main())
