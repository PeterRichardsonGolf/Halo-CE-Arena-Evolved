/*
AE_VIEW_BOTS_RULES.H

DEV MODE's bot view (view bots, ae_view_bots.h), its pure rules: no engine includes, so
port/linux/tests/ae_view_bots_rules_test.c compiles them alone. The board is the owner's pick of 11-07 (spec
2026-11-07-view-bots-spec.md, section 12, direction D round 2): a see-through board of the team's squads under the DEV
MODE chip, two squad columns, a WHY strip as its last line, numbered tags over the bots in view and pips at the screen's
edge for those out of it. Every number is the spec's, at 1080p (1 unit = 1 pixel of a 1080-line frame).

What is here: the names (which system chose a bot's goal, roles, mark kinds, orders, state words), the board's geometry
(1p and split screen) and layout (squads into columns, the strip's choice, what gives way when it is too tall), the tags'
placement and fade, the pips' place on the edge, the aim focus, metres and ages, and fitting text to a width.
*/

#ifndef __AE_VIEW_BOTS_RULES_H
#define __AE_VIEW_BOTS_RULES_H

/* ---------- the data a board shows (filled on the host in bots.c's unit: ae_bots_view.inc) */

#define AE_VB_ROWS 32		/* bots on one board, at most */
#define AE_VB_SQUADS 8		/* squad groups on one board, at most (the last group is the bots in no squad) */
#define AE_VB_DETAIL 40
#define AE_VB_HEAD 64
#define AE_VB_NOTE 24
#define AE_VB_TICKS_PER_SECOND 30

/* which system chose a bot's goal (bots.c's bots_choose_goal asks them in this order; the first that says yes wins) */
enum
{
	AE_VB_SOURCE_NONE,
	AE_VB_SOURCE_VEHICLE,
	AE_VB_SOURCE_ZONE,
	AE_VB_SOURCE_SQUAD,
	AE_VB_SOURCE_MARK,
	AE_VB_SOURCE_OBJECTIVE,
	AE_VB_SOURCE_REVENGE,
	AE_VB_SOURCE_POWER,
	AE_VB_SOURCE_VANTAGE,
	AE_VB_SOURCE_ROUTE,
	AE_VB_SOURCE_TEAM,
	/* the stock brain: the enemy last seen, a better weapon, about the map (by the goal's kind) */
	AE_VB_SOURCE_BASE,
	AE_VB_SOURCES
};
/* the stock brain's goal kinds (bots.c's _bots_goal_*) */
enum { AE_VB_GOAL_NONE, AE_VB_GOAL_ENEMY, AE_VB_GOAL_ROAM, AE_VB_GOAL_ITEM };

/* the row's word: VEHICLE .. TEAM; the stock brain's HUNT, ITEM or ROAM by its goal's kind; "" for none */
char const *ae_vb_source_word(int source, int goal_kind);
/* the short code (the log's): VEH ZONE SQD MRK OBJ REV PWR VAN RTE TEAM BASE; "" for none */
char const *ae_vb_source_code(int source);

/* the squads' roles (ae_fireteam_rules.h's AE_ROLE_*, in its order): DEFEND OURS .. ROAM; "" for none or out of range */
char const *ae_vb_role_name(int role);
/* a ping's kind (ae_mark_rules.h's ENEMY, ITEM, PLACE, then ae_bots_marks.inc's VEHICLE, RIDE): "" out of range */
char const *ae_vb_mark_name(int kind);
/* a squad's standing order (ae_fireteam_rules.h's _ae_order_*): NONE FOLLOW REGROUP ATTACK DEFEND WATCH GO */
char const *ae_vb_order_name(int order);

/* a row's state word, in its priority: a fight, then stuck, no path, held to its leader */
enum { AE_VB_STATE_NONE, AE_VB_STATE_FIGHT, AE_VB_STATE_STUCK, AE_VB_STATE_NO_PATH, AE_VB_STATE_TETHER, AE_VB_STATES };
int ae_vb_state_pick(int fighting, int stuck, int no_path, int tethered);
char const *ae_vb_state_word(int state);
/* fighting: the bot's last fight under a second ago (combat.last_fight_tick; tick->fighting is not kept) */
int ae_vb_fighting(long now, long last_fight_tick);

