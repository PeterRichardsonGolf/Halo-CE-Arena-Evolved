/*
AE_GRENADES.H

Arena Evolved's grenade layer, the game's side: the catalogue's types (ae_grenade_catalogue.h) on a map, beside frag
and plasma, in mcc_grenades.c's side table.

- THE MASK. The game's grenade mask (bit n: catalogue id n) is the gametype's, held in the START bytes' high bits
  (ae_grenade_catalogue.h), or the test override game.grenade_types. 0, every gametype from before: stock frag and
  plasma, and nothing here runs (the wire byte for byte as before).
- THE MAP. As a multiplayer map's tags load for a game whose mask is not 0, the grenade pack (packs\ae_grenades.aepack,
  tags only) is added after the weapon packs (ae_weapon_pack.c), and the map's globals get a grown grenade block: the
  map's own entries, then one per catalogue id in the slot map, its tags found by name in the pack. The map's block
  is put back as the tags are let go. Every machine makes the same slot map from the map and the mask. An id that has
  no slot or no tags is refused for the map, one line; the game plays without it.
- THE LAYER. On while a catalogue id has a slot on the loaded map: mcc_grenades_active() is then TRUE on a Custom
  Edition map too, and the side table holds the catalogue's counts.
- EFFECTS. A catalogue grenade's detonation (projectile_detonate's hook) runs, on the host or a local game, one area
  and line-of-sight query and hands the targets to the row's effect modules (ae_grenade_fx_*.c). Their state (a
  vehicle's stall, a player's scramble) is kept per full object handle here, and sent to the clients in AE's grenade
  message (ae_grenades_network.c), so every machine stalls the vehicle in its own update.
*/

#ifndef __AE_GRENADES_H
#define __AE_GRENADES_H

#include "cseries.h"
#include "math/real_math.h"

/* ---- the mask and the map */

/* the game's grenade mask (the running or pending game's: its gametype's grenade byte; without the weapon cache, the
host's test override). 0: stock */
unsigned int ae_grenades_mask(void);
/* whether the layer is on (a catalogue id has a slot on the loaded map) */
boolean ae_grenades_layer_on(void);
/* the map's own grenade types (its globals' block before the catalogue's: 2 on a Custom Edition map) */
short ae_grenades_own_count(void);
/* the catalogue id in a grenade slot (0-9), NONE for frag, plasma, a map's own type or an empty slot */
short ae_grenades_slot_id(short slot);
/* the slot of a catalogue id on the map, NONE when it has none */
short ae_grenades_slot_of(short id);
/* whether a catalogue id plays on the loaded map: named by the mask, its tags on the map, and a slot */
boolean ae_grenades_type_on_map(short id);
/* the tag index of a catalogue id's tag of a group ('proj', 'eqip', 'itmc', 'grhi', 'effe'): NONE when not on the
map */
long ae_grenades_tag(short id, unsigned long group);

/* (ae_weapon_pack.c) whether the game about to be played on the map whose tags just loaded asks for the grenade
pack: a multiplayer map and a mask not 0 */
boolean ae_grenades_pack_asked(long scenario_tag_index);
/* (ae_weapon_pack.c) every pack added: the slot map and the grown globals block (pack_added: the grenade pack is on
the map) */
void ae_grenades_map_tags_added(char const *map_name, long scenario_tag_index, boolean pack_added);
/* (ae_weapon_pack.c) before the packs are let go: the map's own globals block back */
void ae_grenades_map_tags_let_go(void);
/* (ae_glue_ce_maps.c) whether an address range is the grown block's memory */
int ae_grenades_memory(unsigned long address, long size);

/* (mcc_grenades.c's throw) a projectile thrown from a grenade slot */
void ae_grenades_thrown(long projectile_index, short slot);
/* DEV MODE's give: a unit's grenades of a catalogue id (count -1: the row's maximum); the count it has, or NONE when
the id is not on the map */
short ae_grenades_give(long unit_index, short id, short count);
/* the mask a game's options hold (ae_grenades.c: THE MASK'S PLACE) */
struct game_variant_options;
unsigned int ae_grenades_mask_of(struct game_variant_options const *options);

