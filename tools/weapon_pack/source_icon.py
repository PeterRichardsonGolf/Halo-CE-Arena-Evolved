"""The prompts' picture of a weapon from a source's own HUD picture of it (a bitmap tag of the source pack, read by
hek.py): its coverage (alpha times brightness), cropped to what it covers and scaled to the width the prompts draw.
No line is drawn here: every pixel is the source's, resampled. The art stage puts it on a colour plate as a sprite
(hud_sheet.plate), as the other packs' pictures.

    source_icon.py <bitmap tag> <width> <out.png>
"""
import sys

import hek

# a texel counts as the picture's from this coverage up (0-255): the faint edges of the source's own drawing kept,
# its empty background cut
EDGE = 8


class Refused(Exception):
    """not a picture this takes, with the reason"""


def picture(tag, width, index=0, sequence=None):
    """the source bitmap's picture (its first, the one at index of a sheet of pictures, or its sequence's) as a coverage
    array (rows x columns, 0-255), cropped and scaled to width"""
    import numpy as np
    from PIL import Image
    columns, rows, rgba = hek.bitmap_rgba(tag, index) if sequence is None else hek.sequence_bitmap_rgba(tag, sequence)
    pixels = np.frombuffer(rgba, np.uint8).reshape(rows, columns, 4).astype(np.float32)
    brightness = pixels[..., :3].max(axis=2)
    coverage = brightness * pixels[..., 3] / 255.0
    rows_on = np.where(coverage.max(axis=1) >= EDGE)[0]
    columns_on = np.where(coverage.max(axis=0) >= EDGE)[0]
    if not len(rows_on) or not len(columns_on):
        raise Refused("the picture is empty")
    cropped = coverage[rows_on[0]:rows_on[-1] + 1, columns_on[0]:columns_on[-1] + 1]
    height = max(1, int(round(cropped.shape[0] * width / float(cropped.shape[1]))))
    scaled = Image.fromarray(np.clip(cropped, 0, 255).astype(np.uint8), "L").resize((width, height), Image.LANCZOS)
    return np.asarray(scaled, np.uint8)


def main(arguments):
    if len(arguments) != 4:
        print(__doc__.strip().splitlines()[-1], file=sys.stderr)
        return 2
    import hud_sheet
    with open(arguments[1], "rb") as file:
        image = hud_sheet.plate(picture(file.read(), int(arguments[2])))
    image.save(arguments[3])
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
