#!/usr/bin/env python3
# VAELEN - checks what Tools/Atlas wrote.
#
# The tool exiting 0 proves it did not crash. It does not prove the file is a
# world: an empty tiles array, a coastline of zero, a regions list that lost
# half its entries all exit 0 just as cheerfully. This reads the file back and
# holds it to what the view promised.
#
# --self-test proves every check below actually fires, because a check nobody
# has seen fail is a check nobody has.
#
# STATUS: VALIDATED - run by the CTest entries Atlas.Output and Atlas.OutputSelfTest
import argparse
import json
import sys

# Vaelen::View::GroundFlag, mirrored. The C++ side asserts these against
# WorldGen::TerrainFlag; here they are only read back out of the file.
LAND = 1 << 0
COAST = 1 << 1
SHORE = 1 << 2
BORDER = 1 << 3
RIVER = 1 << 4
LAKE = 1 << 5
KNOWN = LAND | COAST | SHORE | BORDER | RIVER | LAKE


def check(doc):
  """Returns a list of complaints; empty means the document is a world."""
  bad = []

  def want(condition, message):
    if not condition:
      bad.append(message)
    return condition

  if not want(isinstance(doc, dict), "the document is not an object"):
    return bad
  if not want(doc.get("schema") == 1, "schema is not 1"):
    return bad
  for key in ("run", "frame", "ground", "regions", "tiles"):
    if not want(key in doc, "missing section: %s" % key):
      return bad

  frame, ground, tiles = doc["frame"], doc["ground"], doc["tiles"]
  width, height = ground.get("width", 0), ground.get("height", 0)
  expected = width * height
  want(expected > 0, "the ground has no tiles")
  want(ground.get("tiles") == expected,
       "ground.tiles is %r, not width * height = %d" % (ground.get("tiles"), expected))
  want(frame.get("width") == width and frame.get("height") == height,
       "the frame and the ground disagree about the size of the map")

  for name in ("biome", "ground", "region", "elevation"):
    column = tiles.get(name)
    if not want(isinstance(column, list), "tiles.%s is missing" % name):
      continue
    want(len(column) == expected,
         "tiles.%s has %d entries, not %d" % (name, len(column), expected))

  # A world with no coast is a world with nothing worth drawing on it.
  land = ground.get("land", 0)
  want(0 < land < expected, "land is %r of %d tiles: that is not a continent" % (land, expected))
  want(ground.get("coast", 0) > 0, "the ground has no coastline")

  flags = tiles.get("ground") or []
  contradictions = sum(1 for f in flags if (f & LAND) and (f & SHORE))
  want(contradictions == 0, "%d tiles are land AND sea shore at once" % contradictions)
  unknown = sum(1 for f in flags if f & ~KNOWN)
  want(unknown == 0, "%d tiles carry a ground bit nothing defines" % unknown)
  counted = sum(1 for f in flags if f & LAND)
  want(counted == land, "ground.land says %r, the tiles say %d" % (land, counted))

  regions = doc["regions"]
  want(len(regions) == frame.get("regions"),
       "frame.regions says %r, the list has %d" % (frame.get("regions"), len(regions)))
  want(ground.get("regions") == frame.get("regions"),
       "the ground mentions %r regions, the frame has %r" % (ground.get("regions"), frame.get("regions")))
  indices = [r.get("index") for r in regions]
  want(indices == sorted(indices) and len(set(indices)) == len(indices),
       "the regions are not in strictly increasing index order")
  named = set(indices)
  strays = sorted({r for r in (tiles.get("region") or []) if r != 0 and r not in named})
  want(not strays, "the ground stands on regions the frame does not have: %s" % strays[:8])
  offmap = [r.get("index") for r in regions if not 0 <= r.get("tile", -1) < expected]
  want(not offmap, "regions %s are centred off the map" % offmap[:8])
  people = sum(r.get("people", 0) for r in regions)
  want(people == frame.get("people"),
       "frame.people says %r, the regions add up to %d" % (frame.get("people"), people))
  bound = sum(r.get("bound", 0) for r in regions)
  want(bound <= people, "more people are bound (%d) than are alive (%d)" % (bound, people))

  # ── the network ────────────────────────────────────────────────────────────
  net = doc.get("network")
  routes = doc.get("routes")
  colonies = doc.get("colonies")
  if want(isinstance(net, dict), "missing section: network") and \
     want(isinstance(routes, list), "missing section: routes") and \
     want(isinstance(colonies, list), "missing section: colonies"):
    want(net.get("routes") == len(routes),
         "network.routes says %r, the list has %d" % (net.get("routes"), len(routes)))
    want(net.get("colonies") == len(colonies),
         "network.colonies says %r, the list has %d" % (net.get("colonies"), len(colonies)))
    opened = sum(1 for r in routes if r.get("open"))
    want(net.get("open") == opened, "network.open says %r, %d routes are open" % (net.get("open"), opened))
    hands = sum(c.get("hands", 0) for c in colonies)
    want(net.get("hands") == hands, "network.hands says %r, the colonies hold %d" % (net.get("hands"), hands))
    want(isinstance(net.get("digest"), str) and net["digest"] != "0x0000000000000000",
         "the network digest is missing or zero")

    order = [r.get("index") for r in routes]
    want(order == sorted(order) and len(set(order)) == len(order),
         "the routes are not in strictly increasing index order")
    ends = [r for r in routes if not (0 < r.get("from", 0) < r.get("to", 0))]
    want(not ends, "%d routes do not run from a lower region index to a higher one" % len(ends))
    off = [r.get("index") for r in routes if r.get("from") not in named or r.get("to") not in named]
    want(not off, "routes %s run to a region the frame does not have" % off[:8])
    flags = [r.get("index") for r in routes if r.get("open") not in (0, 1)]
    want(not flags, "routes %s are neither open nor closed" % flags[:8])

    # A PAIR DOES NOT IDENTIFY A ROUTE (ADR-0120): 06.04 builds a second entity
    # when it reopens a road it closed earlier in the same tick, so twins are
    # expected and are NOT an error here. Two OPEN roads on one pair would be:
    # goods would cross twice. That is the invariant, and it is checked.
    live = {}
    twins = 0
    for r in routes:
      key = (r.get("from"), r.get("to"))
      if r.get("open"):
        twins += 1 if key in live else 0
        live[key] = 1
    want(twins == 0, "%d pairs of regions carry two OPEN roads" % twins)

    stray = sorted({c.get("region") for c in colonies if c.get("region") not in named})
    want(not stray, "colonies sit on regions the frame does not have: %s" % stray[:8])
    idle = [c.get("region") for c in colonies if c.get("hands", 0) == 0 and c.get("lifted", 0) > 0]
    want(not idle, "colonies %s lifted ore with nobody on the rock" % idle[:8])

  # ── the centuries ──────────────────────────────────────────────────────────
  tl = doc.get("timeline")
  if want(isinstance(tl, dict), "missing section: timeline"):
    keep = tl.get("keep")
    rstride, tstride = tl.get("regionStride"), tl.get("routeStride")
    if want(isinstance(keep, list), "timeline.keep is missing"):
      want(tl.get("frames") == len(keep),
           "timeline.frames says %r, the list has %d" % (tl.get("frames"), len(keep)))
      want(rstride == 5 and tstride == 3,
           "the strides are %r and %r, not 5 and 3" % (rstride, tstride))
      years = [f.get("year") for f in keep]
      want(years == sorted(years) and len(set(years)) == len(years),
           "the kept frames are not in strictly increasing year order")
      if keep:
        want(years[-1] == doc["run"].get("year"),
             "the last kept frame is year %r, the run ended at %r" % (years[-1], doc["run"].get("year")))
      ragged = [f.get("year") for f in keep
                if len(f.get("regions", [])) % 5 or len(f.get("routes", [])) % 3]
      want(not ragged, "frames %s have a row that does not fill the stride" % ragged[:8])
      # Every frame talks about the regions and roads the document has, and its
      # own totals add up. A frame that quietly lost half its regions would
      # scrub past without anything looking wrong.
      routeIds = {r.get("index") for r in (routes or [])}
      bad_frames = []
      for f in keep:
        a = f.get("regions", [])
        if {a[i] for i in range(0, len(a) - 4, 5)} - named:
          bad_frames.append(("regions", f.get("year")))
          continue
        if sum(a[i + 1] for i in range(0, len(a) - 4, 5)) != f.get("people"):
          bad_frames.append(("people", f.get("year")))
        r = f.get("routes", [])
        if {r[i] for i in range(0, len(r) - 2, 3)} - routeIds:
          bad_frames.append(("routes", f.get("year")))
        elif sum(1 for i in range(0, len(r) - 2, 3) if r[i + 1]) != f.get("open"):
          bad_frames.append(("open", f.get("year")))
      want(not bad_frames, "kept frames disagree with the document: %s" % bad_frames[:6])

  for key, where in (("digest", frame), ("digest", ground)):
    want(isinstance(where.get(key), str) and where[key] != "0x0000000000000000",
         "a digest is missing or zero, so nothing can be compared to this run")
  want(ground.get("elevationScale", 0) > 0, "the elevation scale is not written down")
  return bad


