"""What a weapon pack measures as, in numbers a design is held to (pack_tags.py reads the tags): the first-person model's
rear, the zoom and the crosshair that draws while zoomed, the trigger, the damage and the time to kill against a full
shield, the projectile's bounces, the dropped item's collision, the back placement and the firing sound's tail.

TIME TO KILL (ttk_seconds): a player has 75 body and 75 shield (the stock multiplayer cyborg's collision model:
maximum_body_vitality 75, maximum_shield_vitality 75, shield material cyborg_energy_shield). A hit does the mean of the
damage effect's lower and upper bound (no falloff: point blank), a shield hit that many times the effect's
cyborg_energy_shield modifier; what a hit does beyond the shield's remainder goes on into the body times the
cyborg_armor modifier (the engine's spill over; a profile that says "no spill" stops it). A shot does its pellets
(projectiles_per_shot) at once. Body shots only, every one hits, the first at second 0: ttk = (shots - 1) / the
trigger's final rate of fire. A weapon the engine paces (an AE rifle behaviour, 1 to 5) takes its rounds a pull, ticks
between them and the recovery from port/linux/game/ae_rifle_profiles.c (read from the source, 30 ticks a second).

    pack_measure.py <pack.aepack>...      every weapon of each pack, as JSON
"""
import json
import math
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import aepack
import pack_tags

REPO = os.path.dirname(os.path.dirname(HERE))
PLAYER_SHIELD, PLAYER_BODY = 75.0, 75.0
TICKS_A_SECOND = 30.0
MODEL_UNIT = 0.01              # world units in the AE view table's model unit


def source(relative):
    path = os.path.join(REPO, relative)
    return open(path, errors="replace").read() if os.path.exists(path) else None


# ---------- time to kill

def shots_to_kill(damage, shield_modifier=1.0, body_modifier=1.0, no_spill=False, shield=PLAYER_SHIELD, body=PLAYER_BODY):
    """hits of `damage` (before modifiers) that kill a full shield player, or None for a damage that never does"""
    if damage <= 0 or shield_modifier <= 0 and body_modifier <= 0:
        return None
    shield_left, body_left, hits = shield, body, 0
    while body_left > 1e-6 and hits < 1000:
        hits += 1
        on_shield = damage * shield_modifier
        if shield_left > 1e-6:
            if on_shield < shield_left:
                shield_left -= on_shield
                continue
            spill = 0.0 if no_spill or shield_modifier <= 0 else (on_shield - shield_left) / shield_modifier
            shield_left = 0.0
            body_left -= spill * body_modifier
        else:
            body_left -= damage * body_modifier
    return hits if body_left <= 1e-6 else None


def engine_profile(behaviour, key=None):
    """the AE rifle profile a behaviour (1 to 5) plays, read from ae_rifle_profiles.c, or None; a pack that plays its
    tags' behaviour (0) plays the profile its weapon key names there (ae_rifle_kind_of_weapon: a `#define <NAME>_KEY
    "<key>"`, its row "<NAME>" with spaces: the HJ MAGNUM, the TACTICAL SMG)"""
    text = source("port/linux/game/ae_rifle_profiles.c")
    if text is None:
        return None
    by_key = None
    if behaviour == 0 and key:
        named = [name for name, value in re.findall(r'#define\s+(\w+)_KEY\s+"([^"]*)"', text) if value == key]
        if not named:
            return None
        by_key = named[0].replace("_", " ")
    elif behaviour not in (1, 2, 3, 4, 5):
        return None
    defines = dict(re.findall(r"#define\s+(\w+)\s+(.+)", text))

    def number(token):
        token = token.strip()
        for _ in range(4):
            for name, value in defines.items():
                token = re.sub(r"\b%s\b" % name, "(%s)" % value, token)
        token = re.sub(r"(\d)f\b", r"\1", token)
        return eval(token, {"__builtins__": {}}, {})
    match = re.search(r"profiles\[AE_RIFLE_KINDS\]\s*=\s*\{(.*?)\n\};", text, re.S)
    if not match:
        return None
    body = re.sub(r"/\*.*?\*/", "", match.group(1), flags=re.S)
    rows = re.findall(r'\{\s*"([^"]+)",(.*?)\},?\s*(?=\{\s*"|$)', body, re.S)
    if by_key is not None:
        found = [row for row in rows if row[0] == by_key]
        if not found:
            return None
        name, rest = found[0]
    else:
        index = behaviour - 1
        if index >= len(rows):
            return None
        name, rest = rows[index]
    fields = [part.strip() for part in re.sub(r"/\*.*?\*/", "", rest, flags=re.S).replace("\n", " ").split(",") if part.strip()]
    try:
        return {"name": name, "rounds_a_pull": int(number(fields[1])), "round_ticks": int(number(fields[2])),
                "recovery_ticks": int(number(fields[4])), "damage": number(fields[5]), "damage_slowed": number(fields[6]),
                "shield_modifier": number(fields[7]), "body_modifier": number(fields[8]),
                "error_upper_degrees": number(fields[18]), "tags_rounds": fields[-3] == "TRUE", "no_spill": fields[-2] == "TRUE"}
    except (ValueError, IndexError, SyntaxError, NameError, TypeError):
        return None


