"""Players held out of a game while their machine downloads the map (network_server_manager.c, OpenCE's) are the
pregame's again after a game the download did not finish in, unless their machine left: AE's hook in
network_game_server_reset_to_pregame (DamnationCE v0.3.41)."""

import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from harness import CHECK_FAILED, build, constant, function, mutated, read, run  # noqa: E402

CASES = ["held-back", "gone-machine", "none-held", "slot-taken"]

SOURCE = "source/networking/network_server_manager.c"
RESTORE = "for (i = 0; i < network_game_server_held_player_count; i++) /* AE hook */"
RETRY = "((network_game_server_held_players[i].player_list_index = NONE), "

# a fault in the hook, and the case that must catch it
NEGATIVE_CONTROLS = {
    "held-dropped": ((RESTORE, "if (0)"), "held-back"),
    # (no second try in any free slot: the player whose slot was taken is dropped)
    "slot-retry-dropped": ((RETRY, "(0 && "), "slot-taken"),
    "gone-kept": (("if (!network_game_server_client_machine_is_joined_to_game", "if (0 && !network_game_server_client_machine_is_joined_to_game"),
                  "gone-machine"),
}


def hook_lines(source):
    """The AE hook's lines in network_game_server_reset_to_pregame."""
    reset = function(source, "network_game_server_reset_to_pregame")
    lines = [line.strip() for line in reset.splitlines() if "/* AE hook */" in line and "held_player" in line]
    if RESTORE not in lines:
        raise LookupError("the held players' AE hook is not in network_game_server_reset_to_pregame")
    return "\n".join(lines) + "\n"


def generated(fault=None):
    source = read(SOURCE)
    config = ""
    for name in ("MAXIMUM_NETWORK_PLAYER_COUNT", "MAXIMUM_NETWORK_MACHINE_COUNT"):
        config += f"#define {name} {constant(source + read('port/linux/include/halo_port_limits.h'), name)}\n"
    text = ""
    for name in ("network_game_server_hold_map_fetcher_players", "network_game_server_restore_held_players",
                 "network_game_server_discard_held_players"):
        text += function(source, name) + "\n"
    hook = hook_lines(source)
    if fault:
        hook = mutated(hook, *fault)
    return (("config.inc", config), ("under_test.inc", text), ("reset_hook.inc", hook))


@pytest.mark.parametrize("case", CASES)
def test_case(case):
    status, output = run(build("held_players", generated()), case)
    assert status == 0, output


@pytest.mark.parametrize("control", NEGATIVE_CONTROLS)
def test_negative_control(control):
    fault, case = NEGATIVE_CONTROLS[control]
    status, output = run(build("held_players", generated(fault)), case)
    assert status == CHECK_FAILED, f"the reset hook with a fault ({control}) passed '{case}': the test cannot see it"
