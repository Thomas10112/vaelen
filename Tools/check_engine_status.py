#!/usr/bin/env python3
"""Every engine file's STATUS, held to what was built. 19.01, ADR-0159.

The four engine modules (parse_engine_modules.ENGINE_MODULES) are the only code
in this repository that no leg of the CI compiles. What a file there says about
itself is therefore the whole record of whether it ever ran - and on
2026-09-25 five files said VALIDATED over code no compiler had read.

THE RULE. The first `// STATUS:` line of every engine file names one of

    VALIDATED | BUILT | PROTOTYPE     a claim that an engine build compiled it.
                                      The file must also carry `// BUILD: <id>`
                                      naming a row of Tools/engine_builds.txt,
                                      that build's record must hold the file,
                                      and the file's CODE must be what that
                                      build compiled: same fingerprint, comments
                                      ignored (Tools/engine_ledger.py).
    UNVERIFIED (engine) | INCOMPLETE  no claim. A `// BUILD:` line may still name
                                      the last build that compiled an earlier
                                      form of the file; it must be a real row.

So a comment may be reworded under a VALIDATED line and a single token of code
may not. A file whose code moved since its build is refused by name, and the
way to make it green is to say UNVERIFIED (engine) - or to build it.

    check_engine_status.py [--root DIR]     the check
    check_engine_status.py --self-test      the controls below
    check_engine_status.py --record ID      derive Tools/EngineBuilds/ID.txt from git

THE KNOWN ANSWER (--self-test): every engine file marked VALIDATED at build
b0921 (15.10, 867a129) must be refused for exactly the files whose code moved
since 15.10 or were written after it (MOVED_SINCE_B0921, NEW_SINCE_B0921 below:
seven on 2026-09-25 at 19.01, twenty-four after 19.06 wrote the walk) - while
the three whose comments alone changed (VaelenViewDrawer.cpp/.h, VaelenViewActor.h)
pass. That set is a measurement of git, not of how anybody marked anything, and
a task that moves engine code adds its files to it.
"""

import argparse
import os
import re
import shutil
import sys
import tempfile

import engine_ledger as L

STATUS = re.compile(r"^\s*//\s*STATUS:\s*(.*)$")
BUILD = re.compile(r"^\s*//\s*BUILD:\s*(\S+)")
CLAIMS = ("VALIDATED", "BUILT", "PROTOTYPE")
NO_CLAIM = ("UNVERIFIED (engine)", "INCOMPLETE")


def status_of(text):
    """(status text, build id, status line number) - None where absent."""
    status, build, where = None, None, None
    for number, line in enumerate(text.split("\n"), 1):
        if status is None:
            m = STATUS.match(line)
            if m:
                status, where = m.group(1).strip(), number
        if build is None:
            m = BUILD.match(line)
            if m:
                build = m.group(1)
    return status, build, where


def check(root, ledger_path=L.LEDGER, records=L.RECORDS):
    """Every refusal, as (path, reason). Empty means every file says no more than was built."""
    builds = {b.ident: b for b in L.load_ledger(ledger_path)}
    loaded = {}
    refused = []
    files = L.engine_files(root)
    if not files:
        return [("Source", "no engine file found under {}".format(root))]
    for path in files:
        text = L.read(root, path)
        status, build, where = status_of(text)
        if status is None:
            refused.append((path, "no // STATUS: line"))
            continue
        claim = next((c for c in CLAIMS if status.startswith(c)), None)
        if claim is None and not any(status.startswith(n) for n in NO_CLAIM):
            refused.append((path, "line {}: STATUS '{}' is none of {} / {}".format(
                where, status[:40], "|".join(CLAIMS), "|".join(NO_CLAIM))))
            continue
        if build is not None and build not in builds:
            refused.append((path, "BUILD {} is no row of Tools/engine_builds.txt".format(build)))
            continue
        if claim is None:
            continue
        if build is None:
            refused.append((path, "line {}: says {} and names no // BUILD: row".format(where, claim)))
            continue
        if build not in loaded:
            loaded[build] = L.load_record(build, records)
        record = loaded[build]
        if path not in record.files:
            refused.append((path, "says {} at {}, and build {} did not contain this file".format(
                claim, build, build)))
            continue
        now = L.fingerprint(text)
        if now != record.files[path]:
            refused.append((path, "says {} at {} ({}), and its code has changed since: {} then, {} now".format(
                claim, build, builds[build].commit, record.files[path], now)))
    return refused


