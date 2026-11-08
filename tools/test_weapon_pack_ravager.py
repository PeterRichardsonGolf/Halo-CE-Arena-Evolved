"""The ravager pack's recipe, tools/weapon_pack/ravager/recipe.json: CMT's prototype ravager from the stock-compatible
TSC:E 1.4 source release, set up as the latest-generation ravager is and tuned for CE (three lobbed, bursting bolts a
pull; hold to charge one big bolt that leaves a burning pool). It shares CMT's spiker model, so it gets its own
look: CMT's spiker textures recoloured by numbers (ravager_art.py) under its own copies of the shaders and models. The
recipe's own rules (no prototype), the allowlist (CMT's tags only), only what an Xbox map needs done to CMT's tags, the
look (its own pictures, shaders and models; the spiker's never bound), the numbers (bolts and pulls to strip and to
kill, the charged bolt and its pool, heat, battery), the engine's paced burst (behaviour 5, the RAVAGER profile), the pad
pulse, the pack's arguments, texts and credits. Everywhere (CI): no game file
is read; a run of the recipe is the box's (make_pack.py --zip, docs/weapon-packs.md)."""
import json
import math
import re
import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parent.parent
TOOLS = ROOT / "tools" / "weapon_pack"
sys.path.insert(0, str(TOOLS))
sys.path.insert(0, str(ROOT / "tools"))

import make_pack  # noqa: E402

FOLDER = TOOLS / "ravager"
SHARED = "cmt/weapons/_shared/spiker/"
BASE = "cmt/weapons/evolved_prototypes/ravager/"
WEAPON = BASE + "ravager.weapon"
PROJECTILE = BASE + "projectiles/ravager_bolt.projectile"
DAMAGE = BASE + "damage_effects/ravager_bolt_impact.damage_effect"
OWN = "ae_pack/ravager/"
TRIGGER = OWN + "bolt_trigger.damage_effect"
SPLASH = OWN + "bolt_splash.damage_effect"
HUD = "cmt/ui/hudv4.1/weapons/evolved_prototypes/ravager.weapon_hud_interface"
# CE's player: 75 shield and 75 body; CE's 30 ticks a second
SHIELD = BODY = 75
TICKS = 30


def recipe():
    return json.loads((FOLDER / "recipe.json").read_text())


def step(tag, layer="fix-ae"):
    return [entry for entry in recipe()["steps"] if entry["tag"] == tag and entry["layer"] == layer][0]


def sets(tag, layer="fix-ae"):
    """the step's field values (its "set" and "sets" edits, in order)"""
    found = {}
    for edit in step(tag, layer)["edits"]:
        if edit[0] == "sets":
            found.update(dict(edit[1]))
        elif edit[0] == "set":
            found[edit[1]] = edit[2]
    return found


def inherited(tag):
    """the step's fields over those of the step it is made from, when that is one of this layer's"""
    entry = step(tag)
    fields = inherited(entry["from"][1]) if entry["from"][0] == "fix-ae" else {}
    fields.update(sets(tag))
    return fields


def hits(damage):
    """(damage at full scale, multiplier on a shield, multiplier on the body) of a damage effect"""
    fields = inherited(damage)
    return (float(fields["damage_upper_bound"].split()[0]), float(fields["cyborg_energy_shield"]),
            float(fields["cyborg_armor"]))


def bolts_to(spill=True, head=False):
    """(bolts that strip a full player's shield, bolts that kill) for direct hits, each a hit then its full splash,
    as CE's damage does it: shield damage x the shield multiplier, the stripping event's excess carried on to the
    body (spill) at the body multiplier, a head hit's body damage x2 (the hit only; a splash has no region)"""
    events = [hits(DAMAGE) + (2.0 if head else 1.0,), hits(SPLASH) + (1.0,)]
    shield, body, strip, bolts = float(SHIELD), float(BODY), None, 0
    while body > 0:
        bolts += 1
        for per, shield_x, body_x, head_x in events:
            if shield > 0:
                excess = per * shield_x - shield
                shield = max(0.0, shield - per * shield_x)
                if shield == 0:
                    strip = strip or bolts
                    if spill and excess > 0:
                        body -= excess / shield_x * body_x * head_x
            else:
                body -= per * body_x * head_x
    return strip, bolts


