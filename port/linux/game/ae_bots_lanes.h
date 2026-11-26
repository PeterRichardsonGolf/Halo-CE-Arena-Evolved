/*
AE_BOTS_LANES.H

The engine's side of the bots' side lanes (ae_bots_lanes_rules.h has the rules): the map's walkable surfaces as a graph,
read a little each tick as the bots' first ticks of a game run (nothing at the map's load), the two bases, the direct
route and the side lanes found, the settings bots.routes and bots.vantage. ae_bots_vantage.c reads the graph for its
spots and lets it go. From halo-ce-vita (GPL-3.0) via Halo Content Evolved (Watanoge/halo-content-evolved,
AGPL-3.0-or-later).

- bots.routes "auto" (the default): a mini squad's LEFT and RIGHT routes walk the map's side lanes where the builder found
  them, else the offset points of before; "offset": the offset points only.
- bots.vantage (true): bots with a precision or power weapon hold the map's best overlooks (ae_bots_vantage.c); false
  plays as before.
*/

#ifndef __AE_BOTS_LANES_H
#define __AE_BOTS_LANES_H

#include "ae_bots_lanes_rules.h"

/* the walkable graph while it is read: a node a walkable surface (the lanes rules' node and the surface's own numbers) */
struct ae_bots_graph
{
	long node_count;
	struct ae_lanes_node *nodes;
	float *area;
	float *normal_k;
	long *surface_index;
};

/* a new game's map (bots.c's bots_prepare_map): all forgotten, the graph's memory let go */
void ae_bots_lanes_reset(void);
/* once a tick, whatever the bots do: the settings read, the map's graph and lanes made a bounded step; TRUE once they are
made (or the map has none) */
boolean ae_bots_lanes_update(void);
boolean ae_bots_lanes_ready(void);
/* the settings, read once a second */
boolean ae_bots_lanes_auto(void);
boolean ae_bots_vantage_setting(void);
/* (0.9.0) bots.high_ground: free bots with any weapon hold the vantage spots too (ae_bots_vantage_rules.h) */
boolean ae_bots_high_ground_setting(void);
/* the map's lanes: how many are found, a lane (NULL for none) and its waypoint's surface; the direct route (NULL for none) */
long ae_bots_lanes_count(void);
struct ae_lanes_lane const *ae_bots_lanes_lane(int lane);
long ae_bots_lanes_waypoint_surface(int lane, int waypoint);
struct ae_lanes_route const *ae_bots_lanes_route(void);
boolean ae_bots_lanes_has_bases(void);
void ae_bots_lanes_bases(real_point3d *first, real_point3d *second);
/* a point's walkable ground: its surface (NONE for none) and the point there */
long ae_bots_lanes_ground(real_point3d const *point, real_point3d *ground);
/* the graph for ae_bots_vantage.c's spots: NULL until it is made and reached from the first base; and it let go once the
spots are made */
struct ae_bots_graph const *ae_bots_lanes_graph(void);
void ae_bots_lanes_release_graph(void);

#endif