def time_to_kill(measure):
    """{"shots", "seconds", "how"} for a weapon's first trigger, or {"unreadable": why}"""
    triggers = measure["triggers"]
    if not triggers:
        return {"unreadable": "no trigger"}
    trigger = triggers[0]
    projectile = measure["projectile"]
    profile = engine_profile(measure["behaviour"], measure.get("key"))
    if profile is not None and not profile["tags_rounds"]:
        per_round, shield_modifier, body_modifier = profile["damage"], profile["shield_modifier"], profile["body_modifier"]
        rounds = shots_to_kill(per_round, shield_modifier, body_modifier, profile["no_spill"])
        if rounds is None:
            return {"unreadable": "the profile's rounds never kill"}
        pulls = math.ceil(rounds / profile["rounds_a_pull"])
        spacing = ((profile["rounds_a_pull"] - 1) * profile["round_ticks"] + profile["recovery_ticks"] + 2) / TICKS_A_SECOND
        last = rounds - (pulls - 1) * profile["rounds_a_pull"]
        seconds = (pulls - 1) * spacing + (last - 1) * profile["round_ticks"] / TICKS_A_SECOND
        return {"shots": rounds, "seconds": round(seconds, 3), "per_round": per_round,
                "how": "the engine profile %s: %d rounds a pull, %d ticks apart, %d ticks recovery" % (
                    profile["name"], profile["rounds_a_pull"], profile["round_ticks"], profile["recovery_ticks"])}
    paced = profile if profile is not None and profile["tags_rounds"] else None
    damage = (projectile or {}).get("impact_damage") or (projectile or {}).get("explosion_damage")
    if damage is None:
        return {"unreadable": "no damage effect on the projectile"}
    mean = (damage["lower"] + damage["upper"]) / 2.0
    pellets = max(1, trigger["projectiles_per_shot"])
    shield_modifier = damage["all_modifiers"].get("cyborg_energy_shield", 1.0)
    body_modifier = damage["all_modifiers"].get("cyborg_armor", 1.0)
    # pellets are separate hits of the same damage
    pellet_hits = shots_to_kill(mean, shield_modifier, body_modifier)
    if pellet_hits is None:
        return {"unreadable": "the damage never kills"}
    shots = math.ceil(pellet_hits / pellets)
    rate = trigger["rate_final"]
    seconds = (shots - 1) / rate if rate > 0 else None
    if paced is not None:
        pulls = math.ceil(shots / paced["rounds_a_pull"])
        spacing = ((paced["rounds_a_pull"] - 1) * paced["round_ticks"] + paced["recovery_ticks"] + 3) / TICKS_A_SECOND
        last = shots - (pulls - 1) * paced["rounds_a_pull"]
        seconds = (pulls - 1) * spacing + (last - 1) * paced["round_ticks"] / TICKS_A_SECOND
    result = {"shots": shots, "per_shot": round(mean * pellets, 3), "pellets": pellets, "rate": rate, "seconds": None if seconds is None else round(seconds, 3),
              "how": ("tags: %.2f x %d pellets, shield x%.2f, body x%.2f, " % (mean, pellets, shield_modifier, body_modifier)) + (
                  "paced by the engine, %d rounds a pull" % paced["rounds_a_pull"] if paced else "%.2f shots a second" % rate)}
    if trigger["charging_time"] > 0 or trigger["charged_time"] > 0:
        result["note"] = "a charging trigger: the charge time is not in the rate"
    return result


# ---------- the back placement

def placements():
    """{name: {"node", "offset", "barrel", "top"}} read from ae_backpack_weapons.c"""
    text = source("port/linux/game/ae_backpack_weapons.c")
    found = {}
    if text is None:
        return found
    for name, node, offset, barrel, top in re.findall(
            r'\{\s*"(\w+)",\s*"([^"]+)",\s*\{([^}]*)\},\s*\{([^}]*)\},\s*\{([^}]*)\}\s*\}', text):
        vector = lambda value: [float(part.strip().rstrip("f")) for part in value.split(",")]
        found[name] = {"node": node, "offset": vector(offset), "barrel": vector(barrel), "top": vector(top)}
    return found


