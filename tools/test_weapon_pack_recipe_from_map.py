"""tools/weapon_pack/recipe_from_map.py: a map-source recipe from one tag of a map. The unit tests run on fake extract
listings (no game file, no Invader); the integration test runs where AE_TEST_INVADER (Invader's build folder),
AE_TEST_MAP (the silenced rifle's source map, the recipe's own file), AE_TEST_STOCK_NAMES (ae_pack_build --list-names
of the stock maps), AE_TEST_BUILDER (ae_pack_build) and AE_TEST_WORK are given."""
import json
import os
import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parent.parent
TOOLS = ROOT / "tools" / "weapon_pack"
sys.path.insert(0, str(TOOLS))

import recipe_from_map as rfm  # noqa: E402

WEAPON = "weapons/ma5s/ma5s.weapon"
# what `invader-extract -r` wrote for the weapon, stock tags among them
LISTING = [
    WEAPON, "weapons/ma5s/ma5s.model", "weapons/ma5s/fp/fp.model", "weapons/ma5s/sounds/fire.sound",
    "weapons/ma5s/shaders/silencer.shader_model", "weapons/ma5s/bitmaps/silencer.bitmap",
    "weapons/ma5s/bitmaps/silencer_multi.bitmap",
    # the stock rifle's, which the map names too
    "weapons/assault rifle/bullet.projectile", "weapons/assault rifle/bullet.damage_effect",
    "sound/sfx/impulse/impacts/metalhit.sound",
]
STOCK = {("proj", "weapons\\assault rifle\\bullet"), ("jpt!", "weapons\\assault rifle\\bullet"),
         ("snd!", "sound\\sfx\\impulse\\impacts\\metalhit")}


def fake_reader(values=None):
    values = values or {}
    return lambda tag, field: values.get((tag, field), "")


def compose(listing=LISTING, version=7, reader=None, tag=WEAPON, **more):
    return rfm.compose(tag, "weapon", "ae_ma5s_test", "TEST RIFLE", "test.map", "test.map", "ab" * 32, 100, version,
                       listing, STOCK, reader or fake_reader({(WEAPON, "label"): "ar"}), **more)


def test_allowlist_is_extract_output_minus_stock():
    recipe = compose()
    assert recipe["source"]["allowlist"] == sorted(
        [WEAPON, "weapons/ma5s/ma5s.model", "weapons/ma5s/fp/fp.model", "weapons/ma5s/sounds/fire.sound",
         "weapons/ma5s/shaders/silencer.shader_model", "weapons/ma5s/bitmaps/silencer.bitmap",
         "weapons/ma5s/bitmaps/silencer_multi.bitmap"])


def test_stock_tags_stay_external():
    """the stock rifle's bullet, its damage effect and a stock impact sound are not the pack's own: in no allowlist,
    no verbatim list and no step"""
    recipe = compose()
    stock_tags = {"weapons/assault rifle/bullet.projectile", "weapons/assault rifle/bullet.damage_effect",
                  "sound/sfx/impulse/impacts/metalhit.sound"}
    assert not stock_tags & set(recipe["source"]["allowlist"])
    assert not stock_tags & set(recipe["source"]["verbatim"])
    assert not stock_tags & {step["tag"] for step in recipe["steps"]}
    assert not stock_tags & {step["from"][1] for step in recipe["steps"] if step["from"][0] == "source"}


def test_the_item_itself_stock_refused():
    with pytest.raises(rfm.Refused, match="stock"):
        compose(listing=["weapons/assault rifle/bullet.projectile"], tag="weapons/assault rifle/bullet.projectile")


@pytest.mark.parametrize("name", ["<protected>/x.weapon", "weapons/<protected>.bitmap", "protected_17.bitmap", "weapons/.model"])
def test_protected_map_refused(name):
    with pytest.raises(rfm.Refused, match="stripped or protected"):
        compose(listing=LISTING + [name])


def test_a_name_given_twice_refused():
    with pytest.raises(rfm.Refused, match="two tags named"):
        compose(listing=LISTING + ["WEAPONS/ma5s/ma5s.weapon"])