def report(refused, root):
    for path, reason in refused:
        print("[engine-status] REFUSED {}: {}".format(path, reason))
    if refused:
        print("[engine-status] {} file(s) claim more than was built".format(len(refused)))
        return 1
    print("[engine-status] {} engine files, every STATUS within what was built ({} builds in the ledger)".format(
        len(L.engine_files(root)), len(L.load_ledger())))
    return 0


# ── the controls ────────────────────────────────────────────────────────────

MOVED_SINCE_B0921 = {
    "Source/Vaelen/Private/VaelenAtlasActor.cpp",
    "Source/VaelenGame/Private/VaelenPlayCommands.cpp",
    "Source/VaelenGame/Private/VaelenWorldSubsystem.cpp",
    "Source/VaelenGame/Public/VaelenWorldSubsystem.h",
    "Source/VaelenPresentation/Private/VaelenViewActor.cpp",
    # 19.10: the controller binds from the host's table; 19.06: the look helper is
    # virtual, and both module rules gained a dependency.
    "Source/VaelenUI/Private/VaelenPlayerController.cpp",
    "Source/VaelenUI/Public/VaelenPlayerController.h",
    "Source/VaelenUI/VaelenUI.Build.cs",
    "Source/VaelenGame/VaelenGame.Build.cs",
}
NEW_SINCE_B0921 = {
    "Source/VaelenGame/Private/VaelenCheckpointStore.cpp",
    "Source/VaelenGame/Private/VaelenCheckpointStore.h",
    # 19.06: the walk, a module no build has seen.
    "Source/VaelenUI/Private/VaelenWalkController.cpp",
    "Source/VaelenUI/Public/VaelenWalkController.h",
    "Source/VaelenWalk/Private/VaelenLand.cpp",
    "Source/VaelenWalk/Private/VaelenSky.cpp",
    "Source/VaelenWalk/Private/VaelenWalkCommands.cpp",
    "Source/VaelenWalk/Private/VaelenWalkGameMode.cpp",
    "Source/VaelenWalk/Private/VaelenWalkModule.cpp",
    "Source/VaelenWalk/Private/VaelenWalker.cpp",
    "Source/VaelenWalk/Public/VaelenLand.h",
    "Source/VaelenWalk/Public/VaelenSky.h",
    "Source/VaelenWalk/Public/VaelenWalkGameMode.h",
    "Source/VaelenWalk/Public/VaelenWalker.h",
    "Source/VaelenWalk/VaelenWalk.Build.cs",
}
COMMENTS_ONLY_SINCE_B0921 = {
    "Source/VaelenPresentation/Private/VaelenViewDrawer.cpp",
    "Source/VaelenPresentation/Public/VaelenViewDrawer.h",
    "Source/VaelenPresentation/Public/VaelenViewActor.h",
}


def copy_tree(into):
    for path in L.engine_files(L.ROOT):
        dest = os.path.join(into, path)
        os.makedirs(os.path.dirname(dest), exist_ok=True)
        shutil.copyfile(os.path.join(L.ROOT, path), dest)


def rewrite(root, path, change):
    full = os.path.join(root, path)
    with open(full, encoding="utf-8") as f:
        text = f.read()
    after = change(text)
    assert after != text, "the mutation of {} changed nothing".format(path)
    with open(full, "w", encoding="utf-8") as f:
        f.write(after)


def claim_everything(text, build="b0921"):
    """Every file VALIDATED at one build, whatever it said before."""
    lines = text.split("\n")
    out, done = [], False
    for line in lines:
        if BUILD.match(line):
            continue
        if not done and STATUS.match(line):
            out.append("// STATUS: VALIDATED (self-test)")
            out.append("// BUILD: " + build)
            done = True
            continue
        out.append(line)
    if not done:
        out = ["// STATUS: VALIDATED (self-test)", "// BUILD: " + build] + out
    return "\n".join(out)