struct ae_vb_row
{
	short brain;			/* the bot's index; its number is brain + 1, as the logs' "bot N" */
	long player_index;
	char name[16];
	short source;			/* AE_VB_SOURCE_* */
	short goal_kind;		/* AE_VB_GOAL_* */
	char detail[AE_VB_DETAIL];	/* the goal in words and its distance ("spot 2 · 58 m") */
	short state;			/* AE_VB_STATE_* */
	short dead;
	short back_in;			/* dead: the seconds to its respawn, 0 unknown ("dead · back in 4 s"; split screen "dead · 4 s") */
	short squad;			/* its group on the board */
	short has_goal;
	float goal[3];			/* its goal's point (world units) */
	char const *why;		/* its last WHY (a static text), NULL for none */
	long why_tick;
};

struct ae_vb_squad
{
	char head[AE_VB_HEAD];		/* "SQUAD A · P1 · FOLLOW" */
	char note[AE_VB_NOTE];		/* "leash 18 m", "no order", "2 of 4 there" */
	char letter[4];			/* "A", or "" for the bots in no squad */
	short rows;
};

struct ae_vb_board
{
	short team;			/* the team shown; -1: every bot (no teams) */
	char team_name[16];		/* "RED" */
	short count;			/* its bots, living or dead */
	short squads;
	struct ae_vb_squad squad[AE_VB_SQUADS];
	short rows;			/* in board order: by squad, then by number */
	struct ae_vb_row row[AE_VB_ROWS];
	char const *newest_why;		/* the team's newest WHY (a bot's or a call's), NULL for none */
	long newest_tick;
	short newest_brain;		/* -1: a NEED A RIDE call's ("CALL") */
	long now;
};

/* whole metres from world units (x 3.048, the markers' factor), never negative */
long ae_vb_metres(float world_units);
/* an age in ticks as the board says it: "0 s".."59 s", then "1 m".. */
void ae_vb_age_text(long ticks, char *text, int size);

/* ---------- the geometry (1080p units) */

struct ae_vb_geometry
{
	float band;			/* the board's width: always the band's (664), as squads split or merge */
	float bar, pad_t, pad_b, pad_l, pad_r;
	float gutter, col;
	int header;			/* 1p only: "BOTS · RED · 7" */
	float head_h, head_px, head_right_px, head_gap;
	float sq_h, sq_px, note_px, stack_gap;
	float row_h, num_px, name_px, src_px, det_px, state_px;
	float col_num, col_name, col_src, col_det, state_w;
	float strip_sep, strip_h, strip_px, strip_small_px;
};
/* split: 0 one view (G1), 1 split screen (G2: no header line, rows 20, text 14 / 11) */
void ae_vb_geometry_of(int split, struct ae_vb_geometry *geometry);

/* the board's left edge (layout units) in a frame layout_width wide: its right 16 left of the TEST RUN card's coded left
edge (the card 530 wide, 34 in from the 16:9 frame's right), so it never meets the card, up or not; a view of half the
frame's width (3 or 4 players) centres it in the view. Never under 8 */
float ae_vb_board_left(float layout_width, float view_x, float view_width, int half_width_view);
/* the board's top in a view: 64 (12 under the DEV MODE chip's foot, 52) with one view; 48 under a split view's top */
float ae_vb_board_top(float view_y, int split);
/* the crosshair area of a view (the reticle, 46 across, plus 40 round it): 126 square at the view's centre */
void ae_vb_crosshair_area(float view_x, float view_y, float view_width, float view_height, float box[4]);

/* ---------- the layout */

struct ae_vb_place
{
	short column;		/* 0 or 1 */
	float y;		/* its header's top, from the body's top */
	short shown;		/* its rows drawn (-1: the squad is not drawn: no room) */
	short more;		/* rows not drawn, said on its last line ("+N MORE"), 0 for none */
};

struct ae_vb_layout
{
	float x0, y0, width, height;
	short columns;		/* 1: one squad uses both columns' room */
	float col_x[2];		/* the columns' left, from the content's left */
	float col_w;
	float body_y;		/* the squads' top, from the board's top */
	float body_h;
	short strip_lines;	/* 0: no strip */
	float strip_y;		/* the strip's rule, from the board's top */
	struct ae_vb_place place[AE_VB_SQUADS];
};

/* lays the board out at (x0, top): the squads' row counts in board order, the strip's lines wanted (0..2). The first
two squads go side by side; a third and later go under the shorter column. Split screen: a view whose largest squad has
5 bots or more drops the strip (it would come to the crosshair). Taller than bottom: the strip goes, then rows are cut
from each column's foot, its last line saying how many more. Returns the board's height */
float ae_vb_layout(struct ae_vb_geometry const *geometry, int split, float x0, float top, float bottom,
	short const *rows, int squads, int strip_lines, struct ae_vb_layout *layout);

