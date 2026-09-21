#!/usr/bin/env bash
# VAELEN - every fast check the CI runs, in one command, to be run BEFORE a push.
#
# Exists because the same failure happened twice in three days: a source file
# pushed without the format check, the clang-format job red, a round trip. Both
# times the check took four seconds and was skipped because it lived in a
# person's memory. This file is where it lives now.
#
# Fast means seconds. The test suite is not here - it is ten minutes, and
# `ctest --preset linux-gcc-release` is the command for it. What IS here is
# everything the CI can fail on before the hour-long legs even start:
#
#   clang-format   Source/ and Tests/ (not Source/Vaelen/, per the CI's own glob)
#   purity         no engine header inside a kernel module
#   shim self-test 7 mutations the engine-module parse must catch
#   parse          the engine modules, against Tools/EngineShim
#   wiring         the four wirings of AELVOR agree
#   ui fence       the UI includes no kernel header and names no world
#   compiles       every TU the change can reach, syntax-only, the build's flags
#
# THE LAST ONE WAS ADDED ON 2026-09-21, and it is the reason this comment is
# being rewritten rather than appended to. Everything above it passed green on
# two consecutive commits that could not be COMPILED - a value passed where
# VT_CHECK_MSG wanted a format literal, which -Wformat-security rejects and
# -Werror turns fatal. Eight of ten CI legs died at the Build step and not one
# test ran. One of the two sat broken on the branch for two commits.
#
# So the list above had a shape to it that nobody had noticed: every check in
# it reads the source as TEXT. None of them had ever handed a file to a
# compiler. A whole class of failure - the largest one the CI actually catches
# - had no fast check at all, and the gap was invisible precisely because the
# checks that did exist were green.
#
# It needs a configured build directory and REFUSES if it cannot find one,
# rather than passing quietly. A check that skips in silence is worse than no
# check, because a green run is then read as an answer it never gave.
#
# Any failure stops the script and exits non-zero. No "warnings".
set -euo pipefail
cd "$(dirname "$0")/.."

CLANG_FORMAT="${CLANG_FORMAT:-clang-format-18}"
if ! command -v "$CLANG_FORMAT" >/dev/null 2>&1; then
	echo "[verify] $CLANG_FORMAT not found - set CLANG_FORMAT to the binary the CI uses (18)" >&2
	exit 2
fi

echo "[verify] clang-format"
# --others --exclude-standard IS NOT DECORATION. This used `git ls-files` alone
# until 2026-09-21, which lists only TRACKED files - so a file that had just
# been written and not yet staged was invisible to the one check most likely to
# have something to say about it. 16.04 added three new files, verify_fast went
# green on all six checks, and the clang-format leg of the CI was red on all
# three of them. A new file is exactly when this check is worth running.
{
	git ls-files 'Source/*.h' 'Source/*.cpp' 'Tests/*.h' 'Tests/*.cpp'
	git ls-files --others --exclude-standard 'Source/*.h' 'Source/*.cpp' 'Tests/*.h' 'Tests/*.cpp'
} | sort -u | grep -v 'Source/Vaelen/' \
	| xargs "$CLANG_FORMAT" --style=file --dry-run -Werror

echo "[verify] purity"
python3 Tools/check_kernel_purity.py | tail -1

echo "[verify] shim self-test"
python3 Tools/test_engine_shim.py | tail -1

echo "[verify] parse"
python3 Tools/parse_engine_modules.py | tail -2 | head -1

echo "[verify] wiring"
python3 Tools/check_world_wiring.py | tail -1

echo "[verify] ui fence"
python3 Tools/check_ui_fence.py | tail -2 | head -1

echo "[verify] compiles"
# NOT piped through tail, unlike every check above it. The others answer with a
# count; this one answers with a compiler's diagnostic, and the diagnostic IS
# the useful part. Set VAELEN_BUILD_DIR if your build directory is not ./build.
python3 Tools/check_changed_compiles.py

echo "[verify] all fast checks pass - the suite (ctest) and the CI legs are still the authority"
