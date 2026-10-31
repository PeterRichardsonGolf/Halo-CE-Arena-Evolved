/*
AE_WAYPOINT_ICONS.H

The item waypoints' labels with pictures (display.waypoint_style: TEXT, ICONS,
BOTH), the part with no game types: the style's names, which picture an item
has, whether a map's sprite can be drawn, how large a picture is drawn and
what a label's line is made of. source/interface/hud_item_timers.c draws by
these. Unit tested in port/linux/tests/ae_waypoint_icons_test.c.

A power weapon's picture is the pickup prompt's, a sprite of the map being played (nothing is shipped), drawn
alone; a map or mod without one shows Arena Evolved's burst in an outlined badge instead. The overshield's and the
active camouflage's are Arena Evolved's own glyphs, dark on a filled badge (ae_waypoint_glyphs.c, drawn by
tools/ae_waypoint_glyphs.py).
*/

#ifndef __AE_WAYPOINT_ICONS_H
#define __AE_WAYPOINT_ICONS_H

enum
{
	AE_WAYPOINT_MAXIMUM_MEMBERS = 16	/* (members of a line looked at) */
};

enum
{
	AE_WAYPOINT_TEXT,	/* "RED SNIPER 0:07" */
	AE_WAYPOINT_ICONS,	/* the distance, the picture, the timer */
	AE_WAYPOINT_BOTH,	/* the distance, "RED SNIPER", the picture, the timer: stacked */
	AE_WAYPOINT_STYLE_COUNT
};

/* the power items' classes, as item_timers.h numbers them (hud_item_timers.c checks they agree) */
enum
{
	AE_WAYPOINT_ROCKETS,
	AE_WAYPOINT_SNIPER,
	AE_WAYPOINT_SHOTGUN,
	AE_WAYPOINT_OVERSHIELD,
	AE_WAYPOINT_CAMO
};

enum
{
	AE_WAYPOINT_ICON_NONE,	/* no picture: its text */
	AE_WAYPOINT_ICON_STOCK,	/* the map's own sprite (a weapon's) */
	AE_WAYPOINT_ICON_GLYPH	/* one of ours */
};

enum
{
	AE_WAYPOINT_GLYPH_OVERSHIELD,
	AE_WAYPOINT_GLYPH_CAMO,
	AE_WAYPOINT_GLYPH_POWER_WEAPON,	/* the burst of every power weapon */
	AE_WAYPOINT_GLYPH_DEAD_ALLY,	/* DEAD ALLY MARKERS' cross */
	AE_WAYPOINT_GLYPH_DEAD_ALLY_OUTLINE,	/* the same, wider: its dark outline in HUD STYLE AE */
	AE_WAYPOINT_GLYPH_COUNT,
	AE_WAYPOINT_GLYPH_SIZE = 64
};

/* the glyphs' coverage, row by row from the top (ae_waypoint_glyphs.c) */
extern const unsigned char ae_waypoint_glyphs[AE_WAYPOINT_GLYPH_COUNT][AE_WAYPOINT_GLYPH_SIZE * AE_WAYPOINT_GLYPH_SIZE];

/* a part of a label's line */
enum
{
	AE_WAYPOINT_PART_NAME,	/* a member's name */
	AE_WAYPOINT_PART_TIME,	/* the line's time */
	AE_WAYPOINT_PART_SPACE,	/* a space */
	AE_WAYPOINT_PART_SEPARATOR	/* between two names (" · ") */
};

struct ae_waypoint_part
{
	short kind;
	short member;	/* a name's: which of the line's members */
};

/* display.waypoint_style's value: "text", "icons" or "both" (any case); anything else is the default, BOTH */
int ae_waypoint_style_from_text(char const *text);
char const *ae_waypoint_style_name(int style);

/* the picture of an item of a class: for a weapon the map's sprite while it can be drawn, else (a map or mod
without one) our burst (*glyph); for the overshield and the camouflage ours (*glyph, may be NULL, says which), while
they can be drawn; else none (also any other class: a mixed entry not seen yet) */
int ae_waypoint_icon_source(int item_class, int stock_sprite_drawable, int glyphs_drawable, int *glyph);

/* the item a waypoint's picture is of: an entry's item of each class (a NONE where it has none, count of them), and
the class its waypoint names (a mixed entry's: the class it spawned, else NONE); none_value where that class has
none, -1 (NONE) when the class is out of range: never another class's item (a collection of rockets and a sniper rifle that spawned the
rifle shows the rifle) */
long ae_waypoint_class_definition(const long *class_definitions, int count, int item_class);

/* a map's sprite, checked before anything is drawn from it (a Custom Edition map's or a mod's sheet may be
anything): its bitmap's size, and its bounds as fractions of the bitmap (x0 < x1, y0 < y1, all in 0..1). 1 with
its size in texels (at least 2 each way) when it can be drawn, else 0 */
int ae_waypoint_sprite_size(int bitmap_width, int bitmap_height, float x0, float x1, float y0, float y1,
	int *width, int *height);

