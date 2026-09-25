#!/usr/bin/env python3
"""A classified census of every frozen sixteen-hex literal, and a diff of them.

WHY THIS EXISTS. Phase 17's gate clause (i) - "no frozen digest moved" - was
measured with

    git diff A..B -- Tests/ Source/ Tools/ | grep -E '^-.*\\b[0-9a-f]{16}\\b'

and that grep prints 0 over be9df8d, the ADR-0131 re-freeze that removed 55
`0x...ull` literals: `\\b` finds no word boundary between the last hex digit
and the `ull` suffix, and 394 of the tree's 415 sixteen-hex sites carry one.
The instrument could not see the thing it was meant to see, and it had never
failed on purpose (ADR-0149). Phase 18 moves digests deliberately, once, in
one commit; this file is what says WHICH moved and which did not, by class
and by kind, and its self-test has a known answer in git.

WHAT IS CLASSIFIED. Every `0x` + sixteen hex digits in .cpp/.h/.py under
Tests/, Source/ and Tools/; every bare sixteen-hex token in the two test
CMakeLists and the Tests/Run/*.cmake drivers; every bare token in the three
corpus READMEs. Each site gets a CLASS from an EXPLICIT table of files, named
one by one (the check_world_wiring.py rule: no widening pattern, so a new file
with a literal is refused here rather than waved through), and a KIND read off
its own line: state, log, life, panel, frame, ground, trailer, section, seed,
digest (a frozen digest the line does not name further) or note (a value
quoted in a comment or prose, which pins nothing).

    WORLD      a digest of a generated world: the gates, the goldens, the
               containers, the replay pins, the CMake -DEXPECT lines
    GEN        the Phase 02 generation-stage digests (elevation, climate,
               hydrology, regions, deposits) - a flip that touches them touched
               the map, not the simulation
    HASH       hash-function constants and pinned hashes of fixed strings:
               FNV, splitmix, the salts, the event-type table
    CONST      sentinels, arithmetic test values, examples in a harness test
    SEED       AELVOR's seed, 0x000041454c564f52, wherever it is written
    SYNTHETIC  the JSON checker's made-up digests
    RECORD     README rows and comments: the record of a value, not a pin

Modes:
    --check              classify every site; an unclassified file exits 1
                         naming file:line; per-class totals printed
    --diff A B           sites whose VALUE differs between git revisions A and
                         B (git show REV:path), file:line old -> new, and the
                         totals per class and kind; exit 0 always - it reports
    --self-test          the controls below; exit 1 when one fails

THE SELF-TEST, three arms and their controls (ADR-0149):
    1. a temporary tree holding one literal in a file the table does not
       name is REFUSED, naming file:line;
    2. the ADR-0131 re-freeze, as a checked-in fixture of its literal lines
       (Tools/CensusFixtures/adr-0131.diff, because the CI clones at depth 1
       and cannot show be9df8d), reports the pinned figures - WORLD removed
       and added in the pinned number of files, GEN 0, HASH 0 - and, where
       the history IS available, the fixture regenerated from git must be
       byte-identical to the checked-in one; the blind `\\b` regex over the
       same fixture must report 0, kept permanently as the pre-fix arm;
    3. HEAD against HEAD reports 0 in every class;
    plus: a table with one file struck out must refuse that file's sites by
    name, and a changed HASH literal must be reported under HASH and not under
    WORLD, so the class comes from the table and not from the file's name.
"""
import argparse
import os
import re
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

HEX_CODE = re.compile(r"0x([0-9a-fA-F]{16})(?![0-9a-fA-F])")
HEX_BARE = re.compile(r"(?<![0-9a-fA-Fx])([0-9a-f]{16})(?![0-9a-fA-F])")
# The regex Phase 17's clause (i) used. Kept ONLY so the self-test can show it
# blind: it must report 0 over the fixture that the suffix-aware one reports
# 55 removed on.
BLIND = re.compile(r"\b[0-9a-f]{16}\b")

CLASSES = ("WORLD", "GEN", "HASH", "CONST", "SEED", "SYNTHETIC", "RECORD")
KINDS = ("state", "log", "life", "panel", "frame", "ground", "trailer", "section", "seed", "digest", "note")