def test_recipe_rules_allowlist_and_pins():
    assert not re.search("/" + "home" + r"/|~/|\.\./", (FOLDER / "recipe.json").read_text())
    data = recipe()
    make_pack.check_recipe(data)
    assert make_pack.source_kind(data) == "zip"
    assert not any(key in data for key in make_pack.PROTOTYPE_KEYS)
    source = data["source"]
    assert source["file"] == "tsce_1-4_source_release_stock.zip"
    assert source["root"] == "tsce_1-4_source_release_stock/tags/"
    assert source["allowlist"] == sorted(set(source["allowlist"]))
    assert all(name.startswith("cmt/") for name in source["allowlist"]) and WEAPON in source["allowlist"]
    assert not any(name.endswith((".model_collision_geometry", "melee.damage_effect", "melee_response.damage_effect"))
                   for name in source["allowlist"])
    assert data["layers"] == ["fix-nopanel", "fix-ae", "fix-xbox"]
    assert data["pack"]["stock_derived"] == {}
    for key in ("pack_sha256", "pack_own_sha256", "donor_sha256"):
        assert re.fullmatch(r"[0-9a-f]{64}", data["expected"][key])
    for name, entry in data["art"].items():
        assert re.fullmatch(r"[0-9a-f]{64}", entry["pixels_sha256"]), name
    for name, entry in data["art_inputs"].items():
        assert re.fullmatch(r"[0-9a-f]{64}", entry["sha256"]) and entry["tag"] in source["allowlist"], name


def test_only_what_an_xbox_map_needs():
    data = recipe()
    ops = {}
    for entry in data["steps"]:
        if entry["layer"] == "fix-xbox" and not entry["tag"].startswith(OWN):
            ops.setdefault(entry["op"], []).append(entry["tag"])
    assert set(ops) == {"edit", "model", "multipurpose", "hud_hires", "sound"}
    assert sorted(ops["model"]) == [SHARED + "fp/fp.model", SHARED + "spiker.model"]
    assert ops["multipurpose"] == [SHARED + "bitmaps/spiker_clip_multi.bitmap"]
    assert ops["sound"] == ["cmt/sounds/sfx/effects/impacts/flesh_impact.sound"]
    assert HUD in ops["hud_hires"]
    weapon = step(WEAPON, "fix-xbox")
    assert {edit[1] for edit in weapon["edits"]} == {"model", "first_person_model", "pickup_text_index",
                                                       "hud_text_message_index"}
    assert dict((edit[1], edit[2]) for edit in weapon["edits"])["model"] == OWN + "ravager.model"
    # the HUD's weapon picture and message icon: CMT's spiker picture (the ravager's model), not CMT's borrowed 7
    assert step(HUD, "fix-xbox")["edits"] == [["set", "sequence_index", "11"],
                                               ["set", "static_elements[0].sequence_index", "11"]]


def test_its_own_look():
    """the ravager's own pictures (ravager_art.py, one look by name), shaders and models; no shader of the ravager's
    names a spiker picture, and the models bind the ravager's shaders in every slot but CMT's heat shader"""
    data = recipe()
    drawings = data["art_drawings"]
    assert {entry["draw"] for entry in drawings.values()} == {"ravager_diffuse", "ravager_multi"}
    looks = {entry["look"] for entry in drawings.values()}
    assert looks == {"B3bd"}
    import ravager_art
    assert looks <= set(ravager_art.LOOKS)
    shaders = [entry for entry in data["steps"] if entry["tag"].endswith(".shader_model")]
    assert sorted(entry["tag"] for entry in shaders) == sorted(
        OWN + name + ".shader_model" for name in ("ravager_body_shiny", "ravager_body_dull", "ravager_body_blade",
                                                   "ravager_clip"))
    for entry in shaders:
        named = dict((edit[1], edit[2]) for edit in entry["edits"] if edit[0] == "set")
        if "body" in entry["tag"]:
            colours = dict(sum((edit[1] for edit in entry["edits"] if edit[0] == "sets"), []))
            assert colours["animation_color_lower_bound"] == colours["animation_color_upper_bound"] == "0.35 0.95 1"
        assert named["base_map"].startswith(OWN + "ravager_")
        assert "spiker" not in named.get("multipurpose_map", OWN + "ravager_body_multi.bitmap") or "clip" in entry["tag"]
    for tag, slots in ((OWN + "ravager.model", {0, 2, 3, 4}), (OWN + "ravager_fp.model", {0, 1, 3, 4})):
        fields = sets(tag, "fix-xbox")
        assert {int(re.search(r"\[(\d+)\]", key).group(1)) for key in fields} == slots
        assert all(value.startswith(OWN) for value in fields.values())
    bitmaps = [entry for entry in data["steps"] if entry["op"] == "bitmap"]
    assert sorted(entry["tag"] for entry in bitmaps) == [OWN + "ravager_body_diff.bitmap",
                                                         OWN + "ravager_body_multi_pc.bitmap",
                                                         OWN + "ravager_clip_diff.bitmap"]


