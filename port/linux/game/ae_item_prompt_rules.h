/*
AE_ITEM_PROMPT_RULES.H

The ITEM PROMPT (ae_item_prompt.c) as pure rules (port/linux/tests/ae_item_prompt_rules_test.c runs them): what the prompt of an
item, a vehicle, a flip, a device or a weapon to switch to says, and its sizes, its fit to a width, its place and its fade. HUD STYLE
AE only; the Classic HUD keeps the game's line.

One view: a block under the reticle, the item's name in capitals over the action ("Hold [chip] to swap"), sized by WAYPOINT SIZE.
Two to four views: one line, "NAME Hold [chip] to swap", the first line of the message list (ae_hud_messages.c); the words and the
no-wrap are the glue's, the name's cut to the view is here.

All numbers are screen pixels of the view, x right and y down. s is the view's scale (the score strip's: 1 in one view times the
screen's height over 1080).
*/

#ifndef __AE_ITEM_PROMPT_RULES_H
#define __AE_ITEM_PROMPT_RULES_H

#include <wchar.h>

/* what the prompt is for */
enum
{
	AE_IP_NONE,
	AE_IP_PICKUP,		/* Hold [chip] to pick up */
	AE_IP_SWAP,			/* Hold [chip] to swap */
	AE_IP_ENTER,		/* Press [chip] to enter driver seat (detail: the seat's word) */
	AE_IP_FLIP,			/* Press [chip] to flip */
	AE_IP_DEVICE,		/* Press [chip] to touch device (no name) */
	AE_IP_CUSTOM,		/* Press [chip] to <detail>, a device's own word (no name) */
	AE_IP_SWITCH,		/* Press [chip] to switch (out of ammunition) */
	AE_IP_REVIVE,		/* Hold [chip] to revive, a downed teammate's name over it (the button ringed by the revive's progress) */
	AE_IP_KINDS
};

#define AE_IP_NAME_SIZE 32
#define AE_IP_BEFORE_SIZE 8
#define AE_IP_AFTER_SIZE 64

/* the words round the chip, fixed English: before ("Hold", "Press") and after ("to swap"). detail: the seat's word for an
enter and a device's own word for a custom one; NULL or empty for none. FALSE (both empty) for no prompt */
int ae_ip_words(int kind, wchar_t const *detail, wchar_t *before, wchar_t *after, int after_size);
/* whether a kind has a name row (a device has none: the action alone) */
int ae_ip_kind_named(int kind);
/* a name in capitals: the source with each letter upper cased, size characters at most, the rest cut */
void ae_ip_upper(wchar_t const *source, wchar_t *name, int size);

/* ---------- sizes */

/* the sizes of the block at one scale and the pieces' rules: all whole pixels */
struct ae_ip_sizes
{
	int name;		/* the name's em: 22 at 70 %, 1080 lines */
	int action;		/* the action text's em: 18 */
	int chip;		/* the chip's height: 25 */
	int gap;		/* the chip's body to the words, each side: 6 */
	int row_gap;	/* the name's baseline to the chip's top: 6 */
};

/* the size scale k from the waypoint text scale (hud_item_timers_ae_waypoint_scale(0): 0.49 at the default 70 % in one view) and
the drawable's height in pixels (0 for unknown: 1080): k = 1 at the default at 1080 lines */
float ae_ip_scale(float waypoint_scale, float drawable_height);
/* the sizes at k: action 18 k, at least 16; name 22 k, at least action + 1; chip 1.39 x action, then the capitals' odd or even;
gap action / 3; row gap 6 k, at least 4 */
void ae_ip_sizes_get(float k, struct ae_ip_sizes *sizes);
/* the sizes when the action row steps down to this action size (name and row gap as they were; chip and gap follow the action) */
void ae_ip_sizes_for_action(struct ae_ip_sizes const *nominal, int action, struct ae_ip_sizes *sizes);
/* the widest the block is: 338 x s (it does not grow with WAYPOINT SIZE) */
float ae_ip_width_limit(float view_scale);
/* the capitals' top of the name: floor(the view's middle y + 93 s + 0.5) */
float ae_ip_top(float view_centre_y, float view_scale);

/* a chip under 20 px with a key's label of 4 letters or more is the key's name as a plain word (SHIFT, ENTER, SPACE, CTRL), not a
chip; TAB and single keys stay chips, and a mouse button's picture stays */
#define AE_IP_WORD_BELOW 20
int ae_ip_chip_is_word(int chip_px, wchar_t const *label);

/* ---------- fit */