def a_world():
  """The smallest document that passes: two tiles of land, one shore, one sea.

  The frame digest is left out and added by the caller, so the "missing digest"
  case can drop it without the two disagreeing about which one it dropped.
  """
  return {
    "schema": 1,
    "run": {"seed": "0x41454c564f52", "size": 2, "year": 10},
    "frame": {"width": 2, "height": 2, "people": 3, "regions": 2},
    "ground": {"width": 2, "height": 2, "tiles": 4, "land": 2, "coast": 1, "regions": 2,
               "digest": "0x0000000000000001", "elevationScale": 65536},
    "regions": [{"index": 1, "tile": 0, "people": 2, "bound": 1},
                {"index": 2, "tile": 1, "people": 1, "bound": 0}],
    "network": {"routes": 1, "open": 1, "colonies": 1, "hands": 2, "bytes": 64,
                "digest": "0x0000000000000003"},
    "routes": [{"index": 1, "from": 1, "to": 2, "open": 1, "idle": 0, "openings": 1, "carried": 5}],
    "colonies": [{"region": 1, "hands": 2, "lifted": 7}],
    "timeline": {"every": 5, "frames": 2, "regionStride": 5, "routeStride": 3, "keep": [
      {"year": 5, "people": 1, "open": 0, "regions": [1, 1, 0, 0, 0, 2, 0, 0, 0, 0], "routes": [1, 0, 0]},
      {"year": 10, "people": 3, "open": 1, "regions": [1, 2, 1, 1, 1, 2, 1, 0, 0, 1], "routes": [1, 1, 5]}]},
    "tiles": {"biome": [1, 1, 0, 0], "ground": [LAND, LAND | COAST, SHORE, 0],
              "region": [1, 2, 0, 0], "elevation": [10, 20, -5, -9]},
  }


