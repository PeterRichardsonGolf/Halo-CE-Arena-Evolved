/*
AE_MEDAL_DRAW_RULES.H

The medals shown on screen as pure rules (port/linux/tests/ae_medal_draw_rules_test.c runs them): where a medal's place is in the
holder, how it moves, fades in, holds and fades out, how many are showing at a time, and how a medal is shown at all (a picture, a
line of text where it has no picture, or a line in the message list in split screen). The numbers are a stage of 1920 by 1080:
the holder is at 150, 525; the pictures are 80 across at 100 apart, four to a row, left to right, then the next row down.

Time is in game ticks, not in real milliseconds: a paused game holds its medals still, as it holds the rest of the HUD. The game
runs 30 ticks a second.
*/

#ifndef __AE_MEDAL_DRAW_RULES_H
#define __AE_MEDAL_DRAW_RULES_H

#include <wchar.h>

#define AE_MEDAL_TICKS_PER_SECOND 30
/* the holder's place on the stage, and where a medal starts across from its place (the holder's own units) */
#define AE_MEDAL_HOLDER_X 150.0f
#define AE_MEDAL_HOLDER_Y 525.0f
#define AE_MEDAL_START_X 156.0f
/* a slot: AE_MEDAL_COLUMNS to a row, the pitch apart; a picture is AE_MEDAL_PICTURE across */
#define AE_MEDAL_COLUMNS 4
#define AE_MEDAL_PITCH 150.0f
#define AE_MEDAL_ROW_PITCH 128.0f
#define AE_MEDAL_PICTURE 80.0f
/* its name under it, centred: the room under the picture, its size at 1080 lines, how light its colour is made to read */
#define AE_MEDAL_CAPTION_GAP 4.0f
#define AE_MEDAL_CAPTION_SIZE 18.0f
#define AE_MEDAL_CAPTION_LIGHT 0.62f
/* a name wider than this (at the caption's size, the stage's pixels) breaks at a word to a second line, this far under the first (the same
size); one long word has nowhere to break and stays whole */
#define AE_MEDAL_CAPTION_WIDTH 142.0f
#define AE_MEDAL_CAPTION_LINE_PITCH 20.0f
/* a medal with no picture is its name as text: one under another from the holder's left (its name is wider than a slot), the
rows this far apart, its size in stage lines */
#define AE_MEDAL_TEXT_PITCH 40.0f
#define AE_MEDAL_TEXT_SIZE 24.0f
#define AE_MEDAL_TEXT_ROWS 8
/* a medal's time: 2 s in (moving to its place while fading in), 2 s held, 2 s fading out */
#define AE_MEDAL_MOVE_TICKS (2 * AE_MEDAL_TICKS_PER_SECOND)
#define AE_MEDAL_HOLD_TICKS (2 * AE_MEDAL_TICKS_PER_SECOND)
#define AE_MEDAL_FADE_TICKS (2 * AE_MEDAL_TICKS_PER_SECOND)
#define AE_MEDAL_LIFE_TICKS (AE_MEDAL_MOVE_TICKS + AE_MEDAL_HOLD_TICKS + AE_MEDAL_FADE_TICKS)
/* with more than this many medals up, every picture is AE_MEDAL_SHRUNK_PICTURE across; the places keep their pitch (the block does not
tighten, so that the names, which break to two lines where long, have room); the names keep their size */
#define AE_MEDAL_SHRINK_ABOVE 4
#define AE_MEDAL_SHRUNK_PICTURE 56.0f
/* the most a view shows: a 17th takes the place of the oldest, so nothing runs past the array or the picture */
#define AE_MEDAL_MOST 8

/* how a medal is shown */
enum
{
	AE_MEDAL_SHOW_PICTURE,		/* its picture in the holder */
	AE_MEDAL_SHOW_TEXT,			/* its name in the holder, for want of a picture */
	AE_MEDAL_SHOW_FEED			/* its name as a line in the player's message list */
};

/* by the number of local players (more than one: split screen) and whether the medal has a picture to draw. Split screen is the
list's line whether or not there is a picture; one player's view has the holder, a picture if there is one */
int ae_medal_show_mode(int local_players, int has_picture);

struct ae_medal_shown
{
	int medal;
	long born;			/* the tick it was added */
	int slot;			/* the counter's number when it came */
	int counted;		/* still counted as active: until its fade starts */
};

/* one view's medals, oldest first */
struct ae_medal_holder
{
	struct ae_medal_shown shown[AE_MEDAL_MOST];
	int count;
	int active;
	long last;			/* the tick of the last call */
};

void ae_medal_holder_reset(struct ae_medal_holder *holder);
/* the tick is now: medals past their life are gone; those whose fade has started stop being active, and with none active the slot
counter starts again. A tick before the last one is a new game (the clock starts again): all gone */
void ae_medal_holder_step(struct ae_medal_holder *holder, long now);
/* a medal joins at the next slot, which is returned; with the holder full the oldest goes */
int ae_medal_holder_add(struct ae_medal_holder *holder, int medal, long now);

/* 1 - (1 - t)^5, t held to 0..1 */
float ae_medal_quint_out(float t);
/* the factor of the pictures' size for this many medals up: 1, or the shrunk size's share of the full one past
AE_MEDAL_SHRINK_ABOVE (the places' pitch is not scaled by it: ae_medal_pose) */
float ae_medal_shrink(int count);
/* the same, the pitches of the places (not their start) times shrink */
int ae_medal_pose_shrunk(long age, int slot, int text, float shrink, float *x, float *y, float *alpha);
/* a medal's place and alpha at this age in ticks, on the stage: the top left of its 80 square, or of its line of text. 0 before it is
added and past its life, with the outputs left alone */
int ae_medal_pose(long age, int slot, int text, float *x, float *y, float *alpha);
/* the text's size in a view's pixels: the stage's size times the view's scale, never under 16 */
float ae_medal_text_pixels(float scale);
/* a caption's size at this view's scale, 16 px at the least */
float ae_medal_caption_pixels(float scale);
/* a text's width in the caption's face at this size in pixels (the engine's measure, or a stand-in in a test) */
typedef float (*ae_medal_measure)(wchar_t const *text, float px);
/* a caption as one line or two: a name that is no wider than limit is one line (first holds it, second is empty). A wider one breaks at
the space that leaves the narrower wider half (second line below the first, the same size); one with no space has no word to break and
stays one line, except UNFRIGGENBELIEVABLE, a fixed entry: UNFRIGGEN / BELIEVABLE. size is each buffer's length in characters; a name
that does not fit them is cut to one line. Returns the number of lines: 0 for no name */
int ae_medal_caption_lines(wchar_t const *name, ae_medal_measure measure, float px, float limit, wchar_t *first, wchar_t *second,
	int size);
/* a caption's colour from its picture: the picture's most common colour, made light enough to read; white for one with none */
void ae_medal_caption_color(unsigned char const *rgba, unsigned int width, unsigned int height, float color[3]);

#endif /* __AE_MEDAL_DRAW_RULES_H */