typedef float (*ae_ip_measure)(wchar_t const *text, float px);
/* the chip's width (a word's width in the action text's size when the rule above made it a word) at a chip and an action size */
typedef float (*ae_ip_chip_width)(int chip_px, int action_px, void *context);

struct ae_ip_text
{
	wchar_t name[AE_IP_NAME_SIZE];		/* empty for none */
	wchar_t before[AE_IP_BEFORE_SIZE];
	wchar_t after[AE_IP_AFTER_SIZE];
};

struct ae_ip_layout
{
	struct ae_ip_sizes nominal;		/* the sizes at k: where the baselines and the chip's middle come from */
	struct ae_ip_sizes drawn;		/* the sizes drawn: the action stepped down to fit, the name stepped or cut */
	wchar_t name[AE_IP_NAME_SIZE];	/* the name as drawn (cut with an ellipsis when it still does not fit) */
	wchar_t after[AE_IP_AFTER_SIZE];	/* the words after the chip as drawn (cut with an ellipsis when the row is still too wide at the floor) */
	float name_width, before_width, after_width, chip_width, row_width;
};

/* the block at k within limit: the action row steps down a whole pixel at a time, chip and gaps following, to the floor of 16;
then the name steps down, never under the action's size; then the name is cut with an ellipsis. A row still wider than the limit at the floor has its words after the chip cut with an ellipsis to
what is left of the limit (a long custom device's own words) */
void ae_ip_layout_get(float k, float limit, struct ae_ip_text const *text, ae_ip_measure measure, ae_ip_chip_width chip_width,
	void *context, struct ae_ip_layout *layout);
/* any text cut to limit at px, the same way (size characters at most in out) */
int ae_ip_text_fit(wchar_t const *text, ae_ip_measure measure, float px, float limit, wchar_t *out, int size);
/* a name cut to limit at px, with "..." in place of what is left out: out gets the whole name when it fits. Returns whether it was cut */
int ae_ip_name_fit(wchar_t const *name, ae_ip_measure measure, float px, float limit, wchar_t *out, int size);

/* ---------- place */

struct ae_ip_place
{
	float name_x, name_baseline;				/* the name's left and baseline (unused with no name) */
	float row_x, before_x, chip_x, after_x;		/* the action row's left, the words and the chip's body left */
	float chip_top, chip_middle, action_baseline;
};

/* where each piece goes, on whole pixels: each row centred by its width on centre_x; the capitals' top (ae_ip_top) of the name; the
name's baseline and the chip's top from the sizes at k (so a name of another length, a row stepped down or no name moves nothing
the eye is on); the chip's middle the nominal one, the action's baseline from it and the drawn action size */
void ae_ip_place_get(float centre_x, float top, struct ae_ip_layout const *layout, struct ae_ip_place *place);

/* ---------- time */

/* the prompt's fade, in game ticks: it fades in over 3 from the tick it began (instant: at once, a new item while one is up, or one
back in range), stays 4 after it was last true, then fades out over 4. 0 to 1 */
#define AE_IP_FADE_IN_TICKS 3
#define AE_IP_HOLD_TICKS 4
#define AE_IP_FADE_OUT_TICKS 4
float ae_ip_alpha(long now, long began, long last_true, int instant);

/* ---------- the split line */

/* the split line's chip and gaps for the list's text size px (20.5 in two views, 18 in four): the chip about 1.35 x the capitals, odd or
even as they are (20 and 17), the chip's body 1/3 em from the words each side, the name 1 em from the action */
int ae_ip_split_chip(float text_px);
int ae_ip_split_gap(float text_px);
float ae_ip_split_name_gap(float text_px);
/* the whole split line within the view: the name first (it keeps its room, cut only when the line without the action's words is already
too wide), then the words after the chip get what is left, cut with an ellipsis. name_out and after_out are buffers of
AE_IP_NAME_SIZE and AE_IP_AFTER_SIZE characters; fixed_width is the words before the chip, the gap and the chip, after_gap the gap after the chip */
void ae_ip_split_fit(float room_total, float name_gap, float fixed_width, float after_gap, wchar_t const *name, wchar_t const *after,
	ae_ip_measure measure, float px, wchar_t *name_out, wchar_t *after_out);
/* the name's room in a list line: the view's width less 16 s at the edge, the line's left and the rest of the line (the action's
words, the chip and the name's gap to them); never negative */
float ae_ip_split_room(float view_width, float left, float view_scale, float rest_width);

#endif /* __AE_ITEM_PROMPT_RULES_H */
