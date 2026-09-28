#!/usr/bin/env python3
"""An engine sitting's log, re-read against the headless command that must print the same lines. 19.01, ADR-0159.

Until Phase 19 an engine clause was closed by pasting lines into a document and
saying they matched. A document cannot fail. This reads the log the owner's
machine wrote, committed as it came (Tests/Run/Sessions/<id>-<date>.log), runs
the headless command that must print the same lines, and compares them byte
for byte - so a sitting is closed by a CTest entry, which can.

A SESSION FILE (Tests/Run/Sessions/<id>.session) says what to compare:

    log <file>                 the log, beside the session file
    head <commit>              optional: the log's first line must be `git rev-parse HEAD`'s
    run <atlas arguments>      the headless command; {streams} is Tests/Run/Streams
    line <prefix>              one expected line, found by its prefix
    other <atlas arguments>    optional, for --self-test: a command of ANOTHER world, which
                               must NOT agree - so the comparison is seen to depend on the run

Every `line` must be found EXACTLY ONCE in the log and exactly once in what the
command printed, and the two must be equal. A log line is read from its
category on, `LogVaelenPlay: ...`, so the engine's `[timestamp][frame]` prefix
and a `Display:` verbosity word are not part of the comparison; everything
after them is.

    check_session.py <session> --atlas PATH             the check
    check_session.py <session> --atlas PATH --self-test the controls, around the check
"""

import argparse
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
STREAMS = os.path.join(ROOT, "Tests", "Run", "Streams")
ATLAS = [None]
# THE LAST category on the line, not the first (19.11b, the review of 2026-09-26):
# the engine writes `[ts][frame]<Category>: <message>`, and every composed line
# this project prints carries its own `LogVaelenX: ` at the head of the MESSAGE
# (`UE_LOG(LogVaelenWalk, ..., TEXT("LogVaelenScene: AELVOR ..."))`), so a raw
# Vaelen.log holds `LogVaelenWalk: LogVaelenScene: AELVOR ...` and the first
# category is the engine's, the second the line's. The Atlas prints the second.
CATEGORY = re.compile(r"(Log[A-Za-z]+): (?:(?:Display|Warning|Error|Log|Verbose): )?(.*)$")


class Session:
    def __init__(self, path):
        self.path = path
        self.log = None
        self.head = None
        self.run = None
        self.other = None
        self.lines = []
        self.moved = []  # (old, new) 16-hex digests: a view's wording changed since the sitting
        with open(path, encoding="utf-8") as f:
            for number, raw in enumerate(f, 1):
                raw = raw.rstrip("\r\n")
                if not raw.strip() or raw.lstrip().startswith("#"):
                    continue
                key, _, rest = raw.partition(" ")
                if key == "log":
                    self.log = os.path.join(os.path.dirname(path), rest.strip())
                elif key == "head":
                    self.head = rest.strip()
                elif key == "run":
                    self.run = rest.strip()
                elif key == "other":
                    self.other = rest.strip()
                elif key == "line":
                    self.lines.append(rest)
                elif key == "moved":
                    old, new = rest.split()[:2]
                    if not (re.fullmatch(r"[0-9a-f]{16}", old) and re.fullmatch(r"[0-9a-f]{16}", new)):
                        raise ValueError("{}:{}: moved takes two 16-hex digests".format(path, number))
                    self.moved.append((old, new))
                else:
                    raise ValueError("{}:{}: unknown key '{}'".format(path, number, key))
        if not self.log or not self.run or not self.lines:
            raise ValueError("{}: a session needs log, run and at least one line".format(path))


def normal(text):
    """Every line of a log, read from its LAST category on; lines with no category dropped."""
    out = []
    for raw in text.replace("\r", "").split("\n"):
        m = CATEGORY.search(raw)
        while m:
            # A message that begins with a category of its own: read from there.
            inner = CATEGORY.search(m.group(2))
            if inner is None or inner.start() != 0:
                break
            m = inner
        if m:
            out.append("{}: {}".format(m.group(1), m.group(2)))
    return out


