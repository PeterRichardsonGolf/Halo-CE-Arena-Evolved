/*
AE_MARKER_RULES.H

MARKERS and the FEED (ae_marks.c, hud_messaging.c, ae_hud_messages.c), as pure rules (port/linux/tests/ae_marker_rules_test.c
runs them): a marker's size by where it is on the screen, the feed's merged lines and their words, which message slot a new
line takes, and which text rows an open wheel hides. All numbers are the view's pixels at 1080 lines, x right and y down.
*/

#ifndef __AE_MARKER_RULES_H
#define __AE_MARKER_RULES_H

#include <wchar.h>

/* ---------- the marker's size */

/* e: how far a marker is from the view's middle, in the marks' ellipse's radii (840 across and 340 down at 1920 x 1080): the
ellipse is 0.4375 of the view's width and 340 of 1080 of its height. dx and dy are from the view's middle */
float ae_marker_e(float dx, float dy, float view_width, float view_height);

/* the glyph's size: 36 x the waypoint's scale (hud_item_timers_ae_waypoint_scale x the pixels of a HUD unit at 1080 lines, 2.25)
x f(e), whole pixels (halves round down), not under 20. f is 0.72 to e 0.30, linear to 1 at 0.50, 1 to 0.70, linear to 1.20 at
1.00, and 1.20 past it (off the screen) */
float ae_marker_size(float waypoint_scale, float e);
/* f(e) alone */
float ae_marker_f(float e);

/* the size eased to its target: current moved toward target by dt_ms of the 120 ms it takes */
float ae_marker_ease(float current, float target, long dt_ms);

/* a call made again at the same place: its glyph is larger by 1.12 for each repeat, two at most */
float ae_marker_repeat_scale(int repeats);

/* the OK flash on a marker, since_ms after the answer: the ring's radius (out to 38 over 240 ms), its alpha (fading out) and
the glyph's lift (1.15 settling to 1 over 160 ms). Returns 0 once the ring is over (the lift too) */
int ae_marker_flash(long since_ms, float *radius, float *alpha, float *lift);

/* how many names the OK row shows of count answers, and how many it leaves to "+n" */
#define AE_OK_NAMES_MOST 3
void ae_ok_names_split(int count, int *shown, int *more);

/* ---------- the style */

/* the inks of a marker's pieces: the HUD line (the lead line, the pill, a power item's picture), the white of its text and glyph,
the label blue (the owner, the count), the enemy red, the dark of the keylines and the flash. HUD STYLE AE's are the strip's own;
any other style has the forms in CE's own: ce_line is CE's message colour, the line and the label in it, the neutral white the
marks always had, the enemy red of the classic marks and a black keyline */
struct ae_marker_ink
{
	float line[3], white[3], blue[3], red[3], dark[3], flash[3];
};
void ae_marker_ink_get(int ae, float const ce_line[3], struct ae_marker_ink *ink);

/* whether the name and distance row is drawn in a waypoint style (AE_WAYPOINT_TEXT, ICONS or BOTH): the pictures alone have
none; the prompt's, the fact's and the owner's rows answer the call, and stay in every style */
int ae_marker_name_row_shown(int waypoint_style);

/* ---------- the lead line and the block of rows */

/* the lead line of a marker near the crosshair (and of one with a fact): from 6 x s off the glyph's upper corner on the side
away from the view's middle, up and out 16 x s across and 22 x s up (36 x s where the block would meet the reticle's zone, a
circle of 66 x s about the middle), then a shelf ending 4 x s past the widest row. The block's rows stand on it, 9 x s above it,
and start at the shelf's start. At the view's edge the lead flips toward the middle. All of it in the view's pixels. s is
the view's scale (the strip's: 1, 0.85, 0.75, times the screen's lines over 1080) */
struct ae_marker_lead_in
{
	float view_width, view_height, s;
	float glyph_x, glyph_y, glyph_size;	/* the glyph's middle and its square */
	float block_width, block_height;	/* the rows, the widest and all of them with their gaps (no more than that: the 9 is the lead's) */
};

struct ae_marker_lead_out
{
	int dir;							/* +1 out to the right, -1 to the left */
	float start_x, start_y;				/* where the line starts, at the glyph */
	float elbow_x, elbow_y;				/* where it meets the shelf: the rows' edge, and the shelf's height */
	float shelf_end_x;					/* the shelf's far end */
	float rows_bottom;					/* the bottom of the last row, 9 x s over the shelf */
	float box[4];						/* all of it: the shelf and the rows over it, x0 y0 x1 y1 */
	int raised;							/* the rise is the long one */
	int flipped;
};