# Values that mean the same thing wherever they are written.
SEED_VALUES = {"000041454c564f52"}
CONST_VALUES = {"ffffffffffffffff"}
HASH_VALUES = {"9e3779b97f4a7c15", "bf58476d1ce4e5b9", "94d049bb133111eb", "cbf29ce484222325", "5641454c454e2d45"}

# THE TABLE. Every file that carries a sixteen-hex literal, named one by one.
# A file that is not here and carries one is refused by --check.
TABLE = {
    # -- WORLD: the gates and the frozen worlds
    "Tests/Sim/Test_HistoryGate.cpp": "WORLD",
    "Tests/Sim/Test_PreHistory.cpp": "WORLD",
    "Tests/Sim/Test_MiniWorld.cpp": "WORLD",
    "Tests/Sim/Test_Replay.cpp": "WORLD",
    "Tests/Sim/Test_Naming.cpp": "WORLD",
    "Tests/Sim/Test_Population.cpp": "WORLD",
    "Tests/Sim/Test_Disasters.cpp": "WORLD",
    "Tests/Sim/Test_HistoryText.cpp": "WORLD",
    "Tests/Sim/Test_Religion.cpp": "WORLD",
    "Tests/Population/Test_PopulationGate.cpp": "WORLD",
    "Tests/Population/Test_Lod.cpp": "WORLD",
    "Tests/Population/Test_Traits.cpp": "WORLD",
    "Tests/Population/Test_Persons.cpp": "WORLD",
    "Tests/Population/Test_PersonHistory.cpp": "WORLD",
    "Tests/Population/Test_Needs.cpp": "WORLD",
    "Tests/Population/Test_Lives.cpp": "WORLD",
    "Tests/Population/Test_Families.cpp": "WORLD",
    "Tests/Society/Test_SocietyGate.cpp": "WORLD",
    "Tests/Society/Test_Organizations.cpp": "WORLD",
    "Tests/Society/Test_Strata.cpp": "WORLD",
    "Tests/Society/Test_Standing.cpp": "WORLD",
    "Tests/Society/Test_SocietyHistory.cpp": "WORLD",
    "Tests/Society/Test_Norms.cpp": "WORLD",
    "Tests/Society/Test_Decisions.cpp": "WORLD",
    "Tests/Society/Test_Bondage.cpp": "WORLD",
    "Tests/Economy/Test_EconomyGate.cpp": "WORLD",
    "Tests/Economy/Test_Production.cpp": "WORLD",
    "Tests/Economy/Test_Ledger.cpp": "WORLD",
    "Tests/Economy/Test_Grains.cpp": "WORLD",
    "Tests/Economy/Test_Wealth.cpp": "WORLD",
    "Tests/Economy/Test_Trade.cpp": "WORLD",
    "Tests/Economy/Test_Stocks.cpp": "WORLD",
    "Tests/Economy/Test_Markets.cpp": "WORLD",
    "Tests/Economy/Test_EconomyHistory.cpp": "WORLD",
    "Tests/Politics/Test_PoliticsGate.cpp": "WORLD",
    "Tests/Politics/Test_Succession.cpp": "WORLD",
    "Tests/Politics/Test_Reach.cpp": "WORLD",
    "Tests/Politics/Test_Polities.cpp": "WORLD",
    "Tests/Politics/Test_PoliticsHistory.cpp": "WORLD",
    "Tests/Politics/Test_Law.cpp": "WORLD",
    "Tests/Politics/Test_Factions.cpp": "WORLD",
    "Tests/Politics/Test_Diplomacy.cpp": "WORLD",
    "Tests/Military/Test_MilitaryGate.cpp": "WORLD",
    "Tests/Military/Test_War.cpp": "WORLD",
    "Tests/Military/Test_Toll.cpp": "WORLD",
    "Tests/Military/Test_Siege.cpp": "WORLD",
    "Tests/Military/Test_MilitaryHistory.cpp": "WORLD",
    "Tests/Military/Test_March.cpp": "WORLD",
    "Tests/Military/Test_Battle.cpp": "WORLD",
    "Tests/Military/Test_Armies.cpp": "WORLD",
    "Tests/Infrastructure/Test_InfrastructureGate.cpp": "WORLD",
    "Tests/Infrastructure/Test_WorksHistory.cpp": "WORLD",
    "Tests/Infrastructure/Test_Works.cpp": "WORLD",
    "Tests/Infrastructure/Test_Roads.cpp": "WORLD",
    "Tests/Infrastructure/Test_Places.cpp": "WORLD",
    "Tests/Infrastructure/Test_Logistics.cpp": "WORLD",
    "Tests/Infrastructure/Test_Decay.cpp": "WORLD",
    "Tests/Infrastructure/Test_Buildings.cpp": "WORLD",
    "Tests/Player/Test_PlayerGate.cpp": "WORLD",
    "Tests/Player/Test_LifeHistory.cpp": "WORLD",
    "Tests/Colony/Test_ColonyGate.cpp": "WORLD",
    "Tests/Colony/Test_Mining.cpp": "WORLD",
    "Tests/Colony/Test_Supply.cpp": "WORLD",
    "Tests/Gameplay/Test_GameplayGate.cpp": "WORLD",
    "Tests/View/Test_ViewGate.cpp": "WORLD",
    "Tests/View/Test_Panel.cpp": "WORLD",
    # 19.04: hand-built facts for the line composer; the climate digest is
    # copied from Atlas.ClimateFrozen128 as a sample, and does not move with it.
    "Tests/View/Test_Proof.cpp": "SYNTHETIC",
    "Tests/Run/Test_Aelvor.cpp": "WORLD",
    "Tests/Run/Test_Climate.cpp": "WORLD",
    "Tests/Run/Test_Golden.cpp": "WORLD",
    "Tests/Run/Test_Containers.cpp": "WORLD",
    "Tests/CMakeLists.txt": "WORLD",
    "Tests/Run/CMakeLists.txt": "WORLD",
    # -- GEN: the generation stages of Phase 02
    "Tests/Sim/Test_WorldGen.cpp": "GEN",
    "Tests/Sim/Test_Climate.cpp": "GEN",
    "Tests/Sim/Test_WorldPipeline.cpp": "GEN",
    "Tests/Sim/Test_Hydrology.cpp": "GEN",
    "Tests/Sim/Test_Regions.cpp": "GEN",
    "Tests/Sim/Test_Deposits.cpp": "GEN",
    # -- HASH: the functions and the fixed strings
    "Tests/Core/Test_Hash.cpp": "HASH",
    "Tests/Core/Test_Random.cpp": "HASH",
    "Tests/Core/Test_Ids.cpp": "HASH",
    "Tests/Sim/Test_Noise.cpp": "HASH",
    "Tests/Harness/VaelenTest.h": "HASH",
    "Source/VaelenCore/Public/Vaelen/Core/Hash.h": "HASH",
    "Source/VaelenCore/Private/Random.cpp": "HASH",
    "Source/VaelenSim/Public/Vaelen/Sim/Noise.h": "HASH",
    "Source/VaelenSim/Private/Noise.cpp": "HASH",
    "Source/VaelenSim/Public/Vaelen/Sim/FixedPoint.h": "HASH",
    "Source/VaelenSim/Public/Vaelen/Sim/EventBus.h": "HASH",
    "Source/VaelenSim/Public/Vaelen/Sim/EventTypeNames.h": "HASH",
    "Source/VaelenSim/Private/Naming.cpp": "HASH",
    "Source/VaelenSim/Private/Climate.cpp": "HASH",  # 18.03: the "CLIMATE" salt of YearVariation
    "Source/VaelenPopulation/Private/Persons.cpp": "HASH",
    "Source/VaelenGameplay/Private/Maps.cpp": "HASH",
    "Source/VaelenGameplay/Private/Documents.cpp": "HASH",
    "Tools/Atlas/Main.cpp": "HASH",
    "Tools/gen_event_names.py": "HASH",
    # -- CONST: sentinels, arithmetic, examples
    "Tests/Sim/Test_Archive.cpp": "CONST",
    "Tests/Sim/Test_FixedPoint.cpp": "CONST",
    "Tests/Core/Test_Assert.cpp": "CONST",
    "Tests/Core/Test_Harness.cpp": "CONST",
    "Source/VaelenPopulation/Private/Families.cpp": "CONST",
    "Tools/frozen_census.py": "CONST",  # its own examples: the seed in the docstring, the planted literal
    # -- SYNTHETIC and RECORD
    "Tools/check_atlas_output.py": "SYNTHETIC",
    "Source/VaelenPresentation/Private/VaelenViewActor.cpp": "RECORD",
    "Tests/Run/Containers/README.md": "RECORD",
    "Tests/Run/Golden/README.md": "RECORD",
    "Tests/Run/Streams/README.md": "RECORD",
}

