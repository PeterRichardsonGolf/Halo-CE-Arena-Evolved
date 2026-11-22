/*
AE_SPAWNERS.H

port: AE weapon spawners (0.7.8). Every
weapon item of the loaded map (after its layout) becomes a spawner with a class (ae_spawner_rules.h), a tier (the
weapon's, promoted by a long map time) and a kind (rack, pad); two weapons lying together are one spawner; the map's
optional sidecar (ae_spawner_file_rules.h) re-tags them; the game's WEAPON SET (ae_weaponset_rules.h: casual, comp,
btb) gives each (class, tier) its spawn logic (ae_spawner_logic_rules.h), its seconds and its pool
(ae_spawner_pool_rules.h). The host alone spawns (game_engine_update_item_spawn's loop hands a converted entry here);
a guest's timers read the host's state message (128).

The set: the gametype's AECW set id when its AE WEAPONS toggle is on (ae_weapon_selection.h); debug.weapon_set
(HALO_AE_WEAPON_SET: auto|casual|comp|btb|off) and DEV MODE's `weaponset` override it for tests. debug.spawners
(HALO_AE_SPAWNERS): the dev view's chips, the host only.
*/

#ifndef AE_SPAWNERS_H
#define AE_SPAWNERS_H

struct scenario_netgame_equipment;

/* (game_engine_update_item_spawn, before its loop) the spawners' build for this game and the set's state */
void ae_spawners_tick(void);
/* (its loop, for an entry of this game type) TRUE when the entry is a spawner: it was handled here */
boolean ae_spawners_update_entry(long equipment_index, struct scenario_netgame_equipment const *equipment);

/* (item_timers.c) a spawner's timer, the host's: its period, ticks to its next spawn (1..period), and whether its weapon
is on its spot now; FALSE for an entry that is not a spawner (the clock as before) */
boolean ae_spawners_timer_period(long equipment_index, long *period_ticks);
boolean ae_spawners_timer(long equipment_index, long *ticks_left, boolean *on_spot);
/* (ae_power_list.c) whether the spawner shows RED: its weapon is taken and the next waits for it to leave play (the list
says TAKEN instead of a time); FALSE for any other state, and for a POWER pad under a set's `rules reference`; a guest's
from the last message */
boolean ae_spawners_timer_red(long equipment_index);

/* (item_timers_map_begin) a spawner on the power list: 0 not a spawner (the entry as before), -1 a BASE spawner (a rack:
not listed, no waypoint, no callout), 1 a POWER spawner (a pad): listed; one of another class than the power classes
becomes a PAD (_item_timer_pad) named by its weapon. Both the host and a guest (a guest builds the table too) */
int ae_spawners_listing(long equipment_index, short *timer_class, long definition_index, long *class_definitions,
	unsigned short *classes);

/* (hud_item_timers.c) a pad waypoint's alpha: always 1, a pad's waypoint follows the item timers' own rule (shown while
its weapon lies on the map); kept as the hook's one call */
real ae_spawners_waypoint_fade(long equipment_index, real distance);

/* (the ammo walk-over) whether a weapon on the ground refuses the walk-over ammo refill: a spawner's weapon under a set
with `rack_ammo fresh` */
boolean ae_spawners_refuses_ammo(long object_index);

/* (units.c's unit_approve_weapon_swap, rc2) under a set with `rack_ammo fresh`: whether a weapon on the ground is a rack's
fresh one, which a unit holding the same weapon may swap for (the walk-over refill is refused there, so without this the
rack's weapon could be neither taken nor refilled from) */
boolean ae_spawners_fresh_swap(long object_index);

/* (DEV MODE's weaponset) the set from the next spawn: "casual", "comp", "btb", "off" or "auto" (the gametype's);
FALSE for another word */
boolean ae_spawners_set_choose(char const *name, char *text, int size);

/* (DEV MODE's spawner, rc2) the spawner nearest `from` (within 8 world units): "wait" its weapon gone and back in `seconds`,
"red" a POWER weapon taken and in play for `seconds` (then the count restarts), "ready" its next weapon on the next tick; this
machine's game only; *spot (when not NULL) the spawner's spot. FALSE with a text when none is near */
boolean ae_spawners_dev_stage(char const *what, long seconds, real_point3d const *from, real_point3d *spot, char *text,
	int size);

/* the set in play: its id (AE_WEAPONSET_*), -1 off */
int ae_spawners_set_id(void);

/* (hud.c) the dev view's chips over the spawners, debug.spawners on */
void ae_spawners_draw(short local_player_index);

/* (ae_spawner_holo.c, rc2) the spawners as the hologram pass sees them, the host's and a guest's alike: k runs over
ae_spawners_holo_count(); get is FALSE for a spawner this game does not run (off, merged into another, not this game
type's), else its map item, spot and facing (faces_item: whether a spawn faces its weapon that way, as the map's own
item; else the placement's default forward), its state (ae_spawner_msg_rules.h's AE_SPAWNER_MSG_*: WAITING, READY or
RED), whether it is a POWER pad, the ticks to its next weapon and its period, the weapon it makes next (the host's: the
one its pool drew as the count began, definition_next TRUE; a guest's: the pool's first, else the map's own; NONE none),
and the weapon it made while that weapon is its own (the host's view; NONE on a guest and for a POWER pad, whose weapon is
found by its place). accepts: whether the weapon definition is one the spawner makes */
struct ae_spawners_holo
{
	long equipment_index;
	real_point3d position;
	real facing;
	boolean faces_item;
	int state;
	boolean power;
	long ticks_left, period;
	long definition;
	boolean definition_next;
	long object_index;
};
int ae_spawners_holo_count(void);
boolean ae_spawners_holo_get(int k, struct ae_spawners_holo *out);
boolean ae_spawners_holo_accepts(int k, long definition);

/* the state to guests: distributed message 128 (ae_spawner_msg_rules.h), the host's view when a state changes and every
two seconds anyway; a machine new to the game gets it with the round's (network_distributed.c); a guest's item timers
read it (ae_spawners_timer above) */
void ae_spawners_send(long machine_index);
void ae_spawners_net_reset(void);
int ae_spawners_handles_message(unsigned short const *message, unsigned short size);
void ae_spawners_handle_message(long machine_index, unsigned short const *data, unsigned short size);

#endif
