"""The weapon pack "done" check's measuring half (docs/weapon-pack-done.md points 15 to 21): pack_tags.py's readers, pack_measure.py's
numbers and pack_done.py's design checks, on synthetic packs and measurements: each check has a passing and a failing input, and none
depends on which registry rows are active today. No game file is read."""
import json
import math
import struct
import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools" / "weapon_pack"))

import aepack  # noqa: E402
import class_reference  # noqa: E402
import pack_done  # noqa: E402
import pack_measure  # noqa: E402
import pack_tags  # noqa: E402

REFERENCE = {
    "tolerance": {"rear": 3.0, "bottom": 3.0, "reach": 0.02, "tail_seconds": 0.25, "spread_degrees": 0.5, "single_member_factor": 4.0},
    "fallback": {"shotgun": "rifle"},
    "classes": {
        "smg": {"members": ["a", "b"], "rear": [-8.0, -6.0], "bottom": [-8.0, -7.0], "reach": {"thigh": [-0.04, -0.02]},
                "tail_seconds": [0.2, 0.3], "spread_degrees": [3.0, 5.0], "ttk_seconds": [0.8, 0.9]},
        "rifle": {"members": ["c", "d"], "rear": [-20.0, -9.0], "bottom": [-12.0, -2.0], "reach": {"back": [-0.07, -0.05]},
                  "tail_seconds": [0.2, 0.8], "spread_degrees": [0.0, 2.0], "ttk_seconds": [2.0, 2.5]},
        "launcher": {"members": ["e"], "rear": [-25.0, -25.0], "bottom": [-10.0, -10.0], "reach": {"back": [-0.05, -0.05]},
                     "tail_seconds": [1.6, 1.6], "spread_degrees": [0.7, 0.7], "ttk_seconds": None},
    },
}


def measure(**changes):
    base = {
        "key": "t", "behaviour": 0, "root_name": "ae\\packs\\t\\weapon", "projectile": None,
        "triggers": [{"flags": ["client_side_only"], "rate_final": 15.0, "charging_time": 0.0, "charged_time": 0.0}],
        "weapon": {"zoom_levels": 0}, "hud": None, "idle": {"rear": -7.0, "bottom": -7.5, "idle": "first-person idle", "idle_unread": None},
        "reticle_zoomed": None, "fire_mode": "auto", "ttk": {"seconds": 0.85, "shots": 13, "how": "tags"},
        "dropped": {"extent": [40.0, 4.0, 10.0], "long_axis": "x", "own_collision": False}, "spread_degrees": 4.0,
        "back": {"place": "thigh", "reach": -0.03}, "sound_tail_seconds": 0.25, "sound_seconds": 0.9,
    }
    base.update(changes)
    return base


def run_checks(point, measured, design, reference=REFERENCE):
    found = []
    pack_done.design_checks(lambda number, name, verdict, detail="": found.append((number, name, verdict, detail)), "t", measured, design, reference)
    return [(name, "skip" if verdict is None else "pass" if verdict else "fail", detail) for number, name, verdict, detail in found if number == point]


def verdicts(point, measured, design, reference=REFERENCE):
    return [verdict for _, verdict, _ in run_checks(point, measured, design, reference)]


SMG = {"class": "smg", "fire_mode": "auto"}


# ---------- 15 first-person placement

def test_15_inside_the_class_range_passes():
    assert verdicts(15, measure(), SMG) == ["pass"]


@pytest.mark.parametrize("idle", [{"rear": -35.0, "bottom": -7.5}, {"rear": -7.0, "bottom": -40.0}, {"rear": -2.0, "bottom": -7.5}])
def test_15_outside_the_range_fails(idle):
    assert verdicts(15, measure(idle={**measure()["idle"], **idle}), SMG) == ["fail"]


def test_15_without_a_design_is_skipped_not_passed():
    result = run_checks(15, measure(), None)
    assert [verdict for _, verdict, _ in result] == ["skip"] and "no design block" in result[0][2]


def test_15_a_class_with_no_shipped_weapon_falls_back_and_says_so():
    result = run_checks(15, measure(idle={"rear": -12.0, "bottom": -5.0, "idle": "x", "idle_unread": None}), {"class": "shotgun"})
    assert [verdict for _, verdict, _ in result] == ["pass"] and "held to the rifle range" in result[0][2]


