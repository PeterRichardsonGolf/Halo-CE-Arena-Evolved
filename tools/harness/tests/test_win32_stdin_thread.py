"""The Windows dedicated server's stdin reader that cannot start ends the input: every read after says -1."""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from harness import CHECK_FAILED, build, function, mutated, read, run

GUARD = '\t\tif (!thread) input_ended = 1; /* AE hook */\n'


def generated(fault=False):
    code = function(read('port/windows/src/win32_server.c'), 'posix_server_stdin_read')
    if fault:
        code = mutated(code, GUARD, '')
    return (('under_test.inc', code + '\n'),)


def test_no_thread():
    status, output = run(build('win32_stdin_thread', generated()), 'no-thread')
    assert status == 0, output


def test_negative_control():
    status, output = run(build('win32_stdin_thread', generated(fault=True)), 'no-thread')
    assert status == CHECK_FAILED, output
