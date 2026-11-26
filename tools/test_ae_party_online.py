"""One game of the harness against the party service (tools/ae_party_service): the game, with network.party_service set, starts the link
from init.txt (DEV MODE's `party online ...`), joins a party by invite on its own (autojoin) and sees QUICK PLAY's answer, a follow and
a kick, while a second player ("Rook", tools/ae_party_service/party_smoke_helper.py) leads the party. The game runs in a network
namespace of its own: the service is a helper on its loopback. Needs AE_MENUS_BUILD and the harness config like tools/test_ae_menus.py."""
import importlib.util
import re
import sys
from pathlib import Path

import pytest

HERE = Path(__file__).resolve().parent
_spec = importlib.util.spec_from_file_location("ae_party_menus", HERE / "test_ae_menus.py")
menus = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(menus)

PORT = 18781


@pytest.mark.skipif(menus.config() is None, reason="needs AE_MENUS_BUILD and the harness config (tools/test_ae_menus.py)")
def test_game_joins_a_party_and_follows_quick_play():
    cfg = menus.config()
    out = menus.out_dir(cfg, "party-online")
    helper = [sys.executable, str(HERE / "ae_party_service" / "party_smoke_helper.py"), str(PORT), str(out / "steps.txt")]
    init = ["dev party online name Chief", "dev party online autojoin", "dev party online start"]
    spec = {"name": "party-online", "build": menus.build(), "window": "1280x720", "exit_after": 100, "screenshots": 0,
            "init": init, "helper": helper,
            "env": {"HALO_ARENA_MENUS": "1", "HALO_AE_TEST_SCREEN": "1", "HALO_MCC_USE": "never", "HALO_DEV_MODE": "1", "HALO_AE_TEST_MOTD_SKIP": "1",
                    "HALO_NET_PARTY_SERVICE": f"http://127.0.0.1:{PORT}/party"}}
    menus.harness.play(cfg, spec, out / "party-online")
    # (AE's log lines are in the game's stdout)
    log = out / "party-online" / "stdout.log"
    text = log.read_text(errors="replace") if log.exists() else ""
    lines = [line for line in text.splitlines() if "ae party:" in line]
    joined = "\n".join(lines)
    assert re.search(r"ae party: connected as [0-9A-Z]{4}-", joined), joined
    assert re.search(r"view rev \d+, in a party, 0 invites", joined), joined
    assert re.search(r"view rev \d+, in a party, 0 invites, search found", joined), joined
    assert re.search(r"view rev \d+, no party", joined), joined
    # the found server is tried by the follow (nothing listens there: it fails), then the leader's hosted game is tried
    assert re.search(r"follow: joining 127\.0\.0\.1:10660", joined), joined
    assert re.search(r"follow: (joining invite|could not join invite)", joined), joined
    steps = out / "steps.txt"
    assert steps.exists() and "FAIL" not in steps.read_text() and "end" in steps.read_text()