def test_missing_tag_refused():
    with pytest.raises(rfm.Refused, match="no tag"):
        compose(tag="weapons/nothing/nothing.weapon")


def test_gbxmodel_gets_model_step():
    """a Custom Edition map (version 609): each gbxmodel is converted to the Xbox model, the weapon's references follow,
    and what no step reads is verbatim"""
    listing = [WEAPON, "weapons/ma5s/ma5s.gbxmodel", "weapons/ma5s/fp/fp.gbxmodel", "weapons/ma5s/shaders/s.shader_model",
               "weapons/ma5s/bitmaps/s_multi.bitmap", "weapons/ma5s/bitmaps/s.bitmap"]
    reader = fake_reader({(WEAPON, "label"): "ar", (WEAPON, "model"): "weapons/ma5s/ma5s.gbxmodel",
                          (WEAPON, "first_person_model"): "weapons/ma5s/fp/fp.gbxmodel",
                          ("weapons/ma5s/shaders/s.shader_model", "multipurpose_map"): "weapons/ma5s/bitmaps/s_multi.bitmap"})
    recipe = compose(listing=listing, version=609, reader=reader)
    steps = {(step["op"], step["tag"]): step for step in recipe["steps"]}
    assert steps[("model", "weapons/ma5s/ma5s.model")]["from"] == ["source", "weapons/ma5s/ma5s.gbxmodel"]
    assert steps[("model", "weapons/ma5s/fp/fp.model")]["from"] == ["source", "weapons/ma5s/fp/fp.gbxmodel"]
    assert ("multipurpose", "weapons/ma5s/bitmaps/s_multi.bitmap") in steps
    assert steps[("edit", WEAPON)]["edits"] == [["set", "model", "weapons/ma5s/ma5s.model"],
                                                ["set", "first_person_model", "weapons/ma5s/fp/fp.model"]]
    assert recipe["source"]["verbatim"] == ["weapons/ma5s/bitmaps/s.bitmap", "weapons/ma5s/shaders/s.shader_model"]


def test_xbox_map_has_no_conversion_steps():
    recipe = compose(version=7)
    assert [step["op"] for step in recipe["steps"]] == ["edit", "edit"]
    assert set(recipe["source"]["verbatim"]) == set(recipe["source"]["allowlist"])


def test_recipe_fields():
    recipe = compose()
    pack = recipe["pack"]
    assert pack["id"] == "ae_ma5s_test" and pack["revision"] == 1 and pack["display"] == "TEST RIFLE"
    assert pack["prefix"] == "ae\\packs\\ma5s_test\\" and pack["weapon"] == "weapons\\ma5s\\ma5s"
    assert pack["weapon_key"] == "ma5s_test" and pack["labels"] == "ar" and pack["behaviour"] == 0 and pack["replaces"] == 0
    assert set(recipe["layers"]) == {"fix-ae", "fix-xbox"}
    assert recipe["source"]["kind"] == "map" and recipe["source"]["tags"] == 100
    assert "TO FILL" in recipe["source"]["name"]
    first = recipe["steps"][0]
    assert first["edits"][-1] == ["set", "weapon_palette[0].name", "weapons\\ma5s\\ma5s.weapon"]
    # (a recipe make_pack.py accepts as a recipe: its own rules)
    import make_pack
    make_pack.check_recipe(recipe)


def test_bad_ids_refused():
    for bad in ("ma5s", "ae_", "ae_Bad", "ae_a b"):
        with pytest.raises(rfm.Refused):
            compose().__class__ and rfm.compose(WEAPON, "weapon", bad, "X", "m", "m", "00", 1, 7, LISTING, STOCK, fake_reader())


def test_vehicle_kind_not_yet():
    with pytest.raises(rfm.Refused, match="no such kind"):
        rfm.compose(WEAPON, "vehicle", "ae_x", "X", "m", "m", "00", 1, 7, LISTING, STOCK, fake_reader())


EQUIPMENT = "powerups/ammo/ammo.equipment"
GRENADE = "weapons/frag/frag.equipment"
PROJECTILE = "weapons/frag/frag.projectile"


