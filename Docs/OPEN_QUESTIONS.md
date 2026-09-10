# Three things AELVOR did that nobody had decided were right

**All three were settled on 2026-09-10.** Two are applied, one is decided and
deliberately unchanged. This page is kept as the record of what was chosen and
why, and it has one new question at the bottom that applying the first turned up.

| | what happened | where |
|---|---|---|
| **ADR-0120** twin roads | **fixed**, and it was three fixes, not one | applied |
| **ADR-0118** towns in empty regions | **decided: the rule is intended.** No code change | decided |
| **ADR-0111** nothing is ever eaten | **open** — see below | open |
| **ADR-0129** a road cannot be abandoned | **new**, found by fixing 0120 | open |

---

## 1. Twin roads — ADR-0120 — FIXED

### What was wrong

AELVOR held **317 route entities for 186 pairs of regions**. `TradeSystem::Tick`
sorted routes into open and closed once at the top of a tick, so a road that
went idle *during* that tick was in neither list the reuse pass searched, and a
second entity was built for a pair that already had one. 131 pairs carried two.

The chronicle said a road opened for the first time 317 times, and **131 of
those sentences were false** — the same road told it was born, twice, centuries
apart.

### What fixing it turned up, which was worse

**Three quarters of this world's road openings never happened.** Once the reuse
pass could find the road closed this tick, it turned out step 1 closed roads and
step 2 reopened them in the same tick, constantly, each one counting an opening
and publishing an event. The closing is now held back until step 2 has had its
say and published only for roads still shut. `Openings` summed over 184 roads
across 420 years: **2697 → 696**. That was a simulation field wrong by a factor
of four, not a reporting problem.

**And every road closing this world ever recorded was decided on a different
road's traffic.** 06.07 asked whether a closing mattered via
`RouteBetween(From, To)`, which returns only routes whose `Closed` is 0 — so it
could never return the road that had just shut. With twins present it returned
the pair's other route and read *its* `Carried`. All 686 recorded closings were
judged on a road that had not closed. The chronicle now looks a road up by its
index, which is the ADR's own title: a pair of regions does not identify a road.

### The result

| | before | after |
|---|---|---|
| route entities | 317 | **184** |
| pairs carrying twins | 131 | **0** |
| chronicled first openings | 317, **131 false** | **184, none false** |
| chronicled closings | 686, **each read off another road** | **309, each its own** |
| sum of `Openings` | 2697 | **696** |
| living at 256 | 206710 | 206710 |

**26 of 155 tests failed and every one was a frozen constant.** No behavioural
assertion broke. 44 constants re-recorded across 8 gates in one deliberate pass.

`Test_PlayerGate.cpp`'s `Lives > 1` was changed to `Lives >= 1`. It asserted
that the world's mortality ended the played life, and the paragraph directly
above it declines to require that, in as many words. It held because of the
seed, not the design.

---

## 2. Towns where nobody lives — ADR-0118 — DECIDED, NOT CHANGED

**Traffic alone founds and keeps a settlement, and that is intended.**

Six settlements sit in regions holding zero people, and five of the six have
their abandonment counter reset every year by traffic passing through. A
waystation on a road with nobody living in its region is a real thing, and the
world is entitled to have them.

Three reasons this is a decision and not a shrug:

1. **It is self-limiting, and that was measured.** At year 420 the count was
   still climbing. Run to 1500 and **zero towns stand in empty regions.**
2. **The alternative costs more than the defect.** Settlements drive traffic,
   traffic drives roads, roads drive everything above Phase 06. Re-recording
   eight gates to stop something the world already stops is a poor trade.
3. **What was actually wrong is that nobody had chosen.** Thirteen phases never
   asked whether a town could stand in an empty region, because no test knew it
   was a question. It is a question now, and it has an answer.

---

## 3. The world cannot say anything was eaten — ADR-0111 — STILL OPEN

One ordinary year, busiest region of a 128 world, 1460 people:

```
grain   stores end +6;   the log names +7329;  7323 units moved unnamed
```

A region harvested 7329 units of grain and its stores rose by six. The log
records the harvest and records nothing about where the rest went. In 8595
sentences about AELVOR, **not one says anything was eaten.**

`ProductionSystem::Tick` writes `RegionStock::Amount` and `HouseStock::Amount`
directly in about twenty places and publishes exactly two events. `AddStock`
already exists, already publishes `StockAdded`/`StockTaken` with a cause, and
already has the right signature. Routing the writes through it is mechanical.

**Why it is still open: the granularity is the decision, and it is not small.**
One event per good per region per year at 256 is roughly a thousand a year —
420 000 over a run, against the ADR's original estimate of four hundred a year.
Per house it is far larger. That is a change to how much the event log holds,
which is a question about the shape of the project, and it wants its own
measurement before it is taken rather than during.

**Recommendation: do it, as its own deliberate pass, with a measured answer to
the volume question first.**

---

## 4. NEW — A road cannot be abandoned, only shut — ADR-0129

Settlements carry `Abandoned` and end once, for good. Routes carry only
`Closed`, a state a road leaves the moment trade wants it again. So the world
says 184 roads were built and says 309 times that a road stopped, across 94 of
them, one of them twelve times — and none of those 309 is a road *ending*.

Nothing it says is false. It is less than a reader would want, and fixing it
means giving routes what settlements have, which is a design change with a rule
hiding in it: how long shut is dead? I lean to mirroring the settlements. It has
a recommendation and no measurement, so it waits rather than being decided on a
hunch.

---

The full reasoning and the raw numbers are in `Docs/DECISIONS.md` under
ADR-0111, ADR-0118, ADR-0120 and ADR-0129.
