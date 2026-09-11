#!/usr/bin/env python3
"""Parse the Unreal-facing modules against Tools/EngineShim. ADR-0134.

WHAT THIS IS FOR

VaelenPresentation is the only module in the project that no build on a CI
runner and no build on the kernel developer's machine ever compiles. It needs
an engine; the CI has none. That reasoning is right about VALIDATION and wrong
about SYNTAX, and the difference cost two round trips on the project owner's
machine:

    13.07c  ReliefScale renamed in the settings struct, the actor still
            assigning the old name    -> error C2039, found by the owner
    13.08b  a fourth view, a fifth component, two new functions -> found here

This script is the second row's answer. It runs a real C++20 front end over the
module's translation units with a minimal shim standing in for the engine, and
fails on anything that is not a well-formed program.

WHAT A GREEN RUN PROVES

    Names resolve. Signatures match. A renamed member is gone, an argument
    count is checked, a type that changed is caught.

WHAT IT DOES NOT PROVE

    That the module BUILDS under UnrealBuildTool, that UHT accepts the UCLASS,
    or that anything DRAWS. VaelenPresentation stays UNVERIFIED until an editor
    has run it. ADR-0134 would be wrong if it moved a single STATUS line, and
    it does not.

THE .generated.h STUBS ARE MADE HERE, NOT CHECKED IN

Every UCLASS header includes a .generated.h that UnrealHeaderTool writes. This
script writes an empty stub for each one it finds INCLUDED, into a temporary
directory. Generating them rather than committing them is the point: a new
actor added next year is covered the day it is written, instead of being the
one file nobody remembered to add to a list.
"""

import argparse
import os
import re
import shutil
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SHIM = os.path.join(ROOT, "Tools", "EngineShim")

# The modules that need an engine to build. Deliberately a short, explicit
# list and not a scan: a module that appears here is a module somebody decided
# cannot be covered by the real headless build, and that decision should be
# visible in a file rather than inferred from a directory layout.
ENGINE_MODULES = ["VaelenPresentation"]

GENERATED_INCLUDE = re.compile(r'^\s*#\s*include\s+"([^"]+\.generated\.h)"', re.MULTILINE)


def module_dir(source_root, module, sub):
    return os.path.join(source_root, "Source", module, sub)


def kernel_include_dirs():
    """Every kernel module's Public directory, which the engine modules may use."""
    out = []
    source = os.path.join(ROOT, "Source")
    for name in sorted(os.listdir(source)):
        public = os.path.join(source, name, "Public")
        if os.path.isdir(public):
            out.append(public)
    return out


def translation_units(source_root, module):
    private = module_dir(source_root, module, "Private")
    if not os.path.isdir(private):
        return []
    return [os.path.join(private, f) for f in sorted(os.listdir(private)) if f.endswith(".cpp")]


def stub_generated_headers(source_root, module, into):
    """Write an empty header for every .generated.h the module includes."""
    made = []
    for sub in ("Public", "Private"):
        folder = module_dir(source_root, module, sub)
        if not os.path.isdir(folder):
            continue
        for name in sorted(os.listdir(folder)):
            if not name.endswith((".h", ".cpp")):
                continue
            with open(os.path.join(folder, name), "r", encoding="utf-8") as f:
                text = f.read()
            for wanted in GENERATED_INCLUDE.findall(text):
                path = os.path.join(into, wanted)
                os.makedirs(os.path.dirname(path) or into, exist_ok=True)
                with open(path, "w", encoding="utf-8") as f:
                    f.write(
                        "// Written by Tools/parse_engine_modules.py, not by UnrealHeaderTool.\n"
                        "// The real one declares reflection boilerplate; GENERATED_BODY() is a\n"
                        "// no-op in Tools/EngineShim, so an empty header is the honest stand-in.\n"
                        "#pragma once\n"
                    )
                made.append(wanted)
    return made


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--compiler", default=os.environ.get("CXX", "clang++"))
    ap.add_argument("--verbose", action="store_true")
    # Where the ENGINE modules' own sources live. Defaults to the repository,
    # and exists so Tools/test_engine_shim.py can parse a deliberately broken
    # copy without ever writing a broken file into the working tree.
    ap.add_argument("--source-root", default=ROOT)
    ap.add_argument("--quiet", action="store_true")
    args = ap.parse_args()

    if shutil.which(args.compiler) is None:
        print(f"[parse] no compiler named {args.compiler!r} on PATH", file=sys.stderr)
        return 2

    failures = 0
    checked = 0
    for module in ENGINE_MODULES:
        public = module_dir(args.source_root, module, "Public")
        units = translation_units(args.source_root, module)
        if not units:
            print(f"[parse] {module}: no translation units found", file=sys.stderr)
            failures += 1
            continue

        with tempfile.TemporaryDirectory(prefix="vaelen-uht-") as generated:
            stubs = stub_generated_headers(args.source_root, module, generated)
            includes = [SHIM, generated, public] + kernel_include_dirs()
            command = [
                args.compiler,
                "-std=c++20",
                "-fsyntax-only",
                "-Wall",
                "-Wextra",
                "-Werror",
                # The shim's stand-in bodies do nothing with what they are
                # handed, which is the whole idea; the warnings that produces
                # are about the shim and not about the code under test.
                "-Wno-unused-parameter",
                "-Wno-unused-private-field",
                "-DVAELENPRESENTATION_API=",
                "-DVAELEN_SHIM_PARSE=1",
            ]
            for d in includes:
                command += ["-I", d]
            command += units

            if args.verbose:
                print("[parse] " + " ".join(command))
            done = subprocess.run(command, capture_output=True, text=True)
            checked += len(units)
            if done.returncode != 0:
                failures += 1
                if not args.quiet:
                    print(f"[parse] {module}: FAILED", file=sys.stderr)
                    sys.stderr.write(done.stderr)
            elif not args.quiet:
                print(f"[parse] {module}: {len(units)} translation units, "
                      f"{len(stubs)} generated stubs, OK")

    if failures:
        if not args.quiet:
            print(f"[parse] {failures} module(s) failed to parse", file=sys.stderr)
        return 1
    if args.quiet:
        return 0
    print(f"[parse] {checked} translation units parsed against Tools/EngineShim, 0 errors")
    print("[parse] this proves the SHAPE of a program and nothing about the engine "
          "- see Tools/EngineShim/CoreMinimal.h")
    return 0


if __name__ == "__main__":
    sys.exit(main())
