#!/usr/bin/env python3
# VAELEN - kernel purity checker.
#
# STATUS: VALIDATED (Phase 00) - self-tested with --self-test; runs in CTest as Kernel.Purity.
#
# The simulation kernel (Source/VaelenCore and the other modules listed in
# Tools/kernel_modules.txt) is compiled BOTH by Unreal Build Tool and headless by
# CMake. This script enforces, textually, the rules that keep it engine-agnostic
# and deterministic. Python 3 standard library only.
#
# Usage:
#   python3 Tools/check_kernel_purity.py --root <repo> [--modules <file>] [--list] [--verbose]
#   python3 Tools/check_kernel_purity.py --self-test
#
# Scanned files, per module M:
#   Source/M/Public/**/*.h, *.inl
#   Source/M/Private/**/*.cpp, *.h, *.inl
#   *Module.cpp is the single allowed Unreal-facing file: skipped, but at most one
#   per module (R0).
#
# Rules (each violation is printed as `path:line: Rn rule-name: message`):
#   R0 structure             - at most one *Module.cpp per module; PURITY-ALLOW comments
#                              must be well-formed. Not exemptable.
#   R1 include-whitelist     - `#include "X"` must start with "Vaelen/"; `#include <X>`
#                              must be a C/C++ standard header that is not banned
#                              (<random>, <chrono>, <ctime>, <iostream>, ... see
#                              BANNED_STD_HEADERS). <cstdio>, <atomic>, <mutex> and
#                              <thread> are allowed.
#   R2 no-exceptions         - no `throw`, `try {`, `catch (`.
#   R3 no-rtti               - no `dynamic_cast`, `typeid`.
#   R4 deterministic-random  - no rand()/srand(), std::random_device, std::mt19937,
#                              default_random_engine, std::chrono, time(NULL), clock()...
#   R5 header-hygiene        - every header has `#pragma once` (.h); every scanned file
#                              (.h, .inl and .cpp) has a `// STATUS:` line whose value is
#                              VALIDATED, PROTOTYPE, INCOMPLETE or UNVERIFIED.
#   R6 no-fake-done          - a file whose STATUS is VALIDATED contains no TODO, FIXME
#                              or "implement later".
#   R7 fixed-width           - no bare `long` / `unsigned long` / `long long` types
#                              (heuristic: `long` tokens outside static_cast<...>).
#
# R1-R4 and R7 are applied after stripping comments and string/character
# literals (line numbers are preserved); R5 and R6 are applied to the raw text.
# A violation is exempted by a trailing comment on the same line:
#   // PURITY-ALLOW(R7): reason            (several rules: PURITY-ALLOW(R1, R4): reason)
# File-level R5 violations (missing pragma/STATUS) accept the exemption on any line.
#
# Exit status: 0 when clean (`[purity] N files, 0 violations`), 1 with violations,
# 2 on a configuration error (missing root, module list or module directory).
from __future__ import annotations

import argparse
import bisect
import io
import re
import sys
import tempfile
from contextlib import redirect_stdout
from pathlib import Path
from typing import Dict, List, NamedTuple, Optional, Sequence, Set, Tuple

# ---------------------------------------------------------------------------
# Rule table
# ---------------------------------------------------------------------------

RULE_NAMES = {
  "R0": "structure",
  "R1": "include-whitelist",
  "R2": "no-exceptions",
  "R3": "no-rtti",
  "R4": "deterministic-random",
  "R5": "header-hygiene",
  "R6": "no-fake-done",
  "R7": "fixed-width",
}
EXEMPTABLE_RULES = frozenset(rule for rule in RULE_NAMES if rule != "R0")
STATUS_VALUES = ("VALIDATED", "PROTOTYPE", "INCOMPLETE", "UNVERIFIED")

MODULE_FILE_SUFFIX = "Module.cpp"
DEFAULT_MODULES_FILE = Path("Tools") / "kernel_modules.txt"
PUBLIC_EXTENSIONS = (".h", ".inl")
PRIVATE_EXTENSIONS = (".cpp", ".h", ".inl")
VAELEN_INCLUDE_PREFIX = "Vaelen/"

# C++ standard library headers (C++11 .. C++26 draft) and C standard headers.
# Anything not listed here is rejected by R1, whether it is an engine header
# (<CoreMinimal.h>), an OS header (<windows.h>, <unistd.h>) or a third party.
_CXX_STD_HEADERS = """
  algorithm any array atomic barrier bit bitset charconv codecvt compare complex
  concepts condition_variable coroutine deque debugging exception execution expected
  filesystem flat_map flat_set format forward_list fstream functional future
  generator hazard_pointer initializer_list inplace_vector iomanip ios iosfwd
  iostream istream iterator latch limits linalg list locale map mdspan memory
  memory_resource mutex new numbers numeric optional ostream print queue random
  ranges ratio rcu regex scoped_allocator semaphore set shared_mutex
  source_location span spanstream sstream stack stacktrace stdexcept stdfloat
  stop_token streambuf string string_view strstream syncstream system_error
  text_encoding thread tuple type_traits typeindex typeinfo unordered_map
  unordered_set utility valarray variant vector version
  cassert ccomplex cctype cerrno cfenv cfloat cinttypes ciso646 climits clocale
  cmath csetjmp csignal cstdalign cstdarg cstdbool cstddef cstdint cstdio cstdlib
  cstring ctgmath ctime cuchar cwchar cwctype
""".split()
_C_STD_HEADERS = """
  assert.h complex.h ctype.h errno.h fenv.h float.h inttypes.h iso646.h limits.h
  locale.h math.h setjmp.h signal.h stdalign.h stdarg.h stdatomic.h stdbit.h
  stdbool.h stddef.h stdint.h stdio.h stdlib.h stdnoreturn.h string.h tgmath.h
  threads.h time.h uchar.h wchar.h wctype.h
""".split()
STD_HEADERS = frozenset(_CXX_STD_HEADERS) | frozenset(_C_STD_HEADERS)

