/*
AE_BOT_NAV.H

Arena Evolved: the bots' hand-placed navigation (ae_bot_nav_rules.h has the file and the rules). The loaded map's nav
file is port/assets/bot_nav/<file>.txt as built into the game (ae_bot_nav_data.c, tools/bot_nav_assets.py), or the
config folder's bot_nav/<file>.txt in its place (a route under test). <file> is the map's file name: the level
name without its @source and .map (bloodgulch for bloodgulch@mcc). Setting bots.nav (on); off: no file is used and the
bots drive and walk as before. A map without a file plays as before.
- vehicles: a far goal is driven by the routes (ae_bot_nav_drive), keepouts are obstacles every driver's route pays to
  cross (ae_path_cost.h SCENERY OBSTACLES);
- on foot: the file's links are links of ae_bots_travel.h's (taken when they beat the walk), its avoids close the bots'
  walkable surfaces there.
halo.log: "bot nav: <file>: N routes (N points), N keepouts, N links, N avoids" once a map, "bot nav: <file>: line N:
<why>" for a refused line; with debug.bot_events "bot nav: bot N on route <name> ..." and "bot nav: bot N leaves the
routes (<why>)".
*/

#ifndef __AE_BOT_NAV_H
#define __AE_BOT_NAV_H

#include "cseries.h"
#include "math/real_math.h"
#include "ae_bot_nav_rules.h"

/* one built-in nav file (ae_bot_nav_data.c, generated) */
struct ae_bot_nav_file
{
	char const *file;
	char const *text;
};
extern struct ae_bot_nav_file const ae_bot_nav_files[];
extern long const ae_bot_nav_file_count;

/* bots.nav */
boolean ae_bot_nav_on(void);
/* (a map or game made ready: ae_bots_travel_prepare, ae_bots_vehicle_reset) the loaded map's file read, once a map */
void ae_bot_nav_ready(void);
/* the loaded map's nav, NULL for none (or bots.nav off) */
struct ae_bot_nav_map const *ae_bot_nav_map(void);
/* the loaded map's file name (no @source, no .map), "" for none */
char const *ae_bot_nav_map_file(void);
/* where the loaded map's file came from: "built in", "config folder" or "none" */
char const *ae_bot_nav_source(void);

/* the route kind of a vehicle class (ae_bots_vehicle_rules.h), 0 for one no route takes */
int ae_bot_nav_kind_of_class(int vehicle_class);
/* (the drive tick) a driver with a far goal on the routes: the point it makes for, the one after (have_after), and the
share of its top speed there; FALSE: no route (the search's path as before). search_short: the driver's own search found
no way to the goal, so a route plan is taken however long its way (ae_bot_nav_plan_any) */
boolean ae_bot_nav_drive(long bot_index, int vehicle_class, real_point3d const *position, real_point3d const *goal,
	long now, boolean events, boolean search_short, real_point3d *waypoint, real_point3d *after, boolean *have_after,
	real *speed);
/* (ae_bots_nav_dump.inc) the loaded map's ground as JSON (spec: tools/halo_mcp/nav.py), NULL with *why; given back by
ae_bots_nav_dump_free */
char *ae_bots_nav_dump_text(char const *level, char const *file, char const *hash, long *length, char *why, long why_size);
void ae_bots_nav_dump_free(char *text);
/* DEV "nav dump": the dump written to the config folder's bot_nav_geo/<file>.json (local only: the map's own geometry,
never in git or a release); FALSE with *why */
boolean ae_bot_nav_dump(char *path, long path_size, char *why, long why_size);
/* (ae_bots_obstacles_collect) the loaded map's keepouts added as obstacles; their count */
long ae_bot_nav_add_keepouts(void);
/* the route a driver follows now and its next point's index on that route (0 = the route's first); FALSE when it is on
none (the recorder's and the overlay's) */
boolean ae_bot_nav_driver_place(long bot_index, long now, char const **route_name, int *point_index,
	real_point3d *next_point);

#endif
