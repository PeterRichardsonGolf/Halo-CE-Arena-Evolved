# Unit tests for mcc_convert/hud_scale.py on small synthetic HUD tags.
import os
import struct
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), 'mcc_convert'))
import hud_scale as hs  # noqa: E402


def header(group):
    h = bytearray(64)
    h[0x24:0x28] = group
    h[0x3C:0x40] = b'blam'
    return h


def placement(x, y, w, h, flags):
    return struct.pack('>hhffHH', x, y, w, h, flags, 0) + bytes(20)


def ref(path):
    p = path.encode()
    return b'bitm' + struct.pack('>III', 0, len(p), 0xFFFFFFFF), p + b'\0'


def block(count):
    return struct.pack('>III', count, 0, 0)


def weapon_tag(static_flags=0, item_flags=(0, 4)):
    """A weapon HUD: one static element (bitmap A), one number element, one crosshair (bitmap B) with two items."""
    root = bytearray(0x17C)
    root[0x0:0x10] = b'wphi' + struct.pack('>III', 0, 0, 0xFFFFFFFF)  # empty parent reference
    root[0x60:0x6C] = block(1)
    root[0x78:0x84] = block(1)
    root[0x84:0x90] = block(1)
    static = bytearray(0xB4)
    static[0x24:0x48] = placement(100, -50, 1.0, 2.0, static_flags)
    r, a = ref('ui\\hud\\a')
    static[0x48:0x58] = r
    number = bytearray(0xA0)
    number[0x24:0x48] = placement(40, 30, 1.0, 1.0, 0)
    cross = bytearray(0x68)
    r, b_path = ref('ui\\hud\\b')
    cross[0x24:0x34] = r
    cross[0x34:0x40] = block(2)
    items = b''.join(
        placement(10, -7, 1.0, 1.0, f) + bytes(0x6C - 0x24) for f in item_flags)
    return bytes(header(b"wphi") + root + static + a + number + cross + b_path + items)


def lookup(half_paths):
    return lambda path: path in half_paths


def read_placements(data):
    return hs.understand(data, b'wphi')


def values(data, off):
    x, y, w, h, flags = struct.unpack_from('>hhffH', data, off)
    return x, y, w, h, flags


def test_factors_and_offsets():
    data = weapon_tag()
    new, full, only = hs.scale_tag(data, b'wphi', lookup({'ui\\hud\\b'}))
    assert (full, only) == (3, 1)
    p = read_placements(data)
    # the static element: bitmap A is not half: 0.5 on both scales, offsets halved
    off, mode, bitmap = p[0]
    assert (mode, bitmap) == ('full', 'ui\\hud\\a')
    assert values(new, off) == (50, -25, 0.5, 1.0, 0)
    # the number element: offset only, scale kept
    off = [o for o, m, _ in p if m == 'offset'][0]
    assert values(new, off) == (20, 15, 1.0, 1.0, 0)
    # the crosshair items use the crosshair's bitmap B, which is half: 0.25
    items = [o for o, m, b in p if b == 'ui\\hud\\b']
    assert len(items) == 2
    assert values(new, items[0]) == (5, -3, 0.25, 0.25, 0)


def test_flag_four_keeps_offset_and_is_cleared():
    data = weapon_tag(item_flags=(0, 4))
    new, _, _ = hs.scale_tag(data, b'wphi', lookup(set()))
    items = [o for o, m, b in read_placements(data) if b == 'ui\\hud\\b']
    assert values(new, items[0])[:2] == (5, -3)       # no flag: halved
    assert values(new, items[1]) == (10, -7, 0.5, 0.5, 0)  # flag 4: offset kept, flag cleared, scale still scaled


def test_other_flags_survive():
    data = weapon_tag(static_flags=4 | 3)
    new, _, _ = hs.scale_tag(data, b'wphi', lookup(set()))
    off = read_placements(data)[0][0]
    assert values(new, off) == (100, -50, 0.5, 1.0, 3)


def test_missing_bitmap_is_not_understood():
    data = weapon_tag()
    try:
        hs.scale_tag(data, b'wphi', lambda path: None)
    except hs.NotUnderstood:
        return
    raise AssertionError('a missing bitmap must stop the tag')


def test_truncated_tag_is_not_understood():
    data = weapon_tag()[:-40]
    try:
        hs.scale_tag(data, b'wphi', lookup(set()))
    except hs.NotUnderstood:
        return
    raise AssertionError('a cut tag must be reported')


def test_non_hud_tag_untouched(tmp_path):
    other = tmp_path / 'x' / 'thing.weapon'
    other.parent.mkdir()
    blob = bytes(range(200))
    other.write_bytes(blob)
    hs.run(str(tmp_path))
    assert other.read_bytes() == blob


def test_run_rewrites_hud_tag_and_leaves_bad_one(tmp_path, capsys):
    bm = tmp_path / 'ui' / 'hud'
    bm.mkdir(parents=True)
    for name, flags in (('a', 0), ('b', 0x10)):
        t = bytearray(80)
        struct.pack_into('>H', t, hs.HEAD + 6, flags)
        (bm / (name + '.bitmap')).write_bytes(bytes(t))
    good = tmp_path / 'good.weapon_hud_interface'
    good.write_bytes(weapon_tag())
    bad = tmp_path / 'bad.weapon_hud_interface'
    bad.write_bytes(weapon_tag()[:-40])
    bad_bytes = bad.read_bytes()
    hs.run(str(tmp_path))
    out = capsys.readouterr().out
    assert 'hud scaled good.weapon_hud_interface' in out
    assert 'layout not understood, left alone bad.weapon_hud_interface' in out
    assert bad.read_bytes() == bad_bytes
    p = read_placements(good.read_bytes())
    item = [o for o, m, b in p if b == 'ui\\hud\\b'][0]
    assert values(good.read_bytes(), item)[2] == 0.25
