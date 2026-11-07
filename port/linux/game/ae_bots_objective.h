/*
AE_BOTS_OBJECTIVE.H

The bots play the objective modes (notes/specs/2026-10-16-bots-infinite.md,
area 1): CTF, Oddball (and its reverse tag), King of the Hill, Juggernaut
(stock and AE JUGGER's rules), Race and Bomb (AE BOMB, AE GRIFBALL:
the neutral bomb to the enemy goal; melee only, the bots brawl), besides
Slayer, in local (split screen) games and the LAN host's up to three. Each tick a bot weighs
fighting (bots.c's own brain) against its mode's goals (the pure rules:
ae_bots_objective_rules.h); the goal it picks is where bots.c walks it.

- ae_bots_objective.c reads the game's own state once a tick (the CTF flags
  and their carriers, the balls and their holders, the hill and who is in
  it, the race's flags each player may touch next) and writes halo.log's
  lines: "bots: bot N took the flag" / "captured the flag" / "returned the
  flag" / "holds the ball" / "is the juggernaut" / "in the hill" / "touched
  a race flag" / "took the bomb" / "planted the bomb" / "armed the bomb", and each minute "bots: objective tick T (<mode>): ...".
- ae_bots_objective.inc (included by ae_bots_lan.inc, so at the end of
  bots.c, beside the brain's static functions) turns the choice into the
  bot's goal (bots.c's bots_set_goal) and, for a carrier or a bot going to
  the hill, its walk while it fights.

What the AE hook lines call (port/linux/game/ae_hooks.txt lists them).
Included after cseries.h (by ae_bots.h, so bots.c and network_server_manager.c
have it).
*/

#ifndef __AE_BOTS_OBJECTIVE_H
#define __AE_BOTS_OBJECTIVE_H

#include "ae_bots_objective_rules.h"

/* whether bots join and play a game of this engine (game_engine_type): Slayer, CTF, Oddball, King, Race and Bomb (the
server's lobby, network_server_manager.c; the LAN host's, ae_lan_bots.c; the split screen lobby's bot rows) */
boolean ae_bots_engine_played(long game_engine_index);

/* ---------- the game's state, once a tick */

#define AE_BOTS_OBJECTIVE_MAXIMUM_BALLS 16
#define AE_BOTS_OBJECTIVE_MAXIMUM_RACE_FLAGS 32

struct ae_bots_objective_flag
{
	/* a CTF flag of the team (index = its owner team): its stand (the netgame flag), where it is now, at its
	stand untouched or not, who carries it (a player datum index, NONE) */
	boolean exists;
	long weapon_index;
	real_point3d stand;
	real_point3d position;
	boolean home;
	long carrier_player_index;
};

struct ae_bots_objective_snapshot
{
	long tick;
	short mode;
	boolean teams;
	/* CTF: by owner team (0 red, 1 blue); the capture point of a team (its flags[team].stand: the engine scores a
	carrier there) */
	struct ae_bots_objective_flag flags[2];
	/* oddball (stock ball): the balls lying free */
	long ball_count;
	long ball_indices[AE_BOTS_OBJECTIVE_MAXIMUM_BALLS];
	real_point3d ball_positions[AE_BOTS_OBJECTIVE_MAXIMUM_BALLS];
	/* king: the hill (its centre on the ground, as the HUD's marker has it) */
	boolean hill;
	real_point3d hill_centre;
	/* race: the track's flags (their order index, the netgame flag's team index) */
	long race_flag_count;
	long race_flag_orders[AE_BOTS_OBJECTIVE_MAXIMUM_RACE_FLAGS];
	real_point3d race_flag_positions[AE_BOTS_OBJECTIVE_MAXIMUM_RACE_FLAGS];
	/* bomb (game_engine_bomb.c; what its markers show everyone): the bomb (NONE: none), where it is, whether anyone
	may take it now (lying loose, not planted, no goal's countdown), its carrier (a player datum index, NONE), the
	goal it is planted in, arming (the goal's team, NONE), and each team's goal (by team: the other team arms there) */
	long bomb_weapon_index;
	real_point3d bomb_position;
	boolean bomb_takeable;
	long bomb_carrier_player_index;
	long bomb_planted_goal;
	boolean bomb_has_goal[2];
	real_point3d bomb_goals[2];
	/* a melee-only game (Grifball): the bots brawl (ae_bots_combat.inc) */
	boolean bomb_melee_only;
	/* AE's FLAG SPAWNS (ae_bomb_assault.h): the team the bomb is for (the other can't take it); NONE: anyone's */
	long bomb_attacking_team;
};

/* the state of the game now (read once a tick); NULL when no multiplayer game runs */
struct ae_bots_objective_snapshot const *ae_bots_objective_snapshot(void);

/* whether the player holds the objective (the enemy flag, a ball, is "it" or the juggernaut, the bomb) */
boolean ae_bots_objective_player_carries(struct ae_bots_objective_snapshot const *snapshot, long player_index);
/* whether the player is in the hill (king) */
boolean ae_bots_objective_player_in_hill(long player_index);
/* race: whether the player may touch the flag of that order index next */
boolean ae_bots_objective_race_may_touch(long player_index, long order);

/* (ae_bots_objective.inc, each bot's tick) its behaviour this tick, for the logs */
void ae_bots_objective_note(long bot_index, long player_index, short behaviour);
void ae_bots_objective_strategy_note(long team_index, boolean carrier_out, boolean wingman,
	boolean snd_defending, boolean holding);
/* SEARCH AND DESTROY's logs: a bot's tick by side, where it was (0 about its own goal, 1 between, 2 about the enemy's)
and its role; and the map data a role lacked */
enum { AE_BOTS_SND_EVENT_NO_GOAL, AE_BOTS_SND_EVENT_NO_SPOT, AE_BOTS_SND_EVENT_LANE_MADE, AE_BOTS_SND_EVENT_LANE_FAILED };
void ae_bots_objective_snd_note(boolean attacking, int zone, int role, boolean role_changed);
void ae_bots_objective_snd_event(int kind);
/* a carrier deliberately gave up its objective for a gun, or reclaimed that same objective */
void ae_bots_objective_fight_drop_note(long bot_index, boolean picked_up);

/* ---------- bots.c's hooks (ae_bots_objective.inc, in bots.c's unit) */

struct bots_bot;
struct bots_tick;
/* (bots_choose_goal, first) TRUE when the bot's goal is its mode's (bots.c's own choice is skipped) */
boolean ae_bots_objective_goal(struct bots_bot *bot, long player_index, real_point3d const *position, boolean fighting);
/* (bots_bot_update, after its weapons) the objective's walk while it fights (a carrier, the hill), and a carrier
kept from swapping or reloading away its flag or ball */
void ae_bots_objective_move(struct bots_bot *bot, long bot_index, struct bots_tick *tick);

/* ---------- the game types' state (one-line AE hooks in their files, whose state is private there) */

/* game_engine_ctf.c: the team's flag (weapon index, NONE for none), the stand it resets to, and whether it has
been taken from it since; FALSE for a team without a stand */
boolean ae_bots_ctf_flag(long team_index, long *weapon_index, real_point3d *stand, boolean *handled);
/* game_engine_race.c: whether the player may touch the track's flag of that order index now (can_touch_team) */
boolean ae_bots_race_can_touch(long player_index, long order);
/* game_engine_bomb.c: whether the unit stands where it arms the bomb in that team's goal (the engine's own test: a
court's goal volume, else BOMB_GOAL_RADIUS of the goal's marker); FALSE with no Bomb game */
boolean ae_bots_bomb_in_goal(long unit_index, long goal_team);

#endif
