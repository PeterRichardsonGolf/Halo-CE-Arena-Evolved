"""Tests of tools/ae_test/test_run_lint.py: the sample passes, one case per rule fails."""
import subprocess
import sys
from pathlib import Path

import pytest

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE / "ae_test"))
import test_run_lint  # noqa: E402

LISTS = HERE / "ae_test" / "test_runs"
LINT = HERE / "ae_test" / "test_run_lint.py"

GOOD = """list {name}
revision 1
test a
check Look at it
map bloodgulch
"""


def write(tmp_path, text, name="case"):
    path = tmp_path / f"{name}.txt"
    path.write_text(text)
    return path


def messages(path):
    return [f"{line}: {message}" for line, message in test_run_lint.lint(path)]


@pytest.mark.parametrize("path", sorted(LISTS.glob("*.txt")), ids=lambda p: p.name)
def test_shipped_lists_pass(path):
    assert test_run_lint.lint(path) == []


def test_sample_passes_from_the_command_line():
    run = subprocess.run([sys.executable, str(LINT), str(LISTS / "sample.txt")], capture_output=True, text=True)
    assert run.returncode == 0 and run.stdout == ""


def test_good_case_passes(tmp_path):
    assert messages(write(tmp_path, GOOD.format(name="case"))) == []


@pytest.mark.parametrize("extra, expect", [
    ("frobnicate yes\n", "6: unknown line"),
    ("check two\ncheck three\ncheck four\n", "more than 3 checks"),
    ("screen 2\n", "both map and screen"),
    ("set no.such_key 1\n", "not in the config table"),
    ("set debug.ae_test_screen\n", "set needs a key and a value"),
    ("check " + "x" * 61 + "\n", "at most 60"),
    ("players 5\n", "players must be 1 to 4"),
    ("card left\n", "card is top or bottom"),
    ("retest soon\n", "bad retest"),
    ("test a\ncheck again\nmap m\n", "duplicate test id 'a'"),
    ("test Bad_Id\ncheck c\nmap m\n", "bad test id"),
    ("".join("set debug.ae_test_screen 1\n" for _ in range(17)), "more than 16 sets"),
    ("".join(f"test t{i}\ncheck c\nmap m\n" for i in range(64)), "more than 64 tests"),
    ("screen 0\n", "bad screen number"),
    ("screen 40000\n", "bad screen number"),
    ("screen \u00b2\n", "bad screen number"),
    ("players \u00b2\n", "players must be 1 to 4"),
    ("players 0\n", "players must be 1 to 4"),
    ("check\n", "empty check"),
    ("map\n", "map has no name"),
    ("set display.fov 90\n", "per-player setting"),
    ("set input.invert_mouse true\n", "per-player setting"),
])
def test_bad_case_fails(tmp_path, extra, expect):
    path = write(tmp_path, GOOD.format(name="case") + extra)
    found = messages(path)
    assert any(expect in message for message in found), found
    run = subprocess.run([sys.executable, str(LINT), str(path)], capture_output=True, text=True)
    assert run.returncode == 1 and f"{path}:" in run.stdout


def test_known_set_key_passes(tmp_path):
    path = write(tmp_path, GOOD.format(name="case") + "set debug.network_test_loadout assault_rifle\n")
    assert messages(path) == []


def test_neither_map_nor_screen_and_no_check(tmp_path):
    assert any("neither map nor screen" in m for m in messages(write(tmp_path, "list case\ntest a\ncheck c\n")))
    assert any("no check" in m for m in messages(write(tmp_path, "list case\ntest a\nmap m\n")))


def test_list_id_must_be_the_file_name(tmp_path):
    assert any("not the file name" in m for m in messages(write(tmp_path, GOOD.format(name="other"))))


def test_no_list_line(tmp_path):
    assert any("no list line" in m for m in messages(write(tmp_path, "test a\ncheck c\nmap m\n")))


@pytest.mark.parametrize("name", ["Bad_Id", "bad.id", "bad id", "a" * 32])
def test_bad_list_id_characters(tmp_path, name):
    path = write(tmp_path, GOOD.format(name=name), name="case")
    assert any("bad list id" in m for m in messages(path))


def test_leading_zeros_are_numbers(tmp_path):
    """The parser reads `players 04` as 4, so the linter accepts it too."""
    assert not any("players" in m for m in messages(write(tmp_path, GOOD.format(name="case") + "players 04\n")))


def test_byte_order_mark_is_skipped(tmp_path):
    path = tmp_path / "case.txt"
    path.write_bytes(b"\xef\xbb\xbf" + GOOD.format(name="case").encode())
    assert messages(path) == []


def test_missing_file_is_a_lint_error(tmp_path):
    path = tmp_path / "nope.txt"
    run = subprocess.run([sys.executable, str(LINT), str(path)], capture_output=True, text=True)
    assert run.returncode == 1 and f"{path}:1: cannot read" in run.stdout and "Traceback" not in run.stderr


def test_per_player_keys_are_read_from_the_table():
    keys = test_run_lint.player_keys()
    assert {"display.fov", "display.spawn_heat", "input.keyboard_prompts"} <= keys
    assert keys <= test_run_lint.config_keys()


def test_no_per_player_keys_fails_loudly(tmp_path, monkeypatch):
    table = tmp_path / "ae_config_players.c"
    table.write_text("nothing here\n")
    monkeypatch.setattr(test_run_lint, "PLAYER_TABLE", table)
    with pytest.raises(SystemExit) as stopped:
        test_run_lint.player_keys()
    assert "cannot read per-player keys" in str(stopped.value)


def test_no_config_keys_fails_loudly(tmp_path, monkeypatch):
    table = tmp_path / "port_config.c"
    table.write_text("nothing here\n")
    monkeypatch.setattr(test_run_lint, "CONFIG_TABLE", table)
    with pytest.raises(SystemExit) as stopped:
        test_run_lint.config_keys()
    assert "cannot read config keys" in str(stopped.value)


@pytest.mark.parametrize("value", ["x", "40000", "\u0663", "-1", ""])
def test_bad_revision(tmp_path, value):
    path = write(tmp_path, GOOD.format(name="case").replace("revision 1", f"revision {value}"))
    assert any("bad revision" in m for m in messages(path))
