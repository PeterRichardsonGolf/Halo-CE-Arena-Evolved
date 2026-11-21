/*
AE_VEHICLE_SIDES.H

Whose vehicle a map's vehicle placement is in a team game (ae_vehicle_sides_rules.h): what the AE hook line in
source/game/game_engine.c (game_engine_vehicle_placement_allowed) calls (port/linux/game/ae_hooks.txt lists it).
Every machine reads the same map and decides the same. Included after cseries.h.
*/

#ifndef __AE_VEHICLE_SIDES_H
#define __AE_VEHICLE_SIDES_H

/* the team (0 red, 1 blue) of the placement at the point: the team of the nearest of the map's CTF starting
locations, else the side given (the port's nearest of any game type) */
short ae_vehicle_sides_team(real_point3d const *point, short side);
/* (game_engine_log_vehicles_placed) the log of a game's vehicles as placed: a team's custom counts asked for, then
each vehicle's tag, side and whether it stands upright */
void ae_vehicle_sides_log_placed(void);
/* whether the placement's team (its side as game_engine_vehicle_placement_allowed reads it) plays a CUSTOM vehicle
set: then the map's spawn flags (a Halo PC or MCC map's default placements for the game type) do not pick its
vehicles, the set's counts do (owner 10-31: AE BTB HEAVIES placed one vehicle a side) */
boolean ae_vehicle_sides_counted(void const *placement);
/* (0.7.9) whether the placement's team plays the PC vehicle set (team_vehicles 1: every placement the map allows) */
boolean ae_vehicle_sides_every(void const *placement);

#endif
