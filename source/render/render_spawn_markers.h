/*
RENDER_SPAWN_MARKERS.H

port: the gametype's TRAINING's spawn markers (render_spawn_markers.c).
*/

#ifndef __RENDER_SPAWN_MARKERS_H
#define __RENDER_SPAWN_MARKERS_H
#pragma once

/* ---------- constants */

/* as Halo 1: NHE's floor markers: GREEN for every spawn, 0.1 world units
over the floor, drawn within 25 world units and fading out over the last 10 */
#define SPAWN_MARKER_HEIGHT 0.1f
#define SPAWN_MARKER_RANGE 25.0f
#define SPAWN_MARKER_FADE 10.0f

/* ---------- prototypes/RENDER_SPAWN_MARKERS.C */

void render_spawn_markers(short local_player_index);	/* in render_window's world pass, before the HUD */
void render_spawn_markers_initialize_for_new_map(void);	/* the log's per-view state, from render_initialize_for_new_map */

#endif // __RENDER_SPAWN_MARKERS_H