def test_15_one_shipped_weapon_gets_wider_slack():
    # the launcher class has one member: -25 +- 12 holds -14, the smg range (+- 3) would not
    assert verdicts(15, measure(idle={"rear": -14.0, "bottom": -10.0, "idle": "x", "idle_unread": None}), {"class": "launcher"}) == ["pass"]
    assert verdicts(15, measure(idle={"rear": -2.0, "bottom": -10.0, "idle": "x", "idle_unread": None}), {"class": "launcher"}) == ["fail"]


# ---------- 16 a reticle while zoomed

def test_16_reticle_drawn_while_zoomed_passes_and_missing_fails():
    zooming = {"zoom_levels": 2}
    assert verdicts(16, measure(weapon=zooming, reticle_zoomed={"draws": True, "aim_elements": 2, "zoom_elements": 1}), None) == ["pass"]
    assert verdicts(16, measure(weapon=zooming, reticle_zoomed={"draws": False, "aim_elements": 1, "zoom_elements": 2}), None) == ["fail"]


def test_16_a_weapon_without_zoom_is_skipped():
    assert verdicts(16, measure(), None) == ["skip"]


def hud(items_flags, state="aim", bitmap="b"):
    return {"crosshairs": [{"state": state, "bitmap": bitmap, "items": [{"flags": flags} for flags in items_flags]}]}


def test_reticle_while_zoomed_reads_the_flags():
    assert pack_measure.reticle_while_zoomed(hud([["flashes"]]))["draws"]
    assert pack_measure.reticle_while_zoomed(hud([["only_on_default_zoom"], ["not_on_default_zoom"]]))["draws"]
    assert not pack_measure.reticle_while_zoomed(hud([["only_on_default_zoom"]]))["draws"]
    assert not pack_measure.reticle_while_zoomed(hud([["flashes"]], state="zoom"))["draws"]
    assert not pack_measure.reticle_while_zoomed(hud([["flashes"]], bitmap=None))["draws"]
    assert pack_measure.reticle_while_zoomed(None) is None


# ---------- 17 fire mode

def test_17_matches_or_fails():
    assert verdicts(17, measure(fire_mode="auto"), {"class": "smg", "fire_mode": "auto"}) == ["pass"]
    assert verdicts(17, measure(fire_mode="semi"), {"class": "smg", "fire_mode": "auto"}) == ["fail"]
    assert verdicts(17, measure(), {"class": "smg"}) == ["skip"]


@pytest.mark.parametrize("behaviour, flags, charging, expected", [
    (0, ["client_side_only"], 0.0, "auto"), (0, ["latched"], 0.0, "semi"), (0, [], 0.75, "charge"),
    (1, [], 0.0, "burst"), (4, ["latched"], 0.0, "burst"), (2, [], 0.0, "semi"), (5, [], 0.0, "burst"), (5, [], 0.5, "charge")])
def test_fire_mode_from_the_trigger(behaviour, flags, charging, expected):
    trigger = {"flags": flags, "charging_time": charging, "charged_time": 0.0}
    assert pack_measure.fire_mode(behaviour, trigger) == expected


# ---------- 18 time to kill and spread

def test_18_ttk_inside_and_outside_the_window():
    assert verdicts(18, measure(ttk={"seconds": 2.5, "shots": 5, "how": "t"}), {"class": "smg", "ttk_seconds": [2.2, 3.0]})[0] == "pass"
    assert verdicts(18, measure(ttk={"seconds": 0.57, "shots": 2, "how": "t"}), {"class": "smg", "ttk_seconds": [2.2, 3.0]})[0] == "fail"
    assert verdicts(18, measure(), {"class": "smg"})[0] == "skip"


def test_18_a_declared_window_with_no_computable_time_fails():
    assert verdicts(18, measure(ttk={"unreadable": "no damage effect on the projectile"}), {"class": "smg", "ttk_seconds": [1, 2]})[0] == "fail"


def test_18_spread_in_the_class_passes_and_tighter_than_every_shipped_fails():
    assert verdicts(18, measure(spread_degrees=4.0), SMG)[-1] == "pass"
    assert verdicts(18, measure(spread_degrees=1.2), SMG)[-1] == "fail"
    assert verdicts(18, measure(spread_degrees=0.0), {"class": "launcher"})[-1] == "skip"      # one shipped weapon is not a range


