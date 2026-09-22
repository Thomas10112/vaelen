# The stream a replay is held to

A `.stream` here is the text form of `Vaelen::Player::InputStream` (14.01): one
header line naming the world, then one record per line in time order — takings,
commands, day turns. `Tools/Atlas --replay <file>` builds a fresh world from
that header and puts the records back through `Run::Door`, and the four digests
it comes to are what CTest `Replay.Played` pins.

## `aelvor256-2026-09-16.stream` — a month somebody played

Eighty-three days at the keyboard, in UE 5.6 on 2026-09-16, written out with
`Vaelen.Stream.Write`. Not generated. Somebody pressed W, R, E, T, S, G, K and M
ninety-nine times and turned the day eighty-three times, and this is what came
through the door.

What the engine printed that day:

```
LogVaelenPlay: AELVOR 256 seed 41454c564f52: played Dukem (person 15019, region 37) 83 days, 99 intents (76 taken, 23 refused by the world, 0 dropped at the door), state 2ffed5236a593c1b, log 1fdd4211695649bc, life 654e2de6455d8b7a, panel 7de2c5faf3cc1813
LogVaelenPlay: verbs work 34 rest 1 eat 2 wait 2 speak 5 give 2 take 26 move 27
```

`Tools/Atlas --replay … --panel --want-bound 0` prints those two lines back,
byte for byte — same MD5 — from a world rebuilt out of the header alone. That
is the claim of ADR-0136 met in the one way that counts: a played life is a
function of what came through the door, and of nothing else the engine did.

The `panel` digest, `7de2c5faf3cc1813`, is also the last line of the screenshot
taken that day. The log and the pixels agree, which is what putting the page's
digest inside the page was for.

The host configuration it was recorded under, which a replay must be given
because the rules are deliberately not in the stream (`Run/Door.h`):

| | |
|---|---|
| size / pre-history / years | 256 / 300 / 100 |
| seed | `0x41454c564f52` |
| `StartRules::WantBound` | **0** — whoever the world offers, as `VaelenWorldSubsystem` does |
| colony, lively | neither: the host asks for `Play` and nothing else |

The header line carries the first three and the seed, so the world it belongs to
is readable without tooling. `WantBound` is the one thing that is not in the
file, which is why `Replay.Played` passes `--want-bound 0`.

## The stand-in that used to be here

`aelvor256-stand-in-2026-09-15.stream` was written by `VaelenAtlas --stand`, not
by anybody, and existed for one day so that the replay, the two lines, the four
pinned digests and the CTest entry were exercised and green before the real
month could arrive. It did its job — it is how the Move defect of ADR-0139 was
found, because `--stand` refused to write a file without one — and it is gone.
`--stand` stays: the next time this machinery needs testing before a human is
available, it is there.

## What makes this stream go stale

