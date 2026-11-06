"""THE AE SMOKE GRENADE (port/linux/game/ae_smoke_grenade.c, ae_smoke_volume.c, ae_smoke_rules.h; the rules' own unit
test is port/linux/tests/ae_smoke_rules_test.c, run by tools/test_ae_units.py; the pack's recipe is
tools/weapon_pack/smoke/).

- the pack's recipe: the pack the game names, the cloud's lifetime and fade the same as the volume's (10 s there, 3 s
  fade, gone by 13 s), no stock file in the repository, the pins well formed;
- the hooks: a detonation makes a volume, the bots' sight asks the volume, the update and the map's new-map call;
- (a game, only when $AE_SMOKE_BUILD names a linux64 build and $AE_SMOKE_DATA a data root holding maps, packs and the
  built packs\\ae_smoke.aepack, through xvfb on the machine that plays it): a SNOWBALL FIGHT on Blood Gulch with the
  test override game.smoke_grenade and three bots: the plasma grenade becomes the smoke, a detonation logs a volume
  (centre, hold 300 ticks, gone at 390), a bot's sight through it is refused ("sight refused"), and with the override
  off none of that is logged (stock rules keep Halo CE's plasma grenade).
"""
import json
import os
import re
import shutil
import subprocess
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parent.parent
GAME = ROOT / "port" / "linux" / "game"
FOLDER = ROOT / "tools" / "weapon_pack" / "smoke"


def recipe():
    return json.loads((FOLDER / "recipe.json").read_text())


def rules():
    text = (GAME / "ae_smoke_rules.h").read_text()
    return {name: value for name, value in re.findall(r"#define (AE_SMOKE_\w+) ([^\n]+)", text)}


def test_recipe_is_stock_tags_only_and_pinned():
    text = (FOLDER / "recipe.json").read_text()
    assert not re.search("/" + "home" + r"/|~/|\.\./", text)
    data = recipe()
    assert data["source"]["allowlist"] == [] and data["source"]["verbatim"] == []
    for key in ("donor_sha256", "pack_sha256", "pack_own_sha256"):
        assert re.fullmatch(r"[0-9a-f]{64}", data["expected"][key])
    assert data["expected"]["own_tags"] == 3 and data["expected"]["external_tags"] == 1
    assert data["expected"]["pack_size"] < 64 * 1024
    assert not [path for path in FOLDER.rglob("*") if path.suffix in (".bitmap", ".map", ".png", ".wav", ".tga", ".jms")]
    assert set(data["pack"]["stock_derived"]) == {"ae\\packs\\smoke\\cloud", "ae\\packs\\smoke\\puff",
                                                   "ae\\packs\\smoke\\puff_physics"}
    assert "weapon" not in data["pack"]


def test_names_the_game_looks_up():
    data = recipe()
    pack = data["pack"]
    values = rules()
    assert values["AE_SMOKE_PACK_ID"] == '"%s"' % pack["id"]
    assert values["AE_SMOKE_PACK_PREFIX"].strip('"') == pack["prefix"].replace("\\", "\\\\")
    assert pack["roots"] == ["effe:ae_pack\\cloud"]
    # (the effect's name the game finds is the prefix and the root's name without "ae_pack\")
    assert values["AE_SMOKE_EFFECT_NAME"] == "AE_SMOKE_PACK_PREFIX \"cloud\""


def test_cloud_lifetime_matches_the_volume():
    steps = {step["tag"]: step for step in recipe()["steps"]}
    particle = steps["ae_pack/puff.particle"]["edits"][0][1]
    fields = {key: value for key, value in particle}
    low, high = (float(x) for x in fields["lifespan"].split())
    fade_out = float(fields["fade_out_time"])
    values = rules()
    fade_ticks = int(eval(values["AE_SMOKE_FADE_TICKS"].replace("AE_SMOKE_TICKS_PER_SECOND", "30")))
    life_ticks = int(eval(values["AE_SMOKE_LIFE_TICKS"].replace("AE_SMOKE_HOLD_TICKS", "300").replace(
        "AE_SMOKE_FADE_TICKS", "90")))
    assert fade_out * 30 == fade_ticks
    # (every sprite is gone by the time the volume ends, and the longest is fully there for the hold)
    assert high * 30 <= life_ticks and high - fade_out >= 9.9 and low - fade_out >= 9.5


