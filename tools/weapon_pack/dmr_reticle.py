"""The DMR's aiming reticle (our own art, from Kenney's crosshair 033, CC0, the battle rifle pack's reticle picture), in
two parts so the HUD can kick the outer ring out with each shot while the middle stays on the target
(ae_reticle_growth.c; the owner 10-30: "the same size as the evo crosshair and an animation pass"): the dot and the
inner ring (the plate's first bitmap, the fixed item) and the outer ring broken at the axes (the second). Each part
64 x 64, the lines a texel bolder than the picture's (alpha widened and made solid at 128, then halved), so they stay
crisp at the size the HUD draws them; colour and alpha both the line's strength (the HUD adds the colour). A colour
plate for invader-bitmap as the battle rifle's: the two pictures one above the other on a blue (0 0 255) background, a
texel of it round each, so each is a sequence of its own. At the recipe's crosshair scale (0.267) the outer ring is as
big as the EVO RIFLE's (its edge 28.5 of 64 texels at 0.30).
Usage: dmr_reticle.py <the 512 x 512 reticle picture> <out png>"""
import sys


N = 64
SPLIT = 22.0                      # the radius (texels of 64) between the middle and the outer ring


def draw(alpha512):
    """the plate (a Pillow image) from the 512 x 512 alpha (bytes, a numpy array)"""
    import numpy as np
    from PIL import Image, ImageFilter
    small = np.asarray(Image.fromarray(alpha512, "L").resize((128, 128), Image.LANCZOS))
    a = small.astype(float) / 255.0
    wide = Image.fromarray((a * 255).astype(np.uint8)).filter(ImageFilter.MaxFilter(3))
    wide = np.asarray(wide).astype(float) / 255.0
    wide = np.clip((wide * 0.5 + a * 0.5) * 2.0, 0, 1)         # solid core, soft edge
    half = np.asarray(Image.fromarray((wide * 255).round().astype(np.uint8)).resize((N, N), Image.LANCZOS))
    half = half.astype(float) / 255.0
    y, x = np.mgrid[0:N, 0:N]
    middle = np.hypot(x + 0.5 - N / 2, y + 0.5 - N / 2) < SPLIT
    plate = np.zeros((2 * N + 3, N + 2, 4), np.uint8)
    plate[...] = (0, 0, 255, 255)
    for row, cover in ((1, half * middle), (N + 2, half * ~middle)):
        value = (cover * 255).round().astype(np.uint8)
        plate[row:row + N, 1:N + 1] = np.dstack([value] * 4)
    return Image.fromarray(plate, "RGBA")


if __name__ == "__main__":
    import numpy as np
    from PIL import Image
    draw(np.array(Image.open(sys.argv[1]).convert("RGBA").getchannel("A"))).save(sys.argv[2])