def test_shots_to_kill_spill_and_modifiers():
    assert pack_measure.shots_to_kill(25.0) == 6                    # 150 total: 6 hits of 25
    assert pack_measure.shots_to_kill(30.0, 0.5, 1.0) == 8          # 15 a hit on the shield: 5 hits strip it, 3 of 30 on the body
    assert pack_measure.shots_to_kill(50.0, 1.0, 1.0) == 3            # the breaking hit spills 25 into the body
    assert pack_measure.shots_to_kill(50.0, 1.0, 1.0, no_spill=True) == 4   # a profile that says no spill
    assert pack_measure.shots_to_kill(0.0) is None


def test_time_to_kill_uses_the_rate_and_the_pellets():
    damage = {"lower": 20.0, "upper": 20.0, "all_modifiers": {}}
    base = {"behaviour": 0, "projectile": {"impact_damage": damage},
            "triggers": [{"rate_final": 2.0, "projectiles_per_shot": 1, "charging_time": 0.0, "charged_time": 0.0}]}
    one = pack_measure.time_to_kill(base)
    assert one["shots"] == 8 and one["seconds"] == pytest.approx(3.5)
    many = pack_measure.time_to_kill({**base, "triggers": [{**base["triggers"][0], "projectiles_per_shot": 10}]})
    assert many["shots"] == 1 and many["seconds"] == 0.0
    assert "unreadable" in pack_measure.time_to_kill({**base, "projectile": None})


def test_the_engine_profile_is_read_from_the_source():
    profile = pack_measure.engine_profile(4)
    assert profile["rounds_a_pull"] == 3 and profile["recovery_ticks"] > 0 and profile["damage"] > 0
    assert pack_measure.engine_profile(0) is None


# ---------- 19 the dropped item

def test_19_long_axis_forward_or_own_collision_passes():
    assert verdicts(19, measure(), None) == ["pass"]
    upright = {"extent": [18.0, 38.0, 56.0], "long_axis": "z", "own_collision": False}
    assert verdicts(19, measure(dropped=upright), None) == ["fail"]
    assert verdicts(19, measure(dropped={**upright, "own_collision": True}), None) == ["pass"]
    assert verdicts(19, measure(dropped=None), None) == ["skip"]


def test_dropped_item_finds_the_long_axis():
    bounds = ([-1.0, -0.1, -0.1], [3.0, 0.1, 0.1])
    found = pack_measure.dropped_item({"collision": None, "physics": None}, bounds)
    assert found["long_axis"] == "x" and found["extent"][0] == pytest.approx(400.0)
    assert pack_measure.dropped_item({"collision": None, "physics": None}, ([0, 0, -2.0], [0.1, 0.1, 2.0]))["long_axis"] == "z"


def test_19_the_ground_point_sets_the_rest():
    """with a ground point the tilt off the ground decides: across the long direction passes, along it (a weapon stood
    on end) fails, whatever the extents say"""
    upright = {"extent": [18.0, 38.0, 56.0], "long_axis": "z", "own_collision": False}
    across = {**upright, "ground_point": {"up": [0.0, 0.82, 0.572], "long_direction": [0.0, -0.533, 0.846], "tilt_degrees": 2.7}}
    along = {**upright, "ground_point": {"up": [0.0, -0.533, 0.846], "long_direction": [0.0, -0.533, 0.846], "tilt_degrees": 86.8}}
    assert verdicts(19, measure(dropped=across), None) == ["pass"]
    assert verdicts(19, measure(dropped=along), None) == ["fail"]
    assert verdicts(19, measure(dropped={**along, "own_collision": True}), None) == ["pass"]
    assert verdicts(19, measure(dropped={**across, "ground_point": {**across["ground_point"], "tilt_degrees": 10.5}}), None) == ["fail"]


def hammer_model(marker_rotation):
    """a model shaped as the gravity hammer pack's: node 0 rests turned 90 degrees about its up (its x the model's y), the
    shaft leans 32.2 degrees from the model's up toward its -y, the ground point on node 0"""
    lean = math.radians(32.2)
    shaft = (0.0, -math.sin(lean), math.cos(lean))
    points = [tuple(t * shaft[axis] + (0.01 if axis == 0 and n % 2 else 0.0) for axis in range(3))
              for n, t in enumerate(x / 20.0 - 0.5 for x in range(21))]
    return {"nodes": [{"name": "frame gun", "parent": -1, "position": [0.0, 0.0, 0.0], "rotation": [0.0, 0.0, -0.707107, 0.707107]}],
            "points": points, "markers": [{"name": "ground point", "node": 0, "translation": [0.0, 0.0, 0.0], "rotation": marker_rotation}]}


