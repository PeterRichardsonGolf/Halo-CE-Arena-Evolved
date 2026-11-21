"""The shipped pack set is complete: tools/weapon_pack/SHIPPED.txt names every pack a release carries, the release
kit's build-assets reads it, and these checks make a new pack slot impossible to ship without its line.

No build needed: the checks read the manifest, the recipes and ae_weapon_slots.c's slot table.
"""
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
PACKS = ROOT / "tools" / "weapon_pack"
SHIPPED = PACKS / "SHIPPED.txt"
REGISTRY = ROOT / "port" / "linux" / "game" / "ae_weapon_registry.inc"
# packs a release carries beyond the weapon slots
EXTRAS = {"vehicle_damage", "bomb"}


def shipped():
    lines = [l.strip() for l in SHIPPED.read_text().splitlines() if l.strip() and not l.startswith("#")]
    return lines


def slot_folders():
    """every weapon's recipe folder from the weapon registry (ae_weapon_registry.inc, the slots' source since the weapon
    cache's id API): a row's name, when tools/weapon_pack has a recipe folder of that name. Slot 0 rows are left out, as the
    old slots table's no-slot row was: the Protomag (starter_pistol) is a start weapon, not a WEAPON PACKS slot"""
    text = REGISTRY.read_text(encoding="latin-1")
    rows = re.findall(r'^AE_WEAPON_REGISTRY_ROW\(\d+, "([a-z0-9_]+)", "[^"]*", (\d+),', text, re.M)
    return [name for name, slot in rows if int(slot) != 0 and (PACKS / name).is_dir()]


def test_every_slot_is_shipped():
    missing = [f for f in slot_folders() if f not in shipped()]
    assert not missing, f"slots without a SHIPPED.txt line (a release would leave their packs out): {missing}"


def test_the_extras_are_shipped():
    missing = sorted(EXTRAS - set(shipped()))
    assert not missing, f"packs beyond the slots missing from SHIPPED.txt: {missing}"


def test_no_duplicates_and_every_line_is_a_recipe():
    lines = shipped()
    assert len(lines) == len(set(lines)), "SHIPPED.txt names a pack twice"
    for line in lines:
        recipe = PACKS / line / "recipe.json"
        assert recipe.is_file(), f"SHIPPED.txt names {line}, which has no recipe.json"


def test_every_shipped_recipe_has_its_pins():
    for line in shipped():
        data = json.loads((PACKS / line / "recipe.json").read_text())
        assert data.get("pack", {}).get("id"), f"{line}: recipe.json has no pack id"
        text = (PACKS / line / "recipe.json").read_text()
        hashes = re.findall(r'"pack_sha256": "([0-9a-f]*)"', text)
        assert hashes and hashes[-1], f"{line}: no pack_sha256 pin (the kit cannot verify its pack)"
