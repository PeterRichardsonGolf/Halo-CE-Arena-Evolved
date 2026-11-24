/*
AE_BOT_NAV.C

Arena Evolved: the bots' hand-placed navigation, the loaded map's file and the drivers on its routes (ae_bot_nav.h).
*/

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cseries.h"
#include "cache/cache_files.h"
#include "../src/port_config.h"
#include "../src/posix.h"

/* (the config folder is a host path: not the game's Xbox-path file calls, port/linux/include/stdio.h) */
#undef fopen
#undef remove
#undef rename

#include "ae_layout.h"
#include "ae_path_cost.h"
#include "ae_bot_nav.h"
#include "ae_bot_nav_rules.h"
#include "ae_map_alias_rules.h"
#include "ae_bots_vehicle_rules.h"

int config_boolean(char const *name);
unsigned long config_changes(void);
void platform_log(char const *format, ...);

#define NAV_FOLDER "bot_nav"
#define NAV_TEXT 131072
#define NAV_DRIVERS 128
#define NAV_PLAN 128
#define NAV_TICKS_PER_SECOND 30
/* a driver's plan is kept while its goal stays within this of the planned one */
#define NAV_GOAL_MOVED 8.0f
/* off its route farther than this: planned again from where it is (rejoined); while it is still joining (it has not yet
come within NAV_JOINED of the route), the distance it joined from, plus NAV_JOIN_SLACK */
#define NAV_OFF_ROUTE 8.0f
#define NAV_JOINED 4.0f
#define NAV_JOIN_SLACK 3.0f
/* a point is passed within this (plus the point's own turn radius) */
#define NAV_REACH 4.0f
/* no drive tick for this long: the driver got out or died; its plan forgotten */
#define NAV_STALE_TICKS (2 * NAV_TICKS_PER_SECOND)
/* a plan tried and none found: tried again after this */
#define NAV_RETRY_TICKS (3 * NAV_TICKS_PER_SECOND)
/* a point's speed holds within this of it, on the way in and out */
#define NAV_SPEED_NEAR 10.0f

static struct
{
	unsigned long changes;
	char level[128];
	boolean loaded;
	char file[AE_BOT_NAV_FILE];
	char const *source;
	boolean have;
	struct ae_bot_nav_map map;
} nav = { 0xFFFFFFFFUL, "", FALSE, "", "none", FALSE };

struct nav_driver
{
	long last_tick;
	int kind;
	int count, current;
	int plan[NAV_PLAN];
	float goal[3];
	boolean done;
	long retry_tick;
	long wait_until;
	int waited;	/* the plan's index whose wait was held */
	boolean joined;	/* once within NAV_JOINED of the route */
	float join_off;	/* how far from its first point it planned */
};
static struct nav_driver nav_drivers[NAV_DRIVERS];

boolean ae_bot_nav_on(void)
{
	static unsigned long changes = 0xFFFFFFFFUL;
	static boolean on = TRUE;

	if (config_changes() != changes)
	{
		changes = config_changes();
		on = (boolean)(config_boolean("bots.nav") != 0);
	}
	return on;
}

/* the level's file name: after its last folder, before its @source and .map */
static void nav_file_of(char const *level, char *file, size_t size)
{
	char const *slash = strrchr(level, '\\'), *other = strrchr(level, '/');
	char *cut;

	if (other > slash)
		slash = other;
	snprintf(file, size, "%s", slash ? slash + 1 : level);
	cut = strchr(file, '@');
	if (cut)
		*cut = 0;
	cut = strrchr(file, '.');
	if (cut && !strcmp(cut, ".map"))
		*cut = 0;
	for (cut = file; *cut; cut++)
	{
		if (*cut >= 'A' && *cut <= 'Z')
			*cut = (char)(*cut - 'A' + 'a');
	}
}

/* the config folder's bot_nav/<file>.txt into a new buffer, or NULL */
static char *nav_read_config_file(char const *file)
{
	char folder[400], path[520];
	FILE *handle;
	char *text;
	size_t length;

	config_folder(folder, sizeof folder);
	if (snprintf(path, sizeof path, "%s/%s/%s.txt", folder, NAV_FOLDER, file) >= (int)sizeof path)
		return NULL;
	handle = fopen(path, "rb");
	if (!handle)
		return NULL;
	text = (char *)malloc(NAV_TEXT + 1);
	if (!text)
	{
		fclose(handle);
		return NULL;
	}
	length = fread(text, 1, NAV_TEXT, handle);
	fclose(handle);
	text[length] = 0;
	return text;
}

