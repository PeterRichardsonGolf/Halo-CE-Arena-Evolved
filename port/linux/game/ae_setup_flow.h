/*
AE_SETUP_FLOW.H

What AE's menu screens of rows share (ae_setup_flow.c; the game setup spike's CREATE GAME, LOBBY, MAIN MENU,
MULTIPLAYER): the look (the title, its crumb and rule, the frame's positions), a column of rows with group headers
and the focused row's help under it, the prompts, moving the focus past headers, the pointer over the rows, an error
dialog. Rows are drawn by AE's widgets (ae_widgets.h); a row of an option of the game setup copy (AE_FLOW_ROW_OPTION)
shows its changed dot and the value it started from. Positions are fractions of the frame (ae_frame_compute: a
centred 16:9 box as tall as the window, the whole width when narrower), sizes u (1080 to the frame's height, times UI
SCALE), so every resolution and aspect draws the same layout. Include after cseries.h.
*/

#ifndef __AE_SETUP_FLOW_H
#define __AE_SETUP_FLOW_H

#include "ae_style.h"
#include "ae_widgets.h"

enum
{
	AE_FLOW_ROWS = 20,
	AE_FLOW_PROMPTS = 7,
	/* the row kinds the flow knows; a screen's own start at AE_FLOW_ROW_OWN */
	AE_FLOW_ROW_HEADER = 0,
	AE_FLOW_ROW_OPTION,
	AE_FLOW_ROW_OWN
};

/* the frame's positions (fractions of 1920 x 1080 at 100 %) */
#define AE_FLOW_TITLE_Y (70.0f / 1080.0f)
#define AE_FLOW_RULE_Y (156.0f / 1080.0f)
#define AE_FLOW_CONTENT_Y (190.0f / 1080.0f)
#define AE_FLOW_ROWS_WIDTH 0.46f
#define AE_FLOW_PANEL_X (1095.0f / 1920.0f)
/* sizes, u */
#define AE_FLOW_RULE_U 1.0f
#define AE_FLOW_CARD_GAP_U 8.0f
#define AE_FLOW_INSET_U 23.0f
#define AE_FLOW_PRIMARY_ROOM_U 16.0f

/* a row as drawn: what it is, its option (AE_FLOW_ROW_OPTION: ae_setup_options.h's), its texts (NULL value: an action
row; help: the line under the rows while it has the focus; reason: disabled, the reason shown in the value's place;
primary: the screen's primary action, AE_ROW_PRIMARY's look with AE_FLOW_PRIMARY_ROOM_U more room above it, pinned
under the scrolling rows when it is the last (0.3.9: it never scrolls away at 130 %); status: a value nothing changes
here, AE_ROW_STATUS's look, no arrows; quiet: a line nothing is done on, never lit (no selection bar) though its help
shows while it has the focus; subscreen: a row that opens a screen of its own, AE_ROW_SUBSCREEN's look (the value a
muted summary with a chevron, no arrows), the summary scrolling sideways when it does not fit (AE_ROW_MARQUEE,
marquee_ms into it)) */
struct ae_flow_row { short kind, option; const char *label, *value, *help, *reason; short primary, status, quiet;
	short subscreen; unsigned long marquee_ms; };

struct ae_flow_screen
{
	struct ae_density density;
	struct ae_frame frame;
	struct ae_flow_row rows[AE_FLOW_ROWS];
	short count, focus;
	short first;                          /* the first row drawn: the rows scroll when they don't all fit (130 %) */
	struct ae_rect rects[AE_FLOW_ROWS];   /* layout units, as last drawn (a picker's place; width 0: not drawn) */
	struct ae_prompt prompts[AE_FLOW_PROMPTS];
	short prompt_count;
	char value_texts[AE_FLOW_ROWS][48];   /* the screen's own value texts, kept while drawn */
	short hit_rows, hit_prompts;          /* the screen's hit ids */
};

float ae_flow_u(struct ae_flow_screen const *flow, float spec_units);
float ae_flow_left_x(struct ae_flow_screen const *flow);
float ae_flow_right_x(struct ae_flow_screen const *flow);
float ae_flow_cap_top(int font, float size, float center_y);
/* the FULL density and the frame for the window now (UI SCALE from the settings) */
void ae_flow_full_density(struct ae_flow_screen *flow);
/* the title top left, the crumb top right on its middle, the rule under them */
void ae_flow_title(struct ae_flow_screen *flow, const char *text, const char *crumb);
/* the content's bottom: above the prompts' row */
float ae_flow_content_bottom(struct ae_flow_screen const *flow);
/* the rows from AE_FLOW_CONTENT_Y down, width wide, the focus's lit; the focused row's help under them (2 lines).
Rows that don't fit above the help scroll with the focus, with "▲ / ▼ N more" lines where rows are left out; a
primary last row stays drawn under them */
void ae_flow_draw_rows(struct ae_flow_screen *flow, float width);
void ae_flow_draw_prompts(struct ae_flow_screen *flow);
struct ae_prompt ae_flow_prompt(int button, const char *label, int action);
/* up / down past headers; returns whether it moved (the cursor sound when it did) */
int ae_flow_focus_step(struct ae_flow_screen *flow, short direction, int repeat);
/* the pointer over the rows: hover focuses; the row clicked, -2 / -3 for the focused row's left / right arrow, or -1 */
short ae_flow_rows_pointer(struct ae_flow_screen *flow, struct ae_pointer const *pointer);
/* an error dialog over the frame (the glue's reason as it is) */
void ae_flow_error(struct ae_flow_screen *flow, const char *title, const char *body);

#endif
