#!/usr/bin/env bash
# Run every phase gate, fastest first.
#
# WHEN TO USE THIS. A task inside one module is covered by building and running
# that module's suites. A change that touches a module BELOW the one being built
# is not, and this is the check for it.
#
# WHY IT EXISTS. Phase 10 added a component type inside 04.06's Declare, which
# put it into the type registry of every world that declares that module - every
# gate from Phase 04 up - and a state digest counts type ids. Six closed phases'
# frozen digests moved at once. Ten commits of "both presets green" were true and
# blind to it, because the local loop excluded the gates and every affected test
# was one. The CI matrix found it an hour later; this finds it in a minute.
#
# Fastest first, so a moved digest is known before the twenty-minute gates run.
#
# THE CONTROL, because a gate list that cannot go red is not a gate list.
# `run_gates.sh <preset> --self-test` appends a gate name that does not exist
# and requires the count to come back non-zero. It proves the COUNTING arm; the
# other arm - that a gate refuses a figure that moved - is proved by running
# Tests/Run/Gate.cmake by hand with one digit changed, which exits 1. Both were
# run on 2026-09-21 when 16.01 added the last six entries.
set -u
PRESET="${1:-linux-clang-debug}"
SELFTEST="${2:-}"
cd "$(dirname "$0")/.."
cmake --build --preset "$PRESET" -j"$(nproc)" 2>&1 | grep -E "error|warning" | head -5
FAILED=0
TOTAL=0
# Colony.ColonyGate and Gameplay.GameplayGate were missing from this list for
# the whole of Phases 11 and 12 - the two slowest gates in the project, and the
# two that cover the newest code. Last, because the point of the order is that a
# moved digest is known before the long ones run.
#
# Phase 16 task 16.01 added the last five. Eleven of these check a digest a
# system computes; the five below check a digest a RECORDING replays to, and
# Phase 16 is about to change the code that reads recordings. A gate list that
# covered only the first kind would have gone green through all of it.
# Run.Gate.Lived and Replay.Lived carry the walk a person played in Unreal on
# 2026-09-21 - the only entries here that came off another machine.
for G in Sim.HistoryGate Population.PopulationGate Society.SocietyGate Economy.EconomyGate \
         Politics.PoliticsGate Military.MilitaryGate Infrastructure.InfrastructureGate Player.PlayerGate \
         Run.Golden Replay.Played Replay.Walked Replay.Lived Run.Gate Run.Gate.Lived \
         Colony.ColonyGate Gameplay.GameplayGate View.ViewGate \
         ${SELFTEST:+Nonexistent.GateThatMustFail}; do
  OUT=$(ctest --preset "$PRESET" -R "^${G}$" 2>&1 | grep -E "tests passed|tests failed" | head -1)
  echo "${G} | ${OUT}"
  TOTAL=$((TOTAL + 1))
  case "$OUT" in *"100% tests passed"*) ;; *) FAILED=$((FAILED + 1)) ;; esac
done
echo "GATES-DONE ${FAILED} failing (of ${TOTAL})"
if [ -n "$SELFTEST" ]; then
  # Inverted on purpose: with the impossible gate appended, ZERO failures means
  # the loop cannot report one, and that is the thing being tested.
  if [ "$FAILED" -eq 0 ]; then
    echo "SELF-TEST FAILED: the list reported nothing wrong with an impossible gate in it"
    exit 1
  fi
  echo "SELF-TEST OK: the list can go red"
  exit 0
fi
exit $((FAILED > 0))
