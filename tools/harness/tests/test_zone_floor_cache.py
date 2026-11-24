"""The zone ring's kept floor heights: one from the previous game is neither used nor kept, a fresh one is used."""
import re
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from harness import CHECK_FAILED, build, function, mutated, read, run

FIX = ('boolean have = entry->stamp != 0 && entry->stamp <= now;', 'boolean have = entry->stamp != 0;')


def generated(fault=None):
    source = read('port/linux/game/ae_zone_world.c')
    config = '#define MAXIMUM_NUMBER_OF_LOCAL_PLAYERS 4\n'
    grid = source.index('\tGRID = ')
    start = source.rindex('enum\n{\n', 0, grid)
    config += source[start:source.index('\n};\n', grid) + 4]
    config += re.search(r'^#define FLOOR_STALE_MOVE .*$', source, re.M).group(0) + '\n'
    config += re.search(r'^struct floor_entry\n\{.*?^\};\n', source, re.M | re.S).group(0)
    config += re.search(r'^static struct floor_entry floors\[.*$', source, re.M).group(0) + '\n'
    code = function(source, 'floor_at')
    if fault:
        code = mutated(code, *fault)
    return (('config.inc', config), ('under_test.inc', code + '\n'))


@pytest.mark.parametrize('case', ['previous-game-entry', 'fresh-entry'])
def test_case(case):
    status, output = run(build('zone_floor_cache', generated()), case)
    assert status == 0, output


def test_negative_control():
    status, output = run(build('zone_floor_cache', generated(FIX)), 'previous-game-entry')
    assert status == CHECK_FAILED, output