CODE_SUFFIXES = (".cpp", ".h", ".py")
CODE_ROOTS = ("Tests", "Source", "Tools")
BARE_FILES = ("Tests/CMakeLists.txt", "Tests/Run/CMakeLists.txt")
BARE_GLOB_DIR = "Tests/Run"  # every *.cmake driver in it
README_FILES = ("Tests/Run/Containers/README.md", "Tests/Run/Golden/README.md", "Tests/Run/Streams/README.md")

# A kind is read off the text BEFORE the token on its line: the last keyword
# wins, so "state X, log Y" gives X state and Y log.
KIND_WORDS = [
    (re.compile(r"(?i)trailer"), "trailer"),
    (re.compile(r"(?i)section|^\s*\{?\{\d+,\s*\d+,\s*\d+,\s*$"), "section"),
    (re.compile(r"(?i)\bseed\b"), "seed"),
    (re.compile(r"(?i)\bground\b|_GROUND\b"), "ground"),
    (re.compile(r"(?i)\bframe\b|_FRAME\b"), "frame"),
    (re.compile(r"(?i)\bpanel\b|_PANEL|PANEL_|\bdigest\s*$"), "panel"),
    (re.compile(r"(?i)\blife\b|_LIFE\b"), "life"),
    (re.compile(r"(?i)\blog\b|_LOG\b|_LOG_"), "log"),
    (re.compile(r"(?i)\bstate\b|FROZEN|STILL|LIVING|EXPECT=|RIGHT="), "state"),
]