def backpack_kind(tag_path, radius):
    """the kind ae_backpack_rules.c gives a weapon (thigh, long, back) from its tag path and bounding radius"""
    text = source("port/linux/game/ae_backpack_rules.c") or ""
    header = source("port/linux/game/ae_backpack_rules.h") or ""
    small = re.search(r"small_words\[\]\s*=\s*\{(.*?)\};", text, re.S)
    long = re.search(r"long_words\[\]\s*=\s*\{(.*?)\};", text, re.S)
    small_words = re.findall(r'"([^"]+)"', small.group(1)) if small else []
    long_words = re.findall(r'"([^"]+)"', long.group(1)) if long else []
    limit = re.search(r"AE_BACKPACK_SMALL_RADIUS\s+([\d.]+)f", header)
    lowered = tag_path.lower()
    if any(word in lowered for word in small_words):
        return "thigh"
    if any(word in lowered for word in long_words):
        return "long"
    return "thigh" if radius < (float(limit.group(1)) if limit else 0.12) else "back"


def back_reach(kind, bounds):
    """how far a weapon with these bounds (low, high; x forward, y left, z up) reaches toward the body when hung in its
    place: {"place", "reach", "outer"} in world units: reach = the farthest the geometry goes toward the body past the
    weapon's own anchor, as the body frame's x for the back (the chest) and y toward the middle for the thighs"""
    table = placements()
    place = table.get(kind)
    if place is None or bounds is None:
        return None
    barrel, top = place["barrel"], place["top"]
    left = [top[1] * barrel[2] - top[2] * barrel[1], top[2] * barrel[0] - top[0] * barrel[2], top[0] * barrel[1] - top[1] * barrel[0]]
    low, high = bounds
    axis = 0 if kind in ("back", "long", "back2") else 1
    inward = 1.0 if kind != "thigh" else 1.0
    values = []
    for x in (low[0], high[0]):
        for y in (low[1], high[1]):
            for z in (low[2], high[2]):
                values.append(x * barrel[axis] + y * left[axis] + z * top[axis])
    return {"place": kind, "reach": round(place["offset"][axis] + max(values), 4), "outer": round(place["offset"][axis] + min(values), 4),
            "anchor": round(place["offset"][axis], 4)}


# ---------- the pieces of a design check

def first_person_idle(pack, root, fp, fp_bounds):
    """the first-person model in its idle pose, in the AE view table's model unit (a hundredth of a world unit): the model's
    rear (lowest x) and bottom (lowest z) after the idle animation's root node has moved it, or the rest pose with
    "idle": None and a reason when the animation cannot be read. Only the root node's idle translation is applied: the
    other nodes' idle moves (a bolt, a magazine) are not"""
    if fp is None or fp_bounds is None:
        return None
    low, high = fp_bounds
    delta, idle, why = [0.0, 0.0, 0.0], None, None
    if root["animations"] is not None:
        idle = pack_tags.idle_root(pack, root["animations"])
        if "translation" in idle:
            rest = fp["nodes"][0]["position"] if fp["nodes"] else [0.0, 0.0, 0.0]
            delta = [idle["translation"][axis] - rest[axis] for axis in range(3)]
        else:
            why, idle = idle["unreadable"], None
    else:
        why = "no animation graph"
    unit = 1.0 / MODEL_UNIT
    return {"rear": round((low[0] + delta[0]) * unit, 2), "front": round((high[0] + delta[0]) * unit, 2),
            "bottom": round((low[2] + delta[2]) * unit, 2), "top": round((high[2] + delta[2]) * unit, 2),
            "rest_rear": round(low[0] * unit, 2), "idle_delta": [round(value * unit, 2) for value in delta],
            "idle": None if idle is None else idle["animation"], "idle_unread": why}


def reticle_while_zoomed(hud):
    """whether the weapon HUD has an aim crosshair that draws while zoomed: an aim-state element with a bitmap whose item
    is not flagged "only on default zoom". (A zoom-state element is the scope's picture, not a crosshair.) None when the
    pack has no HUD of its own"""
    if hud is None:
        return None
    drawn = [element for element in hud["crosshairs"] if element["state"] == "aim" and element["bitmap"]
             and any("only_on_default_zoom" not in item["flags"] for item in element["items"])]
    return {"draws": bool(drawn), "aim_elements": sum(1 for element in hud["crosshairs"] if element["state"] == "aim"),
            "zoom_elements": sum(1 for element in hud["crosshairs"] if element["state"] == "zoom")}


