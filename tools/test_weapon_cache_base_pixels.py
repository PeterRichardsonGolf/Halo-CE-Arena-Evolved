"""The cache's base layer bitmaps hold the stock map's pixels where the game reads them: at the bitmap's own
pixels_offset (a retail map's bitmaps come with the cached bit set, so texture_cache_bitmap_new never adds the group's
file_offset). A builder that summed file_offset + pixels_offset cut the pixels file_offset bytes too far on: every
ae\\base decal, impact particle and HUD bitmap drew as garbage (11-05, black decals under the dead).

Needs a built cache and the stock map it was cut from, given by AE_CACHE and AE_STOCK_MAP; skipped without them.
"""
import os
import struct
import sys
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "weapon_pack"))
import aepack  # noqa: E402
import donor_map  # noqa: E402

BITMAP_ELEMENTS_AT, PIXELS_OFFSET_AT = 100, 0x18


class BasePixels(unittest.TestCase):
    @unittest.skipUnless(os.environ.get("AE_CACHE") and os.environ.get("AE_STOCK_MAP"), "AE_CACHE and AE_STOCK_MAP")
    def test_base_bitmaps_are_the_stock_pixels(self):
        image = open(os.environ["AE_CACHE"], "rb").read()
        parsed = aepack.read_cache(image)
        stock = donor_map.MapFile(os.environ["AE_STOCK_MAP"])
        tag_bytes = parsed.sections[aepack.TAG_BYTES]
        checked = 0
        for index, tag in enumerate(parsed.tags):
            name_at = tag[3]
            name = parsed.strings[name_at:parsed.strings.index(b"\0", name_at)].decode()
            if tag[0] != aepack.group("bitm") or not name.startswith("ae\\base\\"):
                continue
            body = tag_bytes[tag[9]:tag[9] + tag[10]]
            fixups = {f[0]: f[3] for f in parsed.fixups[tag[11]:tag[11] + tag[12]]}
            if BITMAP_ELEMENTS_AT not in fixups:
                continue
            stock_tag = stock.tags.find("bitm", name[len("ae\\base\\"):])
            self.assertIsNotNone(stock_tag, name)
            stock_body = stock.tags.read(stock_tag.address, tag[10])
            stock_first = struct.unpack_from("<I", stock_body, BITMAP_ELEMENTS_AT)[0]
            for number in range(struct.unpack_from("<I", body, 96)[0]):
                at = fixups[BITMAP_ELEMENTS_AT] + number * 48
                offset, size = struct.unpack_from("<ii", body, at + PIXELS_OFFSET_AT)
                stock_offset = struct.unpack_from("<i", stock.tags.read(stock_first + number * 48, 48), PIXELS_OFFSET_AT)[0]
                resource = next(r for r in parsed.resources if r[0] == index and r[1] <= offset and offset + size <= r[1] + r[2])
                data = aepack.cache_read_resource_data(image, parsed, resource)[offset - resource[1]:offset - resource[1] + size]
                self.assertEqual(data, stock.file_bytes(stock_offset, size), "%s element %d" % (name, number))
                checked += 1
        self.assertGreater(checked, 0)


if __name__ == "__main__":
    unittest.main()