# Standard headers that are nevertheless forbidden in the kernel, with the reason.
_REASON_RANDOM = "non-deterministic / implementation-defined randomness; use Vaelen/Core/Random.h"
_REASON_TIME = "wall-clock time is non-deterministic; simulation time is part of the world state"
_REASON_STREAMS = "stream I/O is heavy and locale-dependent; the kernel formats with <cstdio>"
_REASON_LOCALE = "locale-dependent behaviour makes formatting platform-dependent"
_REASON_EXCEPTIONS = "exception machinery in a kernel built with -fno-exceptions"
_REASON_RTTI = "RTTI in a kernel built with -fno-rtti"
_REASON_JUMPS = "non-local jumps are exceptions in disguise"
_REASON_FS = "OS file-system access does not belong in the simulation kernel"
BANNED_STD_HEADERS = {
  "random": _REASON_RANDOM,
  "chrono": _REASON_TIME,
  "ctime": _REASON_TIME,
  "time.h": _REASON_TIME,
  "iostream": _REASON_STREAMS,
  "istream": _REASON_STREAMS,
  "ostream": _REASON_STREAMS,
  "iomanip": _REASON_STREAMS,
  "ios": _REASON_STREAMS,
  "fstream": _REASON_STREAMS,
  "sstream": _REASON_STREAMS,
  "strstream": _REASON_STREAMS,
  "syncstream": _REASON_STREAMS,
  "spanstream": _REASON_STREAMS,
  "streambuf": _REASON_STREAMS,
  "locale": _REASON_LOCALE,
  "clocale": _REASON_LOCALE,
  "locale.h": _REASON_LOCALE,
  "codecvt": _REASON_LOCALE,
  "exception": _REASON_EXCEPTIONS,
  "stdexcept": _REASON_EXCEPTIONS,
  "typeinfo": _REASON_RTTI,
  "typeindex": _REASON_RTTI,
  "csetjmp": _REASON_JUMPS,
  "setjmp.h": _REASON_JUMPS,
  "filesystem": _REASON_FS,
}

# Token patterns applied to code with comments and literals blanked out.
R2_PATTERNS = (
  (re.compile(r"\bthrow\b"), "'throw'"),
  (re.compile(r"\btry\s*\{"), "'try' block"),
  (re.compile(r"\bcatch\s*\("), "'catch' handler"),
)
R3_PATTERNS = (
  (re.compile(r"\bdynamic_cast\b"), "'dynamic_cast'"),
  (re.compile(r"\btypeid\b"), "'typeid'"),
)
R4_PATTERNS = (
  (re.compile(r"\bs?rand\s*\("), "rand()/srand()"),
  (re.compile(r"\brandom_device\b"), "std::random_device"),
  (re.compile(r"\bmt19937(?:_64)?\b"), "std::mt19937"),
  (re.compile(r"\b(?:default_random_engine|minstd_rand0?|ranlux(?:24|48)(?:_base)?|knuth_b)\b"),
   "a <random> engine"),
  (re.compile(r"\bstd\s*::\s*chrono\b"), "std::chrono"),
  (re.compile(r"\b(?:steady_clock|system_clock|high_resolution_clock)\b"), "a std::chrono clock"),
  (re.compile(r"\btime\s*\(\s*(?:NULL|nullptr|0|&\s*\w+)?\s*\)"), "time()"),
  (re.compile(r"\bclock\s*\("), "clock()"),
  (re.compile(r"\b(?:gettimeofday|clock_gettime|timespec_get|QueryPerformanceCounter|GetTickCount(?:64)?|__rdtsc)\s*\("),
   "an OS clock"),
)
R6_PATTERNS = (
  (re.compile(r"\bTODO\b"), "TODO"),
  (re.compile(r"\bFIXME\b"), "FIXME"),
  (re.compile(r"\bimplement\s+later\b", re.IGNORECASE), "'implement later'"),
)
R7_LONG_RE = re.compile(r"\b(?:unsigned\s+)?long\b(?:\s+(?:long|int|double)\b)*")
STATIC_CAST_RE = re.compile(r"\bstatic_cast\s*<")

INCLUDE_RE = re.compile(r'^[ \t]*#[ \t]*include[ \t]*(?:<([^>\n]*)>|"([^"\n]*)"|(\S+))', re.MULTILINE)
PRAGMA_ONCE_RE = re.compile(r"^[ \t]*#[ \t]*pragma[ \t]+once\b", re.MULTILINE)
STATUS_RE = re.compile(r"^\s*//\s*STATUS\s*:\s*([A-Za-z_-]+)")
EXEMPT_RE = re.compile(r"//\s*PURITY-ALLOW\s*\(([^)]*)\)\s*(.*)$")
EXEMPT_MARKER = "PURITY-ALLOW"
EXEMPT_SYNTAX = "expected '// PURITY-ALLOW(Rn): reason'"


# ---------------------------------------------------------------------------
# Data
# ---------------------------------------------------------------------------

class Violation(NamedTuple):
  path: str
  line: int
  rule: str
  message: str
  file_level: bool = False

  def format(self) -> str:
    return "%s:%d: %s %s: %s" % (self.path, self.line, self.rule, RULE_NAMES[self.rule], self.message)


class Exemption(NamedTuple):
  line: int
  rule: str
  reason: str


class FileReport(NamedTuple):
  path: str
  violations: List[Violation]
  exempted: List[Tuple[Violation, str]]
  unused_exemptions: List[Exemption]


class ModuleInfo(NamedTuple):
  name: str
  files: List[Path]
  module_files: List[Path]


class Report(NamedTuple):
  root: Path
  modules: List[ModuleInfo]
  files: List[str]
  file_reports: List[FileReport]
  structural: List[Violation]
  errors: List[str]

  @property
  def violations(self) -> List[Violation]:
    result = list(self.structural)
    for report in self.file_reports:
      result.extend(report.violations)
    result.sort(key=lambda v: (v.path, v.line, v.rule, v.message))
    return result

  @property
  def exempted(self) -> List[Tuple[Violation, str]]:
    result: List[Tuple[Violation, str]] = []
    for report in self.file_reports:
      result.extend(report.exempted)
    return result


# ---------------------------------------------------------------------------
# Lexer: classify every character as code, comment or literal
# ---------------------------------------------------------------------------

CODE, COMMENT, LITERAL = 0, 1, 2
RAW_STRING_PREFIXES = frozenset(("R", "u8R", "uR", "UR", "LR"))
CHAR_PREFIXES = frozenset(("u8", "u", "U", "L"))


def _word_run_before(text: str, index: int) -> str:
  """Maximal identifier-character run ending just before `index`."""
  start = index
  while start > 0 and (text[start - 1].isalnum() or text[start - 1] == "_"):
    start -= 1
  return text[start:index]