def self_test():
    failures = []

    def expect(name, ok, detail=""):
        print("[engine-status self-test] {} {}{}".format("ok  " if ok else "FAIL", name,
                                                          "" if ok else ": " + detail))
        if not ok:
            failures.append(name)

    # CONTROL: the tree as it stands.
    got = check(L.ROOT)
    expect("the tree as committed is within what was built", not got, "; ".join(p for p, _ in got))

    with tempfile.TemporaryDirectory() as tmp:
        # THE KNOWN ANSWER: claim everything at b0921 and let git's fingerprints say which files moved.
        copy_tree(tmp)
        for path in L.engine_files(tmp):
            rewrite(tmp, path, claim_everything)
        named = {p for p, _ in check(tmp)}
        expect("claiming b0921 everywhere names exactly the moved and the newer files (a measurement of git, kept here)",
               named == MOVED_SINCE_B0921 | NEW_SINCE_B0921,
               "named {}".format(sorted(named)))
        expect("the 3 files whose comments alone changed since b0921 are not named",
               not (named & COMMENTS_ONLY_SINCE_B0921), "named {}".format(sorted(named & COMMENTS_ONLY_SINCE_B0921)))

    with tempfile.TemporaryDirectory() as tmp:
        copy_tree(tmp)
        hud = "Source/VaelenUI/Private/VaelenHUD.cpp"
        # A comment changed under VALIDATED: still the code that was built.
        rewrite(tmp, hud, lambda t: t.replace("// ", "// (reworded) ", 3))
        expect("a changed comment under VALIDATED is accepted", not check(tmp))
        # One code token changed: refused, by name.
        rewrite(tmp, hud, lambda t: re.sub(r"\bconst\b", "const volatile", t, count=1))
        named = check(tmp)
        expect("one code token changed in VaelenHUD.cpp is refused as changed since its build",
               len(named) == 1 and named[0][0] == hud and "changed since" in named[0][1], str(named))

    with tempfile.TemporaryDirectory() as tmp:
        copy_tree(tmp)
        hud = "Source/VaelenUI/Private/VaelenHUD.cpp"
        rewrite(tmp, hud, lambda t: re.sub(r"(//\s*BUILD:\s*)\S+", r"\1b9999", t, count=1))
        named = check(tmp)
        expect("a BUILD naming no ledger row is refused",
               len(named) == 1 and "no row" in named[0][1], str(named))

    with tempfile.TemporaryDirectory() as tmp:
        copy_tree(tmp)
        hud = "Source/VaelenUI/Private/VaelenHUD.cpp"
        rewrite(tmp, hud, lambda t: re.sub(r"//\s*STATUS:", "// STATE:", t, count=1))
        named = check(tmp)
        expect("a file with no STATUS line is refused", len(named) == 1 and "no // STATUS" in named[0][1],
               str(named))

    with tempfile.TemporaryDirectory() as tmp:
        copy_tree(tmp)
        store = "Source/VaelenGame/Private/VaelenCheckpointStore.cpp"
        rewrite(tmp, store, lambda t: t.replace("STATUS: UNVERIFIED (engine)", "STATUS: VALIDATED", 1))
        named = check(tmp)
        expect("VALIDATED with no BUILD line is refused",
               len(named) == 1 and named[0][0] == store and "names no" in named[0][1], str(named))

    # THE RECORDS ARE GIT'S: re-derived where the history exists, byte for byte.
    rows = L.load_ledger()
    missing = [b.ident for b in rows if not L.has_commit(b.commit)]
    if missing:
        print("[engine-status self-test] note: records of {} not re-derived here - their commits are not "
              "in this clone (the CI clones at depth 1); they were re-derived where they are".format(
                  ", ".join(missing)))
    for b in rows:
        if b.ident in missing:
            continue
        with open(L.record_path(b.ident), encoding="utf-8") as f:
            committed = f.read()
        expect("record {} re-derives from {} byte for byte".format(b.ident, b.commit),
               committed == L.record_text(b))

    if failures:
        print("[engine-status self-test] {} control(s) FAILED".format(len(failures)))
        return 1
    print("[engine-status self-test] every control holds")
    return 0


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("--root", default=L.ROOT)
    parser.add_argument("--self-test", action="store_true")
    parser.add_argument("--record", metavar="ID")
    args = parser.parse_args()
    if args.self_test:
        return self_test()
    if args.record:
        build = next((b for b in L.load_ledger() if b.ident == args.record), None)
        if build is None:
            print("no row {} in Tools/engine_builds.txt".format(args.record))
            return 1
        os.makedirs(L.RECORDS, exist_ok=True)
        with open(L.record_path(build.ident), "w", encoding="utf-8", newline="\n") as f:
            f.write(L.record_text(build))
        print("recorded {} from {}".format(build.ident, build.commit))
        return 0
    return report(check(args.root), args.root)


if __name__ == "__main__":
    sys.exit(main())
