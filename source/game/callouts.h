/*
CALLOUTS.H

port: CALLOUTS (game.callouts, Settings > Game Options): the spoken calls of
the power items' spawns and of the match clock (callouts.c).
*/

#ifndef __CALLOUTS_H
#define __CALLOUTS_H
#pragma once

/* ---------- prototypes/CALLOUTS.C */

void callouts_map_begin(void);	/* reads the voice pack, if CALLOUTS is on; nothing queued */
void callouts_update(void);	/* per tick of a multiplayer game, on every machine */

#endif /* __CALLOUTS_H */