def test_equipment_recipe_is_tags_only():
    """no weapon entry or texts; the equipment is the root, and the donor's scenario names it in its equipment palette"""
    listing = [EQUIPMENT, "powerups/ammo/ammo.model", "powerups/ammo/bitmaps/a.bitmap", "weapons/assault rifle/bullet.projectile"]
    recipe = rfm.compose(EQUIPMENT, "equipment", "ae_gem_ammo", "AMMO", "m.map", "m.map", "00", 1, 7, listing, STOCK,
                         fake_reader())
    pack = recipe["pack"]
    assert "weapon" not in pack and "weapon_key" not in pack and "texts" not in pack
    assert pack["roots"] == ["eqip:powerups\\ammo\\ammo"]
    assert pack["prefix"] == "ae\\packs\\gem_ammo\\"
    assert recipe["steps"][0]["edits"][-1] == ["set", "equipment_palette[0].name", "powerups\\ammo\\ammo.equipment"]
    assert "weapons/assault rifle/bullet.projectile" not in recipe["source"]["allowlist"]
    import make_pack
    make_pack.check_recipe(recipe)


def test_grenade_recipe_roots():
    """a grenade's roots are its projectile and its equipment; the donor's globals name them as the first grenade"""
    listing = [GRENADE, PROJECTILE, "weapons/frag/frag.model", "weapons/frag/bitmaps/f.bitmap"]
    recipe = rfm.compose(GRENADE, "grenade", "ae_gem_frag", "FRAG", "m.map", "m.map", "00", 1, 7, listing, STOCK,
                         fake_reader(), projectile=PROJECTILE)
    assert recipe["pack"]["roots"] == ["proj:weapons\\frag\\frag", "eqip:weapons\\frag\\frag"]
    assert recipe["steps"][1]["edits"][-2:] == [["set", "grenades[0].equipment", "weapons\\frag\\frag.equipment"],
                                                ["set", "grenades[0].projectile", "weapons\\frag\\frag.projectile"]]
    assert {GRENADE, PROJECTILE} <= set(recipe["source"]["allowlist"])
    with pytest.raises(rfm.Refused, match="projectile"):
        rfm.compose(GRENADE, "grenade", "ae_gem_frag", "FRAG", "m.map", "m.map", "00", 1, 7, listing, STOCK, fake_reader())


def test_output_folder_must_be_empty(tmp_path):
    out = tmp_path / "out"
    out.mkdir()
    (out / "something").write_text("x")
    with pytest.raises(rfm.Refused, match="not empty"):
        rfm.make_recipe(tmp_path / "none.map", WEAPON, "weapon", "ae_x", "X", tmp_path, tmp_path / "names", out)
    assert [p.name for p in out.iterdir()] == ["something"]


def test_stock_names_file(tmp_path):
    path = tmp_path / "names.txt"
    path.write_text("proj\tweapons\\assault rifle\\bullet\nDeLa\tui\\x\n")
    assert rfm.read_stock_names(path) == {("proj", "weapons\\assault rifle\\bullet"), ("DeLa", "ui\\x")}
    assert rfm.stock_key("weapons/assault rifle/bullet.projectile") == ("proj", "weapons\\assault rifle\\bullet")


@pytest.mark.skipif(not all(os.environ.get(name) for name in ("AE_TEST_INVADER", "AE_TEST_MAP", "AE_TEST_STOCK_NAMES")),
                    reason="needs Invader, the silenced rifle's source map and the stock names")
def test_integration_extracts_a_recipe(tmp_path):
    """the generated recipe's allowlist is the weapon's own tags: the stock rifle's bullet and effects are not in it"""
    out = tmp_path / "gen"
    path = rfm.make_recipe(os.environ["AE_TEST_MAP"], WEAPON, "weapon", "ae_ma5s_test", "TEST RIFLE",
                           os.environ["AE_TEST_INVADER"], os.environ["AE_TEST_STOCK_NAMES"], out,
                           os.environ.get("AE_TEST_LIBRARY_PATH"))
    recipe = json.loads(path.read_text())
    allowlist = recipe["source"]["allowlist"]
    assert WEAPON in allowlist and "weapons/assault rifle/bullet.projectile" not in allowlist
    assert recipe["source"]["tags"] == 1840
    assert (out / "texts.txt").is_file() and (out / "CREDITS.txt").is_file() and not (tmp_path / "gen.scratch").exists()


