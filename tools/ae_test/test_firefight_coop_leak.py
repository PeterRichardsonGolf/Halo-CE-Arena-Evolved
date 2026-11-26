"""A Firefight game, then a co-op campaign level in the same process: the co-op game is a campaign game (no Firefight
roster gathered, no game engine, no asserts), and the list's Firefight test waits for the campaign's tag pack.

Opt-in: it runs only when $AE_FF_LEAK_BUILD names the game's binary (build/linux64/halo) and $AE_FF_LEAK_MAPS a maps
folder that holds the campaign's maps (b30 and the ones its tag pack is made of) beside the multiplayer ones (bloodgulch);
$AE_FF_LEAK_PACKS (optional) is a tag_pack folder to link in, else the game writes its own. It skips on CI.

    AE_FF_LEAK_BUILD=$PWD/build/linux64/halo AE_FF_LEAK_MAPS=<a maps folder with the campaign's maps> \
        python3 -m pytest -q tools/ae_test/test_firefight_coop_leak.py
"""
import os
import subprocess
import sys
from pathlib import Path

import pytest

LIST = """list ffleak
revision 1
title FIREFIGHT THEN CO-OP

group ff
map bloodgulch
gametype firefight
set game.firefight_pack true

test ff
title firefight
setup god me
setup firefight wave 1
check Covenant arrive

group coop
map b30
gametype coop

test coop
title co-op after firefight
check The level loads as a campaign level
"""
RUN_LIST = Path(__file__).resolve().parent / "run_list.py"


def test_coop_after_firefight_is_a_campaign_game(tmp_path):
    build = os.environ.get("AE_FF_LEAK_BUILD")
    maps = os.environ.get("AE_FF_LEAK_MAPS")
    if os.environ.get("CI") or not build or not maps or not Path(build).expanduser().is_file():
        pytest.skip("opt-in: $AE_FF_LEAK_BUILD and $AE_FF_LEAK_MAPS name a game and a maps folder with the campaign's maps")
    if not (Path(maps).expanduser() / "b30.map").is_file():
        pytest.skip("no campaign maps at " + maps)
    listing = tmp_path / "ffleak.txt"
    listing.write_text(LIST)
    command = [sys.executable, str(RUN_LIST), str(listing), "--game", build, "--maps", maps, "--root", str(tmp_path / "run"),
               "--after", "20", "--timeout", "420"]
    if os.environ.get("AE_FF_LEAK_PACKS"):
        command += ["--link", "tag_pack=" + os.environ["AE_FF_LEAK_PACKS"]]
    subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=600)
    folder = tmp_path / "run" / "ffleak"
    log = (folder / "game.log").read_text(errors="replace").splitlines()
    # (debug.txt has the lines the game writes as errors, the waves' and the asserts')
    debug = (folder / "data" / "debug.txt").read_text(errors="replace").splitlines()
    # (the Firefight game started with its wave, the campaign's tags in)
    assert any("firefight: wave 1 started" in line for line in debug), "no Firefight wave"
    start = next(i for i, line in enumerate(log) if "network test: local:b30:coop" in line)
    coop = log[start:]
    assert any("network test: co-op, levels" in line for line in coop), "the co-op level was not chosen"
    leaked = [line for line in coop if "firefight: " in line]
    assert not leaked, "\n".join(leaked[:10])
    asserts = [line for line in log + debug if "EXCEPTION" in line]
    assert not asserts, "\n".join(asserts[:10])