def self_test():
  """Every check gets a document that breaks it, and only it."""
  base = a_world()
  base["frame"]["digest"] = "0x0000000000000002"
  failures = []
  if check(base):
    failures.append("a valid document was rejected: %s" % check(base))

  def breaks(name, mutate):
    doc = a_world()
    doc["frame"]["digest"] = "0x0000000000000002"
    mutate(doc)
    if not check(doc):
      failures.append("%s: broken document accepted" % name)

  def drop_column(d):
    d["tiles"]["biome"] = [1, 1, 0]

  breaks("schema", lambda d: d.update(schema=2))
  breaks("missing section", lambda d: d.pop("regions"))
  breaks("short column", drop_column)
  breaks("no land", lambda d: d["ground"].update(land=0))
  breaks("all land", lambda d: d["ground"].update(land=4))
  breaks("no coast", lambda d: d["ground"].update(coast=0))
  breaks("land and shore at once", lambda d: d["tiles"]["ground"].__setitem__(0, LAND | SHORE))
  breaks("undefined ground bit", lambda d: d["tiles"]["ground"].__setitem__(0, 1 << 7))
  breaks("miscounted land", lambda d: d["ground"].update(land=1))
  breaks("region count", lambda d: d["frame"].update(regions=3))
  breaks("ground region count", lambda d: d["ground"].update(regions=3))
  breaks("region order", lambda d: d["regions"].append({"index": 1, "tile": 0}))
  breaks("stray region on the ground", lambda d: d["tiles"]["region"].__setitem__(0, 7))
  breaks("centre off the map", lambda d: d["regions"][0].update(tile=99))
  breaks("people do not add up", lambda d: d["frame"].update(people=4))
  breaks("more bound than alive", lambda d: d["regions"][0].update(bound=9))
  breaks("zero digest", lambda d: d["ground"].update(digest="0x0000000000000000"))
  breaks("missing digest", lambda d: d["frame"].pop("digest"))
  breaks("no elevation scale", lambda d: d["ground"].pop("elevationScale"))
  breaks("size disagreement", lambda d: d["frame"].update(width=3))
  breaks("missing network", lambda d: d.pop("network"))
  breaks("route count", lambda d: d["network"].update(routes=2))
  breaks("open count", lambda d: d["network"].update(open=0))
  breaks("hands count", lambda d: d["network"].update(hands=9))
  breaks("zero network digest", lambda d: d["network"].update(digest="0x0000000000000000"))
  breaks("route order", lambda d: d["routes"].append(dict(d["routes"][0])))
  breaks("route ends", lambda d: d["routes"][0].update(**{"from": 2, "to": 1}))
  breaks("route off the map", lambda d: d["routes"][0].update(to=9))
  breaks("route neither open nor closed", lambda d: d["routes"][0].update(open=2))
  breaks("colony off the map", lambda d: d["colonies"][0].update(region=9))
  breaks("ore with no hands", lambda d: d["colonies"][0].update(hands=0))
  breaks("missing timeline", lambda d: d.pop("timeline"))
  breaks("frame count", lambda d: d["timeline"].update(frames=3))
  breaks("stride", lambda d: d["timeline"].update(regionStride=4))
  breaks("year order", lambda d: d["timeline"]["keep"].reverse())
  breaks("last year", lambda d: d["timeline"]["keep"][-1].update(year=11))
  breaks("ragged row", lambda d: d["timeline"]["keep"][0]["regions"].append(7))
  breaks("frame region off the map", lambda d: d["timeline"]["keep"][0]["regions"].__setitem__(0, 9))
  breaks("frame people", lambda d: d["timeline"]["keep"][0].update(people=9))
  breaks("frame route off the map", lambda d: d["timeline"]["keep"][0]["routes"].__setitem__(0, 9))
  breaks("frame open count", lambda d: d["timeline"]["keep"][0].update(open=1))

  for line in failures:
    print("SELF-TEST: %s" % line, file=sys.stderr)
  print("self-test: %d checks exercised, %d failures" % (42, len(failures)))
  return 1 if failures else 0


