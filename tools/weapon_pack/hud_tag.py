"""The battle rifle's ammunition HUD: the edits that turn a copy of the stock assault rifle's weapon HUD interface
(CE's own top-left panel: numbers, master rounds, reload warnings) into the rifle's.

- Its crosshair goes (the rifle's own interface, the parent of this one, has the crosshairs).
- Its round meter becomes two meter elements. The FIRST keeps the rifle's icon sheet (hud_sheet.py's, whose
  sequence 0 is the side view the swap and pickup prompts show) and is placed off the screen: the engine takes
  the prompt's picture from the pack's weapon entry, which names that sheet, and a sheet no element names would
  not be in the pack. The SECOND is the counter: the sharp sheet, four texels a HUD unit, drawn at a quarter of
  the scale.
- The low-ammo flash's static element takes the counter's alpha sheet at the same scale.
- No art of the game's is in it: it is the stock interface's structure with the positions, sizes and references
  set here.

The edits are tag_fields.py's lists; make_pack.py applies them.
Usage: hud_tag.py base|counter     prints the edits as JSON (the names and numbers are the recipe's)
"""
import json
import sys


def _number(value):
    return str(value)


def base_edits(icon_sheet):
    """the stock assault rifle's interface as the rifle's first HUD: no crosshair, its meter the icon sheet"""
    return [["erase", "crosshairs[*]"],
            ["set", "meter_elements[0].meter_bitmap", icon_sheet]]


def counter_edits(alphas, meter, scale, anchor, off_screen, icon_sequence=0):
    """from the first HUD to the one with the sharp counter: the meter element copied, the copy the counter, the
    first one (the icon sheet's: its sequence the icon's) put off the screen"""
    scale = _number(scale)
    return [["copy", "meter_elements[0]", "end"],
            ["sets", [["static_elements[0].interface_bitmap", alphas],
                      ["static_elements[0].sequence_index", "0"],
                      ["static_elements[0].width_scale", scale],
                      ["static_elements[0].height_scale", scale],
                      ["static_elements[0].anchor_offset", "%d %d" % tuple(anchor)],
                      ["meter_elements[1].meter_bitmap", meter],
                      ["meter_elements[1].sequence_index", "0"],
                      ["meter_elements[1].width_scale", scale],
                      ["meter_elements[1].height_scale", scale]]],
            ["set", "meter_elements[0].anchor_offset", "%d %d" % tuple(off_screen)],
            ["set", "meter_elements[0].sequence_index", "%d" % icon_sequence]]


def main(arguments):
    if arguments == ["base"]:
        edits = base_edits("ae_pack\\hud\\br_meter.bitmap")
    elif arguments == ["counter"]:
        edits = counter_edits("ae_pack\\hud\\br_alphas_smooth.bitmap", "ae_pack\\hud\\br_meter_smooth.bitmap", 0.25,
                              (-1, 20), (-3000, -3000))
    else:
        print(__doc__.strip().splitlines()[-1], file=sys.stderr)
        return 2
    print(json.dumps(edits, indent=1))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