#define AE_MARKER_ZONE 66.0f
void ae_marker_lead(struct ae_marker_lead_in const *in, struct ae_marker_lead_out *out);

/* ---------- the feed */

/* the colour runs in a feed line's text: from the mark to the next one, the HUD white or the label blue. A line starts in
white (ae_hud_messages.c draws them; any other text has none) */
#define AE_RUN_WHITE ((wchar_t)0xE000)
#define AE_RUN_BLUE ((wchar_t)0xE001)
/* the kill feed's lines (ae_killfeed_rules.h) add a run of any colour: AE_RUN_RGB and three characters AE_RUN_BYTE(red, green,
blue), and a mark at a line's start that the line is not to wrap */
#define AE_RUN_RGB ((wchar_t)0xE002)
#define AE_RUN_NOWRAP ((wchar_t)0xE003)
#define AE_RUN_BYTE(value) ((wchar_t)(0xE100 + ((value) & 0xFF)))
#define AE_RUN_BYTE_VALUE(c) ((int)((c) - 0xE100))
#define AE_RUN_IS_BYTE(c) ((c) >= 0xE100 && (c) <= 0xE1FF)

/* the last feed line a view had: who called, which call (any number that stands for the same words), when (game ticks),
and how many times over. All zero is none yet */
struct ae_feed_merge
{
	int caller;
	int call;
	long at;
	int count;
};

#define AE_FEED_MERGE_TICKS 180	/* 6 seconds */
#define AE_FEED_MERGE_MOST 99

/* a feed line of this caller's call at now: 0 for a new line (it becomes the last), else the count to show (2, 3...) for the
line that is already there: the same caller and call within 6 seconds of the last, which restarts the 6 seconds */
int ae_feed_merge(struct ae_feed_merge *merge, int caller, int call, long now);

/* the words of a call (entry: the wheel's AE_WHEEL_*; kind: AE_MARK_KIND_*): "Enemy spotted", "Going here", "Go here" (the
place of someone dead), "Help me", "Regroup", "Need a ride", "Attacking here", "Defending here", "Watching here", and for
a tap "<noun> here" (noun: Rockets, Flag, Fight...) or "<noun> m:ss" when seconds is 0 or more (a timer), "Go here" for the
bare ground. Returns the length written, 0 when size has no room */
int ae_feed_words(wchar_t *out, int size, int entry, int kind, int dead, wchar_t const *noun, int seconds);

/* the line: the caller and a colon in the label blue, the words in white, and " x2" in the label blue when count is over 1.
Returns the length written, 0 when size has no room */
int ae_feed_line(wchar_t *out, int size, wchar_t const *caller, wchar_t const *words, int count);

/* the same text without its runs (a HUD that draws no colour runs) */
void ae_feed_strip_runs(wchar_t *text);

/* a noun in capitals (ROCKETS) as the feed says it: the first letter alone in capitals */
void ae_feed_noun(wchar_t *out, int size, wchar_t const *name);

/* ---------- the message slots */

#define AE_SLOT_NONE (-1)

/* one of the game's message slots: whether it holds a line, whether the line is a feed line (the rest are kill lines, and
every other line the game prints), and when it was printed */
struct ae_message_slot
{
	int valid;
	int is_feed;
	long time;
};

/* the slot a new line takes. A free slot first, the first of them. A kill line (new_is_feed 0) else replaces the oldest feed
line, and only with none the oldest kill line. A feed line takes the oldest feed line, and gets no slot (AE_SLOT_NONE) when
every slot holds a kill. With no feed line in the slots a kill takes what the game's choice took before: the oldest */
short ae_message_slot_choice(struct ae_message_slot const *slots, short count, int new_is_feed);

/* ---------- the open wheel and the text rows */

/* whether a text row is hidden under the open wheel: its box (x0, y0, x1, y1) comes within limit of the ring (a circle of that
radius about its centre: its outer edge with the focused sector's growth, so it does not change as the focus moves) or of the
focused word's box (NULL or empty for none) */
int ae_row_hidden_by_wheel(float const row_box[4], float centre_x, float centre_y, float radius, float const word_box[4],
	float limit);

#endif