/* (the host's network game's options, as they are made: network_game_server_variant_options) the test override
game.grenade_types written into the gametype's grenade byte (under the weapon cache), so it travels to the clients */
void ae_grenades_host_options(struct game_variant_options *options);
/* (network_test.c) the automated test host's gametype's grenade mask (debug.network_test_grenade_types) */
void ae_grenades_network_test_options(struct game_variant_options *options, unsigned int mask);
/* whether a game's mask travels with its variant (a build with the weapon cache's grenade byte); without, the only
grenade games are the test override, which every machine of the test sets */
boolean ae_grenades_mask_travels(void);
/* (units_initialize_for_new_map) a map started again: the effects, the throws and the test aids cleared */
void ae_grenades_new_map(void);

/* ---- the join */

/* the feature mark's bits this build tells (ae_silent_rules.h; ae_grenade_join_bits): the grenade layer's version and
its pack's hash nibble, only when the grenade pack was read and taken (none without it: such a machine is refused
from a game with grenade types, code 68) */
unsigned int ae_grenades_join_feature_bits(void);

/* ---- effects */

/* (projectile_detonate's hook) a projectile went off at the point */
void ae_grenade_detonated(long projectile_index, real_point3d const *point);
/* (game_engine_update's hook) each tick of a running game: the test aids, the ends' logs */
void ae_grenades_update(void);

/* a vehicle's stall's ticks left (0: not stalled), and whether it is stalled now */
long ae_grenades_stall_ticks_left(long vehicle_index);
boolean ae_grenades_vehicle_stalled(long vehicle_index);
/* a unit's scramble's ticks left, and the ticks since it was scrambled (NONE: not scrambled) */
long ae_grenades_scramble_ticks_left(long unit_index);
long ae_grenades_scramble_age(long unit_index);

/* (unit_update's hook, after the driver's and gunner's controls are taken) a stalled vehicle: no throttle, no
jump or crouch, and its driver's gun locked (a gunner on another seat keeps firing) */
void ae_grenades_stall_controls(long unit_index);
/* (vehicle_update's hook, after its brake flag is set) a stalled vehicle brakes: its speed seeks 0 */
void ae_grenades_stall_brake(long vehicle_index);
/* (the flying vehicles' physics' hooks) a stalled flyer's lift scale (0.35), 1 otherwise */
real ae_grenades_stall_lift_scale(long vehicle_index);
/* (the host's vehicle prediction's hook) whether a client's driver prediction of the vehicle is ignored: it is
stalled */
boolean ae_grenades_prediction_ignored(long vehicle_index);

/* ---- the input */

/* (player_control's hook) the local player's grenade choice after the NEXT GRENADE / PREVIOUS GRENADE keys
(controls.grenade_next, controls.grenade_previous): a fresh press steps the choice +1 / -1 over the types carried */
short ae_grenades_cycle_input(short local_player_index, long unit_index, short desired_grenade_index);

/* ---- the state the network keeps (ae_grenades_network.c) */

/* set a vehicle's stall or a unit's scramble to end `ticks` from now (0: none), as the host said */
void ae_grenades_state_set_stall(long object_index, long ticks);
void ae_grenades_state_set_scramble(long object_index, long ticks);
/* the host: whether an object's state changed since it was last sent, and noted sent */
boolean ae_grenades_state_dirty(long object_index);
void ae_grenades_state_sent(long object_index);
/* the host: each object that has a state or had one (iterator: start at 0; NONE at the end) */
long ae_grenades_state_next(long *cursor);
/* a new game: every state cleared */
void ae_grenades_state_reset(void);

void ae_grenades_network_new_game(void);
void ae_grenades_network_host_tick(void);
void ae_grenades_network_client_tick(void);
word ae_grenades_network_entry_size(void);
void ae_grenades_network_handle(void const *entries, short count);

#endif
