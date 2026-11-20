"""The weapon pack "done" check (notes/plans/2026-11-02-weapon-pack-done-checklist.md, section 2), its static half: a
built .aepack read with aepack.py, its recipe (optional) and the game's load log (optional), against the checklist's
points that a pack can show by itself: 1 and 2 models and art, 3 HUD with the message icon, 4 pickup texts, 5 sounds
and effects, 6 projectile and damage tags, 7 credits and the recipe's recorded pack, 10 the kill feed's name and
icon, 14 haptics, and (since the check measures the pack against the shipped weapons, docs/weapon-pack-done.md) 15 the
first-person placement, 16 a zooming weapon's reticle while zoomed, 17 the trigger against the declared fire mode, 18 the
time to kill against the declared window (and the spread against the class), 19 the dropped item's rest, 20 the back
placement and 21 the firing sound's tail. The game-side points (8 slots and rows, 9 bots, 11 seeds and docs, 12 kit, 13
notes) and the in-game half (fire, reload, zoom, kill a bot, drop it, hang it on a back) are not checked here.

    pack_done.py [--recipe recipe.json] [--design design.json] [--reference class_reference.json] [--log debug.txt] <pack.aepack>...

The recipe may hold a "design" block (a design sheet): {"class": "smg", "fire_mode": "auto" | "semi" | "burst" | "charge",
"ttk_seconds": [lowest, highest]}, or the same under "weapons": {"<key>": {...}} for a pack of several; --design reads
one from a file instead. Checks 15, 17, 18 and 21 need it (the class for 15, 18's spread and 21, the fire mode for 17,
the window for 18): without it they are "skipped: the recipe has no design block", never passed. The ranges the class is
held to are tools/weapon_pack/class_reference.json (class_reference.py writes it from the shipped weapons).

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
import pack_measure

# the AE rifles' behaviours (ae_pack_format.h): the engine gives them a rumble-only kick, a reticle growth and a
# DualSense trigger; every other weapon's feel is its tags'
RIFLE_BEHAVIOURS = (1, 2, 3)
# every behaviour the engine's burst driver plays (ae_burst_rifle.c): the rifles' and the burst PDW's (4), its kick
# rumble-only and its rounds its profile's, but no zoom pieces
ENGINE_BEHAVIOURS = RIFLE_BEHAVIOURS + (4,)
# the behaviours whose rounds stay the tags' own while the engine paces them (ae_rifle_profiles.c's tags_rounds: the
# RAVAGER's burst, 5): not engine ones; their projectile, damage and kick are checked as any weapon's
TAGS_PACED_BEHAVIOURS = (5,)
# every behaviour a pack may hold (ae_pack_format.h's AE_PACK_BEHAVIOUR_TAGS .. AE_PACK_BEHAVIOUR_LAST)
BEHAVIOURS = (0,) + ENGINE_BEHAVIOURS + TAGS_PACED_BEHAVIOURS
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
    the same log are not this one's. A weapon cache logs one "cache pack: added to <map>: ... N corrections" line for
    every pack in it, before their look lines: its count is read the same way, so 0 is this pack's 0 and any other
    number, which cannot be put on one pack, fails it (a weapon past slot 16 loads only from a cache)"""
    counts, last = [], None
    look = "weapon pack: %s's look:" % key
    for line in log.splitlines():
        added = re.search(r"(?:weapon|cache) pack: added to [^:]*: .*?(\d+) corrections", line)
        if added:
            last = added.group(1)
        elif look in line and last is not None:
            counts.append(last)
            last = None
    return counts


def recipe_names_no_picture(recipe):
    """(0.7.6) whether the recipe opts its weapon into "no picture, my name instead" (ae_pack_names.c): its pack names
    no icon and a step sets a HUD interface's messaging icon sequence to -1. No recipe: not known, so not"""
    if not recipe or recipe.get("pack", {}).get("icon"):
        return False
    for step in recipe.get("steps", ()):
        if step.get("tag", "").endswith(".weapon_hud_interface"):
            for edit in step.get("edits", ()):
                if edit[:3] == ["set", "sequence_index", "-1"]:
                    return True
    return False


