#!/usr/bin/env python3
"""The frame lint: what a person sees wrong on an AE menus frame, read from the frame's boxes, not by eye.

    python3 tools/ae_test/frame_lint.py <list>-<test>.boxes.json... [--json]

The boxes are written by the game beside each scene picture of a scripted TEST RUN (ae_test_run.c: test-runs/preflight/
<list>-<test>.boxes.json; ae_draw.c boxes_write): every shape, image and text run the AE menus drew that frame, in the
order drawn, in the layout's units, with colours (0xRRGGBBAA) and clips. Three findings:

  CUT       a text run wider than its clip: its end is cut off
  PAST      a text run that runs past the right or bottom edge of the panel it starts in (the smallest filled shape
            drawn before it that holds its first letters)
  OVERLAP   two text runs drawn over each other (their boxes share more than a fifth of the smaller one); the same
            text drawn twice a few units apart (a shadow) is not one
  CONTRAST  a text run's colour against what lies under it (the filled shapes drawn before it, blended from black)
            under 4.5:1, or under 3:1 for large text (24 px and up)

Exit 1 when there is a finding. preflight.py runs it on each test's boxes and lists the findings as FRAME lines.
"""
import argparse
import json
import sys
from pathlib import Path

SHADOW = 3.0       # (units) the same text this near is its shadow
OVERLAP_SHARE = 0.2
LARGE_PX = 24.0


def rgba(value):
    return ((value >> 24) & 255) / 255, ((value >> 16) & 255) / 255, ((value >> 8) & 255) / 255, (value & 255) / 255


def luminance(rgb):
    def channel(c):
        return c / 12.92 if c <= 0.03928 else ((c + 0.055) / 1.055) ** 2.4
    r, g, b = (channel(c) for c in rgb)
    return 0.2126 * r + 0.7152 * g + 0.0722 * b


def contrast(a, b):
    high, low = sorted((luminance(a), luminance(b)), reverse=True)
    return (high + 0.05) / (low + 0.05)


def blend(under, color):
    r, g, b, a = rgba(color)
    return tuple(u * (1 - a) + c * a for u, c in zip(under, (r, g, b)))


def holds(box, x, y):
    return box["x"] <= x <= box["x"] + box["w"] and box["y"] <= y <= box["y"] + box["h"]


def filled(box):
    return box["k"] == "shape" and box.get("thickness", 0) == 0 and (box.get("top", 0) & 255) > 0 \
        and box["w"] > 0 and box["h"] > 0


def findings(boxes, overlay=None):
    """[(kind, text, words)] of one frame's boxes; overlay: the box (x0, y0, x1, y1) of what is drawn over the screen on
    purpose (TEST RUN's card): its own text and the text under it are left alone."""
    found = []

    def under_overlay(box):
        return overlay is not None and box["x"] < overlay[2] and box["x"] + box["w"] > overlay[0] and \
            box["y"] < overlay[3] and box["y"] + box["h"] > overlay[1]

    texts = [box for box in boxes if box["k"] == "text" and box.get("text", "").strip() and box["w"] > 0
             and not under_overlay(box)]
    for text in texts:
        label = text["text"][:40]
        x0, y0, x1, y1 = text["clip"]
        if text["x"] + text["w"] > x1 + 0.5 or text["x"] < x0 - 0.5:
            found.append(("CUT", label, f"{text['w']:.0f} wide in a clip of {x1 - x0:.0f}: its end is cut off"))
        under = [box for box in boxes if box["i"] < text["i"] and filled(box)]
        start_x, mid_y = text["x"] + min(4.0, text["w"] / 2), text["y"] + text["h"] / 2
        # (its panel: the last filled shape drawn under its first letters, the one it is seen on (a dialog over a screen
        # is its text's panel, not the screen's rows behind it); bigger than a key cap or a chip beside the text: at least
        # 1.5 lines high and 3 wide)
        panels = [box for box in under if holds(box, start_x, mid_y) and box["h"] >= text["h"] * 1.5
                  and box["w"] >= text["h"] * 3]
        if panels:
            panel = max(panels, key=lambda box: box["i"])
            over = text["x"] + text["w"] - (panel["x"] + panel["w"])
            if over > 1.0:
                found.append(("PAST", label, f"runs {over:.0f} past the right edge of its panel"))
            elif text["y"] + text["h"] - (panel["y"] + panel["h"]) > text["h"] * 0.35:
                found.append(("PAST", label, "runs past the bottom edge of its panel"))
        # (what lies under its middle: the filled shapes before it, in their order, over black)
        color = (0.0, 0.0, 0.0)
        for box in under:
            if holds(box, text["x"] + text["w"] / 2, mid_y):
                color = blend(color, box["top"])
        r, g, b, a = rgba(text["color"])
        ink = blend(color, text["color"]) if a < 1 else (r, g, b)
        ratio = contrast(ink, color)
        wanted = 3.0 if text["h"] * 1.0 >= LARGE_PX else 4.5
        # (a glyph's one or two letters on its own chip are its picture, judged with the chip)
        if a > 0 and ratio < wanted and len(text["text"].strip()) > 2:
            found.append(("CONTRAST", label, f"{ratio:.2f}:1 against what is under it (wanted {wanted}:1)"))
    def hidden(text):
        """covered by a mostly opaque filled shape drawn after it (a dialog over the screen behind)"""
        middle_x, middle_y = text["x"] + text["w"] / 2, text["y"] + text["h"] / 2
        return any(box["i"] > text["i"] and filled(box) and (box["top"] & 255) >= 0xB3 and holds(box, middle_x, middle_y)
                   for box in boxes)

    shown = [text for text in texts if not hidden(text)]
    for index, first in enumerate(shown):
        for second in shown[index + 1:]:
            if first["text"] == second["text"] and abs(first["x"] - second["x"]) <= SHADOW and \
                    abs(first["y"] - second["y"]) <= SHADOW:
                continue
            width = min(first["x"] + first["w"], second["x"] + second["w"]) - max(first["x"], second["x"])
            height = min(first["y"] + first["h"], second["y"] + second["h"]) - max(first["y"], second["y"])
            if width <= 0 or height <= 0:
                continue
            smaller = min(first["w"] * first["h"], second["w"] * second["h"])
            if smaller > 0 and width * height / smaller > OVERLAP_SHARE:
                found.append(("OVERLAP", first["text"][:40], f"drawn over \"{second['text'][:40]}\""))
    return found


def lint_file(path):
    data = json.loads(Path(path).read_text(errors="replace"))
    return findings(data.get("boxes", []), data.get("overlay"))


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("files", nargs="+")
    parser.add_argument("--json", action="store_true")
    args = parser.parse_args(argv)
    every = {}
    for name in args.files:
        every[name] = lint_file(name)
    if args.json:
        print(json.dumps(every, indent=1))
    else:
        for name, found in every.items():
            for kind, text, words in found:
                print(f"{kind:8} {Path(name).name}: \"{text}\" {words}")
    return 1 if any(every.values()) else 0


if __name__ == "__main__":
    sys.exit(main())