def test_the_ground_point_is_read_through_its_node_as_the_game_reads_it():
    """the recipe's revision 3 ground point lays the hammer on its side (2.7 degrees); revision 2's, its up along the
    shaft, stood it on end; a model without the marker has no ground point and keeps the extents rule"""
    root = {"collision": None, "physics": None}
    bounds = ([-0.05, -0.3, -0.4], [0.05, 0.3, 0.4])
    revision3 = pack_measure.dropped_item(root, bounds, hammer_model([-0.46242, 0.0, -0.886661, 0.0]))["ground_point"]
    revision2 = pack_measure.dropped_item(root, bounds, hammer_model([0.0, 0.2776, 0.0, 0.9607]))["ground_point"]
    assert revision3["up"] == pytest.approx([0.0, 0.82, 0.572], abs=0.002)
    assert revision3["tilt_degrees"] == pytest.approx(2.7, abs=0.3)
    assert revision2["up"] == pytest.approx([0.0, -0.533, 0.846], abs=0.002)
    assert revision2["tilt_degrees"] > 80.0
    assert verdicts(19, measure(dropped=pack_measure.dropped_item(root, bounds, hammer_model([0.0, 0.2776, 0.0, 0.9607]))), None) == ["fail"]
    plain = pack_measure.dropped_item(root, bounds, {**hammer_model([0, 0, 0, 1]), "markers": []})
    assert plain["ground_point"] is None and plain["long_axis"] == "z"


# ---------- 20 the back placement

def test_20_reach_inside_and_past_the_shipped_weapons():
    assert verdicts(20, measure(back={"place": "thigh", "reach": -0.03}), None) == ["pass"]
    assert verdicts(20, measure(back={"place": "thigh", "reach": 0.085}), None) == ["fail"]
    assert verdicts(20, measure(back=None), None) == ["fail"]
    assert verdicts(20, measure(back={"place": "bomb", "reach": 0.0}), None) == ["skip"]


def test_back_reach_puts_a_long_head_into_the_chest():
    flat = pack_measure.back_reach("back", ([-0.1, -0.01, -0.01], [0.3, 0.01, 0.02]))
    head = pack_measure.back_reach("back", ([-0.1, -0.2, -0.3], [0.07, 0.17, 0.3]))
    assert flat["place"] == "back" and flat["reach"] < head["reach"]
    assert head["reach"] > 0.0 > flat["reach"]


def test_backpack_kind_follows_the_game_rules():
    assert pack_measure.backpack_kind("ae\\packs\\x\\smg\\gun", 0.5) == "thigh"
    assert pack_measure.backpack_kind("weapons\\sniper rifle\\sniper rifle", 0.5) == "long"
    assert pack_measure.backpack_kind("weapons\\hammer\\hammer", 0.35) == "back"
    assert pack_measure.backpack_kind("weapons\\tiny\\tiny", 0.05) == "thigh"

def test_backpack_fit_is_read_from_the_rules_source():
    assert pack_measure.backpack_fit("ae\\packs\\gravity_hammer\\hammer\\gravity_hammer") == {"turn": -32.2, "out": 0.05, "lean": 15.0}
    assert pack_measure.backpack_fit("AE\\Packs\\Gravity_Hammer\\hammer\\gravity_hammer")["out"] == 0.05
    assert pack_measure.backpack_fit("weapons\\gravity hammer\\gravity hammer") is None
    assert pack_measure.backpack_fit("ae\\packs\\battle_rifle\\weapons\\battle_rifle") is None


