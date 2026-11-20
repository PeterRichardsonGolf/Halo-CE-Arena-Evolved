"""The shader key lists (port/linux/src/xgpu_shader_keys.c, tools/shader_keys.py,
port/assets/shaders): the game's reader and writer (built from
port/linux/tests/shader_keys_test.c with the build machine's C compiler, and
for a 32-bit and a big-endian machine where those compilers are there) read
the stock list as the tool does, and write what they read back the same; and
the stock list is whole."""
import os
import shutil
import subprocess
import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
import shader_keys  # noqa: E402
try:
    from test_delta_big_endian import toolchain  # noqa: E402
except ImportError:  # (AE has no big-endian test of its own: the leg is skipped)
    def toolchain():
        return None

TEST = ROOT / "port" / "linux" / "tests" / "shader_keys_test.c"
STOCK = ROOT / "port" / "assets" / "shaders" / "stock.keys"
SOURCE = ROOT / "port" / "linux" / "src" / "xgpu_shader_keys.c"
WORDS = (0x00000000, 0x0020001b, 0x0836106c, 0x2f100ff8)
SAMPLE = """chupathingy shader keys 1
# a map's own list: no map line
p 0 0:18200000 9:1c00 100:421 101:1 114:1 113:3 119:2 199:5
k 6822c58b59dab8f8 0 e 0
k 00000000000000ff 5 0 0
p 0 3:1 120:1
k 6822c58b59dab8f8 2 0 0
"""


def code_hash(words):
    value = 14695981039346656037
    for word in words:
        for byte in word.to_bytes(4, "little"):
            value = ((value ^ byte) * 1099511628211) & 0xFFFFFFFFFFFFFFFF
    return value


def lines_of(keys):
    """the C test's lines for keys (shader_keys.read_keys' sets), in any order."""
    return sorted(f"{vertex:016x} {flags:x} {packed:x}" + "".join(f" {slot}:{value:x}" for slot, value in pixel
                                                                  if slot < 57 or 100 <= slot <= 123)
                  for vertex, flags, packed, pixel in keys)


def compilers():
    """(name, compile command, runner) for each machine this one can build for."""
    found = []
    for name in ("clang", "cc", "gcc"):
        cc = shutil.which(name)
        if cc:
            found.append(("native", [cc], []))
            if sys.platform.startswith("linux"):
                found.append(("32-bit", [cc, "-m32"], []))
            break
    tools = toolchain()
    found.append(("big-endian", [tools[0], "-static"] if tools else None, [tools[1]] if tools else None))
    return found


def build(tmp_path, name, command):
    binary = tmp_path / f"shader_keys_test_{name}"
    result = subprocess.run([*command, "-std=c99", "-Wall", "-Wextra", "-Werror", "-O1", str(TEST), "-o", str(binary)],
                            capture_output=True, text=True)
    if result.returncode != 0 and name == "32-bit":
        pytest.skip("no 32-bit C library here")
    assert result.returncode == 0, result.stdout + result.stderr
    return binary


def run(binary, runner, keys, map_name, out):
    result = subprocess.run([*runner, str(binary), str(keys), map_name, str(out)], capture_output=True, text=True)
    assert result.returncode == 0, result.stdout + result.stderr
    lines = result.stdout.splitlines()
    assert lines[0] == f"code hash {code_hash(WORDS):016x}"
    return lines[1:]


