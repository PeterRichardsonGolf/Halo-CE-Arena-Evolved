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
# the logical frame (the PC version's: 512 by 256, of which the widget shows the top left; measured in game, a frame
# texel is 1.84 screen pixels at 864 lines and the panel's inside is frame x 0 to about 298, its description's text
# from frame y 240 down on the Multiplayer screen and from 198 down on Edit Gametype)
FRAME = (512, 256)
MULTIPLAYER = "main_menu/multiplayer_type_select/mp_options"
GAMETYPE = "main_menu/settings_select/multiplayer_setup/playlist_edit/gametype_options"
# where each screen's drawings go (owner 10-15: placement, sizing and padding alike on every entry): the drawing's ink
# (INK) centred on the panel's middle above the text, the same space above it as between it and the text; scaled so
# every drawing has about the same visual mass (the square root of its ink's area: MASS), no taller than HEIGHT and
# no wider than WIDTH (frame texels)
ZONES = {MULTIPLAYER: {"centre": (146, 120), "mass": 140, "height": 150, "width": 215},
         GAMETYPE: {"centre": (144, 99), "mass": 116, "height": 128, "width": 200}}


# the strokes' weights ON SCREEN, in frame texels (owner 10-18 polish: one weight on every picture; each drawing is
# scaled to its zone by its own ink, so the stroke widths in its 100 by 100 box are these divided by its scale):
# EDGE the silhouettes' edge and the fine lines alike, HEAVY the cables, letters, tracks and hands, ARC a countdown's
# band, GLOW the glow's blur
EDGE, HEAVY, ARC, GLOW = 3.2, 4.8, 8.0, 2.4
# (the scale INK is measured at: a middling one, so the measured ink's stroked edges are near what is drawn)
INK_SCALE = 1.5


def _globe(cx, cy, r, clip):
    """the world: its meridians and parallels clipped to its rim (they end under the rim's edge: no caps poke out)"""
    shapes = [f'<clipPath id="{clip}"><circle cx="{cx}" cy="{cy}" r="{r}"/></clipPath>',
              f'<circle cx="{cx}" cy="{cy}" r="{r}" class="b"/>', f'<g clip-path="url(#{clip})">']
    for k in (0.38, 0.74):
        shapes.append(f'<ellipse cx="{cx}" cy="{cy}" rx="{r * k:.1f}" ry="{r + 2}" class="l"/>')
    for dy in (-0.5, 0, 0.5):
        y = cy + dy * r
        shapes.append(f'<line x1="{cx - r - 2}" y1="{y:.1f}" x2="{cx + r + 2}" y2="{y:.1f}" class="l"/>')
    return shapes + ["</g>"]


def _monitor(x, y, w, h):
    """a screen on its stand (the stand first: the screen's edge covers its top, no seam), its bezel wide enough to
    stay open at 720 lines"""
    return [f'<path d="M{x + w / 2 - 4},{y + h - 1} l-3,8 h14 l-3,-8 z" class="b"/>',
            f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="2.5" class="b"/>',
            f'<rect x="{x + 4.5}" y="{y + 4.5}" width="{w - 9}" height="{h - 9}" rx="1" class="l"/>']