def _scan_quoted(text: str, index: int, quote: str) -> int:
  """Returns the index just past a (non-raw) literal whose opening quote is at index-1.
  An unterminated literal ends at the end of its line so it cannot swallow the file."""
  size = len(text)
  while index < size:
    ch = text[index]
    if ch == "\n":
      break
    if ch == "\\":
      index += 2
      continue
    index += 1
    if ch == quote:
      break
  return min(index, size)


def classify(text: str) -> bytearray:
  """One kind (CODE / COMMENT / LITERAL) per character of `text`."""
  size = len(text)
  kinds = bytearray(size)
  index = 0
  while index < size:
    ch = text[index]
    nxt = text[index + 1] if index + 1 < size else ""
    if ch == "/" and nxt == "/":
      end = index + 2
      while end < size and text[end] != "\n":
        if text[end] == "\\" and end + 1 < size and text[end + 1] == "\n":
          end += 2  # line splicing: the comment continues on the next line
          continue
        end += 1
      kinds[index:end] = bytes((COMMENT,)) * (end - index)
      index = end
      continue
    if ch == "/" and nxt == "*":
      close = text.find("*/", index + 2)
      end = size if close < 0 else close + 2
      kinds[index:end] = bytes((COMMENT,)) * (end - index)
      index = end
      continue
    if ch == '"':
      if _word_run_before(text, index) in RAW_STRING_PREFIXES:
        open_paren = text.find("(", index + 1)
        if open_paren < 0:
          end = size
        else:
          delimiter = text[index + 1:open_paren]
          close = text.find(")" + delimiter + '"', open_paren + 1)
          end = size if close < 0 else close + len(delimiter) + 2
      else:
        end = _scan_quoted(text, index + 1, '"')
      kinds[index:end] = bytes((LITERAL,)) * (end - index)
      index = end
      continue
    if ch == "'":
      prefix = _word_run_before(text, index)
      if prefix and prefix not in CHAR_PREFIXES:
        index += 1  # digit separator (1'000'000) or an apostrophe glued to a word
        continue
      end = _scan_quoted(text, index + 1, "'")
      kinds[index:end] = bytes((LITERAL,)) * (end - index)
      index = end
      continue
    index += 1
  return kinds


def blank(text: str, kinds: bytearray, blanked: Sequence[int]) -> str:
  """Replaces every character of the given kinds with a space, keeping newlines."""
  chars = list(text)
  for index, kind in enumerate(kinds):
    if kind in blanked and chars[index] != "\n":
      chars[index] = " "
  return "".join(chars)


class SourceText:
  """A source file in three views sharing the same offsets and line numbers."""

  def __init__(self, raw: str):
    self.raw = raw
    self.raw_lines = raw.split("\n")
    kinds = classify(raw)
    self.no_comments = blank(raw, kinds, (COMMENT,))
    self.code = blank(raw, kinds, (COMMENT, LITERAL))
    self._line_starts = [0] + [index + 1 for index, ch in enumerate(raw) if ch == "\n"]

  def line_of(self, offset: int) -> int:
    return bisect.bisect_right(self._line_starts, offset)


# ---------------------------------------------------------------------------
# Rules
# ---------------------------------------------------------------------------

def parse_exemptions(path: str, raw_lines: Sequence[str]) -> Tuple[Dict[int, Dict[str, str]], List[Violation]]:
  exemptions: Dict[int, Dict[str, str]] = {}
  violations: List[Violation] = []
  for number, line in enumerate(raw_lines, 1):
    if EXEMPT_MARKER not in line:
      continue
    match = EXEMPT_RE.search(line)
    if match is None:
      violations.append(Violation(path, number, "R0", "malformed PURITY-ALLOW comment; " + EXEMPT_SYNTAX))
      continue
    rules = [item.strip() for item in match.group(1).split(",")]
    unknown = [rule for rule in rules if rule not in EXEMPTABLE_RULES]
    if unknown:
      violations.append(Violation(path, number, "R0",
                                  "PURITY-ALLOW names unknown or non-exemptable rule(s) %s (allowed: R1-R7)"
                                  % ", ".join(repr(rule) for rule in unknown)))
      continue
    rest = match.group(2).strip()
    reason = rest[1:].strip() if rest.startswith(":") else ""
    if not reason:
      violations.append(Violation(path, number, "R0", "PURITY-ALLOW without a reason; " + EXEMPT_SYNTAX))
      continue
    for rule in rules:
      exemptions.setdefault(number, {})[rule] = reason
  return exemptions, violations


def check_includes(path: str, src: SourceText) -> List[Violation]:
  out: List[Violation] = []
  for match in INCLUDE_RE.finditer(src.no_comments):
    line = src.line_of(match.start())
    angle, quoted, other = match.group(1), match.group(2), match.group(3)
    if angle is not None:
      name = angle.strip()
      if name in BANNED_STD_HEADERS:
        out.append(Violation(path, line, "R1", "<%s> is banned in the kernel: %s" % (name, BANNED_STD_HEADERS[name])))
      elif name not in STD_HEADERS:
        out.append(Violation(path, line, "R1",
                             "<%s> is not a C/C++ standard header; the kernel may only include the standard "
                             "library and \"Vaelen/...\" headers" % name))
    elif quoted is not None:
      name = quoted.strip()
      if not name.startswith(VAELEN_INCLUDE_PREFIX):
        out.append(Violation(path, line, "R1",
                             "\"%s\" is not a Vaelen/ header (engine, OS or third-party header in the kernel)" % name))
    else:
      out.append(Violation(path, line, "R1", "non-literal include '%s' cannot be verified" % other))
  return out


def check_patterns(path: str, text: str, src: SourceText, rule: str,
                   patterns: Sequence[Tuple["re.Pattern[str]", str]], what: str) -> List[Violation]:
  """One violation per (line, rule): the first matching pattern on a line wins."""
  found: Dict[int, Violation] = {}
  for pattern, label in patterns:
    for match in pattern.finditer(text):
      line = src.line_of(match.start())
      if line not in found:
        found[line] = Violation(path, line, rule, "%s: %s" % (what, label))
  return [found[line] for line in sorted(found)]


def _blank_static_casts(code: str) -> str:
  """Blanks the template argument of every static_cast<...> (angle-bracket matched)."""
  chars = list(code)
  for match in STATIC_CAST_RE.finditer(code):
    depth = 0
    index = match.end() - 1  # the '<'
    while index < len(code):
      ch = code[index]
      if ch == "<":
        depth += 1
      elif ch == ">":
        depth -= 1
        if depth == 0:
          break
      if ch != "\n":
        chars[index] = " "
      index += 1
  return "".join(chars)


