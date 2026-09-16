#!/usr/bin/env python3
"""The same world is wired in four places. Check that they agree. ADR-0135.

WHY THIS EXISTS

VaelenViewActor.cpp carries this comment, and it was right:

    Copied deliberately from AVaelenAtlasActor's KernelRun rather than
    reinvented: [...] a second, subtly different wiring in the same project
    would be a bug waiting for a year when the two disagree.

The year came. Tools/Atlas had gained BondageSystem and the two actors had not,
so the engine drew a world in which nobody was ever in bondage - while bondage
is, after death, the most recorded fact this simulation produces. Nothing said
so. The drift was found by accident, while cross-checking something else.

ADR-0135 fixed the instance. This file is the class: a comment claiming two
things are kept identical is worth nothing unless something checks it, which is
the same reason BiomeKinds has a static_assert and View::AliveState has one.

WHAT IS COMPARED

    the DECLARE order   which *Types::Declare calls, and in what order, because
                        that order fixes component type ids and a system
                        declared in a different position is a different world
    the SYSTEM order    which systems are added to the schedule, and in what
                        order
    the LISTENERS       which event listeners are attached (`X->Attach()`),
                        in what order - a chronicle is not a system and the
                        ADD pattern never sees it, which is how three of them
                        sat in OPTIONAL_SYSTEMS for a week doing nothing

WHAT IS NOT

Rules, RunAfter edges, and anything conditional and deliberately so - listed in
OPTIONAL below, named one by one rather than pattern-matched, so that adding a
system nobody meant to make optional fails here instead of being waved through.
"""

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

WIRINGS = {
    "Tools/Atlas (headless)": "Tools/Atlas/Main.cpp",
    "AVaelenAtlasActor (game)": "Source/Vaelen/Private/VaelenAtlasActor.cpp",
    "AVaelenViewActor (presentation)": "Source/VaelenPresentation/Private/VaelenViewActor.cpp",
    "Run::Aelvor (VaelenRun)": "Source/VaelenRun/Private/Aelvor.cpp",
}

# Deliberately present in some wirings and not others, each named ONE BY ONE
# with its reason. Anything not named here is expected in all four, which is
# the whole point - a pattern like "ignore anything ending in Chronicle" would
# wave through the next system nobody meant to make optional.
#
# Three lists, because the three things compared are different: a TYPE declared
# in a different position shifts every component type id after it, a SYSTEM
# only changes the schedule, and a LISTENER only hears. Colony is declared LAST
# in Tools/Atlas for exactly that reason, and Run::Aelvor declares its Play
# and Lively types after Colony for the same one; this is the note that says so.
OPTIONAL_DECLARES = {
    "Colony": "Tools/Atlas and Run::Aelvor, behind --colony / Options::Colony, declared after Polity so it shifts nothing",
    "PersonChronicle": "Tools/Atlas only, behind --chronicle, declared after Polity",
    "SocietyChronicle": "Tools/Atlas only, behind --chronicle, declared after Polity",
    "EconomyChronicle": "Tools/Atlas only, behind --chronicle, declared after Polity",
    "Player": "Run::Aelvor only, behind Options::Play, declared after Colony so it shifts nothing",
    "Start": "Run::Aelvor only, behind Options::Play, declared after Colony",
    "Hour": "Run::Aelvor only, behind Options::Play, declared after Colony",
    "Order": "Run::Aelvor only, behind Options::Play, declared after Colony",
    "Regard": "Run::Aelvor only, behind Options::Play, declared after Colony",
    "LifeChronicle": "Run::Aelvor only, behind Options::Play, declared after Colony",
    "Living": "Run::Aelvor only, behind Options::Lively, declared after Play's",
    "Repute": "Run::Aelvor only, behind Options::Lively, declared after Play's",
    "Fame": "Run::Aelvor only, behind Options::Lively, declared after Play's",
}
OPTIONAL_SYSTEMS = {
    "MiningSystem": "Tools/Atlas and Run::Aelvor, behind --colony / Options::Colony",
    "DetailSystem": "Run::Aelvor only, behind Options::Stream (15.02); the bridge keeps the crossings, this decides detail on a day",
    "PlayerDaySystem": "Run::Aelvor only, behind Options::Play",
    "PlayerOrderSystem": "Run::Aelvor only, behind Options::Play",
    "RegardSystem": "Run::Aelvor only, behind Options::Play",
    "LivingSystem": "Run::Aelvor only, behind Options::Lively",
    "ReputeSystem": "Run::Aelvor only, behind Options::Lively",
    "FameSystem": "Run::Aelvor only, behind Options::Lively",
    "JudgementSystem": "Run::Aelvor only, behind Options::Lively",
}
OPTIONAL_LISTENERS = {
    "PersonChronicle": "Tools/Atlas only, behind --chronicle",
    "SocietyChronicle": "Tools/Atlas only, behind --chronicle",
    "EconomyChronicle": "Tools/Atlas only, behind --chronicle",
    "LifeChronicle": "Run::Aelvor only, behind Options::Play",
}