def _rack(x, y, w, h, bays=4, inset=4.5, gap=3):
    """a host: a server rack, its bays evenly spaced, the same inset on every side (no lights: they are noise at the
    size it is drawn)"""
    shapes = [f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="2.5" class="b"/>']
    bay = (h - 2 * inset - (bays - 1) * gap) / bays
    for i in range(bays):
        shapes.append(f'<rect x="{x + inset}" y="{y + inset + i * (bay + gap):.2f}" width="{w - 2 * inset}" '
                      f'height="{bay:.2f}" rx="1" class="l"/>')
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


def _link_path(cx, cy, w, h, wall):
    """a chain link's path data: a rounded ring, its hole open (fill-rule evenodd)"""
    return _rounded(cx - w / 2, cy - h / 2, w, h, h / 2) + " " + _rounded(
        cx - w / 2 + wall, cy - h / 2 + wall, w - 2 * wall, h - 2 * wall, h / 2 - wall)


def _link(cx, cy, w, h, wall):
    """one chain link: a rounded ring, its hole open"""
    return [f'<path d="{_link_path(cx, cy, w, h, wall)}" fill-rule="evenodd" class="b"/>']


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


def _split(name, cells, frame=(4, 8, 92, 72), k=0.5, top=7.2):
    """one screen split in cells, a player in each: each player clipped to its cell, the dividers clipped to the
    screen, the screen's edge drawn again over them (so nothing ends in mid-air and no line's cap pokes out)"""
    x, y, w, h = frame
    shapes = [f'<clipPath id="{name}_{i}"><rect x="{cx}" y="{cy}" width="{cw}" height="{ch}"/></clipPath>'
              for i, (cx, cy, cw, ch) in enumerate(cells)]
    shapes.append(f'<clipPath id="{name}_screen"><rect x="{x}" y="{y}" width="{w}" height="{h}"/></clipPath>')
    shapes += [f'<path d="M{x + w / 2 - 8},{y + h - 1} l-4,11 h24 l-4,-11 z" class="b"/>',
               f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="3" class="b"/>']
    for i, (cx, cy, cw, ch) in enumerate(cells):
        # (the helmet's top `top` below the cell's: 21.6 k above the bust's middle)
        shapes += [f'<g clip-path="url(#{name}_{i})">'] + _bust(cx + cw / 2, cy + top + 21.6 * k, k) + ["</g>"]
    shapes.append(f'<g clip-path="url(#{name}_screen)">')
    shapes += [f'<line x1="{x}" y1="{cy}" x2="{x + w}" y2="{cy}" class="w"/>'
               for cy in sorted({cy for _, cy, _, _ in cells}) if cy > y]
    shapes += [f'<line x1="{cx}" y1="{y}" x2="{cx}" y2="{y + h}" class="w"/>'
               for cx in sorted({cx for cx, _, _, _ in cells}) if cx > x]
    return shapes + ["</g>", f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="3" class="e"/>']


def _tyre(cx, cy, r, knobs=16, depth=1.6):
    """a chunky off-road tyre's side: its tread a ring of square knobs (an even number, each a little over half its
    pitch), its rim and hub inside"""
    points = []
    for i in range(knobs):
        a0 = 2 * math.pi * i / knobs
        a1, a2 = a0 + 2 * math.pi / knobs * 0.55, a0 + 2 * math.pi / knobs
        for angle, radius in ((a0, r), (a1, r), (a1, r - depth), (a2, r - depth)):
            points.append(f"{cx + radius * math.cos(angle):.2f},{cy + radius * math.sin(angle):.2f}")
    return [f'<polygon points="{" ".join(points)}" class="b"/>', f'<circle cx="{cx}" cy="{cy}" r="7.5" class="l"/>',
            f'<circle cx="{cx}" cy="{cy}" r="2.5" class="f"/>']


def _flag(cross, lean, red):
    """a CTF flag: a pole and its rectangular banner in the team's colour, a knob on top; the pole passes through
    `cross` leaning `lean` degrees, its banner flying out on the side it leans to (a slight ripple, no flourish)"""
    team, w = ("red", -34) if red else ("blue", 34)
    banner = (f"M0,-54 Q{w * 0.25:.1f},-56.5 {w * 0.5:.1f},-54 Q{w * 0.75:.1f},-51.5 {w},-54 V-24 "
              f"Q{w * 0.75:.1f},-21.5 {w * 0.5:.1f},-24 Q{w * 0.25:.1f},-26.5 0,-24 Z")
    edge = " red" if red else ""
    return [f'<g transform="translate({cross[0]} {cross[1]}) rotate({lean})">',
            f'<path d="{banner}" class="b{edge} {team}-fill"/>',
            f'<line x1="0" y1="-55" x2="0" y2="20" class="w{edge}"/>',
            f'<circle cx="0" cy="-57" r="3.2" class="f {team}-fill"/>', "</g>"]


def _octagon(cx, cy, r, cls):
    points = " ".join(f"{cx + r * math.cos(math.radians(22.5 + 45 * i)):.1f},"
                      f"{cy + r * math.sin(math.radians(22.5 + 45 * i)):.1f}" for i in range(8))
    return [f'<polygon points="{points}" class="{cls}"/>']


def _cartridge(x, w, top, bottom):
    """one round standing up: its case and its pointed bullet"""
    return [f'<path d="M{x},{bottom} V{top + 16} Q{x},{top + 5} {x + w / 2},{top} Q{x + w},{top + 5} {x + w},{top + 16} '
            f'V{bottom} Z" class="b"/>', f'<line x1="{x + 2.5}" y1="{top + 20}" x2="{x + w - 2.5}" y2="{top + 20}" class="l"/>']


def _pencil(tip, toward, length, width):
    """a pencil, its point at `tip`, its body toward `toward`: a body, a sharpened cone (its line across) and a
    graphite point, all on one axis (symmetric: no lopsided nib)"""
    (tx, ty), (dx, dy) = tip, (toward[0] - tip[0], toward[1] - tip[1])
    n = math.hypot(dx, dy)
    ux, uy, vx, vy = dx / n, dy / n, -dy / n * width / 2, dx / n * width / 2

    def at(d, side=0.0):
        return f"{tx + ux * d + vx * side:.2f},{ty + uy * d + vy * side:.2f}"
    cone = 13
    return [f'<path d="M{at(0)} L{at(cone, 1)} L{at(length, 1)} L{at(length, -1)} L{at(cone, -1)} Z" class="b"/>',
            f'<path d="M{at(cone, 1)} L{at(cone, -1)}" class="l"/>',
            f'<path d="M{at(0)} L{at(4.5, 0.35)} L{at(4.5, -0.35)} Z" class="f"/>',
            f'<path d="M{at(length - 9, 1)} L{at(length - 9, -1)}" class="l"/>']


# the Warthog's wheel arches (radius 19 about its wheels at x 24 and 76, y 68) where they meet its body's floor at y 60
_ARCH = tuple(cx + side * math.sqrt(19 ** 2 - 8 ** 2) for cx in (24, 76) for side in (-1, 1))

# each entry's drawing, in a 100 by 100 box (class b: black with the blue edge, as the game's own icons; l: a fine
# blue line, the edge's weight; w: a heavy one; f: blue filled; arc: a countdown's band; e: an edge alone, drawn
# again over what was clipped to it; red: the red team's edge; red-fill, blue-fill: a team's colour filled). Drawn
# back to front so a part that meets another tucks under its edge (no round cap or seam shows at a join). One grammar
# across the Multiplayer screen: a JOIN row never shows a host; a CREATE row shows the host (a rack) broadcasting
# (arcs); the medium says where: the globe the Internet, a cabled switch the LAN, a chain link a direct invite.
MULTIPLAYER_ICONS = [
    # SERVER BROWSER: a list of games (a signal for each) searched with a lens; three rows, every one whole (the
    # lens sits below the short last row, over the window's corner)
    ("server_browser", ['<rect x="-4.5" y="0" width="9" height="20" rx="2.5" class="b" '
                        'transform="translate(80.5 80.5) rotate(-45)"/>',
                        '<rect x="4" y="10" width="78" height="68" rx="3" class="b"/>',
                        '<line x1="5" y1="21" x2="81" y2="21" class="l"/>'] +
     [f'<rect x="{10 + j * 5.5}" y="{row + 5 - 4 - j * 3}" width="3.5" height="{4 + j * 3}" class="f"/>'
      for row in (33, 48, 63) for j in range(3)] +
     [f'<line x1="31" y1="{row}" x2="{x2}" y2="{row}" class="w"/>' for row, x2 in ((33, 70), (48, 60), (63, 48))] +
     ['<circle cx="72" cy="72" r="14" class="b"/>', '<circle cx="72" cy="72" r="8.5" class="l"/>']),
    # LAN (join): two screens on one switch (the cables first: their ends tuck under the stands and the switch)
    ("lan_join", ['<path d="M23,46 V66 H77 V46" class="w"/>', '<line x1="50" y1="66" x2="50" y2="77" class="w"/>'] +
     _monitor(4, 14, 38, 28) + _monitor(58, 14, 38, 28) +
     ['<rect x="30" y="74" width="40" height="16" rx="2.5" class="b"/>'] +
     [f'<line x1="{38 + i * 8}" y1="79.5" x2="{38 + i * 8}" y2="84.5" class="l"/>' for i in range(4)]),
    # DIRECT LINK: an invite link, two chain links through each other: the left one whole, the right one over it at
    # the top crossing and under it at the bottom (a mask cuts the right link where the left one's band, a little
    # wider for a gap, passes below the links' middle line). The walls are 7 thick, so between the crossings each
    # link's end runs down the other's hole well clear of its band: the mask's edge cuts nothing there, and the
    # crossings are steep (9.5-thick walls left the bands nearly touching all the way: a notch and a knot)
    ("direct_link", ['<mask id="direct_link_under" maskUnits="userSpaceOnUse" x="-20" y="-20" width="140" height="140">'
                     '<rect x="-20" y="-20" width="140" height="140" fill="#fff"/>'
                     '<clipPath id="direct_link_low"><rect x="-20" y="50" width="140" height="70"/></clipPath>'
                     f'<path d="{_link_path(30, 50, 60, 30, 7)}" fill-rule="evenodd" fill="#000" stroke="#000" '
                     'stroke-width="5" clip-path="url(#direct_link_low)"/></mask>',
                     '<g transform="rotate(-30 50 50)">'] +
     _link(30, 50, 60, 30, 7) + ['<g mask="url(#direct_link_under)">'] + _link(70, 50, 60, 30, 7) +
     ["</g>", "</g>"]),
    # INTERNET (create): the host broadcasting, the world behind it
    ("internet_host", _globe(64, 44, 32, "internet_host_globe") + _rack(6, 44, 30, 48) +
     _waves(21, 38, (8, 16), toward=-105)),
    # LAN (create): the host's router broadcasting, at the top of a tree cabled to three screens (owner 10-15: a
    # router there, not a server); its four lights centred on it, the screens evenly spaced under their cables
    # (drawn before the router and the screens, so their ends tuck under them)
    ("lan_host", ['<line x1="34" y1="33" x2="29" y2="12" class="w"/>', '<line x1="66" y1="33" x2="71" y2="12" class="w"/>',
                  '<path d="M50,47 V57 M17,57 H83 M17,57 V68 M50,57 V68 M83,57 V68" class="w"/>',
                  '<rect x="22" y="32" width="56" height="18" rx="4" class="b"/>'] +
     [f'<line x1="{34.5 + i * 10}" y1="41" x2="{35.5 + i * 10}" y2="41" class="w"/>' for i in range(4)] +
     _waves(50, 30, (9, 17)) +
     _monitor(3, 65, 28, 21) + _monitor(36, 65, 28, 21) + _monitor(69, 65, 28, 21)),
    # SPLIT SCREEN (AE): one screen split in four, a player in each quarter, on a stand (CO-OP's in two)
    ("split_screen", _split("split_screen", [(4, 8, 46, 36), (50, 8, 46, 36), (4, 44, 46, 36), (50, 44, 46, 36)])),
    # CO-OP CAMPAIGN: one screen split in two, a player in each half
    ("coop", _split("coop", [(4, 8, 92, 36), (4, 44, 92, 36)], k=0.68, top=6)),
    # EDIT GAMETYPES: settings' sliders and a pennant, on four posts at one pitch (the pennant's pole the fourth)
    ("edit_gametypes", [f'<line x1="{x}" y1="30" x2="{x}" y2="92" class="w"/>' for x in (14, 38, 62)] +
     ['<line x1="86" y1="11" x2="86" y2="92" class="w"/>'] +
     [f'<rect x="{x - 9}" y="{y - 6}" width="18" height="12" rx="3" class="b"/>'
      for x, y in ((14, 72), (38, 44), (62, 60))] +
     ['<path d="M86,10 L101,18 L86,26 Z" class="b"/>']),
]

# the Edit Gametype screen's, by frame (the others are the PC version's or the Xbox's own)
GAMETYPE_ICONS = {
    # CHANGE NAME: a name field (letters and a block caret) and a pencil beside it (it does not cross the field)
    0: ("name", ['<rect x="4" y="34" width="72" height="40" rx="4" class="b"/>',
                 '<path d="M12,65 L18,45 L24,65 M14.1,58.5 H21.9" class="w"/>',
                 '<path d="M30,65 V45 H36 A5,5 0 0 1 36,55 H30 M36,55 H37 A5,5 0 0 1 37,65 H30" class="w"/>',
                 '<path d="M58.3,48.5 A7,10 0 1 0 58.3,61.5" class="w"/>',
                 '<rect x="64" y="44" width="6" height="22" class="f"/>'] +
        _pencil((81, 84), (98, 26), 60, 10)),
    # ITEM OPTIONS: three rounds and a frag grenade (its pin's ring and lever tuck under the fuse and the body)
    3: ("items", [s for x in (6, 22, 38) for s in _cartridge(x, 12, 30, 86)] +
        ['<circle cx="63" cy="41" r="4.5" class="l"/>', '<path d="M82,40 Q92,40 88,52" class="w"/>',
         '<rect x="67" y="36" width="16" height="11" rx="2" class="b"/>', '<circle cx="75" cy="64" r="20" class="b"/>']),
    # VEHICLE OPTIONS: a Halo CE Warthog from the side, nose to the right (owner 10-18: true to the source, not a
    # buggy): big knobby tyres in deep wheel arches, a long low hood dropping to an angular nose and its forward
    # bumper, the open cab under its roll bar, and the chain gun on its post on the rear deck, its barrel pointing
    # back (the post, the barrel and the roll bar's feet tuck under what they meet)
    4: ("vehicles", ['<rect x="15" y="32" width="7" height="16" class="b"/>',
                     '<g transform="translate(14 29) rotate(160)"><rect x="0" y="-3.5" width="20" height="7" rx="2" '
                     'class="b"/><line x1="4" y1="0" x2="17" y2="0" class="l"/></g>',
                     '<rect x="10" y="23" width="18" height="11" rx="2.5" class="b"/>',
                     '<path d="M36,50 L40,29 L53,30 L61,46" class="w"/>',
                     f'<path d="M3,60 L4,44 H31 L35,50 H55 L59,44 L80,42 L92,48 L95,56 L{_ARCH[3]:.1f},60 '
                     f'A19,19 0 0 0 {_ARCH[2]:.1f},60 H{_ARCH[1]:.1f} A19,19 0 0 0 {_ARCH[0]:.1f},60 Z" class="b"/>',
                     '<path d="M91,50 L99.5,53.5 L98.5,60 L92,60 Z" class="b"/>'] +
        _tyre(24, 68, 15.5) + _tyre(76, 68, 15.5)),
    # TEAMPLAY OPTIONS: a red and a blue CTF flag, crossed (the blue pole over the red), in the teams' colours
    6: ("teams", _flag((50, 62), -22, True) + _flag((50, 62), 22, False)),
    # ARENA OPTIONS: the arena's walls and a trophy in them: the competitive rules (the handles and the stem tuck
    # under the cup, the cup's foot round, clear of the inner wall)
    7: ("arena", _octagon(50, 50, 46, "b") + _octagon(50, 50, 38, "l") +
        ['<path d="M40,33 H31 Q27,33 27,37.5 Q27,46 40,48 M60,33 H69 Q73,33 73,37.5 Q73,46 60,48" class="w"/>',
         '<rect x="46.5" y="54" width="7" height="11" class="b"/>',
         '<path d="M36,30 H64 V38 C64,50 58,57 50,57 C42,57 36,50 36,38 Z" class="b"/>',
         '<rect x="38" y="64" width="24" height="7" rx="1.5" class="b"/>']),
    # NHE OPTIONS (a ninth frame): a stopwatch counting down, for NHE's timers (its crown and side button on stems
    # that run under the case)
    8: ("nhe", ['<rect x="47" y="11" width="6" height="9" class="b"/>',
                '<rect x="43" y="4" width="14" height="9" rx="2" class="b"/>',
                '<rect x="-3" y="-5" width="6" height="10" rx="1.5" class="b" transform="translate(79.7 26.3) rotate(45)"/>',
                '<circle cx="50" cy="56" r="39" class="b"/>',
                '<path d="M50,30 A26,26 0 1 1 24,56" class="arc"/>',
                '<path d="M50,56 V36" class="w"/>', '<circle cx="50" cy="56" r="4" class="f"/>']),
}

# each drawing's ink (ink(): left, top, right, bottom in its 100 by 100 box), measured once and kept here so the
# frames can be written without rsvg-convert or Pillow; tools/test_ae_menu_pictures.py measures them again
INK = {
    "server_browser": (2.9, 8.9, 97.9, 97.9),
    "lan_join": (2.9, 12.9, 97.1, 91.1),
    "direct_link": (3.6, 16.4, 96.4, 83.6),
    "internet_host": (4.9, 10.9, 97.1, 93.1),
    "lan_host": (1.9, 10.4, 98.1, 94.1),
    "split_screen": (2.9, 6.9, 97.1, 91.1),
    "coop": (2.9, 6.9, 97.1, 91.1),
    "edit_gametypes": (3.9, 8.9, 102.1, 93.6),
    "name": (2.9, 23.9, 103.8, 85.1),
    "items": (4.9, 28.9, 96.1, 87.1),
    "vehicles": (-6.5, 21.9, 100.6, 84.6),
    "teams": (-2.9, 5.9, 102.9, 82.1),
    "arena": (6.4, 6.4, 93.6, 93.6),
    "nhe": (9.9, 2.9, 90.1, 96.1),
}


def style(scale: float) -> str:
    """the strokes' style for a drawing drawn at `scale` frame texels a unit: the same weights on screen for all"""
    edge, heavy, arc = EDGE / scale, HEAVY / scale, ARC / scale
    return (f"<style>\n.b {{ fill:#000; stroke:{BLUE}; stroke-width:{edge:.3f}; stroke-linejoin:round }}\n"
            f".l {{ fill:none; stroke:{BLUE}; stroke-width:{edge:.3f}; stroke-linecap:round; stroke-linejoin:round }}\n"
            f".w {{ fill:none; stroke:{BLUE}; stroke-width:{heavy:.3f}; stroke-linecap:round; stroke-linejoin:round }}\n"
            f".e {{ fill:none; stroke:{BLUE}; stroke-width:{edge:.3f}; stroke-linejoin:round }}\n"
            f".arc {{ fill:none; stroke:{BLUE}; stroke-width:{arc:.3f} }}\n"
            f".f {{ fill:{BLUE}; stroke:none }}\n.red {{ stroke:{RED} }}\n"
            f".red-fill {{ fill:{RED} }}\n.blue-fill {{ fill:{BLUE} }}\n</style>")


def _split_defs(shapes):
    """a drawing's clip paths and masks (they go in the defs, once) and its body"""
    defs = "".join(shape for shape in shapes if shape.startswith(("<clipPath", "<mask")))
    body = "\n".join(shape for shape in shapes if not shape.startswith(("<clipPath", "<mask")))
    return defs, body


def ink(shapes) -> tuple:
    """a drawing's ink in its 100 by 100 box (left, top, right, bottom; its edges' strokes included, at INK_SCALE),
    measured from the drawing itself (rsvg-convert and Pillow: tools/test_ae_menu_pictures.py checks INK against it)"""
    import io
    from PIL import Image
    clips, body = _split_defs(shapes)
    text = (f'<svg xmlns="http://www.w3.org/2000/svg" width="140" height="140" viewBox="-20 -20 140 140">'
            f"{style(INK_SCALE)}<defs>{clips}</defs>{body}</svg>")
    png = subprocess.run(["rsvg-convert", "-z", "8"], input=text.encode(), capture_output=True, check=True).stdout
    alpha = Image.open(io.BytesIO(png)).getchannel("A").point(lambda value: 255 if value > 40 else 0)
    return tuple(round(value / 8 - 20, 1) for value in alpha.getbbox())


def svg(shapes, box, zone) -> str:
    """an entry's frame: its drawing, glowing, its ink (box) placed and sized in its screen's zone, its strokes at
    the weights every frame shares"""
    left, top, right, bottom = box
    width, height = right - left, bottom - top
    scale = min(zone["mass"] / (width * height) ** 0.5, zone["height"] / height, zone["width"] / width)
    x = zone["centre"][0] - scale * (left + right) / 2
    y = zone["centre"][1] - scale * (top + bottom) / 2
    # (a drawing's clip paths go in the defs, once: the body is drawn twice, glowing and sharp)
    clips, body = _split_defs(shapes)
    return "\n".join([
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{FRAME[0]}" height="{FRAME[1]}" '
        f'viewBox="0 0 {FRAME[0]} {FRAME[1]}">', style(scale),
        f'<defs><filter id="glow" x="-20%" y="-20%" width="140%" height="140%">'
        f'<feGaussianBlur stdDeviation="{GLOW / scale:.2f}"/></filter>{clips}</defs>',
        f'<g transform="translate({x:.2f} {y:.2f}) scale({scale:.4f})">',
        f'<g filter="url(#glow)" opacity="0.9">{body}</g>', body, "</g>", "</svg>", ""])


def own_frames() -> dict:
    """each bitmap's own frames, by frame number, as tools/ce_menus.py's Art.own_frame takes them: (SVG text,
    width, height); a number past the PC version's frames is added after them"""
    return {
        MULTIPLAYER: {index: (svg(shapes, INK[name], ZONES[MULTIPLAYER]), *FRAME)
                      for index, (name, shapes) in enumerate(MULTIPLAYER_ICONS)},
        GAMETYPE: {index: (svg(shapes, INK[name], ZONES[GAMETYPE]), *FRAME)
                   for index, (name, shapes) in GAMETYPE_ICONS.items()},
    }


def frame_paths(bitmap: str, index: int) -> tuple:
    """an own frame's SVG (under port/assets/menus) and PNG (as bitmaps.xml and menus.json name it)"""
    return f"port_svg/{bitmap}__{index}.svg", f"ce/port/{bitmap}__{index}.png"


# (AE) the split screen lobby's bot rows' band: the PC menus' own option row band (bitmaps/option_bkds, 512 wide:
# its rounded ends kept, its middle shortened) at the lobby's row width, its two frames (the focused one outlined)
LOBBY_BAND = "bitmaps/ae_option_bkds_lobby"
LOBBY_BAND_WIDTH = 366


def lobby_band(menus: Path) -> list:
    """the band's frames, cut from the game's own (ce/shell/bitmaps/option_bkds__<n>.png, at 4 pixels a unit)"""
    from PIL import Image
    pngs = []
    for frame in (0, 1):
        source = Image.open(menus / "ce" / "shell" / "bitmaps" / f"option_bkds__{frame}.png").convert("RGBA")
        width, height, end = LOBBY_BAND_WIDTH * 4, source.height, 96
        band = Image.new("RGBA", (width, height))
        band.paste(source.crop((0, 0, end, height)), (0, 0))
        band.paste(source.crop((end, 0, end + width - 2 * end, height)), (end, 0))
        band.paste(source.crop((source.width - end, 0, source.width, height)), (width - end, 0))
        png = f"ce/port/{LOBBY_BAND}__{frame}.png"
        (menus / png).parent.mkdir(parents=True, exist_ok=True)
        band.save(menus / png)
        pngs.append(png)
    return pngs


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
    return pngs + lobby_band(menus)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--write", action="store_true", help="write the SVGs and PNGs into port/assets/menus")
    arguments = parser.parse_args()
    if arguments.write:
        for path in write(Path(__file__).resolve().parent.parent / "port" / "assets" / "menus"):
            print(path)