def is_bare_file(relative):
    return relative in BARE_FILES or (relative.startswith(BARE_GLOB_DIR + "/") and relative.endswith(".cmake"))


def is_readme(relative):
    return relative in README_FILES


def pattern_for(relative):
    if is_bare_file(relative) or is_readme(relative):
        return HEX_BARE
    return HEX_CODE


def kind_of(line, start, value, relative):
    """The kind of the token at line[start:], by its own line."""
    if value in SEED_VALUES or value == "41454c564f52":
        return "seed"
    before = line[:start]
    stripped = line.lstrip()
    # A comment or a README sentence: a value quoted, not pinned. Table rows
    # in a README are pins of the RECORD class and keep their kind.
    if "//" in before or stripped.startswith("#") and not stripped.startswith("#define"):
        return "note"
    if is_readme(relative) and not stripped.startswith("|"):
        return "note"
    # Test_Containers' rows: the section table lines carry no word; their
    # shape does. A lone token line after the header numbers is the trailer.
    if relative.endswith("Test_Containers.cpp"):
        if re.match(r"^\s*\{?\{\d+,\s*\d+,\s*\d+,\s*$", before):
            return "section"
        if re.match(r"^\s*$", before):
            return "trailer"
    if relative.endswith("Test_Golden.cpp") and stripped.startswith("{\""):
        return "trailer"
    if is_readme(relative):
        # The corpus tables: the last cell of a container row is its trailer,
        # a row that names a section kind is a section digest.
        if re.search(r"(?i)\|\s*(STATE|HOST|RUN|STREAM)\s*\(", before):
            return "section"
        if relative.endswith("Golden/README.md"):
            cells = before.count("|")
            return "digest" if cells <= 7 else "trailer"
        if relative.endswith("Containers/README.md"):
            return "trailer"
    found = None
    for word, kind in KIND_WORDS:
        for match in word.finditer(before):
            found = (match.start(), kind) if found is None or match.start() >= found[0] else found
    return found[1] if found else "digest"


