/*
AE_WHEEL_LAYOUT.H

The CALLOUT WHEEL's ring (ae_wheel_draw.c): where everything of it sits, as pure numbers (port/linux/tests/
ae_wheel_layout_test.c runs them). All of it is the ring's own pixels about its centre, x right and y down, as the
HUD draws; nothing here needs the game.

One view, the ring is 1 at 1080 lines; a split view takes it at 0.68 (its views are 540 high at 1080, so one scale
serves for two, three and four players). Both then follow the view's height, never its width. Texts and the word's
cell keep their floors in screen pixels; the sprite and the gaps given whole stay as given.
*/

#ifndef __AE_WHEEL_LAYOUT_H
#define __AE_WHEEL_LAYOUT_H

#include <wchar.h>

/* the ring in a split view, of the ring in one view */
#define AE_WHEEL_SPLIT_SCALE 0.68f

struct ae_wheel_layout
{
	int split;
	float scale;			/* the ring's: 1 or AE_WHEEL_SPLIT_SCALE, times the height's */
	float height_scale;		/* the height's alone: a view of 1080 lines is 1 (a split view of 540 lines too) */
	float inner, outer;		/* the sectors' radii */
	float gap;				/* between two sectors' lines, parallel from the hub to the rim */
	float corner;			/* the cut off each sector's four corners */
	float grow;				/* the focused sector's outer radius grows this much */
	float line;				/* the lines' width (the HUD line), and its dark keyline under it */
	float keyline;
	float icon;				/* an icon's square, whole pixels */
	float ride;				/* the steering wheel's picture's square, whole pixels */
	float sprite_width;		/* the rifle's sprite, whole pixels, and its height as laid out (the map's own aspect may differ) */
	float sprite_height;
	float rifle_boost;		/* the sprite's ink, over 1 to thicken it */
	float caption_px;		/* a caption's size, floored */
	float wedge_tip, wedge_base, wedge_half, wedge_notch;
	float word_height;		/* the focused word's cell, floored */
	float word_px;
	float word_pad;			/* the room in the cell, each side of the text, less the lean */
	float word_past;		/* its gap from the ring */
	int word_above;			/* above the ring (split: the score strip is under it), else below */
	float word_nudge;		/* above, the cell moves this much further up */
	int legend;				/* the legend of hints under the ring: one view only */
	float legend_y;			/* its middle under the ring's */
	float legend_px;
	float chip_height;
	float chip_gap;			/* from a chip to its word */
	float legend_gap;		/* between two items */
	float died_y;			/* "PLACED WHERE YOU DIED" under the ring's middle */
	float died_px;
};

/* a view's layout. view_height: the view's own height in pixels (in a split view too, not the screen's); split: one of
several views on the screen */
void ae_wheel_layout_get(int split, float view_height, struct ae_wheel_layout *layout);
/* how far the ring reaches from its centre, the focused sector's outer radius (what must keep clear of it measures
from here) */
float ae_wheel_outer_radius(int split, float view_height);

/* the ring's centre in a view of that size: whole pixels, the view's middle */
void ae_wheel_centre(float view_width, float view_height, float *x, float *y);

/* a sector's outline, clockwise on the screen from the outer arc's start, about the ring's centre: the outer arc, the cut
corners, the inner arc. grown: the focused one's. Returns the number of points (at most AE_WHEEL_SECTOR_POINTS) */
#define AE_WHEEL_SECTOR_POINTS 40
int ae_wheel_sector_polygon(struct ae_wheel_layout const *layout, int entry, int grown, float points[][2]);
/* whether a point (about the centre) is inside the sector's body, its parallel sides and its radii */
int ae_wheel_sector_contains(struct ae_wheel_layout const *layout, int entry, int grown, float x, float y);
/* the direction wedge's outline for a focused entry: the tip, a wing, the notch, the other wing, with the middle of each
side of the tip added (the soft fill is a fan from the corners' mean, which must see every corner: a dart's four are
not enough, its notch bends in). Returns AE_WHEEL_WEDGE_POINTS */
#define AE_WHEEL_WEDGE_POINTS 6
int ae_wheel_wedge_polygon(struct ae_wheel_layout const *layout, int entry, float points[AE_WHEEL_WEDGE_POINTS][2]);

/* where an entry's parts sit, about the centre, whole pixels: the icon's middle (the rifle's sprite's, the steering
wheel's picture's), the caption's middle in x and the middle of its capitals in y. In split the caption sits at its
own offset from the sector's middle (ae_wheel_split_offset); the weapon's over its sprite */
struct ae_wheel_place
{
	float middle_x, middle_y;	/* the sector's middle (not rounded) */
	float icon_x, icon_y;
	float caption_x, caption_y;
	int caption_over_sprite;
};
void ae_wheel_entry_place(struct ae_wheel_layout const *layout, int entry, struct ae_wheel_place *place);
/* a split view's caption offset from the sector's middle at 1080 (found by a search for the best margin) */
void ae_wheel_split_offset(int entry, int *x, int *y);

/* the focused word's cell: its eight corners (a leaning box, clockwise from the top left), for a text of that width,
and its middle, about the centre */
void ae_wheel_word_cell(struct ae_wheel_layout const *layout, float text_width, float points[8][2], float *x, float *y);

/* the legend's items from the left, each a chip (width 0: none) and its word, centred on the ring: where each item
starts, the whole width. Returns the width */
float ae_wheel_legend_place(struct ae_wheel_layout const *layout, int count, float const *chip_widths,
	float const *word_widths, float *lefts);

/* the words: a caption (SMART, ATTACK...) and the whole call (SMART CALL, ATTACKING HERE...), alive or dead */
wchar_t const *ae_wheel_caption(int entry, int dead);
wchar_t const *ae_wheel_word(int entry, int dead);

/* the motion: the send flashes the focused sector to white in AE_WHEEL_FLASH_MS, then the ring closes in
AE_WHEEL_CLOSE_MS (opening is ae_wheel_open_part's). since_ms: from the send. Returns 0 once it is over */
#define AE_WHEEL_FLASH_MS 60
#define AE_WHEEL_CLOSE_MS 80
int ae_wheel_motion(long since_ms, float *flash, float *alpha);

#endif
