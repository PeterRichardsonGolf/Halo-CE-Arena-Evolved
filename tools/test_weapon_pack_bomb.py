"""The SnD bomb pack's recipe, tools/weapon_pack/bomb/recipe.json: the recipe's own rules, no stock file in the
repository (the art is baked from the player's own game at build time), the pack's names the game looks up
(port/linux/game/ae_bomb_pack.c, ae_bomb_feedback.c), the blast looks kept (A and C), the pins. Everywhere (CI): no
game file is read; a run of the recipe is the box's (make_bomb.py)."""
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
TOOLS = ROOT / "tools" / "weapon_pack"
sys.path.insert(0, str(TOOLS))

import make_pack  # noqa: E402

FOLDER = TOOLS / "bomb"
GAME = ROOT / "port" / "linux" / "game"


def recipe():
    return json.loads((FOLDER / "recipe.json").read_text())


def test_recipe_pins_and_no_stock_file_in_the_repository():
    text = (FOLDER / "recipe.json").read_text()
    assert not re.search("/" + "home" + r"/|~/|\.\./", text)
    data = recipe()
    assert data["source"]["allowlist"] == sorted(set(data["source"]["allowlist"]))
    assert not [name for name in data["source"]["allowlist"] if name.endswith((".vehicle", ".biped", ".weapon"))]
    for key in ("donor_sha256", "pack_sha256", "pack_own_sha256"):
        assert re.fullmatch(r"[0-9a-f]{64}", data["expected"][key])
    assert data["expected"]["pack_size"] < 16 * 1024 * 1024
    # (the repository holds our own scripts and geometry only: no tag, map, picture or sound of the game's)
    assert not [path for path in FOLDER.rglob("*") if path.suffix in (".bitmap", ".map", ".png", ".wav", ".tga",
                                                                        ".gbxmodel", ".model", ".sound", ".jms")]
    for digest in list(data["inputs"]["hek"].values()) + list(data["inputs"]["tsce"].values()):
        assert re.fullmatch(r"[0-9a-f]{64}", digest)
    assert set(data["pack"]["stock_derived"]) .isdisjoint(data["pack"]["own_art"])


def test_names_the_game_looks_up():
    data = recipe()
    pack = data["pack"]
    assert (pack["id"], pack["prefix"], pack["display"]) == ("ae_bomb", "ae\\packs\\bomb\\", "SND BOMB")
    assert '#define AE_BOMB_PACK_ID "ae_bomb"' in (GAME / "ae_bomb_pack.h").read_text()
    arguments = make_pack.builder_arguments(data, "donor.map", "names.txt", "out.aepack", FOLDER)
    assert "--weapon" not in arguments and arguments.count("--root") == len(pack["roots"])
    game = (GAME / "ae_bomb_pack.c").read_text() + (GAME / "ae_bomb_feedback.c").read_text()
    names = set(re.findall(r'"(ae\\\\packs\\\\bomb\\\\[^"]+)"', game))
    names = {name.replace("\\\\", "\\") for name in names}
    shaders = {name.rsplit("\\", 1)[1] for name in names if "\\shaders\\" in name}
    assert shaders == {"bomb_bar_a", "bomb_bar_b", "bomb_bar_c", "bomb_bar_d", "bomb_beacon"}
    roots = {root.split(":", 1)[1] for root in pack["roots"]}
    # (the model, the pickup sound and the blast looks the pack holds: each a root; the shaders and the LED bitmap
    # are reached through the model and are in the pack's own tags)
    assert "ae_pack\\bomb\\bomb" in roots and "ae_pack\\bomb\\sounds\\pickup" in roots
    own = {"ae\\packs\\bomb\\bomb\\shaders\\" + name for name in shaders} | {"ae\\packs\\bomb\\bomb\\bitmaps\\bomb_led"}
    assert own <= set(pack["stock_derived"]) | set(pack["own_art"])


def test_blast_looks_a_and_c_only_no_damage_of_its_own():
    data = recipe()
    attachments = data["donor"]["attachments"]
    assert [name.rsplit("/", 1)[1] for name in attachments] == ["scorpion_cannon_shell_explosion.effect",
                                                                "warthog_chaingun_body_depleted.effect"]
    assert not [name for name in data["source"]["allowlist"] if "rocket_launcher_super" in name]
    assert set(data["blast"]["edits"]) == set(attachments)
    assert all(root.split(":", 1)[0] in ("mode", "effe", "snd!") for root in data["pack"]["roots"])
    credits = (FOLDER / data["pack"]["credits"]).read_text()
    assert "CMT" in credits and "Silent Cartographer: Evolved" in credits and "Digsite" in credits
    assert "stock-derived" in credits