def test_hooks_are_in_the_files():
    expected = {
        "source/items/projectiles.c": "ae_smoke_detonated(projectile->definition_index",
        "port/linux/game/bots.c": "ae_smoke_blocks_sight(eye, point)",
        "source/game/game_engine.c": "ae_smoke_grenade_update();",
        "source/items/weapons.c": "ae_smoke_grenade_new_map();",
    }
    for path, text in expected.items():
        content = (ROOT / path).read_text()
        assert text in content, path
        # (the line is marked as AE's)
        assert re.search(re.escape(text) + r".*/\* AE hook \*/", content), path


def test_stock_gametypes_never_ask_for_it():
    text = (GAME / "ae_smoke_grenade.c").read_text()
    asked = text[text.index("boolean ae_smoke_asked"):]
    asked = asked[:asked.index("\n}\n")]
    # (a Bomb game with AE SND's flag, or the test override, and nothing else)
    assert "game_engine_bomb" in asked and "ae_bomb_snd_flag" in asked and "game.smoke_grenade" in asked
    assert asked.count("return") == 3


BUILD = os.environ.get("AE_SMOKE_BUILD")
DATA = os.environ.get("AE_SMOKE_DATA")


def play(tmp_path, smoke):
    work = tmp_path / ("on" if smoke else "off")
    data = work / "data"
    (data / "maps").mkdir(parents=True)
    for source in Path(DATA, "maps").iterdir():
        (data / "maps" / source.name).symlink_to(source)
    (data / "packs").mkdir()
    for source in Path(DATA, "packs").iterdir():
        (data / "packs" / source.name).symlink_to(source)
    env = dict(os.environ, SDL_VIDEODRIVER="offscreen", SDL_AUDIO_DRIVER="dummy", HALO_DATA_ROOT=str(data),
               HALO_SAVE_ROOT=str(work / "save"), HALO_HIDDEN_WINDOW="1", HALO_UPDATE_AUTO="false",
               HALO_NET_ONLINE="false", HALO_NET_ADDRESS="127.0.0.78", HALO_ARENA_MENUS="1",
               HALO_NETWORK_TEST="local:bloodgulch", HALO_EXIT_AFTER="60", HALO_MCC_USE="never",
               HALO_NETWORK_TEST_GAMETYPE="SNOWBALL FIGHT", HALO_NETWORK_TEST_START="5", HALO_TEST_INPUT="bot:1",
               HALO_BOT_EVENTS="1", HALO_BOTS="3", HALO_SMOKE_GRENADE="1" if smoke else "0")
    (work / "save").mkdir()
    subprocess.run(["xvfb-run", "-a", "-s", "-screen 0 1280x720x24", str(Path(BUILD) / "halo")], cwd=work, env=env,
                   timeout=180, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=False)
    return (data / "debug.txt").read_text(errors="replace")


@pytest.mark.skipif(not (BUILD and DATA and shutil.which("xvfb-run")), reason="$AE_SMOKE_BUILD and $AE_SMOKE_DATA name a build and a data root")
def test_detonation_makes_a_volume_and_bots_lose_sight(tmp_path):
    log = play(tmp_path, True)
    assert "the plasma grenade is the smoke grenade" in log
    made = re.findall(r"ae smoke: volume \d+ at (\S+) (\S+) (\S+) radius 2\.80 from tick (\d+), holds until (\d+), gone at (\d+)", log)
    assert made, "no detonation made a volume"
    for _, _, _, start, hold, gone in made:
        assert int(hold) - int(start) == 300 and int(gone) - int(start) == 390
    refused = re.findall(r"ae smoke: sight refused through volume at \S+ \S+ \S+, age (\d+) ticks", log)
    assert refused, "no bot's sight was refused"
    assert all(0 <= int(age) < 390 for age in refused)


@pytest.mark.skipif(not (BUILD and DATA and shutil.which("xvfb-run")), reason="$AE_SMOKE_BUILD and $AE_SMOKE_DATA name a build and a data root")
def test_without_the_override_the_plasma_grenade_stays_the_maps(tmp_path):
    log = play(tmp_path, False)
    assert "ae smoke:" not in log