def fire_mode(behaviour, trigger, profile=None):
    """what a pack's trigger does when it is held: "auto" (repeats), "semi" (one round a pull), "burst" (an engine
    profile's burst), "charge" (a charge time), by the tags and the behaviour; profile: the engine profile a behaviour-0
    pack's key names (engine_profile), whose pull fires its rounds a pull"""
    if trigger is None:
        return "none"
    if behaviour == 0 and profile is not None:
        return "burst" if profile["rounds_a_pull"] > 1 else "semi"
    if behaviour in (1, 4):
        return "burst"
    if behaviour in (2, 3):
        return "semi"
    if trigger["charging_time"] > 0 or trigger["charged_time"] > 0:
        return "charge"
    if behaviour == 5:
        return "burst"
    return "semi" if "latched" in trigger["flags"] else "auto"


def dropped_item(root, tp_bounds):
    """how a dropped weapon rests: its model's extents (model unit), the long axis, and whether it has a collision model of
    its own (the weapon lies on the sides of its model's bounds when there is none)"""
    if tp_bounds is None:
        return None
    unit = 1.0 / MODEL_UNIT
    extent = [round((tp_bounds[1][axis] - tp_bounds[0][axis]) * unit, 1) for axis in range(3)]
    return {"extent": extent, "long_axis": "xyz"[extent.index(max(extent))], "own_collision": bool(root["collision"] and root["collision"]["own"]),
            "has_physics": bool(root["physics"])}


def spread_degrees(measure):
    """the widest spread a round has: the engine profile's upper bound (a profile that paces the rounds) or the trigger's"""
    profile = engine_profile(measure["behaviour"], measure.get("key"))
    if profile is not None and not profile["tags_rounds"]:
        return profile["error_upper_degrees"]
    trigger = measure["triggers"][0] if measure["triggers"] else None
    return None if trigger is None else round(math.degrees(trigger["error_upper"]), 2)


# ---------- one weapon

def measure_weapon(pack, weapon, image):
    key = pack.key(weapon)
    root = pack_tags.read_weapon(pack, weapon[1])
    projectile = None
    for trigger in root["triggers"][:1]:
        if trigger["projectile"] is not None:
            projectile = pack_tags.read_projectile(pack, trigger["projectile"])
    fp = pack_tags.read_model(pack, root["model_fp"])
    tp = pack_tags.read_model(pack, root["model_3p"])
    hud = pack_tags.read_hud(pack, root["hud"])
    fp_bounds = pack_tags.model_bounds(fp)
    tp_bounds = pack_tags.model_bounds(tp)
    root_tag = pack.tag(weapon[1])
    physics = pack_tags.read_physics(pack, root["physics"]["index"]) if root["physics"] and root["physics"]["own"] else None
    sounds = pack_tags.firing_sounds(pack, weapon[1], image)
    tails = [(p["tail_seconds"], p["seconds"]) for permutations in sounds["sounds"].values() if isinstance(permutations, list)
             for p in permutations if "tail_seconds" in p]
    measure = {
        "key": key, "behaviour": weapon[3], "root_name": root_tag.name, "weapon": root, "triggers": root["triggers"],
        "projectile": projectile, "hud": hud,
        "fp": None if fp is None else {"name": fp["name"], "bounds": fp_bounds, "parts": [
            {"shader": p["shader"], "low": p["low"], "high": p["high"]} for p in fp["parts"]], "nodes": fp["nodes"][:2]},
        "tp": None if tp is None else {"name": tp["name"], "bounds": tp_bounds},
        "physics": physics, "firing_sounds": sounds,
        "sound_tail_seconds": max((t for t, _ in tails), default=None), "sound_seconds": max((s for _, s in tails), default=None),
    }
    measure["ttk"] = time_to_kill(measure)
    measure["idle"] = first_person_idle(pack, root, fp, fp_bounds)
    measure["reticle_zoomed"] = reticle_while_zoomed(hud)
    measure["fire_mode"] = fire_mode(weapon[3], root["triggers"][0] if root["triggers"] else None,
                                     engine_profile(weapon[3], key) if weapon[3] == 0 else None)
    measure["dropped"] = dropped_item(root, tp_bounds)
    measure["spread_degrees"] = spread_degrees(measure)
    kind = backpack_kind(root_tag.name, root["bounding_radius"])
    measure["back"] = back_reach(kind, tp_bounds)
    return measure


def measure(image):
    pack = pack_tags.WeaponPack(image)
    return [measure_weapon(pack, weapon, image) for weapon in pack.parsed.weapons]


def main(arguments):
    if not arguments:
        print(__doc__.strip().splitlines()[-1], file=sys.stderr)
        return 2
    for path in arguments:
        print(json.dumps({"pack": path, "weapons": measure(open(path, "rb").read())}, default=str))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
