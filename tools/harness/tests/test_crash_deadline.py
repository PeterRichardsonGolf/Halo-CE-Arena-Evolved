"""A crash report that hangs is ended by SIGALRM at its default (port/linux/src/ae_crash_deadline.h), not by the
dedicated server's SIGALRM handler, which exits 0."""
import signal
import subprocess
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from harness import build, mutated, read, run

FAULTS = {
    "server-handler-kept": ("\tsigaction(SIGALRM, &action, NULL);\n", ""),
    "alarm-left-blocked": ("\tsigprocmask(SIG_UNBLOCK, &alarm_only, NULL);\n", ""),
}


def generated(fault=None):
    code = read("port/linux/src/ae_crash_deadline.h")
    if fault:
        code = mutated(code, *fault)
    return (("under_test.inc", code),)


def test_stuck_report_is_ended():
    status, output = run(build("crash_deadline", generated()), "stuck-report")
    assert status == -signal.SIGALRM, (status, output)


@pytest.mark.parametrize("fault", FAULTS)
def test_negative_control(fault):
    # (the server's handler exits 0; an alarm left blocked never comes, and the report waits forever)
    executable = build("crash_deadline", generated(FAULTS[fault]))
    try:
        result = subprocess.run([str(executable), "stuck-report"], capture_output=True, timeout=5)
    except subprocess.TimeoutExpired:
        return
    assert result.returncode != -signal.SIGALRM, result.returncode
