/*
AE_SCREEN_PGCR.H

The post-game report (0.7.6; the approved option A: a board and a live detail pane): the result banner, the board of every
player (rank, colour chip, name, a strip of medals, K D A and the score; a team game under a bar for each team, the winner
first, a free for all by place) and, beside it, the pane of the player the cursor is on: weapon of choice, the stats of the
game's kind, medals, and who killed whom most. Nothing is pressed to open the pane.

Its numbers are ae_pgcr_screen_rules.h's, from ae_glue_pgcr.c. A number this machine does not have reads as a dash: a host that is
not Arena Evolved sends no summaries, an older build's summaries lack what a newer one counts.

Two homes. The post-game (ae_screen_postgame.c) draws it as its CARNAGE step and passes it the cursor's keys and the pointer;
the lobby reopens the last game's report as a screen of its own (ae_screen_pgcr_open_last), B closing it.

A player's card (the moderation card's pattern, 0.7.6): X on the post-game's player (A in the lobby's reopen, a click anywhere),
its rows SET RIVAL / REMOVE RIVAL (other players of real games) and SERVICE RECORD (ae_screen_record.h). A rival's rows are
marked on the board with a violet edge and a RIVAL tag, and on the in-game scoreboard.

Its log: "ae pgcr: card <name>", "ae pgcr: card closed", "ae pgcr: shown (<n> players, <teams|ffa>, summaries <yes|no>)", "ae pgcr: cursor <row> <name>",
"ae pgcr: closed".
*/

#ifndef __AE_SCREEN_PGCR_H
#define __AE_SCREEN_PGCR_H

#include "ae_pgcr_screen_rules.h"
#include "ae_ui.h"

struct ae_frame;
struct ae_density;

struct ae_pgcr_view
{
	struct ae_pgr_report report;
	int cursor;
	/* the board's scroll, in design units (the 1920 x 1080 of the mockups) */
	float scroll;
	/* A on a player opens their card (the lobby's reopen); the post-game's A is CONTINUE, so there it is X */
	int accept_opens_card;
	/* a player's card (SET RIVAL / REMOVE RIVAL, SERVICE RECORD) over the board: whose (the slot), its rows and their focus */
	int card_open, card_slot, card_focus, card_count;
	short card_rows[3];
	char card_note[96];
};

/* a report taken: the cursor on this machine's row, the scroll at the top */
void ae_pgcr_view_open(struct ae_pgcr_view *view, struct ae_pgr_report const *report);
/* a fresher report of the same game (the numbers settled): the cursor stays on its player */
void ae_pgcr_view_refresh(struct ae_pgcr_view *view, struct ae_pgr_report const *report);
/* UP, DOWN, TAB_PREVIOUS (LB) and TAB_NEXT (RB), PAGE_UP / PAGE_DOWN: 1 when the action moved or was taken */
int ae_pgcr_view_handle(struct ae_pgcr_view *view, int action);
/* whether a player's card is open over the board (the callers' prompts are then the card's) */
int ae_pgcr_view_card_is_open(struct ae_pgcr_view const *view);
/* whether the cursor's row can have a card (not a bot's): what the X prompt is shown for */
int ae_pgcr_view_can_open_card(struct ae_pgcr_view const *view);
/* hover moves the cursor; the wheel scrolls; a click on a player opens their card; in the card hover moves its focus, a
click takes a row, a click outside closes it */
void ae_pgcr_view_pointer(struct ae_pgcr_view *view, struct ae_pointer const *pointer);
/* draws the banner, the board and the pane in the frame (the backdrop is the caller's); returns the board's foot or the pane's,
whichever is lower, in layout units (for the prompts) */
float ae_pgcr_view_draw(struct ae_pgcr_view *view, struct ae_frame const *frame);
/* the footer's centre line in layout units, and its left and right */
void ae_pgcr_view_footer(struct ae_frame const *frame, float *center_y, float *x, float *right);

/* ---------- the lobby's reopen */

/* whether a last report is kept to reopen */
int ae_screen_pgcr_has_last(void);
/* opens the last game's report on the stack (player p's, B closes it); 0 when none is kept or the stack refused */
int ae_screen_pgcr_open_last(short owner);

#endif