The digests are of a whole simulated world, so anything that changes what the
world does moves them: the wiring order in `Run/Aelvor.cpp` ("a type declared in
a different position is a different world"), any system's arithmetic, the
pre-history rules, worldgen, the panel's text layout, `ExportLife`. That is the
same class as `Atlas.Frozen128` and the fix is the same: re-freeze deliberately,
with an ADR, never by quietly editing a number until the test goes green.

A change to the stream FORMAT is different and fails loudly: `DecodeStream`
refuses a version it does not know, so the file stops loading rather than
loading wrong.

## `aelvor128-walk-2026-09-17.stream` — a walk nobody walked

Written by `VaelenAtlas --walk`, not by anybody. Phase 15 task 15.10's headless
half: a hundred days at AELVOR 128 with `Options::Stream` on — the daily detail
cadence of ADR-0141 and the warden of ADR-0144 — four takings, and a `Looked`
record for every day. It is the shape the engine half will record for real,
exercised and green before a human is available, exactly as
`aelvor256-stand-in-2026-09-15.stream` was for 14.10.

```
walk: 100 looks, 4 takings, 100 days, worst day turn 1 promotion(s),
longest wait for somewhere to walk 1 day(s), audit clean
```

`Tools/Atlas --replay … --stream --panel --want-bound 0` replays it to
`state 18e6984252055df0, log e32e170ba6dc052b, life 414d2eaa749ed6dd,
panel fe2920e707995254`, with 0 wrong and 100 of 100 days. CTest
`Replay.Walked` pins that.

**Those six numbers were wrong here until 2026-09-18**, and the file was not.
They were the figures of the FIRST walk `--walk` wrote; the Phase 15 review then
found that the replay dropped `Attention::Most` and applied a same-tick taking
before the look it followed, both were fixed, `--walk` was made to replay its own
walk before writing a byte, and the walk it then wrote is this one. The CTest
was updated with it and this page was not. Nothing was broken by the drift -
`Replay.Walked` reads the CTest and not this page - but a page that documents a
file by numbers the file does not have is worse than a page that documents
nothing, so it is said here rather than quietly corrected.

**`--stream` is not optional and is not in the file.** The cadence a world
decides its detail on is the host's configuration, like `StartRules::WantBound`:
replay this walk without it and you are replaying it into a world that pays
attention on a different schedule, and the four digests say so.

**What the guard refused, twice, before it would write this.** `--walk` will not
write a walk that misses the gate's clauses, and on its first two runs it did
not:

1. It read `near:` at the instant of each taking and found it empty. Correctly:
   `NearDetail` asks for neighbours as REQUESTS and the bridge answers on its
   next daily pass, so at the instant somebody is taken up there is nowhere to
   walk yet.
2. Then it read `near:` a day later and still refused — because 15.08 caps the
   daily pass at ONE promotion, and a taking asks for three neighbours, so the
   third cannot be detailed before the third day whatever anybody wants.

**"`near:` is never empty" was not a clause any implementation could meet.** The
walk measures the wait instead: **two days**, at most, from a taking to
somewhere to walk. That is the same guarantee stated at a rate the world can
keep, and it is what the gate now asks for.

## Asking a walk the gate's questions: `--gate`

`--walk` checks the clauses on a walk it writes itself. `--gate` checks them on
a walk that arrives as a file — which is what the 15.10 gate is actually about,
because the walk it closes on is recorded in the editor on another machine.

```
VaelenAtlas --gate Tests/Run/Streams/aelvor128-walk-2026-09-17.stream --want-bound 0 \
  --expect "state 18e6984252055df0, log e32e170ba6dc052b, life 414d2eaa749ed6dd, panel fe2920e707995254"
```

```
gate: Tests/Run/Streams/aelvor128-walk-2026-09-17.stream, want-bound 0, daily cadence forced on (a walk carrying looks was recorded with it)
AELVOR 128 seed 41454c564f52: played person 4171 (person 4171, region 9) 100 days, 0 intents, state 18e6984252055df0, log e32e170ba6dc052b, life 414d2eaa749ed6dd, panel fe2920e707995254
  (a) PASS  the stream carries looks and the fence fired: 100 Looked records, 53 pins published while held
  (b) PASS  every record replayed as it was recorded: 0 wrong (0 of them takings), 0 records left unreached
  (B) PASS  the four digests are the ones the host printed: state 18e6984252055df0, log e32e170ba6dc052b, life 414d2eaa749ed6dd, panel fe2920e707995254
  (c) PASS  both grains agree on every day of the walk: worst over 100 days: heads 0, slots 0, faiths 0
  (d) PASS  no day turn promoted twice: worst day turn promoted 1
  (e) PASS  somewhere to walk within four days of each taking: 4 takings, longest wait 1 day turn(s)
  (f) the frozen constants of Phases 00-14 are the CI suite's business, not this command's
gate: every clause this command can ask is kept
```

CTest `Run.Gate` matches those six clause lines WHOLE. Whole, and not "does it
say PASS", and not a substring either: a clause that stopped being asked would
still print a verdict, and a clause whose figures drifted would still match a
prefix. The numbers ARE the measurement.

Five notes on reading it.

**Clause (b) is two halves and only one of them is free.** Every record replaying
as recorded, this command measures. The four digests it cannot: it has no way to
know what the engine printed. So `--expect` is how you hand them over, and
without it the command prints `(B) NOT JUDGED` rather than a pass over a
property nothing measured. That failure mode is the reason this command exists.

**`--want-bound 0` is not optional.** The engine host takes whoever the world
offers (`VaelenWorldSubsystem.cpp` sets 0) and the rules are not in the stream,
for the same reason the cadence is not: they are the host's configuration, and a
replay is told them. At `--want-bound 1` this same walk replays to a different
person and reports four wrong takings, which is not a defect in the walk.

**The streaming cadence is forced on and not asked for.** A walk carrying
`Looked` records is a walk recorded with it, so `--gate` does not offer the
choice — unlike `--replay`, where `--stream` must be passed by hand. The first
line says so out loud, because neither the cadence nor the want-bound is in the
file and both change what the walk replays to. **This remains a real gap in the
format**: a version-1 stream cannot say which cadence wrote it, so a walk handed
to the wrong one fails every clause for a reason that has nothing to do with the
walk. `Vaelen.Stream.Write`'s third line is where a host records it for a human.

**A walk that measures nothing is refused.** No day turns at all — every clause
below would otherwise be kept by an empty file, because nothing disagreed,
nothing promoted twice and nobody waited. And a walk whose last taking was still
waiting for somewhere to walk when the records ran out: it sits on a wait of 0
that it was never observed long enough to earn.

**`longest wait` is in DAY TURNS since the taking, and both instruments now say
1.** They did not always. `--gate` reported 0 until 2026-09-18, because a walk's
first taking is applied before the first day turn and the watcher only saw the
ends of days — so it read that taking's wait a whole turn late and called it
zero. `Run::DayWatch::Begun` is the hook that closes it. The disagreement between
the two instruments is what found it, which is the second time on this task that
a second instrument was worth more than a careful reading.

## `aelvor128-played-2026-09-21.stream` — the walk somebody lived

The other two streams in this folder were written by this project, to itself.
This one came out of Unreal Engine 5.6 on Win64 (MSVC 19.51), through a camera
and a console, from a host that had never run headlessly — and it is the only
file here that proves the ENGINE half of 15.10 rather than the kernel's.

142 day turns, 138 looks, 4 takings, 2 Speak intents, with the daily cadence on.

```
VaelenAtlas --gate Tests/Run/Streams/aelvor128-played-2026-09-21.stream --want-bound 0 \
  --expect "state 609253a29361ec5f, log a2839792e0837328, life 0a4babc60f6e4d90, panel c4ddbe971538c59c"
```

```
  (a) PASS  the stream carries looks and the fence fired: 138 Looked records, 15 pins published while held
  (b) PASS  every record replayed as it was recorded: 0 wrong (0 of them takings), 0 records left unreached
  (B) PASS  the four digests are the ones the host printed: state 609253a29361ec5f, ...
  (c) PASS  both grains agree on every day of the walk: worst over 142 days: heads 0, slots 0, faiths 0
  (d) PASS  no day turn promoted twice: worst day turn promoted 1
  (e) PASS  somewhere to walk within four days of each taking: 4 takings, longest wait 1 day turn(s)
```

CTest `Replay.Lived` pins what it replays to and `Run.Gate.Lived` pins that it
still meets the gate.

**138 looks over 142 days, and the four missing ones are the point.** A look is
skipped on any tick that already carries a taking, because `Door::Day` records a
taking at the tick AFTER the turn — the tick the host's next look would land on
— and a replay applies looks before takings at equal ticks. One skip per taking,
four takings, four skips. The engine does this on its own, and the arithmetic
coming out right in a file recorded on another machine is the best evidence
there is that the guard works.

**What a stand-in could not have invented**, and what makes this file worth
keeping beside the tidy one:

- two `Speak` intents pressed by accident while reaching for W-A-S-D;
- a stretch where the camera looked at **region 0** — off the map, over the sea
  — before the owner found their bearings;
- twenty-five day turns where the camera did not move at all;
- and, before the last thirty days, seventy-five where it sat on one region
  while the played person lived forty-five metres away. That is what made the
  gate refuse the first version of this walk, and the story is in ADR-0140's
  amendment: **the fence fires only when the camera crosses the played person's
  own region and leaves it.**

The last thirty day turns were driven from the console rather than the keyboard:
`Vaelen.Look 9`, `Vaelen.Day 5`, `Vaelen.Look 54`, `Vaelen.Day 5`, and so on.
`Vaelen.Day` does not consult the camera — it uses the remembered look — so a
whole walk can be recorded at exact region numbers without flying at all. Only
the Space key routes through the controller and reads the camera.

---

## `aelvor64-death-2026-09-22.stream` — the one where the played person dies

Written by `Tools/Atlas --deathwalk` on 2026-09-22 for Phase 16 task 16.12(c).
AELVOR 64 with sixty years of pre-history and thirty more lived, a start window
from age 85, twelve hundred recorded day turns: **person 300 is taken up, dies
on day 1080, and person 100 is taken up after them.** Two takings, one death,
28 866 bytes.

**This file cannot tell you how to replay it**, and that is the point of saying
so here. `Player::StreamHeader` carries a seed, a size, a pre-history, a year
count and a version — and no wiring bits at all. Replay it with the defaults
and you get a different world and a different person:

```
Tools/Atlas --savefuzz Tests/Run/Streams/aelvor64-death-2026-09-22.stream \
    --points 3 --seed 63 --stream --lively --from-age 85 --to-age 120 --want-bound 0
```

Drop `--lively` and the world is not the one this was recorded in. Drop
`--from-age 85` and `TakeUp` hands you a twenty-year-old who does not die
inside the horizon, so every taking after the first is somebody else's. A save
carries its `Options` in a HOST section (16.10); a `.stream` file has nowhere
to put them. That asymmetry is why `Run.SaveFuzz.Death` spells all four flags
out rather than relying on a default.

**Why it was written at all.** A death is the single day that exercises
`Release`, a fresh `TakeUp`, `NearDetail`'s give-back path and a recorded
`TakenUp` at once. At seed 63 the save points land on days 537, 891 and
**1081 — the day after the death** — and all three re-save byte-identically,
re-adopt to the same digest, refuse a second adopt, and lead the rest of the
walk to `state 18aec14e39a68a8c`.

**Why `--walk` could not write it.** That mode enforces the Phase 15 gate's
clauses on what it writes — somewhere to walk within four days of each taking,
no day turn promoting twice — and a horizon long enough for somebody to die of
old age is not shaped like that. `--deathwalk` enforces its own clause instead,
and refuses to write a file whose name would be a lie: no death in the horizon,
or a death with nobody taken up afterwards, and nothing is written at all. Both
refusals were exercised on purpose before this file was kept.