def run_headless(session, atlas, command=None):
    args = (command or session.run).replace("{streams}", STREAMS).split()
    done = subprocess.run([atlas] + args, capture_output=True, text=True, cwd=ROOT)
    if done.returncode != 0:
        raise SystemExit("[session] the headless command did not run clean (exit {}):\n{}{}".format(
            done.returncode, done.stdout, done.stderr))
    return done.stdout


def first_line(text):
    for raw in text.replace("\r", "").split("\n"):
        if raw.strip():
            return raw.strip()
    return ""


def compare(session, log_text, headless_text):
    """Every refusal, as a sentence. Empty means every expected line is the same on both sides."""
    refused = []
    if session.head is not None:
        got = first_line(log_text)
        if not got.endswith(session.head):
            refused.append("the log's first line is '{}', not the sitting's commit {}".format(got[:60], session.head))
    engine, headless = normal(log_text), normal(headless_text)
    # A digest of a VIEW (the life, the page) moves when the view's wording
    # does, and the engine's log is a record that cannot be rewritten: `moved
    # <old> <new>` in the session says which digest the log carries for a
    # wording the view no longer has, and the old one is read as the new one -
    # that token exactly, nothing else on the line. The world's digests (state,
    # log) never move this way, and a session that said so of them would be
    # lying: the self-test holds a wrong `moved` to a refusal.
    for old, new in session.moved:
        engine = [l.replace(old, new) for l in engine]
    for prefix in session.lines:
        e = [l for l in engine if l.startswith(prefix)]
        h = [l for l in headless if l.startswith(prefix)]
        if len(e) != 1:
            refused.append("'{}': {} in the engine's log".format(prefix, "missing" if not e else
                                                                    "{} times".format(len(e))))
            continue
        if len(h) != 1:
            refused.append("'{}': {} in the headless output".format(prefix, "missing" if not h else
                                                                      "{} times".format(len(h))))
            continue
        if e[0] != h[0]:
            column = next((i for i, (a, b) in enumerate(zip(e[0], h[0])) if a != b), min(len(e[0]), len(h[0])))
            refused.append("'{}': differs at column {}: engine '...{}' headless '...{}'".format(
                prefix, column + 1, e[0][max(0, column - 8):column + 8], h[0][max(0, column - 8):column + 8]))
    return refused


