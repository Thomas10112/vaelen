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
MODULE = "VaelenPresentation"

ACTOR = os.path.join("Source", MODULE, "Private", "VaelenViewActor.cpp")
DRAWER = os.path.join("Source", MODULE, "Private", "VaelenViewDrawer.cpp")
HEADER = os.path.join("Source", MODULE, "Public", "VaelenViewDrawer.h")

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
]


def parses(source_root):
    done = subprocess.run(
        [sys.executable, os.path.join(ROOT, "Tools", "parse_engine_modules.py"),
         "--source-root", source_root, "--quiet"],
        capture_output=True, text=True)
    return done.returncode == 0


def main():
    with tempfile.TemporaryDirectory(prefix="vaelen-shim-test-") as work:
        pristine = os.path.join(work, "pristine")
        os.makedirs(os.path.join(pristine, "Source"))
        shutil.copytree(os.path.join(ROOT, "Source", MODULE),
                        os.path.join(pristine, "Source", MODULE))

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