def melee_weapon(recipe, weapon, projectiles, ammo):
    """None for a weapon that fires (the usual checks); for a melee weapon, (verdict, detail) of its two checks:
    True when its recipe says melee and the pack agrees (no projectile, no ammo text), None (skipped) when the pack
    looks melee but no recipe says so, False when the recipe says melee and the pack does not agree"""
    says = bool(recipe and recipe.get("pack", {}).get("melee"))
    looks = projectiles == 0 and not ammo and weapon[3] not in ENGINE_BEHAVIOURS
    if says:
        return (looks, "the recipe says melee" + ("" if looks else ": but projectiles %d, ammo text %r" % (projectiles, ammo)))
    if looks and recipe is None:
        return (None, "no projectile and no ammo text: a melee weapon? no recipe given to say so")
    return None


REFERENCE_FILE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "class_reference.json")
NO_DESIGN = "the recipe has no design block"
# check 19: the most a dropped weapon's long direction may tilt off the ground its ground point sets it on
FLAT_TILT_DEGREES = 10.0


def load_reference(path=None):
    path = path or REFERENCE_FILE
    return json.load(open(path)) if os.path.exists(path) else None


def design_for(recipe, design, key):
    """the design sheet of a weapon: --design's, else the recipe's "design" block (its "weapons" entry of that key
    first), else None"""
    block = design if design is not None else (recipe or {}).get("design")
    if not block:
        return None
    return dict(block.get("weapons", {}).get(key) or {k: v for k, v in block.items() if k != "weapons"})


def class_range(reference, name, field):
    """([low, high], the class the range is of, the note) of a class's measured range for a field; the class's fallback
    when it has no shipped weapon of its own; None when there is none"""
    if reference is None or name is None:
        return None
    classes = reference["classes"]
    used = name if name in classes else reference.get("fallback", {}).get(name)
    if used not in classes or classes[used].get(field) is None:
        return None
    return classes[used][field], used, ("" if used == name else " (no shipped %s: held to the %s range)" % (name, used))


def slack(reference, used, field):
    """the slack a class's range gets: the tolerance, wider for a class with one shipped weapon (not a range)"""
    tolerance = reference.get("tolerance", {})
    one = len(reference["classes"][used]["members"]) < 2
    return tolerance.get(field, 0.0) * (tolerance.get("single_member_factor", 1.0) if one else 1.0)


def widest_reach(reference, place):
    """the farthest toward the body any shipped weapon of any class reaches in a place"""
    values = [row["reach"][place][1] for row in reference["classes"].values() if place in row.get("reach", {}) and row["reach"][place]]
    return max(values) if values else None


