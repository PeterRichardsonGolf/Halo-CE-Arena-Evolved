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
SLOTS_C = ROOT / "port" / "linux" / "game" / "ae_weapon_slots.c"
# packs a release carries beyond the weapon slots
EXTRAS = {"vehicle_damage", "bomb"}


def shipped():
    lines = [l.strip() for l in SHIPPED.read_text().splitlines() if l.strip() and not l.startswith("#")]
    return lines


def slot_folders():
    """every slot's recipe folder from the slots table: its name, or its name less "_pack" (ae_weapon_slots.c)"""
    text = SLOTS_C.read_text(encoding="latin-1")
    table = text[text.index("slots[AE_WEAPON_SLOT_COUNT + 1] ="):]
    table = table[:table.index("};")]
    names = re.findall(r'\{\s*"([a-z0-9_]+)"', table)
    folders = []
    for name in names:
        if name == "none":
            continue  # (slot 0: the table's no-slot row)
        folder = name[: -len("_pack")] if name.endswith("_pack") else name
        folders.append(folder)
    return folders


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