void ae_bot_nav_ready(void)
{
	char const *level = cache_file_loaded_map_name();
	char header[160], *config_text = NULL, word[64], alias[64];
	char const *fit_file = nav.file;
	char const *text = NULL;
	unsigned int hash = 0;
	int version, hash_known = 0, index;
	long built;

	if (!level)
		level = "";
	if (nav.loaded && nav.changes == config_changes() && !strcmp(nav.level, level))
		return;
	nav.changes = config_changes();
	snprintf(nav.level, sizeof nav.level, "%s", level);
	nav.loaded = TRUE;
	nav.have = FALSE;
	nav.source = "none";
	memset(nav_drivers, 0, sizeof(nav_drivers));
	nav_file_of(level, nav.file, sizeof nav.file);
	if (!nav.file[0] || !ae_bot_nav_on())
		return;
	/* (the map's hash prefix when the layout code knows the map's file: "layout N <file> <hash>") */
	if (ae_layout_map_header(header, (long)sizeof header) && sscanf(header, "layout %d %63s %x", &version, word, &hash) == 3)
		hash_known = 1;
	config_text = nav_read_config_file(nav.file);
	if (config_text)
	{
		text = config_text;
		nav.source = "config folder";
	}
	else
	{
		for (built = 0; built < ae_bot_nav_file_count; built++)
		{
			if (!strcmp(ae_bot_nav_files[built].file, nav.file))
			{
				text = ae_bot_nav_files[built].text;
				nav.source = "built in";
				break;
			}
		}
	}
	/* (a Workshop map with none under its own file: the file its converted copy had, ae_map_alias_rules.h) */
	if (!text && ae_map_alias_old_file(level, alias, (int)sizeof alias))
	{
		config_text = nav_read_config_file(alias);
		if (config_text)
		{
			text = config_text;
			nav.source = "config folder";
		}
		else
		{
			for (built = 0; built < ae_bot_nav_file_count; built++)
			{
				if (!strcmp(ae_bot_nav_files[built].file, alias))
				{
					text = ae_bot_nav_files[built].text;
					nav.source = "built in";
					break;
				}
			}
		}
		if (text)
		{
			platform_log("bot nav: %s has none: %s's (the converted copy's) is used", nav.file, alias);
			fit_file = alias;
		}
	}
	if (!text)
	{
		nav.source = "none";
		return;
	}
	ae_bot_nav_parse(text, &nav.map);
	for (index = 0; index < nav.map.error_count && index < AE_BOT_NAV_ERRORS; index++)
		platform_log("bot nav: %s: line %d: %s", nav.file, nav.map.errors[index].line, nav.map.errors[index].why);
	if (nav.map.error_count > AE_BOT_NAV_ERRORS)
		platform_log("bot nav: %s: %d more refused lines", nav.file, nav.map.error_count - AE_BOT_NAV_ERRORS);
	if (!ae_bot_nav_fits(&nav.map, fit_file, hash_known, hash))
	{
		platform_log("bot nav: %s (%s): not used: %s", nav.file, nav.source, nav.map.version == 0 ? "no \"nav 1 <file>\" line" :
			strcmp(nav.map.file, fit_file) ? "made for another map" : "made for another version of the map");
		nav.source = "none";
	}
	else
	{
		nav.have = TRUE;
		platform_log("bot nav: %s (%s): %d routes (%d points), %d keepouts, %d links, %d avoids", nav.file, nav.source,
			nav.map.route_count, nav.map.point_count, nav.map.keepout_count, nav.map.link_count, nav.map.avoid_count);
	}
	/* (the game layer's free checks its block's header: never given NULL) */
	if (config_text)
		free(config_text);
}

struct ae_bot_nav_map const *ae_bot_nav_map(void)
{
	ae_bot_nav_ready();
	return nav.have ? &nav.map : NULL;
}

char const *ae_bot_nav_map_file(void)
{
	ae_bot_nav_ready();
	return nav.file;
}

char const *ae_bot_nav_source(void)
{
	ae_bot_nav_ready();
	return nav.source;
}

long ae_bot_nav_add_keepouts(void)
{
	struct ae_bot_nav_map const *map = ae_bot_nav_map();
	long added = 0;
	int index;

	for (index = 0; map && index < map->keepout_count; index++)
	{
		struct ae_bot_nav_circle const *keepout = &map->keepouts[index];

		ae_path_cost_obstacle_add(keepout->x, keepout->y, keepout->z, keepout->radius);
		added++;
	}
	return added;
}