def class_of(relative, value, kind):
    base = TABLE.get(relative)
    if base is None:
        return None
    if value in SEED_VALUES:
        return "SEED"
    if value in CONST_VALUES and base in ("WORLD", "GEN"):
        return "CONST"
    if value in HASH_VALUES and base in ("WORLD", "GEN", "CONST"):
        return "HASH"
    if kind == "note" and base in ("WORLD", "GEN"):
        return "RECORD"
    return base


def matches_in(relative, line):
    """Every token of the line, in order. A CMake driver or a README carries
    bare tokens AND `0x` ones (`--expect frame=0x...`): the bare pattern's
    lookbehind skips the latter on purpose, so both patterns read those files
    and a `0x` token is counted once, by the code pattern."""
    found = list(HEX_CODE.finditer(line))
    if pattern_for(relative) is HEX_BARE:
        found += list(HEX_BARE.finditer(line))
        found.sort(key=lambda m: m.start())
    return found


def sites_in(relative, text):
    """[(line_no, value, kind, class)] for one file's text; class None when unclassified."""
    out = []
    for number, line in enumerate(text.split("\n"), 1):
        for match in matches_in(relative, line):
            value = match.group(1).lower()
            kind = kind_of(line, match.start(), value, relative)
            out.append((number, value, kind, class_of(relative, value, kind)))
    return out


def walk(root):
    """Every path this census reads, relative to root, sorted."""
    found = []
    for top in CODE_ROOTS:
        base = os.path.join(root, top)
        for directory, _, files in os.walk(base):
            for name in files:
                if name.endswith(CODE_SUFFIXES):
                    found.append(os.path.relpath(os.path.join(directory, name), root))
    for relative in BARE_FILES:
        if os.path.exists(os.path.join(root, relative)):
            found.append(relative)
    run_dir = os.path.join(root, BARE_GLOB_DIR)
    if os.path.isdir(run_dir):
        for name in os.listdir(run_dir):
            if name.endswith(".cmake"):
                found.append(os.path.join(BARE_GLOB_DIR, name))
    for relative in README_FILES:
        if os.path.exists(os.path.join(root, relative)):
            found.append(relative)
    return sorted(set(p.replace(os.sep, "/") for p in found))


def read(root, relative):
    with open(os.path.join(root, relative), "r", encoding="utf-8", errors="replace") as handle:
        return handle.read()


def census(root, table=None):
    """{relative: [sites]} over the tree; `table` overrides TABLE for the self-test."""
    global TABLE
    kept = TABLE
    if table is not None:
        TABLE = table
    try:
        out = {}
        for relative in walk(root):
            found = sites_in(relative, read(root, relative))
            if found:
                out[relative] = found
        return out
    finally:
        TABLE = kept


def totals(found):
    by_class = {c: 0 for c in CLASSES}
    by_kind = {}
    unclassified = []
    for relative, sites in found.items():
        for number, value, kind, cls in sites:
            if cls is None:
                unclassified.append((relative, number, value))
                continue
            by_class[cls] += 1
            by_kind[(cls, kind)] = by_kind.get((cls, kind), 0) + 1
    return by_class, by_kind, unclassified


def run_check(root, out=sys.stdout):
    found = census(root)
    by_class, by_kind, unclassified = totals(found)
    files = len(found)
    sites = sum(len(s) for s in found.values())
    for relative, number, value in unclassified:
        out.write("frozen_census: UNCLASSIFIED {}:{} 0x{} - name the file in TABLE\n".format(relative, number, value))
    out.write("[frozen-census] {} sites in {} files: {}\n".format(
        sites, files, ", ".join("{} {}".format(c, by_class[c]) for c in CLASSES)))
    kinds = sorted(by_kind.items())
    out.write("[frozen-census] by kind: {}\n".format(", ".join("{}/{} {}".format(c, k, n) for (c, k), n in kinds)))
    return 0 if not unclassified else 1


def git_show(rev, relative):
    """The file at a revision, or '' when it did not exist there."""
    done = subprocess.run(["git", "show", "{}:{}".format(rev, relative)], cwd=ROOT,
                          stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)
    return done.stdout.decode("utf-8", errors="replace") if done.returncode == 0 else ""


