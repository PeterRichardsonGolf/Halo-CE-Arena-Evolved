/*
FORGE_AI_ROUTES.H

Forge's AI routes (port/forge/src/forge_ai_routes.c): the ways an AI spawn's
units walk, set as its GOAL's ROUTE (forge_ai.c). A route is a line of points
placed from the Actors tab ("AI Route", two points to begin with), each a
marker-only live instance ('airt', no object) whose team number holds what it
is:

	bits 0-4    its place on the route (0 first)
	bits 5-9    the route's number, 1 to 31: given as the route is made, never
	            changed, none two routes have
	bits 10-11  the route's BEHAVIOR, the same on all its points: PING PONG
	            (back the way it came at either end), STAY AT END (it stops at
	            the last), LOOP (from the last back to the first)
	bits 12-15  the point's WAIT TIME, an index of s_route_waits (0: none)

so the network's spawn (!FH:/!FS:) and usage (!FU:) packets, Forge's
history and its copies carry a point whole. A saved map keeps each point on
an AI_ROUTE| line of its trailer.

A point's Object Settings, titled AI ROUTE #X POINT Y: ROUTE #X SETTINGS > (a
page: the route's NUMBER, its BEHAVIOR, DELETE ROUTE, BACK), WAIT TIME, ROT
SNAP, DROP TO FLOOR, DELETE POINT, then ADD POINT BEFORE and ADD POINT AFTER
side by side on one row (on the line, or on past an end).
*/

#ifndef __FORGE_AI_ROUTES_H
#define __FORGE_AI_ROUTES_H

#include <stdint.h>

/* ---------- constants */

#define FORGE_GROUP_AI_ROUTE 'airt'
#define FORGE_AI_ROUTE_PATH "ai\\route_point"

#define FORGE_AI_ROUTE_MAX 31
/* (the engine walks a squad's first 32 move positions: action_alert.c) */
#define FORGE_AI_ROUTE_MAX_POINTS 32

enum forge_ai_route_behavior
{
	_forge_ai_route_ping_pong = 0,
	_forge_ai_route_stay_at_end,
	_forge_ai_route_loop,
	NUMBER_OF_FORGE_AI_ROUTE_BEHAVIORS
};

#define FORGE_AI_ROUTE_ORDER(usage) (((int)(uint16_t)(usage)) & 0x1F)
#define FORGE_AI_ROUTE_NUMBER(usage) ((((int)(uint16_t)(usage)) >> 5) & 0x1F)
#define FORGE_AI_ROUTE_BEHAVIOR(usage) ((((int)(uint16_t)(usage)) >> 10) & 0x03)
#define FORGE_AI_ROUTE_WAIT_INDEX(usage) ((((int)(uint16_t)(usage)) >> 12) & 0x0F)
#define FORGE_AI_ROUTE_USAGE(number, order, behavior, wait_index) \
	((int16_t)(uint16_t)((((unsigned)(order)) & 0x1Fu) | ((((unsigned)(number)) & 0x1Fu) << 5) | \
		((((unsigned)(behavior)) & 0x03u) << 10) | ((((unsigned)(wait_index)) & 0x0Fu) << 12)))

/* a route point's Object Settings rows: its own, the ROUTE SETTINGS page's,
then those every marker has (forge_3d_mode.c does those) */
enum forge_ai_route_row
{
	_forge_ai_route_row_point = 0,
	_forge_ai_route_row_wait,
	_forge_ai_route_row_add_before,
	_forge_ai_route_row_add_after,
	_forge_ai_route_row_settings,
	_forge_ai_route_row_number,
	_forge_ai_route_row_behavior,
	_forge_ai_route_row_delete_route,
	_forge_ai_route_row_back,
	_forge_ai_route_row_rot_snap,
	_forge_ai_route_row_floor,
	_forge_ai_route_row_delete,
	/* (ADD POINT BEFORE and AFTER, two buttons on one row: left and right
	choose, a press adds) */
	_forge_ai_route_row_add,
	NUMBER_OF_FORGE_AI_ROUTE_ROWS
};

