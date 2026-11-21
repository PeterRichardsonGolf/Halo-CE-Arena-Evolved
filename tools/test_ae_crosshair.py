"""CROSSHAIR v3's checks that need no game: the generated shape table is what tools/ae_crosshair_shapes.py makes, and every
weapon the registry can show a reticle for has a row in HUD > RETICLE > WEAPONS (a weapon added to the registry without one would
only ever use the main crosshair: this keeps the list whole)."""
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
GAME = ROOT / "port" / "linux" / "game"


def test_shape_table_is_generated():
    run = subprocess.run([sys.executable, str(ROOT / "tools" / "ae_crosshair_shapes.py"), "--check"], capture_output=True, text=True)
    assert run.returncode == 0, run.stdout + run.stderr


def test_every_active_registry_weapon_has_a_row():
    keys = re.findall(r'^AE_WEAPON_REGISTRY_ROW\(\d+, "([a-z_0-9]+)", "[^"]*", \d+, "active"', (GAME / "ae_weapon_registry.inc").read_text(),
                      re.M)
    rows = (GAME / "ae_settings_rows.c").read_text()
    assert len(keys) >= 13
    missing = [key for key in keys if f'"xw.{key}"' not in rows]
    assert not missing, f"no HUD > RETICLE > WEAPONS row for {missing}"


def test_the_stock_weapons_and_vehicle_guns_are_listed():
    rows = (GAME / "ae_settings_rows.c").read_text()
    for key in ("pistol", "assault_rifle", "shotgun", "sniper_rifle", "rocket_launcher", "plasma_pistol", "plasma_rifle", "needler",
                "flamethrower", "gravity_rifle", "warthog_gun", "scorpion_cannon", "ghost_gun"):
        assert f'"xw.{key}"' in rows, key


def test_the_host_rule_bit_is_the_documented_one():
    """universal_variant.ball_melee bit 2 (0x04), set = off, free of BALL MELEE (0-1), FALL DAMAGE (3), SPAWNS (4-5), the weapon
    slots' extra (6) and the marks through walls (7)"""
    text = (GAME / "ae_crosshair_rules.c").read_text()
    assert "ball_melee & 0x04" in text
    taken = {"ae_spawn_modern.h": "0x03", "ae_fall_damage_rules.h": "0x08", "ae_weapon_slots.h": "0x40u", "ae_mark_rules.h": "0x80"}
    for name, bit in taken.items():
        assert bit in (GAME / name).read_text(), name