def git_paths(rev):
    done = subprocess.run(["git", "ls-tree", "-r", "--name-only", rev], cwd=ROOT,
                          stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)
    if done.returncode != 0:
        return None
    return [p for p in done.stdout.decode("utf-8", errors="replace").split("\n") if p]


def wanted(relative):
    if any(relative.startswith(top + "/") for top in CODE_ROOTS) and relative.endswith(CODE_SUFFIXES):
        return True
    return is_bare_file(relative) or is_readme(relative)


def diff_texts(reader_a, reader_b, paths):
    """Compare values per (file, class, kind) between two readers.

    Returns (removed, added, changed): removed/added are [(relative, class,
    kind, value, line)], changed pairs them where one value left and one
    arrived in the same file and kind, in order.
    """
    removed, added = [], []
    for relative in sorted(paths):
        before = sites_in(relative, reader_a(relative))
        after = sites_in(relative, reader_b(relative))
        keyed_before = {}
        for number, value, kind, cls in before:
            keyed_before.setdefault((cls, kind, value), []).append(number)
        keyed_after = {}
        for number, value, kind, cls in after:
            keyed_after.setdefault((cls, kind, value), []).append(number)
        for key, lines in keyed_before.items():
            gone = len(lines) - len(keyed_after.get(key, []))
            for number in lines[:max(gone, 0)]:
                removed.append((relative, key[0], key[1], key[2], number))
        for key, lines in keyed_after.items():
            came = len(lines) - len(keyed_before.get(key, []))
            for number in lines[:max(came, 0)]:
                added.append((relative, key[0], key[1], key[2], number))
    return removed, added


def report_diff(label, removed, added, out=sys.stdout):
    def tally(rows):
        by_class = {c: 0 for c in CLASSES}
        by_class["UNCLASSIFIED"] = 0
        by_kind = {}
        for relative, cls, kind, value, number in rows:
            by_class[cls or "UNCLASSIFIED"] += 1
            by_kind[(cls or "UNCLASSIFIED", kind)] = by_kind.get((cls or "UNCLASSIFIED", kind), 0) + 1
        return by_class, by_kind

    for relative, cls, kind, value, number in removed:
        out.write("  - {}:{} {}/{} 0x{}\n".format(relative, number, cls, kind, value))
    for relative, cls, kind, value, number in added:
        out.write("  + {}:{} {}/{} 0x{}\n".format(relative, number, cls, kind, value))
    r_class, r_kind = tally(removed)
    a_class, a_kind = tally(added)
    files = sorted(set(r[0] for r in removed) | set(a[0] for a in added))
    out.write("[frozen-census] {}: {} removed, {} added, in {} file(s)\n".format(label, len(removed), len(added), len(files)))
    for c in list(CLASSES) + ["UNCLASSIFIED"]:
        if r_class[c] or a_class[c]:
            kinds = sorted(set(k for (cc, k) in list(r_kind) + list(a_kind) if cc == c))
            detail = ", ".join("{} -{}/+{}".format(k, r_kind.get((c, k), 0), a_kind.get((c, k), 0)) for k in kinds)
            out.write("[frozen-census]   {} -{}/+{}: {}\n".format(c, r_class[c], a_class[c], detail))
    quiet = [c for c in CLASSES if not r_class[c] and not a_class[c]]
    out.write("[frozen-census]   unmoved: {}\n".format(", ".join(quiet) if quiet else "nothing"))
    return len(removed), len(added), len(files)


def run_diff(rev_a, rev_b, out=sys.stdout):
    paths_a, paths_b = git_paths(rev_a), git_paths(rev_b)
    if paths_a is None or paths_b is None:
        out.write("frozen_census: cannot list {} or {} (a shallow clone?)\n".format(rev_a, rev_b))
        return 2
    paths = set(p for p in paths_a + paths_b if wanted(p))
    removed, added = diff_texts(lambda p: git_show(rev_a, p), lambda p: git_show(rev_b, p), paths)
    report_diff("{}..{}".format(rev_a, rev_b), removed, added, out)
    return 0


# ---------------------------------------------------------------- the fixture

FIXTURE = os.path.join("Tools", "CensusFixtures", "adr-0131.diff")
FIXTURE_REVS = ("be9df8d^", "be9df8d")