# ---------- a map's whole sandbox

class FakeTags:
    """extracted tags as Invader's editor reads them: {tag: {key: value}} and the blocks' counts"""

    def __init__(self, values, counts):
        self.values, self.counts = values, counts

    def get(self, tag, key):
        return self.values.get(tag, {}).get(key, "")

    def count(self, tag, block):
        return self.counts.get(tag, {}).get(block, 0)


SCENARIO = "levels/test/test.scenario"
COLLECTION = "item collections/rifles.item_collection"
GLOBALS = "globals/globals.globals"
SANDBOX_LISTING = [SCENARIO, COLLECTION, "item collections/other.item_collection", GLOBALS]


def sandbox_fixture():
    values = {
        SCENARIO: {"weapon_palette[0].name": "weapons/ma5s/ma5s.weapon", "weapon_palette[1].name": "weapons/pistol/pistol.weapon",
                   "equipment_palette[0].name": "powerups/ammo/ammo.equipment",
                   "netgame_equipment[0].item_collection": "item collections/other.item_collection"},
        COLLECTION: {"permutations[0].item": "weapons/ma5s/ma5s.weapon", "permutations[1].item": "powerups/health/health.equipment",
                     "permutations[2].item": "vehicles/ghost/ghost.vehicle"},
        "item collections/other.item_collection": {"permutations[0].item": "weapons/needler/needler.weapon"},
        GLOBALS: {"grenades[0].equipment": "weapons/frag grenade/frag grenade.equipment",
                  "grenades[0].projectile": "weapons/frag grenade/frag grenade.projectile"},
    }
    counts = {SCENARIO: {"weapon_palette": 2, "equipment_palette": 1, "netgame_equipment": 1},
              COLLECTION: {"permutations": 3}, "item collections/other.item_collection": {"permutations": 1},
              GLOBALS: {"grenades": 1}}
    return FakeTags(values, counts)


def test_list_sandbox_from_fixture_scenario():
    """the palettes' weapons and equipment, the item collections' (the scenario's netgame ones too), the globals'
    grenades; a vehicle in a collection is no item of the first version; each once"""
    tags = sandbox_fixture()
    items = rfm.sandbox_items(SANDBOX_LISTING, tags.get, tags.count)
    assert set(items) == {("weapon", "weapons/ma5s/ma5s.weapon"), ("weapon", "weapons/needler/needler.weapon"),
                          ("weapon", "weapons/pistol/pistol.weapon"), ("equipment", "powerups/ammo/ammo.equipment"),
                          ("equipment", "powerups/health/health.equipment"),
                          ("grenade", "weapons/frag grenade/frag grenade.equipment")}
    assert len(items) == 6


def test_sandbox_of_protected_map_refused():
    tags = sandbox_fixture()
    with pytest.raises(rfm.Refused, match="stripped or protected"):
        rfm.sandbox_items(SANDBOX_LISTING + ["<protected>/x.item_collection"], tags.get, tags.count)


def test_ids_unique():
    tags = ["weapons/a/rifle.weapon", "weapons/b/rifle.weapon", "powerups/rifle/rifle.equipment", "x/Frag Grenade.equipment"]
    ids = rfm.item_ids("Gem Ini", tags)
    assert ids == ["ae_gem_ini_rifle", "ae_gem_ini_rifle_2", "ae_gem_ini_rifle_3", "ae_gem_ini_frag_grenade"]
    assert len(set(ids)) == len(ids)


