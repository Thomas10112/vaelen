#!/usr/bin/env python3
"""Which names of the shim a real engine has compiled, and which are beliefs. 19.01, ADR-0159.

Tools/EngineShim is what the engine modules are parsed against in CI, and it is
written from memory of Unreal's headers. A shim entry nobody has compiled
against the real engine is a BELIEF, and a wrong belief does not turn the CI
red - it turns it GREEN over code that will not build (14.08 found three such
defects on the owner's machine, ENGINE_HANDOFF.md). So the belief has to be
visible, and has to go away only when a build has seen it.

WHAT IS A NAME HERE. Every identifier in the shim's code (comments and string
contents stripped, keywords removed) that the engine modules' code also uses.
Such a name is SEEN when an owner build compiled engine code that used it - the
`seen` lines of Tools/EngineBuilds/<id>.txt, derived from git at that build's
commit against the shim of that commit. Every other one is a BELIEF, and must
be listed, with the task that introduced the use, in Tools/shim_beliefs.txt:

    <name>  <task>  <why the engine code needs it>

The check refuses a belief that is not listed (naming the first place the
engine code uses it), a listed name a build has since seen (drop it), and a
listed name no engine code uses (a belief about nothing).

WHAT IT DOES NOT PROVE. That the SHAPE the shim gives a seen name is the
engine's: a build that used FString::Printf proves Printf exists, not that the
shim's overloads match. The ledger makes beliefs countable; it does not make
the shim right.

    check_shim_ledger.py                 the check
    check_shim_ledger.py --self-test     the controls below
    check_shim_ledger.py --diff A B      names the engine code used at B and not at A (needs git)
"""

import argparse
import os
import re
import shutil
import sys
import tempfile

import engine_ledger as L

BELIEFS = os.path.join(L.ROOT, "Tools", "shim_beliefs.txt")
LINE = re.compile(r"^([A-Za-z_]\w*)\s+(\d{2}\.\d{2}\w*)\s+(\S.*)$")


def load_beliefs(path=BELIEFS):
    out = {}
    with open(path, encoding="utf-8") as f:
        for number, line in enumerate(f, 1):
            line = line.rstrip("\n")
            if not line.strip() or line.lstrip().startswith("#"):
                continue
            m = LINE.match(line)
            if not m:
                raise ValueError("{}:{}: not '<name> <task> <why>': {}".format(path, number, line))
            if m.group(1) in out:
                raise ValueError("{}:{}: {} listed twice".format(path, number, m.group(1)))
            out[m.group(1)] = (m.group(2), number)
    return out


def all_seen(records=L.RECORDS, ledger=L.LEDGER):
    seen = {}
    for b in L.load_ledger(ledger):
        for name in L.load_record(b.ident, records).seen:
            seen.setdefault(name, b.ident)
    return seen


def check(root=L.ROOT, shim=L.SHIM, beliefs_path=BELIEFS, records=L.RECORDS):
    used = L.used_names(root)
    vocabulary = L.shim_vocabulary(shim)
    seen = all_seen(records)
    listed = load_beliefs(beliefs_path)
    beliefs = {n for n in used if n in vocabulary and n not in seen}
    refused = []
    for name in sorted(beliefs - set(listed)):
        refused.append("{} is a belief nobody listed: the engine code uses it at {} and no build has "
                       "compiled a use of it".format(name, used[name]))
    for name in sorted(set(listed) - beliefs):
        task, line = listed[name]
        if name in seen:
            refused.append("shim_beliefs.txt:{}: {} was SEEN by build {} - it is no longer a belief".format(
                line, name, seen[name]))
        elif name not in used:
            refused.append("shim_beliefs.txt:{}: {} is used by no engine file - a belief about nothing".format(
                line, name))
        else:
            refused.append("shim_beliefs.txt:{}: {} is not a name of the shim".format(line, name))
    return refused, beliefs, used


def report(refused, beliefs):
    for r in refused:
        print("[shim-ledger] REFUSED " + r)
    if refused:
        print("[shim-ledger] {} refusal(s)".format(len(refused)))
        return 1
    print("[shim-ledger] {} shim names used by the engine code are beliefs no build has compiled, "
          "each listed in Tools/shim_beliefs.txt".format(len(beliefs)))
    return 0