def check_fixed_width(path: str, src: SourceText) -> List[Violation]:
  found: Dict[int, Violation] = {}
  code = _blank_static_casts(src.code)
  for match in R7_LONG_RE.finditer(code):
    line = src.line_of(match.start())
    if line not in found:
      spelled = " ".join(match.group(0).split())
      found[line] = Violation(path, line, "R7",
                              "'%s' has a platform-dependent width; use int64/uint64 from Vaelen/Core/CoreTypes.h "
                              "(static_cast<long long>(...) is allowed at printf boundaries)" % spelled)
  return [found[line] for line in sorted(found)]


def check_header_hygiene(path: str, src: SourceText, extension: str) -> Tuple[List[Violation], Optional[str]]:
  out: List[Violation] = []
  is_header = extension in (".h", ".inl")
  if extension == ".h" and PRAGMA_ONCE_RE.search(src.no_comments) is None:
    out.append(Violation(path, 1, "R5", "header lacks '#pragma once'", file_level=True))
  status: Optional[str] = None
  for number, line in enumerate(src.raw_lines, 1):
    match = STATUS_RE.match(line)
    if match is None:
      continue
    value = match.group(1)
    if value not in STATUS_VALUES:
      out.append(Violation(path, number, "R5", "STATUS value '%s' is not one of %s"
                           % (value, ", ".join(STATUS_VALUES))))
    else:
      status = value
    break
  if status is None and not any(v.message.startswith("STATUS value") for v in out):
    out.append(Violation(path, 1, "R5", "%s lacks a '// STATUS: <%s>' line"
                         % ("header" if is_header else "source file", "|".join(STATUS_VALUES)),
                         file_level=True))
  return out, status


def check_no_fake_done(path: str, src: SourceText, status: Optional[str]) -> List[Violation]:
  if status != "VALIDATED":
    return []
  out: List[Violation] = []
  for pattern, label in R6_PATTERNS:
    for match in pattern.finditer(src.raw):
      line = src.line_of(match.start())
      out.append(Violation(path, line, "R6", "file is marked VALIDATED but contains %s" % label))
  out.sort(key=lambda v: (v.line, v.message))
  return out


def check_file(path: str, raw: str, extension: str) -> FileReport:
  src = SourceText(raw)
  exemptions, structural = parse_exemptions(path, src.raw_lines)

  candidates: List[Violation] = []
  candidates += check_includes(path, src)
  candidates += check_patterns(path, src.code, src, "R2", R2_PATTERNS, "exception machinery")
  candidates += check_patterns(path, src.code, src, "R3", R3_PATTERNS, "RTTI")
  candidates += check_patterns(path, src.code, src, "R4", R4_PATTERNS, "non-deterministic source")
  hygiene, status = check_header_hygiene(path, src, extension)
  candidates += hygiene
  candidates += check_no_fake_done(path, src, status)
  candidates += check_fixed_width(path, src)

  violations: List[Violation] = list(structural)
  exempted: List[Tuple[Violation, str]] = []
  used: Set[Tuple[int, str]] = set()
  for violation in candidates:
    reason: Optional[str] = None
    if violation.file_level:
      for number in sorted(exemptions):
        if violation.rule in exemptions[number]:
          reason = exemptions[number][violation.rule]
          used.add((number, violation.rule))
          break
    else:
      rules = exemptions.get(violation.line, {})
      if violation.rule in rules:
        reason = rules[violation.rule]
        used.add((violation.line, violation.rule))
    if reason is None:
      violations.append(violation)
    else:
      exempted.append((violation, reason))

  unused = [Exemption(number, rule, reason)
            for number in sorted(exemptions)
            for rule, reason in sorted(exemptions[number].items())
            if (number, rule) not in used]
  violations.sort(key=lambda v: (v.line, v.rule, v.message))
  return FileReport(path, violations, exempted, unused)


# ---------------------------------------------------------------------------
# Discovery
# ---------------------------------------------------------------------------

def read_module_list(text: str) -> List[str]:
  names: List[str] = []
  for line in text.splitlines():
    name = line.split("#", 1)[0].strip()
    if name and name not in names:
      names.append(name)
  return names


def relative_posix(root: Path, path: Path) -> str:
  return path.relative_to(root).as_posix()


MODULE_NAME_RE = re.compile(r"[A-Za-z0-9_]+")


def discover_module(root: Path, name: str) -> Tuple[Optional[ModuleInfo], Optional[str]]:
  if MODULE_NAME_RE.fullmatch(name) is None:
    return None, "module name '%s' is not a plain identifier (no path components allowed)" % name
  base = root / "Source" / name
  if not base.is_dir():
    return None, "module '%s' listed in the module list has no directory %s" % (name, base.as_posix())
  files: List[Path] = []
  module_files: List[Path] = []
  seen_dir = False
  for directory, extensions in ((base / "Public", PUBLIC_EXTENSIONS), (base / "Private", PRIVATE_EXTENSIONS)):
    if directory.is_symlink():
      return None, "module '%s': %s is a symlink, which is not scanned" % (name, relative_posix(root, directory))
    if not directory.is_dir():
      continue
    seen_dir = True
    for path in sorted(directory.rglob("*")):
      if path.is_symlink():
        return None, "module '%s': %s is a symlink, which is not allowed in a kernel module" % (
          name, relative_posix(root, path))
      if not path.is_file():
        continue
      if path.name.endswith(MODULE_FILE_SUFFIX):
        module_files.append(path)
        continue
      if path.suffix in extensions:
        files.append(path)
  if not seen_dir:
    return None, "module '%s' has neither a Public nor a Private directory under %s" % (name, base.as_posix())
  if not files:
    return None, "module '%s' has no scannable source files (a vacuous pass is not a pass)" % name
  files.sort(key=lambda p: relative_posix(root, p))
  module_files.sort(key=lambda p: relative_posix(root, p))
  return ModuleInfo(name, files, module_files), None


