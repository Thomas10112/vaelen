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
ENGINE_MODULES = ["VaelenPresentation", "Vaelen", "VaelenGame", "VaelenUI"]

# The modules the UI fence covers (14.07). They are parsed with a LITERAL
# include list and not with kernel_include_dirs(), so that a header the UI may
# not see is "file not found" from a real front end rather than a rule in a
# checker: Take.h, Commands.h and Sim/World.h are not on this path, and after
# 14.02 no header that is declares Vaelen::World.
#
# It is a literal list on purpose. kernel_include_dirs() enumerates every
# Source/*/Public there is, so a filter over it would silently widen the day a
# module is added - which is the failure this whole file exists to prevent.
UI_MODULES = {"VaelenUI"}

def ui_include_dirs(source_root):
    """What VaelenUI may see, and nothing else."""
    out = [module_dir(source_root, "VaelenGame", "Public")]
    for name in ("VaelenView", "VaelenCore", "VaelenPlayer"):
        out.append(os.path.join(ROOT, "Source", name, "Public"))
    return out

# Where a module lives when Source/ does not hold it yet. 14.08 writes
# Source/VaelenGame and 14.09 Source/VaelenUI; until then the tools read the
# witness of 14.07 (Tools/UiWitness/README.md says what it is and is not), so
# that the fence and its self-tests are green BEFORE the code they guard is
# written rather than after. A module with a Private directory under Source
# always wins: the day the real one lands, nothing here has to be told.
WITNESS = os.path.join(ROOT, "Tools", "UiWitness")

GENERATED_INCLUDE = re.compile(r'^\s*#\s*include\s+"([^"]+\.generated\.h)"', re.MULTILINE)


def module_dir(source_root, module, sub):
    return os.path.join(module_root(source_root, module), sub)


def module_root(source_root, module):
    """Where this module's Public and Private live: Source, or the witness."""
    under_source = os.path.join(source_root, "Source", module)
    if os.path.isdir(os.path.join(under_source, "Private")):
        return under_source
    witness = os.path.join(WITNESS, module)
    if os.path.isdir(os.path.join(witness, "Private")):
        return witness
    return under_source


def kernel_include_dirs():
    """Every kernel module's Public directory, which the engine modules may use."""
    out = []
    source = os.path.join(ROOT, "Source")
    for name in sorted(os.listdir(source)):
        public = os.path.join(source, name, "Public")
        if os.path.isdir(public):
            out.append(public)
    return out


def module_files(source_root, module, suffixes):
    """Every file of these kinds anywhere under the module, sorted.

    UnrealBuildTool compiles what is UNDER a module directory, not what is
    under its Private one: a Widgets/ or Classes/ folder, or a subdirectory of
    Private, is built exactly like the rest. Reading only Private's top level
    is how a translation unit gets built by UBT and parsed by nothing - so
    this walks, and the fence (Tools/check_ui_fence.py) walks with it.
    """
    root = module_root(source_root, module)
    if not os.path.isdir(root):
        return []
    out = []
    for here, folders, names in os.walk(root):
        folders.sort()
        for name in sorted(names):
            if name.endswith(suffixes):
                out.append(os.path.join(here, name))
    return sorted(out)


def translation_units(source_root, module):
    return module_files(source_root, module, (".cpp",))


def stub_generated_headers(source_root, modules, into):
    """Write an empty header for every .generated.h these modules include.

    Over EVERY module at once, into ONE directory, because a header of one
    module is included by another: 14.08's VaelenWorldSubsystem.h is included
    by 14.09's HUD, and a stub scan that only saw the module being parsed
    would leave VaelenWorldSubsystem.generated.h "file not found" on the UI's
    first line. --stub-scope own restores the per-module scan, and the
    self-test parses the healthy tree with it to prove this is load-bearing.
    """
    made = []
    for module in modules:
        for source in module_files(source_root, module, (".h", ".cpp")):
            with open(source, "r", encoding="utf-8") as f:
                text = f.read()
            for wanted in GENERATED_INCLUDE.findall(text):
                stub = os.path.join(into, wanted)
                os.makedirs(os.path.dirname(stub) or into, exist_ok=True)
                with open(stub, "w", encoding="utf-8") as f:
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
    # Whether a module's .generated.h stubs are written from EVERY module's
    # includes (shared, the default and what 14.07 made load-bearing) or only
    # from its own, which is what this file did before and what the self-test
    # uses to prove the difference.
    ap.add_argument("--stub-scope", choices=("shared", "own"), default="shared")
    args = ap.parse_args()

    if shutil.which(args.compiler) is None:
        print(f"[parse] no compiler named {args.compiler!r} on PATH", file=sys.stderr)
        return 2

    failures = 0
    checked = 0
    # ONE directory for every module's stubs, made before the first module is
    # parsed. See stub_generated_headers.
    with tempfile.TemporaryDirectory(prefix="vaelen-uht-") as shared:
        if args.stub_scope == "shared":
            stubs = stub_generated_headers(args.source_root, ENGINE_MODULES, shared)
            if args.verbose:
                print(f"[parse] generated stubs: {len(stubs)} ({', '.join(sorted(set(stubs)))})")
        failures, checked = parse_modules(args, shared)

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


def parse_modules(args, shared):
    failures = 0
    checked = 0
    for module in ENGINE_MODULES:
        public = module_dir(args.source_root, module, "Public")
        units = translation_units(args.source_root, module)
        if not units:
            print(f"[parse] {module}: no translation units found", file=sys.stderr)
            failures += 1
            continue

        with tempfile.TemporaryDirectory(prefix="vaelen-uht-own-") as own:
            generated = shared
            if args.stub_scope == "own":
                stub_generated_headers(args.source_root, [module], own)
                generated = own
            if module in UI_MODULES:
                includes = [SHIM, generated, public] + ui_include_dirs(args.source_root)
            else:
                includes = [SHIM, generated, public] + kernel_include_dirs()
            if args.verbose:
                print(f"[parse] {module}: from {module_root(args.source_root, module)}")
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
                "-DVAELEN_SHIM_PARSE=1",
            ]
            # UnrealBuildTool defines <MODULE>_API per module for the dllexport
            # dance. Every engine module here gets its own, and the ones it
            # depends on, rather than one hardcoded name - the second module
            # added to this list is what showed that a constant would not do.
            for name in ENGINE_MODULES:
                command.append(f"-D{name.upper()}_API=")
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
                print(f"[parse] {module}: {len(units)} translation units, OK")
    return failures, checked


if __name__ == "__main__":
    sys.exit(main())
