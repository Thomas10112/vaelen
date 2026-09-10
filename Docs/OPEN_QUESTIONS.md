# Three things AELVOR does that nobody has decided are right

Each of these is a defect I found, measured, wrote up as an ADR — and did not
fix, because fixing it changes what the world *is*, and that is not a call an
engineer takes alone at the bottom of a task list.

They have been sitting in `Docs/DECISIONS.md` as ADR-0111, ADR-0118 and ADR-0120,
each a few thousand words down a file of a hundred and twenty-eight entries.
That is the right place for the reasoning and the wrong place for the choice.
This page is the choice: what the world does, what it costs to leave it, what it
costs to change it, and the one sentence from you that unblocks each.

**Nothing here has been applied.** The measurements below were taken by putting
a change into the working tree, running the suite, writing the world out, and
reverting to the byte.

---

## 1. A pair of regions does not identify a road — ADR-0120

**MEASURED.** This is the only one of the three whose cost is counted rather
than estimated, because its fix is mechanically determined: find the road that
is already there instead of building a second one.

### What the world does now

At 256 over 420 years, AELVOR holds **317 route entities for 186 pairs of
regions**. 131 pairs carry two. The chronicle says a road opened for the first
time 317 times, and **131 of those sentences are false** — the same road, told
it was born, twice, centuries apart, with nothing said in between about it ever
having closed.

```
Year 0:   The road from Miogu to Yiotur was opened.
Year 385: The road from Miogu to Yiotur was opened.
```

### Why

`TradeSystem::Tick` sorts routes into `Open` and `Closed` once, at the top of
the tick. A road that goes idle *during* that tick has its copy marked closed
in place — inside `Open`, which is where it stays. The reuse pass then searches
`Closed` only, does not find it, and builds a new entity for a pair that already
has one.

It was invisible for eleven phases because the only check that existed counts
pairs with two **open** roads, and every twin is one open and one closed.

### What it costs to leave it

Thirty-eight per cent of this world's road history is not missing but **untrue**,
and it is the one class of defect that gets worse with time rather than better:
`Openings` restarts on each twin, so a road opened five times reads as five
roads, and `Identity` — the hash meant to name a road — names two.

### What it costs to change it

Nineteen lines in `Trade.cpp`, and then:

| | before | after |
|---|---|---|
| route entities | 317 | **184** |
| pairs carrying twins | 131 | **0** |
| false first-openings | **131** | **0** |
| living (256) | 206710 | 206710 |
| living (128) | 45535 | **45544** |

**26 of 155 tests fail.** Twenty-four are frozen constants across eight gates —
mechanical to re-freeze, though each must be justified rather than pasted. Two
are not:

- `Test_EconomyHistory.cpp:494 VT_CHECK(S.Records < Harvests / 4)` — the fix is
  **necessary and not sufficient.** With twins, every reopening was a new entity
  and so read as a first opening: false, but symmetric with the closings. Remove
  the twins and the symmetry goes — a road is then said to open **once** and to
  fall out of use **1395 times**. Which raises a question 06.07 has never been
  asked: *is a road reopening history?*
- `Test_PlayerGate.cpp:1574 VT_CHECK(Lives > 1)` — the gate asserts *"the world's
  mortality really did end a life and start another."* With the fix it does not:
  one played life spans the whole forty years. Nothing about mortality changed;
  the roads changed, so the food changed, so the bound person lived.

The gates that move are ECONOMY, POLITICS, MILITARY, INFRASTRUCTURE, COLONY,
PLAYER, GAMEPLAY and VIEW. The ones that do not are HISTORY, POPULATION and
SOCIETY — every phase below 06. The blast radius is exactly the layering.

### What I recommend

**Fix it, and treat 06.07 as part of the same job.** This is the only one of the
three where leaving it is actively corrosive: the record of the world is wrong
today and gets wronger. The re-freeze is an afternoon, and it is an afternoon
that also answers *what should the world say about a road that reopens* — which
is a better question than the project has been able to ask so far.

### The sentence I need

> *"Fix ADR-0120, re-freeze the eight gates, and record a road's reopening as
> history / and leave reopenings unrecorded."* — and, for `PlayerGate`, whether
> *"a played life ends"* is an invariant of the design or an accident of the seed.

---

## 2. The world cannot say that anything was eaten — ADR-0111