def run(root: Path, modules_file: Path) -> Report:
  errors: List[str] = []
  if not root.is_dir():
    errors.append("root directory %s does not exist" % root.as_posix())
    return Report(root, [], [], [], [], errors)
  if not modules_file.is_file():
    errors.append("module list %s does not exist" % modules_file.as_posix())
    return Report(root, [], [], [], [], errors)
  names = read_module_list(modules_file.read_text(encoding="utf-8-sig", errors="replace"))
  if not names:
    errors.append("module list %s names no module" % modules_file.as_posix())
    return Report(root, [], [], [], [], errors)

  modules: List[ModuleInfo] = []
  structural: List[Violation] = []
  for name in names:
    info, error = discover_module(root, name)
    if error is not None:
      errors.append(error)
      continue
    assert info is not None
    modules.append(info)
    if len(info.module_files) > 1:
      listed = ", ".join(relative_posix(root, p) for p in info.module_files)
      for path in info.module_files:
        structural.append(Violation(relative_posix(root, path), 1, "R0",
                                    "module %s has %d Unreal-facing *Module.cpp files, at most one is allowed: %s"
                                    % (name, len(info.module_files), listed)))
  if errors:
    return Report(root, modules, [], [], structural, errors)

  files: List[str] = []
  file_reports: List[FileReport] = []
  for info in modules:
    for path in info.files:
      rel = relative_posix(root, path)
      files.append(rel)
      raw = path.read_text(encoding="utf-8-sig", errors="replace")
      file_reports.append(check_file(rel, raw, path.suffix))
  return Report(root, modules, files, file_reports, structural, errors)


# ---------------------------------------------------------------------------
# Command line
# ---------------------------------------------------------------------------

def print_report(report: Report, verbose: bool) -> int:
  violations = report.violations
  if verbose:
    for info in report.modules:
      skipped = ", ".join(relative_posix(report.root, p) for p in info.module_files) or "none"
      print("[purity] module %s: %d files (Unreal-facing, skipped: %s)" % (info.name, len(info.files), skipped))
    for file_report in report.file_reports:
      for violation, reason in file_report.exempted:
        print("%s (exempted: %s)" % (violation.format(), reason))
      for exemption in file_report.unused_exemptions:
        print("%s:%d: note: unused PURITY-ALLOW(%s): %s"
              % (file_report.path, exemption.line, exemption.rule, exemption.reason))
  for violation in violations:
    print(violation.format())
  if verbose:
    exempted = report.exempted
    for rule in sorted(RULE_NAMES):
      count = sum(1 for v in violations if v.rule == rule)
      exempt_count = sum(1 for v, _ in exempted if v.rule == rule)
      print("[purity] %s %-21s %d violation%s, %d exempted"
            % (rule, RULE_NAMES[rule] + ":", count, "" if count == 1 else "s", exempt_count))
  print("[purity] %d files, %d violation%s" % (len(report.files), len(violations), "" if len(violations) == 1 else "s"))
  return 0 if not violations else 1


def main(argv: Optional[Sequence[str]] = None) -> int:
  parser = argparse.ArgumentParser(description="VAELEN kernel purity checker (see the header of this file).")
  parser.add_argument("--root", help="repository root (default: the parent of this script's directory)")
  parser.add_argument("--modules", help="module list file (default: <root>/Tools/kernel_modules.txt)")
  parser.add_argument("--list", action="store_true", help="print the scanned files and exit")
  parser.add_argument("--verbose", action="store_true", help="print per-module and per-rule counts and exemptions")
  parser.add_argument("--self-test", action="store_true", help="exercise every rule on temporary files and exit")
  args = parser.parse_args(argv)

  # Violations may quote non-UTF-8 bytes (decoded as U+FFFD); never let the
  # report itself crash on a non-UTF-8 console (Windows ctest pipes).
  for stream in (sys.stdout, sys.stderr):
    try:
      stream.reconfigure(errors="backslashreplace")  # type: ignore[attr-defined]
    except (AttributeError, ValueError):
      pass

  if args.self_test:
    return self_test()

  root = Path(args.root).resolve() if args.root else Path(__file__).resolve().parent.parent
  modules_file = Path(args.modules).resolve() if args.modules else root / DEFAULT_MODULES_FILE
  report = run(root, modules_file)
  if report.errors:
    for error in report.errors:
      print("[purity] error: %s" % error, file=sys.stderr)
    return 2
  if args.list:
    for path in report.files:
      print(path)
    return 0
  return print_report(report, args.verbose)


# ---------------------------------------------------------------------------
# Self-test
# ---------------------------------------------------------------------------

class _FileBuilder:
  """Builds a test file line by line while recording the expected findings."""

  def __init__(self, path: str):
    self.path = path
    self.lines: List[str] = []
    self.expected: List[Tuple[str, int, str]] = []
    self.expected_exempt: List[Tuple[str, int, str]] = []
    self.expected_unused: List[Tuple[str, int, str]] = []

  def line(self, text: str, *rules: str, exempt: Sequence[str] = (), unused: Sequence[str] = ()) -> int:
    self.lines.append(text)
    number = len(self.lines)
    for rule in rules:
      self.expected.append((self.path, number, rule))
    for rule in exempt:
      self.expected_exempt.append((self.path, number, rule))
    for rule in unused:
      self.expected_unused.append((self.path, number, rule))
    return number

  def text(self) -> str:
    return "\n".join(self.lines) + "\n"


