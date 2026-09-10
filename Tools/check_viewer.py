#!/usr/bin/env python3
# VAELEN - checks the viewer page without opening a browser.
#
# Tools/Viewer/Atlas.html is a thousand lines of markup, style and script that
# nothing compiled and nothing tested. It broke three times on the day it was
# written - an orphaned variable left behind by an edit, an element id the script
# asked for and the markup did not have, a thin space that read as mojibake off a
# file:// URL - and each time a person looking at it was the only thing that
# caught it.
#
# WHAT IT DOES NOT CATCH, said first because a checker's limits matter more than
# its rules. The orphaned variable was `var html = ""` inside one function and a
# bare `html = ""` at the level above, left behind when an edit moved the
# declaration. `html` IS declared - in another scope - so no textual check sees
# it, and `node --check` does not either: the parse is valid and the failure is
# a ReferenceError at run time. Catching that needs scope analysis, which needs
# a real JavaScript parser, and reaching for one to guard a thousand-line page
# is a larger dependency than the page. Rule 5 below catches the neighbouring
# class - a name declared NOWHERE - and stops there.
#
# It is deliberately NOT a browser. A headless Chromium in CI is a hundred
# megabytes and a flake surface; what actually broke here is checkable by
# reading the file, plus `node --check` for the syntax the eye misses.
#
# --self-test proves every rule fires, the way Tools/check_kernel_purity.py and
# Tools/check_atlas_output.py do.
#
# STATUS: VALIDATED - run by the CTest entries Viewer.Page and Viewer.SelfTest
import argparse
import os
import re
import shutil
import subprocess
import sys
import tempfile

MARK = "__AELVOR_DATA__"


def script_of(page):
  """The contents of the last <script> block: the page's own code."""
  blocks = re.findall(r"<script>(.*?)</script>", page, re.S)
  return blocks[-1] if blocks else None


def check(page, node=True):
  """Returns a list of complaints; empty means the page is sound."""
  bad = []

  def want(condition, message):
    if not condition:
      bad.append(message)
    return condition

  want(MARK in page or '"vaelen"' in page,
       "the page has neither the %s placeholder nor inlined data" % MARK)
  want("<title>" in page, "the page has no <title>")

  body = script_of(page)
  if not want(body is not None, "the page has no script block"):
    return bad

  # 1. Syntax. The orphaned-variable bug was a ReferenceError at run time and a
  #    perfectly valid parse, so this does not catch everything - but a page that
  #    does not parse is a page that does nothing at all, silently.
  if node and shutil.which("node"):
    with tempfile.NamedTemporaryFile("w", suffix=".js", delete=False, encoding="utf-8") as handle:
      handle.write(body)
      path = handle.name
    try:
      done = subprocess.run(["node", "--check", path], capture_output=True, text=True)
      want(done.returncode == 0,
           "the script does not parse: %s" % (done.stderr.strip().splitlines() or [""])[0])
    finally:
      os.unlink(path)

  # 2. Every element the script reaches for exists in the markup. This is the
  #    one that would have caught a script asking for #toldBlock before the
  #    block was written.
  markup = page[:page.index("<script>")] if "<script>" in page else page
  ids = set(re.findall(r'id="([^"]+)"', markup))
  asked = set(re.findall(r'getElementById\("([^"]+)"\)', body))
  missing = sorted(asked - ids)
  want(not missing, "the script asks for elements the markup does not have: %s" % missing)

  # 3. Every toggle in the markup names a key the script knows, and every key
  #    the script offers has a toggle. A checkbox wired to nothing looks like a
  #    working control.
  offered = set(re.findall(r'\["(\w+)",\s*"', body))
  shown = re.search(r"var show = \{(.*?)\};", body, re.S)
  if want(shown is not None, "the script has no `show` object"):
    keys = set(re.findall(r"(\w+):", shown.group(1)))
    controls = re.search(r"var CONTROLS = \[(.*?)\];", body, re.S)
    if want(controls is not None, "the script has no CONTROLS list"):
      named = set(re.findall(r'\["(\w+)"', controls.group(1)))
      want(not (named - keys), "controls name keys `show` does not have: %s" % sorted(named - keys))
      want(not (keys - named), "`show` has keys no control offers: %s" % sorted(keys - named))
  del offered

  # 4. Nothing the page RENDERS may be non-ASCII. Entities survive a file:// URL
  #    with no charset; a literal does not, and reads as mojibake. Comments are
  #    exempt: they are never rendered.
  rendered = []
  for line in body.split("\n"):
    stripped = line.strip()
    if stripped.startswith("//") or stripped.startswith("/*") or stripped.startswith("*"):
      continue
    rendered.append(line)
  loud = [c for c in "".join(rendered) if ord(c) > 127]
  want(not loud, "the script renders %d non-ASCII characters (use entities): %r"
       % (len(loud), sorted(set(loud))[:6]))

  # 5. An assignment to a name nothing ever declares, anywhere. `node --check`
  #    cannot see this: the parse is valid and the failure is a ReferenceError at
  #    run time. It is NOT the bug that cost the most - that one was declared in
  #    another scope and needs a parser to see - but it is the same family, and
  #    it is free.
  # A declaration can name several: `var zoom = 1, panX = 0, panY = 0;`. Taking
  # only the first was six false positives, and a checker that cries wolf is a
  # checker somebody turns off - so the whole statement is read, and every name
  # in a declaring position counts.
  declared = set()
  for statement in re.findall(r"\b(?:var|let|const)\s+([^;]{0,600}?);", body, re.S):
    declared |= set(re.findall(r"(?:^|,)\s*(\w+)\s*(?==|,|$)", statement, re.M))
    declared |= set(re.findall(r"^\s*(\w+)", statement))
  declared |= set(re.findall(r"function\s+(\w+)", body))
  declared |= set(re.findall(r"function\s*\(([^)]*)\)", body) and
                  [a.strip() for group in re.findall(r"function\s*\(([^)]*)\)", body)
                   for a in group.split(",") if a.strip()])
  declared |= set(re.findall(r"catch\s*\((\w+)\)", body))
  declared |= {"window", "document", "console", "Math", "JSON", "String", "Number", "Array",
               "Object", "parseInt", "parseFloat", "setInterval", "clearInterval", "Event",
               "setTimeout", "clearTimeout", "requestAnimationFrame", "isNaN"}
  loose = []
  for line in body.split("\n"):
    hit = re.match(r"\s*(\w+)\s*=[^=]", line)
    if hit and hit.group(1) not in declared and not hit.group(1).isdigit():
      loose.append(hit.group(1))
  want(not loose, "assignment to names nothing declares (a ReferenceError waiting): %s"
       % sorted(set(loose))[:6])

  # 6. The page must not have lost its theme tokens. A colour defined only under
  #    a media query is the classic unreadable-artifact bug.
  root = re.search(r":root\s*\{(.*?)\}", page, re.S)
  if want(root is not None, "the page has no :root token block"):
    base = set(re.findall(r"(--[\w-]+):", root.group(1)))
    used = set(re.findall(r"var\((--[\w-]+)", page))
    orphan = sorted(used - base)
    want(not orphan, "these tokens are used but never defined on bare :root: %s" % orphan[:8])
  return bad