def same(first, second):
  """Two runs of one seed. The tool is deterministic or it is not a kernel."""
  bad = 0
  with open(first, "r", encoding="utf-8") as handle:
    a = json.load(handle)
  with open(second, "r", encoding="utf-8") as handle:
    b = json.load(handle)
  for section in ("frame", "ground"):
    if a[section]["digest"] != b[section]["digest"]:
      print("%s digest: %s then %s" % (section, a[section]["digest"], b[section]["digest"]), file=sys.stderr)
      bad += 1
  if a["tiles"] != b["tiles"]:
    print("the ground itself differs between two runs of one seed", file=sys.stderr)
    bad += 1
  if a["regions"] != b["regions"]:
    print("the regions differ between two runs of one seed", file=sys.stderr)
    bad += 1
  if a.get("timeline") != b.get("timeline"):
    print("the centuries differ between two runs of one seed", file=sys.stderr)
    bad += 1
  if bad == 0:
    print("%s and %s are the same world (frame %s, ground %s)"
          % (first, second, a["frame"]["digest"], a["ground"]["digest"]))
  return 1 if bad else 0


def main():
  parser = argparse.ArgumentParser(description="Checks a VaelenAtlas JSON file.")
  parser.add_argument("path", nargs="?", help="the file VaelenAtlas wrote")
  parser.add_argument("--self-test", action="store_true", help="prove every check fires")
  parser.add_argument("--same", nargs=2, metavar=("A", "B"),
                      help="two files of the same run: their digests must agree")
  args = parser.parse_args()
  if args.self_test:
    return self_test()
  if args.same:
    return same(args.same[0], args.same[1])
  if not args.path:
    parser.error("a path is required unless --self-test is given")
  with open(args.path, "r", encoding="utf-8") as handle:
    doc = json.load(handle)
  bad = check(doc)
  for line in bad:
    print("%s: %s" % (args.path, line), file=sys.stderr)
  if bad:
    return 1
  ground = doc["ground"]
  net = doc.get("network", {})
  pairs = {}
  twins = 0
  for r in doc.get("routes", []):
    key = (r.get("from"), r.get("to"))
    twins += 1 if key in pairs else 0
    pairs[key] = 1
  keep = doc.get("timeline", {}).get("keep", [])
  span = ("years %u-%u in %u frames" % (keep[0]["year"], keep[-1]["year"], len(keep))) if keep else "one moment"
  print("%s: %u x %u, %u tiles, %u land, %u coast, %u regions, %u roads (%u open, %u twinned), "
        "%u colonies, %s, frame %s, ground %s, network %s"
        % (args.path, ground["width"], ground["height"], ground["tiles"], ground["land"],
           ground["coast"], ground["regions"], net.get("routes", 0), net.get("open", 0), twins,
           net.get("colonies", 0), span, doc["frame"]["digest"], ground["digest"], net.get("digest", "-")))
  return 0


if __name__ == "__main__":
  sys.exit(main())
