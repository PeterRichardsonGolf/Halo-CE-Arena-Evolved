"""A weapon HUD interface tag made for Halo PC, for a map built for the Xbox: every element flagged "use high res
scale" (a Halo PC flag the Xbox's build refuses) has its width and height scale halved and the flag cleared, as
the engine's own ce_hud.c does when it loads a Custom Edition map. An unset scale (0) is 1.

It works through tag_fields.py's editor (Invader's field editor, a separate program).
Usage: hud_hires_to_xbox.py <invader build folder> <tags folder> <tag>
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))    # (found when run with python -I, too)
import tag_fields

FLAG = ".scaling_flags.use_high_res_scale"
ELEMENT_BLOCKS = ("static_elements", "meter_elements", "number_elements")
# blocks whose elements hold the flagged elements: (block, the block inside each)
NESTED_BLOCKS = (("crosshairs", "crosshair_overlays"), ("overlay_elements", "overlays"))


def halved(text):
    """a scale as the editor prints it, halved, as the editor takes it"""
    value = float(text)
    return str((value if value else 1.0) * 0.5)


def _element(editor, key, done):
    if editor.get(key + FLAG) == "1":
        for scale in ("width_scale", "height_scale"):
            editor.set(key + "." + scale, halved(editor.get(key + "." + scale)))
        editor.set(key + FLAG, "0")
        done.append(key)


def convert(editor):
    """the elements changed, in order"""
    done = []
    for block in ELEMENT_BLOCKS:
        for index in range(editor.count(block)):
            _element(editor, "%s[%d]" % (block, index), done)
    for block, inner in NESTED_BLOCKS:
        for index in range(editor.count(block)):
            for inner_index in range(editor.count("%s[%d].%s" % (block, index, inner))):
                _element(editor, "%s[%d].%s[%d]" % (block, index, inner, inner_index), done)
    return done


def main(arguments):
    if len(arguments) != 3:
        print(__doc__.strip().splitlines()[-1], file=sys.stderr)
        return 2
    try:
        for key in convert(tag_fields.Editor(*arguments)):
            print("%s: halved" % key)
    except tag_fields.Failed as failure:
        print("hud_hires_to_xbox: failed: %s" % failure, file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
