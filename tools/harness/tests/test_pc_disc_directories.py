"""A crafted disc image: looping folder records stop the listing (no 60^8 reads); a cut-off record is not read past."""
import re
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from harness import CHECK_FAILED, build, function, mutated, read, run, structure

LOOP_GUARD = '\tif (++list->directories > MAXIMUM_DIRECTORIES) { list->damaged = 1; return; } /* AE hook */\n'


def generated(faults=()):
    source = read('port/linux/src/pc_disc.c')
    constants = re.search(r'^enum\n\{\n\tSECTOR_SIZE = 2048,.*?^\};\n', source, re.M | re.S)
    assert constants, 'pc_disc.c constants not found'
    config = constants.group(0) + structure(source, 'disc_file') + '\n' + structure(source, 'disc_files') + '\n'
    code = ''
    for name in ('read_u32', 'same_name', 'list_directory'):
        code += function(source, name) + '\n'
    for before in faults:
        code = mutated(code, before, '')
    return (('config.inc', config), ('under_test.inc', code))


@pytest.mark.parametrize('case', ['self-loop', 'wide-chain', 'record-at-end', 'plain'])
def test_case(case):
    status, output = run(build('pc_disc_directories', generated()), case)
    assert status == 0, output


def test_negative_control_wide_chain():
    status, output = run(build('pc_disc_directories', generated([LOOP_GUARD])), 'wide-chain')
    assert status == CHECK_FAILED, output

