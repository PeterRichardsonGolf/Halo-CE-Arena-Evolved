"""Arena Evolved's range on its own Delta wire, chupa-38z (port/linux/src/ae_delta_wire.c): the C tests
(port/linux/tests/ae_delta_wire_test.c), built with the build machine's C compiler, and the hooks that use it."""
import shutil
import subprocess
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parent.parent
INCLUDE = ROOT / "port" / "linux" / "include"
SOURCES = [ROOT / "port/linux/tests/ae_delta_wire_test.c", ROOT / "port/linux/src/ae_delta_wire.c",
           ROOT / "port/linux/src/delta_compat.c"]


def compiler():
    for name in ("clang", "cc", "gcc"):
        path = shutil.which(name)
        if path:
            return path
    return None


def test_rules(tmp_path):
    cc = compiler()
    if not cc:
        pytest.skip("no C compiler")
    binary = tmp_path / "ae_delta_wire_test"
    build = subprocess.run([cc, "-std=c99", "-Wall", "-Wextra", "-Werror", "-O1", "-iquote", str(INCLUDE),
                            *map(str, SOURCES), "-o", str(binary)], capture_output=True, text=True)
    assert build.returncode == 0, build.stdout + build.stderr
    run = subprocess.run([str(binary)], capture_output=True, text=True)
    assert run.returncode == 0, run.stdout + run.stderr
    assert "ae_delta_wire_test: ok" in run.stdout


def test_hooks():
    """the host narrows what delta_compat_advertise wrote; the client decides through ae_delta_wire_decide"""
    game = (ROOT / "port/linux/src/delta_peer_game.c").read_text(encoding="utf-8")
    advertise = game[game.index("void delta_peer_advertised_wire("):]
    advertise = advertise[:advertise.index("\n}")]
    assert advertise.index("delta_compat_advertise(") < advertise.index("ae_delta_wire_narrow(")
    matches = game[game.index("int delta_peer_advertised_wire_matches("):]
    matches = matches[:matches.index("\n}")]
    assert "ae_delta_wire_decide(delta_compat_decide(" in matches