def test_back_reach_applies_the_fit_as_the_game_does():
    """a hammer-shaped model (its shaft 32.2 degrees off its up, a wide head at the top): placed with no fit its head
    goes into the chest and check 20 fails it; turned onto its up it reaches less; moved out it reaches less again by
    exactly the distance; the lean (about the back's own normal) does not change how far it reaches"""
    lean = math.radians(32.2)
    shaft, side = (0.0, -math.sin(lean), math.cos(lean)), (0.0, math.cos(lean), math.sin(lean))
    points = [tuple(t * shaft[axis] for axis in range(3)) for t in (-0.3, 0.0, 0.25)]
    points += [tuple(0.25 * shaft[axis] + w * side[axis] + d * (1.0 if axis == 0 else 0.0) for axis in range(3))
               for w in (-0.045, 0.045) for d in (-0.12, 0.12)]
    bounds = ([min(p[axis] for p in points) for axis in range(3)], [max(p[axis] for p in points) for axis in range(3)])
    none = pack_measure.back_reach("back", bounds)
    turned = pack_measure.back_reach("back", bounds, {"turn": -32.2, "out": 0.0, "lean": 0.0}, points)
    fitted = pack_measure.back_reach("back", bounds, {"turn": -32.2, "out": 0.05, "lean": 15.0}, points)
    turned_out = pack_measure.back_reach("back", bounds, {"turn": -32.2, "out": 0.05, "lean": 0.0}, points)
    assert none["reach"] > turned["reach"] > fitted["reach"]
    assert turned["reach"] - fitted["reach"] == pytest.approx(0.05, abs=1e-4)
    assert fitted["reach"] == pytest.approx(turned_out["reach"], abs=1e-4)
    reference = {**REFERENCE, "classes": {**REFERENCE["classes"], "rifle": {**REFERENCE["classes"]["rifle"], "reach": {"back": [-0.07, -0.07]}}}}
    assert verdicts(20, measure(back=none), None, reference) == ["fail"]
    assert verdicts(20, measure(back=fitted), None, reference) == ["pass"]



# ---------- 21 the sound tail

def test_21_tail_under_and_over_the_class_limit():
    assert verdicts(21, measure(sound_tail_seconds=0.25), SMG) == ["pass"]
    assert verdicts(21, measure(sound_tail_seconds=0.68), SMG) == ["fail"]
    assert verdicts(21, measure(sound_tail_seconds=None), SMG) == ["skip"]
    assert verdicts(21, measure(), None) == ["skip"]


def silent(samples=1000, rate=22050):
    return samples, rate


def test_adpcm_block_decodes_a_known_ramp():
    block = struct.pack("<hBB", 100, 0, 0) + bytes([0x77] * 32)           # nibble 7: the largest step up
    samples = pack_tags.decode_adpcm_block(block)
    assert len(samples) == 64 and samples[0] == 100 and samples[1] > samples[0] and samples == sorted(samples)
    down = struct.pack("<hBB", 0, 0, 0) + bytes([0xFF] * 32)
    assert pack_tags.decode_adpcm_block(down)[-1] < 0


# ---------- the reference is generated, not typed

def test_the_reference_file_is_the_generators_shape():
    reference = json.load(open(ROOT / "tools" / "weapon_pack" / "class_reference.json"))
    assert set(reference["classes"]) <= set(class_reference.CLASSES)
    for row in reference["classes"].values():
        assert row["members"] and row["rear"][0] <= row["rear"][1] and set(row["reach"])
    assert reference["tolerance"] == class_reference.TOLERANCE


def test_every_active_weapon_has_a_class_or_the_generator_stops():
    registry = {"rows": [{"kind": "weapon", "status": "active", "fragment_weapon_key": "no_such_weapon"}]}
    with pytest.raises(SystemExit):
        class_reference.build(registry, "/nonexistent")


# ---------- the readers on a synthetic pack

def put(buffer, offset, fmt, *values):
    struct.pack_into("<" + fmt, buffer, offset, *values)


