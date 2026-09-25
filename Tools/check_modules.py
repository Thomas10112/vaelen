#!/usr/bin/env python3
"""The module lists agree with each other. 19.02, ADR-0157.

A module of this project is named in up to six places, and nothing checked any
two of them against each other until 19.02: its Build.cs, Vaelen.uproject's
Modules, both targets' ExtraModuleNames, Tools/kernel_modules.txt (the modules
CMake builds and the purity check reads) and parse_engine_modules.ENGINE_MODULES
(the ones only the owner's machine builds). The Phase 19 panel found them
already apart: VaelenPresentation was in the uproject and missing from both
targets' ExtraModuleNames. Phase 19 adds two modules and a plugin, so the lists
are checked before they grow.

THE RULES, for every Source/<Name>/<Name>.Build.cs:

    1  the uproject lists <Name> as a module, and lists nothing without a Build.cs;
    2  both targets' ExtraModuleNames name it;
    3  Tools/kernel_modules.txt names it  <=>  Source/<Name>/CMakeLists.txt exists;
    4  ENGINE_MODULES names it            <=>  it has no CMakeLists.txt, except the
                                              kernel modules that stop at a Build.cs;
    5  every plugin module a Build.cs depends on (PLUGIN_OF below) is an enabled
       plugin of the uproject.

    check_modules.py [--root DIR]      the check
    check_modules.py --self-test       the controls; its known answer is HEAD's
"""

import argparse
import json
import os
import re
import shutil
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "Tools"))
from parse_engine_modules import ENGINE_MODULES  # noqa: E402

TARGETS = ("Source/Vaelen.Target.cs", "Source/VaelenEditor.Target.cs")
# A dependency that is a PLUGIN's module, and the plugin that must be enabled
# for it. Written as a table because nothing in a Build.cs says which of its
# dependencies are plugins; the day a Build.cs names another, it goes here.
PLUGIN_OF = {
    "EnhancedInput": "EnhancedInput",
    "ProceduralMeshComponent": "ProceduralMeshComponent",
}
STRINGS = re.compile(r'"([A-Za-z_]\w*)"')
DEPENDENCIES = re.compile(r"(?:Public|Private)DependencyModuleNames\.AddRange\(\s*new\s+string\[\]\s*\{([^}]*)\}",
                          re.S)
EXTRA = re.compile(r"ExtraModuleNames\.AddRange\(\s*new\s+string\[\]\s*\{([^}]*)\}", re.S)


def build_modules(root):
    out = {}
    source = os.path.join(root, "Source")
    for name in sorted(os.listdir(source)):
        rules = os.path.join(source, name, name + ".Build.cs")
        if os.path.isfile(rules):
            out[name] = rules
    return out


def kernel_list(root):
    out = []
    with open(os.path.join(root, "Tools", "kernel_modules.txt"), encoding="utf-8") as f:
        for line in f:
            line = line.split("#", 1)[0].strip()
            if line:
                out.append(line)
    return out


def check(root=ROOT, engine_modules=ENGINE_MODULES):
    refused = []
    modules = build_modules(root)
    with open(os.path.join(root, "Vaelen.uproject"), encoding="utf-8") as f:
        project = json.load(f)
    listed = [m["Name"] for m in project.get("Modules", [])]
    enabled = {p["Name"] for p in project.get("Plugins", []) if p.get("Enabled")}
    kernel = kernel_list(root)

    for name in sorted(set(listed) - set(modules)):
        refused.append("Vaelen.uproject lists module {} and Source/{}/{}.Build.cs does not exist".format(
            name, name, name))
    for name in modules:
        if name not in listed:
            refused.append("{} has a Build.cs and is not a module of Vaelen.uproject".format(name))
    for target in TARGETS:
        with open(os.path.join(root, target), encoding="utf-8") as f:
            text = f.read()
        found = EXTRA.search(text)
        extra = set(STRINGS.findall(found.group(1))) if found else set()
        line = text.count("\n", 0, found.start()) + 1 if found else 0
        for name in modules:
            if name not in extra:
                refused.append("{} absent from ExtraModuleNames at {}:{}".format(name, target, line))
        for name in sorted(extra - set(modules)):
            refused.append("{}:{} names {}, which has no Build.cs".format(target, line, name))
    for name in modules:
        has_cmake = os.path.isfile(os.path.join(root, "Source", name, "CMakeLists.txt"))
        if has_cmake and name not in kernel:
            refused.append("{} has a CMakeLists.txt and is not in Tools/kernel_modules.txt".format(name))
        if not has_cmake and name in kernel:
            refused.append("{} is in Tools/kernel_modules.txt and has no CMakeLists.txt".format(name))
        if not has_cmake and name not in engine_modules:
            refused.append("{} has no CMakeLists.txt, so nothing headless builds it, and is not in "
                           "ENGINE_MODULES, so nothing parses it either".format(name))
        if has_cmake and name in engine_modules:
            refused.append("{} is in ENGINE_MODULES and has a CMakeLists.txt".format(name))
    for name in sorted(set(kernel) - set(modules)):
        refused.append("Tools/kernel_modules.txt names {}, which has no Build.cs".format(name))
    for name, rules in modules.items():
        with open(rules, encoding="utf-8") as f:
            text = f.read()
        needs = set()
        for block in DEPENDENCIES.findall(text):
            needs |= set(STRINGS.findall(block))
        for dependency in sorted(needs):
            plugin = PLUGIN_OF.get(dependency)
            if plugin and plugin not in enabled:
                refused.append("{} depends on {}, whose plugin {} is not enabled in Vaelen.uproject".format(
                    name, dependency, plugin))
    return refused


