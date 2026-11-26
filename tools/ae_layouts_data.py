#!/usr/bin/env python3
"""Writes port/linux/game/ae_layouts_data.c: the map layouts AE ships, port/assets/ae/layouts.txt, as C data
(ae_layout.c reads it for a map with no layout file beside it):

    python3 tools/ae_layouts_data.py           # writes the file
    python3 tools/ae_layouts_data.py --check   # exit 1 when the committed file differs from what it would write

The data are packed as tools/ae_maps_data.py packs them."""

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import ae_maps_data  # noqa: E402

ae_maps_data.SOURCES = [(Path("port/assets/ae/layouts.txt"), "ae_layouts_data")]
ae_maps_data.OUTPUT = Path("port/linux/game/ae_layouts_data.c")


_maps_text = ae_maps_data.text


def text() -> str:
    """The generated file's text."""
    return _maps_text().replace("tools/ae_maps_data.py", "tools/ae_layouts_data.py")


if __name__ == "__main__":
    ae_maps_data.text = text
    sys.exit(ae_maps_data.main())