def design_checks(add, key, measure, design, reference):
    """checks 15 to 21 for a weapon, through add(point, name, verdict, detail)"""
    tolerance = (reference or {}).get("tolerance", {})
    name = (design or {}).get("class")
    # 15 first-person placement against the class
    idle = measure["idle"]
    if design is None or name is None:
        add(15, "%s: the first-person model sits as the shipped %s do" % (key, name or "weapons of its class"), None, NO_DESIGN)
    elif idle is None:
        add(15, "%s: the first-person model sits as the shipped %s do" % (key, name), None, "no first-person model of its own")
    else:
        rear, bottom = class_range(reference, name, "rear"), class_range(reference, name, "bottom")
        if rear is None or bottom is None:
            add(15, "%s: the first-person model sits as the shipped %s do" % (key, name), None, "no shipped weapon measures the class %r" % name)
        else:
            slack_by = {"rear": slack(reference, rear[1], "rear"), "bottom": slack(reference, bottom[1], "bottom")}
            problems = []
            for label, value, (low_high, used, note) in (("rear", idle["rear"], rear), ("bottom", idle["bottom"], bottom)):
                if not low_high[0] - slack_by[label] <= value <= low_high[1] + slack_by[label]:
                    problems.append("%s %.1f outside %.1f to %.1f" % (label, value, low_high[0] - slack_by[label], low_high[1] + slack_by[label]))
            detail = "rear %.1f, bottom %.1f (hundredths of a world unit, idle pose%s); the shipped %s: rear %.1f to %.1f, bottom %.1f to %.1f +-%.1f%s" % (
                idle["rear"], idle["bottom"], "" if idle["idle"] else ", rest pose: " + str(idle["idle_unread"]), rear[1], rear[0][0], rear[0][1],
                bottom[0][0], bottom[0][1], slack_by["rear"], rear[2])
            add(15, "%s: the first-person model sits as the shipped %s do" % (key, rear[1]), not problems, "; ".join(problems) or detail)
    # 16 a zooming weapon's reticle draws while zoomed
    zoom = measure["weapon"]["zoom_levels"]
    shown = measure["reticle_zoomed"]
    if zoom <= 0:
        add(16, "%s: a reticle while zoomed" % key, None, "not a zooming weapon (zoom levels %d)" % zoom)
    elif shown is None:
        add(16, "%s: a reticle while zoomed" % key, None, "no HUD of its own")
    else:
        add(16, "%s: a reticle draws while zoomed" % key, shown["draws"],
            "zoom levels %d; aim crosshair elements %d, a zoomed one: %s" % (zoom, shown["aim_elements"], "yes" if shown["draws"] else "none"))
    # 17 the trigger against the declared fire mode
    declared = (design or {}).get("fire_mode")
    if declared is None:
        add(17, "%s: the trigger fires as the design says" % key, None, NO_DESIGN)
    else:
        trigger = measure["triggers"][0] if measure["triggers"] else None
        add(17, "%s: the trigger fires as the design says (%s)" % (key, declared), measure["fire_mode"] == declared,
            "measured %s: trigger flags %s, %.2f a second, behaviour %d" % (measure["fire_mode"], (trigger or {}).get("flags"),
                                                                              (trigger or {}).get("rate_final", 0.0), measure["behaviour"]))
    # 18 the time to kill against the window; the spread against the class
    window = (design or {}).get("ttk_seconds")
    ttk = measure["ttk"]
    if window is None:
        add(18, "%s: the time to kill is in the design's window" % key, None, NO_DESIGN)
    elif ttk.get("seconds") is None:
        add(18, "%s: the time to kill is in the design's window" % key, False, "cannot be computed: %s" % ttk.get("unreadable", "no rate"))
    else:
        add(18, "%s: the time to kill is in the design's window (%.2f to %.2f s)" % (key, window[0], window[1]),
            window[0] <= ttk["seconds"] <= window[1], "%.3f s in %d shots (%s)" % (ttk["seconds"], ttk["shots"], ttk["how"]))
    spread = class_range(reference, name, "spread_degrees")
    if design is None or name is None:
        add(18, "%s: the spread is as the shipped weapons of its class" % key, None, NO_DESIGN)
    elif spread is None or measure["spread_degrees"] is None:
        add(18, "%s: the spread is as the shipped weapons of its class" % key, None, "no spread or no shipped weapon to hold it to")
    elif len(reference["classes"][spread[1]]["members"]) < 2:
        add(18, "%s: the spread is as the shipped weapons of its class" % key, None,
            "only one shipped %s: not a range to hold it to" % spread[1])
    else:
        wide = slack(reference, spread[1], "spread_degrees")
        value = measure["spread_degrees"]
        add(18, "%s: the spread is as the shipped %s" % (key, spread[1]), spread[0][0] - wide <= value <= spread[0][1] + wide,
            "widest spread %.2f degrees; the shipped: %.2f to %.2f +-%.2f%s" % (value, spread[0][0], spread[0][1], wide, spread[2]))
    # 19 the dropped item rests flat
    dropped = measure["dropped"]
    if dropped is None:
        add(19, "%s: a dropped weapon lies flat" % key, None, "no third-person model of its own")
    else:
        ground = dropped.get("ground_point")
        if ground is not None:
            # (the ground point sets the rest: the long direction lies within FLAT_TILT_DEGREES of the ground)
            flat = ground["tilt_degrees"] <= FLAT_TILT_DEGREES or dropped["own_collision"]
            how = "its ground point's up %s, its long direction %s: %.1f degrees off the ground (at most %.0f); own collision model %s" % (
                ground["up"], ground["long_direction"], ground["tilt_degrees"], FLAT_TILT_DEGREES, "yes" if dropped["own_collision"] else "no")
        else:
            flat = dropped["long_axis"] == "x" or dropped["own_collision"]
            how = "no ground point; model extents x %.1f, y %.1f, z %.1f; long axis %s; own collision model %s" % (
                dropped["extent"][0], dropped["extent"][1], dropped["extent"][2], dropped["long_axis"], "yes" if dropped["own_collision"] else "no")
        add(19, "%s: a dropped weapon lies flat" % key, flat, how)
    # 20 the back placement exists and is outside the body's bounds
    back = measure["back"]
    if back is None:
        add(20, "%s: a back placement" % key, False, "the game's rules place it nowhere (no third-person model, or no table row)")
    elif reference is None or widest_reach(reference, back["place"]) is None:
        add(20, "%s: the back placement is outside the body" % key, None, "no shipped weapon hangs in the %s place" % back["place"])
    else:
        limit = widest_reach(reference, back["place"]) + tolerance.get("reach", 0.0)
        add(20, "%s: the back placement is outside the body (%s)" % (key, back["place"]), back["reach"] <= limit,
            "reaches %.3f toward the body (world units, body frame); no shipped weapon past %.3f in that place" % (back["reach"], limit))
    # 21 the firing sound's tail under the class limit
    tail = measure["sound_tail_seconds"]
    limit = class_range(reference, name, "tail_seconds")
    if design is None or name is None:
        add(21, "%s: the firing sound's tail is within the class limit" % key, None, NO_DESIGN)
    elif tail is None:
        add(21, "%s: the firing sound's tail is within the class limit" % key, None,
            "no firing sound of its own to measure (a stock sound or none)")
    elif limit is None:
        add(21, "%s: the firing sound's tail is within the class limit" % key, None, "no shipped weapon of the class has a measured sound")
    else:
        top = limit[0][1] + slack(reference, limit[1], "tail_seconds")
        add(21, "%s: the firing sound's tail is within the %s limit" % (key, limit[1]), tail <= top,
            "tail %.2f s (within 30 dB of the loudest 10 ms), sound %.2f s; the shipped: up to %.2f +%.2f%s" % (
                tail, measure["sound_seconds"] or 0.0, limit[0][1], slack(reference, limit[1], "tail_seconds"), limit[2]))


