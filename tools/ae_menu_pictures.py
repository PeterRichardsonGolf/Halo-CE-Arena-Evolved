#!/usr/bin/env python3
"""Arena Evolved's own pictures for the PC menus' frames that were PLACE HOLDER (the PC version's pictures, which
nothing here may ship): the Multiplayer screen's (main_menu/multiplayer_type_select/mp_options, every entry) and the
Edit Gametype screen's (.../playlist_edit/gametype_options: CHANGE NAME, ITEM, VEHICLE, TEAMPLAY, ARENA and NHE
OPTIONS; GAME, PLAYER and INDICATOR OPTIONS keep their pictures). In the menus' redraw style: black silhouettes, a
bright blue edge and a soft glow, as GAME TYPE's icons (svg/shell/bitmaps/game_type_grafix__*.svg). Every shape is
simple geometry drawn here for Arena Evolved (CC0 with the rest of the code); nothing is traced.

A screen shows the frame whose number is the focused entry's place among the entries that take events
(ui_widget_game_data_input_functions.c's update_extended_description functions; the bitmap's frames wrap): the
Multiplayer screen's SERVER BROWSER, LAN, DIRECT LINK, INTERNET, LAN, CO-OP CAMPAIGN, EDIT GAMETYPES are 0-6, the Edit
Gametype screen's CHANGE NAME ... NHE OPTIONS 0-8 (NHE OPTIONS is a ninth frame, added: it wrapped to CHANGE NAME's).
tools/port_settings.py's OWN_FRAMES gives them to tools/ce_menus.py, which writes each SVG to port_svg/ and draws it
to ce/port/ (Art.own_frame). As ce_menus.py needs Halo PC's tags, this file can write the same files itself:

    python3 tools/ae_menu_pictures.py --write

(then check port/assets/menus/ce/bitmaps.xml and menus.json list them: tools/test_ae_menu_pictures.py).
"""
import argparse
import subprocess
from pathlib import Path

BLUE = "#2896FF"
RED = "#FF3A3A"
# the logical frame (the PC version's: 512 by 256, of which the widget shows the top left, 307 by 244 on the
# Multiplayer screen and 284 by 208 on Edit Gametype, the description's text from about 190 down) and the drawing's
# size there (its 100 by 100 box), centred where the PC version's picture (and the placeholder) was
FRAME = (512, 256)
ICON_SIZE = 160
MULTIPLAYER = "main_menu/multiplayer_type_select/mp_options"
GAMETYPE = "main_menu/settings_select/multiplayer_setup/playlist_edit/gametype_options"
CENTRES = {MULTIPLAYER: (141, 96), GAMETYPE: (142, 90)}


def _globe(cx, cy, r):
    shapes = [f'<circle cx="{cx}" cy="{cy}" r="{r}" class="b"/>']
    for k in (0.35, 0.7):
        shapes.append(f'<ellipse cx="{cx}" cy="{cy}" rx="{r * k:.1f}" ry="{r}" class="l"/>')
    for dy in (-0.5, 0, 0.5):
        y = cy + dy * r
        w = r * (1 - dy * dy) ** 0.5
        shapes.append(f'<line x1="{cx - w:.1f}" y1="{y:.1f}" x2="{cx + w:.1f}" y2="{y:.1f}" class="l"/>')
    return shapes


def _monitor(x, y, w, h):
    return [f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="2" class="b"/>',
            f'<rect x="{x + 3}" y="{y + 3}" width="{w - 6}" height="{h - 6}" rx="1" class="l"/>',
            f'<path d="M{x + w / 2 - 4},{y + h} l-3,7 h14 l-3,-7 z" class="b"/>']