def test_the_numbers():
    weapon = sets(WEAPON)
    # (the charged bolt's trigger is a copy of the first, made before the fields are set)
    assert step(WEAPON)["edits"][0] == ["copy", "triggers[0]", "end"]
    # (one bolt a shot, no spread: the burst is the engine's, test_the_burst_is_the_engines)
    assert weapon["triggers[0].projectiles_per_shot"] == "1" and weapon["triggers[0].error_angle"] == "0 0"
    assert weapon["triggers[0].maximum_rate_of_fire"] == "7.5 7.5"
    assert (weapon["triggers[0].charging_time"], weapon["triggers[0].charged_time"]) == ("0.75", "45")
    assert weapon["triggers[1].projectile_object"] == "ae_pack\\ravager\\charged_bolt.projectile"
    assert weapon["triggers[1].projectiles_per_shot"] == "1"
    assert (weapon["heat_loss_rate"], weapon["heat_recovery_threshold"]) == ("0.33", "0.015")
    assert (weapon["age_misfire_start"], weapon["age_misfire_chance"]) == ("0", "0")
    assert weapon["zoom_magnification_range"] == "1.4 1.4"
    assert (weapon["autoaim_angle"], weapon["autoaim_range"], weapon["magnetism_angle"],
            weapon["magnetism_range"]) == ("4.5", "20", "6.75", "24")
    assert weapon["player_melee_damage"] == "weapons\\assault rifle\\melee.damage_effect"
    assert weapon["collision_model"] == "weapons\\assault rifle\\assault rifle.model_collision_geometry"
    # a battery of 36 bolts: 12 pulls of 3 (a pull's 0.0833 over its bolts), a charged bolt one bolt's worth
    assert round(1 / float(weapon["triggers[0].age_generated_per_round"])) == 36
    assert round(3 * float(weapon["triggers[0].age_generated_per_round"]), 3) == 0.083
    assert round(1 / float(weapon["triggers[1].age_generated_per_round"])) == 36
    projectile = sets(PROJECTILE)
    assert (projectile["initial_velocity"], projectile["final_velocity"], projectile["air_gravity_scale"]) == (
        "45", "45", "5.2")
    # every material's response is a burst (the splash)
    responses = {key: value for key, value in projectile.items() if key.startswith("projectile_material_response")}
    assert len(responses) == 66 and set(responses.values()) == {"detonate"}
    assert projectile["effect"] == "ae_pack\\ravager\\bolt_splash.effect"
    assert sets(DAMAGE)["damage_flags.can_cause_multiplayer_headshots"] == "1"


@pytest.mark.parametrize("spill", [True, False])
def test_bolts_and_pulls_to_kill(spill):
    """the source's 4 bolts to strip a full shield and 7 to kill: in CE (which spills the stripping bolt's excess
    onto the body) 4 and 7; either way the kill comes in the third pull of 3, as the source's third burst; x2 on the
    head once the shield is down saves a bolt"""
    strip, kill = bolts_to(spill)
    assert strip == 4
    if spill:
        assert kill == 7
    assert math.ceil(kill / 3) == 3
    assert bolts_to(spill, head=True)[1] < kill