def _build_self_test_repo(root: Path) -> Tuple[List[Tuple[str, int, str]], List[Tuple[str, int, str]],
                                              List[Tuple[str, int, str]]]:
  builders: List[_FileBuilder] = []

  def new(path: str) -> _FileBuilder:
    builder = _FileBuilder(path)
    builders.append(builder)
    return builder

  (root / "Tools").mkdir(parents=True)
  (root / "Tools" / "kernel_modules.txt").write_text(
    "# self-test module list\n\nFakeKernel   # trailing comment\nOtherKernel\nFakeKernel\n", encoding="utf-8")
  (root / "Tools" / "clean_modules.txt").write_text("OtherKernel\n", encoding="utf-8")
  (root / "Tools" / "missing_modules.txt").write_text("NoSuchModule\n", encoding="utf-8")
  (root / "Tools" / "empty_modules.txt").write_text("EmptyKernel\n", encoding="utf-8")
  (root / "Tools" / "traversal_modules.txt").write_text("../Outside\n", encoding="utf-8")
  (root / "Tools" / "bom_modules.txt").write_bytes(b"\xef\xbb\xbfOtherKernel\r\n")
  (root / "Source" / "EmptyKernel" / "Public").mkdir(parents=True)
  (root / "Source" / "EmptyKernel" / "Private").mkdir(parents=True)

  # -- A clean header exercising every lexer corner case: must produce nothing.
  b = new("Source/FakeKernel/Public/Vaelen/Fake/Clean.h")
  b.line("// VAELEN - purity self-test: a clean header. Comments may mention throw,")
  b.line("// dynamic_cast, rand(), std::chrono, typeid and long.")
  b.line("//")
  b.line("// STATUS: VALIDATED (self-test)")
  b.line("#pragma once")
  b.line("")
  b.line('#include "Vaelen/Fake/Sub/Deep.h"')
  b.line("#include <cstdint>")
  b.line("#include <cstdio>")
  b.line("#include <atomic>")
  b.line("#include <mutex>")
  b.line("#include <thread>")
  b.line("#include <string_view>")
  b.line("#  include  <cmath>   /* odd spacing is still an include */")
  b.line("")
  b.line("/* block comment: try { } catch (...) { throw; } dynamic_cast typeid rand() long */")
  b.line("namespace Vaelen")
  b.line("{")
  b.line('\tinline const char* Words() { return "throw try { catch ( dynamic_cast typeid rand( long"; }')
  b.line('\tinline const char* Url() { return "http://example.invalid/x // still a string, not a comment"; }')
  b.line('\tinline const char* Raw() { return R"raw(dynamic_cast "quoted" // no comment)raw"; }')
  b.line("\tinline char Quote() { return '\"'; }")
  b.line("\tinline char Apostrophe() { return '\\''; }")
  b.line("\tinline std::int64_t Million() { return 1'000'000; }")
  b.line("\tinline std::uint32_t HexSeparated() { return 0xFFFF'FFFFu; }")
  b.line("\tinline char16_t Prefixed() { return u'x'; }")
  b.line("\tinline wchar_t Wide() { return L'\\\\'; }")
  b.line('\tinline void Print(std::int64_t Value) { std::printf("%lld", static_cast<long long>(Value)); }')
  b.line('\tinline void PrintU(std::uint64_t Value) { std::printf("%llu", static_cast<unsigned long long>(Value)); }')
  b.line("\tinline int Longitude(int longitude) { return longitude; }")
  b.line("\tinline int Retry(int tryCount) { return tryCount; }")
  b.line("\tinline int Randomness(int myrand) { return myrand; }")
  b.line("\tinline int Timer(int uptime) { return uptime; }")
  b.line("\tinline const char* Status() { return \"STATUS: DONE\"; }")
  b.line("} // namespace Vaelen")

  b = new("Source/FakeKernel/Public/Vaelen/Fake/Sub/Deep.h")
  b.line("// VAELEN - purity self-test: nested directory recursion.")
  b.line("// STATUS: PROTOTYPE")
  b.line("#pragma once")
  b.line("#include <cstddef>")
  b.line("// TODO: allowed because the file is PROTOTYPE, not VALIDATED")
  b.line("namespace Vaelen { inline std::size_t DeepSize() { return sizeof(std::size_t); } }")

  # -- Every rule violated, plus exemptions of every kind.
  b = new("Source/FakeKernel/Private/Bad.cpp")
  b.line("// VAELEN - purity self-test: every rule violated.")
  b.line("// STATUS: VALIDATED (self-test)")
  b.line('#include "CoreMinimal.h"', "R1")
  b.line('#include "Vaelen/Fake/Clean.h"')
  b.line("#include <windows.h>", "R1")
  b.line("#include <random>", "R1")
  b.line("#include <chrono>", "R1")
  b.line("#include <ctime>", "R1")
  b.line("#include <iostream>", "R1")
  b.line("#include <cstdio>")
  b.line("#include VAELEN_DYNAMIC_HEADER", "R1")
  b.line("#include <Vaelen/Fake/Clean.h>", "R1")
  b.line("#include <fstream> // PURITY-ALLOW(R1): exempted include", exempt=["R1"])
  b.line("")
  b.line("namespace Vaelen")
  b.line("{")
  b.line("\tvoid Boom(Base* D)")
  b.line("\t{")
  b.line("\t\tthrow 42;", "R2")
  b.line("\t\ttry", "R2")
  b.line("\t\t{")
  b.line("\t\t}")
  b.line("\t\tcatch (...)", "R2")
  b.line("\t\t{")
  b.line("\t\t}")
  b.line("\t\tBase* B = dynamic_cast<Base*>(D);", "R3")
  b.line("\t\tconst auto& T = typeid(B);", "R3")
  b.line("\t\tint R = rand();", "R4")
  b.line("\t\tsrand(1);", "R4")
  b.line("\t\tstd::random_device Dev;", "R4")
  b.line("\t\tstd::mt19937 Gen(1);", "R4")
  b.line("\t\tstd::default_random_engine Eng;", "R4")
  b.line("\t\tauto Now = std::chrono::steady_clock::now();", "R4")
  b.line("\t\ttime_t Now2 = time(NULL);", "R4")
  b.line("\t\ttime_t Now3 = time(nullptr);", "R4")
  b.line("\t\tclock_t C = clock();", "R4")
  b.line("\t\tlong A = 0;", "R7")
  b.line("\t\tunsigned long B2 = 0;", "R7")
  b.line("\t\tlong long C2 = 0;", "R7")
  b.line("\t\tunsigned long long D2 = static_cast<unsigned long long>(A);", "R7")
  b.line("\t\tconst long long E = static_cast<long long>(A); // PURITY-ALLOW(R7): printf boundary", exempt=["R7"])
  b.line("\t\t// TODO: finish", "R6")
  b.line("\t\t// FIXME later", "R6")
  b.line("\t\t// we will Implement Later", "R6")
  b.line("\t\tthrow 1; // PURITY-ALLOW(R3): wrong rule does not exempt R2", "R2", unused=["R3"])
  b.line("\t\tlong F = 1; // PURITY-ALLOW(R7)", "R7", "R0")
  b.line("\t\tlong G = 1; // PURITY-ALLOW(R9): unknown rule", "R7", "R0")
  b.line("\t\tlong G2 = 1; // PURITY-ALLOW(R0): structure is not exemptable", "R7", "R0")
  b.line("\t\tlong G3 = 1; // PURITY-ALLOW R7: missing parentheses", "R7", "R0")
  b.line("\t\tlong H = 1; // PURITY-ALLOW(R7): allowed", exempt=["R7"])
  b.line("\t\tint I = rand(); throw 2; // PURITY-ALLOW(R4, R2): two rules at once", exempt=["R4", "R2"])
  b.line("\t\t// PURITY-ALLOW(R2): unused exemption", unused=["R2"])
  b.line("\t}")
  b.line("}")

  b = new("Source/FakeKernel/Private/NoStatus.h")
  b.line("int NoStatusNoPragma;", "R5", "R5")

  # A UTF-8 BOM and CRLF line endings (Windows editors) must not produce false
  # R5 violations: this header is clean.
  b = new("Source/FakeKernel/Public/Vaelen/Fake/Bom.h")
  b.line("\ufeff#pragma once\r")
  b.line("// STATUS: VALIDATED (self-test, BOM + CRLF)\r")
  b.line("#include <cstdint>\r")
  b.line("inline int Bom() { return 1; }\r")

  b = new("Source/FakeKernel/Private/BadStatus.h")
  b.line("#pragma once")
  b.line("// STATUS: DONE", "R5")

  b = new("Source/FakeKernel/Private/Legacy.h")
  b.line("// STATUS: PROTOTYPE")
  b.line("// PURITY-ALLOW(R5): included several times on purpose (X-macro)")
  b.line("int Legacy;")
  b.expected_exempt.append((b.path, 1, "R5"))  # file-level violations are reported at line 1

  b = new("Source/FakeKernel/Private/Proto.inl")
  b.line("// STATUS: INCOMPLETE")
  b.line("// TODO: fine, the file is not VALIDATED; no #pragma once needed for .inl")
  b.line("inline int Proto() { return 1; }")

  b = new("Source/FakeKernel/Private/NoStatus.inl")
  b.line("inline int NoStatusInl() { return 1; }", "R5")

  b = new("Source/FakeKernel/Private/Validated.cpp")
  b.line("// STATUS: VALIDATED")
  b.line("// FIXME: a .cpp with a VALIDATED status is held to R6 too", "R6")

  # -- Two Unreal-facing files: both skipped for R1-R7, both reported by R0.
  for module_file in ("Source/FakeKernel/Private/FakeKernelModule.cpp", "Source/FakeKernel/Private/ExtraModule.cpp"):
    b = new(module_file)
    b.line("// STATUS: UNVERIFIED - Unreal-facing", "R0")
    b.line('#include "Modules/ModuleManager.h"')
    b.line("void Unreal() { throw 1; }")

  # -- A second, clean module: one Module.cpp (no R0), a .cpp with a STATUS line.
  b = new("Source/OtherKernel/Public/Vaelen/Other/Thing.h")
  b.line("// STATUS: UNVERIFIED")
  b.line("#pragma once")
  b.line('#include "Vaelen/Other/Thing.h"')
  b.line("namespace Vaelen { int Thing(); }")

  b = new("Source/OtherKernel/Private/Thing.cpp")
  b.line("// STATUS: PROTOTYPE")
  b.line('#include "Vaelen/Other/Thing.h"')
  b.line("namespace Vaelen { int Thing() { return 1; } }")

  b = new("Source/FakeKernel/Private/NoStatus.cpp")
  b.line('#include "Vaelen/Fake/Clean.h"', "R5")  # file-level: reported at line 1

  b = new("Source/OtherKernel/Private/OtherKernelModule.cpp")
  b.line('#include "Modules/ModuleManager.h"')

  b = new("Source/OtherKernel/Private/Notes.txt")
  b.line("throw dynamic_cast rand() long: not a C++ file, never scanned")

  b = new("Source/OtherKernel/Public/Vaelen/Other/Ignored.cpp")
  b.line("throw 1; // a .cpp under Public is not part of the scan set")

  expected: List[Tuple[str, int, str]] = []
  expected_exempt: List[Tuple[str, int, str]] = []
  expected_unused: List[Tuple[str, int, str]] = []
  for builder in builders:
    target = root / builder.path
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(builder.text(), encoding="utf-8")
    expected += builder.expected
    expected_exempt += builder.expected_exempt
    expected_unused += builder.expected_unused
  return sorted(expected), sorted(expected_exempt), sorted(expected_unused)


