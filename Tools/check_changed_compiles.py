#!/usr/bin/env python3
"""VAELEN - compile, syntax-only, every translation unit a change can reach.

WHY THIS EXISTS. On 2026-09-21 eight of ten CI legs died at the Build step, on
a one-line mistake, twice in two commits: VT_CHECK_MSG hands its second argument
to std::snprintf as a FORMAT, and both times a value was passed instead of a
literal. -Wformat-security rejects it and this project builds with -Werror, so
no test ran at all on either commit. One of the two sat broken on the branch for
two commits before anybody noticed.

verify_fast was green for both. It checks formatting, kernel purity, the engine
shim, the engine parse, the world wiring and the UI fence - and it never
compiles a test. That was the hole, and this is the patch for it.

WHAT IT CHECKS. Every TU that a change can reach, and nothing else:

  a changed .cpp   ->  itself
  a changed .h     ->  every TU whose build actually included it

The second half is not guessed from #include lines. It is read out of Ninja's
own dependency log, which records the full transitive include set the compiler
reported for each object file. A header three levels down is found the same way
one included directly is.

WHAT IT REFUSES TO DO. It never passes quietly when it could not do its job. No
build directory, no deps log, a TU it cannot map - each of those is an EXIT
CODE, not a warning in the middle of a green run. A check that skips silently
is worse than no check, because it is mistaken for one that ran.

USAGE
    python3 Tools/check_changed_compiles.py [--build DIR] [--all] [--since REF]

    --build DIR   a configured build directory (default: $VAELEN_BUILD_DIR, then
                  build/, then out/, then the first sibling holding a
                  compile_commands.json)
    --all         every TU, not just the reachable ones - the slow, total answer
    --since REF   compare against REF instead of the upstream branch
"""

import argparse
import json
import os
import shlex
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
SOURCE_SUFFIXES = {".cpp", ".cc", ".cxx", ".c"}


def fail(message):
    print(f"[compiles] {message}")
    sys.exit(1)


def run(args, cwd=REPO):
    return subprocess.run(args, cwd=cwd, capture_output=True, text=True)


def find_build_dir(explicit):
    """A configured build directory, or a refusal naming what was tried."""
    tried = []
    candidates = []
    if explicit:
        candidates.append(Path(explicit))
    elif os.environ.get("VAELEN_BUILD_DIR"):
        candidates.append(Path(os.environ["VAELEN_BUILD_DIR"]))
    else:
        candidates += [REPO / "build", REPO / "out", REPO / "cmake-build-debug"]
        for preset in sorted((REPO / "build").glob("*")) if (REPO / "build").is_dir() else []:
            candidates.append(preset)
    for candidate in candidates:
        tried.append(str(candidate))
        if (candidate / "compile_commands.json").is_file():
            return candidate
    fail(
        "no configured build directory. Tried: "
        + ", ".join(tried or ["(none)"])
        + ". Configure one (cmake --preset linux-gcc-debug) or pass --build DIR "
        "or set VAELEN_BUILD_DIR. This check does not skip."
    )


def changed_files(since):
    """Files this push would carry: uncommitted work plus commits not yet upstream."""
    if since:
        base = since
    else:
        upstream = run(["git", "rev-parse", "--abbrev-ref", "--symbolic-full-name", "@{upstream}"])
        base = upstream.stdout.strip() if upstream.returncode == 0 else "HEAD"
    diff = run(["git", "diff", "--name-only", base])
    if diff.returncode != 0:
        fail(f"git diff against {base} failed: {diff.stderr.strip()}")
    names = set(diff.stdout.split())
    untracked = run(["git", "ls-files", "--others", "--exclude-standard"])
    names.update(untracked.stdout.split())
    return base, {(REPO / n).resolve() for n in names if n}


def load_entries(build):
    with (build / "compile_commands.json").open() as handle:
        raw = json.load(handle)
    entries = {}
    for entry in raw:
        entries[Path(entry["file"]).resolve()] = entry
    return entries


def includers(build, entries):
    """header -> {TU source paths}, read from Ninja's own recorded dependencies."""
    if not (build / ".ninja_deps").is_file():
        fail(
            f"{build} has no .ninja_deps. A header change cannot be mapped to the "
            "translation units that include it without one. Build the directory "
            "once, or pass --all to check every unit instead. This check does not skip."
        )
    dump = run(["ninja", "-t", "deps"], cwd=build)
    if dump.returncode != 0:
        fail(f"`ninja -t deps` failed in {build}: {dump.stderr.strip()[:200]}")
    mapping = {}
    current = None
    for line in dump.stdout.splitlines():
        if not line.startswith(" "):
            current = None
            continue
        included = Path(line.strip())
        if current is None:
            # The first indented line of a block is the TU's own source file.
            current = included.resolve() if included.suffix in SOURCE_SUFFIXES else None
            if current is not None and current not in entries:
                current = None
            continue
        mapping.setdefault(included.resolve(), set()).add(current)
    if not mapping:
        fail(f"`ninja -t deps` in {build} recorded nothing. Build it once first.")
    return mapping


def reachable(changed, entries, mapping):
    hit = set()
    for path in changed:
        if path in entries:
            hit.add(path)
        hit.update(mapping.get(path, set()))
    return hit


def syntax_only(entry):
    """The build's own command for this TU, minus its output, plus -fsyntax-only."""
    argv = shlex.split(entry["command"]) if "command" in entry else list(entry["arguments"])
    out = []
    skip = False
    for token in argv:
        if skip:
            skip = False
            continue
        if token == "-o":
            skip = True
            continue
        if token.startswith("-o") and len(token) > 2:
            continue
        out.append(token)
    out.append("-fsyntax-only")
    result = subprocess.run(out, cwd=entry["directory"], capture_output=True, text=True)
    return entry["file"], result.returncode, (result.stderr or result.stdout).strip()


def main():
    parser = argparse.ArgumentParser(add_help=True)
    parser.add_argument("--build")
    parser.add_argument("--all", action="store_true")
    parser.add_argument("--since")
    args = parser.parse_args()

    build = find_build_dir(args.build)
    entries = load_entries(build)

    if args.all:
        targets = set(entries)
        base = "(every translation unit)"
    else:
        base, changed = changed_files(args.since)
        if not changed:
            print(f"[compiles] nothing changed against {base}, 0 translation units to check")
            return 0
        targets = reachable(changed, entries, includers(build, entries))
        if not targets:
            print(
                f"[compiles] {len(changed)} file(s) changed against {base}, none of them "
                "reaches a translation unit this build compiles"
            )
            return 0

    with ThreadPoolExecutor(max_workers=os.cpu_count() or 4) as pool:
        results = list(pool.map(syntax_only, (entries[t] for t in sorted(targets))))

    broken = [r for r in results if r[1] != 0]
    for path, _, message in broken:
        print(f"[compiles] FAILED {os.path.relpath(path, REPO)}")
        for line in message.splitlines()[:12]:
            print(f"    {line}")
    if broken:
        print(f"[compiles] {len(broken)} of {len(results)} translation unit(s) do not compile")
        return 1
    print(
        f"[compiles] {len(results)} translation unit(s) reachable from the change "
        f"against {base} compile clean, with the build's own flags"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
