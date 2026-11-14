"""OpenCE's vertical metrics (port/assets/fonts/OpenCE-Regular.ttf, built by tools/title_font.py): its hhea and OS/2
typo descents are negative, as descents are (Newtown's were +190, which put a line box's bottom above the baseline),
and nothing else in the font changed with that fix. Needs fontTools (as title_font.py does): skipped without it."""
import hashlib
import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parent.parent
FONT = ROOT / "port" / "assets" / "fonts" / "OpenCE-Regular.ttf"

TTFont = pytest.importorskip("fontTools.ttLib", reason="needs fontTools (python-fonttools), as tools/title_font.py "
                                                         "does").TTFont

# the bytes left out of a table's hash: head's checkSumAdjustment and modified (they change on every save), hhea's
# descent and OS/2's sTypoDescender (the fix)
BLANK = {"head": ((8, 12), (28, 36)), "hhea": ((6, 8),), "OS/2": ((70, 72),)}
# each table's SHA-256 in the font before the fix (b8068715), with BLANK's bytes zeroed
BEFORE_THE_FIX = {
    "OS/2": "dfceed4ac2c3b1d7d00b0d8127acecfc92e53f478a49d8b1cd19d6552916fed3",
    "cmap": "7c412f46f21acec37a6f92408c5016b4597f73c8ef8c3336e9d6cb723f4af4ff",
    "cvt ": "d8e2c66b09c13b52f6d2d21f1a97885d525ae3780fb243b16dca6f3c8ecb14e4",
    "fpgm": "a113b1a3d6f20dfc0c420936b87d6a7e6a51fbe12062af96fe75caaf7a65b8c9",
    "glyf": "eb57f2d97fc593a425916b49c71c45d862f869eb2f65cff676ab49af0b571db7",
    "head": "ee912cd08f3f41f2d8f6d3722aacad0adc7367cabbd5a93605e699155f56298a",
    "hhea": "bc8171b80319eafccc2cab8a86d3db455d16b3ea9c9adc58e92d194a609aa9ba",
    "hmtx": "baea3063e9f53bb90af2cdcac4a81a426f7b92e3f8e32cf9a1d4905bf26032aa",
    "kern": "c627e4249f35b16dc75d825bcaf4e9d013936c76ef18b1c14a5cf117a9c1aab2",
    "loca": "d14b0fee433b11b7795f6e73aeb483910b4db33852bf607ea3f9d572b36f08d9",
    "maxp": "ecf4df465e0343b48ef03fd6b0089615d2d909e1dbe5ba1f788ee29f05ac9b55",
    "name": "65da32e3a88be1ad389ecf2db37b8297456976d8abfc08ff57e7bf87017a6757",
    "post": "5caab71f4d124e101006bf400e819db2727b665b72caa6b227a7f6e584ff255d",
    "prep": "f4622b035b2a8b4e553321d7c10b8ebd7f9b080fb963450b3c60a3014355b761",
}


def tables(path):
    """a font file's tables: {tag: its bytes as in the file}"""
    reader = TTFont(path, lazy=True).reader
    return {tag: bytes(reader[tag]) for tag in reader.keys()}


def blanked(tag, data):
    data = bytearray(data)
    for start, end in BLANK.get(tag, ()):
        data[start:end] = bytes(end - start)
    return bytes(data)


def test_descents_negative():
    font = TTFont(FONT)
    assert font["hhea"].descent == -190 and font["OS/2"].sTypoDescender == -190
    assert (font["OS/2"].usWinAscent, font["OS/2"].usWinDescent, font["head"].yMin) == (964, 234, -234)
    # (the rest of the vertical metrics as they were)
    assert (font["hhea"].ascent, font["hhea"].lineGap, font["OS/2"].sTypoAscender, font["OS/2"].sTypoLineGap) == \
        (729, 0, 729, 0)


def test_only_the_descents_changed():
    """every table of the committed font, its descent fields and head's save stamps left out, is byte-identical to
    the font's before the fix: every glyph, advance, kerning pair, cmap and name entry"""
    found = {tag: hashlib.sha256(blanked(tag, data)).hexdigest() for tag, data in tables(FONT).items()}
    assert sorted(found) == sorted(BEFORE_THE_FIX)
    changed = [tag for tag in found if found[tag] != BEFORE_THE_FIX[tag]]
    assert not changed, f"tables changed besides the descents: {changed}"


def test_build_is_current(tmp_path):
    """tools/title_font.py writes the committed font: every table byte-identical but head's save stamps"""
    sys.path.insert(0, str(ROOT / "tools"))
    try:
        import title_font
    finally:
        sys.path.remove(str(ROOT / "tools"))
    built = tables(title_font.build(tmp_path / "OpenCE-Regular.ttf"))
    committed = tables(FONT)
    assert sorted(built) == sorted(committed)
    for tag in committed:
        expected = blanked(tag, committed[tag]) if tag == "head" else committed[tag]
        got = blanked(tag, built[tag]) if tag == "head" else built[tag]
        assert got == expected, f"{tag} differs: run python3 tools/title_font.py"