def test_batch_failure_recorded_and_continues(tmp_path, monkeypatch):
    """one item's make_recipe raises (a missing shader, a bad bitmap): the rest are made and the manifest says why"""
    calls = []

    def fake_make(map_path, tag, kind, item_id, display, invader, stock_names, out, library_path=None):
        calls.append(tag)
        if "bad" in tag:
            raise rfm.Refused("a shader\tthe map lacks\nsecond line")
        Path(out).mkdir(parents=True)
        (Path(out) / "recipe.json").write_text("{}")
        return Path(out) / "recipe.json"

    monkeypatch.setattr(rfm, "make_recipe", fake_make)
    items = [("weapon", "weapons/a/a.weapon"), ("weapon", "weapons/bad/bad.weapon"), ("equipment", "p/e/e.equipment")]
    manifest = rfm.batch(tmp_path / "m.map", tmp_path, tmp_path / "n", tmp_path / "out", "t", items=items)
    rows = [line.split("\t") for line in manifest.read_text().splitlines()]
    assert rows[0] == ["kind", "tag", "id", "status", "reason"]
    assert [row[3] for row in rows[1:]] == ["made", "refused", "made"]
    assert rows[2][4] == "a shader the map lacks second line" and rows[2][2] == "ae_t_bad"
    assert calls == [tag for _, tag in items]
    assert (tmp_path / "out" / "ae_t_a" / "recipe.json").is_file() and not (tmp_path / "out" / "ae_t_bad").exists()


def test_batch_output_folder_must_be_empty(tmp_path):
    (tmp_path / "out").mkdir()
    (tmp_path / "out" / "x").write_text("x")
    with pytest.raises(rfm.Refused, match="not empty"):
        rfm.batch(tmp_path / "m.map", tmp_path, tmp_path / "n", tmp_path / "out", "t", items=[])


# ---------- the review's fixes: stock twins, hostile names, every model reference, a batch that stops

def test_stock_filter_ignores_case_and_separators():
    """a map that spells a stock tag with other case or separators does not make it the pack's own"""
    stock = {("proj", "weapons\\assault rifle\\bullet")}
    assert rfm.stock_key("Weapons/Assault Rifle/BULLET.projectile") in stock
    listing = [WEAPON, "Weapons/Assault Rifle/Bullet.projectile"]
    assert rfm.allowlist_of(listing, stock) == [WEAPON]
    # (the names file, read: lower case, backslashes)
    assert rfm.stock_key("a/B.model") == ("mode", "a\\b")


def test_gbxmodel_matches_the_stock_xbox_model(tmp_path):
    """a Custom Edition map's gbxmodel of a stock model's name is the stock's twin: external, never an own tag"""
    names = tmp_path / "names.txt"
    names.write_text("mode\tweapons\\assault rifle\\assault rifle\nmode\tWEAPONS\\Pistol\\Pistol\n")
    stock = rfm.read_stock_names(names)
    assert rfm.stock_key("weapons/assault rifle/assault rifle.gbxmodel") in stock
    assert rfm.stock_key("weapons/pistol/pistol.gbxmodel") in stock
    listing = [WEAPON, "weapons/assault rifle/assault rifle.gbxmodel", "weapons/ma5s/ma5s.gbxmodel"]
    assert rfm.allowlist_of(listing, stock) == ["weapons/ma5s/ma5s.gbxmodel", WEAPON]
    # (and a model the map holds as a gbxmodel is not a Xbox model of another path)
    assert rfm.stock_key("weapons/ma5s/ma5s.gbxmodel") not in stock


@pytest.mark.parametrize("name", [
    "/etc/passwd", "../x.weapon", "a/../x.weapon", "a/./x.weapon", "a\\b.weapon", "C:/x.weapon", "a//b.weapon",
    "a/b\x00.weapon", "a/b\t.weapon", "a/b\n.weapon", "a/\x7f.weapon", "a/ b /c.weapon", "", ".", ".."])
def test_hostile_names_refused(name):
    assert rfm.name_problem(name)
    with pytest.raises(rfm.Refused):
        compose(listing=LISTING + [name])
    with pytest.raises(rfm.Refused):
        compose(tag=name)


def test_good_names_pass():
    for name in (WEAPON, "weapons/assault rifle/bullet.projectile", "ui/hud/a-b_c.weapon_hud_interface", "a.b/c.d.weapon"):
        assert rfm.name_problem(name) == ""


def test_hostile_sandbox_names_refused():
    tags = sandbox_fixture()
    tags.values[COLLECTION]["permutations[0].item"] = "../../x.weapon"
    with pytest.raises(rfm.Refused, match="\\.\\."):
        rfm.sandbox_items(SANDBOX_LISTING, tags.get, tags.count)
    tags = sandbox_fixture()
    tags.values[GLOBALS]["grenades[0].equipment"] = "a\tb.equipment"
    with pytest.raises(rfm.Refused, match="control"):
        rfm.sandbox_items(SANDBOX_LISTING, tags.get, tags.count)


