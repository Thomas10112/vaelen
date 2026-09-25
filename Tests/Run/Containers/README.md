# The container corpus — Phase 17 task 17.01

Three **VAELENCP containers**, written by today's build with
`VaelenAtlas --containers`. They are **regenerated**, not frozen — which is the
opposite of `Tests/Run/Golden/` next door, and the difference matters. **And
one that is not**: `host24-16.container`, kept since 18.02 (2026-09-24) as the
first older-format instance in the tree — see the end of this file.

## The two corpora are not the same thing, and confusing them is what this task exists for

| | `Tests/Run/Golden/` | `Tests/Run/Containers/` (here) |
|---|---|---|
| format | the **image** — `SaveSnapshot`'s output, magic `VAELEN\0\0\0` | the **container** — magic `VAELENCP`, an image inside it |
| version | `VAELEN_SAVE_FORMAT_VERSION` = 3 | `CheckpointVersion` = 2 |
| origin | a build that no longer exists (`867a129`, 2026-09-21) | today's build, regenerated on demand |
| never regenerated? | **yes** — a golden is a one-way door | no — `Atlas.ContainersRegenerate` requires byte equality |
| what it buys | that an OLD file still loads | that a container is readable **without the simulation that wrote it** |

Hand a `.snapshot` to `ReadCheckpoint` and it says `BadMagic`. Phase 16 built
the container, its section table, its two flag halves and its migration
machinery, and closed **without a single container checked in anywhere in the
tree** — so three of Phase 17's four planning angles wrote gate clauses over
files that did not exist. `Run.Containers.AnImageIsNotAContainer` makes that
mistake on purpose and requires the refusal.

## What is here

The shape differs and not only the contents, because a corpus where every file
has the same section table cannot catch a reader that assumes one.

| file | tiles | pre-history | years | wiring | container v | image v | tick | log events | log bytes | bytes | image trailer |
|---|---|---|---|---|---|---|---|---|---|---|---|
| `bare-16.container` | 16 | 10 | 1 | Climate | 2 | 3 | 95040 | 139 | 15584 | 82388 | `a002a2992df640fe` |
| `played-16.container` | 16 | 10 | 1 | Play+Stream+Lively+Colony+Climate | 2 | 3 | 95184 | 159 | 17824 | 85677 | `333beace65e0feaa` |
| `full-32.container` | 32 | 10 | 1 | Play+Stream+Lively+Colony+Climate | 2 | 3 | 95184 | 701 | 78528 | 200190 | `a066899eaa656f2e` |
| `host24-16.container` | 16 | 10 | 1 | Play+Stream+Lively+Colony | 2 | 3 | 95184 | 148 | 16592 | 80862 | `64d11b40b612581c` |

The **image trailer** is the last eight bytes of the STATE section: what
`ComputeStateDigest` returns, and what every frozen digest in this repository
is. It is **not** the STATE row's section digest in the tables below, which is a
different number over the same bytes. Both are recorded, deliberately, so that a
reader which swapped them cannot pass — that swap was a live defect in
`Tools/Store/StdioCheckpointStore.h`, **fixed in 17.03**, and measuring it there
showed the two are different numbers on every container: `e0614906cb8a5676`
where `ComputeStateDigest` returns `0f6fa26b35d09a70`.

Until 18.10, `bare-16.container`'s trailer was `d17d7fd9a6f09ea6` -
`bare-16.snapshot`'s state digest in the golden README - and its STATE section
was that file's exact size, 77478 bytes: the container carries the image
**verbatim**. Since 18.10 the corpus is written by a climate world and the
goldens stay images of the world before it, so the two no longer coincide; the
verbatim claim is checked against the container's own trailer instead.

## The section tables, read back out of the bytes
### `bare-16.container`

no play wiring at all: three sections, and nobody was ever offered

Seed `000041454c564f52`, flags `00000000`, 3 sections.

| kind | offset | length | section digest |
|---|---|---|---|
| STATE (1) | 146 | 82180 | `34eb7363dd3affc8` |
| RUN (2) | 82326 | 29 | `bfd0910bf8f1c63f` |
| HOST (3) | 82355 | 25 | `4a4c60d69390d998` |

### `played-16.container`

walked six days and carrying its own tape: four sections

Seed `000041454c564f52`, flags `00000000`, 4 sections.

| kind | offset | length | section digest |
|---|---|---|---|
| STATE (1) | 176 | 85234 | `261ab2c8943e064b` |
| RUN (2) | 85410 | 37 | `befe7c22e544cdd4` |
| HOST (3) | 85447 | 25 | `d4f214b467921044` |
| STREAM (4) | 85472 | 197 | `a65b8a037dcc3b7a` |

### `full-32.container`

walked six days and saved WITHOUT its tape: three sections, and the difference from the one above is the whole point

