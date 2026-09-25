# The golden save corpus — Phase 16 task 16.01

Three images written by the build at commit `867a129`, save format version 3,
on 2026-09-21. **They are never regenerated.** A golden is an image written by a
build that no longer exists; the moment this phase changes what a refusal means
(16.08) or puts a container around the image (16.09), no build in this
repository can write an honest v3 file again, and there is nothing older than
Phase 16 for a migration chain to migrate. That is why 16.01 is the first commit
of the phase and a one-way door.

| file | size | pre-history | years | options | format | layout digest | state digest | bytes |
|---|---|---|---|---|---|---|---|---|
| `bare-16.snapshot` | 16 | 10 | 1 | none | 3 | `02ffc6da0ab4b0db` | `d17d7fd9a6f09ea6` | 77478 |
| `full-16.snapshot` | 16 | 10 | 1 | Play+Stream+Lively+Colony | 3 | `5ab1a2f994715f25` | `0a88aacecd9aaa23` | 78036 |
| `full-32.snapshot` | 32 | 10 | 1 | Play+Stream+Lively+Colony | 3 | `5ab1a2f994715f25` | `3803ec5a28729144` | 173414 |

Regenerated — only to prove today's build still writes them byte for byte — by:

```
VaelenAtlas --golden Tests/Run/Golden/
```

**They are images of the world before the winter.** Since 18.10 the climate is
the default (`Options::Climate = true`, ADR-0154); the writer above and
`Golden.V3RoundTrips` both name `Climate = false`, and no layer or type of
Phase 18 enters a world wired so, which is why the layout digests above still
match. Found at the flip: `--golden` had inherited the default and wrote three
climate images that differed from every file here; it now says which world,
and was measured writing all three byte for byte again.

CTest `Golden.V3RoundTrips` loads each one into a freshly wired world of its
recorded options, requires `Ok`, requires the state digest above, and requires a
re-save to be byte-identical to the file on disk.

## Why the worlds are this small, which is a measurement and not modesty

A golden lives in git forever. Measured on this tree, at the full wiring:

| world | image |
|---|---|
| 16 tiles, 10 years of pre-history, 1 of history | 78 KB |
| 16 tiles, **30** and **3** | **11.4 MB** |
| 32 tiles, 20 and 2 | 7.1 MB |
| 32 tiles, 50 and 5 | 22.6 MB |

One hundred and forty-six times larger for twenty more years of pre-history.
**The event log is 75% of any image that has a history at all** — 76,356 events
at 112 bytes each in that 11.4 MB world — and the rest is the people it made.

The planning proposed a third golden of "32 tiles, 5 years, about 104 KB". That
world is 22,568,335 bytes: two hundred and sixteen times the estimate, in a
repository whose largest file is 539 KB. The estimate was not checked before it
was written down, and this table is what checking it produced.

**So the corpus buys FORMAT coverage, not WORLD coverage.** Every section of the
image, the full type set, on two map sizes — and no world with a long history.
16.09's forge exists because of that gap, and this paragraph is here so nobody
reads a green `Golden.V3RoundTrips` as more than it is.

## What the corpus already proved, on the day it was written

`full-16` and `full-32` have **the same layout digest**, `5ab1a2f994715f25`.
Two worlds whose maps differ by a factor of four in area are indistinguishable
to the only value `LoadSnapshot` compares, because `WorldMap::LayoutDigest`
folds layer name hashes and element sizes and no extents. That is defect 5 of
the twelve this phase opened on, and the corpus demonstrated it by existing.