def _rack(x, y, w, h):
    shapes = [f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="2" class="b"/>']
    bays = 4
    for i in range(bays):
        by = y + 4 + i * (h - 8) / bays
        shapes.append(f'<rect x="{x + 4}" y="{by:.1f}" width="{w - 8}" height="{(h - 8) / bays - 3:.1f}" rx="1" '
                      'class="l"/>')
        shapes.append(f'<circle cx="{x + w - 9}" cy="{by + ((h - 8) / bays - 3) / 2:.1f}" r="1.6" class="f"/>')
    return shapes


def _helmet(cx, cy, k):
    outline = (f"M{cx - 16 * k},{cy + 14 * k} C{cx - 20 * k},{cy - 4 * k} {cx - 16 * k},{cy - 22 * k} {cx},{cy - 22 * k} "
               f"C{cx + 16 * k},{cy - 22 * k} {cx + 20 * k},{cy - 4 * k} {cx + 16 * k},{cy + 14 * k} "
               f"L{cx + 8 * k},{cy + 20 * k} L{cx - 8 * k},{cy + 20 * k} Z")
    visor = (f"M{cx - 13 * k},{cy - 6 * k} Q{cx},{cy - 11 * k} {cx + 13 * k},{cy - 6 * k} "
             f"L{cx + 11 * k},{cy + 4 * k} Q{cx},{cy + 1 * k} {cx - 11 * k},{cy + 4 * k} Z")
    return [f'<path d="{outline}" class="b"/>', f'<path d="{visor}" class="f"/>']


def _bust(cx, cy, k):
    """a player's head and shoulders: a rounded helmet, its visor, broad shoulder plates"""
    shoulders = (f"M{cx - 30 * k},{cy + 40 * k} L{cx - 28 * k},{cy + 22 * k} Q{cx - 26 * k},{cy + 12 * k} "
                 f"{cx - 14 * k},{cy + 10 * k} L{cx + 14 * k},{cy + 10 * k} Q{cx + 26 * k},{cy + 12 * k} "
                 f"{cx + 28 * k},{cy + 22 * k} L{cx + 30 * k},{cy + 40 * k} Z")
    return [f'<path d="{shoulders}" class="b"/>',
            f'<path d="M{cx - 12 * k},{cy + 18 * k} H{cx + 12 * k}" class="l"/>'] + _helmet(cx, cy - 4 * k, 0.8 * k)


def _shield(cx, cy, w, h, cls):
    outline = (f"M{cx},{cy - h / 2} L{cx + w / 2},{cy - h / 2 + h * 0.18} L{cx + w / 2},{cy + h * 0.05} "
               f"Q{cx + w / 2},{cy + h * 0.35} {cx},{cy + h / 2} Q{cx - w / 2},{cy + h * 0.35} {cx - w / 2},{cy + h * 0.05} "
               f"L{cx - w / 2},{cy - h / 2 + h * 0.18} Z")
    return [f'<path d="{outline}" class="{cls}"/>']


# each entry's drawing, in a 100 by 100 box (class b: black with the blue edge; l: a thin blue line; w: a thick one;
# f: blue filled; red: the red team's edge)
MULTIPLAYER_ICONS = [
    # SERVER BROWSER: the world and a list of games
    ("server_browser", _globe(38, 46, 32) + ['<rect x="52" y="36" width="44" height="54" rx="3" class="b"/>'] +
     [f'<rect x="57" y="{42 + i * 11}" width="34" height="7" rx="1.5" class="l"/>' for i in range(4)]),
    # LAN (join): two screens on one switch
    ("lan_join", _monitor(4, 14, 38, 28) + _monitor(58, 14, 38, 28) + [
        '<path d="M23,49 V66 H77 V49" class="w"/>', '<line x1="50" y1="66" x2="50" y2="74" class="w"/>',
        '<rect x="32" y="74" width="36" height="14" rx="2" class="b"/>'] +
     [f'<circle cx="{39 + i * 7}" cy="81" r="1.6" class="f"/>' for i in range(4)]),
    # DIRECT LINK: two screens straight to each other, an address under them
    ("direct_link", _monitor(0, 22, 34, 26) + _monitor(66, 22, 34, 26) + [
        '<line x1="34" y1="35" x2="66" y2="35" class="w"/>', '<path d="M57,29 L63,35 L57,41" class="w"/>',
        '<path d="M43,29 L37,35 L43,41" class="w"/>', '<rect x="22" y="64" width="56" height="20" rx="3" class="b"/>'] +
     [f'<circle cx="{x}" cy="74" r="2.2" class="f"/>' for x in (29, 36, 43, 57, 64, 71)] +
     ['<line x1="50" y1="68" x2="50" y2="80" class="l"/>']),
    # INTERNET (create): a host before the world
    ("internet_host", _globe(58, 44, 34) + _rack(10, 30, 34, 60)),
    # LAN (create): a host wired to three screens
    ("lan_host", _rack(36, 4, 28, 50) + ['<path d="M50,54 V64 M16,64 H84 M16,64 V70 M50,64 V70 M84,64 V70" class="w"/>'] +
     _monitor(2, 70, 28, 18) + _monitor(36, 70, 28, 18) + _monitor(70, 70, 28, 18)),
    # CO-OP CAMPAIGN: two players side by side
    ("coop", _bust(64, 44, 1.15) + _bust(36, 50, 1.15)),
    # EDIT GAMETYPES: settings' sliders and a flag
    ("edit_gametypes", [f'<line x1="{x}" y1="22" x2="{x}" y2="90" class="w"/>' for x in (22, 50, 78)] +
     [f'<rect x="{x - 9}" y="{y - 6}" width="18" height="12" rx="3" class="b"/>'
      for x, y in ((22, 70), (50, 38), (78, 58))] + ['<path d="M86,4 V30 M86,6 L100,11 L86,17" class="w"/>']),
]

# the Edit Gametype screen's, by frame (the others are the PC version's or the Xbox's own)
GAMETYPE_ICONS = {
    # CHANGE NAME: a name field and a pen
    0: ("name", ['<rect x="6" y="34" width="88" height="34" rx="4" class="b"/>'] +
        [f'<rect x="{14 + i * 15}" y="45" width="10" height="12" rx="1.5" class="f"/>' for i in range(3)] +
        ['<line x1="62" y1="42" x2="62" y2="60" class="w"/>', '<path d="M58,84 L86,20 L94,24 L66,88 L56,92 Z" class="b"/>',
         '<line x1="80" y1="33" x2="88" y2="37" class="l"/>']),
    # ITEM OPTIONS: an ammunition box and a grenade
    3: ("items", ['<rect x="6" y="50" width="50" height="38" rx="3" class="b"/>'] +
        [f'<path d="M{14 + i * 13},74 V60 q4.5,-7 9,0 V74 Z" class="l"/>' for i in range(3)] +
        ['<line x1="10" y1="78" x2="52" y2="78" class="l"/>', '<circle cx="74" cy="58" r="20" class="b"/>',
         '<rect x="66" y="30" width="16" height="10" rx="2" class="b"/>', '<path d="M82,34 q10,-2 10,8" class="w"/>',
         '<line x1="58" y1="58" x2="90" y2="58" class="l"/>']),
    # VEHICLE OPTIONS: a wheel
    4: ("vehicles", ['<circle cx="50" cy="50" r="42" class="b"/>', '<circle cx="50" cy="50" r="27" class="l"/>',
                     '<circle cx="50" cy="50" r="8" class="f"/>'] +
        [f'<line x1="50" y1="50" x2="{50 + 27 * c:.1f}" y2="{50 + 27 * s:.1f}" class="l"/>'
         for c, s in ((1, 0), (-0.5, 0.866), (-0.5, -0.866))] +
        [f'<rect x="47" y="4" width="6" height="7" class="f" transform="rotate({a} 50 50)"/>' for a in range(0, 360, 30)]),
    # TEAMPLAY OPTIONS: a red and a blue shield
    6: ("teams", _shield(34, 50, 50, 70, "b red") + _shield(66, 50, 50, 70, "b") +
        ['<path d="M66,30 V70 M50,46 H82" class="l"/>', '<path d="M34,30 V70 M18,46 H50" class="l red"/>']),
    # ARENA OPTIONS: an octagonal arena, its middle marked
    7: ("arena", ['<polygon points="30,6 70,6 94,30 94,70 70,94 30,94 6,70 6,30" class="b"/>',
                  '<polygon points="36,20 64,20 80,36 80,64 64,80 36,80 20,64 20,36" class="l"/>',
                  '<circle cx="50" cy="50" r="11" class="l"/>',
                  '<path d="M50,30 V42 M50,58 V70 M30,50 H42 M58,50 H70" class="w"/>']),
    # NHE OPTIONS (a ninth frame): a stopwatch, for NHE's timers
    8: ("nhe", ['<rect x="44" y="2" width="12" height="9" rx="2" class="b"/>',
                '<line x1="78" y1="20" x2="84" y2="14" class="w"/>', '<circle cx="50" cy="54" r="40" class="b"/>',
                '<circle cx="50" cy="54" r="31" class="l"/>', '<path d="M50,54 V30" class="w"/>',
                '<path d="M50,54 L64,64" class="w"/>', '<circle cx="50" cy="54" r="4" class="f"/>'] +
        [f'<line x1="50" y1="25" x2="50" y2="29" class="l" transform="rotate({a} 50 54)"/>' for a in range(0, 360, 30)]),
}

STYLE = (f"<style>\n.b {{ fill:#000; stroke:{BLUE}; stroke-width:2.2; stroke-linejoin:round }}\n"
         f".l {{ fill:none; stroke:{BLUE}; stroke-width:1.6; stroke-linecap:round; stroke-linejoin:round }}\n"
         f".w {{ fill:none; stroke:{BLUE}; stroke-width:3.2; stroke-linecap:round; stroke-linejoin:round }}\n"
         f".f {{ fill:{BLUE}; stroke:none }}\n.red {{ stroke:{RED} }}\n</style>")


def svg(shapes, centre) -> str:
    """an entry's frame: its drawing, glowing, where the PC version's picture was"""
    scale = ICON_SIZE / 100
    left, top = centre[0] - ICON_SIZE / 2, centre[1] - ICON_SIZE / 2
    body = "\n".join(shapes)
    return "\n".join([
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{FRAME[0]}" height="{FRAME[1]}" '
        f'viewBox="0 0 {FRAME[0]} {FRAME[1]}">', STYLE,
        f'<defs><filter id="glow" x="-20%" y="-20%" width="140%" height="140%">'
        f'<feGaussianBlur stdDeviation="{2.4 / scale:.2f}"/></filter></defs>',
        f'<g transform="translate({left:g} {top:g}) scale({scale:g})">',
        f'<g filter="url(#glow)" opacity="0.9">{body}</g>', body, "</g>", "</svg>", ""])


def own_frames() -> dict:
    """each bitmap's own frames, by frame number, as tools/ce_menus.py's Art.own_frame takes them: (SVG text,
    width, height); a number past the PC version's frames is added after them"""
    return {
        MULTIPLAYER: {index: (svg(shapes, CENTRES[MULTIPLAYER]), *FRAME)
                      for index, (_, shapes) in enumerate(MULTIPLAYER_ICONS)},
        GAMETYPE: {index: (svg(shapes, CENTRES[GAMETYPE]), *FRAME) for index, (_, shapes) in GAMETYPE_ICONS.items()},
    }


def frame_paths(bitmap: str, index: int) -> tuple:
    """an own frame's SVG (under port/assets/menus) and PNG (as bitmaps.xml and menus.json name it)"""
    return f"port_svg/{bitmap}__{index}.svg", f"ce/port/{bitmap}__{index}.png"


def write(menus: Path, scale: int = 4) -> list:
    """the SVGs and their drawings, where Art.own_frame puts them; the PNGs' paths"""
    pngs = []
    for bitmap, frames in own_frames().items():
        for index, (text, _, _) in sorted(frames.items()):
            source, png = frame_paths(bitmap, index)
            (menus / source).parent.mkdir(parents=True, exist_ok=True)
            (menus / source).write_text(text)
            (menus / png).parent.mkdir(parents=True, exist_ok=True)
            subprocess.run(["rsvg-convert", "-z", str(scale), "-o", str(menus / png), str(menus / source)], check=True)
            pngs.append(png)
    return pngs


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--write", action="store_true", help="write the SVGs and PNGs into port/assets/menus")
    arguments = parser.parse_args()
    if arguments.write:
        for path in write(Path(__file__).resolve().parent.parent / "port" / "assets" / "menus"):
            print(path)
