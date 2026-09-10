#!/usr/bin/env python3
# VAELEN - inlines an atlas run into the viewer page.
#
# The page cannot fetch its data: an artifact host forbids it, and a file:// page
# forbids it too. So the world is embedded. This does the embedding, and refuses
# to do it for a file the checker would reject - a viewer that draws a broken
# world convincingly is worse than one that will not open.
#
# STATUS: PROTOTYPE (Phase 13) - the page it writes is the deliverable of 13.07b
import argparse
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(HERE))
import importlib.util

MARK = "__AELVOR_DATA__"


def load_checker():
  spec = importlib.util.spec_from_file_location(
    "check_atlas_output", os.path.join(os.path.dirname(HERE), "check_atlas_output.py"))
  module = importlib.util.module_from_spec(spec)
  spec.loader.exec_module(module)
  return module


def main():
  parser = argparse.ArgumentParser(description="Inlines a VaelenAtlas run into the viewer page.")
  parser.add_argument("atlas", help="the JSON VaelenAtlas wrote")
  parser.add_argument("--template", default=os.path.join(HERE, "Atlas.html"))
  parser.add_argument("--out", required=True, help="where to write the standalone page")
  args = parser.parse_args()

  with open(args.atlas, "r", encoding="utf-8") as handle:
    doc = json.load(handle)
  bad = load_checker().check(doc)
  for line in bad:
    print("%s: %s" % (args.atlas, line), file=sys.stderr)
  if bad:
    return 1

  with open(args.template, "r", encoding="utf-8") as handle:
    page = handle.read()
  if MARK not in page:
    print("the template has no %s to fill" % MARK, file=sys.stderr)
    return 1

  # Separators without spaces: at 65536 tiles a space per number is 260 KB of
  # nothing. "</script>" cannot occur in a document of numbers, but it is
  # escaped anyway rather than trusted not to.
  data = json.dumps(doc, separators=(",", ":")).replace("</", "<\\/")
  page = page.replace(MARK, data)
  with open(args.out, "w", encoding="utf-8") as handle:
    handle.write(page)
  print("%s: %u x %u, %u tiles, %.1f KB"
        % (args.out, doc["ground"]["width"], doc["ground"]["height"], doc["ground"]["tiles"],
           os.path.getsize(args.out) / 1024.0))
  return 0


if __name__ == "__main__":
  sys.exit(main())
