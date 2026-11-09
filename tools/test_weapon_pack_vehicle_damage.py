"""The vehicle damage pack's recipe, tools/weapon_pack/vehicle_damage/recipe.json: the recipe's own rules (no
prototype, no art), a pack of tags only whose roots are exactly the looks the game finds by name
(port/linux/game/ae_vehicle_pack_rules.h), nothing a network game would send (no vehicle, biped, weapon or
equipment among its tags), no damage of its own in its explosions, the parts naming campaign tags left out, each
edit with its reason, the pins. Everywhere (CI): no game file is read; a run of the recipe is the box's
(make_pack.py, docs/weapon-packs.md)."""
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
TOOLS = ROOT / "tools" / "weapon_pack"
sys.path.insert(0, str(TOOLS))

import make_pack  # noqa: E402

FOLDER = TOOLS / "vehicle_damage"
RULES = ROOT / "port" / "linux" / "game" / "ae_vehicle_pack_rules.h"
# the object groups a network game sends to every machine (network_objects.c NETWORKED_OBJECT_TYPES)
NETWORKED = (".vehicle", ".biped", ".weapon", ".equipment")


def recipe():
    return json.loads((FOLDER / "recipe.json").read_text())


def made_tags(data):
    """every tag the donor's build takes from the recipe: the steps' and the verbatim ones"""
    return {entry["tag"] for entry in data["steps"]} | set(data["source"]["verbatim"])


def test_recipe_rules_allowlist_and_pins():
    assert not re.search("/" + "home" + r"/|~/|\.\./", (FOLDER / "recipe.json").read_text())
    data = recipe()
    make_pack.check_recipe(data)
    assert not any(key in data for key in make_pack.PROTOTYPE_KEYS)
    assert data["art"] == {} and data["art_inputs"] == {} and data["art_drawings"] == {}
    assert set(data["source"]["allowlist"]) == make_pack.recipe_inputs(data)["cmt"] | set(data["source"]["verbatim"])
    assert data["source"]["allowlist"] == sorted(set(data["source"]["allowlist"]))
    stock = {name for entry in data["steps"] for source, name in make_pack.step_inputs(entry) if source == "stock"}
    assert stock == {"levels/test/bloodgulch/bloodgulch.scenario", "globals/globals.globals",
                     "scenery/small beacon/small red beacon.scenery"}
    assert data["pack"]["stock_derived"] == {}
    for key in ("donor_sha256", "pack_sha256", "pack_own_sha256"):
        assert re.fullmatch(r"[0-9a-f]{64}", data["expected"][key])
    assert data["expected"]["pack_size"] < 16 * 1024 * 1024


def test_a_pack_of_tags_only_whose_roots_are_the_games_looks():
    data = recipe()
    pack = data["pack"]
    assert (pack["id"], pack["prefix"], pack["display"]) == ("ae_vehicle_damage", "ae\\packs\\vehicle_damage\\",
                                                             "VEHICLE DAMAGE")
    assert not {"weapon", "weapon_key", "labels", "texts", "icon"} & set(pack)
    arguments = make_pack.builder_arguments(data, "donor.map", "names.txt", "out.aepack", FOLDER)
    assert "--weapon" not in arguments and arguments.count("--root") == len(pack["roots"])
    # (the rules header's look names, each the pack's name of a root: CMT's name without "cmt\")
    header = RULES.read_text()
    assert '#define AE_VEHICLE_PACK_ID "ae_vehicle_damage"' in header
    looks = set(re.findall(r'"(vehicles\\\\[^"]+)"', header))
    roots = {root.split(":", 1)[1] for root in pack["roots"]}
    assert {"cmt\\" + look.replace("\\\\", "\\") for look in looks} == roots
    groups = {root.split(":", 1)[0] for root in pack["roots"]}
    assert groups == {"effe", "scen"}
    # (the wrecks are the roots that are sceneries; every root is in the donor's build)
    assert all(root.split(":", 1)[1].replace("\\", "/") + {"effe": ".effect", "scen": ".scenery"}[root[:4]]
               in made_tags(data) for root in pack["roots"])
    credits = (FOLDER / pack["credits"]).read_text()
    assert "CMT" in credits and "Silent Cartographer: Evolved" in credits and "Evolved team" in credits


def test_nothing_a_network_game_would_send():
    data = recipe()
    assert not [tag for tag in data["source"]["allowlist"] if tag.endswith(NETWORKED)]
    assert not [tag for tag in made_tags(data) if tag.endswith(NETWORKED)]
    depleted = [entry for entry in data["steps"]
                if entry["tag"].endswith("warthog_chaingun_body_depleted.effect")]
    assert len(depleted) == 1 and ["erase", "events[0].parts[1]"] in depleted[0]["edits"]
    assert "wheel" in depleted[0]["why"]


def test_no_damage_of_its_own_and_no_campaign_tags():
    data = recipe()
    allowlist = "\n".join(data["source"]["allowlist"])
    # (the turret's explosion dealt 15-25 around a dying ghost or banshee: the map's explosion deals the damage)
    assert "turret_shade_destroyed_explosion" not in allowlist
    assert [tag for tag in data["source"]["allowlist"] if tag.endswith(".damage_effect")] == [
        "cmt/effects/shared effects/damage_effects/medium explosion shockwave.damage_effect"]
    for word in ("underwater", "panel debris", "bubble flare", "covenant_damaged.sound_looping"):
        assert word not in allowlist, word


def test_every_edit_has_its_reason_and_the_particle_systems_are_stripped():
    data = recipe()
    for entry in data["steps"]:
        if entry["op"] == "edit":
            assert entry.get("why"), entry["tag"]
        if entry["tag"].endswith(".particle_system"):
            assert (entry["op"], entry["group"]) == ("os_strip", "particle_system")
    systems = [tag for tag in data["source"]["allowlist"] if tag.endswith(".particle_system")]
    assert systems and all(any(entry["tag"] == tag and entry["op"] == "os_strip" for entry in data["steps"])
                           for tag in systems)
    # (the warthog wreck: its smoke always on; its lights and antenna gone)
    wreck = [entry for entry in data["steps"] if entry["tag"].endswith("warthog_chaingun_damaged_smoking.scenery")]
    assert len(wreck) == 1
    assert ["set", "attachments[0].primary_scale", "none"] in wreck[0]["edits"]
    assert ["set", "model", "cmt/vehicles/_shared/warthog/_warthog_chaingun/_scenery_damaged/_scenery_damaged.model"] \
        in wreck[0]["edits"]
