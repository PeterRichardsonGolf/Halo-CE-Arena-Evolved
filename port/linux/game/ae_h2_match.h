#ifndef AE_H2_MATCH_H
#define AE_H2_MATCH_H

/*
AE_H2_MATCH.H

The match, player and equipment rows of AE's extra gametype settings in play (ae_variant_ext_rules.h): rounds, round
time, resolve ties, max active players, the respawn time modifier, spawning with the overshield, the weapon respawn,
camo and overshields on the map, and slayer's point rules. What the AE hook lines in source/game/game_engine.c call
(port/linux/game/ae_hooks.txt lists them). The host decides all of it; every function is a no-op while the settings
are the default.
*/

#include "cseries.h"

/* each tick of a running game (game_engine_update) */
void ae_h2_match_update(void);
/* game_engine_should_end_game: the end the engine found, as the rounds' and the ties' rules have it */
boolean ae_h2_match_filter_end(boolean should_end);
/* game_engine_end_game: TRUE when the game goes on (another round starts) */
boolean ae_h2_match_end_game(void);
/* an end the host decided (END GAME, RESTART GAME, an admin, a vote, a script): the whole game, not a round; the call for it
(game_engine_end_game_final, declared in game_engine.h) */
/* game_engine_should_spawn_player: TRUE keeps the player waiting (MAX ACTIVE PLAYERS) */
boolean ae_h2_match_holds_spawn(long player_index);
/* game_engine_player_killed: the respawn wait after the respawn time modifier */
long ae_h2_match_respawn_timer(long dead_player_index, long timer);
/* game_engine_player_killed, once the engine has counted the kill: slayer's death, suicide and bonus points */
void ae_h2_match_player_killed(long killing_player_index, long dead_player_index, boolean friendly_fire);
/* game_engine_remap_equipment: TRUE when this powerup (equipment_powerup_type) does not appear on the map */
boolean ae_h2_match_powerup_banned(short powerup_type);
/* game_engine_item_respawn_period: a map item's respawn period in ticks, as WEAPON RESPAWN has it */
long ae_h2_match_item_respawn_period(long period);
/* game_engine_update_item_spawn: TRUE on the tick a new round puts the map's items back */
boolean ae_h2_match_items_reset_now(void);

/* the HUD's round line (ae_hud_style_draw.c): TRUE in a rounds game on the host: the round now, and in a team game
the viewer's team's round wins and the best other team's (NONE in a free for all) */
boolean ae_h2_match_round_line(long viewer_team, long *round, long *mine, long *theirs);

/* game_engine_player_killed, on the host: TRUE once for a player a new round's reset killed (nobody's kill, nothing
counted, no feed line, no suicide wait: as a team change's death) */
boolean ae_h2_match_quiet_death(long dead_player_index);

/* the pause menu's RESTART ROUND (the host's): whether a rounds game (NUMBER OF ROUNDS above 1) is running here */
boolean ae_h2_match_rounds_active(void);
/* the round starts again as it began (everyone killed, the round's scores and the map's items back; the round number and the
wins kept): TRUE when it did */
boolean ae_h2_match_restart_round(void);

/* a round's reset, for the round modes' machine (ae_round.c): everyone killed with no death counted, the engine's
scores started again, the map's items and vehicles back (unless ROUNDS RESET MAP is off) */
void ae_h2_match_reset_round(void);

/* deaths the player had when the round began (0 outside rounds play) */
long ae_h2_match_round_deaths_base(long player_index);

#endif
