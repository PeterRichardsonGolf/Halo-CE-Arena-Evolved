"""tools/ae_test/replay_read.py: a made-up recording reads, and each kind of damage is refused with its line."""
import struct
import sys
import zlib
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent / "ae_test"))
import replay_read  # noqa: E402


def header(length=900, fmt=5, network=44, size=384, ticks=30, flags=0, map_name=b"levels\\test\\bloodgulch\\bloodgulch"):
    raw = bytearray(384)
    raw[:8] = b"OCREPLAY"
    struct.pack_into("<6I", raw, 8, fmt, network, 1, ticks, 100, size)
    raw[40:40 + len(map_name)] = map_name
    raw[168:173] = b"Slyr1"
    struct.pack_into("<2I", raw, 280, length, flags)
    return bytes(raw)


def records(*items):
    return b"".join(struct.pack("<BBHI", kind, 0, len(data), tick) + data for kind, tick, data in items)


def recording(tmp_path, body, finish=True, head=None):
    stream = zlib.compressobj(1)
    data = stream.compress(body) + (stream.flush() if finish else stream.flush(zlib.Z_SYNC_FLUSH))
    recording.made += 1
    path = tmp_path / f"20261118-18{recording.made:04d}_bloodgulch.ocreplay"
    path.write_bytes((head or header()) + data)
    return path


recording.made = 0
GOOD = records((5, 100, b"g" * 40), (7, 100, b"s" * 80), (1, 101, b"d" * 300), (2, 130, b"r" * 20), (1, 160, b"d" * 9))


def test_reads(tmp_path, capsys):
    assert replay_read.main([str(recording(tmp_path, GOOD))]) == 0
    line = capsys.readouterr().out
    assert "bloodgulch (Slyr1), 2 s, 5 records" in line and "datagram 2" in line and "cut" not in line


def test_cut_stream_reads_to_its_sync(tmp_path, capsys):
    assert replay_read.main([str(recording(tmp_path, GOOD, finish=False))]) == 0
    assert "cut after 2 s" in capsys.readouterr().out


def test_refusals(tmp_path, capsys):
    cases = [
        (recording(tmp_path, GOOD, head=header()[:200]), "not a replay"),
        (recording(tmp_path, GOOD, head=header(fmt=6)), "made by a later build"),
        (recording(tmp_path, GOOD, head=header(network=43)), "another network version"),
        (recording(tmp_path, GOOD, head=header(length=0xFFFFFFFF)), "header is damaged"),
        (recording(tmp_path, GOOD, head=header(size=0)), "header is damaged"),
        (recording(tmp_path, GOOD, head=header(flags=2)), "header is damaged"),
        (recording(tmp_path, GOOD, head=header(map_name=b"")), "header is damaged"),
        (recording(tmp_path, GOOD, head=header(map_name=b"x" * 128)), "header is damaged"),
        (recording(tmp_path, records((9, 100, b""))), "kind 9"),
        (recording(tmp_path, records((1, 200, b"a"), (1, 150, b"b"))), "at tick 150"),
        (recording(tmp_path, GOOD + struct.pack("<BBHI", 1, 0, 65535, 170) + b"x" * 10), "last record is cut"),
        (recording(tmp_path, records((1, 101, b"d"))), "no game record"),
    ]
    for path, why in cases:
        assert replay_read.main([str(path)]) == 1, why
        out = capsys.readouterr().out
        assert "refused" in out and why in out, (why, out)
    damaged = recording(tmp_path, GOOD)
    raw = bytearray(damaged.read_bytes())
    raw[384:388] = b"\xff\xff\xff\xff"
    damaged.write_bytes(bytes(raw))
    assert replay_read.main([str(damaged)]) == 1
    assert "stream is damaged" in capsys.readouterr().out
