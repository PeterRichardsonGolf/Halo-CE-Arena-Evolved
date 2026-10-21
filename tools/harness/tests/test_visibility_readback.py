"""The desktop visibility readback through the buffer asks the driver for nothing at a present (a GL error, a fence's
state), so that a threaded driver need not catch up with the frame just drawn; it takes a test's count only once the
GPU has copied it, and drops a batch whose counts never come."""
import re
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from harness import CHECK_FAILED, build, constant, function, mutated, read, run, structure

CASES = ['quiet', 'behind', 'reused', 'lost', 'logging']
SOURCE = 'port/linux/src/d3d8_gl.c'


def generated(fault=None):
    source = read(SOURCE)
    defines = ''.join(match.group(0) + '\n' for match in
                      re.finditer(r'^#define VISIBILITY_(?:UNCOPIED|COPY_FRAMES)\b.*$', source, re.M))
    config = '#define VISIBILITY_TEST_SLOTS %d\n#define VISIBILITY_BATCH_COUNT %d\n%s%s;\n' % (
        constant(source, 'VISIBILITY_TEST_SLOTS'), constant(source, 'VISIBILITY_BATCH_COUNT'), defines,
        structure(source, 'visibility_batch'))
    code = '\n'.join(function(source, name) for name in ('visibility_readback_gl_ok', 'visibility_read_batch'))
    if fault:
        code = mutated(code, *fault)
    return (('config.inc', config), ('under_test.inc', code))


@pytest.mark.parametrize('case', CASES)
def test_case(case):
    status, output = run(build('visibility_readback', generated()), case)
    assert status == 0, output


@pytest.mark.parametrize('fault,case', [
    # GL errors asked for at every present
    (('if (device.visibility_logging && !visibility_readback_gl_ok("query copies"))',
      'if (!visibility_readback_gl_ok("query copies"))'), 'quiet'),
    # a batch used again without marking its counts uncopied: the earlier ones are taken
    (('counts[i] = VISIBILITY_UNCOPIED;', 'counts[i] = counts[i];'), 'reused'),
    # a count not yet copied taken as one
    (('if (samples == VISIBILITY_UNCOPIED)', 'if (0)'), 'behind'),
    # counts that never come wait for ever
    (('if (device.frame - batch->frame > VISIBILITY_COPY_FRAMES)', 'if (0)'), 'lost'),
])
def test_negative_control(fault, case):
    status, output = run(build('visibility_readback', generated(fault)), case)
    assert status == CHECK_FAILED, output
