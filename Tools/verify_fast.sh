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
#   wiring         the three wirings of AELVOR agree
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
git ls-files 'Source/*.h' 'Source/*.cpp' 'Tests/*.h' 'Tests/*.cpp' | grep -v 'Source/Vaelen/' \
	| xargs "$CLANG_FORMAT" --style=file --dry-run -Werror

echo "[verify] purity"
python3 Tools/check_kernel_purity.py | tail -1

echo "[verify] shim self-test"
python3 Tools/test_engine_shim.py | tail -1

echo "[verify] parse"
python3 Tools/parse_engine_modules.py | tail -2 | head -1

echo "[verify] wiring"
python3 Tools/check_world_wiring.py | tail -1

echo "[verify] all fast checks pass - the suite (ctest) and the CI legs are still the authority"
