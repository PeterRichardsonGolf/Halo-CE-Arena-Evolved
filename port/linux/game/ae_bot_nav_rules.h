/*
AE_BOT_NAV_RULES.H

Arena Evolved: hand-placed bot navigation (1.0 rc2) for the BTB+ rotation's maps, where drivers ran into rocks. A map may have a nav file, our own data in world
units (port/assets/bot_nav/<file>.txt, built into the game; a file of the same name in the config folder's bot_nav/
folder is read in its place):

	nav 1 <file>
	made <8 hex> [<8 hex> ...]            the map hash prefixes it fits (none: any version of that file)
	link walk|jump|drop X Y Z to X Y Z [oneway]   a way on foot the map's walkable surfaces miss
	avoid X Y Z radius R                   on foot: a place the bots keep out of (as a layout's bot avoid)
	keepout X Y Z radius R                 vehicles: a circle every driver's route pays heavily to cross (rocks, trees)
	route NAME KIND[,KIND...] [loop] [oneway]   KIND: warthog ghost scorpion any
	point X Y Z [speed S] [turn R] [wait W]     S: share of top speed 0.1-1; R: corner radius 0-30; W: seconds 0-10
	end

'#' starts a comment. A route is two-way unless oneway; loop joins its last point to its first. Points of different routes
within AE_BOT_NAV_JUNCTION units are joined both ways (a junction). A bad line, or one past a limit, is refused with its
line number; the rest of the file still loads.

A driver whose goal is far takes the routes (ae_bot_nav_plan): from a point within AE_BOT_NAV_ENTRY of it, along the
routes, to the point nearest its goal, when that brings it at least AE_BOT_NAV_GAIN nearer and the way is no more than
AE_BOT_NAV_DETOUR times the straight line plus AE_BOT_NAV_DETOUR_ADD. A driver whose own search found no way to its goal
(the only way is the long way round: Danger Canyon's two base halls, 24 units apart, are joined for vehicles only by the
canyon, 265 units round) takes them however long the way is (ae_bot_nav_plan_any). Pure
(port/linux/tests/ae_bot_nav_rules_test.c).
*/

#ifndef __AE_BOT_NAV_RULES_H
#define __AE_BOT_NAV_RULES_H

#define AE_BOT_NAV_VERSION 1
#define AE_BOT_NAV_ROUTES 32
#define AE_BOT_NAV_ROUTE_POINTS 128
#define AE_BOT_NAV_POINTS 1024
#define AE_BOT_NAV_KEEPOUTS 64
#define AE_BOT_NAV_LINKS 64
#define AE_BOT_NAV_AVOIDS 32
#define AE_BOT_NAV_MADE 8
#define AE_BOT_NAV_JOINS 4
#define AE_BOT_NAV_ERRORS 8
#define AE_BOT_NAV_NAME 32
#define AE_BOT_NAV_FILE 64

#define AE_BOT_NAV_JUNCTION 4.0f
#define AE_BOT_NAV_ENTRY 15.0f
#define AE_BOT_NAV_GAIN 10.0f
#define AE_BOT_NAV_DETOUR 1.6f
#define AE_BOT_NAV_DETOUR_ADD 10.0f

/* vehicle kinds (bits of a route's KIND list) */
#define AE_BOT_NAV_WARTHOG 1
#define AE_BOT_NAV_GHOST 2
#define AE_BOT_NAV_SCORPION 4
#define AE_BOT_NAV_ANY (AE_BOT_NAV_WARTHOG | AE_BOT_NAV_GHOST | AE_BOT_NAV_SCORPION)

enum
{
	AE_BOT_NAV_LINK_WALK = 0,
	AE_BOT_NAV_LINK_JUMP,
	AE_BOT_NAV_LINK_DROP
};

struct ae_bot_nav_point
{
	float x, y, z;
	float speed, turn, wait;
	short route;
	/* (ae_bot_nav_join) other routes' points within a junction's reach */
	short join_count;
	short joins[AE_BOT_NAV_JOINS];
};

struct ae_bot_nav_route
{
	char name[AE_BOT_NAV_NAME];
	int kinds;
	int loop, oneway;
	int first, count;	/* its points: map->points[first .. first + count - 1] */
	int line;
};

struct ae_bot_nav_circle
{
	float x, y, z, radius;
	int line;
};

struct ae_bot_nav_link
{
	int kind;
	float from[3], to[3];
	int oneway;
	int line;
};

struct ae_bot_nav_error
{
	int line;
	char why[64];
};

struct ae_bot_nav_map
{
	int version;
	char file[AE_BOT_NAV_FILE];
	int made_count;
	unsigned int made[AE_BOT_NAV_MADE];
	int route_count;
	struct ae_bot_nav_route routes[AE_BOT_NAV_ROUTES];
	int point_count;
	struct ae_bot_nav_point points[AE_BOT_NAV_POINTS];
	int keepout_count;
	struct ae_bot_nav_circle keepouts[AE_BOT_NAV_KEEPOUTS];
	int avoid_count;
	struct ae_bot_nav_circle avoids[AE_BOT_NAV_AVOIDS];
	int link_count;
	struct ae_bot_nav_link links[AE_BOT_NAV_LINKS];
	int error_count;	/* every refused line; the first AE_BOT_NAV_ERRORS kept */
	struct ae_bot_nav_error errors[AE_BOT_NAV_ERRORS];
};

/* the file's text into *map (cleared first; junctions joined); returns the refused lines' count. A text with no
"nav 1 <file>" first statement leaves version 0 */
int ae_bot_nav_parse(char const *text, struct ae_bot_nav_map *map);
/* whether the map's file fits a loaded map: its file name (no ".map", no "@source"), and its hash prefix when known */
int ae_bot_nav_fits(struct ae_bot_nav_map const *map, char const *file, int hash_known, unsigned int hash);
/* a vehicle kind's bit from its name (warthog, ghost, scorpion, any), 0 for none */
int ae_bot_nav_kind_of_name(char const *name);
/* the plan from start to goal for a kind's bit: the points' indexes in order, at most `maximum`; 0 for no plan */
int ae_bot_nav_plan(struct ae_bot_nav_map const *map, int kind, float const start[3], float const goal[3], int *plan,
	int maximum);
/* the same; any_length: the way's length is not held against it (the driver's own search found no way to the goal) */
int ae_bot_nav_plan_any(struct ae_bot_nav_map const *map, int kind, float const start[3], float const goal[3], int *plan,
	int maximum, int any_length);
/* the next point of a plan a driver at `at` makes for: the index into plan of the first point after the one whose
segment it is nearest (from `current` on), or `count` once past the last; *off its distance off that segment */
int ae_bot_nav_follow(struct ae_bot_nav_map const *map, int const *plan, int count, int current, float const at[3],
	float reach, float *off);

#endif