def self_test(session, log_text, headless_text):
    failures = []

    def expect(name, ok, detail=""):
        print("[session self-test] {} {}{}".format("ok  " if ok else "FAIL", name, "" if ok else ": " + detail))
        if not ok:
            failures.append(name)

    got = compare(session, log_text, headless_text)
    expect("CONTROL: the committed log agrees with the headless command", not got, "; ".join(got))

    # One hex digit changed on the engine's side: refused, naming the column.
    # The digit is taken from one of the EXPECTED lines and never from the
    # head line (a session with a `head` has a 40-hex commit first, and a
    # digit changed there is refused for the other reason - found when
    # s2.session, the first session with a head, ran this control).
    at = next((i for i, l in enumerate(log_text.split("\n"))
               if any(p in l for p in session.lines) and re.search(r"[0-9a-f]{16}", l)), None)
    assert at is not None, "the self-test needs an expected line with a 16-hex digest in it"
    before = sum(len(l) + 1 for l in log_text.split("\n")[:at])
    m = re.search(r"[0-9a-f]{16}", log_text.split("\n")[at])
    digit = before + m.start() + 7
    flipped = "0" if log_text[digit] != "0" else "1"
    got = compare(session, log_text[:digit] + flipped + log_text[digit + 1:], headless_text)
    expect("one hex digit changed in the log is refused, naming the column",
           len(got) == 1 and "differs at column" in got[0], str(got))

    # A line missing from the log: refused as missing, never passed as "nothing to compare".
    victim = session.lines[-1]
    kept = "\n".join(l for l in log_text.split("\n") if victim not in l)
    got = compare(session, kept, headless_text)
    expect("a line missing from the log is refused as missing", len(got) == 1 and "missing" in got[0], str(got))

    # The engine's decorations are not part of the line, and do not make one differ.
    dressed = "\n".join("[2026.09.16-18.00.00:000][  0]" + l.replace(": ", ": Display: ", 1) if l.startswith("Log")
                        else l for l in log_text.split("\n"))
    got = compare(session, dressed, headless_text)
    expect("the engine's timestamp and verbosity prefix are not compared", not got, str(got))

    # AS THE ENGINE REALLY WRITES IT (19.11b): the log category the UE_LOG was
    # given, then the message, which carries its own `LogVaelenX: ` - so a raw
    # line reads `[ts][f]LogVaelenWalk: LogVaelenScene: AELVOR ...` or
    # `LogVaelenPlay: LogVaelenPlay: AELVOR ...`. Read from the LAST category.
    as_written = "\n".join("[2026.09.16-18.00.00:000][  0]LogVaelenWalk: " + l if l.startswith("Log")
                            else l for l in log_text.split("\n"))
    got = compare(session, as_written, headless_text)
    expect("a line under the engine's own category is read from the line's category", not got, str(got))
    # And the first-category reading, kept as the pre-fix arm: it must REFUSE
    # such a log, or the case above is not testing anything.
    first = [("{}: {}".format(m.group(1), m.group(2)) if (m := CATEGORY.search(raw)) else None)
             for raw in as_written.split("\n")]
    expect("the pre-fix reading (the first category) does not equal the line",
           not any(l is not None and any(l.startswith(p) for p in session.lines) for l in first), str(first[:3]))

    # A sitting that names its commit: a log opened on another commit is refused.
    session.head, saved = "0123456789abcdef0123456789abcdef01234567", session.head
    got = compare(session, "0000000000000000000000000000000000000000\n" + log_text, headless_text)
    session.head = saved
    expect("a log whose first line is not the sitting's commit is refused",
           any("not the sitting's commit" in r for r in got), str(got))

    # `moved`: the substitution must do work, and only the work it says. With
    # the moved list emptied the log must be refused at the moved digest (or
    # the session carries a `moved` nothing needs); with a wrong new value it
    # must be refused as well.
    if session.moved:
        saved_moved = session.moved
        session.moved = []
        got = compare(session, log_text, headless_text)
        expect("without its `moved` the log is refused at the moved digest",
               len(got) >= 1 and all("differs" in r for r in got), str(got))
        session.moved = [(old, "0123456789abcdef") for old, _ in saved_moved]
        got = compare(session, log_text, headless_text)
        expect("a `moved` naming a wrong new value is refused",
               len(got) >= 1 and all("differs" in r for r in got), str(got))
        session.moved = saved_moved

    if session.other:
        got = compare(session, log_text, run_headless(session, ATLAS[0], session.other))
        expect("the same log against another world's command is refused ({})".format(session.other.split()[-1]),
               len(got) >= 1 and all("differs" in r for r in got), str(got))
    else:
        print("[session self-test] note: no `other` command - the run's own weight is not shown")

    if failures:
        print("[session self-test] {} control(s) FAILED".format(len(failures)))
        return 1
    print("[session self-test] every control holds")
    return 0


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("session")
    parser.add_argument("--atlas", required=True)
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    session = Session(args.session)
    ATLAS[0] = args.atlas
    with open(session.log, encoding="utf-8", errors="replace") as f:
        log_text = f.read()
    headless_text = run_headless(session, args.atlas)
    if args.self_test:
        return self_test(session, log_text, headless_text)
    refused = compare(session, log_text, headless_text)
    for r in refused:
        print("[session] REFUSED " + r)
    if refused:
        return 1
    print("[session] {}: {} line(s) the engine printed, printed again headless byte for byte{}".format(
        os.path.basename(args.session), len(session.lines),
        "" if not session.moved else " ({} view digest(s) read as moved: {})".format(
            len(session.moved), ", ".join("{} -> {}".format(o, n) for o, n in session.moved))))
    return 0


if __name__ == "__main__":
    sys.exit(main())