def literal_lines_of_diff(text):
    """The `diff --git` headers and the +/- lines carrying a literal, only."""
    keep = []
    current = None
    for line in text.split("\n"):
        if line.startswith("diff --git "):
            current = line
            continue
        if line.startswith(("---", "+++")):
            continue
        if line[:1] in "-+" and (HEX_CODE.search(line) or HEX_BARE.search(line)):
            if current is not None:
                keep.append(current)
                current = None
            keep.append(line)
    return "\n".join(keep) + "\n"


def fixture_from_git():
    done = subprocess.run(["git", "diff", FIXTURE_REVS[0], FIXTURE_REVS[1], "--", "Tests/", "Source/", "Tools/"],
                          cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)
    if done.returncode != 0:
        return None
    return literal_lines_of_diff(done.stdout.decode("utf-8", errors="replace"))


def parse_fixture(text):
    """{relative: (removed_lines, added_lines)} from a literal-lines diff."""
    out = {}
    relative = None
    for line in text.split("\n"):
        if line.startswith("diff --git "):
            relative = line.split(" b/")[-1]
            out.setdefault(relative, ([], []))
        elif line.startswith("-") and relative:
            out[relative][0].append(line[1:])
        elif line.startswith("+") and relative:
            out[relative][1].append(line[1:])
    return out


def diff_fixture(parsed):
    """The census diff of a fixture: the removed/added token lists, classified."""
    removed, added = [], []
    for relative, (gone, came) in parsed.items():
        for number, line in enumerate(gone, 1):
            for match in matches_in(relative, line):
                value = match.group(1).lower()
                kind = kind_of(line, match.start(), value, relative)
                removed.append((relative, class_of(relative, value, kind), kind, value, number))
        for number, line in enumerate(came, 1):
            for match in matches_in(relative, line):
                value = match.group(1).lower()
                kind = kind_of(line, match.start(), value, relative)
                added.append((relative, class_of(relative, value, kind), kind, value, number))
    return removed, added


# The known answer: what the ADR-0131 re-freeze moved, as this census reads
# its literal lines. Pinned from the fixture on 2026-09-24; a change here is a
# change to the classifier's reading of a commit that cannot change.
FIXTURE_WORLD_REMOVED = 55
FIXTURE_WORLD_ADDED = 50
FIXTURE_FILES = 19


