/*
AE_JUGGERNAUT.H

JUGGERNAUT (the rules: ae_juggernaut_rules.h): what the AE hook lines in
source/game/game_engine_oddball.c and game_engine.c call
(port/linux/game/ae_hooks.txt lists them). Included after cseries.h.
*/

#ifndef __AE_JUGGERNAUT_H
#define __AE_JUGGERNAUT_H

#include "ae_juggernaut_rules.h"

/* the gametype's byte: bit 2 of universal_variant's pad5 (ae_juggernaut_rules.h) */
#define AE_JUGGERNAUT_VARIANT_BYTE(variant) ((variant)->universal_variant.pad5)

/* the oddball engine's state the rules read and write, given once a map
(game_engine_oddball.c's oddball_globals.current_ball_owner, its host's
count of the balls passed and to whom (oddball_events, which a network client
shows), and its oddball_add_score) */
void ae_juggernaut_attach(long *ball_owners, unsigned char *transfers, unsigned char *transfer_player,
	void (*add_score)(long player_index));

/* TRUE in a Juggernaut game (the oddball engine, its terminator ball) whose gametype has AE JUGGER's rules */
boolean ae_juggernaut_active(void);

/* a player killed (the oddball engine's player_killed_player): TRUE when the
rules took it (the score, the statistics and the role's succession), so the
stock handler does nothing */
boolean ae_juggernaut_player_killed(long killing_player_index, long dead_player_index, boolean friendly_fire);

/* each tick (the oddball engine's update): on the host, a ball nobody carries
(the game's start, the juggernaut quit, no one alive to take it before) goes
to a random player who is alive */
void ae_juggernaut_update(void);

/* whether the player is the juggernaut now, in a game with AE JUGGER's rules (ae_ds5_game.c) */
boolean ae_juggernaut_player_is(long player_index);

/* the rules' name for the game's "game rules:" line: "halo 2" while active, else "halo ce" */
char const *ae_juggernaut_log_name(void);

#endif /* __AE_JUGGERNAUT_H */