@pytest.mark.parametrize("machine", ["native", "32-bit", "big-endian"])
def test_reader_and_writer(tmp_path, machine):
    machines = {entry[0]: entry for entry in compilers()}
    if machine not in machines:
        pytest.skip("no C compiler" if machine == "native" else f"not built for {machine} here")
    _, command, runner = machines[machine]
    if command is None:
        # (CI installs a big-endian cross compiler and QEMU, and asks for them)
        assert os.environ.get("DELTA_BE_REQUIRED") != "1", "no big-endian cross compiler and QEMU"
        pytest.skip("no big-endian cross compiler and QEMU")
    binary = build(tmp_path, machine.replace("-", ""), command)
    sample = tmp_path / "own.keys"
    sample.write_text(SAMPLE)
    got = run(binary, runner, sample, "-", tmp_path / "own-out.keys")
    expected = shader_keys.read_keys(SAMPLE)[None]
    assert sorted(got) == lines_of(expected)
    # (a pixel key's number holds until another line gives it: the third key
    # has the second "p 0"; a slot the build does not know is skipped)
    assert got[2] == "6822c58b59dab8f8 2 0 3:1 120:1"
    assert " 199:" not in got[0]
    # (a map's keys out of the stock list, as the tool reads them)
    if not STOCK.exists():
        return
    for map_name in ("bloodgulch", "damnation", "ui"):
        got = run(binary, runner, STOCK, map_name, tmp_path / f"{map_name}.keys")
        assert sorted(got) == lines_of(shader_keys.read_keys(STOCK.read_text())[map_name]), map_name
        # (and what the writer wrote, read again by the tool)
        written = shader_keys.read_keys((tmp_path / f"{map_name}.keys").read_text())[None]
        assert lines_of(written) == sorted(got)


# numbers too large for their records: each line is refused, and leaves the
# pixel key it would have overwritten (on 64-bit Windows, unsigned long is 32
# bits: 4294967296 would have been 0)
HOSTILE = """chupathingy shader keys 1
p 0 0:18200000 100:421
p 4294967296 0:1
p 18446744073709551616 0:2
p 99999999999999999999999 0:3
p 1048576 0:4
p 16384 0:5
p 1 3:ffffffffff 4:7
k 6822c58b59dab8f8 0 e 0
k 6822c58b59dab8f8 0 e 4294967296
k 6822c58b59dab8f8 0 e 18446744073709551616
k 6822c58b59dab8f8 100000000 e 0
k 6822c58b59dab8f8 0 100000000 0
k 16822c58b59dab8f8 0 e 0
k 00000000000000ff 1 0 1
k 00000000000000ff 1 0 16384
"""
HOSTILE_KEYS = ["6822c58b59dab8f8 0 e 0:18200000 100:421", "00000000000000ff 1 0 4:7"]


@pytest.mark.parametrize("machine", ["native", "32-bit", "big-endian"])
def test_reader_refuses_numbers_too_large(tmp_path, machine):
    machines = {entry[0]: entry for entry in compilers()}
    if machine not in machines or machines[machine][1] is None:
        if machine == "big-endian" and machine in machines:
            assert os.environ.get("DELTA_BE_REQUIRED") != "1", "no big-endian cross compiler and QEMU"
        pytest.skip(f"not built for {machine} here")
    _, command, runner = machines[machine]
    binary = build(tmp_path, machine.replace("-", ""), command)
    hostile = tmp_path / "hostile.keys"
    hostile.write_text(HOSTILE)
    assert run(binary, runner, hostile, "-", tmp_path / "hostile-out.keys") == HOSTILE_KEYS


def test_writer_after_a_cut_line(tmp_path):
    """a map's own list whose last line a crash cut short: the next run's
    keys start on a line of their own, and the list reads whole"""
    machines = {entry[0]: entry for entry in compilers()}
    if "native" not in machines:
        pytest.skip("no C compiler")
    binary = build(tmp_path, "native", machines["native"][1])
    own = tmp_path / "own.keys"
    own.write_text("chupathingy shader keys 1\np 0 0:18200000\nk 6822c58b59dab8f8 0 e 0\nk 6822c58b")
    sample = tmp_path / "sample.keys"
    sample.write_text(SAMPLE)
    result = subprocess.run([str(binary), "--append", str(own), str(sample), "-"], capture_output=True, text=True)
    assert result.returncode == 0, result.stdout + result.stderr
    text = own.read_text()
    # (the cut line is cut off, not completed into a line of its own)
    assert "k 6822c58b" not in text.replace("k 6822c58b59dab8f8", "")
    keys = shader_keys.read_keys(text)[None]
    assert lines_of(keys) == sorted(set(lines_of(shader_keys.read_keys(SAMPLE)[None]) +
                                        ["6822c58b59dab8f8 0 e 0:18200000"]))


