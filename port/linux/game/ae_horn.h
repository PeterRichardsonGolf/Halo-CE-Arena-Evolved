#ifndef __AE_HORN_H__
#define __AE_HORN_H__

/*
AE_HORN.H

(spike 10-26) VEHICLE HORN (owner): a vehicle's driver honks with MELEE (pad: B or the right stick by layout; key: the
melee key); bot drivers honk at a teammate on foot in their way. A honk is GET IN: teammate bots on foot near the
vehicle go for its open seats, the gunner's first (ae_bots_vehicle.inc). The sound: CMT TSC:E's warthog horn
(released source, credit CMT), a WAV played on this machine's UI voice at a loudness by its distance from the nearest
local player (HALO_HORN_WAV, else horn.wav in the game's folder). Local games only: nothing goes over the network yet.
*/

/* (player_control.c's AE hook, each local player each tick) MELEE held: a driver's press honks */
void ae_horn_local(short local_player_index, boolean melee);
/* a honk of that vehicle (its driver's team asks its riders in) */
void ae_horn_honk(long vehicle_index);
/* the same; here: a driver on this PC honked (heard at full) */
void ae_horn_honk_heard(long vehicle_index, boolean here);
/* the vehicle a teammate of that team honked in the last 3 s, within reach of point (NONE: none) */
long ae_horn_recent(short team_index, real_point3d const *point, real reach);

#endif