/* Centring and tight rows (a stack's rows centre on one axis; each row's ink, not its text box, is what is placed):
a text row drawn from x shows its first ink ink_offset to the right of x (the font's leading), so it is drawn from
the axis less half its ink width less ink_offset. All in the HUD's units (480 to the view's height: 2.25 pixels at
1080p), kept exact: a rounding to whole units here was a picture row 1 unit (2 to 3 pixels at 1080p) left of the text */
float ae_waypoint_row_left(float axis, float ink_width, float ink_offset);
/* a block's axis: centred on a point (anchor_x, its arrow's ink) the block's left is the whole unit at or left of
anchor_x less half its width, then pinned in its room by whole units; its axis is that left, half its width and the
fraction of a unit dropped */
float ae_waypoint_block_axis(int left, int width, float anchor_x);
/* text drawn scaled about an origin (rasterizer_text_set_scale: p' = origin + (p - origin) scale) from a whole unit:
how far the origin is moved to move the scaled text by shift (a fraction of a unit); 0 for a scale too near 1 to
move it so (the text then at the nearest whole unit) */
float ae_waypoint_scaled_shift(float shift, float scale);
/* A sprite's ink (the map's waypoint arrow): the columns of a width by height sheet of weights (0..255, a pixel's
alpha and brightness) whose weight reaches threshold, the leftmost and the rightmost; 0 if none does */
int ae_waypoint_ink_columns(const unsigned char *weights, int width, int height, int threshold, int *left, int *right);
/* the middle of a sprite's ink (columns left..right of width) from the sprite's middle, a fraction of its width
(negative: left of it) */
float ae_waypoint_ink_middle(int left, int right, int width);
/* a text row's ink at a scale: its capitals' top below its line's top (ascent less cap_height) and its height (cap_height,
and a texel under the baseline), both in the view's pixels */
void ae_waypoint_row_ink(int ascent, int cap_height, float scale, int *top_offset, int *height);

/* A respawning item's faded colours (part AE_WAYPOINT_FADE_MARK: the distance and the picture; AE_WAYPOINT_FADE_NAME:
the name): the mark's colour blended 60% toward a neutral grey and 40% as opaque, the name's as it was at 85%;
the timer is not faded. in and out are r, g, b, a in 0..1 (may be the same) */
enum
{
	AE_WAYPOINT_FADE_MARK,
	AE_WAYPOINT_FADE_NAME
};
void ae_waypoint_fade(int part, const float *in, float *out);

int ae_waypoint_stock_height(int cap_height);
/* a weapon's sprite drawn alone: 1.4 capitals high, its shape kept and its width following, or
smaller still to max_width (the name's width; none when 0). *scale from its texels to that size.
0 for a sprite of no size */
int ae_waypoint_stock_layout(int cap_height, int sprite_width, int sprite_height, int max_width, int *width,
	int *height, float *scale);

/* How a line of labels is laid out, centred, top to bottom: the distance, the name(s), the
pictures side by side, the respawn timer. BOTH: always stacked (a distance row, the names, the pictures of the members
that have one, the timer row while there is a time); ICONS: the same without the names of the members that have a
picture; TEXT, and ICONS where no member has a picture: one line of text ("RED SNIPER 0:07", several "0:07 A · B"),
not stacked. */
struct ae_waypoint_plan
{
	int stacked;
	int distance_row;	/* the top row: the distance (stacked lines always have it) */
	int time_row;	/* the bottom row: the respawn timer (when the line has a time) */
	int name_count;	/* the names row's members (none: no row) */
	int names[AE_WAYPOINT_MAXIMUM_MEMBERS];
	int picture_count;	/* the pictures row's members (none: no row) */
	int pictures[AE_WAYPOINT_MAXIMUM_MEMBERS];
};
/* returns plan->stacked (members beyond AE_WAYPOINT_MAXIMUM_MEMBERS are left out) */
int ae_waypoint_line_plan(int style, int members, const int *member_has_icon, int has_time,
	struct ae_waypoint_plan *plan);
/* the row of pictures of these widths, `gap` apart: each one's offset from the row's left, and the row's width.
Returns count */
int ae_waypoint_picture_row(int count, const int *widths, int gap, int *offsets, int *total);
/* the names row's parts: the members' names, a separator between */
int ae_waypoint_name_parts(int count, const int *names, struct ae_waypoint_part *parts, int max_parts);
/* the unstacked line's parts: one "NAME 0:07"; several "0:07 A · B"; at most max_parts (members that do not fit
are left out, the time never) */
int ae_waypoint_line_parts(int members, int has_time, struct ae_waypoint_part *parts, int max_parts);

/* A picture's badge: a regular hexagon, flat at its left and right, pointed at the top and bottom;
a weapon's dark and translucent with a thin light outline and a light picture, a powerup's filled gold with a dark
picture and a lighter rim. Its height is 2.2 times the name's capital height (cap_height, the view's pixels), its
width 0.866 of that; the picture is fitted into the hexagon (a padding of a tenth of its height all round), its own
shape kept, as large as fits: a wide sprite becomes small, a square one about 0.5 of the badge's height.
1 when it can be drawn (a sprite and cap_height of some size) */
struct ae_waypoint_badge
{
	int badge_width;
	int badge_height;
	int icon_width;
	int icon_height;
	float scale;	/* from the sprite's texels to the icon's size */
};
int ae_waypoint_badge_height(int cap_height);
int ae_waypoint_badge_layout(int cap_height, int sprite_width, int sprite_height, struct ae_waypoint_badge *badge);
/* the badge's coverage, width by height texels, row by row from the top: outline 0 the filled shape, 1 its
outline alone (a line of badge_height / 12 texels, at least one), both anti-aliased about their edge */
void ae_waypoint_badge_coverage(int width, int height, int outline, unsigned char *coverage);

/* DEAD ALLY MARKERS (display.dead_ally_markers; the cross where a teammate fell, owner 2026-10-24): whether a
player is marked in a view: a team game, the viewer's teammate (never the viewer), dead (no unit) and their body
still there; until they respawn or the body goes */
int ae_waypoint_dead_ally_marked(int team_game, int is_viewer, int teammate, int alive, int body);
/* its rows by WAYPOINTS, top to bottom: the distance (ICONS, BOTH), the name (TEXT, BOTH), then the cross (always) */
void ae_waypoint_dead_ally_rows(int style, int *distance, int *name);

#endif