def test_writer_never_wipes_a_list_it_could_not_read(tmp_path):
    """a list past the size cap (or one that could not be read) is not added to, and not written over"""
    machines = {entry[0]: entry for entry in compilers()}
    if "native" not in machines:
        pytest.skip("no C compiler")
    binary = build(tmp_path, "native", machines["native"][1])
    big = tmp_path / "big.keys"
    text = "chupathingy shader keys 1\np 0 0:18200000\nk 6822c58b59dab8f8 0 e 0\n" + "# padding\n" * 900000
    big.write_text(text)
    sample = tmp_path / "sample.keys"
    sample.write_text(SAMPLE)
    result = subprocess.run([str(binary), "--append", str(big), str(sample), "-"], capture_output=True, text=True)
    assert result.returncode != 0
    assert big.read_text() == text


def test_header_is_the_whole_line(tmp_path):
    machines = {entry[0]: entry for entry in compilers()}
    if "native" not in machines:
        pytest.skip("no C compiler")
    binary = build(tmp_path, "native", machines["native"][1])
    for header, ok in (("chupathingy shader keys 1", True), ("chupathingy shader keys 1  ", True),
                       ("chupathingy shader keys 10", False), ("chupathingy shader keys 1x", False)):
        keys = tmp_path / "h.keys"
        keys.write_text(header + "\np 0 0:18200000\nk 6822c58b59dab8f8 0 e 0\n")
        result = subprocess.run([str(binary), str(keys), "-", str(tmp_path / "h-out.keys")], capture_output=True, text=True)
        assert (result.returncode == 0) == ok, header


def test_every_pixel_key_field_has_a_slot():
    """a field added to nv2a_pixel_key.h that fits in the key's padding changes no size, so the static assert in
    xgpu_shader_keys.c cannot see it: every field's name must be there (a slot of its own)"""
    import re
    header = (ROOT / "port" / "linux" / "src" / "nv2a_pixel_key.h").read_text()
    body = header[header.index("struct nv2a_pixel_shader_key"):]
    body = re.sub(r"/\*.*?\*/", "", body, flags=re.S)
    fields = re.findall(r"^\s*(?:unsigned char|unsigned long|DWORD)\s+(\w+)", body, re.M)
    assert len(fields) > 15
    source = SOURCE.read_text()
    missing = [name for name in fields if f"key->{name}" not in source]
    assert not missing, missing


def test_stock_list_whole():
    if not STOCK.exists():
        pytest.skip("no stock list in this checkout")
    maps = shader_keys.read_keys(STOCK.read_text())
    assert None not in maps
    # (the menus' map and two multiplayer maps at the least)
    for name in ("ui", "damnation", "bloodgulch"):
        assert maps.get(name), name
    # (written as the tool writes it: merging it again changes nothing)
    assert shader_keys.write_keys(maps) == STOCK.read_text()


def test_slots_match_the_game():
    """the tool's and the README's slots are the game's (xgpu_shader_keys.c)."""
    source = SOURCE.read_text()
    assert "_slot_texture_modes = 100," in source and "_slot_source_blend,\n\t_slot_last = _slot_source_blend" in source
    readme = (ROOT / "port" / "assets" / "shaders" / "README.md").read_text()
    assert "121 the Light effects" in readme.replace("\n  ", " ") and "123 the source blend" in readme.replace("\n  ", " ")


def test_embedded_in_every_build():
    embed = (ROOT / "tools" / "embed_assets.py").read_text()
    assert 'SHADER_KEYS_FILE = Path("port/assets/shaders/stock.keys")' in embed
    assert "xgpu_shader_keys_stock_data" in embed
