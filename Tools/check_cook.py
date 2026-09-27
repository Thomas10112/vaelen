#!/usr/bin/env python3
"""A packaged build, checked against what the code will ask of it. 22.03, section 27's step 7.

A package that starts and then draws no house is worse than one that refuses
to start: the scenery finds its shapes by name at construction
(`/Engine/BasicShapes/Cube.Cube`, ConstructorHelpers), the ground paints with
the engine's vertex-colour material when the project's tile material is not
on this disk, and the default map is the engine's Entry. None of that is an
asset the cooker can see from a map; all of it is a string in a .cpp. This
reads what UnrealAutomationTool staged - the two manifests every staged
build carries, one line per file - and refuses a package missing any file
the code names, BEFORE a stranger meets it.

    check_cook.py <staged directory or a Manifest_*Files_<platform>.txt>   [--platform Win64]
    check_cook.py --self-test

What is required, and where it comes from:
  - every `/Engine/<path>.<name>` the engine modules' code names (Source/**.cpp,
    a TEXT("/Engine/...") literal): `Engine/Content/<path>.uasset` in the UFS manifest;
  - the GameDefaultMap of Config/DefaultEngine.ini: `Engine/Content/<path>.umap`
    (or `<project>/Content/...` for a project map);
  - the project's Config/Default*.ini and the .uproject, staged as UFS;
  - the executable, Development (`<project>.exe` under Binaries/<platform>) or
    Shipping (`<project>-<platform>-Shipping.exe`), in the NonUFS manifest.
What is optional, and said: every `/Game/<path>.<name>` the code names - the
project's own assets, which the code falls back from when absent (M_VaelenTile).
"""

import argparse
import os
import re
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PROJECT = "Vaelen"
ASSET = re.compile(r'TEXT\("(/(?:Engine|Game)/[A-Za-z0-9_/]+)\.[A-Za-z0-9_]+"\)')
DEFAULT_MAP = re.compile(r"^\s*GameDefaultMap\s*=\s*(/[A-Za-z0-9_/]+)\.[A-Za-z0-9_]+", re.MULTILINE)


def normal(path):
    """One spelling for a staged path: forward slashes, lower case, no leading ./ or /."""
    p = path.replace("\\", "/").strip().lower()
    while p.startswith("./") or p.startswith("/"):
        p = p[2:] if p.startswith("./") else p[1:]
    return p


def named_in_code(root):
    """Every /Engine and /Game asset the engine modules name, by scanning their sources."""
    engine, game = set(), set()
    source = os.path.join(root, "Source")
    for folder, _, files in os.walk(source):
        for name in files:
            if not name.endswith((".cpp", ".h")):
                continue
            with open(os.path.join(folder, name), encoding="utf-8", errors="replace") as f:
                for path in ASSET.findall(f.read()):
                    (engine if path.startswith("/Engine/") else game).add(path)
    return sorted(engine), sorted(game)


def default_map(root):
    with open(os.path.join(root, "Config", "DefaultEngine.ini"), encoding="utf-8", errors="replace") as f:
        m = DEFAULT_MAP.search(f.read())
    return m.group(1) if m else None


def staged_of(asset, extension):
    """`/Engine/BasicShapes/Cube` -> `Engine/Content/BasicShapes/Cube.uasset`; `/Game/X` -> `<project>/Content/X`."""
    if asset.startswith("/Engine/"):
        return "Engine/Content/" + asset[len("/Engine/"):] + extension
    return PROJECT + "/Content/" + asset[len("/Game/"):] + extension


def requirements(root, platform):
    """(required UFS, optional UFS, acceptable executables) as staged paths."""
    engine, game = named_in_code(root)
    ufs = [staged_of(a, ".uasset") for a in engine]
    the_map = default_map(root)
    if the_map is not None:
        ufs.append(staged_of(the_map, ".umap"))
    ufs += [PROJECT + "/Config/DefaultEngine.ini", PROJECT + "/Config/DefaultGame.ini",
            PROJECT + "/Config/DefaultInput.ini", PROJECT + "/" + PROJECT + ".uproject"]
    optional = [staged_of(a, ".uasset") for a in game]
    executables = [PROJECT + "/Binaries/" + platform + "/" + PROJECT + ".exe",
                   PROJECT + "/Binaries/" + platform + "/" + PROJECT + "-" + platform + "-Shipping.exe"]
    return ufs, optional, executables


def read_manifest(path):
    """The staged paths a manifest lists: the first tab-separated field of every line."""
    out = set()
    with open(path, encoding="utf-8", errors="replace") as f:
        for raw in f:
            line = raw.rstrip("\r\n")
            if not line.strip():
                continue
            out.add(normal(line.split("\t")[0]))
    return out


def find_manifests(where, platform):
    """(UFS manifest, NonUFS manifest) from a staged directory or from either file."""
    ufs_name = "Manifest_UFSFiles_" + platform + ".txt"
    non_name = "Manifest_NonUFSFiles_" + platform + ".txt"
    if os.path.isfile(where):
        folder = os.path.dirname(where)
    else:
        folder = where
        for candidate in (where, os.path.join(where, "Windows"), os.path.join(where, platform)):
            if os.path.isfile(os.path.join(candidate, ufs_name)):
                folder = candidate
                break
    ufs = os.path.join(folder, ufs_name)
    non = os.path.join(folder, non_name)
    return (ufs if os.path.isfile(ufs) else None), (non if os.path.isfile(non) else None)


