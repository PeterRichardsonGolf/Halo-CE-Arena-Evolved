/*
AE_WAYPOINT_ICONS.H

The item waypoints' labels with pictures (display.waypoint_style: TEXT, ICONS,
BOTH), the part with no game types: the style's names, which picture an item
has, whether a map's sprite can be drawn, how large a picture is drawn and
what a label's line is made of. source/interface/hud_item_timers.c draws by
these. Unit tested in port/linux/tests/ae_waypoint_icons_test.c.

A weapon's picture is the pickup prompt's, a sprite of the map being played
(nothing is shipped); a map or mod without one shows that item's text. The
overshield's and the active camouflage's are Arena Evolved's own glyphs
(ae_waypoint_glyphs.c, drawn by tools/ae_waypoint_glyphs.py).
*/

#ifndef __AE_WAYPOINT_ICONS_H
#define __AE_WAYPOINT_ICONS_H

enum
{
	AE_WAYPOINT_TEXT,	/* "RED SNIPER 0:07" */
	AE_WAYPOINT_ICONS,	/* the picture, then the time */
	AE_WAYPOINT_BOTH,	/* "RED SNIPER 0:07", then the picture */
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
	AE_WAYPOINT_ICON_STOCK,	/* the map's own sprite */
	AE_WAYPOINT_ICON_GLYPH	/* one of ours */
};

enum
{
	AE_WAYPOINT_GLYPH_OVERSHIELD,
	AE_WAYPOINT_GLYPH_CAMO,
	AE_WAYPOINT_GLYPH_COUNT,
	AE_WAYPOINT_GLYPH_SIZE = 32
};

/* the glyphs' coverage, row by row from the top (ae_waypoint_glyphs.c) */
extern const unsigned char ae_waypoint_glyphs[AE_WAYPOINT_GLYPH_COUNT][AE_WAYPOINT_GLYPH_SIZE * AE_WAYPOINT_GLYPH_SIZE];

/* a part of a label's line */
enum
{
	AE_WAYPOINT_PART_NAME,	/* a member's name */
	AE_WAYPOINT_PART_ICON,	/* a member's picture */
	AE_WAYPOINT_PART_TIME,	/* the line's time */
	AE_WAYPOINT_PART_SPACE,	/* a space */
	AE_WAYPOINT_PART_SEPARATOR	/* between two names (" · ") */
};

struct ae_waypoint_part
{
	short kind;
	short member;	/* a name's or a picture's: which of the line's members */
};

/* display.waypoint_style's value: "text", "icons" or "both" (any case); anything else is the default, BOTH */
int ae_waypoint_style_from_text(char const *text);
char const *ae_waypoint_style_name(int style);

/* the picture of an item of a class: ours for the overshield and the camouflage (while they can be drawn), the
map's sprite for a weapon that has a drawable one, else none (also any other class: a mixed entry not seen yet).
*glyph (may be NULL) is set to which of ours */
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

/* a row's height where pictures may be drawn: a text line's and a tenth (the line has its own room over and under
its letters: a picture as high as that row is about one and a half times the capitals) */
int ae_waypoint_row_height(int text_line_height);
/* the size a picture is drawn at in a row: as high as the row, its own shape kept, at most six times as wide as
high (wider: made lower, to that width); *scale (may be NULL) from its texels to that size. 0 for a sprite or row of
no size */
int ae_waypoint_icon_size(int sprite_width, int sprite_height, int row_height, int *width, int *height, float *scale);

/* the parts of a label's line, in order, for a line of `members` labels (at one time when more than one), each
with a picture or not, and a time or none:
  TEXT   one "NAME 0:07"; several "0:07 A · B" (as before pictures)
  BOTH   one "NAME 0:07 [picture]"; several as TEXT's (too wide with a picture each)
  ICONS  "[picture] 0:07", every member its picture (its name where it has none), the time last
Returns the count (at most max_parts; members that do not fit are left out, the time never) */
int ae_waypoint_line_parts(int style, int members, const int *member_has_icon, int has_time,
	struct ae_waypoint_part *parts, int max_parts);

#endif
