"""The weapon pack "done" check (notes/plans/2026-11-02-weapon-pack-done-checklist.md, section 2), its static half: a
built .aepack read with aepack.py, its recipe (optional) and the game's load log (optional), against the checklist's
points that a pack can show by itself: 1 and 2 models and art, 3 HUD with the message icon, 4 pickup texts, 5 sounds
and effects, 6 projectile and damage tags, 7 credits and the recipe's recorded pack, 10 the kill feed's name and
icon, 14 haptics. The game-side points (8 slots and rows, 9 bots, 11 seeds and docs, 12 kit, 13 notes) and the
in-game half (fire, reload, zoom, kill a bot) are not checked here.

    pack_done.py [--recipe recipe.json] [--log debug.txt] <pack.aepack>...

A melee weapon (the energy sword) fires nothing and holds no rounds: its pack has no projectile tag and no ammo
pickup text. Its recipe says so ("melee": true in its "pack"); with that and the pack agreeing, the ammo text and the
projectile are not asked for (its weapon pickup text and its damage effect still are). Without a recipe such a pack's
two checks are skipped, not passed; a recipe that says melee for a pack with a projectile or an ammo text fails them.

Exit 0 when every check of every pack passes (a check that cannot be made, no recipe or no log, is "skipped" and does
not fail), 1 when one fails, 2 for bad arguments or a pack the reader refuses. It prints one line a check.
"""
import hashlib
import json
import os
import re
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))    # (found when run with python -I, too)
import aepack

# the AE rifles' behaviours (ae_pack_format.h): the engine gives them a rumble-only kick, a reticle growth and a
# DualSense trigger; every other weapon's feel is its tags'
RIFLE_BEHAVIOURS = (1, 2, 3)
GROUPS = {name: aepack.group(name) for name in ("weap", "mode", "mod2", "bitm", "effe", "snd!", "proj", "jpt!", "wphi")}


def string_at(parsed, offset):
    if offset == aepack.NONE:
        return None
    end = parsed.strings.index(b"\0", offset)
    return parsed.strings[offset:end].decode("utf-8")


def tag_name(parsed, tag):
    return string_at(parsed, tag[3]) or ""


def recipe_vibrates(recipe):
    """whether a step of the recipe sets a damage effect's pad vibration above 0 (a haptics edit)"""
    for step in recipe.get("steps", ()):
        for edit in step.get("edits", ()):
            pairs = edit[1] if edit and edit[0] == "sets" else [edit[1:3]] if edit and edit[0] == "set" else []
            for field, value in pairs:
                if "vibrate_frequency" in str(field):
                    try:
                        if float(str(value).split()[0]) > 0:
                            return True
                    except ValueError:
                        pass
    return False


def pack_corrections(log, key):
    """the corrections the game counted when it added this pack weapon's pack to a map, one number a load: each
    "weapon pack: added to <map>: ... N corrections" line is the pack's whose look line ("weapon pack: <key>'s look:")
    follows it (the game logs a pack's added line, then its weapons' look lines), so another pack's corrections in
    the same log are not this one's"""
    counts, last = [], None
    look = "weapon pack: %s's look:" % key
    for line in log.splitlines():
        added = re.search(r"weapon pack: added to [^:]*: .*?(\d+) corrections", line)
        if added:
            last = added.group(1)
        elif look in line and last is not None:
            counts.append(last)
            last = None
    return counts


def melee_weapon(recipe, weapon, projectiles, ammo):
    """None for a weapon that fires (the usual checks); for a melee weapon, (verdict, detail) of its two checks:
    True when its recipe says melee and the pack agrees (no projectile, no ammo text), None (skipped) when the pack
    looks melee but no recipe says so, False when the recipe says melee and the pack does not agree"""
    says = bool(recipe and recipe.get("pack", {}).get("melee"))
    looks = projectiles == 0 and not ammo and weapon[3] not in RIFLE_BEHAVIOURS
    if says:
        return (looks, "the recipe says melee" + ("" if looks else ": but projectiles %d, ammo text %r" % (projectiles, ammo)))
    if looks and recipe is None:
        return (None, "no projectile and no ammo text: a melee weapon? no recipe given to say so")
    return None