def test_every_tag_naming_a_gbxmodel_is_rewritten():
    """CE: the projectile, a child object and the weapon each name a gbxmodel: each gets a step, so the donor build (which
    takes no gbxmodel) finds none"""
    projectile, scenery = "weapons/frag/frag.projectile", "scenery/crate/crate.scenery"
    listing = [WEAPON, projectile, scenery, "weapons/ma5s/ma5s.gbxmodel", "weapons/frag/frag.gbxmodel",
               "scenery/crate/crate.gbxmodel"]
    reader = fake_reader({(WEAPON, "model"): "weapons/ma5s/ma5s.gbxmodel", (projectile, "model"): "weapons/frag/frag.gbxmodel",
                          (scenery, "model"): "scenery/crate/crate.gbxmodel"})
    recipe = compose(listing=listing, version=609, reader=reader)
    edits = {step["tag"]: step["edits"] for step in recipe["steps"] if step["op"] == "edit" and step["from"][0] == "source"}
    assert edits[WEAPON] == [["set", "model", "weapons/ma5s/ma5s.model"]]
    assert edits[projectile] == [["set", "model", "weapons/frag/frag.model"]]
    assert edits[scenery] == [["set", "model", "scenery/crate/crate.model"]]
    named = {step["tag"] for step in recipe["steps"] if step["op"] == "model"}
    assert named == {"weapons/ma5s/ma5s.model", "weapons/frag/frag.model", "scenery/crate/crate.model"}
    import make_pack
    make_pack.check_recipe(recipe)


def test_batch_stops_on_an_unexpected_error(tmp_path, monkeypatch):
    """only a Refused is one item's refusal: any other error aborts the batch, the rows so far kept in the manifest"""
    def fake_make(map_path, tag, kind, item_id, display, invader, stock_names, out, library_path=None):
        if "boom" in tag:
            raise RuntimeError("invader crashed")
        Path(out).mkdir(parents=True)
        return Path(out) / "recipe.json"

    monkeypatch.setattr(rfm, "make_recipe", fake_make)
    items = [("weapon", "a/a.weapon"), ("weapon", "a/boom.weapon"), ("weapon", "a/c.weapon")]
    with pytest.raises(RuntimeError, match="invader crashed"):
        rfm.batch(tmp_path / "m.map", tmp_path, tmp_path / "n", tmp_path / "out", "t", items=items)
    rows = (tmp_path / "out" / "manifest.tsv").read_text().splitlines()
    assert len(rows) == 2 and rows[1].split("\t")[3] == "made"


def test_batch_with_nothing_made_exits_nonzero(tmp_path, monkeypatch):
    def refuse(*arguments, **more):
        raise rfm.Refused("stock")

    monkeypatch.setattr(rfm, "make_recipe", refuse)
    manifest = rfm.batch(tmp_path / "m.map", tmp_path, tmp_path / "n", tmp_path / "out", "t",
                         items=[("weapon", "a/a.weapon")])
    assert rfm.made_count(manifest) == 0
    monkeypatch.setattr(rfm, "batch", lambda *a, **k: manifest)
    (tmp_path / "x.txt").write_text("")
    code = rfm.main(["--batch", "--prefix", "t", "--map", str(tmp_path / "x.txt"), "--invader", str(tmp_path),
                     "--stock-names", str(tmp_path / "x.txt"), "--out", str(tmp_path / "out2")])
    assert code == 1


def test_manifest_fields_never_hold_tabs_or_newlines(tmp_path, monkeypatch):
    def fake_make(*arguments, **more):
        raise rfm.Refused("a\tb\nc")

    monkeypatch.setattr(rfm, "make_recipe", fake_make)
    manifest = rfm.batch(tmp_path / "m.map", tmp_path, tmp_path / "n", tmp_path / "out", "t",
                         items=[("weapon", "a/a.weapon")])
    lines = manifest.read_text().splitlines()
    assert len(lines) == 2 and all(len(line.split("\t")) == 5 for line in lines)
