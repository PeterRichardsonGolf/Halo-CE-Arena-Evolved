"""A round's map cleanup holds spawns only in the engines that run it: the previous game's holds none in Race."""
import re
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from harness import CHECK_FAILED, build, enum_with, function, mutated, read, run

CASES = ['race-after-round-reset', 'ctf-cleanup-waits', 'slayer-no-cleanup', 'no-engine']
GUARD = ('if (st.cleanup_at != NONE && playing_engine())', 'if (st.cleanup_at != NONE)')


def generated(fault=None):
    match_c = read('port/linux/game/ae_h2_match.c')
    state = re.search(r'^static struct\n\{\n\tint started;.*?^\} st;\n', match_c, re.M | re.S)
    if not state:
        raise LookupError('ae_h2_match.c state struct not found')
    config = enum_with(read('source/game/game_engine.h'), 'game_engine_race') + '\n'
    config += '#define MAX_SIDES 16\n'
    code = state.group(0) + '\n'
    for name in ('playing_engine', 'unit_alive', 'in_game'):
        code += function(match_c, name) + '\n'
    holds = function(match_c, 'ae_h2_match_holds_spawn')
    if fault:
        holds = mutated(holds, *fault)
    code += holds + '\n'
    return (('config.inc', config), ('under_test.inc', code))


@pytest.mark.parametrize('case', CASES)
def test_case(case):
    status, output = run(build('h2_match_cleanup', generated()), case)
    assert status == 0, output


def test_negative_control():
    status, output = run(build('h2_match_cleanup', generated(GUARD)), 'race-after-round-reset')
    assert status == CHECK_FAILED, output