def check_pack(path, recipe=None, log=None):
    """[(point, name, "pass" | "fail" | "skip", detail)] for one pack"""
    image = open(path, "rb").read()
    parsed = aepack.read(image)
    results = []

    def add(point, name, ok, detail=""):
        results.append((point, name, "skip" if ok is None else "pass" if ok else "fail", detail))

    own = [tag for tag in parsed.tags if tag[4] == aepack.OWN]
    anytag = parsed.tags
    count = lambda tags, name: sum(1 for tag in tags if tag[0] == GROUPS[name])
    names = [tag_name(parsed, tag).lower() for tag in own]
    if not parsed.weapons:
        add("-", "weapons", False, "no weapon entry (a pack of tags only is not a weapon pack)")
        return results
    for weapon in parsed.weapons:
        key = string_at(parsed, weapon[0])
        root = parsed.tags[weapon[1]]
        add(1, "%s: a weapon root, 3P and FP models" % key, root[0] == GROUPS["weap"] and count(own, "mode") + count(own, "mod2") >= 2,
            "own models %d" % (count(own, "mode") + count(own, "mod2")))
        add(2, "%s: textures" % key, count(own, "bitm") >= 1, "own bitmaps %d" % count(own, "bitm"))
        add(3, "%s: a HUD interface" % key, count(own, "wphi") >= 1, "own HUD interfaces %d" % count(own, "wphi"))
        icon = weapon[10]
        icon_ok = icon != aepack.NONE and icon < len(parsed.tags) and parsed.tags[icon][0] == GROUPS["bitm"]
        add(3, "%s: the message icon (pickup prompt, kill feed)" % key, icon_ok,
            "icon NONE" if icon == aepack.NONE else "bitmap tag %d sequence %d" % (icon, weapon[11]))
        pickup, ammo = string_at(parsed, weapon[8]), string_at(parsed, weapon[9])
        projectiles = count(anytag, "proj")
        melee = melee_weapon(recipe, weapon, projectiles, ammo)
        if melee is None:
            add(4, "%s: pickup texts" % key, bool(pickup) and bool(ammo), "weapon %r, ammo %r" % (pickup, ammo))
        else:
            add(4, "%s: the weapon pickup text" % key, bool(pickup), "weapon %r" % pickup)
            add(4, "%s: no ammo pickup text (a melee weapon)" % key, melee[0], melee[1])
        add(5, "%s: sounds" % key, count(anytag, "snd!") >= 1, "sound tags %d" % count(anytag, "snd!"))
        add(5, "%s: effects" % key, count(anytag, "effe") >= 1, "effect tags %d" % count(anytag, "effe"))
        if melee is None:
            add(6, "%s: projectile and damage tags" % key,
                projectiles >= 1 or weapon[3] in RIFLE_BEHAVIOURS, "projectiles %d" % projectiles)
        else:
            add(6, "%s: no projectile (a melee weapon)" % key, melee[0], melee[1])
        add(6, "%s: a damage effect" % key, count(anytag, "jpt!") >= 1, "damage effects %d" % count(anytag, "jpt!"))
        if weapon[3] in RIFLE_BEHAVIOURS:
            add(3, "%s: a reticle (a rifle: zoom pieces)" % key, any(w in n for n in names for w in ("reticle", "scope", "sight")),
                "own tags named reticle, scope or sight: %d" % sum(any(w in n for w in ("reticle", "scope", "sight")) for n in names))
            add(10, "%s: the kill feed's name" % key, bool(string_at(parsed, weapon[7])), "")
        else:
            add(10, "%s: the kill feed's name" % key, bool(string_at(parsed, weapon[7])), "")
        # (14) haptics: an AE rifle has the engine's rumble-only kick; any other weapon's rumble is its damage effect's
        if weapon[3] in RIFLE_BEHAVIOURS:
            add(14, "%s: haptics (the engine's rifle kick)" % key, True, "behaviour %d" % weapon[3])
        elif recipe is None:
            add(14, "%s: haptics" % key, None, "no recipe given: the rumble is its damage effect's")
        else:
            add(14, "%s: haptics (a rumble on the weapon's damage effect)" % key, recipe_vibrates(recipe))
        if log is not None:
            look = re.search(r"weapon pack: %s's look: .*?message icon ([^\s;]+)(?: sequence (\d+) \((\d+) sprites\))?;?"
                             r".*?its HUD's icon sequence (\S+)" % re.escape(key or ""), log)
            add(3, "%s: the game's load line shows the icon" % key,
                bool(look) and look.group(1) not in ("none", "refused") and look.group(4) not in ("none", "-1"),
                look.group(0)[:120] if look else "no look line in the log")
            corrected = pack_corrections(log, key)
            add(7, "%s: no tag corrections in the game" % key, bool(corrected) and all(int(n) == 0 for n in corrected),
                "corrections %s" % corrected if corrected else "no added line before its look line")
    credits = parsed.credits.decode("utf-8", "replace")
    add(7, "credits", len(credits.strip()) > 100 and ("Thank you" in credits or "credit" in credits.lower()),
        "%d bytes" % len(credits))
    if recipe is None:
        add(7, "the recipe's recorded pack", None, "no recipe given")
    else:
        expected = recipe.get("expected", {}).get("pack_sha256")
        add(7, "the recipe's recorded pack", expected == hashlib.sha256(image).hexdigest(),
            "recorded %s, pack %s" % ((expected or "none")[:12], hashlib.sha256(image).hexdigest()[:12]))
    return results


def main(arguments):
    recipe = log = None
    paths = []
    arguments = list(arguments)
    while arguments:
        item = arguments.pop(0)
        if item == "--recipe" and arguments:
            recipe = json.load(open(arguments.pop(0)))
        elif item == "--log" and arguments:
            log = open(arguments.pop(0), errors="replace").read()
        elif item.startswith("-"):
            print(__doc__.strip().splitlines()[-1], file=sys.stderr)
            return 2
        else:
            paths.append(item)
    if not paths:
        print(__doc__.strip().splitlines()[-1], file=sys.stderr)
        return 2
    status = 0
    for path in paths:
        print("pack_done %s" % path)
        try:
            results = check_pack(path, recipe, log)
        except aepack.Refused as refusal:
            print("  refused by the reader: %s" % refusal)
            return 2
        for point, name, verdict, detail in results:
            print("  [%s] %s %s%s" % (verdict.upper(), ("%s." % point).ljust(3), name, " (%s)" % detail if detail else ""))
            if verdict == "fail":
                status = 1
        print("  %s" % ("DONE (static half)" if status == 0 else "NOT DONE"))
    return status


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
