"""A machine's players (players.c): a player made for a machine whose list is full is refused, not the game stopped;
a player taken out frees its entry."""
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from harness import CHECK_FAILED, build, function, mutated, read, run  # noqa: E402

FUNCTIONS = ("machine_add_player", "machine_remove_player", "player_new")

# a fault in players.c and the case that must catch it
NEGATIVE_CONTROLS = {
    # (the old way: the game stopped)
    "exit-when-full": (("return FALSE; /* AE hook */", "system_exit(-1); return FALSE; /* AE hook */"), "full-refused"),
    # (the refused player's datum kept)
    "datum-kept": (("{ datum_delete(player_data, player_index); return NONE; }", "{ return NONE; }"), "full-refused"),
}


def generated(fault=None):
    players = read("source/game/players.c")
    code = "\n\n".join(function(players, name) for name in FUNCTIONS)
    if fault:
        code = mutated(code, *fault)
    return (("under_test.inc", code),)


@pytest.mark.parametrize("case", ["full-refused", "room-after-removal"])
def test_case(case):
    status, output = run(build("machine_player_room", generated()), case)
    assert status == 0, output


@pytest.mark.parametrize("control", NEGATIVE_CONTROLS)
def test_negative_control(control):
    fault, case = NEGATIVE_CONTROLS[control]
    status, output = run(build("machine_player_room", generated(fault)), case)
    assert status != 0, output