def test_the_charged_bolt_and_its_pool():
    """a charged bolt on a full player takes about three quarters of the shield (the source's 105 of 140); its burst
    reaches 2.5 units; then nine burning pulses at the same spot, light on shields (the fire type)"""
    hit, hit_shield, _ = hits(OWN + "charged_impact.damage_effect")
    blast, blast_shield, _ = hits(OWN + "charged_splash.damage_effect")
    assert 0.7 < (hit * hit_shield + blast * blast_shield) / SHIELD < 0.8
    assert sets(OWN + "charged_splash.damage_effect")["radius"] == "0.6 2.5"
    pool = sets(OWN + "charged_pool.effect")
    assert pool["events[0].parts[2].type"] == OWN + "charged_splash.damage_effect"
    assert [pool["events[%d].parts[0].type" % i] for i in range(1, 10)] == [OWN + "pool_burn.damage_effect"] * 9
    burn = sets(OWN + "pool_burn.damage_effect")
    assert float(burn["cyborg_energy_shield"]) < float(burn["cyborg_armor"])
    assert sets(OWN + "charged_bolt.projectile")["effect"] == "ae_pack\\ravager\\charged_pool.effect"


def test_heat_overheats_in_seven_pulls_and_vents_in_three_seconds():
    """a pull's heat (0.38) spread over its three bolts, 4 ticks apart, pulls 23 ticks apart at the quickest: the heat
    bleeds over a burst's 8 ticks, so 7 pulls back to back overheat it (6 did when the three bolts left together)"""
    weapon = sets(WEAPON)
    per_bolt, loss = float(weapon["triggers[0].heat_generated_per_round"]), float(weapon["heat_loss_rate"])
    assert round(3 * per_bolt, 2) == 0.38
    heat, last, pulls = 0.0, 0, 0
    while heat < 1.0:
        pulls += 1
        for bolt in range(3):
            tick = (pulls - 1) * PULL_TICKS + bolt * BOLT_TICKS
            heat = max(0.0, heat - loss / 30 * (tick - last)) + per_bolt
            last = tick
            if heat >= 1.0:
                break
    assert pulls == 7
    assert 2.9 < (1.0 - float(weapon["heat_recovery_threshold"])) / loss < 3.1


PROFILES = ROOT / "port" / "linux" / "game" / "ae_rifle_profiles.c"
# (the RAVAGER profile's pacing: bolts 4 ticks apart, taps 23 apart at the quickest)
BOLT_TICKS, PULL_TICKS = 4, 23


def ravager_profile():
    """the RAVAGER row of ae_rifle_profiles.c's profiles, its values as written"""
    text = PROFILES.read_text(encoding="latin-1")
    row = re.search(r'\{ "RAVAGER",(.*?)\}', text, re.S).group(1)
    return [value.strip() for value in row.split(",")]


def test_the_burst_is_the_engines():
    """the engine paces the burst (behaviour 5, ae_rifle_profiles.c's RAVAGER, played by ae_burst_rifle.c with the tags'
    own rounds): the tags' trigger fires one bolt a shot at the profile's 7.5 a second (30 / its 4 ticks), the tap's
    first on the charging trigger's release; the engine fires the other two and the 12-tick recovery: taps 23 ticks
    apart (0.77 s), 15 (0.5 s) from a burst's last bolt to the next's first; the charged shot stays the second trigger's"""
    data = recipe()
    assert data["pack"]["behaviour"] == 5
    weapon = sets(WEAPON)
    # (name, a rifle, rounds a pull, ticks between its rounds, keeps a pull, recovery, ..., the tags' rounds)
    row = ravager_profile()
    assert row[0:5] == ["FALSE", "3", "4", "FALSE", "12"] and row[-1] == "TRUE"
    assert float(weapon["triggers[0].maximum_rate_of_fire"].split()[0]) == 30 / int(row[2])
    assert (3 - 1) * BOLT_TICKS + 12 + 2 + 1 == PULL_TICKS
    assert PULL_TICKS - (3 - 1) * BOLT_TICKS == 15
    assert 0.76 < PULL_TICKS / 30 < 0.78
    assert float(weapon["triggers[0].charging_time"]) > 0 and weapon["triggers[1].projectiles_per_shot"] == "1"
    # the 7th bolt kills (test_bolts_and_pulls_to_kill): the 3rd tap's first, 46 ticks after the first bolt
    assert 2 * PULL_TICKS == 46


