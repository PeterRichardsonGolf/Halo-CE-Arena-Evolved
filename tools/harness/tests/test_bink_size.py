"""Movie sizes bink_playback.c can play (port/linux/game/ae_bink_size.h): the game's own and HD ones that fit the
pool, nothing whose frames would be written outside the frame buffer."""
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from harness import CHECK_FAILED, build, mutated, read, run

FAULTS = {
    'narrow': ('width < 16 || ', ''),
    'pool': ('+ 4096 + 65536 <= pool_size', '<= 0xFFFFFFFFUL'),
    'multiple-of-16': (' || width % 16', ''),
    'tall': (' || height > 4096', ''),
}


def generated(fault=None):
    code = read('port/linux/game/ae_bink_size.h')
    if fault:
        code = mutated(code, *fault)
    return (('under_test.inc', code),)


def test_sizes():
    status, output = run(build('bink_size', generated()), 'sizes')
    assert status == 0, output


@pytest.mark.parametrize('fault', FAULTS)
def test_negative_control(fault):
    status, output = run(build('bink_size', generated(FAULTS[fault])), 'sizes')
    assert status == CHECK_FAILED, output