Seed `000041454c564f52`, flags `00000000`, 3 sections.

| kind | offset | length | section digest |
|---|---|---|---|
| STATE (1) | 146 | 199968 | `050a7c2f4f8af71d` |
| RUN (2) | 200114 | 43 | `40ed8c3bbb2a1640` |
| HOST (3) | 200157 | 25 | `0df7618587f8e774` |

### `host24-16.container` — kept, not regenerated (18.02)

`played-16.container` exactly as `--containers` wrote it before 18.02, when the
HOST section had four flag bytes and was 24 bytes long. 18.02 added a fifth,
`Options::Climate`, so every container the tool writes now has a 25-byte HOST
section, a section digest of its own, and every offset after it one byte
further on — and every container written BEFORE it reads as `Climate = false`.
This file is the reader's proof of that: `Run.Containers` checks every figure
below against its bytes, `Run.Checkpoint` adopts it into a host that did not
ask for a climate (`Ok`) and into one that did (`ClimateDiffers`, by name).
The image inside is `played-16`'s as it was before 18.10 too - the world
without the climate - so since the flip its trailer (`64d11b40b612581c`) is no
longer today's `played-16`'s.

Seed `000041454c564f52`, flags `00000000`, 4 sections.

| kind | offset | length | section digest |
|---|---|---|---|
| STATE (1) | 176 | 80420 | `389defae6a977e74` |
| RUN (2) | 80596 | 37 | `befe7c22e544cdd4` |
| HOST (3) | 80633 | 24 | `8616198be3fd64ad` |
| STREAM (4) | 80657 | 197 | `a65b8a037dcc3b7a` |

## Regenerated by

```
VaelenAtlas --containers Tests/Run/Containers/
```

(the three regenerated files only; `host24-16.container` is never written by
anything again, and `Atlas.ContainersRegenerate` requires it to differ from the
`played-16.container` the tool writes today.)

**Every figure above comes out of `ReadCheckpoint` over the bytes that were just
written**, not out of what the writer intended. `--containers` reads its own
output back before recording a single number, and refuses if the trailer it
finds is not `ComputeStateDigest`. The two differing is exactly the class of
defect 17.03 found.

## The measurement that made the played containers possible

The first cut of this corpus could not produce one. `Door::TakeUp` was offered
nobody, at 16, 24 and 32 tiles alike. Sweeping the start rules at ten years of
pre-history and one of history:

| rules | offered |
|---|---|
| `WantBound 1`, any age window at all | **nobody** |
| `WantBound 0`, ages 0–10 | person 1 |
| `WantBound 0`, ages 12–120 | **nobody** |

**Nobody in a ten-year world is twelve, and nobody in it is bound to anything.**
Everyone alive was born inside it. So `StartRules{}` — a bound life aged 16 to
40 — is offered nobody, which is why `full-16.snapshot` next door was never
played, and why this corpus needed the window opened before it could have a
played container at all.

The two played containers therefore carry
`StartRules{FromAge 0, ToAge 45, WantBound 0, PreferOre 0}` — **all four
non-default**, and that is the second reason. A round trip that compares two
default-constructed structs passes even when the reader wrote nothing into
either; this phase committed exactly that assertion, and 17.07 is the harness
fix. A corpus whose STREAM section carried `StartRules{}` would hand every later
test the same vacuous comparison. `Run.Containers.TheStreamSectionCarriesRulesNobodyDefaulted`
asserts each field against the default before asserting its value.

The window is 0–45 and not 0–11 on purpose: a superset outlives the day these
ages move, and pinning the boundary would make the corpus a test of demography.

## What checks what

| entry | what it says | generates a world? |
|---|---|---|
| `Run.Containers` | the three files read, and every number above matches the bytes | **no**, and that is the claim |
| `Atlas.ContainersRegenerate` | the recorded command still produces these exact bytes | yes, three of them |

They are two entries because they make opposite claims. `Run.Containers` would
go on passing on the day this build lost the ability to *write* a container;
`Atlas.ContainersRegenerate` is what notices.

Both have failed on purpose (ADR-0149). Flipping byte 40000 of
`bare-16.container` makes the regeneration entry report
`bare-16.container no longer regenerates` and makes `Run.Containers` report the
refusal by name. Deleting one field's comparison from `FirstDisagreement` makes
`Run.Containers.PerturbationsAreSeen` report `bending log bytes was not seen at
all` — that test bends every recorded field in turn and requires the comparison
to name the field that was bent.

## If these files change

The corpus, this README and the table in `Tests/Run/Test_Containers.cpp` are
**one record in three places**. Change one and change all three in the same
commit; `Atlas.ContainersRegenerate`'s failure message says so too, because the
day somebody hits it they will not be reading this file.