def self_test() -> int:
  failures: List[str] = []
  checks = [0]

  def expect(condition: bool, what: str) -> None:
    checks[0] += 1
    if not condition:
      failures.append(what)

  # -- Lexer.
  def code_of(text: str) -> str:
    return SourceText(text).code

  sample = "int a; // throw\nconst char* s = \"throw\"; /* try { */ char c = '\"'; throw X;\n"
  code = code_of(sample)
  expect(len(code) == len(sample) and code.count("\n") == sample.count("\n"), "lexer preserves length and newlines")
  expect(code.count("throw") == 1 and "try" not in code, "lexer blanks comments and literals")
  expect(code_of('R"d(dynamic_cast "q" // c)d" typeid(x);').count("typeid") == 1
         and "dynamic_cast" not in code_of('R"d(dynamic_cast "q" // c)d" typeid(x);'), "lexer handles raw strings")
  expect(code_of("int x = 1'000'000; throw Y;").count("throw") == 1, "lexer treats digit separators as code")
  expect(code_of("auto c = u'x'; auto w = L'\\''; throw Z;").count("throw") == 1
         and "x" not in code_of("auto c = u'x'; throw Z;").replace("throw Z", ""),
         "lexer handles prefixed character literals")
  expect(code_of("// c \\\nthrow A;\nthrow B;\n").count("throw") == 1, "lexer honours line splicing in // comments")
  expect(code_of('const char* s = "oops;\nthrow Q;\n').count("throw") == 1,
         "an unterminated string does not swallow the next line")
  no_comments = SourceText('#include "Vaelen/X.h" // c\n').no_comments
  expect('"Vaelen/X.h"' in no_comments and "// c" not in no_comments, "no_comments view keeps literals")
  expect(SourceText("a\nb\nc").line_of(4) == 3 and SourceText("a\nb\nc").line_of(0) == 1, "line_of maps offsets")
  expect(code_of("x = a /* unterminated").strip() == "x = a", "an unterminated block comment runs to end of file")
  expect(code_of('auto s = operator""_vhash; throw W;').count("throw") == 1, "empty literal after operator")

  # -- Module list parsing.
  expect(read_module_list("# c\nA # t\n\nB\nA\n") == ["A", "B"], "module list: comments, blanks, duplicates")

  # -- Full run on a synthetic repository.
  with tempfile.TemporaryDirectory(prefix="vaelen-purity-") as temp:
    root = Path(temp).resolve()
    expected, expected_exempt, expected_unused = _build_self_test_repo(root)

    report = run(root, root / "Tools" / "kernel_modules.txt")
    expect(not report.errors, "synthetic repo runs without configuration errors: %s" % report.errors)
    actual = sorted((v.path, v.line, v.rule) for v in report.violations)
    if actual != expected:
      missing = sorted(set(expected) - set(actual))
      unexpected = sorted(set(actual) - set(expected))
      detail = "\n      missing: %s\n      unexpected: %s\n      actual: %s" % (missing, unexpected, actual)
      failures.append("violations differ from the expected set" + detail)
      for violation in report.violations:
        print("    " + violation.format())
    checks[0] += 1
    actual_exempt = sorted((v.path, v.line, v.rule) for v, _ in report.exempted)
    expect(actual_exempt == expected_exempt, "exempted set: expected %s, got %s" % (expected_exempt, actual_exempt))
    actual_unused = sorted((r.path, e.line, e.rule) for r in report.file_reports for e in r.unused_exemptions)
    expect(actual_unused == expected_unused, "unused exemptions: expected %s, got %s" % (expected_unused, actual_unused))

    names = [m.name for m in report.modules]
    expect(names == ["FakeKernel", "OtherKernel"], "modules discovered in order without duplicates: %s" % names)
    expect(all(not f.endswith("Module.cpp") for f in report.files), "*Module.cpp files are never scanned")
    expect("Source/OtherKernel/Private/Notes.txt" not in report.files
           and "Source/OtherKernel/Public/Vaelen/Other/Ignored.cpp" not in report.files,
           "only kernel source extensions are scanned")
    expect("Source/FakeKernel/Public/Vaelen/Fake/Sub/Deep.h" in report.files, "Public is scanned recursively")
    expect(report.files == sorted(report.files), "scanned files are sorted")
    every_rule = {v.rule for v in report.violations}
    expect(every_rule == set(RULE_NAMES), "every rule fires at least once: %s" % sorted(every_rule))
    reasons = {reason for _, reason in report.exempted}
    expect("printf boundary" in reasons and "two rules at once" in reasons, "exemption reasons are captured")

    # -- Command line: exit codes and the summary line.
    def run_main(argv: List[str]) -> Tuple[int, str]:
      buffer = io.StringIO()
      with redirect_stdout(buffer):
        code_value = main(argv)
      return code_value, buffer.getvalue()

    code_value, output = run_main(["--root", str(root)])
    expect(code_value == 1, "exit code 1 with violations")
    expect(output.rstrip().splitlines()[-1] == "[purity] %d files, %d violations" % (len(report.files), len(expected)),
           "summary line counts files and violations")
    expect(all(v.format() in output for v in report.violations), "every violation is printed")

    code_value, output = run_main(["--root", str(root), "--modules", str(root / "Tools" / "clean_modules.txt")])
    expect(code_value == 0, "exit code 0 on a clean module")
    expect(output.strip() == "[purity] 2 files, 0 violations", "clean summary line: %r" % output.strip())

    code_value, output = run_main(["--root", str(root), "--modules", str(root / "Tools" / "clean_modules.txt"),
                                   "--list"])
    expect(code_value == 0 and output.split() == ["Source/OtherKernel/Private/Thing.cpp",
                                                  "Source/OtherKernel/Public/Vaelen/Other/Thing.h"],
           "--list prints the scanned files: %r" % output)

    code_value, output = run_main(["--root", str(root), "--verbose"])
    expect(code_value == 1 and "[purity] R7 fixed-width:" in output and "(exempted: printf boundary)" in output
           and "note: unused PURITY-ALLOW(R2)" in output and "[purity] module FakeKernel:" in output,
           "--verbose prints per-rule counts, exemptions and notes")

    stderr = io.StringIO()
    old_stderr = sys.stderr
    sys.stderr = stderr
    try:
      code_value, _ = run_main(["--root", str(root), "--modules", str(root / "Tools" / "missing_modules.txt")])
      expect(code_value == 2 and "NoSuchModule" in stderr.getvalue(),
             "missing module directory is a configuration error")
      code_value, _ = run_main(["--root", str(root / "does-not-exist")])
      expect(code_value == 2 and "does-not-exist" in stderr.getvalue(), "missing root is a configuration error")
      code_value, _ = run_main(["--root", str(root), "--modules", str(root / "Tools" / "empty_modules.txt")])
      expect(code_value == 2 and "no scannable source files" in stderr.getvalue(),
             "a module without source files is a configuration error, not a vacuous pass")
      code_value, _ = run_main(["--root", str(root), "--modules", str(root / "Tools" / "traversal_modules.txt")])
      expect(code_value == 2 and "not a plain identifier" in stderr.getvalue(),
             "a module name with path components is rejected")
    finally:
      sys.stderr = old_stderr

    # A BOM + CRLF module list resolves to the clean module.
    code_value, output = run_main(["--root", str(root), "--modules", str(root / "Tools" / "bom_modules.txt")])
    expect(code_value == 0 and output.strip() == "[purity] 2 files, 0 violations",
           "BOM + CRLF module list is read correctly: %r" % output.strip())

    # Symlinked files are rejected (never scanned under a fake in-repo path).
    link_dir = root / "Source" / "LinkKernel" / "Private"
    link_dir.mkdir(parents=True)
    try:
      (link_dir / "Evil.h").symlink_to(root / "Tools" / "kernel_modules.txt")
      (root / "Tools" / "link_modules.txt").write_text("LinkKernel\n", encoding="utf-8")
      stderr = io.StringIO()
      sys.stderr = stderr
      try:
        code_value, _ = run_main(["--root", str(root), "--modules", str(root / "Tools" / "link_modules.txt")])
      finally:
        sys.stderr = old_stderr
      expect(code_value == 2 and "symlink" in stderr.getvalue(), "a symlinked file is a configuration error")
    except OSError:
      pass  # symlinks unavailable on this filesystem: not tested here
    finally:
      sys.stderr = old_stderr

  for failure in failures:
    print("[purity] self-test FAILED: %s" % failure)
  print("[purity] self-test: %d checks, %d failed" % (checks[0], len(failures)))
  return 0 if not failures else 1


if __name__ == "__main__":
  sys.exit(main())
