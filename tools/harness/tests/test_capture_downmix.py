"""A recording's sound on 5.1 output is the mix folded to stereo, not 6 channels read as 2; a stereo mix is kept."""
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from harness import CHECK_FAILED, build, mutated, read, run

PASS_AS_IS = ('\tif (channels != 6)\n', '\tif (1)\n')


def generated(fault=None):
    code = read('port/linux/src/ae_capture_downmix.h')
    if fault:
        code = mutated(code, *fault)
    return (('under_test.inc', code),)


@pytest.mark.parametrize('case', ['five-one', 'stereo'])
def test_case(case):
    status, output = run(build('capture_downmix', generated()), case)
    assert status == 0, output


def test_negative_control():
    status, output = run(build('capture_downmix', generated(PASS_AS_IS)), 'five-one')
    assert status == CHECK_FAILED, output