**ESTIMATED, not measured.** The fix routes 06.02's consumption through
`Economy::AddStock`, and *how* to route it — one event per good per region per
year, or per house, or per meal — is itself the decision. Measuring it would
mean choosing it. So the numbers below are the size of the hole, not the cost of
filling it.

### What the world does now

One ordinary year, busiest region of a 128 world, 1460 people:

```
grain   stores end +6;   the log names +7329;  7323 units moved unnamed
cloth   stores end +15;  the log names +0;       15 units moved unnamed
tools   stores end +8;   the log names +0;        8 units moved unnamed
```

**A region harvested 7329 units of grain and its stores rose by six.** The log
records the harvest and records nothing whatever about where the rest went.

Read from the chronicle instead of the ledger, it is starker: in 8595 sentences
about AELVOR, **not one says anything was eaten.**

### Why

`ProductionSystem::Tick` writes `RegionStock::Amount` and `HouseStock::Amount`
directly in a dozen places and publishes exactly two events — `Harvest` and
`Shortfall`. Spoilage, meals, timber and salt burnt, cloth and tools made and
worn out: all of it moves by direct assignment. `Shortfall` reports what could
**not** be fed, which is not the same as what was eaten.

### What it costs to leave it

`History::CauseChain` walks the event log. Anything the log does not hold, the
world cannot explain. Today AELVOR can tell you a drought caused a poor harvest
and a poor harvest caused a hunger — and it can never tell you that a person ate.
Every future question of the form *where did it go* is unanswerable by
construction.

### What it costs to change it

Roughly four hundred more events a year at 256 — cheap in itself. But it moves
the event-log digest, frozen in **eleven** gates, and ADR-0095 exists precisely
because six phases' frozen digests once moved at once without anybody noticing.
Doing that on purpose is fine. Doing it unannounced is how the discipline dies.

### What I recommend

**Do it, but not next.** It is the largest of the three and the least urgent:
nothing it touches is *wrong*, only silent. It is best done as a deliberate
single pass with ADR-0120's re-freeze, not before it — one gate-wide re-record,
not two.

### The sentence I need

> *"Route 06.02's consumption through `AddStock`, at the granularity of ___, and
> re-freeze all eleven gates in one pass."*

---

## 3. Towns stand where nobody lives — ADR-0118

**ESTIMATED.** The fix is a rule change, and *which* rule is the decision.

### What the world does now

Six settlements sit in regions holding zero people. Five of the six have their
abandonment counter reset **every year**, because traffic is credited to both
ends of any route that moved anything — so a road passing through keeps an empty
town alive indefinitely. Regions with nobody left hold thousands of units of
grain, timber, ore and salt.

### Why

06.04 abandons a settlement after `AbandonAfterQuietYears` without traffic, and
never asks whether anyone lives there. Founding does not ask either.

### What it costs to leave it

Less than it first appeared, and I got this wrong once and corrected it: at year
420 the count was still climbing and 55 of 126 regions looked like a world
filling up with ghost towns. Run it to 1500 and **zero towns stand in empty
regions.** The defect is real and it is **self-limiting** — the world eventually
closes them itself.

### What it costs to change it

Unmeasured, and certainly gate-wide: settlements drive traffic, traffic drives
roads, roads drive everything above 06.

### What I recommend

**Leave it, and write the reason down.** A rule that founds and keeps towns by
traffic without asking about people is defensible — a waystation on a road is a
real thing. What is not defensible is that nobody chose it. Turning ADR-0118
from *"a defect nobody decided about"* into *"a decision, taken, with the
1500-year evidence attached"* costs nothing and closes the question.

### The sentence I need

> *"Traffic alone founds and keeps a settlement; that is intended"* — or the
> rule you want instead.

---

## In one table

| | measured? | leaving it | changing it | recommend |
|---|---|---|---|---|
| **0120** twin roads | **yes** | the record is untrue and worsens | 26 tests, 8 gates, one afternoon | **fix, with 06.07** |
| **0111** nothing eaten | no | the world can never explain itself | 11 gates, one deliberate pass | do it, after 0120 |
| **0118** empty towns | no | self-limiting; gone by year 1500 | unmeasured, gate-wide | **decide it, don't change it** |

The full reasoning, the probes and the raw numbers are in `Docs/DECISIONS.md`
under ADR-0111, ADR-0118 and ADR-0120. This page exists so that reading them is
optional.