int ae_bot_nav_kind_of_class(int vehicle_class)
{
	switch (vehicle_class)
	{
	case AE_BOTS_VEHICLE_WARTHOG:
	case AE_BOTS_VEHICLE_ROCKET_WARTHOG:
		return AE_BOT_NAV_WARTHOG;
	case AE_BOTS_VEHICLE_GHOST:
		return AE_BOT_NAV_GHOST;
	case AE_BOTS_VEHICLE_SCORPION:
		return AE_BOT_NAV_SCORPION;
	default:
		return 0;
	}
}

static float nav_distance2(float const *a, float const *b)
{
	float dx = a[0] - b[0], dy = a[1] - b[1];

	return (float)sqrt(dx * dx + dy * dy);
}

boolean ae_bot_nav_drive(long bot_index, int vehicle_class, real_point3d const *position, real_point3d const *goal,
	long now, boolean events, boolean search_short, real_point3d *waypoint, real_point3d *after, boolean *have_after,
	real *speed)
{
	struct ae_bot_nav_map const *map = ae_bot_nav_map();
	struct nav_driver *driver;
	struct ae_bot_nav_point const *point;
	float at[3], to[3], off, reach, share = 1.0f;
	int kind = ae_bot_nav_kind_of_class(vehicle_class), next;

	if (!map || map->route_count == 0 || !kind || bot_index < 0 || bot_index >= NAV_DRIVERS)
		return FALSE;
	driver = &nav_drivers[bot_index];
	at[0] = position->x; at[1] = position->y; at[2] = position->z;
	to[0] = goal->x; to[1] = goal->y; to[2] = goal->z;
	/* (a driver new to its seat, another vehicle kind, or a new goal: planned anew) */
	if (driver->last_tick == 0 || now - driver->last_tick > NAV_STALE_TICKS || now < driver->last_tick ||
		driver->kind != kind || nav_distance2(to, driver->goal) > NAV_GOAL_MOVED)
	{
		driver->count = 0;
		driver->done = FALSE;
		driver->retry_tick = 0;
		driver->kind = kind;
		driver->goal[0] = to[0]; driver->goal[1] = to[1]; driver->goal[2] = to[2];
	}
	driver->last_tick = now;
	if (driver->count == 0)
	{
		if (driver->done || now < driver->retry_tick)
			return FALSE;
		driver->count = ae_bot_nav_plan_any(map, kind, at, to, driver->plan, NAV_PLAN, search_short ? 1 : 0);
		driver->current = 0;
		driver->wait_until = 0;
		driver->waited = -1;
		driver->joined = FALSE;
		driver->retry_tick = now + NAV_RETRY_TICKS;
		if (driver->count == 0)
			return FALSE;
		{
			struct ae_bot_nav_point const *first = &map->points[driver->plan[0]];
			float there[3];

			there[0] = first->x; there[1] = first->y; there[2] = first->z;
			driver->join_off = nav_distance2(at, there);
		}
		if (events)
			platform_log("bot nav: bot %ld on route %s (%d points, goal %.0f m)", bot_index + 1,
				map->routes[map->points[driver->plan[0]].route].name, driver->count, (double)nav_distance2(at, to));
	}
	reach = NAV_REACH + map->points[driver->plan[driver->current < driver->count ? driver->current : 0]].turn;
	next = ae_bot_nav_follow(map, driver->plan, driver->count, driver->current, at, reach, &off);
	/* (a point with a wait: held there once) */
	if (next > driver->current && driver->waited != driver->current && map->points[driver->plan[driver->current]].wait > 0.0f)
	{
		driver->waited = driver->current;
		driver->wait_until = now + (long)(map->points[driver->plan[driver->current]].wait * NAV_TICKS_PER_SECOND);
	}
	driver->current = next;
	if (next >= driver->count)
	{
		driver->count = 0;
		driver->done = TRUE;
		if (events)
			platform_log("bot nav: bot %ld leaves the routes (route's end, goal %.0f m)", bot_index + 1,
				(double)nav_distance2(at, to));
		return FALSE;
	}
	/* (joining: off by no more than it planned from; on the route once near it) */
	if (!driver->joined && off <= NAV_JOINED && next > 0)
		driver->joined = TRUE;
	if (off > (driver->joined ? NAV_OFF_ROUTE : MAX(NAV_OFF_ROUTE, driver->join_off + NAV_JOIN_SLACK)))
	{
		driver->count = 0;
		driver->retry_tick = now + NAV_TICKS_PER_SECOND / 2;
		if (events)
			platform_log("bot nav: bot %ld leaves the routes (%.0f m off), plans again", bot_index + 1, (double)off);
		return FALSE;
	}
	point = &map->points[driver->plan[next]];
	waypoint->x = point->x; waypoint->y = point->y; waypoint->z = point->z;
	*have_after = (boolean)(next + 1 < driver->count);
	if (*have_after)
	{
		struct ae_bot_nav_point const *later = &map->points[driver->plan[next + 1]];

		after->x = later->x; after->y = later->y; after->z = later->z;
	}
	/* (the speed: the point ahead's when near it, the point behind's when just past it) */
	{
		float there[3];

		there[0] = point->x; there[1] = point->y; there[2] = point->z;
		if (nav_distance2(at, there) < NAV_SPEED_NEAR && point->speed < share)
			share = point->speed;
		if (next > 0)
		{
			struct ae_bot_nav_point const *before = &map->points[driver->plan[next - 1]];

			there[0] = before->x; there[1] = before->y; there[2] = before->z;
			if (nav_distance2(at, there) < NAV_SPEED_NEAR && before->speed < share)
				share = before->speed;
		}
	}
	if (now < driver->wait_until)
		share = 0.0f;
	*speed = share;
	return TRUE;
}

