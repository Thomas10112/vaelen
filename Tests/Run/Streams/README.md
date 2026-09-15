# The streams a replay is held to

A `.stream` here is the text form of `Vaelen::Player::InputStream` (14.01): one
header line naming the world, then one record per line in time order — takings,
commands, day turns. `Tools/Atlas --replay <file>` builds a fresh world from
that header and puts the records back through `Run::Door`, and the four digests
it comes to are what CTest `Replay.Played` pins.

## `aelvor256-stand-in-2026-09-15.stream` — NOT a month played by hand

This file was written by

```
VaelenAtlas --size 256 --years 100 --want-bound 0 \
            --stand Tests/Run/Streams/aelvor256-stand-in-2026-09-15.stream
```

**It did not come from the editor.** Nobody pressed a key to make it. Task
14.10's clause (a) asks for thirty days played at the keyboard in the 13.09
level and written out with `F9`, and that is the owner's machine and has not
happened yet. What this is instead is a month of the same SHAPE, so that every
piece of machinery around it — the replay, the two `LogVaelenPlay` lines, the
four pinned digests, the CTest entry — is exercised and green before the real
month arrives. `--stand` refuses to write a file that does not have that shape:
thirty recorded day turns, every one of the eight verbs at least once, a Move
the page offered and the door queued - which it can only do when a neighbour is
adjacent AND detailed - and at least one intent the WORLD refused rather than
the door.

Every command in it went through `View::Press` first, exactly as a key press
does in 14.09, so it walks the path a keyboard walks.

The host configuration it was recorded under, which a replay must be given
because the rules are deliberately not in the stream (`Run/Door.h`):

| | |
|---|---|
| size / pre-history / years | 256 / 300 / 100 |
| seed | `0x41454c564f52` |
| `StartRules::WantBound` | **0** — whoever the world offers, as `VaelenWorldSubsystem` does |
| colony, lively | neither: the host asks for `Play` and nothing else |

The header line says the first three and the seed, so the world it belongs to
is readable without tooling. `WantBound` is the one that is not in the file,
which is why `Replay.Played` passes `--want-bound 0`.

## When the owner's month lands

Check it in beside this one as `aelvor256-<date>.stream`, re-pin the three
expected lines in `Tests/Run/CMakeLists.txt` from its own replay, and delete
the stand-in in the same commit. Until then, no clause of 14.10 that names a
month played by hand may be called satisfied.

## What makes a stream here go stale

The digests are of a whole simulated world, so anything that changes what the
world does moves them: the wiring order in `Run/Aelvor.cpp` ("a type declared
in a different position is a different world"), any system's arithmetic, the
pre-history rules, worldgen, the panel's text layout, `ExportLife`. That is the
same class as `Atlas.Frozen128` and the fix is the same: re-freeze deliberately,
with an ADR, never by quietly editing a number until the test goes green.

A change to the stream FORMAT is different and fails loudly: `DecodeStream`
refuses a version it does not know, so the file stops loading rather than
loading wrong.
