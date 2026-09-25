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
CATEGORY = re.compile(r"(Log[A-Za-z]+): (?:(?:Display|Warning|Error|Log|Verbose): )?(.*)$")


class Session:
    def __init__(self, path):
        self.path = path
        self.log = None
        self.head = None
        self.run = None
        self.other = None
        self.lines = []
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
                else:
                    raise ValueError("{}:{}: unknown key '{}'".format(path, number, key))
        if not self.log or not self.run or not self.lines:
            raise ValueError("{}: a session needs log, run and at least one line".format(path))


def normal(text):
    """Every line of a log, read from its category on; lines with no category dropped."""
    out = []
    for raw in text.replace("\r", "").split("\n"):
        m = CATEGORY.search(raw)
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
    m = re.search(r"[0-9a-f]{16}", log_text)
    digit = m.start() + 7
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

    # A sitting that names its commit: a log opened on another commit is refused.
    session.head, saved = "0123456789abcdef0123456789abcdef01234567", session.head
    got = compare(session, "0000000000000000000000000000000000000000\n" + log_text, headless_text)
    session.head = saved
    expect("a log whose first line is not the sitting's commit is refused",
           any("not the sitting's commit" in r for r in got), str(got))

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
    print("[session] {}: {} line(s) the engine printed, printed again headless byte for byte".format(
        os.path.basename(args.session), len(session.lines)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
