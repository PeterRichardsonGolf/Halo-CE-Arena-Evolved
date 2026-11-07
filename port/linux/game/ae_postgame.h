/*
AE_POSTGAME.H

Arena Evolved: a confirm over the carnage report (owner, 2026-10-15). On the
Xbox (and upstream) B or BACK on the carnage report leaves the game at once
(game_engine.c, network_game_abort): straight to the main menu, where the
next B opens "exit Halo?". Here B first asks "LEAVE THE GAME?" over the
carnage report: A leaves as B did, B stays on the carnage report. Only
player 1 controls the carnage report (its pad, or the keyboard: Escape is B,
xinput_sdl.c); the other split-screen players' buttons do nothing there.
The same game_engine.c code serves the PC and the Xbox menus: both get it.

The decisions are ae_postgame_rules.c's (pure; tools/harness test_ae_postgame.py);
the game's side, ae_postgame.c, drawn in the carnage report's own pass.
*/

#ifndef __AE_POSTGAME_H
#define __AE_POSTGAME_H

#include "ae_pure_types.h"
#include <stddef.h> /* (wchar_t) */

/* ---------- the decisions (ae_postgame_rules.c, pure) */

/* what the carnage report's buttons do this frame */
enum ae_postgame_action
{
	/* the stock code runs (A: the host's pick game; nothing pressed) */
	AE_POSTGAME_STOCK = 0,
	/* the confirm's: nothing more this frame */
	AE_POSTGAME_HELD,
	/* leave the game, as the stock B did */
	AE_POSTGAME_LEAVE,
};

struct ae_postgame_confirm
{
	/* the confirm is up */
	boolean open;
	/* a button the confirm took is still down: nothing until both are up */
	boolean wait_release;
};

/* the carnage report's frame: accept_down (A or START held on any pad), back_down (B or BACK held); A before B, as
the stock code reads them */
enum ae_postgame_action ae_postgame_confirm_step(struct ae_postgame_confirm *state, boolean accept_down,
	boolean back_down);

/* ---------- the game's side (ae_postgame.c) */

/* game_engine.c's AE hook in game_engine_update_non_deterministic, the carnage report's buttons: player 1's only
(this machine's first local player), the host's pick game on its A and the confirm on its B; TRUE: done (the stock
code, which took any pad's, is skipped) */
boolean ae_postgame_buttons(void);

union rectangle2d;

/* game_engine.c's AE hook in the carnage report's prompt (bounds: the prompt's, at 70, 410 of its 640 by 480,
offset as it is): TRUE when the confirm is up and drawn in the prompt's place */
boolean ae_postgame_confirm_draw(union rectangle2d const *prompt_bounds);

/* game_engine.c's AE hook in the carnage report's rows: the Place column's text for a player, given the stock
place ("4th"): a player who left before the end shows the game's own quit word (the in-game scoreboard's), so a
quitter ranked last reads as one */
wchar_t const *ae_postgame_place_string(long player_index, wchar_t const *place);

/* the carnage report's number columns right-aligned to the right end of their headers: game_engine.c's AE hooks.
After the header row: its column texts (Score's name, Kills, Assists, Deaths), measured in the report's font against
its tab stops (stops 2 to 5) */
void ae_postgame_number_columns(short const *tab_stops, wchar_t const *score, wchar_t const *kills,
	wchar_t const *assists, wchar_t const *deaths);
/* before a number's line (row: the line, its number after the last tab; stop: its column's tab stop, 2 to 5): the tab
stops set for that line with its stop moved right so the number ends where the header does (never left of the stop) */
void ae_postgame_right_align(short const *tab_stops, short stop, wchar_t const *row);
/* the move right for a number of this width under a header of this width (pure: the unit tests' too) */
short ae_postgame_number_shift(short header_width, short number_width);

/* ---------- (0.4.0) AE's post-game (ae_screen_postgame.c), with AE's menus on */

/* game_engine.c's AE hook at the top of game_engine_post_rasterize_post_game: TRUE when AE's post-game takes the
carnage report's place (AE's menus on and drawn): the game's report is not drawn, AE's screens are (render_ui_widgets,
where they draw in play, is not drawn in the post-game: render_ui_widgets_postgame is) */
boolean ae_postgame_draw(void);

/* a player's best spree this game (kills in one life: game_stats.c, the game list's recorder, as this machine saw the
kills), NONE when not known (a build without the game browser, or a player it does not track) */
long ae_postgame_best_spree(long player_index);

#endif
