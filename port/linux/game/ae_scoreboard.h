/*
AE_SCOREBOARD.H

(0.4.0) HUD STYLE AE's in-game scoreboard (2026-10-24, the approved layout A).
The full-screen view's scoreboard (held BACK / Tab, and after the game) in the AE menus' look: the gametype and the
map over a rule; a team game's teams stacked (red over blue), each under a bar with its score; FFA one list with
places (1ST, 2ND...) and each player's armour colour; columns K D A PING (SCORE first off Slayer: the engine's own
score text); your row in the menus' focus style; a player out of lives or gone reads DEAD or QUIT.
Big games: up to 16 players rows of 40 px at 1080; more, compact rows (28 px); 25-40 players in teams, the two teams
side by side; past what fits, each list shows a window that the wheel / Page Up/Down scroll, the team bars stay, and
your row is pinned at the foot of its list while it is scrolled out of view; FFA opens at your place with the top
three pinned above.
Split screen: each view its own board, compact, at the viewer's place. Classic HUD style, co-op and the mouse's player picking (network vote kick, mute, kick, ban)
keep game_engine.c's scoreboard.
*/

#ifndef __AE_SCOREBOARD_H
#define __AE_SCOREBOARD_H

#include "ae_pure_types.h"

/* game_engine.c's statistic_buffer, field for field (its size is checked there: 0x1C) */
struct ae_scoreboard_stat
{
	long player_index;
	long score;
	long custom;
	long kills;
	long deaths;
	long assists;
	long place;
};

/* AE's scoreboard draws this frame's view: HUD STYLE AE, a game engine running, a full-screen view */
boolean ae_scoreboard_on(void);

/* (interface.c) the HUD is not drawn under AE's scoreboard once it has faded in (it would show through its title) */
boolean ae_scoreboard_hides_hud(short local_player_index);

/* (game_engine.c) draws it for the view's player from the ranked players; scroll: the rows scrolled (NONE: just
opened, at the viewer's place), notches/pages: the wheel's and Page Up/Down since the last frame; returns the new
scroll */
long ae_scoreboard_draw(long player_index, real alpha, const struct ae_scoreboard_stat *ranked, long count,
	long scroll, long notches, long pages);

/* (game_engine.c) a split-screen view's: the same board in the view, its rows compact, opened at the viewer's place */
long ae_scoreboard_draw_split(long player_index, real alpha, const struct ae_scoreboard_stat *ranked, long count);

/* (0.4.0) the post-game's CARNAGE (ae_postgame.c) draws the same board with these; zeroed: the in-game board */
struct ae_scoreboard_options
{
	boolean full_screen;     /* one board over the whole screen, split screen too */
	boolean all_local_me;    /* every player of this machine's row in the "you" style */
	boolean no_title;        /* no gametype title, map line and rule: the lists from lists_top */
	boolean best_spree;      /* a BEST SPREE column before PING (ae_postgame_best_spree) */
	boolean fit_all;         /* rows as big as lets every player in: full, compact, then (teams) side by side */
	real lists_top;          /* (no_title) the first list's top, in pixels at 1080 */
	real width;              /* the stacked or FFA list's width at 1080 (0: the game's 900) */
	real scrim_alpha;        /* the scrim over the game (0: the game's) */
	/* > 0: drawn in AE's menus' layer (ae_draw.h, at Present) in its layout of this size (units, 1080 tall), not the
	HUD's pass */
	real layer_width, layer_height;
	/* out: the lists' foot, in pixels at 1080 from the screen's top; whether the teams were side by side */
	real bottom;
	boolean side;
};
long ae_scoreboard_draw_with(long player_index, real alpha, const struct ae_scoreboard_stat *ranked, long count,
	long scroll, long notches, long pages, struct ae_scoreboard_options *options);
/* the board's left and width (pixels of a view_w x view_h view) for a list width at 1080 (0: the game's), side by side
or not, as ae_scoreboard_draw_with places it (the post-game's banner sits on it) */
void ae_scoreboard_postgame_place(real view_w, real view_h, boolean side, real width, real *x, real *w);

/* (game_engine.c's) the name the scoreboard shows a player by */
const wchar_t *ae_game_engine_scoreboard_name(long player_index, wchar_t *buffer, long size);

#endif /* __AE_SCOREBOARD_H */
