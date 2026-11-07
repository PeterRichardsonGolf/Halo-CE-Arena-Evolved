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

#endif // __AE_PATH_COST_H
