#!/usr/bin/env python3
"""Builds one part of the base layer (ae_base_<n>.aepack) with ae_pack_build in tags-only mode.

The base layer is linked from several parts (link.py's JoinedBase: ae_pack_build takes at most 64 roots a run). A
part is the builder's own stock Blood Gulch, an EMPTY external-names list (a base root is a map's own tag, which a
names list would refuse), the prefix ae\\base\\, the id ae_base_<n>, revision 1, the display name BASE LAYER, the
credits line below, and the part's roots IN THIS ORDER (the order is part of the bytes). PARTS holds the roots of the
parts whose list is known; the pack's SHA-256 is checked against the recorded one.

    build_part.py <n> --builder build/linux/ae_pack_build --stock-map <bloodgulch.map> --out <ae_base_n.aepack>

Part 5 (0.7.10: the two pickup sounds, the assault rifle's first-person shaders and the two HUD masters, then the
Protomag's four) is rebuilt byte for byte by this. Parts 1 to 4 have not changed since 0.7.6; their root lists were
never written down (their files travel with the release's fragment set), so a change to one of them starts by
recovering its list from the part itself (the own tags that are roots in recipe.json, in the file's order).
"""
import argparse
import hashlib
import os
import subprocess
import sys
import tempfile

CREDITS = "Stock-derived copies of the base game's multiplayer tags (the base game is required).\n"
PARTS = {
    5: {
        "sha256": "84a904f67b540a94f5248f3ba5196c4a17dc8057319ac166ddcac4afaef75d21",
        "roots": [
            "snd!:sound\\sfx\\weapons\\weapon_pickup_ammo\\shotgun_ammo",
            "snd!:sound\\sfx\\weapons\\weapon_pickup_ammo\\sniper_ammo",
            "soso:weapons\\assault rifle\\fp\\shaders\\display",
            "soso:weapons\\assault rifle\\fp\\shaders\\grip",
            "soso:weapons\\assault rifle\\fp\\shaders\\gun",
            "wphi:ui\\hud\\master plasma",
            "wphi:ui\\hud\\master rounds",
            "bitm:ui\\hud\\bitmaps\\pistol\\pistol_scope_mask",
            "bitm:ui\\hud\\bitmaps\\pistol\\pistol_scope_mask split",
            "eqip:powerups\\pistol ammo\\pistol ammo",
            "soso:weapons\\pistol\\fp\\shaders\\bullet",
        ],
    },
}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("part", type=int, choices=sorted(PARTS))
    parser.add_argument("--builder", required=True)
    parser.add_argument("--stock-map", required=True)
    parser.add_argument("--out", required=True)
    args = parser.parse_args(argv)
    part = PARTS[args.part]
    with tempfile.TemporaryDirectory() as work:
        names, credits = os.path.join(work, "names.txt"), os.path.join(work, "credits.txt")
        open(names, "w").close()
        with open(credits, "w") as handle:
            handle.write(CREDITS)
        command = [args.builder, "--donor", args.stock_map, "--external-names", names, "--prefix", "ae\\base\\",
                   "--id", "ae_base_%d" % args.part, "--revision", "1", "--display", "BASE LAYER"]
        for root in part["roots"]:
            command += ["--root", root]
        command += ["--credits", credits, "--out", args.out]
        run = subprocess.run(command, capture_output=True, text=True)
        if run.returncode:
            sys.stderr.write(run.stdout + run.stderr)
            return 1
    with open(args.out, "rb") as handle:
        digest = hashlib.sha256(handle.read()).hexdigest()
    if digest != part["sha256"]:
        sys.stderr.write("build_part: %s is %s, the recorded part is %s\n" % (args.out, digest, part["sha256"]))
        return 1
    print("ok %s %s" % (args.out, digest))
    return 0


if __name__ == "__main__":
    sys.exit(main())