def check(where, platform, root=ROOT):
    """Every refusal, as a sentence; then the notes (optional assets absent). ([], notes) is a package the code can live in."""
    refused, notes = [], []
    ufs_path, non_path = find_manifests(where, platform)
    if ufs_path is None:
        return ["no Manifest_UFSFiles_{}.txt under {}: not a staged build".format(platform, where)], notes
    if non_path is None:
        return ["no Manifest_NonUFSFiles_{}.txt beside {}: not a staged build".format(platform, ufs_path)], notes
    ufs, non = read_manifest(ufs_path), read_manifest(non_path)
    required, optional, executables = requirements(root, platform)
    for path in required:
        if normal(path) not in ufs:
            refused.append("the package lacks {} - the code names it".format(path))
    if not any(normal(e) in non for e in executables):
        refused.append("the package has no executable: none of {}".format(", ".join(executables)))
    for path in optional:
        if normal(path) not in ufs:
            notes.append("{} is not in the package: the code falls back without it".format(path))
    return refused, notes


def self_test(root=ROOT):
    failures = []

    def expect(name, ok, detail=""):
        print("[cook self-test] {} {}{}".format("ok  " if ok else "FAIL", name, "" if ok else ": " + detail))
        if not ok:
            failures.append(name)

    required, optional, executables = requirements(root, "Win64")
    expect("the code names the four basic shapes", all(
        "Engine/Content/BasicShapes/{}.uasset".format(s) in required for s in ("Cube", "Cone", "Cylinder", "Sphere")),
        str(required))
    expect("the default map is required", any(p.endswith(".umap") for p in required), str(required))
    expect("the project's tile material is optional", any("M_VaelenTile" in p for p in optional), str(optional))

    with tempfile.TemporaryDirectory() as tmp:
        def write(ufs, non):
            with open(os.path.join(tmp, "Manifest_UFSFiles_Win64.txt"), "w", encoding="utf-8") as f:
                for p in ufs:
                    f.write(p.replace("/", "\\") + "\t2026.09.27-10.00.00\n")
            with open(os.path.join(tmp, "Manifest_NonUFSFiles_Win64.txt"), "w", encoding="utf-8") as f:
                for p in non:
                    f.write(p + "\t2026.09.27-10.00.00\n")

        # CONTROL: a package with everything the code names, optional included.
        write(required + optional + ["Engine/Content/Something/Else.uasset"], [executables[0]])
        got, notes = check(tmp, "Win64", root)
        expect("CONTROL: a complete package is accepted, backslashes and all", not got and not notes, "; ".join(got + notes))
        # Each required file, missing in turn: refused, and named.
        for victim in required:
            write([p for p in required if p != victim] + optional, [executables[0]])
            got, _ = check(tmp, "Win64", root)
            expect("missing {} is refused by name".format(os.path.basename(victim)),
                   len(got) == 1 and victim in got[0], str(got))
        # No executable: refused; the Shipping name: accepted.
        write(required + optional, [])
        got, _ = check(tmp, "Win64", root)
        expect("a package with no executable is refused", len(got) == 1 and "no executable" in got[0], str(got))
        write(required + optional, [executables[1]])
        got, _ = check(tmp, "Win64", root)
        expect("the Shipping executable's name is accepted", not got, str(got))
        # The optional asset absent: a note, not a refusal.
        write(required, [executables[0]])
        got, notes = check(tmp, "Win64", root)
        expect("the tile material absent is a note and not a refusal", not got and len(notes) == len(optional),
               str(got + notes))
        # Case does not matter on Windows, and the manifest file itself may be named.
        write([p.upper() for p in required] + optional, [executables[0].upper()])
        got, _ = check(os.path.join(tmp, "Manifest_UFSFiles_Win64.txt"), "Win64", root)
        expect("case is not compared, and a manifest may be named instead of its folder", not got, str(got))
        # Not a staged build at all.
        got, _ = check(os.path.join(tmp, "nowhere"), "Win64", root)
        expect("a folder without manifests is refused as not a staged build",
               len(got) == 1 and "not a staged build" in got[0], str(got))

    if failures:
        print("[cook self-test] {} control(s) FAILED".format(len(failures)))
        return 1
    print("[cook self-test] every control holds")
    return 0


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("where", nargs="?")
    parser.add_argument("--platform", default="Win64")
    parser.add_argument("--self-test", action="store_true")
    parser.add_argument("--root", default=ROOT)
    args = parser.parse_args()
    if args.self_test:
        return self_test(args.root)
    if args.where is None:
        parser.error("a staged directory or a manifest, or --self-test")
    refused, notes = check(args.where, args.platform, args.root)
    for r in refused:
        print("[cook] REFUSED " + r)
    for n in notes:
        print("[cook] note: " + n)
    if refused:
        return 1
    required, optional, _ = requirements(args.root, args.platform)
    print("[cook] {}: every one of the {} files the code names is staged ({} optional, {} absent)".format(
        args.where, len(required), len(optional), len(notes)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
