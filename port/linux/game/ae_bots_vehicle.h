/*
AE_BOTS_VEHICLE.H

The bots use vehicles (notes/specs/2026-10-16-bots-infinite.md, area 4), as a
player does: a bot walks to a seat's entrance and presses the action button
(players.c's player_examine_nearby_vehicle offers the seat whose entrance is
nearest; the bot presses only when the one offered is the one it wants), and
presses it again to get out. Local (split screen) games and the LAN host's
bots alike; the vehicles are the gametype's (its vehicle set).

- GUNNER: a free gunner seat (a Warthog's chain gun, a gun turret) near its way,
  or of a vehicle a teammate drives. It fights from it with bots.c's own aim:
  the gunner's facing is the turret's (units.c takes a vehicle's aim and
  trigger from its gunner), and the weapon bots.c reads for it is the
  vehicle's (its bursts, reach and projectile speed).
- PASSENGER: the free passenger seat of a teammate's vehicle (a CTF carrier
  too: it rides home without pulling a trigger).
- Leaving: wrecked, flipped, no driver a while, a vehicle parked a while, near
  its goal or carried away from it (ae_bots_vehicle_rules.h has the numbers).
- DRIVER (debug.bot_vehicles 1: the open maps only; 2: everywhere): an empty
  Warthog or Ghost near its way when its goal is far; it steers for the route's
  points (the walkable surfaces' path, searched from the vehicle), slows for
  turns, backs off when stalled, waits a moment for a teammate bot on its way
  to its gunner seat.
- A rider's position is where it is in the world (bots.c reads an object's
  position, which for a rider is its place in the vehicle).

halo.log: "bots: bot N took the <seat> seat of a <vehicle>", "bots: bot N left
the <vehicle> (<reason>)", each minute "bots: vehicle tick T: ...".
debug.bot_vehicles: 0 never, 1 seats and driving on the open maps (default), 2 seats and driving everywhere.
Included by ae_bots.h, after cseries.h.
*/

#ifndef __AE_BOTS_VEHICLE_H
#define __AE_BOTS_VEHICLE_H

#include "ae_bots_vehicle_rules.h"

/* debug.bot_vehicles (read once a tick): 0 the bots leave vehicles alone, 1 they take seats, 2 and drive */
long ae_bots_vehicle_setting(void);

/* the point in the world of an object, a rider's included (its parent's node matrix); bots_bot_look's hook:
where a riding target is */
void ae_bots_vehicle_world_point(long object_index, real_point3d *point);
/* the same, returned (the objective play's and the fight's reads of where a player is) */
real_point3d ae_bots_vehicle_point(long object_index);
/* the object a bot's line of sight ignores (bots_line_of_sight's hook): the vehicle it rides, its riders with it
(the bot), else the unit (ae_bots_vehicle_sight_ignore_of) */
long ae_bots_vehicle_sight_ignore(long unit_index);
/* the vehicle's kind (its tag's name) and the role of one of its seats (its flags) */
int ae_bots_vehicle_class(long vehicle_index);
int ae_bots_vehicle_seat_role(long vehicle_index, short seat_index);
short ae_bots_vehicle_seat_count(long vehicle_index);

/* ---------- halo.log's counts (ae_bots_vehicle.c) */

void ae_bots_vehicle_count_entry(long bot_index, int vehicle_class, int seat);
void ae_bots_vehicle_count_exit(long bot_index, int vehicle_class, int reason, real seconds);
/* a rider's tick: its seat, its vehicle parked, it fights, the vehicle's speed (units a second) */
void ae_bots_vehicle_count_tick(long bot_index, int seat, boolean parked, boolean fighting, real speed);
void ae_bots_vehicle_count_kill(long bot_index, int seat);
/* each minute's line */
void ae_bots_vehicle_report(void);

/* ---------- bots.c's hooks (ae_bots_vehicle.inc, in bots.c's unit) */

struct bots_bot;
struct bots_tick;
/* (bots_bot_update, after the tick's position) a rider's position in the world */
void ae_bots_vehicle_origin(struct bots_tick *tick);
/* (bots_bot_update, after the tick's weapon) a gunner's weapon is its vehicle's */
void ae_bots_vehicle_weapon(struct bots_bot *bot, struct bots_tick *tick);
/* (bots_choose_goal, first) TRUE while the bot goes for a seat: its goal is the seat's entrance */
boolean ae_bots_vehicle_goal(struct bots_bot *bot, long player_index, real_point3d const *position, boolean fighting);
/* (bots_bot_update, after the objective's walk) a seat looked for and taken, a rider's look, leave and drive */
void ae_bots_vehicle_move(struct bots_bot *bot, long bot_index, struct bots_tick *tick);

/* WHEELMAN ASSISTS (owner 10-25, spike): a kill by a vehicle's gunner or passenger is an assist for its driver (the
Wheelman); game_statistics.c's AE hook after the kill's assists (the attackers, the killer's among them, the damage an
assist needs), not counted twice for a driver who earned one by damage */
struct unit_attacker;
void ae_wheelman_assist(long killer_player_index, short dead_team_index, long statistic_index,
	struct unit_attacker const *attackers, short killer_attacker_index, real assist_damage);

/* the driver's line on this machine, where its kill lines show ("Wheelman: assist on <name>"): game_engine.c's AE hook
after the killer's kill message */
void ae_wheelman_message(long killer_player_index, long dead_player_index);

#endif
