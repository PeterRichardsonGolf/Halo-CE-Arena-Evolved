"""HIT FEEDBACK's early-pop table: a dead or deleted unit's slot goes to a new unit, live units keep theirs, a new map
forgets them all."""
import re
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from harness import CHECK_FAILED, build, function, mutated, read, run

CASES = ['dead-units-free-slots', 'gone-units-free-slots', 'live-units-keep-slots', 'new-map']
REUSE = ('\tif (!free_unit)\n\t\tfree_unit = shield_pop_early_find_gone();\n', '')
NAMES = ['shield_pop_early_find', 'shield_pop_early_find_gone', 'ae_shield_fx_pop_was_early', 'ae_shield_fx_pop_early',
         'ae_shield_fx_pop_rearm', 'ae_shield_fx_new_map']


def generated(fault=None):
    source = read('port/linux/game/ae_shield_fx.c')
    code = re.search(r'^static long shield_pop_early_units\[32\];\n', source, re.M).group(0)
    code += re.search(r'^static boolean shield_pop_early_initialized = FALSE;\n', source, re.M).group(0)
    for name in NAMES:
        code += function(source, name) + '\n'
    if fault:
        code = mutated(code, *fault)
    return (('under_test.inc', code),)


@pytest.mark.parametrize('case', CASES)
def test_case(case):
    status, output = run(build('shield_pop_slots', generated()), case)
    assert status == 0, output


@pytest.mark.parametrize('case', ['dead-units-free-slots', 'gone-units-free-slots'])
def test_negative_control(case):
    status, output = run(build('shield_pop_slots', generated(REUSE)), case)
    assert status == CHECK_FAILED, output
