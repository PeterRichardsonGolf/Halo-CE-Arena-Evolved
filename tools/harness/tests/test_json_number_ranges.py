"""JSON numbers kept in range before they become integers: a master list's network_version, a WebSocket "after"."""
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from harness import CHECK_FAILED, build, read, run

VERSION = 'if (!(version >= 0 && version <= 0x7FFFFFFF)) version = 0; /* AE hook */'
AFTER = ('server_events_read(after > (double)server_events_latest() ? 0 : (unsigned long)after, '
         'SERVER_EVENTS_RING_SIZE, ws_addresses(connection), ws_replay_one, /* AE hook */')
OLD_AFTER = 'server_events_read((unsigned long)after, SERVER_EVENTS_RING_SIZE, ws_addresses(connection), ws_replay_one,'


def line(path, text):
    source = read(path)
    assert source.count(text) == 1, f'{path} no longer has the line: {text}'
    return text


def generated(version=True, after=True):
    version_line = line('port/linux/src/master_list.c', VERSION) if version else ''
    after_line = line('port/linux/src/server_http.c', AFTER) if after else OLD_AFTER
    # (the call's last arguments follow on the next line in the source; the harness gives its own)
    after_line = after_line.rsplit('SERVER_EVENTS_RING_SIZE', 1)[0]
    return (('version_line.inc', version_line + '\n'), ('after_line.inc', after_line + '\n'))


@pytest.mark.parametrize('case', ['version', 'after'])
def test_case(case):
    status, output = run(build('json_number_ranges', generated()), case)
    assert status == 0, output


def test_negative_control_version():
    status, output = run(build('json_number_ranges', generated(version=False)), 'version')
    assert status == CHECK_FAILED, output


def test_negative_control_after():
    status, output = run(build('json_number_ranges', generated(after=False)), 'after')
    assert status != 0, output