DECLARE = re.compile(r"=\s*([A-Za-z_][A-Za-z0-9_]*)Types::Declare\s*\(")
MAKE = re.compile(r"([A-Za-z_][A-Za-z0-9_]*)\s*=\s*std::make_unique<\s*([A-Za-z_][A-Za-z0-9_]*)\s*>")
ADD = re.compile(r"Systems\(\)\.Add\(\s*([A-Za-z_][A-Za-z0-9_]*)\s*\.get\(\)\s*\)")
LISTEN = re.compile(r"([A-Za-z_][A-Za-z0-9_]*)->Attach\(\)")


def read(relative):
    path = ROOT / relative
    if not path.is_file():
        raise SystemExit(f"[wiring] no such file: {relative}")
    return path.read_text(encoding="utf-8")


def declares(text):
    """The TYPE names declared, in order. Local variable names are ignored -
    Tools/Atlas calls its economy types `Economy_` and the actors call theirs
    `Economy`, which is a naming difference and not a wiring one."""
    return DECLARE.findall(text)


def systems(text):
    """The SYSTEM CLASS names added to the schedule, in order. Resolved through
    the make_unique that built each one, for the same reason: `Roads` and
    `Roads_` are the same TradeSystem."""
    built = {name: cls for name, cls in MAKE.findall(text)}
    out = []
    for name in ADD.findall(text):
        out.append(built.get(name, f"<unresolved:{name}>"))
    return out


def listeners(text):
    """The LISTENER CLASS names attached, in order, resolved through the same
    make_unique map. A chronicle is wired by `->Attach()` and never by
    `Systems().Add`, so the ADD pattern cannot see it."""
    built = {name: cls for name, cls in MAKE.findall(text)}
    return [built.get(name, f"<unresolved:{name}>") for name in LISTEN.findall(text)]


def without_optional(names, optional):
    return [n for n in names if n not in optional]


def report(what, found):
    """Print the disagreement as a line-by-line comparison, because a diff of
    two long lists is unreadable and the position is the finding."""
    labels = list(found)
    longest = max(len(v) for v in found.values())
    print(f"\n[wiring] the {what} do not agree:\n", file=sys.stderr)
    width = max(len(l) for l in labels)
    for i in range(longest):
        row = [found[l][i] if i < len(found[l]) else "-" for l in labels]
        mark = "  " if len(set(row)) == 1 else "->"
        print(f"  {mark} {i + 1:2d}. " + "  ".join(f"{v:<28}" for v in row), file=sys.stderr)
    print("\n  columns: " + " | ".join(labels), file=sys.stderr)


def main():
    texts = {label: read(path) for label, path in WIRINGS.items()}

    failures = 0
    for what, extract, optional in (("declarations", declares, OPTIONAL_DECLARES),
                                    ("systems", systems, OPTIONAL_SYSTEMS),
                                    ("listeners", listeners, OPTIONAL_LISTENERS)):
        found = {label: without_optional(extract(text), optional) for label, text in texts.items()}
        unresolved = [v for lst in found.values() for v in lst if v.startswith("<unresolved:")]
        if unresolved:
            print(f"[wiring] could not resolve {', '.join(sorted(set(unresolved)))} to a system class",
                  file=sys.stderr)
            print("[wiring] a system or listener wired without a make_unique this script can see is a",
                  file=sys.stderr)
            print("[wiring] gap in the", file=sys.stderr)
            print("[wiring] script, not a pass - fix Tools/check_world_wiring.py", file=sys.stderr)
            failures += 1
            continue
        if len(set(tuple(v) for v in found.values())) != 1:
            report(what, found)
            failures += 1
        else:
            print(f"[wiring] {what}: {len(next(iter(found.values())))} names, all {len(WIRINGS)} agree")

    if failures:
        print(f"\n[wiring] the {len(WIRINGS)} wirings of AELVOR have drifted apart - see ADR-0135.",
              file=sys.stderr)
        print("[wiring] If a difference is deliberate, name it in OPTIONAL_DECLARES, OPTIONAL_SYSTEMS", file=sys.stderr)
        print("[wiring] or OPTIONAL_LISTENERS with its reason. Do not widen the pattern.", file=sys.stderr)
        return 1
    print(f"[wiring] {len(WIRINGS)} wirings of AELVOR agree "
          f"({len(OPTIONAL_DECLARES)} declarations, {len(OPTIONAL_SYSTEMS)} systems and "
          f"{len(OPTIONAL_LISTENERS)} listeners deliberately optional, each named)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
