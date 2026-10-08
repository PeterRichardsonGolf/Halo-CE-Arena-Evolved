/*
AE_WIDGETS.H

The AE menus' widget core (ae_widgets.c, spec 4.1-4.3): the pointer's hit
rectangles, text fitted and wrapped, the separator dot and dashed outlines,
the row, the group header, and the list with its scrollbar. Each widget is one
call taking a rectangle, its state and a density (ae_style.h), drawing through
ae_draw.h in the current view's units and recording its hit rectangles. Sizes
and colours come from ae_style.h and the spec; this file and ae_widgets.c are
the only place they are applied. Pure: no engine includes (unit tests:
port/linux/tests/ae_widgets_test.c with the recording ae_draw stub).
*/

#ifndef __AE_WIDGETS_H
#define __AE_WIDGETS_H

#include "ae_style.h"
#include "ae_list.h"
#include "ae_ui.h"
#include "ae_motion.h"

/* hits: what the pointer can act on, recorded as widgets draw, in layout units of the whole frame */
enum { AE_HITS_MAXIMUM = 512 };
enum { AE_PART_ROW, AE_PART_ARROW_LEFT, AE_PART_ARROW_RIGHT, AE_PART_VALUE, AE_PART_TRACK, AE_PART_THUMB,
	AE_PART_TAB, AE_PART_DOT, AE_PART_PROMPT, AE_PART_CHIP, AE_PART_ITEM, AE_PART_KEY, AE_PART_CHOICE,
	AE_PART_FIELD, AE_PART_CARD, AE_PART_OUTSIDE };
struct ae_hit { struct ae_rect rect; short id, part, index, layer; };
void ae_hits_clear(void);                      /* before ae_ui_draw each frame (ae_hooks) */
void ae_hits_layer(short layer);               /* the screen index drawing now (before_draw) */
/* nonzero when a hit found the AE_HITS_MAXIMUM full since the last clear (it was dropped: ae_hooks logs it) */
int ae_hits_overflowed(void);
/* x, y, width, height in the current ae_draw view's units (stored in layout units via ae_draw_current_view) */
void ae_hit_add(float x, float y, float width, float height, short id, short part, short index);
/* the last added hit under a layout point at layer >= minimum_layer; 0 when none */
int ae_hit_at(float x, float y, short minimum_layer, struct ae_hit *hit);
/* text fitted to width with "…" (U+2026); 1 when cut; never splits a UTF-8 sequence */
int ae_fit_text(int font, float size, const char *text, float width, char *out, int out_size);
/* text wrapped at spaces into at most maximum lines of width (a word wider than the width is cut with "…", and
so is the last line when more text is left); returns the line count */
int ae_wrap_text(int font, float size, const char *text, float width, char lines[][160], int maximum);
/* the separator: a drawn dot (never U+00B7) of diameter u, centred on x, y */
void ae_widget_separator(struct ae_density const *density, float x, float center_y, float diameter_u,
	unsigned int rgba);
/* a dashed outline (chips, preview area, open card): dash_u on, dash_u off */
void ae_widget_dashed(float x, float y, float width, float height, float thickness, float dash, unsigned int rgba);

enum
{
	AE_ROW_FOCUSED = 1 << 0, AE_ROW_HOVER = 1 << 1, AE_ROW_CHANGED = 1 << 2, AE_ROW_AE = 1 << 3,
	AE_ROW_DISABLED = 1 << 4, AE_ROW_ON_BAR = 1 << 5,   /* drawn on the selection bar (dark text) */
	AE_ROW_BAR_BY_LIST = 1 << 6,                       /* the list draws the moving bar, the row not */
	AE_ROW_HOVER_LEFT = 1 << 7, AE_ROW_HOVER_RIGHT = 1 << 8,
	/* (with AE_ROW_BAR_BY_LIST) the moving bar overlaps the row: the row draws no background either */
	AE_ROW_UNDER_BAR = 1 << 9
};
struct ae_row
{
	const char *label;          /* UTF-8 */
	const char *value;          /* NULL: an action row (opens something) */
	const char *base_value;     /* the struck base value, shown when AE_ROW_CHANGED */
	const char *reason;         /* shown instead of the value when AE_ROW_DISABLED */
	unsigned int flags;
	short tick_side;            /* value tick: -1, 1, 0 none */
	struct ae_motion const *tick;
	const char *old_value;      /* during a tick */
};
/* a row's parts (drawing units, x from the row's left) */
struct ae_row_layout
{
	float height, label_x, label_width, value_x, value_width, base_x, base_width, dot_x, badge_x, badge_width;
	float arrow_left_x, arrow_right_x, arrow_cell;
	int label_cut;
	char label[160];            /* the label as drawn (with "…" when cut) */
	/* (M2) the struck base value as drawn ("…" when short of room, empty when left out), and whether the changed dot
	and the AE badge are drawn (left out only when even an empty label leaves them no room) */
	char base[160];
	int dot, badge;
};
void ae_row_layout(struct ae_density const *density, float width, struct ae_row const *row,
	struct ae_row_layout *layout);
float ae_widget_row(struct ae_density const *density, float x, float y, float width, struct ae_row const *row,
	short hit_id, short index);                                     /* returns its height */
float ae_widget_group(struct ae_density const *density, float x, float y, float width, const char *label);

typedef void (*ae_list_item_draw)(void *context, short item, float x, float y, float width, float height,
	unsigned int flags /* AE_ROW_* */);
struct ae_list_view
{
	struct ae_list list;
	struct ae_motion scroll, bar;   /* first (rows) and the selection bar's row (items; drawn in drawing units), 100 ms */
	short previous_focus;
	short hover;                    /* -1 none */
	int mouse;                      /* the scrollbar's mouse look, after a mouse move, until pad / key input */
	int dragging;
	float drag_grab;                /* where in the thumb it was grabbed */
	/* (as last drawn, layout units: the scrollbar's track and thumb, for the pointer) */
	float track_top, track_height, thumb_height;
	/* (as last drawn, layout units: the list's whole rectangle, the wheel's target) */
	struct ae_rect area;
};
/* a list of count items in rows rows: the focus on the first, no motion running, nothing hovered */
void ae_list_view_init(struct ae_list_view *view, short count, short rows);
/* lays out rows of the density's row height and gap in height, draws the visible ones through draw_item (clipped),
the moving selection bar, overflow fades and "▲ / ▼ N more", and the scrollbar; records hits */
void ae_widget_list(struct ae_density const *density, struct ae_list_view *view, float x, float y, float width,
	float height, ae_list_item_draw draw_item, void *context, short hit_id);
short ae_list_view_rows(struct ae_density const *density, float height);
/* the thumb, in the list's own coordinates (the track from 0 to track_height): never from view or frame offsets
(the M1 right-hand-view bug) */
void ae_scrollbar_thumb(short count, short rows, float first, float track_height, float minimum,
	float *thumb_y, float *thumb_height);
/* the first row for a thumb dragged to thumb_y (same coordinates) */
short ae_scrollbar_first(short count, short rows, float track_height, float thumb_height, float thumb_y);
/* events: up/down move (cursor sound), triggers / PgUp / PgDn page by the visible rows minus one, wrap off;
returns 1 when handled */
int ae_list_view_event(struct ae_list_view *view, struct ae_event const *event);
/* pointer: hover focuses only when pointer->moved; the wheel scrolls 3 rows and never moves the focus unless it
leaves the window; track click pages; thumb drags; returns the item clicked or -1 */
short ae_list_view_pointer(struct ae_list_view *view, struct ae_pointer const *pointer, short hit_id);

#endif
