/*
AE_ROUND.H

The round modes in play (ae_round_rules.h: the gametype's mode ELIMINATION on the Slayer engine): the host's round
machine (ae_round.c), the danger zone (ae_round_zone.c), the round state's message and the debug line
(ae_round_net.c). The host decides everything; its clients read the state from message 106 to freeze the countdown,
count lives and follow a teammate. Every function is a no-op (or the engine's own answer) unless the mode runs, so other
gametypes play as before. What the AE hook lines in the game's sources call (port/linux/game/ae_hooks.txt lists them).

Only Arena Evolved machines play a round mode: a host refuses another at the join, as TERRITORIES does (ae_silent.c).
*/
#ifndef AE_ROUND_H
#define AE_ROUND_H

#include "cseries.h"

struct player_starting_location;

/* a game of a round mode is running (host, client or offline) */
boolean ae_round_on(void);

/* (the host's, each tick of a running game: ae_h2_match_update) */
void ae_round_update(void);
/* game_engine_should_end_game (ae_h2_match_filter_end): the engine's end, as the round machine has it */
boolean ae_round_filter_end(boolean should_end);
/* game_engine_end_game (ae_h2_match_end_game): TRUE when the game goes on (the end was not the host's, nor the
match's) */
boolean ae_round_end_game(boolean final_end);

/* game_engine_should_spawn_player (ae_h2_match_holds_spawn): TRUE keeps the player from spawning (out of the round,
the round over, a late joiner's wait) */
boolean ae_round_holds_spawn(long player_index);
/* find_best_starting_location_index: whether the round machine picks the player's spawn (a team game on a map with
both teams' start spawns), and its pick (a starting location's index, NONE for none) */
boolean ae_round_spawns_own(long player_index);
long ae_round_choose_spawn(long player_index, short location_count);
/* player_spawn: a revived player's spawn at their spot (written into location), else NULL */
struct player_starting_location const *ae_round_revive_location(long player_index,
	struct player_starting_location *location);
/* slayer's kills: TRUE when they score nothing (the score is the round wins) */
boolean ae_round_scores_own(void);

/* players.c, each player's action this tick: the action button held (the weapon swap's flag) */
void ae_round_note_action(long player_index, boolean held);
/* player_handle_weapon_swap: TRUE when the hold is a revive's (no weapon taken from it) */
boolean ae_round_pickup_blocked(long player_index);

/* the frozen countdown before each round (game_engine_pregame_countdown_ticks_left, _covers): its ticks left now (0
none), and whether a tick is one of the current round's countdown */
long ae_round_countdown_ticks_left(void);
boolean ae_round_countdown_covers(long time);
/* the deaths a player brought into the round (ae_h2_match_round_deaths_base): TRUE with *base when the mode runs */
boolean ae_round_deaths_base(long player_index, long *base);
/* the follow camera: spawns are held until the next round (a dead player follows a teammate) */
boolean ae_round_holds_spawns(void);
/* whether the danger zone is what killed this player (their death is the zone's: the feed says ZONE, not the falling
damage the kill is dealt with) */
boolean ae_round_zone_killed(long player_index);

/* the round state's message (106): a client's copy cleared (a new connection), the host's to a machine that is ready,
and the message from the host */
void ae_round_net_reset(void);
void ae_round_send(long machine_index);
int ae_round_handles_message(unsigned short const *message, unsigned short size);
void ae_round_handle_message(long machine_index, unsigned short const *message, unsigned short size);

/* DEV MODE's one text line while a round mode runs (round, alive counts, zone, the downed): a stand-in until the HUD */
void ae_round_debug_draw(short local_player_index);

/* DEV MODE's round commands (ae_dev_cmd.c, the host's): round end|next|live, zone now|small|off, down <player> [here],
revive <player>. TRUE when it ran; the answer's text either way */
boolean ae_round_dev_round(char const *what, char *answer, int size);
boolean ae_round_dev_zone(char const *what, char *answer, int size);
boolean ae_round_dev_down(char const *who, boolean here, char *answer, int size);
boolean ae_round_dev_revive(char const *who, char *answer, int size);
/* the dev kill of a player of the live round not spawned yet (a TEST RUN's setup right after the countdown's end): they
die as they spawn. TRUE when it is so */
boolean ae_round_dev_kill_unspawned(long player_index);
/* the dev reset (a TEST RUN between its tests): the match starts again at round 1 */
void ae_round_dev_restart(void);

/* the pause menu's RESTART ROUND (the host's): the round that is under way starts again as it began (a new countdown, everyone
back at the start, the map's items back; the round number and the wins kept). TRUE when it did; not between rounds or over */
boolean ae_round_restart(void);

/* the HUD's round line (ae_h2_match_round_line's): TRUE once the round state is known (the host's own, or a client's from
message 106): the round now and, with teams, the viewer's team's wins and the best other team's (NONE without teams) */
boolean ae_round_hud_line(long viewer_team, long *round, long *mine, long *theirs);

/* the HUD STYLE AE score strip's rounds view (ae_strip_engine.c): the round, the round wins to win, and each side's
players and those up now (ALIVE, REVIVED, or in the round and not spawned yet; a DOWN or OUT player's cell is dark; a
joiner who waits for the next round is not counted). With teams the viewer's team is "me"; without, the viewer is
"me" (one player) and "other" is everyone else, with the best other's wins. TRUE once the round state is known */
struct ae_round_strip
{
	boolean teams;
	long round, max_rounds, to_win;      /* to_win 0: every round is played */
	long wins_me, wins_other;
	long players_me, alive_me, players_other, alive_other;
};
boolean ae_round_strip_state(long viewer_player_index, long viewer_team, struct ae_round_strip *out);

/* the HUD's respawn text (game_engine_get_state_message): a dead player is out until the round ends (DOWN, OUT, or
waiting for the next round), so the text says so ("You are out of lives", as SEARCH AND DESTROY's dead read) and not the
stock "Rejoin in N" with a counter that does not run */
boolean ae_round_player_out(long player_index);
/* the scoreboard's status of a player this round: 0 none, 1 DOWN (dead with a revive spot: a teammate may bring them back),
2 OUT (dead until the round ends) */
long ae_round_player_scoreboard_state(long player_index);

/* the bots (the host's): a downed teammate's spot to revive within reach of a bot's travel, and where a bot goes to be
inside the zone (outside it, or outside the next ring with the closing near) */
boolean ae_round_bot_revive_spot(long player_index, real_point3d const *position, real_point3d *spot);
/* the same, and whether the downed player it names is a human (a bot hurries to a person) */
boolean ae_round_bot_revive_spot_for(long player_index, real_point3d const *position, real_point3d *spot, boolean *human);
boolean ae_round_bot_zone_goal(long player_index, real_point3d const *position, real_point3d *goal);
/* TRUE when this bot is outside the danger zone's ring, or within AE_ZONE_LEASH_MARGIN of the edge while it closes: a bot
in that state drops its fight and goes back in (the fight must not chase an enemy out) */
boolean ae_round_bot_zone_leash(long player_index, real_point3d const *position);

#endif
