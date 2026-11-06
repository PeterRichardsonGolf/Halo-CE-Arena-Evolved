#ifndef AE_FIRETEAM_H
#define AE_FIRETEAM_H

/*
AE_FIRETEAM.H

Fireteams (ae_fireteam.c, rules in ae_fireteam_rules.c): every bot of a team game with the marks on belongs to one
fireteam, and a person who has spawned leads one. Host only; the table is built from the players in the game.
*/

#include "cseries.h"
#include "math/real_math.h"
#include "ae_fireteam_rules.h"

struct player_datum;

/* the game started (game_initial_pulse): a new table, split by the team rosters */
void ae_fireteam_game_start(void);
/* a player's unit spawned (game_engine_postspawn_player_update): a person leads a fireteam from its first spawn */
void ae_fireteam_player_spawned(long player_index);
/* a player joined or a bot was added or removed (game_engine_player_added): the rosters are split again */
void ae_fireteam_roster_changed(void);
/* a player was deleted (player_delete): its fireteam merges into the team's bot squads, the rosters are split again */
void ae_fireteam_player_left(long player_index);

/* 0.7.6 (tests): the fireteam of a bot's machine (its brain, bots.h), or NONE; and who leads it (a player, or NONE) */
long ae_fireteam_of_brain(long brain);
long ae_fireteam_leader_player(long fireteam);
/* a squad's lead brain (bots.h's index), or -1 */
short ae_fireteam_lead_brain_of(long fireteam);

/* the fireteam's one standing order (a new one replaces the old; point may be NULL: the leader is the point). Host only.
Kinds are the _ae_order_* of ae_fireteam_rules.h */
void ae_fireteam_order(int fireteam, short kind, real_point3d const *point, long object_index);
short ae_fireteam_order_of(long fireteam);
/* the fixed point of a fireteam's order; FALSE with none */
boolean ae_fireteam_order_point_of(long fireteam, real_point3d *point);
/* REGROUP: a bot's distance to the spot; TRUE when that was the last one and the order cleared itself */
boolean ae_fireteam_regroup_report(long brain, real distance);

/* REGROUP: 1 when the brain already arrived */
boolean ae_fireteam_brain_arrived_of(long brain);

/* GOING: a bot's distance to the point and the time in seconds; TRUE when the hold ran out and the order cleared itself */
boolean ae_fireteam_go_report(long brain, real distance, real now_seconds);
/* a leader's mark pulled this bot (a brain) toward a fireteam: AE_RECRUIT_OK when it moved (the fireteam's order, if any, is
now the bot's); the refusals are ae_fireteam_recruit_check's. carrying_objective and seated_driver are the bot's, host side */
int ae_fireteam_recruit_brain(long brain, long fireteam, boolean carrying_objective, boolean seated_driver);
/* the fireteam a player leads, or NONE */
long ae_fireteam_led_by(struct player_datum const *player);

/* ---------- squad roles (ae_bots_fireteam.inc reads the objective and drives these; the rules are ae_fireteam_rules.h's) */
/* fireteam slots in use or free (0 .. count-1), a slot's team (-1: free), and the rebuild count (a new one: roles again) */
long ae_fireteam_slot_count(void);
short ae_fireteam_team_of(long fireteam);
long ae_fireteam_generation(void);
/* a slot's role in the mode, and the role it last took */
int ae_fireteam_role_for(long fireteam, int mode);
int ae_fireteam_role_taken(long fireteam);
/* the squad was wiped out: its next role is the next one */
void ae_fireteam_role_wiped_fireteam(long fireteam);
/* hand the fireteam its role's order when it wants one (see ae_fireteam_role_wants_order); point may be NULL. TRUE when set */
boolean ae_fireteam_role_pass(long fireteam, int role, real_point3d const *point, boolean leader_alive, boolean flipped);
/* whether the fireteam's order is its leader's own call, and what it is made of now */
boolean ae_fireteam_order_explicit_of(long fireteam);
/* the standing order's serial (see ae_fireteam_order_serial) */
long ae_fireteam_order_serial_of(long fireteam);
/* once a tick: a leader moved to the other team (the host moved them, a balance) takes the rosters apart again */
void ae_fireteam_check_leaders(void);

#endif
