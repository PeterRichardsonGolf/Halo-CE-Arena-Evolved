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
import math
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


def _rack(x, y, w, h, bays=4):
    """a host: a server rack, its bays (no lights: they are noise at the size it is drawn)"""
    shapes = [f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="2" class="b"/>']
    for i in range(bays):
        by = y + 4 + i * (h - 8) / bays
        shapes.append(f'<rect x="{x + 4}" y="{by:.1f}" width="{w - 8}" height="{(h - 8) / bays - 3:.1f}" rx="1" '
                      'class="l"/>')
    return shapes


def _waves(cx, cy, radii, toward=-90, spread=42):
    """a host's broadcast: arcs about (cx, cy) facing `toward` (degrees, -90 up): every CREATE row has them"""
    shapes = []
    for r in radii:
        a, b = math.radians(toward - spread), math.radians(toward + spread)
        shapes.append(f'<path d="M{cx + r * math.cos(a):.1f},{cy + r * math.sin(a):.1f} '
                      f'A{r},{r} 0 0 1 {cx + r * math.cos(b):.1f},{cy + r * math.sin(b):.1f}" class="w"/>')
    return shapes


def _rounded(x, y, w, h, r):
    """a rounded rectangle as path data (to cut holes with fill-rule evenodd)"""
    return (f"M{x + r},{y} H{x + w - r} A{r},{r} 0 0 1 {x + w},{y + r} V{y + h - r} A{r},{r} 0 0 1 {x + w - r},{y + h} "
            f"H{x + r} A{r},{r} 0 0 1 {x},{y + h - r} V{y + r} A{r},{r} 0 0 1 {x + r},{y} Z")


def _link(cx, cy, w, h, wall, clip=None):
    """one chain link: a rounded ring, its hole open"""
    d = _rounded(cx - w / 2, cy - h / 2, w, h, h / 2) + " " + _rounded(
        cx - w / 2 + wall, cy - h / 2 + wall, w - 2 * wall, h - 2 * wall, h / 2 - wall)
    extra = f' clip-path="url(#{clip})"' if clip else ""
    return [f'<path d="{d}" fill-rule="evenodd" class="b"{extra}/>']


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
    return [f'<path d="{shoulders}" class="b"/>'] + _helmet(cx, cy - 4 * k, 0.8 * k)


def _shield(cx, cy, w, h, cls):
    outline = (f"M{cx},{cy - h / 2} L{cx + w / 2},{cy - h / 2 + h * 0.18} L{cx + w / 2},{cy + h * 0.05} "
               f"Q{cx + w / 2},{cy + h * 0.35} {cx},{cy + h / 2} Q{cx - w / 2},{cy + h * 0.35} {cx - w / 2},{cy + h * 0.05} "
               f"L{cx - w / 2},{cy - h / 2 + h * 0.18} Z")
    return [f'<path d="{outline}" class="{cls}"/>']


def _octagon(cx, cy, r, cls):
    points = " ".join(f"{cx + r * math.cos(math.radians(22.5 + 45 * i)):.1f},"
                      f"{cy + r * math.sin(math.radians(22.5 + 45 * i)):.1f}" for i in range(8))
    return [f'<polygon points="{points}" class="{cls}"/>']


def _cartridge(x, w, top, bottom):
    """one round standing up: its case and its pointed bullet"""
    return [f'<path d="M{x},{bottom} V{top + 16} Q{x},{top + 5} {x + w / 2},{top} Q{x + w},{top + 5} {x + w},{top + 16} '
            f'V{bottom} Z" class="b"/>', f'<line x1="{x + 2}" y1="{top + 20}" x2="{x + w - 2}" y2="{top + 20}" class="l"/>']


# each entry's drawing, in a 100 by 100 box (class b: black with the blue edge, as the game's own icons; l: a thin
# blue line; w: a heavy one; f: blue filled; arc: a countdown's band; red: the red team's edge). One grammar across
# the Multiplayer screen: a JOIN row never shows a host; a CREATE row shows the host (a rack) broadcasting (arcs);
# the medium says where: the globe the Internet, a cabled switch the LAN, a chain link a direct invite.
MULTIPLAYER_ICONS = [
    # SERVER BROWSER: a list of games (a signal for each) searched with a lens
    ("server_browser", ['<rect x="4" y="10" width="76" height="70" rx="3" class="b"/>',
                        '<line x1="4" y1="21" x2="80" y2="21" class="l"/>'] +
     [f'<line x1="12" y1="{31 + i * 12}" x2="{x2}" y2="{31 + i * 12}" class="w"/>'
      for i, x2 in enumerate((44, 36, 48, 30))] +
     [f'<rect x="{54 + j * 6}" y="{34 + i * 12 - 3 - j * 3}" width="3.5" height="{3 + j * 3}" class="f"/>'
      for i in range(3) for j in range(3)] +
     ['<circle cx="70" cy="68" r="17" class="b"/>', '<circle cx="70" cy="68" r="10.5" class="l"/>',
      '<rect x="-4.5" y="0" width="9" height="20" rx="2" class="b" transform="translate(81 79) rotate(-45)"/>']),
    # LAN (join): two screens on one switch
    ("lan_join", _monitor(4, 14, 38, 28) + _monitor(58, 14, 38, 28) + [
        '<path d="M23,49 V66 H77 V49" class="w"/>', '<line x1="50" y1="66" x2="50" y2="74" class="w"/>',
        '<rect x="30" y="74" width="40" height="16" rx="2" class="b"/>'] +
     [f'<line x1="{38 + i * 8}" y1="79" x2="{38 + i * 8}" y2="85" class="l"/>' for i in range(4)]),
    # DIRECT LINK: an invite link, two chain links through each other
    ("direct_link", ['<clipPath id="direct_link_over"><rect x="44" y="50" width="20" height="22"/></clipPath>',
                     '<g transform="rotate(-30 50 50)">'] +
     _link(30, 50, 60, 30, 9.5) + _link(70, 50, 60, 30, 9.5) + _link(30, 50, 60, 30, 9.5, "direct_link_over") + ["</g>"]),
    # INTERNET (create): the host broadcasting, the world behind it
    ("internet_host", _globe(64, 44, 32) + _rack(6, 44, 30, 48) + _waves(21, 38, (8, 16), toward=-105)),
    # LAN (create): the host broadcasting, cabled to three screens
    ("lan_host", _rack(37, 22, 26, 32, bays=3) + _waves(50, 18, (8, 16)) +
     ['<path d="M50,54 V62 M16,62 H84 M16,62 V68 M50,62 V68 M84,62 V68" class="w"/>'] +
     _monitor(2, 68, 28, 18) + _monitor(36, 68, 28, 18) + _monitor(70, 68, 28, 18)),
    # CO-OP CAMPAIGN: one screen split in two, a player in each half
    ("coop", ['<clipPath id="coop_top"><rect x="12" y="14" width="76" height="30"/></clipPath>',
              '<clipPath id="coop_bottom"><rect x="12" y="46" width="76" height="28"/></clipPath>',
              '<rect x="4" y="8" width="92" height="72" rx="3" class="b"/>',
              '<g clip-path="url(#coop_top)">'] + _bust(50, 33, 0.72) +
     ['</g>', '<g clip-path="url(#coop_bottom)">'] + _bust(50, 64, 0.72) +
     ['</g>', '<line x1="4" y1="45" x2="96" y2="45" class="w"/>',
      '<path d="M42,80 l-4,10 h24 l-4,-10 z" class="b"/>']),
    # EDIT GAMETYPES: settings' sliders and a pennant
    ("edit_gametypes", [f'<line x1="{x}" y1="22" x2="{x}" y2="90" class="w"/>' for x in (22, 50, 78)] +
     [f'<rect x="{x - 9}" y="{y - 6}" width="18" height="12" rx="3" class="b"/>'
      for x, y in ((22, 70), (50, 38), (78, 58))] +
     ['<line x1="88" y1="4" x2="88" y2="34" class="w"/>', '<path d="M88,5 L100,11 L88,19 Z" class="b"/>']),
]

# the Edit Gametype screen's, by frame (the others are the PC version's or the Xbox's own)
GAMETYPE_ICONS = {
    # CHANGE NAME: a name field (letters and a caret) and a pen
    0: ("name", ['<rect x="4" y="32" width="88" height="38" rx="4" class="b"/>',
                 '<path d="M12,61 L18.5,41 L25,61 M14.6,54 H22.4" class="w"/>',
                 '<path d="M31,61 V41 H37 Q42,41 42,46 Q42,51 37,51 H31 M37,51 Q43,51 43,56 Q43,61 37,61 H31" class="w"/>',
                 '<path d="M58,44 Q55,41 52,41 Q47,41 47,51 Q47,61 52,61 Q55,61 58,58" class="w"/>',
                 '<line x1="64" y1="39" x2="64" y2="63" class="w"/>',
                 '<path d="M62,86 L88,24 L96,28 L70,90 L60,94 Z" class="b"/>',
                 '<line x1="82" y1="37" x2="90" y2="41" class="l"/>']),
    # ITEM OPTIONS: three rounds and a grenade
    3: ("items", [s for x in (6, 22, 38) for s in _cartridge(x, 12, 30, 86)] +
        ['<circle cx="74" cy="64" r="21" class="b"/>', '<rect x="66" y="36" width="16" height="10" rx="2" class="b"/>',
         '<path d="M82,40 q10,-2 10,9" class="w"/>', '<circle cx="62" cy="38" r="5" class="l"/>']),
    # VEHICLE OPTIONS: a plain off-road buggy from the side
    4: ("vehicles", ['<path d="M46,36 L52,18 H70 L72,36" class="w"/>',
                     '<path d="M4,60 L9,48 L32,45 L42,34 L74,34 L82,46 L95,49 L97,61 L90,66 L10,66 Z" class="b"/>',
                     '<path d="M44,45 L49,39 H70 L74,45 Z" class="l"/>',
                     '<circle cx="24" cy="68" r="15" class="b"/>', '<circle cx="78" cy="68" r="15" class="b"/>',
                     '<circle cx="24" cy="68" r="5.5" class="l"/>', '<circle cx="78" cy="68" r="5.5" class="l"/>']),
    # TEAMPLAY OPTIONS: a red and a blue shield, plain
    6: ("teams", _shield(34, 50, 50, 72, "b red") + _shield(34, 52, 34, 50, "l red") +
        _shield(66, 50, 50, 72, "b") + _shield(66, 52, 34, 50, "l")),
    # ARENA OPTIONS: the arena's walls and a trophy in them: the competitive rules
    7: ("arena", _octagon(50, 50, 46, "b") + _octagon(50, 50, 38, "l") +
        ['<path d="M32,26 H68 V36 Q68,58 50,60 Q32,58 32,36 Z" class="b"/>',
         '<path d="M32,30 H25 Q22,30 22,34 Q22,46 33,48 M68,30 H75 Q78,30 78,34 Q78,46 67,48" class="w"/>',
         '<path d="M46,60 H54 V68 H46 Z" class="b"/>', '<rect x="37" y="68" width="26" height="8" rx="1.5" class="b"/>']),
    # NHE OPTIONS (a ninth frame): a stopwatch counting down, for NHE's timers
    8: ("nhe", ['<rect x="43" y="4" width="14" height="9" rx="2" class="b"/>', '<line x1="50" y1="13" x2="50" y2="17" class="w"/>',
                '<line x1="79" y1="24" x2="85" y2="18" class="w"/>', '<circle cx="50" cy="56" r="39" class="b"/>',
                '<path d="M50,30 A26,26 0 1 1 24,56" class="arc"/>',
                '<path d="M50,56 V36" class="w"/>', '<circle cx="50" cy="56" r="4" class="f"/>']),
}

STYLE = (f"<style>\n.b {{ fill:#000; stroke:{BLUE}; stroke-width:2.4; stroke-linejoin:round }}\n"
         f".l {{ fill:none; stroke:{BLUE}; stroke-width:2; stroke-linecap:round; stroke-linejoin:round }}\n"
         f".w {{ fill:none; stroke:{BLUE}; stroke-width:3.4; stroke-linecap:round; stroke-linejoin:round }}\n"
         f".arc {{ fill:none; stroke:{BLUE}; stroke-width:6 }}\n"
         f".f {{ fill:{BLUE}; stroke:none }}\n.red {{ stroke:{RED} }}\n</style>")


def svg(shapes, centre) -> str:
    """an entry's frame: its drawing, glowing, where the PC version's picture was"""
    scale = ICON_SIZE / 100
    left, top = centre[0] - ICON_SIZE / 2, centre[1] - ICON_SIZE / 2
    # (a drawing's clip paths go in the defs, once: the body is drawn twice, glowing and sharp)
    clips = "".join(shape for shape in shapes if shape.startswith("<clipPath"))
    body = "\n".join(shape for shape in shapes if not shape.startswith("<clipPath"))
    return "\n".join([
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{FRAME[0]}" height="{FRAME[1]}" '
        f'viewBox="0 0 {FRAME[0]} {FRAME[1]}">', STYLE,
        f'<defs><filter id="glow" x="-20%" y="-20%" width="140%" height="140%">'
        f'<feGaussianBlur stdDeviation="{2.4 / scale:.2f}"/></filter>{clips}</defs>',
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