def self_test():
    failures = []

    def check(condition, what):
        if not condition:
            failures.append(what)
        sys.stderr.write("[census-test] {} {}\n".format("ok  " if condition else "FAIL", what))

    # 1. an unnamed file with a literal is refused, by file:line
    with tempfile.TemporaryDirectory() as temp:
        os.makedirs(os.path.join(temp, "Tests", "Stray"))
        with open(os.path.join(temp, "Tests", "Stray", "Test_Stray.cpp"), "w") as handle:
            handle.write("// planted\n#define STRAY 0x0123456789abcdefull\n")
        import io
        sink = io.StringIO()
        code = run_check(temp, sink)
        check(code == 1 and "UNCLASSIFIED Tests/Stray/Test_Stray.cpp:2 0x0123456789abcdef" in sink.getvalue(),
              "a literal in a file the table does not name is refused, naming file:line")

    # 2. the ADR-0131 re-freeze from its fixture: the known answer
    with open(os.path.join(ROOT, FIXTURE), "r", encoding="utf-8") as handle:
        fixture = handle.read()
    parsed = parse_fixture(fixture)
    removed, added = diff_fixture(parsed)
    import io
    sink = io.StringIO()
    n_removed, n_added, n_files = report_diff("fixture " + "..".join(FIXTURE_REVS), removed, added, sink)
    world_removed = sum(1 for r in removed if r[1] == "WORLD")
    world_added = sum(1 for r in added if r[1] == "WORLD")
    other = [r for r in removed + added if r[1] not in ("WORLD",)]
    check(world_removed == FIXTURE_WORLD_REMOVED and world_added == FIXTURE_WORLD_ADDED and n_files == FIXTURE_FILES,
          "be9df8d reads as WORLD -{}/+{} in {} files (pinned -{}/+{} in {})".format(
              world_removed, world_added, n_files, FIXTURE_WORLD_REMOVED, FIXTURE_WORLD_ADDED, FIXTURE_FILES))
    check(not other, "be9df8d moved nothing outside WORLD ({} other site(s))".format(len(other)))
    blind = sum(1 for lines in parsed.values() for line in lines[0] if BLIND.search(line))
    check(blind == 0, "the pre-fix arm: Phase 17's \\b regex reports {} removed over the same lines (blind: 0)".format(blind))
    suffix_aware = sum(1 for lines in parsed.values() for line in lines[0] if HEX_CODE.search(line))
    check(suffix_aware >= FIXTURE_WORLD_REMOVED, "the raw suffix-aware count over the same lines is {} (>= {})".format(
        suffix_aware, FIXTURE_WORLD_REMOVED))
    regenerated = fixture_from_git()
    if regenerated is None:
        sys.stderr.write("[census-test] skip the fixture's regeneration from git: {} is not in this clone\n".format(FIXTURE_REVS[0]))
    else:
        check(regenerated == fixture, "the fixture regenerated from git is byte-identical to the checked-in one")

    # 3. HEAD against HEAD: nothing
    if git_paths("HEAD") is not None:
        sink = io.StringIO()
        run_diff("HEAD", "HEAD", sink)
        check("HEAD..HEAD: 0 removed, 0 added, in 0 file(s)" in sink.getvalue(), "HEAD against HEAD reports 0 in every class")

    # 4. a table with one file struck out refuses that file's sites by name
    struck = dict(TABLE)
    del struck["Tests/Sim/Test_HistoryGate.cpp"]
    found = census(ROOT, struck)
    _, _, unclassified = totals(found)
    names = sorted(set(r for r, _, _ in unclassified))
    check(names == ["Tests/Sim/Test_HistoryGate.cpp"] and len(unclassified) == 4,
          "striking Test_HistoryGate.cpp from the table refuses exactly its four sites")

    # 5. a changed HASH literal is reported under HASH, not WORLD
    hash_file = "Tests/Core/Test_Hash.cpp"
    original = read(ROOT, hash_file)
    first = HEX_CODE.search(original)
    check(first is not None, "Test_Hash.cpp carries a literal to change")
    if first is not None:
        mutated = original[:first.start(1)] + "0000000000000042" + original[first.end(1):]
        removed, added = diff_texts(lambda p: original, lambda p: mutated, [hash_file])
        check(len(removed) == 1 and len(added) == 1 and removed[0][1] == "HASH" and added[0][1] == "HASH",
              "a changed literal in Test_Hash.cpp is reported as HASH (-1/+1), by the table and not the file name")

    # 6. the CONTROL of the classifier itself: the tree classifies whole today
    sink = io.StringIO()
    check(run_check(ROOT, sink) == 0, "the tree at HEAD classifies without an unclassified site")

    if failures:
        sys.stderr.write("[census-test] {} of the controls FAILED\n".format(len(failures)))
        return 1
    sys.stderr.write("[census-test] every control held\n")
    return 0


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("--check", action="store_true", help="classify every site; unclassified exits 1")
    parser.add_argument("--diff", nargs=2, metavar=("A", "B"), help="sites whose value differs between two revisions")
    parser.add_argument("--self-test", action="store_true", help="the census's own controls")
    parser.add_argument("--write-fixture", action="store_true", help="regenerate the ADR-0131 fixture from git")
    parser.add_argument("--root", default=ROOT, help="the tree to read (the self-test plants one of its own)")
    args = parser.parse_args()
    if args.write_fixture:
        text = fixture_from_git()
        if text is None:
            sys.stderr.write("frozen_census: git cannot show {}\n".format(FIXTURE_REVS[0]))
            return 2
        with open(os.path.join(ROOT, FIXTURE), "w", encoding="utf-8") as handle:
            handle.write(text)
        sys.stderr.write("frozen_census: wrote {}\n".format(FIXTURE))
        return 0
    if args.self_test:
        return self_test()
    if args.diff:
        return run_diff(args.diff[0], args.diff[1])
    if args.check:
        return run_check(args.root)
    parser.error("one of --check, --diff A B, --self-test or --write-fixture")


if __name__ == "__main__":
    sys.exit(main())
