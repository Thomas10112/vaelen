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
set -u
PRESET="${1:-linux-clang-debug}"
cd "$(dirname "$0")/.."
cmake --build --preset "$PRESET" -j"$(nproc)" 2>&1 | grep -E "error|warning" | head -5
FAILED=0
# Colony.ColonyGate and Gameplay.GameplayGate were missing from this list for
# the whole of Phases 11 and 12 - the two slowest gates in the project, and the
# two that cover the newest code. Last, because the point of the order is that a
# moved digest is known before the long ones run.
for G in Sim.HistoryGate Population.PopulationGate Society.SocietyGate Economy.EconomyGate \
         Politics.PoliticsGate Military.MilitaryGate Infrastructure.InfrastructureGate Player.PlayerGate \
         Colony.ColonyGate Gameplay.GameplayGate; do
  OUT=$(ctest --preset "$PRESET" -R "^${G}$" 2>&1 | grep -E "tests passed|tests failed" | head -1)
  echo "${G} | ${OUT}"
  case "$OUT" in *"100% tests passed"*) ;; *) FAILED=$((FAILED + 1)) ;; esac
done
echo "GATES-DONE ${FAILED} failing"
exit $((FAILED > 0))