def diff(rev_a, rev_b):
    """What the engine code started using between two commits, of today's shim."""
    vocabulary = L.shim_vocabulary()
    def used_at(rev):
        code, listing = L.git("ls-tree", "-r", "--name-only", rev, "--",
                              *["Source/" + m for m in L.ENGINE_MODULES])
        if code != 0:
            raise SystemExit("git cannot list {} - this needs the history".format(rev))
        out = set()
        for path in listing.split():
            _, text = L.git("show", "{}:{}".format(rev, path))
            out |= L.identifiers(text)
        return out
    new = sorted((used_at(rev_b) & vocabulary) - used_at(rev_a))
    for name in new:
        print("[shim-ledger] new since {}: {}".format(rev_a, name))
    print("[shim-ledger] {}..{}: {} shim name(s) the engine code started using".format(rev_a, rev_b, len(new)))
    return 0


def self_test():
    failures = []

    def expect(name, ok, detail=""):
        print("[shim-ledger self-test] {} {}{}".format("ok  " if ok else "FAIL", name, "" if ok else ": " + detail))
        if not ok:
            failures.append(name)

    refused, beliefs, _ = check()
    expect("the tree as committed: every belief listed, nothing else listed", not refused, "; ".join(refused))
    expect("there is at least one belief, so the check is not vacuous", len(beliefs) > 0)

    with tempfile.TemporaryDirectory() as tmp:
        listing = os.path.join(tmp, "beliefs.txt")
        with open(BELIEFS, encoding="utf-8") as f:
            text = f.read()
        victim = sorted(beliefs)[0]
        with open(listing, "w", encoding="utf-8") as f:
            f.write(re.sub(r"(?m)^{}\s.*\n".format(re.escape(victim)), "", text))
        refused, _, _ = check(beliefs_path=listing)
        expect("an unlisted belief is refused, naming where the engine code uses it",
               len(refused) == 1 and refused[0].startswith(victim + " ") and " at Source/" in refused[0],
               str(refused))

        with open(listing, "w", encoding="utf-8") as f:
            f.write(text + "GetGameInstance  19.01  a seen name listed as a belief\n")
        refused, _, _ = check(beliefs_path=listing)
        expect("a listed name a build has seen is refused as no longer a belief",
               len(refused) == 1 and "SEEN by build" in refused[0], str(refused))

        with open(listing, "w", encoding="utf-8") as f:
            f.write(text + "ANameNoEngineFileUses  19.01  nothing\n")
        refused, _, _ = check(beliefs_path=listing)
        expect("a listed name no engine file uses is refused", len(refused) == 1 and "used by no" in refused[0],
               str(refused))

    with tempfile.TemporaryDirectory() as tmp:
        # A shim entry added and USED by engine code, and nobody lists it: the day 19.02 forgets.
        for path in L.engine_files(L.ROOT):
            dest = os.path.join(tmp, path)
            os.makedirs(os.path.dirname(dest), exist_ok=True)
            shutil.copyfile(os.path.join(L.ROOT, path), dest)
        shim = os.path.join(tmp, "Shim")
        shutil.copytree(L.SHIM, shim)
        with open(os.path.join(shim, "SelfTestCharacter.h"), "w", encoding="utf-8") as f:
            f.write("#pragma once\nclass ASelfTestCharacter { public: void SetWalkableFloorAngle(float); };\n")
        hud = os.path.join(tmp, "Source", "VaelenUI", "Private", "VaelenHUD.cpp")
        with open(hud, "a", encoding="utf-8") as f:
            f.write("\nstatic void Use(ASelfTestCharacter& C) { C.SetWalkableFloorAngle(44.76f); }\n")
        refused, _, _ = check(root=tmp, shim=shim)
        named = sorted(r.split(" ")[0] for r in refused)
        expect("a new shim entry used by engine code and listed nowhere is refused, both names",
               named == ["ASelfTestCharacter", "SetWalkableFloorAngle"] and all("VaelenHUD.cpp:" in r for r in refused),
               str(refused))
        # CONTROL: the same use inside a comment is no use at all.
        with open(hud, encoding="utf-8") as f:
            body = f.read()
        with open(hud, "w", encoding="utf-8") as f:
            f.write(body.replace("\nstatic void Use(", "\n// static void Use("))
        refused, _, _ = check(root=tmp, shim=shim)
        expect("the same use written in a comment is not a use", not refused, str(refused))

    if failures:
        print("[shim-ledger self-test] {} control(s) FAILED".format(len(failures)))
        return 1
    print("[shim-ledger self-test] every control holds")
    return 0


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("--self-test", action="store_true")
    parser.add_argument("--diff", nargs=2, metavar=("A", "B"))
    args = parser.parse_args()
    if args.self_test:
        return self_test()
    if args.diff:
        return diff(*args.diff)
    refused, beliefs, _ = check()
    return report(refused, beliefs)


if __name__ == "__main__":
    sys.exit(main())
