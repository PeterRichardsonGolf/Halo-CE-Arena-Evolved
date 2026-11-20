/*
AE_STRIP_ENGINE.H

The score strip's engine reader: a local player's view of the running
multiplayer game as a struct ae_strip_snapshot (ae_strip_model.h), from what
every machine of a network game has (the game types' network-synced state,
the players and objects), so that a joined player's strip reads as the host's.

ae_strip_engine_debug_log (debug.score_strip_log, HALO_DEBUG_SCORE_STRIP_LOG):
every 5 seconds of game time, one line per local player saying what the strip
says ("score strip: view 0 mode=ctf ..."), for the automated tests
(tools/ae_test/strip_checks.py).

The getters below are one-line AE hooks in the game types' own files
(game_engine_king.c, _oddball.c, _race.c), whose state is private there.
*/

#ifndef __AE_STRIP_ENGINE_H
#define __AE_STRIP_ENGINE_H

#include "ae_strip_model.h"

/* FALSE when no multiplayer game runs, or its game type has no strip
(campaign, co-op, the stub and terminator types), or the view has no player */
boolean ae_strip_engine_snapshot(short local_player_index, struct ae_strip_snapshot *out);
/* each frame (game_engine_update_non_deterministic): the debug log's lines */
void ae_strip_engine_debug_log(void);

/* (0.3.9) Bomb's PLANTING / DEFUSING bar (ae_score_strip.c draws it, HUD STYLE AE): whether this view's player plants
(the carrier in the enemy goal, Chupa's arming) or defuses (a living defender on the planted bomb), and how far, 0..1
(every machine's: the arming from Chupa's state, the defuse from the host's spare byte) */
boolean ae_strip_engine_bomb_progress(short local_player_index, real *fraction, boolean *defusing);

/* (the hooks' getters) */
long ae_strip_king_hill_timer(void);                       /* king: ticks to the hill's move */
boolean ae_strip_king_player_on_hill(long player_index);   /* king: in the hill's hull, as the host scores it */
long ae_strip_oddball_ball_count(long player_index);       /* oddball: balls the player holds */
long ae_strip_race_flags_touched(long player_index);       /* race: this lap's flags touched */
long ae_strip_race_flag_count(void);                       /* race: the map's flags (checkpoints) */
long ae_strip_race_rally_flag(void);                       /* rally: the flag to touch (its team index), or NONE */

#endif /* __AE_STRIP_ENGINE_H */
