/*
AE_MARK_PROMPT_RULES.H

The AIM PROMPT (ae_mark_prompt.c): the button and the word over what a mark would act on, as pure rules
(port/linux/tests/ae_mark_prompt_rules_test.c runs them): the word of each target and answer, the sizes of the row, and
where it sits. All numbers are screen pixels of the view, x right and y down; s is the view's scale (the strip's: 1
in one view, 0.85 for a half and 0.75 for a quarter, times the screen's height over 1080).
*/

#ifndef __AE_MARK_PROMPT_RULES_H
#define __AE_MARK_PROMPT_RULES_H

#include <wchar.h>

/* the word of a target (tier: AE_TIER_*) for the press that would be made (verb: AE_CALL_*, ae_mark_rules.h). item_name:
the item's, objective's or place's own word in capitals (SNIPER RIFLE, FLAG, HILL), or NULL. NULL: no prompt for it. The
answers to a marker are OK, NEVER MIND, DIBS and CANCEL; a body is FIGHT HERE and the ground GO HERE; the power items have their
waypoints' words (ROCKETS, SNIPER, CAMO). The word may be item_name itself */
wchar_t const *ae_prompt_word(int tier, int verb, wchar_t const *item_name);
/* whether the word is the enemy's, in enemy red (every other word is HUD white) */
int ae_prompt_word_is_enemy(int tier, int verb);

/* the row's sizes, a view's: split is one of several views; hud_scale is the screen's height over 1080 */
struct ae_prompt_metrics
{
	float chip_height;	/* the button's picture: 24, 20 in split */
	float word_px;		/* the word's em: 20, 16 in split */
	float chip_gap;		/* chip to word: 8 */
	float halo;			/* the row's box past its ink on every side: the keylines */
};
void ae_prompt_metrics_get(int split, float hud_scale, struct ae_prompt_metrics *metrics);

/* a box of the view: its corners */
struct ae_prompt_box
{
	float x0, y0, x1, y1;
};

#define AE_PROMPT_ROWS_MOST 8

/* where the prompt goes */
struct ae_prompt_in
{
	float view_width, view_height;
	float scale;					/* s */
	float width, height;			/* the row's box, its halo in */
	struct ae_prompt_box target;	/* where the target is on the screen */
	int tags;						/* names over the target: 0, 1 (its box in tag), or 2 and more, a pile */
	struct ae_prompt_box tag;
	int side;						/* which side beside it (ae_prompt_side_keep): 0 by where the target is */
	int row_count;					/* the game's text rows (the helper and the message list) */
	struct ae_prompt_box rows[AE_PROMPT_ROWS_MOST];
};

/* how the place was found */
enum
{
	AE_PROMPT_NONE,
	AE_PROMPT_ABOVE,		/* centred, its foot 8 x s above the target's top */
	AE_PROMPT_ABOVE_TAG,	/* the same over the one name over the target */
	AE_PROMPT_BESIDE,		/* by the target, on the side away from the view's middle, 8 x s clear */
	AE_PROMPT_BELOW			/* under the target, its top 8 x s under the target's foot (the text rows were in the way) */
};

#define AE_PROMPT_GAP 8.0f			/* clear of the target, a name and the text rows, x s */
#define AE_PROMPT_ZONE 66.0f		/* the reticle's zone, x s: the prompt stays outside it */
#define AE_PROMPT_CAP 150.0f		/* its nearest edge stays within this of the view's middle, x s */

/* the row's top left in *x and *y, whole pixels, and how it was placed: AE_PROMPT_NONE (nothing drawn) when no place
clears the zone and the text rows within the cap from the middle. A place inside the zone is moved out along the line from
the middle; the text rows move it under the target, then no place is none */
int ae_prompt_place(struct ae_prompt_in const *in, float *x, float *y);

/* a target by the reticle: the row stands in one place, by the reticle's zone, whatever the target does, since a target there
moves round the view's middle with every turn. Whether it is pinned now: in within the zone and the row's height of the middle,
out a quarter farther, so that it does not go in and out at the edge */
int ae_prompt_pin_keep(float target_x, float target_y, float view_x, float view_y, float scale, float row_height, int pinned);
/* the pinned place: right of the zone, centred on the view's middle, left when a text row or the view's edge is in the way.
AE_PROMPT_BESIDE, or AE_PROMPT_NONE when neither stands */
int ae_prompt_place_pinned(struct ae_prompt_in const *in, float *x, float *y);

/* the side a prompt beside its target takes, 1 left or 2 right. A target under the reticle wobbles either side of the view's
middle with every turn: the row keeps the side it has until the target is past the reticle's zone on the other. last: the side
it had, 0 for none (right, or the side the target is on) */
int ae_prompt_side_keep(float target_middle, float view_middle, float scale, int last);

#endif
