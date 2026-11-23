"""An MCC announcer change empties the line queue with its clips; a clip with no rate yet is not played."""
import re
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from harness import CHECK_FAILED, build, function, mutated, read, run

CASES = ['close-empties-queue', 'no-rate', 'plays']
QUEUE_CLEAR = '\tannouncer.queued = 0;\n'
RATE_CHECK = ('if (!clip || !clip->samples || !clip->rate)', 'if (!clip || !clip->samples)')


def generated(faults=()):
    source = read('port/linux/game/ae_announcer.c')
    burst = re.search(r'^struct ae_announcer_burst\n\{.*?^\};\n', read('port/linux/game/ae_announcer_rules.h'), re.M | re.S)
    config = 'enum { AE_MEDAL_COUNT = 64 };\n' + burst.group(0)
    for name in ('QUEUE_MOST', 'GAP_MILLISECONDS', 'FOLDER_MOST', 'TAKES_MOST', 'OTHERS_MOST', 'MULTIPLAYER_SOUNDS'):
        config += re.search(r'^#define ' + name + r' .*$', source, re.M).group(0) + '\n'
    config += re.search(r'^enum \{ EXTRA_EXTERMINATION.*$', source, re.M).group(0) + '\n'
    config += re.search(r'^struct clip\n\{.*?^\};\n', source, re.M | re.S).group(0)
    config += re.search(r'^struct voice\n\{.*?^\};\n', source, re.M | re.S).group(0)
    config += re.search(r'^static struct\n\{\n\tstruct voice pack_medals.*?^\} announcer;\n', source, re.M | re.S).group(0)
    code = ''
    for name in ('clip_free', 'mcc_close', 'ae_announcer_update'):
        code += function(source, name) + '\n'
    for before, after in faults:
        code = mutated(code, before, after)
    return (('config.inc', config), ('under_test.inc', code))


@pytest.mark.parametrize('case', CASES)
def test_case(case):
    status, output = run(build('announcer_queue', generated()), case)
    assert status == 0, output


def test_negative_control_queue():
    status, output = run(build('announcer_queue', generated([(QUEUE_CLEAR, ''), RATE_CHECK])), 'close-empties-queue')
    assert status != 0, output


def test_negative_control_rate():
    # without the rate check, the clip's length divides by 0: the run dies (SIGFPE)
    status, output = run(build('announcer_queue', generated([RATE_CHECK])), 'no-rate')
    assert status != 0, output
