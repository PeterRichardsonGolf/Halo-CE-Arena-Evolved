"""A pending team change belongs to the player who asked: a joiner in the same slot does not inherit it."""
import re
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from harness import CHECK_FAILED, build, function, read, run, structure

CASES = ['same-player', 'new-player-in-slot', 'new-player-died']
OWNER_CHECK = 'if (absolute >= 0 && absolute < TEAM_CHANGE_PLAYERS && team_change.players[absolute].player_index != player_index)'


def generated(without_owner_check=False):
    source = read('port/linux/game/team_change.c')
    players = re.search(r'^\tTEAM_CHANGE_PLAYERS = [^,]+,', source, re.M)
    config = 'enum { TEAM_CHANGE_COOLDOWN = 10, TEAM_CHANGE_PLAYERS = 16 };\n'
    assert players, 'TEAM_CHANGE_PLAYERS not found'
    config += structure(source, 'team_change_player') + '\n'
    state = re.search(r'^static struct\n\{\n\t/\* the host\'s rule.*?^\} team_change;\n', source, re.M | re.S)
    if not state:
        raise LookupError('team_change.c state not found')
    config += state.group(0)
    code = ''
    for name in ('change_of', 'effective_team', 'team_change_player_died', 'team_change_round_started'):
        code += function(source, name) + '\n'
    if without_owner_check:
        lines = [line for line in code.split('\n') if OWNER_CHECK not in line]
        assert len(lines) < len(code.split('\n')), 'the owner check is gone from change_of'
        code = '\n'.join(lines)
    return (('config.inc', config), ('under_test.inc', code))


@pytest.mark.parametrize('case', CASES)
def test_case(case):
    status, output = run(build('team_change_slot', generated()), case)
    assert status == 0, output


@pytest.mark.parametrize('case', ['new-player-in-slot', 'new-player-died'])
def test_negative_control(case):
    status, output = run(build('team_change_slot', generated(without_owner_check=True)), case)
    assert status == CHECK_FAILED, output
