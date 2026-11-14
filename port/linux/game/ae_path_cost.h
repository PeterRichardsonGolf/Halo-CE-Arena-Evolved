/*
AE_PATH_COST.H

A vehicle's route prefers open ground (ae_path_cost.c): narrow edges cost
more while a vehicle-width search runs, plus a small per-edge, per-driver noise (adds only: the search stays admissible).
*/

#ifndef __AE_PATH_COST_H
#define __AE_PATH_COST_H

/* arm (radius over a person's: a vehicle search) before path_state_find;
seed picks the driver's own noise (bots_search's AE hook) */
void ae_path_cost_begin(float radius, unsigned long seed);
void ae_path_cost_end(void);

/* an edge's traversal cost, grown when the search is armed and the edge
is narrow (path_state_traverse's AE hook); cost unchanged on foot */
float ae_path_cost_edge(float cost, float edge_length_squared, long from_surface, long to_surface);

/* 0..1: one edge's share of a driver's noise, fixed for the seed and the two surfaces */
float ae_path_cost_noise(unsigned long seed, long from_surface, long to_surface);

/* LEARNT WEDGES (0.9.0, planned driving; owner 2026-11-11, after play-testing: bot driving "still isn't quite there"; box
games on Snow Grove: 86% of the drivers' back-offs were at a few trail spots, the same ones again and again): a walkable
piece where a driver wedged (backed off from something in its way) or crashed (a sudden stop from speed) is noted for
the map; from AE_PATH_WEDGES_TO_AVOID
wedges on it, a vehicle's route pays AE_PATH_WEDGE_PENALTY times an edge into it per wedge, up to AE_PATH_WEDGE_MOST
wedges counted (adds only: the search stays admissible). Pieces kept: AE_PATH_WEDGE_PIECES (the least wedged gives way).
ae_path_cost_wedges_on(0) leaves routes as before (debug.bots_drive_classic). */
#define AE_PATH_WEDGE_PIECES 64
#define AE_PATH_WEDGES_TO_AVOID 2
#define AE_PATH_WEDGE_PENALTY 1.0f
#define AE_PATH_WEDGE_MOST 4
void ae_path_cost_wedge(long surface);
void ae_path_cost_wedges_clear(void);
void ae_path_cost_wedges_on(int on);
/* the wedges noted on a piece (0: none) */
int ae_path_cost_wedges(long surface);

#endif // __AE_PATH_COST_H
