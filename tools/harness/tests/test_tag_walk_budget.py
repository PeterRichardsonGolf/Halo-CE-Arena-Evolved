"""tag_validate.c's walk is bounded by the region's size: tags sharing their bytes over and over are refused, a walk of
distinct elements is not."""
import re
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from harness import CHECK_FAILED, build, function, mutated, read, run

BUDGET = ('\tif (++validation->visits > 4ULL * validation->region_size + 65536) { tag_validate_refuse(validation, '
          '"is walked too many times: its tags share their bytes over and over"); return; } /* AE hook */\n', '')


def generated(fault=None):
    source = read('port/linux/game/tag_validate.c')
    config = re.search(r'^\tMAXIMUM_VALIDATION_DEPTH = \d+,$', source, re.M).group(0).strip()
    config = 'enum { ' + config + ' };\n'
    passes = re.search(r'^enum\n\{\n\t/\* blocks\' and data\'s counts and bytes \*/\n.*?^\};\n', source, re.M | re.S)
    config += passes.group(0)
    config += re.search(r'^struct tag_validation_frame\n\{.*?^\};\n', source, re.M | re.S).group(0)
    config += re.search(r'^struct tag_validation\n\{.*?^\};\n', source, re.M | re.S).group(0)
    code = function(source, 'validate_element') + '\n'
    if fault:
        code = mutated(code, *fault)
    return (('config.inc', config), ('under_test.inc', code))


@pytest.mark.parametrize('case', ['shared-chain', 'distinct-elements'])
def test_case(case):
    status, output = run(build('tag_walk_budget', generated()), case)
    assert status == 0, output


def test_negative_control():
    status, output = run(build('tag_walk_budget', generated(BUDGET)), 'shared-chain')
    assert status == CHECK_FAILED, output
