/*
AE_POSTGAME.H

Arena Evolved: a confirm over the carnage report (owner, 2026-10-15). On the
Xbox (and upstream) B or BACK on the carnage report leaves the game at once
(game_engine.c, network_game_abort): straight to the main menu, where the
next B opens "exit Halo?". Here B first asks "LEAVE THE GAME?" over the
carnage report: A leaves as B did, B stays on the carnage report. Any pad
and the keyboard (Escape is B, xinput_sdl.c), as the stock code reads them.
The same game_engine.c code serves the PC and the Xbox menus: both get it.

The decisions are ae_postgame_rules.c's (pure; tools/harness test_ae_postgame.py);
the game's side, ae_postgame.c, drawn in the carnage report's own pass.
*/

#ifndef __AE_POSTGAME_H
#define __AE_POSTGAME_H

#include "ae_pure_types.h"

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

/* game_engine.c's AE hook in game_engine_update_non_deterministic, the carnage report's buttons: TRUE when the
confirm took them (the stock A and B code is skipped) */
boolean ae_postgame_buttons(boolean accept_down, boolean back_down);

union rectangle2d;

/* game_engine.c's AE hook in the carnage report's prompt (bounds: the prompt's, at 70, 410 of its 640 by 480,
offset as it is): TRUE when the confirm is up and drawn in the prompt's place */
boolean ae_postgame_confirm_draw(union rectangle2d const *prompt_bounds);

#endif