/* what a row's press did (forge_ai_route_row_press) */
enum
{
	FORGE_AI_ROUTE_PRESS_NONE = 0,
	FORGE_AI_ROUTE_PRESS_CHANGED,
	/* (ROUTE SETTINGS opened: the cursor to its first row) */
	FORGE_AI_ROUTE_PRESS_PAGE_OPENED,
	/* (BACK: the cursor to ROUTE SETTINGS) */
	FORGE_AI_ROUTE_PRESS_PAGE_CLOSED,
	/* (the point is gone: the panel closes) */
	FORGE_AI_ROUTE_PRESS_DELETED
};

struct forge_live_instance;

/* ---------- prototypes/FORGE_AI_ROUTES.C */

int forge_ai_route_is(struct forge_live_instance const *inst);
/* a route point made (forge_3d_spawn_object_at): one placed here with no
route (the palette's) is a new route's first point */
void forge_ai_route_spawned(int32_t live_idx, int broadcast_net);
/* a new route's first point placed from the palette: its second added, which
follows it while it is held */
void forge_ai_route_begin(int32_t live_idx);
/* a point deleted here (forge_3d_delete_instance): the route's others
numbered on from 0 again */
void forge_ai_route_point_deleted(int number);
/* a point copied (Forge's duplicate): the copy's usage, the next point along,
room made for it on the route; 0 when the route is full */
int16_t forge_ai_route_copy_usage(int32_t live_idx);
/* a peer's !FU: usage */
void forge_ai_route_usage_apply(int32_t live_idx, int16_t usage);

/* the routes there are, lowest number first: how many (at most `max`) */
int forge_ai_route_numbers(int *numbers, int max);
/* a route's points as live indices, in order: how many (at most `max`) */
int forge_ai_route_points(int number, int32_t *points, int max);
/* a route's BEHAVIOR, and a point's WAIT TIME in seconds */
int forge_ai_route_behavior(int number);
int forge_ai_route_wait_seconds(int16_t usage);
char const *forge_ai_route_behavior_name(int behavior);

/* each frame of Forge (forge_3d_update): a new route's second point after the
first, while it is held */
void forge_ai_routes_update(void);
/* a level going */
void forge_ai_routes_level_reset(void);

/* the routes' lines, in Forge mode (forge_runtime_tags.c's world pass): each
piece by `draw_line`, `color` 0 a route's way, 1 a LOOP's way back, and
`highlighted` for a route one of whose points is held or pointed at */
void forge_ai_routes_render(void (*draw_line)(float const *from, float const *to, int color, int highlighted));

/* Object Settings (forge_3d_mode.c): a point's rows as they are shown (the
ROUTE SETTINGS page's while it is open), how many; their text; and a row
pressed (`dir` -1/1, `enter` its press), FORGE_AI_ROUTE_PRESS_* */
int forge_ai_route_rows(int32_t live_idx, int *rows);
char const *forge_ai_route_panel_title(int32_t live_idx);
void forge_ai_route_row_text(int32_t live_idx, int row, char *label, unsigned long label_size, char *value,
	unsigned long value_size);
int forge_ai_route_row_press(int32_t live_idx, int row, int dir, int enter);
/* the ADD row's button chosen: 0 BEFORE, 1 AFTER */
int forge_ai_route_add_side(void);
void forge_ai_route_add_side_set(int after);
/* the settings page closed (back/Esc): 1 when one was open */
int forge_ai_route_page_back(int32_t live_idx);
/* a point as the view's label calls it ("AI Route #2: Point 3 of 5") */
void forge_ai_route_describe(int32_t live_idx, char *text, unsigned long size);

/* (forge_ai.c) where a point stands on the ground: the walkable ground
under `x, y, z`, else, under a hill or over a drop, the nearest above or
below; 0 for none */
int forge_ai_route_ground(float x, float y, float z, float *ground_z);
/* (forge_ai.c) a new point asked for at `to`, placed from `from`: the ground
nearest `to` back along the way that `from` sees (no wall between, none out
of the map or under it); `from` itself, returning 0, when there is none */
int forge_ai_route_place_toward(float const from[3], float const to[3], float out[3]);

/* the saved map's lines (forge_cache_builder.c): how many bytes */
int forge_ai_routes_save_lines(char *text, unsigned long size);
/* a saved AI_ROUTE| line (forge_runtime_tags.c): the point made */
void forge_ai_routes_load_line(char const *line);

#endif
