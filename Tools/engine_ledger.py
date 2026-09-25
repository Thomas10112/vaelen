#!/usr/bin/env python3
"""What the owner's machine has compiled, kept where a checker can read it. 19.01, ADR-0159.

Nothing in this environment can build the four engine modules. What CAN be kept
here is a record of what the owner's builds compiled, and every STATUS line in
those modules can then be checked against it. Until 19.01 the only record was
the prose of each STATUS line, and the Phase 19 panel measured what that was
worth: `git diff 867a129 HEAD` over the engine modules holds 410 non-comment
lines in seven files that no compiler had read, five of them still saying
VALIDATED.

THE LEDGER (Tools/engine_builds.txt) has one row per build: an id, the date, the
commit whose engine files were built, how the row is known and what ran. The
rows before 19.03 are RECONSTRUCTED from the commits that reported them - the
owner's session recorded no commit beside a UBT result - and say so.

A RECORD (Tools/EngineBuilds/<id>.txt) is derived from git by
`check_engine_status.py --record`, never written by hand. It holds:

    file <path> <fingerprint>   every file of the engine modules at that commit
    seen <name>                 every name of the shim the engine code used then

A fingerprint is a hash of the file's CODE: comments are stripped and the rest
is cut into tokens, so a file whose comments changed keeps its fingerprint and a
file whose code changed by one token does not. That is the whole point: a STATUS
line may be reworded freely; the code it vouches for may not move under it.

Records are committed, not recomputed from git at check time, because the CI
clones at depth 1 and has no history to recompute from (the census's lesson,
Tests/CMakeLists.txt at Kernel.FrozenCensus). Where the history IS available the
self-tests re-derive every record from git and require the same bytes.
"""

import hashlib
import os
import re
import subprocess

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LEDGER = os.path.join(ROOT, "Tools", "engine_builds.txt")
RECORDS = os.path.join(ROOT, "Tools", "EngineBuilds")
SHIM = os.path.join(ROOT, "Tools", "EngineShim")

# The engine modules, from the one place that decides what they are.
from parse_engine_modules import ENGINE_MODULES  # noqa: E402

KEYWORDS = set("""
alignas alignof and asm auto bool break case catch char char8_t char16_t char32_t class
const consteval constexpr constinit const_cast continue co_await co_return co_yield
decltype default delete do double dynamic_cast else enum explicit export extern false
float for friend goto if inline int long mutable namespace new noexcept not nullptr
operator or private protected public register reinterpret_cast requires return short
signed sizeof static static_assert static_cast struct switch template this thread_local
throw true try typedef typeid typename union unsigned using virtual void volatile
wchar_t while override final include pragma once define ifdef ifndef endif elif undef
""".split())

TOKEN = re.compile(r'"(?:\\.|[^"\\\n])*"|\'(?:\\.|[^\'\\\n])*\'|[A-Za-z_]\w*|\d[\w.]*|\S')
IDENT = re.compile(r"[A-Za-z_]\w*$")


def strip_comments(text):
    """The text with // and /* */ comments removed, string and char literals kept whole."""
    out = []
    i, n = 0, len(text)
    while i < n:
        if text.startswith("//", i):
            j = text.find("\n", i)
            i = n if j < 0 else j
            continue
        if text.startswith("/*", i):
            j = text.find("*/", i + 2)
            i = n if j < 0 else j + 2
            out.append(" ")
            continue
        c = text[i]
        if c in "\"'":
            j = i + 1
            while j < n and text[j] != c and text[j] != "\n":
                j += 2 if text[j] == "\\" else 1
            out.append(text[i:j + 1])
            i = j + 1
            continue
        out.append(c)
        i += 1
    return "".join(out)


def tokens(text):
    return TOKEN.findall(strip_comments(text))


def fingerprint(text):
    """Sixteen hex digits over the code tokens of a file, comments ignored."""
    return hashlib.sha256("\n".join(tokens(text)).encode("utf-8")).hexdigest()[:16]


def identifiers(text):
    """The names a text's code uses: no comments, no string contents, no keywords."""
    out = set()
    for tok in tokens(text):
        if IDENT.match(tok) and tok not in KEYWORDS:
            out.add(tok)
    return out


def rel(path, root):
    return os.path.relpath(path, root).replace(os.sep, "/")


def engine_files(root):
    """Every file under the engine modules' directories, as repository paths, sorted."""
    out = []
    for module in ENGINE_MODULES:
        top = os.path.join(root, "Source", module)
        for here, folders, names in os.walk(top):
            folders.sort()
            for name in sorted(names):
                out.append(rel(os.path.join(here, name), root))
    return sorted(out)


def read(root, path):
    with open(os.path.join(root, path), encoding="utf-8", errors="replace") as f:
        return f.read()