def report(refused):
    for r in refused:
        print("[modules] REFUSED " + r)
    if refused:
        print("[modules] {} disagreement(s) between the module lists".format(len(refused)))
        return 1
    print("[modules] {} modules: the uproject, both targets, kernel_modules.txt and ENGINE_MODULES agree".format(
        len(build_modules(ROOT))))
    return 0


def self_test():
    failures = []

    def expect(name, ok, detail=""):
        print("[modules self-test] {} {}{}".format("ok  " if ok else "FAIL", name, "" if ok else ": " + detail))
        if not ok:
            failures.append(name)

    got = check()
    expect("CONTROL: the tree as committed agrees", not got, "; ".join(got))

    def copy():
        tmp = tempfile.mkdtemp(prefix="vaelen-modules-")
        shutil.copy(os.path.join(ROOT, "Vaelen.uproject"), tmp)
        os.makedirs(os.path.join(tmp, "Tools"))
        shutil.copy(os.path.join(ROOT, "Tools", "kernel_modules.txt"), os.path.join(tmp, "Tools"))
        for name, rules in build_modules(ROOT).items():
            os.makedirs(os.path.join(tmp, "Source", name))
            shutil.copy(rules, os.path.join(tmp, "Source", name))
            cmake = os.path.join(ROOT, "Source", name, "CMakeLists.txt")
            if os.path.isfile(cmake):
                shutil.copy(cmake, os.path.join(tmp, "Source", name))
        for target in TARGETS:
            shutil.copy(os.path.join(ROOT, target), os.path.join(tmp, "Source"))
        return tmp

    def edit(tmp, relative, find, put):
        path = os.path.join(tmp, relative)
        with open(path, encoding="utf-8") as f:
            text = f.read()
        assert find in text, "{} not in {}".format(find, relative)
        with open(path, "w", encoding="utf-8") as f:
            f.write(text.replace(find, put, 1))

    # THE KNOWN ANSWER: the targets as they stood at the Phase 19 plan (d339068) -
    # VaelenPresentation missing from both, and nothing else.
    tmp = copy()
    for target in TARGETS:
        edit(tmp, target, '"VaelenUI", "VaelenWalk", "VaelenPresentation", "Vaelen"', '"VaelenUI", "VaelenWalk", "Vaelen"')
    got = check(tmp)
    expect("known answer: the targets as the plan found them name exactly VaelenPresentation, twice",
           len(got) == 2 and all(g.startswith("VaelenPresentation absent from ExtraModuleNames") for g in got),
           str(got))
    shutil.rmtree(tmp)

    # 19.06 enabled ProceduralMeshComponent for the walk, so the control is the
    # uproject FORGETTING it: VaelenWalk's Build.cs names it, and is refused.
    tmp = copy()
    edit(tmp, "Vaelen.uproject", '"Name": "ProceduralMeshComponent"', '"Name": "ProceduralMeshComponentGone"')
    got = check(tmp)
    expect("a Build.cs naming a plugin module the uproject does not enable is refused",
           len(got) == 1 and "ProceduralMeshComponent" in got[0] and "not enabled" in got[0], str(got))
    shutil.rmtree(tmp)

    tmp = copy()
    edit(tmp, "Tools/kernel_modules.txt", "VaelenColony\n", "")
    got = check(tmp)
    expect("a kernel module missing from kernel_modules.txt is refused by name",
           len(got) == 1 and got[0].startswith("VaelenColony has a CMakeLists.txt"), str(got))
    shutil.rmtree(tmp)

    tmp = copy()
    got = check(tmp, engine_modules=[m for m in ENGINE_MODULES if m != "VaelenGame"])
    expect("an engine module nothing parses is refused",
           len(got) == 1 and got[0].startswith("VaelenGame has no CMakeLists.txt"), str(got))
    shutil.rmtree(tmp)

    tmp = copy()
    os.makedirs(os.path.join(tmp, "Source", "VaelenGhost"))
    with open(os.path.join(tmp, "Source", "VaelenGhost", "VaelenGhost.Build.cs"), "w", encoding="utf-8") as f:
        f.write("// a module written and listed nowhere\n")
    got = check(tmp)
    expect("a new Build.cs listed nowhere is refused in the uproject, both targets and ENGINE_MODULES",
           len(got) == 4 and all("VaelenGhost" in g for g in got), str(got))
    shutil.rmtree(tmp)

    if failures:
        print("[modules self-test] {} control(s) FAILED".format(len(failures)))
        return 1
    print("[modules self-test] every control holds")
    return 0


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("--root", default=ROOT)
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    if args.self_test:
        return self_test()
    return report(check(args.root))


if __name__ == "__main__":
    sys.exit(main())