boolean ae_bot_nav_driver_place(long bot_index, long now, char const **route_name, int *point_index,
	real_point3d *next_point)
{
	struct ae_bot_nav_map const *map = ae_bot_nav_map();
	struct nav_driver const *driver;
	struct ae_bot_nav_point const *point;
	int global;

	if (!map || bot_index < 0 || bot_index >= NAV_DRIVERS)
		return FALSE;
	driver = &nav_drivers[bot_index];
	/* (a plan left a while ago is no longer followed) */
	if (driver->count <= 0 || driver->current >= driver->count || driver->last_tick == 0 ||
		now - driver->last_tick > NAV_STALE_TICKS || now < driver->last_tick)
		return FALSE;
	global = driver->plan[driver->current];
	if (global < 0 || global >= map->point_count)
		return FALSE;
	point = &map->points[global];
	if (point->route < 0 || point->route >= map->route_count)
		return FALSE;
	*route_name = map->routes[point->route].name;
	*point_index = global - map->routes[point->route].first;
	if (next_point)
	{
		next_point->x = point->x;
		next_point->y = point->y;
		next_point->z = point->z;
	}
	return TRUE;
}

boolean ae_bot_nav_dump(char *path, long path_size, char *why, long why_size)
{
	char const *level = cache_file_loaded_map_name();
	char folder[400], made[440], header[160], word[64], hash[16] = "", temporary[540];
	unsigned int value;
	int version;
	long length = 0;
	char *text;
	FILE *handle;
	size_t wrote;

	path[0] = 0;
	ae_bot_nav_ready();
	if (!level || !nav.file[0])
	{
		snprintf(why, (size_t)why_size, "no map loaded");
		return FALSE;
	}
	if (ae_layout_map_header(header, (long)sizeof header) && sscanf(header, "layout %d %63s %x", &version, word, &value) == 3)
		snprintf(hash, sizeof hash, "%08x", value);
	text = ae_bots_nav_dump_text(level, nav.file, hash, &length, why, why_size);
	if (!text)
		return FALSE;
	config_folder(folder, sizeof folder);
	snprintf(made, sizeof made, "%s/bot_nav_geo", folder);
	posix_make_directory(made);
	if (snprintf(path, (size_t)path_size, "%s/%s.json", made, nav.file) >= path_size ||
		snprintf(temporary, sizeof temporary, "%s.new", path) >= (int)sizeof temporary)
	{
		ae_bots_nav_dump_free(text);
		snprintf(why, (size_t)why_size, "path too long");
		return FALSE;
	}
	handle = fopen(temporary, "wb");
	if (!handle)
	{
		ae_bots_nav_dump_free(text);
		snprintf(why, (size_t)why_size, "could not write %s", temporary);
		return FALSE;
	}
	wrote = fwrite(text, 1, (size_t)length, handle);
	fclose(handle);
	ae_bots_nav_dump_free(text);
	if (wrote != (size_t)length || rename(temporary, path) != 0)
	{
		remove(temporary);
		snprintf(why, (size_t)why_size, "could not write %s", path);
		return FALSE;
	}
	platform_log("bot nav: %s: ground dumped to %s (%ld bytes)", nav.file, path, length);
	{
		char note[160];

		snprintf(note, sizeof note, "%s, %ld bytes", why, length);
		snprintf(why, (size_t)why_size, "%s", note);
	}
	return TRUE;
}