def shim_vocabulary(shim_root=SHIM):
    """Every name the shim's code mentions. Deliberately every name and not only
    the declared ones: a parser of declarations would be a second instrument
    that could be wrong, and measured on 2026-09-25 the noise this lets in is
    three names (begin, end, push_back) - which are in fact range-for and
    TArray beliefs, so not noise at all."""
    out = set()
    for here, folders, names in os.walk(shim_root):
        folders.sort()
        for name in sorted(names):
            with open(os.path.join(here, name), encoding="utf-8", errors="replace") as f:
                out |= identifiers(f.read())
    return out


def used_names(root):
    """Every name the engine modules' code uses, with the first place each is used."""
    where = {}
    for path in engine_files(root):
        text = read(root, path)
        clean = strip_comments(text).split("\n")
        for number, line in enumerate(clean, 1):
            for tok in TOKEN.findall(line):
                if IDENT.match(tok) and tok not in KEYWORDS and tok not in where:
                    where[tok] = "{}:{}".format(path, number)
    return where


# ── the ledger ──────────────────────────────────────────────────────────────

class Build:
    def __init__(self, ident, date, commit, source, what):
        self.ident, self.date, self.commit, self.source, self.what = ident, date, commit, source, what


def load_ledger(path=LEDGER):
    """The rows, in order. A malformed row is an error, not a skipped line."""
    rows = []
    with open(path, encoding="utf-8") as f:
        for number, line in enumerate(f, 1):
            line = line.rstrip("\n")
            if not line.strip() or line.lstrip().startswith("#"):
                continue
            parts = line.split(None, 4)
            if len(parts) < 5 or not re.match(r"^b\d{4}[a-z]?$", parts[0]) \
                    or not re.match(r"^\d{4}-\d{2}-\d{2}$", parts[1]) \
                    or not re.match(r"^[0-9a-f]{7,40}$", parts[2]) \
                    or parts[3] not in ("RECONSTRUCTED", "RECORDED"):
                raise ValueError("{}:{}: not a ledger row: {}".format(path, number, line))
            rows.append(Build(*parts))
    idents = [b.ident for b in rows]
    if len(set(idents)) != len(idents):
        raise ValueError("{}: a build id appears twice".format(path))
    return rows


class Record:
    def __init__(self):
        self.files = {}
        self.seen = set()


def record_path(ident, records=RECORDS):
    return os.path.join(records, ident + ".txt")


def load_record(ident, records=RECORDS):
    rec = Record()
    with open(record_path(ident, records), encoding="utf-8") as f:
        for line in f:
            line = line.rstrip("\n")
            if not line or line.startswith("#"):
                continue
            kind, _, rest = line.partition(" ")
            if kind == "file":
                path, _, print_ = rest.rpartition(" ")
                rec.files[path] = print_
            elif kind == "seen":
                rec.seen.add(rest)
            else:
                raise ValueError("{}: unknown record line: {}".format(record_path(ident, records), line))
    return rec


# ── git, where there is history ─────────────────────────────────────────────

def git(*args):
    done = subprocess.run(["git"] + list(args), cwd=ROOT, capture_output=True, text=True)
    return done.returncode, done.stdout


def has_commit(commit):
    code, _ = git("cat-file", "-e", commit + "^{commit}")
    return code == 0


def shim_vocabulary_at(commit):
    """The shim's names as the tree held them at a commit: a record says what
    that build's engine code used OF THE SHIM IT WAS PARSED AGAINST, so it is
    derived from that shim and re-derives to the same bytes forever."""
    code, listing = git("ls-tree", "-r", "--name-only", commit, "--", "Tools/EngineShim")
    out = set()
    for path in sorted(listing.split()) if code == 0 else []:
        _, text = git("show", "{}:{}".format(commit, path))
        out |= identifiers(text)
    return out


def record_text(build):
    """The record of one build, derived from git. Byte-stable: sorted, LF."""
    vocabulary = shim_vocabulary_at(build.commit)
    code, listing = git("ls-tree", "-r", "--name-only", build.commit, "--",
                        *["Source/" + m for m in ENGINE_MODULES])
    if code != 0:
        raise RuntimeError("git cannot list {} at {}".format(build.ident, build.commit))
    lines = ["# build {} at {} ({}), derived from git by check_engine_status.py --record."
             .format(build.ident, build.commit, build.source),
             "# Never edit by hand: re-derive it, and the self-test compares the bytes."]
    used = set()
    for path in sorted(listing.split()):
        code, text = git("show", "{}:{}".format(build.commit, path))
        if code != 0:
            raise RuntimeError("git cannot show {}:{}".format(build.commit, path))
        lines.append("file {} {}".format(path, fingerprint(text)))
        used |= identifiers(text)
    for name in sorted(used & vocabulary):
        lines.append("seen " + name)
    return "\n".join(lines) + "\n"