/* the strip's content: the focus bot's last WHY whatever its age (it has one); else the team's newest WHY under 30 s;
else none */
enum { AE_VB_STRIP_NONE, AE_VB_STRIP_FOCUS, AE_VB_STRIP_NEWEST };
int ae_vb_strip_pick(int focus, int focus_has_why, int has_newest, long newest_age_ticks);

/* ---------- the tags over the bots in view */

struct ae_vb_box { float x0, y0, x1, y1; };

/* the gap between two boxes (0 when they meet or overlap) */
float ae_vb_box_gap(struct ae_vb_box const *a, struct ae_vb_box const *b);

enum { AE_VB_TAG_ABOVE, AE_VB_TAG_AFTER, AE_VB_TAG_NUMBER, AE_VB_TAG_NUMBER_AFTER, AE_VB_TAG_FAINT };
/* a tag's place by its bot's name label: centred over it, its foot 8 over the label; else after it, 8 to its right,
centred on it; else the number alone (a circle, height across) over it, then after it. A place is taken when it keeps
clear of every block (other labels, tags placed, the board, the card) and stays inside the view. None free:
AE_VB_TAG_FAINT, the number over the label, drawn faint. Returns the kind; *box its box */
int ae_vb_tag_place(struct ae_vb_box const *label, float width, float number_width, float height,
	struct ae_vb_box const *blocks, int count, struct ae_vb_box const *view, float clear, struct ae_vb_box *box);
/* the tag's alpha: .35 inside the crosshair area or when forced faint (no free place), else 1 */
float ae_vb_tag_alpha(struct ae_vb_box const *tag, float const crosshair[4], int faint);
#define AE_VB_FADE 0.35f

/* ---------- the pips at the edge for bots out of view */

/* a view-space point (x right, y up, z backward: the camera looks down -z) as a direction on the screen from the view's
centre (dx right, dy down, the larger of them 1): one behind the camera points to the bottom edge, its side kept */
void ae_vb_off_direction(float x, float y, float z, float *dx, float *dy);
/* the pip's anchor on the view's edge, inset in from it, along that direction; its side (the triangle points that way) */
enum { AE_VB_SIDE_LEFT, AE_VB_SIDE_RIGHT, AE_VB_SIDE_TOP, AE_VB_SIDE_BOTTOM };
int ae_vb_pip_anchor(float dx, float dy, struct ae_vb_box const *view, float inset, float *x, float *y);
/* the pip's box (the pill and its triangle, width x height with the triangle outside it on that side) at the anchor, slid
along its edge until it keeps clear of every block and stays inside the view (steps of 8 either way, up to half the
edge); 0 when nowhere is free (not drawn) */
int ae_vb_pip_place(int side, float x, float y, float width, float height, float triangle,
	struct ae_vb_box const *blocks, int count, struct ae_vb_box const *view, float clear, struct ae_vb_box *box);
/* a squad shows one pip when its living bots out of view are within 30 m of each other (world units, x y z) */
int ae_vb_together(float const (*points)[3], int count);

/* ---------- the aim focus */

/* the nearest of the bots to the aim (degrees off it), named in view and within 6 degrees; -1 for none */
int ae_vb_aim_pick(float const *degrees, int const *eligible, int count);
/* the focus held: a pick holds 1 s after the aim leaves it, so a row does not flicker; returns the brain focused (-1) */
int ae_vb_aim_hold(int held, long held_tick, int picked, long now);

/* ---------- text */

/* a text's width at the drawing's font (the caller's measure) */
typedef float (*ae_vb_measure)(char const *text, void *context);
/* the text cut to room, ending in an ellipsis when it was cut; out holds size bytes */
void ae_vb_fit(char const *text, float room, ae_vb_measure measure, void *context, char *out, int size);
/* the text in at most lines lines of room each, broken at spaces; the last ends in an ellipsis when it was cut. Returns
the lines used (0 for an empty text) */
#define AE_VB_LINE 96
int ae_vb_wrap(char const *text, float room, int lines, ae_vb_measure measure, void *context, char out[][AE_VB_LINE]);

#endif /* __AE_VIEW_BOTS_RULES_H */
