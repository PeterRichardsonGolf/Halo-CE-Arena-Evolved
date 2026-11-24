"""An actor hearing a friend shoot passes over the friend's aim-assist target once that target is deleted."""
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from harness import CHECK_FAILED, build, function, mutated, read, run

CASES = ['live-target', 'deleted-target', 'old-target']
GUARD = ('if (player->aim_assist_unit_index != NONE && unit_try_and_get(player->aim_assist_unit_index) && /* AE hook */',
         'if (player->aim_assist_unit_index != NONE &&')


def generated(fault=None):
    code = function(read('source/ai/actor_stimulus.c'), 'actor_stimulus_heard_shooting')
    if fault:
        code = mutated(code, *fault)
    return (('under_test.inc', code + '\n'),)


@pytest.mark.parametrize('case', CASES)
def test_case(case):
    status, output = run(build('aim_assist_stimulus', generated()), case)
    assert status == 0, output


def test_negative_control():
    status, output = run(build('aim_assist_stimulus', generated(GUARD)), 'deleted-target')
    assert status == CHECK_FAILED, output
