"""A dedicated server's sv_vpn_fetch with several masters: each master's netlist asked once, then the thread waits."""
import re
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from harness import CHECK_FAILED, build, enum_with, function, mutated, read, run, structure

CASES = ['two-masters', 'four-masters', 'one-master', 'not-due']
# (the code before the fix: sv_vpn_fetch set fetch_now, and the due search took any master while it was set)
OLD = [
    ('{ int index; for (index = 0; index < netlist.master_count; index++) netlist.masters[index].next_fetch = 0; } '
     '/* AE hook */', 'netlist.fetch_now = 1;'),
    ('if (now >= netlist.masters[index].next_fetch) /* AE hook */',
     'if (netlist.fetch_now || now >= netlist.masters[index].next_fetch)'),
]


def generated(faults=()):
    source = read('port/linux/src/server_netlist.c')
    master_h = read('port/linux/src/server_master.h')
    config = enum_with(master_h, 'SERVER_MASTER_MAXIMUM_MASTERS') + '\n'
    config += 'enum { RETRY_SECONDS = %s };\n' % re.search(r'^\tRETRY_SECONDS = ([^,]+),', source, re.M).group(1)
    config += structure(source, 'held_list') + '\n'
    # (the netlist state: the fields the thread and server_netlist_fetch_now use, as server_netlist.c has them)
    config += ('static struct { SDL_Mutex *lock; SDL_Condition *wake; SDL_Thread *thread; int started; int fetch_now;\n'
               '\tstruct held_list masters[SERVER_MASTER_MAXIMUM_MASTERS]; int master_count; } netlist;\n')
    thread = function(source, 'netlist_thread_main')
    fetch_now = [m for m in re.finditer(r'^void server_netlist_fetch_now\(void\)\n\{\n(?:.*\n)*?^\}\n', source, re.M)][0].group(0)
    code = fetch_now + '\n' + thread + '\n'
    for before, after in faults:
        code = mutated(code, before, after)
    return (('config.inc', config), ('under_test.inc', code))


@pytest.mark.parametrize('case', CASES)
def test_case(case):
    status, output = run(build('netlist_fetch_now', generated()), case)
    assert status == 0, output


@pytest.mark.parametrize('case', ['two-masters', 'four-masters'])
def test_negative_control(case):
    status, output = run(build('netlist_fetch_now', generated(OLD)), case)
    assert status == CHECK_FAILED, output