def test_the_pad_pulse():
    trigger = step(TRIGGER)
    assert trigger["from"] == ["cmt", BASE + "damage_effects/ravager_bolt_trigger.damage_effect"]
    fields = sets(TRIGGER)
    assert float(fields["low_frequency_vibrate_frequency"]) > 0 and float(fields["high_frequency_vibrate_frequency"]) > 0
    assert sets(WEAPON)["triggers[0].firing_effects[0].firing_damage"] == TRIGGER.replace("/", "\\")
    charged = sets(OWN + "charged_trigger.damage_effect")
    assert float(charged["low_frequency_vibrate_duration"]) > float(fields["low_frequency_vibrate_duration"])


def test_pack_arguments_texts_and_credits():
    data = recipe()
    pack = data["pack"]
    assert (pack["id"], pack["prefix"], pack["weapon_key"], pack["display"]) == (
        "ae_ravager", "ae\\packs\\ravager\\", "ravager", "RAVAGER")
    assert (pack["labels"], pack["behaviour"], pack["replaces"]) == ("c_bs,ar", 5, 0)
    assert pack["weapon"] == WEAPON[:-len(".weapon")].replace("/", "\\")
    assert pack["icon"] == "cmt\\ui\\hudv4.1\\bitmaps\\weapons\\pickup_icons:11"
    assert pack["roots"] == ["bitm:cmt\\ui\\hudv4.1\\bitmaps\\weapons\\pickup_icons"]
    texts = (FOLDER / pack["texts"]).read_text()
    assert "weapon_pickup=Picked up a ravager\n" in texts
    assert "ammo_pickup=Picked up %d charge for ravager\n" in texts and texts.count("%") == 1
    credits = (FOLDER / pack["credits"]).read_text()
    for name in ("CMT", "Teh Lag", "Ifafudafi", "Siliconmaster", "ravager_art.py"):
        assert name in credits


def test_the_art_script_recolours_by_its_looks():
    """ravager_art.py: paint follows the look's ramp, the shiny parts (the multipurpose picture's specular) turn
    steel, the orange lights take the look's light colour; without a glow the multipurpose picture is CMT's as it
    is, and a glow lights only the self-illumination channel"""
    np = pytest.importorskip("numpy")
    pytest.importorskip("PIL")
    import ravager_art
    o = ravager_art.LOOKS["B3-trim"]
    size = 96
    rgba = np.full((size, size, 4), 0.5, np.float32)
    rgba[..., 3] = 1
    rgba[40:44, 40:44, :3] = (1.0, 0.5, 0.1)               # a self-lit orange dot
    multi = np.zeros((size, size, 4), np.float32)
    multi[..., 3] = 1
    multi[:, 60:, 2] = 1.0                                  # the right side shiny
    body = ravager_art.diffuse(rgba, multi, o)
    assert body.shape == (size, size, 4) and body[..., 3].min() == 255
    paint, steel, dot = body[20, 10, :3].astype(int), body[20, 90, :3].astype(int), body[42, 42, :3].astype(int)
    assert paint[0] > 2 * paint[1]                          # crimson
    assert abs(int(steel[0]) - int(steel[2])) < 20 and steel[0] > paint[1]  # grey steel
    assert dot[2] > dot[0] and dot[1] > dot[0]              # cyan
    same = ravager_art.multipurpose(rgba, multi, o)
    assert (same == (multi * 255 + 0.5).astype(np.uint8)).all()
    glowing = dict(o, glow={"rects": [(0, 0, size, size)], "edge": (0.015, 0.05), "widen": 3,
                            "colour": (0.35, 0.95, 1.0), "amount": 1.0})
    grooved = rgba.copy()
    grooved[20:22, 5:50, :3] = 0.1                          # a dark panel line on the paint
    lit = ravager_art.multipurpose(grooved, multi, glowing)
    assert lit[21, 20, 1] > 128 and (lit[..., [0, 2, 3]] == same[..., [0, 2, 3]]).all()
    with pytest.raises(ravager_art.Refused):
        ravager_art.look("no such look")
