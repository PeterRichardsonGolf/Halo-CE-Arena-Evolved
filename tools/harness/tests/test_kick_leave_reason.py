"""sv_kick / sv_ban give the leave reason to the kicked player's machine, copied before the kick moves the list."""
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from harness import CHECK_FAILED, build, function, mutated, read, run

OLD = [
    ('machine = game->players[slot].machine_index; /* AE hook */\n', ''),
    ('if (!network_game_server_kick_machine_of_player(machine, FALSE)) /* AE hook */',
     'if (!network_game_server_kick_machine_of_player(game->players[slot].machine_index, FALSE))'),
    ('server_director_player_removed(machine, "kicked"); /* AE hook */',
     'server_director_player_removed(game->players[slot].machine_index, "kicked");'),
]
OLD_BAN = [
    ('machine = game->players[slot].machine_index; /* AE hook */\n', ''),
    ('if (!network_game_server_kick_machine_of_player(machine, TRUE)) /* AE hook */',
     'if (!network_game_server_kick_machine_of_player(game->players[slot].machine_index, TRUE))'),
    ('server_director_player_removed(machine, "banned"); /* AE hook */',
     'server_director_player_removed(game->players[slot].machine_index, "banned");'),
]


def generated(kick_faults=(), ban_faults=()):
    source = read('port/linux/game/server_commands.c')
    kick = function(source, 'command_kick')
    ban = function(source, 'command_ban')
    for before, after in kick_faults:
        kick = mutated(kick, before, after)
    for before, after in ban_faults:
        ban = mutated(ban, before, after)
    return (('under_test.inc', kick + '\n' + ban + '\n'),)


@pytest.mark.parametrize('case', ['kick', 'ban'])
def test_case(case):
    status, output = run(build('kick_leave_reason', generated()), case)
    assert status == 0, output


def test_negative_control_kick():
    status, output = run(build('kick_leave_reason', generated(kick_faults=OLD)), 'kick')
    assert status == CHECK_FAILED, output


def test_negative_control_ban():
    status, output = run(build('kick_leave_reason', generated(ban_faults=OLD_BAN)), 'ban')
    assert status == CHECK_FAILED, output
