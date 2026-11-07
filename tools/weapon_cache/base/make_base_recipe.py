#!/usr/bin/env python3
"""Writes base/recipe.json from base/names.tsv (design 5.4): the base layer's tags-only pack recipe.

The base layer holds stock-derived copies of every tag the AE weapons borrow from the map (their externals), under
the prefix ae\\base\\, so that a weapon works on any map that lacks them. It is built on the box by ae_pack_build in
tags-only mode, one `--root group:name` per row, from the builder's own stock Blood Gulch (pinned by sha256 at build
time); the bytes exist only in the built cache and the release zip, never in the repository.

usage: make_base_recipe.py [names.tsv] [recipe.json]
"""
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))


def build(names_path, out_path):
    roots = []
    with open(names_path) as handle:
        for line in handle:
            if line.startswith("#") or not line.strip():
                continue
            group, name = line.rstrip("\n").split("\t")[:2]
            roots.append(f"{group}:{name}")
    roots.sort()
    recipe = {
        "format": 1,
        "about": "The AE base layer: stock-derived copies of every tag the AE weapons borrow from the map, so each "
                 "weapon works on any map. Tags-only pack (no weapons). The builder's own stock Blood Gulch is the "
                 "source; nothing of it is in the repository. make_base_recipe.py writes this file from names.tsv.",
        "pack": {
            "id": "ae_base", "revision": 1, "display": "BASE LAYER", "prefix": "ae\\base\\", "no_weapons": True,
            "roots": roots,
            "stock_derived": {"ae\\base\\*": "a copy of the stock multiplayer tag of the same path (and the tags it "
                                             "needs), moved under ae\\base\\ ; the base game is required to build it"},
        },
        "stock_map": {"name": "bloodgulch.map", "sha256": None,
                      "note": "pinned at the first box build (T1-6); every root must be on all 13 stock MP maps"},
        "tools": {"builder": "ae_pack_build --tags-only"},
        "expected": {"pack_sha256": None, "own_tags": None},
    }
    with open(out_path, "w") as handle:
        json.dump(recipe, handle, indent=1)
        handle.write("\n")
    return len(roots)


if __name__ == "__main__":
    names = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, "names.tsv")
    out = sys.argv[2] if len(sys.argv) > 2 else os.path.join(HERE, "recipe.json")
    print(build(names, out), "roots")