def synthetic_pack(latched=False, rate=15.0, lower=12.0, upper=12.0, zoom=0, crosshair_flags=0, projectiles=1):
    """a pack whose weapon has one magazine, one trigger, a projectile with a damage effect, and a HUD with one crosshair"""
    bitmap = aepack.Tag(aepack.group("bitm"), "ae\\packs\\t\\reticle", bytes=b"\0" * 64)
    jpt = bytearray(512 + 4 * 33)
    put(jpt, D_LOWER := 468, "f", lower)
    put(jpt, 472, "f", upper)
    for index in range(33):
        put(jpt, 512 + 4 * index, "f", 1.0)
    damage = aepack.Tag(aepack.group("jpt!"), "ae\\packs\\t\\hit", bytes=bytes(jpt))
    projectile_bytes = bytearray(588)
    projectile = aepack.Tag(aepack.group("proj"), "ae\\packs\\t\\bullet", bytes=bytes(projectile_bytes),
                            fixups=[aepack.Fixup(548, aepack.FIXUP_REFERENCE, 1)])
    hud_bytes = bytearray(380 + 104 + 108)
    put(hud_bytes, 132, "i", 1)
    put(hud_bytes, 380 + 52, "i", 1)
    put(hud_bytes, 380 + 36 + 0, "I", 0)
    put(hud_bytes, 380 + 108 - 108 + 0, "h", 0)                          # element 0: crosshair type aim
    put(hud_bytes, 380 + 104 + 72, "I", crosshair_flags)
    hud = aepack.Tag(aepack.group("wphi"), "ae\\packs\\t\\hud", bytes=bytes(hud_bytes), fixups=[
        aepack.Fixup(136, aepack.FIXUP_POINTER, 380), aepack.Fixup(380 + 36, aepack.FIXUP_REFERENCE, 0),
        aepack.Fixup(380 + 56, aepack.FIXUP_POINTER, 380 + 104)])
    weapon_bytes = bytearray(1288 + 112 + 276)
    put(weapon_bytes, 776 + 210, "h", zoom)
    put(weapon_bytes, 1264, "i", 1)
    put(weapon_bytes, 1276, "i", 1)
    trigger = 1288 + 112
    put(weapon_bytes, trigger + 0, "I", 8 if latched else 0)
    put(weapon_bytes, trigger + 4, "f", rate)
    put(weapon_bytes, trigger + 8, "f", rate)
    put(weapon_bytes, trigger + 34, "h", 1)
    put(weapon_bytes, trigger + 110, "h", projectiles)
    weapon = aepack.Tag(aepack.group("weap"), "ae\\packs\\t\\weapon", bytes=bytes(weapon_bytes), fixups=[
        aepack.Fixup(1268, aepack.FIXUP_POINTER, 1288), aepack.Fixup(1280, aepack.FIXUP_POINTER, trigger),
        aepack.Fixup(trigger + 148, aepack.FIXUP_REFERENCE, 2),
        aepack.Fixup(776 + 340 + 36, aepack.FIXUP_REFERENCE, 3)])
    tag_list = [weapon, damage, projectile, hud, bitmap]
    # (indexes: weapon 0, damage 1, projectile 2, hud 3, bitmap 4)
    weapon.fixups[3] = aepack.Fixup(776 + 340 + 36, aepack.FIXUP_REFERENCE, 3)
    hud.fixups[1] = aepack.Fixup(380 + 36, aepack.FIXUP_REFERENCE, 4)
    record = aepack.Weapon("t", 0, "ar", "T", weapon_pickup_text="Picked up a t", ammo_pickup_text="Picked up %d rounds for t")
    return aepack.build(aepack.Pack("ae_t", 1, "T", tag_list, [record], credits="Thank you, somebody: the art is theirs, " * 4))


def test_the_readers_find_the_trigger_the_damage_and_the_reticle():
    image = synthetic_pack(latched=True, rate=1.5, lower=20.0, upper=30.0, zoom=2, projectiles=3)
    weapon = pack_measure.measure(image)[0]
    assert weapon["weapon"]["zoom_levels"] == 2 and weapon["fire_mode"] == "semi"
    assert weapon["triggers"][0]["rate_final"] == pytest.approx(1.5) and weapon["triggers"][0]["projectiles_per_shot"] == 3
    assert weapon["projectile"]["impact_damage"]["lower"] == 20.0 and weapon["projectile"]["impact_damage"]["upper"] == 30.0
    assert weapon["ttk"]["shots"] == 2 and weapon["ttk"]["pellets"] == 3
    assert weapon["reticle_zoomed"]["draws"] is True


def test_an_automatic_trigger_and_a_flagged_reticle():
    weapon = pack_measure.measure(synthetic_pack(latched=False, crosshair_flags=1 << 6, zoom=1))[0]
    assert weapon["fire_mode"] == "auto" and weapon["reticle_zoomed"]["draws"] is False


def test_pack_done_runs_the_new_checks_on_the_synthetic_pack(tmp_path):
    path = tmp_path / "t.aepack"
    path.write_bytes(synthetic_pack(latched=False, zoom=1, crosshair_flags=1 << 6))
    results = {(point, name.split(": ", 1)[-1]): verdict for point, name, verdict, _ in
               pack_done.check_pack(str(path), design={"class": "smg", "fire_mode": "semi"}, reference=REFERENCE)}
    assert results[(16, "a reticle draws while zoomed")] == "fail"
    assert results[(17, "the trigger fires as the design says (semi)")] == "fail"
    skipped = {(point, name.split(": ", 1)[-1]): verdict for point, name, verdict, _ in pack_done.check_pack(str(path), reference=REFERENCE)}
    assert skipped[(17, "the trigger fires as the design says")] == "skip"