GOOD = """<title>A page</title>
<style>:root { --ink: #000; --paper: #fff; }
body { color: var(--ink); background: var(--paper); }</style>
<div id="only"></div>
<label><input type="checkbox" data-key="relief"></label>
<script id="aelvor" type="application/json">__AELVOR_DATA__</script>
<script>
	var show = { relief: true };
	var CONTROLS = [["relief", "Relief"]];
	var el = document.getElementById("only");
	el.innerHTML = "&mdash; fine";
</script>
"""


def self_test():
  failures = []
  skipped = []
  if check(GOOD):
    failures.append("a sound page was rejected: %s" % check(GOOD))

  # A break must be caught BY THE RULE IT IS AIMED AT. The syntax break is
  # also caught by the `show` rule, which cannot read a half-written object -
  # so without `saying` this self-test would report the parse rule exercised on
  # a machine that has no node and never ran it.
  exercised = [0]

  def breaks(name, mutate, saying=None):
    exercised[0] += 1
    said = check(mutate(GOOD))
    if not said:
      failures.append("%s: broken page accepted" % name)
    elif saying is not None and not any(saying in line for line in said):
      failures.append("%s: caught, but by the wrong rule: %s" % (name, said))

  breaks("no placeholder", lambda p: p.replace(MARK, "{}"), saying="placeholder")
  breaks("no title", lambda p: p.replace("<title>A page</title>", ""), saying="no <title>")
  if shutil.which("node"):
    breaks("syntax error", lambda p: p.replace("var show = { relief: true };", "var show = { relief: ;"),
           saying="does not parse")
  else:
    skipped.append("syntax error (no node on this machine)")
  breaks("missing element", lambda p: p.replace('id="only"', 'id="other"'),
         saying="the markup does not have")
  breaks("control with no key", lambda p: p.replace('["relief", "Relief"]', '["water", "Water"]'),
         saying="controls name keys")
  breaks("key with no control", lambda p: p.replace("var show = { relief: true };",
                                                    "var show = { relief: true, water: true };"),
         saying="keys no control offers")
  breaks("rendered non-ASCII", lambda p: p.replace('"&mdash; fine"', '"— fine"'),
         saying="renders")
  breaks("undefined token", lambda p: p.replace("var(--ink)", "var(--nowhere)"),
         saying="never defined on bare :root")
  breaks("undeclared assignment", lambda p: p.replace("\tvar el = document", "\tel = document"),
         saying="nothing declares")
  breaks("no script", lambda p: re.sub(r"<script>.*?</script>", "", p, flags=re.S),
         saying="no script block")

  for line in failures:
    print("SELF-TEST: %s" % line, file=sys.stderr)
  for line in skipped:
    print("SELF-TEST: NOT EXERCISED: %s" % line, file=sys.stderr)
  print("self-test: %d checks exercised%s, %d failures"
        % (exercised[0], ", %d NOT exercised" % len(skipped) if skipped else "", len(failures)))
  return 1 if failures else 0


def main():
  parser = argparse.ArgumentParser(description="Checks the VAELEN viewer page.")
  parser.add_argument("path", nargs="?", help="the page to check")
  parser.add_argument("--self-test", action="store_true", help="prove every rule fires")
  args = parser.parse_args()
  if args.self_test:
    return self_test()
  if not args.path:
    parser.error("a path is required unless --self-test is given")
  with open(args.path, "r", encoding="utf-8") as handle:
    page = handle.read()
  bad = check(page)
  for line in bad:
    print("%s: %s" % (args.path, line), file=sys.stderr)
  if bad:
    return 1
  body = script_of(page)
  # Say which rules actually ran. A skipped rule that reports nothing is the
  # green that means nothing, which is the failure this whole file is against.
  parsed = "script parses" if shutil.which("node") else "SCRIPT NOT PARSED (no node on this machine)"
  print("%s: %d lines, %d elements reached, %d controls, %s"
        % (args.path, page.count("\n") + 1,
           len(set(re.findall(r'getElementById\("([^"]+)"\)', body))),
           len(re.findall(r'\["(\w+)",\s*"', re.search(r"var CONTROLS = \[(.*?)\];", body, re.S).group(1))),
           parsed))
  return 0


if __name__ == "__main__":
  sys.exit(main())