def check_pack(path, recipe=None, log=None, design=None, reference=None):
    """[(point, name, "pass" | "fail" | "skip", detail)] for one pack"""
    image = open(path, "rb").read()
    parsed = aepack.read(image)
    results = []
    reference = reference if reference is not None else load_reference()

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
        if icon == aepack.NONE and recipe_names_no_picture(recipe):
            add(3, "%s: no picture: its name in words (prompts, kill feed, HUD)" % key, True,
                "icon NONE, the recipe's own HUD has no messaging icon (ae_pack_names.c)")
        else:
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
                projectiles >= 1 or weapon[3] in ENGINE_BEHAVIOURS, "projectiles %d" % projectiles)
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
        if weapon[3] in ENGINE_BEHAVIOURS:
            add(14, "%s: haptics (the engine's rifle kick)" % key, True, "behaviour %d" % weapon[3])
        elif recipe is None:
            add(14, "%s: haptics" % key, None, "no recipe given: the rumble is its damage effect's")
        else:
            add(14, "%s: haptics (a rumble on the weapon's damage effect)" % key, recipe_vibrates(recipe))
        try:
            measured = {m["key"]: m for m in pack_measure.measure(image)}.get(key)
        except (struct.error, IndexError, ValueError, KeyError, TypeError) as why:
            measured = None
            add("15-21", "%s: measured against the shipped weapons" % key, None, "the tags cannot be read as a weapon's (%s: %s)" % (type(why).__name__, why))
        if measured is not None:
            design_checks(add, key, measured, design_for(recipe, design, key), reference)
        if log is not None:
            look = re.search(r"weapon pack: %s's look: .*?message icon ([^\s;]+)(?: sequence (\d+) \((\d+) sprites\))?;?"
                             r".*?its HUD's icon sequence (\S+)" % re.escape(key or ""), log)
            if icon == aepack.NONE and recipe_names_no_picture(recipe):
                add(3, "%s: the game's load line shows no picture (its name in words)" % key,
                    bool(look) and look.group(1) == "none" and look.group(4) == "-1",
                    look.group(0)[:120] if look else "no look line in the log")
            else:
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
    recipe = log = design = reference = None
    paths = []
    arguments = list(arguments)
    while arguments:
        item = arguments.pop(0)
        if item == "--recipe" and arguments:
            recipe = json.load(open(arguments.pop(0)))
        elif item == "--design" and arguments:
            design = json.load(open(arguments.pop(0)))
        elif item == "--reference" and arguments:
            reference = load_reference(arguments.pop(0))
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
            results = check_pack(path, recipe, log, design, reference)
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
