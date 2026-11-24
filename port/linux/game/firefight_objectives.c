/*
FIREFIGHT_OBJECTIVES.C

port: Firefight's optional objectives (docs/firefight_objectives.md), beside
the waves, never in their way: a wave's clear waits for none of their units.

- A WAVE OBJECTIVE, rolled as each wave starts (game.firefight_objectives_round_chance):
  so many kills of a kind, no deaths, shields kept, the marked enemy killed
  first, or a weapon class (or a grenade) never fired. The whole team's: every
  player counted from its start (one joining later is not, till the next
  wave), and a rule broken is told to all, with who broke it.
- A ROUND OBJECTIVE, rolled once a round (game.firefight_objectives_set_chance), starting at
  its first or second wave and lasting until the round ends: an enemy camp
  round a map objective point (its VIP to kill, its generator to destroy, or
  every one of them to kill), a KOTH hill to hold (Data Link), a flag to
  carry to the other base, marines to rescue, or a boss's skull to hold.

Which may play is the host's config.toml (game.firefight_objectives_round,
game.firefight_objectives_set and each objective's own), the same on every
map; one whose map data the map has not got (a hill, two flags, an oddball
spawn) is left out there, and the game says so as it starts. What a map has
is kept (d:\firefight_objective_maps.txt) for Server Setup to show.

(In the code and the settings a wave objective is a `round` one and a round
objective a `set` one: they were first a round's and a set's.)

Points go to the players by how much each did; a round objective won drops a
pod to the better half of them, as an ordnance drop does, and gives the best
of them more. The host decides it all; its clients are told the view
(firefight_objectives_view) in Firefight's state, and each shows it.

The round time is a round's waves times the wave's time limit (the NEXT WAVE
timer, 60 seconds when the waves are not timed): a hold (Data Link, Oddball)
and the marines' patience (Rescue) are a quarter of it.
*/

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "cseries/cseries.h"
#include "cseries/errors.h"
#include "ai/actors.h"
#include "ai/actor_definitions.h"
#include "ai/actor_types.h"
#include "ai/ai.h"
#include "game/game.h"
#include "game/game_engine.h"
#include "game/game_globals.h"
#include "game/game_engine_king.h"
#include "rasterizer/rasterizer_model_types.h"
#include "game/players.h"
#include "items/weapon_definitions.h"
#include "items/weapons.h"
#include "math/real_math.h"
#include "memory/data.h"
#include "objects/objects.h"
#include "scenario/scenario.h"
#include "scenario/scenario_definitions.h"
#include "tag_files/tag_groups.h"
#include "units/units.h"

#include "halo_firefight.h"
#include "ae_firefight_glue.h"
#include "halo_port_limits.h"
#include "halo_skulls.h"

/* ---------- externals */

extern struct game_variant global_variant;
boolean network_game_distributed_client(void);
int config_boolean(const char *name);
long config_integer(const char *name);
long get_flag_definition_index(void);
long get_ball_definition_index(void);
unsigned long system_milliseconds(void);
boolean tag_index_is_group(long tag_index, long group_tag);

/* ---------- constants */

enum
{
	/* the points a round objective and a set objective give, shared out by
	what each player did; and the set objective's best player's more */
	FFO_ROUND_POINTS = 1000,
	/* (the enemy generator's health, of the variant's GENERATOR HEALTH) */
	FFO_GENERATOR_HEALTH_PERCENT = 35,
	/* (Use X only's pods' time to land and be picked up: its rule holds after) */
	FFO_USE_ONLY_GRACE_TICKS = 12 * TICKS_PER_SECOND,
	FFO_SET_POINTS = 3000,
	FFO_TOP_PLAYER_POINTS = 1000,
	/* (Rescue's, each marine alive at the set's end) */
	FFO_MARINE_POINTS = 250,

	FFO_RESULT_TICKS = 6 * TICKS_PER_SECOND,
	FFO_NOTICE_TICKS = 5 * TICKS_PER_SECOND,
	FFO_MAP_NOTICE_TICKS = 10 * TICKS_PER_SECOND,

	FFO_MAX_SITES = 48,
	FFO_MAX_HILLS = 16,
	FFO_HILL_POINTS = 16,
	FFO_MAX_FLAGS = 16,
	FFO_MAX_UNITS = 24,
	FFO_MAX_VEHICLES = 3,
	FFO_MAX_MARINES = 4,
	FFO_PLAYER_WORDS = (HALO_PORT_MAXIMUM_NETWORK_PLAYERS + 31) / 32,

	/* (a unit's orders, so often: staggered over them) */
	FFO_ORDER_TICKS = TICKS_PER_SECOND,
	FFO_TELEPORT_TICKS = 45,
	/* (a flag or a ball left lying this long goes back) */
	FFO_ITEM_RETURN_TICKS = 30 * TICKS_PER_SECOND,
	FFO_BALL_RETURN_TICKS = 20 * TICKS_PER_SECOND,
	/* (Oddball's more enemies while the ball is held, so often) */
	FFO_ODDBALL_SPAWN_TICKS = 20 * TICKS_PER_SECOND,
	/* (a capture's carry worth this much more) */
	FFO_CAPTURE_BONUS_TICKS = 10 * TICKS_PER_SECOND,
	/* (Collect's skulls on the ground: so many at once, lying so long) */
	FFO_MAX_TOKENS = 48,
	FFO_TOKEN_TICKS = 30 * TICKS_PER_SECOND,
	/* (Bomb's: enemies at it unopposed this long disarm it; more of them so
	often while it is planted) */
	FFO_DISARM_TICKS = 5 * TICKS_PER_SECOND,
	FFO_BOMB_RAID_TICKS = 15 * TICKS_PER_SECOND,
	/* (the marks looked for so often; Use X only's weapon dropped again for
	a player with none of it about, no oftener than this) */
	FFO_MARKS_TICKS = 10,
	FFO_REDROP_TICKS = 20 * TICKS_PER_SECOND,
	/* (Collect's deposit's fire, so often) */
	FFO_FIRE_TICKS = 12
};

/* a camp's size (world units), how near a player must come to the marines,
and a flag's capture */
#define FFO_CAMP_RADIUS 18.0f
/* (Purge's base: bigger, with more of them and the map's vehicle spots in it) */
#define FFO_PURGE_RADIUS 30.0f
/* (Collect's deposit's skull, so many times its size) */
#define FFO_DEPOSIT_SCALE 5.0f
/* (a camp's ring on the ground, so many points: firefight_objectives_render) */
#define FFO_RING_POINTS 48
#define FFO_RESCUE_ARRIVE 14.0f
#define FFO_CAPTURE_RADIUS 3.0f
#define FFO_HILL_HEIGHT 3.5f
/* (Travel's point reached; a skull taken; the bomb planted, disarmed; its
blast) */
#define FFO_TRAVEL_RADIUS 3.5f
#define FFO_TOKEN_REACH 1.4f
#define FFO_BOMB_RADIUS 3.5f
#define FFO_DISARM_RADIUS 3.0f
#define FFO_BLAST_RADIUS 10.0f
/* (two objectives' points no nearer than this: Travel's and a round
objective's, so their markers never sit on each other) */
#define FFO_POINT_APART 8.0f
/* (a camp's units take no notice of a player farther than this past its
radius, nor while none of them was hurt lately: they roam it) */
#define FFO_CAMP_NOTICE_MARGIN 8.0f

/* a unit of an objective's: what it does */
enum ffo_role
{
	_ffo_role_defender = 0,
	_ffo_role_target,
	_ffo_role_attacker,
	_ffo_role_marine,
	_ffo_role_boss,
	_ffo_role_raider
};

/* where an objective can be */
enum ffo_site_kind
{
	_ffo_site_flag = 0,
	_ffo_site_hill,
	_ffo_site_oddball,
	_ffo_site_other
};

struct ffo_site
{
	real_point3d point;
	short kind;
	short index;
};

struct ffo_hill
{
	real_point2d points[FFO_HILL_POINTS];
	/* (each marker's height, for its outline: firefight_objectives_render) */
	real point_z[FFO_HILL_POINTS];
	short point_count;
	real_point3d center;
	real radius;
};

/* ---------- globals */

static struct firefight_objectives_view g_view;

static struct
{
	/* the map's: its objective points, hills, flags and oddball spawns, and
	the set objectives it can play */
	struct ffo_site sites[FFO_MAX_SITES];
	short site_count;
	struct ffo_hill hills[FFO_MAX_HILLS];
	short hill_count;
	real_point3d flags[FFO_MAX_FLAGS];
	short flag_teams[FFO_MAX_FLAGS];
	short flag_count;
	real_point3d balls[FFO_MAX_FLAGS];
	short ball_count;
	unsigned long set_available;

	/* the round objective's: its players (absolute indices), those whose
	shields broke, what each did, its target */
	unsigned long counted[FFO_PLAYER_WORDS];
	short counted_count;
	unsigned long broken[FFO_PLAYER_WORDS];
	long round_contribution[HALO_PORT_MAXIMUM_NETWORK_PLAYERS];
	long round_target_unit;
	/* (Vehicle kills' vehicle type, Use X only's weapon, and whether its rule
	holds yet: from the first wave, once the pods are down) */
	long round_vehicle_tag;
	long round_weapon_tag;
	boolean round_armed;
	/* (Use X only's rule from then: its pods down; and when its weapon may
	be dropped again, for a player with none) */
	long round_arm_time;
	long round_redrop_time;
	long marks_next;
	/* (each player's unit and its shield, last tick) */
	long shield_unit[HALO_PORT_MAXIMUM_NETWORK_PLAYERS];
	real shield_last[HALO_PORT_MAXIMUM_NETWORK_PLAYERS];

	/* the set objective's: the round it starts in (0: none this set) */
	/* (the wave of the round its round objective starts at, 0 for none) */
	short set_start_round;
	long set_contribution[HALO_PORT_MAXIMUM_NETWORK_PLAYERS];
	long set_deadline;
	long units[FFO_MAX_UNITS];
	byte unit_roles[FFO_MAX_UNITS];
	long unit_next_order[FFO_MAX_UNITS];
	short unit_count;
	long vehicles[FFO_MAX_VEHICLES];
	/* (each one's players' vehicle slot whose spot it took, NONE: given back
	once it is gone, firefight_objective_release_vehicle_slot) */
	short vehicle_slots[FFO_MAX_VEHICLES];
	short vehicle_count;
	/* (the VIP, the boss, the generator's target unit) */
	long target_unit;
	/* (the generator's looks: the host's, and each machine's own) */
	long generator_scenery;
	long hill_index;
	/* (the flag or the ball, where it goes back to, and since when it lies) */
	long item_object;
	real_point3d item_home;
	long item_still_since;
	long last_raid_tick;
	/* (Collect's skulls on the ground: each one's object, worth and time) */
	long tokens[FFO_MAX_TOKENS];
	short token_values[FFO_MAX_TOKENS];
	long token_times[FFO_MAX_TOKENS];
	/* (and whether a player dropped it, dying: marked) */
	boolean token_dropped[FFO_MAX_TOKENS];
	short token_count;
	/* (its deposit's skull, never taken) */
	long deposit_skull;
	/* (Bomb's: where it waits, the one planted, how long enemies have been at it) */
	real_point3d bomb_home;
	long bomb_planted;
	long disarm_ticks;
	/* (Rescue's: its marines, and the attackers made) */
	long marines[FFO_MAX_MARINES];
	short marine_count;
	short attackers_spawned;
	/* (the camp leaving: its units cloaked till then, then gone) */
	long teleport_until;
	/* (the camp's units noticing the players: one near, or one of them hurt
	lately, till then) */
	boolean camp_aware;
	long camp_alarm_until;

	unsigned long rng;
} g;

/* the client's own generator looks */
static long g_local_generator_scenery = NONE;

/* ---------- names */

static char const *const ffo_set_names[NUMBER_OF_FF_SET_OBJECTIVES] =
{
	"", "HIGH VALUE TARGET", "ENEMY GENERATOR", "CLEAR ENEMY BASE", "DATA LINK", "FLAG", "RESCUE", "ODDBALL",
	"COLLECT", "BOMB"
};

static char const *const ffo_round_names[NUMBER_OF_FF_ROUND_OBJECTIVES] =
{
	"", "KILLS", "NO DEATHS", "SHIELDS INTACT", "KILL THE MARKED ENEMY FIRST", "DON'T USE", "VEHICLE KILLS",
	"TRAVEL", "USE ONLY"
};

static char const *const ffo_round_settings[NUMBER_OF_FF_ROUND_OBJECTIVES] =
{
	NULL, "game.firefight_objective_kills", "game.firefight_objective_no_deaths",
	"game.firefight_objective_shields", "game.firefight_objective_kill_first", "game.firefight_objective_dont_use",
	"game.firefight_objective_vehicle_kills", "game.firefight_objective_travel", "game.firefight_objective_weapon_only"
};

static char const *const ffo_set_settings[NUMBER_OF_FF_SET_OBJECTIVES] =
{
	NULL, "game.firefight_objective_target", "game.firefight_objective_generator", "game.firefight_objective_purge",
	"game.firefight_objective_data_link", "game.firefight_objective_flag", "game.firefight_objective_rescue",
	"game.firefight_objective_oddball", "game.firefight_objective_collect", "game.firefight_objective_bomb"
};

static char const *const ffo_kill_names[NUMBER_OF_FF_OBJECTIVE_KILLS] =
{
	"HEADSHOT", "MELEE", "GRENADE", "SPLATTER",
	"AUTOMATIC WEAPON", "SEMI-AUTOMATIC WEAPON", "SHOTGUN", "PLASMA WEAPON", "ROCKET"
};

static char const *const ffo_class_names[NUMBER_OF_FF_OBJECTIVE_WEAPON_CLASSES] =
{
	"AUTOMATIC WEAPONS", "SEMI-AUTOMATIC WEAPONS", "SHOTGUNS", "PLASMA WEAPONS", "ROCKETS"
};

char const *firefight_set_objective_name(short kind)
{
	return kind > 0 && kind < NUMBER_OF_FF_SET_OBJECTIVES ? ffo_set_names[kind] : "";
}

/* ---------- small helpers */

static unsigned long ffo_rand(void)
{
	g.rng = g.rng * 1664525UL + 1013904223UL;
	return (g.rng >> 8) & 0xFFFFFFUL;
}

static real ffo_rand_range(real low, real high)
{
	return low + (high - low) * (real)(ffo_rand() % 10000UL) / 10000.0f;
}

static boolean ffo_host(void)
{
	return firefight_active() && !network_game_distributed_client();
}

static boolean ffo_bit(unsigned long const *bits, long index)
{
	return index >= 0 && index < HALO_PORT_MAXIMUM_NETWORK_PLAYERS && (bits[index >> 5] & (1UL << (index & 31))) != 0;
}

static void ffo_bit_set(unsigned long *bits, long index)
{
	if (index >= 0 && index < HALO_PORT_MAXIMUM_NETWORK_PLAYERS)
		bits[index >> 5] |= 1UL << (index & 31);
}

static long ffo_absolute(long player_index)
{
	return player_index == NONE ? NONE : (long)DATUM_INDEX_TO_ABSOLUTE_INDEX(player_index);
}

static struct unit_datum *ffo_living_unit(long unit_index)
{
	struct unit_datum *unit = unit_index != NONE ?
		(struct unit_datum *)object_try_and_get_and_verify_type(unit_index, _object_mask_unit) : NULL;

	return unit && !TEST_FLAG(unit->object.damage_flags, _object_dead_bit) ? unit : NULL;
}

/* a player's name, in capitals, for the HUD */
static void ffo_player_name(long player_index, char *name, size_t size)
{
	wchar_t wide[64];
	size_t index;

	wide[0] = 0;
	if (player_index != NONE)
		campaign_score_player_display_name(player_index, wide, NUMBEROF(wide));
	for (index = 0; index + 1 < size && wide[index]; index++)
		name[index] = wide[index] < 128 ? (char)toupper((int)wide[index]) : '?';
	name[index] = 0;
	if (!name[0])
		snprintf(name, size, "A PLAYER");
}

/* (the game's first notice, which objectives this map cannot play, is kept
up its time: one meanwhile waits for it) */
static long g_notice_sticky_until = NONE;
static char g_notice_waiting[FF_OBJECTIVE_NOTICE_LENGTH + 2];
static short g_notice_waiting_ticks;

/* a tag's name for the HUD: its last part, in capitals, with no "_mp" */
static void ffo_tag_label(long tag_index, char *label, size_t size)
{
	char const *name = tag_index != NONE ? tag_get_name(tag_index) : NULL;
	char const *slash = name ? strrchr(name, '\\') : NULL;
	size_t index;
	char *cut;

	name = slash ? slash + 1 : name ? name : "";
	for (index = 0; index + 1 < size && name[index]; index++)
		label[index] = name[index] == '_' ? ' ' : (char)toupper((unsigned char)name[index]);
	label[index] = 0;
	if ((cut = strstr(label, " MP")) != NULL && !cut[3])
		*cut = 0;
}

static void ffo_notice(char const *text, short ticks)
{
	error(_error_silent, "firefight objectives: %s", text);
	if (g_notice_sticky_until != NONE && game_time_get() < g_notice_sticky_until)
	{
		snprintf(g_notice_waiting, sizeof(g_notice_waiting), "%s", text);
		g_notice_waiting_ticks = ticks;
		return;
	}
	snprintf(g_view.notice, sizeof(g_view.notice), "%s", text);
	g_view.notice_ticks = ticks;
}

static void ffo_notice_update(void)
{
	if (g_notice_sticky_until == NONE || game_time_get() < g_notice_sticky_until)
		return;
	g_notice_sticky_until = NONE;
	if (g_notice_waiting_ticks > 0)
	{
		snprintf(g_view.notice, sizeof(g_view.notice), "%s", g_notice_waiting);
		g_view.notice_ticks = g_notice_waiting_ticks;
		g_notice_waiting_ticks = 0;
	}
}

/* the round objective's time: the round's waves times the wave's limit, the
NEXT WAVE timer's (60 seconds when the waves are not timed) */
static long ffo_set_ticks(void)
{
	struct firefight_options const *opts = firefight_options_get();
	long wave = 60 * TICKS_PER_SECOND;

	if (opts)
	{
		switch (opts->firefight_next_wave_mode)
		{
		case _firefight_next_wave_timer_30s:  wave = 30 * TICKS_PER_SECOND; break;
		case _firefight_next_wave_timer_45s:  wave = 45 * TICKS_PER_SECOND; break;
		case _firefight_next_wave_timer_90s:  wave = 90 * TICKS_PER_SECOND; break;
		case _firefight_next_wave_timer_120s: wave = 120 * TICKS_PER_SECOND; break;
		default: break;
		}
	}
	return FIREFIGHT_WAVES_PER_ROUND * wave;
}

/* the object of an absolute index on this machine (a client's are the
host's, at the same index: firefight_network.c tells them by it) */
static long ffo_object_of_absolute(short absolute_index, unsigned long mask)
{
	static long cached = NONE;
	struct object_iterator iterator;
	struct object_datum *object;

	if (absolute_index == NONE || absolute_index < 0)
		return NONE;
	if (cached != NONE && DATUM_INDEX_TO_ABSOLUTE_INDEX(cached) == absolute_index &&
		object_try_and_get_and_verify_type(cached, mask))
	{
		return cached;
	}
	object_iterator_new(&iterator, mask, 0);
	while ((object = (struct object_datum *)object_iterator_next(&iterator)) != NULL)
	{
		if (DATUM_INDEX_TO_ABSOLUTE_INDEX(iterator.index) == absolute_index)
			return cached = iterator.index;
	}
	return NONE;
}

long firefight_objectives_object(short absolute_index)
{
	return ffo_object_of_absolute(absolute_index, _object_mask_all);
}

static short ffo_absolute_of_object(long object_index)
{
	return object_index == NONE ? NONE : (short)DATUM_INDEX_TO_ABSOLUTE_INDEX(object_index);
}

/* the living players (their datum indices, units and places), at most `maximum` */
static short ffo_living_players(long *players, long *units, real_point3d *positions, short maximum)
{
	struct data_iterator iterator;
	struct player_datum *player;
	short count = 0;

	if (!player_data)
		return 0;
	data_iterator_new(&iterator, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL && count < maximum)
	{
		if (player->quit_out_of_game || !ffo_living_unit(player->unit_index))
			continue;
		if (players)
			players[count] = iterator.datum_index;
		if (units)
			units[count] = player->unit_index;
		if (positions)
			object_get_origin(player->unit_index, &positions[count]);
		count++;
	}
	return count;
}

static real ffo_nearest_player_distance(real_point3d const *point, long *nearest_unit)
{
	real_point3d positions[16];
	long units[16];
	short count = ffo_living_players(NULL, units, positions, 16);
	real best = 1.0e9f;
	short index;

	if (nearest_unit)
		*nearest_unit = NONE;
	for (index = 0; index < count; index++)
	{
		real distance = distance3d(point, &positions[index]);

		if (distance < best)
		{
			best = distance;
			if (nearest_unit)
				*nearest_unit = units[index];
		}
	}
	return best;
}

/* a spot on the ground about a point, `inner` to `outer` from it, this side
of the structure from it; FALSE (the point itself) for none found */
static boolean ffo_spot_near(real_point3d const *center, real inner, real outer, real_point3d *spot)
{
	short tries;

	for (tries = 0; tries < 12; tries++)
	{
		real angle = ffo_rand_range(0.0f, 2.0f * _pi);
		real distance = ffo_rand_range(inner, outer);
		real_point3d candidate = *center;
		real_point3d ground;

		candidate.x += cosine(angle) * distance;
		candidate.y += sine(angle) * distance;
		candidate.z += 1.0f;
		if (firefight_objective_ground(&candidate, &ground) != NONE &&
			fabs(ground.z - center->z) < 6.0f && firefight_objective_reachable(center, &ground))
		{
			*spot = ground;
			return TRUE;
		}
	}
	*spot = *center;
	return FALSE;
}

/* a camp's spot: on the ground `inner` to `outer` from its middle, on
ground walked to from it (the same walkable group: through a base's doors,
round its walls, which a straight line from a flag inside it is not), not
over a drop; FALSE (the middle itself) for none found */
static boolean ffo_camp_spot(real_point3d const *center, real inner, real outer, real_point3d *spot)
{
	long group = firefight_objective_reach_group(center);
	short tries;

	for (tries = 0; tries < 32; tries++)
	{
		real angle = ffo_rand_range(0.0f, 2.0f * _pi);
		real distance = ffo_rand_range(inner, MAX(inner, outer));
		static real const heights[] = { 1.0f, 4.0f, 8.0f };
		short height;

		for (height = 0; height < (short)NUMBEROF(heights); height++)
		{
			real_point3d candidate = *center;
			real_point3d ground;
			long candidate_group;

			candidate.x += cosine(angle) * distance;
			candidate.y += sine(angle) * distance;
			candidate.z += heights[height];
			if (firefight_objective_ground(&candidate, &ground) == NONE || fabs(ground.z - center->z) > 10.0f)
				continue;
			candidate_group = firefight_objective_reach_group(&ground);
			if (group != NONE && candidate_group != NONE ? candidate_group == group :
				firefight_objective_reachable(center, &ground))
			{
				*spot = ground;
				return TRUE;
			}
			break;
		}
	}
	*spot = *center;
	return FALSE;
}

/* an actor's unit sent to a point on the ground */
static void ffo_order_move(long unit_index, real_point3d const *point)
{
	struct unit_datum *unit = ffo_living_unit(unit_index);
	real_point3d ground;
	long surface;

	if (!unit || unit->unit.actor_index == NONE || !actor_try_and_get(unit->unit.actor_index))
		return;
	surface = firefight_objective_ground(point, &ground);
	actor_move_to_point(unit->unit.actor_index, surface != NONE ? &ground : point, surface, NONE);
}

static boolean ffo_actor_fighting(long unit_index)
{
	struct unit_datum *unit = ffo_living_unit(unit_index);
	struct actor_datum *actor = unit && unit->unit.actor_index != NONE ? actor_try_and_get(unit->unit.actor_index) : NULL;

	return actor && actor->target.target_prop_index != NONE;
}

/* ---------- the map's objective points */

static void ffo_map_scan(void)
{
	struct scenario *scen = global_scenario_try_and_get();
	short hill_ids[FFO_MAX_HILLS];
	short i;

	g.site_count = 0;
	g.hill_count = 0;
	g.flag_count = 0;
	g.ball_count = 0;
	g.set_available = 0;
	if (!scen)
		return;
	for (i = 0; i < scen->netgame_flags.count; i++)
	{
		struct scenario_netgame_flag *flag = TAG_BLOCK_GET_ELEMENT(&scen->netgame_flags, i, struct scenario_netgame_flag);

		if (!flag)
			continue;
		if (flag->type == _netgame_flag_ctf_flag && g.flag_count < FFO_MAX_FLAGS)
		{
			g.flag_teams[g.flag_count] = flag->team_index;
			g.flags[g.flag_count++] = flag->position;
		}
		else if (flag->type == _netgame_flag_oddball_ball_spawn && g.ball_count < FFO_MAX_FLAGS)
		{
			g.balls[g.ball_count++] = flag->position;
		}
		else if (flag->type == _netgame_flag_hill)
		{
			short hill;

			for (hill = 0; hill < g.hill_count && hill_ids[hill] != flag->team_index; hill++)
				;
			if (hill == g.hill_count)
			{
				if (g.hill_count >= FFO_MAX_HILLS)
					continue;
				hill_ids[g.hill_count] = flag->team_index;
				csmemset(&g.hills[g.hill_count], 0, sizeof(g.hills[g.hill_count]));
				g.hill_count++;
			}
			if (g.hills[hill].point_count < FFO_HILL_POINTS)
			{
				g.hills[hill].points[g.hills[hill].point_count].x = flag->position.x;
				g.hills[hill].points[g.hills[hill].point_count].y = flag->position.y;
				g.hills[hill].point_z[g.hills[hill].point_count] = flag->position.z;
				g.hills[hill].center.x += flag->position.x;
				g.hills[hill].center.y += flag->position.y;
				g.hills[hill].center.z += flag->position.z;
				g.hills[hill].point_count++;
			}
		}
	}
	/* (each hill's middle, its size, and its markers in order round it) */
	for (i = 0; i < g.hill_count; i++)
	{
		struct ffo_hill *hill = &g.hills[i];
		short a, b;

		hill->center.x /= (real)hill->point_count;
		hill->center.y /= (real)hill->point_count;
		hill->center.z /= (real)hill->point_count;
		hill->radius = 3.0f;
		for (a = 0; a < hill->point_count; a++)
		{
			real dx = hill->points[a].x - hill->center.x, dy = hill->points[a].y - hill->center.y;

			hill->radius = MAX(hill->radius, (real)sqrt(dx * dx + dy * dy));
		}
		for (a = 1; a < hill->point_count; a++)
		{
			for (b = a; b > 0; b--)
			{
				real angle_b = (real)atan2(hill->points[b].y - hill->center.y, hill->points[b].x - hill->center.x);
				real angle_p = (real)atan2(hill->points[b - 1].y - hill->center.y, hill->points[b - 1].x - hill->center.x);
				real_point2d swap;
				real swap_z;

				if (angle_p <= angle_b)
					break;
				swap = hill->points[b];
				hill->points[b] = hill->points[b - 1];
				hill->points[b - 1] = swap;
				swap_z = hill->point_z[b];
				hill->point_z[b] = hill->point_z[b - 1];
				hill->point_z[b - 1] = swap_z;
			}
		}
	}
	/* the sites: flags, hills, oddball spawns */
	for (i = 0; i < g.flag_count && g.site_count < FFO_MAX_SITES; i++)
	{
		g.sites[g.site_count].point = g.flags[i];
		g.sites[g.site_count].kind = _ffo_site_flag;
		g.sites[g.site_count++].index = i;
	}
	for (i = 0; i < g.hill_count && g.site_count < FFO_MAX_SITES; i++)
	{
		g.sites[g.site_count].point = g.hills[i].center;
		g.sites[g.site_count].kind = _ffo_site_hill;
		g.sites[g.site_count++].index = i;
	}
	for (i = 0; i < g.ball_count && g.site_count < FFO_MAX_SITES; i++)
	{
		g.sites[g.site_count].point = g.balls[i];
		g.sites[g.site_count].kind = _ffo_site_oddball;
		g.sites[g.site_count++].index = i;
	}
	if (g.site_count > 0)
	{
		g.set_available |= (1UL << _ff_set_objective_target) | (1UL << _ff_set_objective_generator) |
			(1UL << _ff_set_objective_purge) | (1UL << _ff_set_objective_rescue);
	}
	if (g.hill_count > 0)
		g.set_available |= 1UL << _ff_set_objective_data_link;
	if (g.flag_count >= 2 && get_flag_definition_index() != NONE)
		g.set_available |= 1UL << _ff_set_objective_flag;
	if (g.ball_count > 0 && get_ball_definition_index() != NONE)
		g.set_available |= 1UL << _ff_set_objective_oddball;
	if ((g.hill_count > 0 || g.flag_count > 0) && get_ball_definition_index() != NONE)
		g.set_available |= 1UL << _ff_set_objective_collect;
	if (g.site_count >= 2 && get_ball_definition_index() != NONE)
		g.set_available |= 1UL << _ff_set_objective_bomb;
	error(_error_silent, "firefight objectives: map has %d flags, %d hills, %d oddball spawns (set objectives 0x%lX)",
		g.flag_count, g.hill_count, g.ball_count, g.set_available);
}

/* (a hill's inside: its markers' shape, else round its middle) */
static boolean ffo_in_hill(struct ffo_hill const *hill, real_point3d const *point)
{
	boolean inside = FALSE;
	short a, b;

	if (fabs(point->z - hill->center.z) > FFO_HILL_HEIGHT)
		return FALSE;
	if (hill->point_count < 3)
	{
		real dx = point->x - hill->center.x, dy = point->y - hill->center.y;

		return dx * dx + dy * dy <= hill->radius * hill->radius;
	}
	for (a = 0, b = (short)(hill->point_count - 1); a < hill->point_count; b = a++)
	{
		real_point2d const *pa = &hill->points[a], *pb = &hill->points[b];

		if ((pa->y > point->y) != (pb->y > point->y) &&
			point->x < (pb->x - pa->x) * (point->y - pa->y) / (pb->y - pa->y) + pa->x)
		{
			inside = !inside;
		}
	}
	return inside;
}

/* whether a point is another objective's: for a round objective, Travel's
(it is rolled first, as a wave starts); for Travel, the round objective's
running (its place, its goal) */
static boolean ffo_point_taken(real_point3d const *point, boolean for_set)
{
	if (for_set)
	{
		return g_view.round_state == _ff_objective_running && g_view.round_kind == _ff_round_objective_travel &&
			distance3d(point, &g_view.round_point) < FFO_POINT_APART;
	}
	if (g_view.set_state != _ff_objective_running)
		return FALSE;
	if (distance3d(point, &g_view.set_site) < MAX(FFO_POINT_APART, g_view.set_radius + 3.0f))
		return TRUE;
	return (g_view.set_kind == _ff_set_objective_flag || g_view.set_kind == _ff_set_objective_bomb) &&
		distance3d(point, &g_view.set_goal_point) < FFO_POINT_APART;
}

/* a site for a set objective, of the kinds `kinds` (1 << enum ffo_site_kind):
one far from the players (of the farther half of those they can walk to), not
one a player is at, nor another objective's (ffo_point_taken) */
static short ffo_pick_site_of(unsigned long kinds, real keep_away, boolean for_set)
{
	short order[FFO_MAX_SITES];
	real distance[FFO_MAX_SITES];
	short count = 0;
	real_point3d positions[16];
	short player_count = ffo_living_players(NULL, NULL, positions, 16);
	long groups[16];
	short p;
	short i;

	for (p = 0; p < player_count; p++)
		groups[p] = firefight_objective_reach_group(&positions[p]);
	for (i = 0; i < g.site_count; i++)
	{
		real nearest = ffo_nearest_player_distance(&g.sites[i].point, NULL);
		long group;
		boolean reachable = player_count == 0;
		short place;

		if (!(kinds & (1UL << g.sites[i].kind)) || (player_count > 0 && nearest < keep_away) ||
			ffo_point_taken(&g.sites[i].point, for_set))
		{
			continue;
		}
		group = firefight_objective_reach_group(&g.sites[i].point);
		for (p = 0; p < player_count && !reachable; p++)
			reachable = group == NONE || groups[p] == NONE || group == groups[p];
		if (!reachable)
			continue;
		for (place = count; place > 0 && distance[place - 1] < nearest; place--)
		{
			order[place] = order[place - 1];
			distance[place] = distance[place - 1];
		}
		order[place] = i;
		distance[place] = nearest;
		count++;
	}
	if (!count)
	{
		/* (none far: any of the kind not another's, else any of the kind) */
		for (i = 0; i < g.site_count; i++)
		{
			if ((kinds & (1UL << g.sites[i].kind)) && !ffo_point_taken(&g.sites[i].point, for_set))
				order[count++] = i;
		}
		for (i = 0; !count && i < g.site_count; i++)
		{
			if (kinds & (1UL << g.sites[i].kind))
				order[count++] = i;
		}
		return count ? order[ffo_rand() % (unsigned long)count] : NONE;
	}
	return order[ffo_rand() % (unsigned long)MAX(1, (count + 1) / 2)];
}

#define FFO_ANY_SITE ((1UL << _ffo_site_flag) | (1UL << _ffo_site_hill) | (1UL << _ffo_site_oddball))

static short ffo_pick_site(unsigned long kinds, real keep_away)
{
	return ffo_pick_site_of(kinds, keep_away, TRUE);
}

/* (a VIP's camp, Purge's base, the marines': a flag's spot, the bases a map
is built round, with their cover; any objective point on a map with none) */
static short ffo_pick_base_site(real keep_away)
{
	short site = g.flag_count > 0 ? ffo_pick_site(1UL << _ffo_site_flag, keep_away) : NONE;

	return site != NONE ? site : ffo_pick_site(FFO_ANY_SITE, keep_away);
}

/* ---------- the maps' file (what each map has, for Server Setup) */

#define FFO_MAP_FILE "d:\\firefight_objective_maps.txt"

static struct
{
	char stem[96];
	boolean read;
	boolean known;
	unsigned long available;
} g_map_known;

static void ffo_map_stem(char const *map_name, char *stem, size_t size)
{
	char const *slash;
	char *cut;
	size_t index;

	map_name = map_name ? map_name : "";
	if ((slash = strrchr(map_name, '\\')) != NULL)
		map_name = slash + 1;
	if ((slash = strrchr(map_name, '/')) != NULL)
		map_name = slash + 1;
	for (index = 0; index + 1 < size && map_name[index]; index++)
		stem[index] = (char)tolower((unsigned char)map_name[index]);
	stem[index] = 0;
	if ((cut = strchr(stem, '@')) != NULL)
		*cut = 0;
	if ((cut = strstr(stem, ".map")) != NULL && !cut[4])
		*cut = 0;
}

static boolean ffo_map_file_find(char const *stem, unsigned long *available)
{
	FILE *file = fopen(FFO_MAP_FILE, "r");
	char line[160];
	boolean found = FALSE;

	if (!file)
		return FALSE;
	while (!found && fgets(line, sizeof(line), file))
	{
		char name[128];
		unsigned long bits;

		if (sscanf(line, "%127s %lx", name, &bits) == 2 && !strcmp(name, stem))
		{
			*available = bits;
			found = TRUE;
		}
	}
	fclose(file);
	return found;
}

static void ffo_map_file_write(char const *map_name, unsigned long available)
{
	char stem[96];
	char lines[64][160];
	short count = 0;
	unsigned long known;
	FILE *file;
	short index;

	ffo_map_stem(map_name, stem, sizeof(stem));
	if (!stem[0] || (ffo_map_file_find(stem, &known) && known == available))
		return;
	if ((file = fopen(FFO_MAP_FILE, "r")) != NULL)
	{
		char line[160];
		char name[128];

		while (count < (short)NUMBEROF(lines) - 1 && fgets(line, sizeof(line), file))
		{
			if (sscanf(line, "%127s", name) == 1 && strcmp(name, stem))
				snprintf(lines[count++], sizeof(lines[0]), "%s", line);
		}
		fclose(file);
	}
	if ((file = fopen(FFO_MAP_FILE, "w")) == NULL)
		return;
	for (index = 0; index < count; index++)
		fputs(lines[index], file);
	fprintf(file, "%s %lx\n", stem, available);
	fclose(file);
	g_map_known.stem[0] = 0;
}

boolean firefight_objectives_map_known(char const *map_name, unsigned long *set_available)
{
	char stem[96];

	ffo_map_stem(map_name, stem, sizeof(stem));
	if (!stem[0])
		return FALSE;
	if (strcmp(g_map_known.stem, stem))
	{
		snprintf(g_map_known.stem, sizeof(g_map_known.stem), "%s", stem);
		g_map_known.known = ffo_map_file_find(stem, &g_map_known.available);
	}
	if (set_available)
		*set_available = g_map_known.known ? g_map_known.available : 0;
	return g_map_known.known;
}

/* ---------- weapons' classes */

static boolean ffo_weapon_class(long weapon_tag, short *class_out)
{
	struct weapon_definition *definition;
	struct weapon_trigger_definition *trigger;
	char const *name;

	if (weapon_tag == NONE || !tag_index_is_group(weapon_tag, WEAPON_DEFINITION_TAG))
		return FALSE;
	definition = weapon_definition_get(weapon_tag);
	if (!definition || TEST_FLAG(definition->weapon.flags, _weapon_must_be_readied_bit) ||
		definition->weapon.triggers.count <= 0)
	{
		return FALSE;
	}
	trigger = TAG_BLOCK_GET_ELEMENT(&definition->weapon.triggers, 0, struct weapon_trigger_definition);
	name = tag_get_name(weapon_tag);
	if (name && (strstr(name, "rocket") || strstr(name, "fuel rod") || strstr(name, "plasma_cannon") ||
		strstr(name, "plasma cannon")))
	{
		*class_out = _ff_objective_weapon_rocket;
	}
	else if ((trigger && trigger->projectiles_per_shot >= 5) || (name && strstr(name, "shotgun")))
		*class_out = _ff_objective_weapon_shotgun;
	else if (definition->weapon.magazines.count <= 0 || (trigger && trigger->magazine_index == NONE) ||
		(name && strstr(name, "plasma")))
	{
		*class_out = _ff_objective_weapon_plasma;
	}
	else if (trigger && TEST_FLAG(trigger->flags, _weapon_trigger_latched_bit))
		*class_out = _ff_objective_weapon_semi_automatic;
	else
		*class_out = _ff_objective_weapon_automatic;
	return TRUE;
}

/* the class of what a unit holds now; FALSE for none (a vehicle's seat, its hands) */
static boolean ffo_unit_weapon_class(struct unit_datum const *unit, short *class_out)
{
	long weapon_index;
	struct object_datum *weapon;

	if (!unit || unit->object.parent_object_index != NONE ||
		unit->unit.current_weapon_index < 0 || unit->unit.current_weapon_index >= MAXIMUM_WEAPONS_PER_UNIT)
	{
		return FALSE;
	}
	weapon_index = unit->unit.weapon_object_indices[unit->unit.current_weapon_index];
	weapon = weapon_index != NONE ? object_try_and_get_and_verify_type(weapon_index, _object_mask_weapon) : NULL;
	return weapon && ffo_weapon_class(weapon->definition_index, class_out);
}

/* ---------- points and rewards */

/* each player's share of `points`, by what each did (of those still here;
nobody: the counted players' alike); with pods, a pod to the better half and
more to the best */
static void ffo_award(long const *contribution, long points, boolean pods, boolean equal_counted)
{
	struct data_iterator iterator;
	struct player_datum *player;
	long total = 0;
	long best = 0;
	long best_player = NONE;
	short sharing = 0;
	long order[HALO_PORT_MAXIMUM_NETWORK_PLAYERS];
	long amounts[HALO_PORT_MAXIMUM_NETWORK_PLAYERS];
	short ranked = 0;

	if (!player_data)
		return;
	data_iterator_new(&iterator, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
	{
		if (!player->quit_out_of_game && (short)DATUM_INDEX_TO_ABSOLUTE_INDEX(iterator.datum_index) < HALO_PORT_MAXIMUM_NETWORK_PLAYERS)
		{
			total += MAX(0L, contribution[(short)DATUM_INDEX_TO_ABSOLUTE_INDEX(iterator.datum_index)]);
			if (ffo_bit(g.counted, (short)DATUM_INDEX_TO_ABSOLUTE_INDEX(iterator.datum_index)))
				sharing++;
		}
	}
	data_iterator_new(&iterator, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
	{
		long done;
		long share;

		if (player->quit_out_of_game || (short)DATUM_INDEX_TO_ABSOLUTE_INDEX(iterator.datum_index) >= HALO_PORT_MAXIMUM_NETWORK_PLAYERS)
			continue;
		done = MAX(0L, contribution[(short)DATUM_INDEX_TO_ABSOLUTE_INDEX(iterator.datum_index)]);
		if (total > 0 && !equal_counted)
			share = (long)((double)points * (double)done / (double)total + 0.5);
		else
			share = ffo_bit(g.counted, (short)DATUM_INDEX_TO_ABSOLUTE_INDEX(iterator.datum_index)) && sharing > 0 ? points / sharing : 0;
		if (share > 0)
			campaign_score_award_points(iterator.datum_index, share);
		if (done > 0 || (total == 0 && ffo_bit(g.counted, (short)DATUM_INDEX_TO_ABSOLUTE_INDEX(iterator.datum_index))))
		{
			short place;

			for (place = ranked; place > 0 && amounts[place - 1] < done; place--)
			{
				order[place] = order[place - 1];
				amounts[place] = amounts[place - 1];
			}
			order[place] = (short)DATUM_INDEX_TO_ABSOLUTE_INDEX(iterator.datum_index);
			amounts[place] = done;
			ranked++;
			if (done > best || best_player == NONE)
			{
				best = done;
				best_player = iterator.datum_index;
			}
		}
	}
	if (!pods || !ranked)
		return;
	if (best_player != NONE)
		campaign_score_award_points(best_player, FFO_TOP_PLAYER_POINTS);
	{
		unsigned long players[FFO_PLAYER_WORDS];
		short winners = (short)((ranked + 1) / 2);
		short index;

		csmemset(players, 0, sizeof(players));
		for (index = 0; index < winners; index++)
			ffo_bit_set(players, order[index]);
		firefight_spawn_ordnance_drop_for(players, FFO_PLAYER_WORDS);
	}
}

/* ---------- the round objective */

static boolean ffo_counted(long player_index)
{
	return player_index != NONE && ffo_bit(g.counted, ffo_absolute(player_index));
}

static void ffo_round_end(byte state)
{
	char text[96];

	if (g_view.round_state != _ff_objective_running)
		return;
	g_view.round_state = state;
	g_view.round_result_ticks = FFO_RESULT_TICKS;
	g_view.round_target = NONE;
	g.round_target_unit = NONE;
	switch (state)
	{
	case _ff_objective_won:
		ffo_award(g.round_contribution, FFO_ROUND_POINTS, FALSE,
			g_view.round_kind == _ff_round_objective_no_deaths || g_view.round_kind == _ff_round_objective_shields ||
			g_view.round_kind == _ff_round_objective_dont_use || g_view.round_kind == _ff_round_objective_weapon_only);
		snprintf(text, sizeof(text), "WAVE OBJECTIVE COMPLETE: +%d", (int)FFO_ROUND_POINTS);
		break;
	case _ff_objective_tied:
		snprintf(text, sizeof(text), "WAVE OBJECTIVE: TIE");
		break;
	default:
		snprintf(text, sizeof(text), "WAVE OBJECTIVE FAILED");
		break;
	}
	ffo_notice(text, FFO_NOTICE_TICKS);
}

static void ffo_round_fail(long player_index, char const *what)
{
	char name[48];
	char text[96];

	if (g_view.round_state != _ff_objective_running)
		return;
	ffo_player_name(player_index, name, sizeof(name));
	snprintf(text, sizeof(text), "%s %s", name, what);
	g_view.round_state = _ff_objective_failed;
	g_view.round_result_ticks = FFO_RESULT_TICKS;
	g_view.round_target = NONE;
	g.round_target_unit = NONE;
	ffo_notice(text, FFO_NOTICE_TICKS);
}

/* (the counted players: those in the game now) */
static void ffo_count_players(void)
{
	struct data_iterator iterator;
	struct player_datum *player;

	csmemset(g.counted, 0, sizeof(g.counted));
	csmemset(g.broken, 0, sizeof(g.broken));
	csmemset(g.round_contribution, 0, sizeof(g.round_contribution));
	g.counted_count = 0;
	if (!player_data)
		return;
	data_iterator_new(&iterator, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
	{
		if (player->quit_out_of_game)
			continue;
		ffo_bit_set(g.counted, (short)DATUM_INDEX_TO_ABSOLUTE_INDEX(iterator.datum_index));
		g.counted_count++;
	}
}

/* the weapon classes and grenades the counted players carry: how many of
them carry each */
static void ffo_carried(short classes[NUMBER_OF_FF_OBJECTIVE_WEAPON_CLASSES], short grenades[NUMBER_OF_UNIT_GRENADE_TYPES])
{
	struct data_iterator iterator;
	struct player_datum *player;

	csmemset(classes, 0, sizeof(short) * NUMBER_OF_FF_OBJECTIVE_WEAPON_CLASSES);
	csmemset(grenades, 0, sizeof(short) * NUMBER_OF_UNIT_GRENADE_TYPES);
	if (!player_data)
		return;
	data_iterator_new(&iterator, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
	{
		struct unit_datum *unit = ffo_living_unit(player->unit_index);
		boolean seen[NUMBER_OF_FF_OBJECTIVE_WEAPON_CLASSES];
		short slot;

		if (!unit || !ffo_bit(g.counted, (short)DATUM_INDEX_TO_ABSOLUTE_INDEX(iterator.datum_index)))
			continue;
		csmemset(seen, 0, sizeof(seen));
		for (slot = 0; slot < MAXIMUM_WEAPONS_PER_UNIT; slot++)
		{
			long weapon_index = unit->unit.weapon_object_indices[slot];
			struct object_datum *weapon = weapon_index != NONE ?
				object_try_and_get_and_verify_type(weapon_index, _object_mask_weapon) : NULL;
			short weapon_class;

			if (weapon && ffo_weapon_class(weapon->definition_index, &weapon_class) && !seen[weapon_class])
			{
				seen[weapon_class] = TRUE;
				classes[weapon_class]++;
			}
		}
		for (slot = 0; slot < NUMBER_OF_UNIT_GRENADE_TYPES; slot++)
		{
			if (unit->unit.grenade_counts[slot] > 0)
				grenades[slot]++;
		}
	}
}

static boolean ffo_round_setup(short kind)
{
	struct firefight_options const *opts = firefight_options_get();
	real players_scale = 1.0f + 0.5f * (real)MAX(0, g.counted_count - 1);

	g_view.round_kind = (byte)kind;
	g_view.round_state = _ff_objective_running;
	g_view.round_param = 0;
	g_view.round_count = 0;
	g_view.round_goal = 0;
	g_view.round_target = NONE;
	g_view.round_result_ticks = 0;
	g_view.round_label[0] = 0;
	g.round_target_unit = NONE;
	g.round_vehicle_tag = NONE;
	g.round_weapon_tag = NONE;
	g.round_armed = FALSE;
	switch (kind)
	{
	case _ff_round_objective_kills:
	{
		static short const goals[NUMBER_OF_FF_OBJECTIVE_KILLS] = { 3, 2, 2, 1, 6, 5, 3, 5, 2 };
		short candidates[NUMBER_OF_FF_OBJECTIVE_KILLS];
		short classes[NUMBER_OF_FF_OBJECTIVE_WEAPON_CLASSES];
		short grenades[NUMBER_OF_UNIT_GRENADE_TYPES];
		short count = 0;
		short index;
		short pick;

		ffo_carried(classes, grenades);
		candidates[count++] = _ff_objective_kill_headshot;
		candidates[count++] = _ff_objective_kill_melee;
		for (index = 0; index < NUMBER_OF_UNIT_GRENADE_TYPES; index++)
		{
			if (grenades[index] > 0)
			{
				candidates[count++] = _ff_objective_kill_grenade;
				break;
			}
		}
		if (firefight_player_vehicles_at_start())
			candidates[count++] = _ff_objective_kill_splatter;
		for (index = 0; index < NUMBER_OF_FF_OBJECTIVE_WEAPON_CLASSES; index++)
		{
			if (classes[index] > 0)
				candidates[count++] = (short)(_ff_objective_kill_weapon + index);
		}
		pick = candidates[ffo_rand() % (unsigned long)count];
		g_view.round_param = (byte)pick;
		g_view.round_goal = (short)PIN((long)(goals[pick] * MIN(players_scale, 3.0f) + 0.5f), 1, 99);
		return TRUE;
	}
	case _ff_round_objective_no_deaths:
		return TRUE;
	case _ff_round_objective_shields:
		/* (none to keep, or none that come back) */
		if ((opts && opts->firefight_player_shields == _firefight_shields_none) ||
			firefight_health_rule(_firefight_health_rule_shields) == _firefight_health_rule_off ||
			firefight_skull_active(_firefight_skull_black_eye_bit) || skull_active(_skull_black_eye))
		{
			return FALSE;
		}
		g_view.round_goal = g.counted_count;
		return TRUE;
	case _ff_round_objective_kill_first:
		return TRUE;
	case _ff_round_objective_dont_use:
	{
		short classes[NUMBER_OF_FF_OBJECTIVE_WEAPON_CLASSES];
		short grenades[NUMBER_OF_UNIT_GRENADE_TYPES];
		short best = 0;
		short choices[NUMBER_OF_FF_OBJECTIVE_WEAPON_CLASSES + NUMBER_OF_UNIT_GRENADE_TYPES];
		short count = 0;
		short index;

		/* (Fiesta's weapons are anyone's guess) */
		if (opts && (opts->primary_weapon == _loadout_weapon_random || opts->secondary_weapon == _loadout_weapon_random))
			return FALSE;
		ffo_carried(classes, grenades);
		for (index = 0; index < NUMBER_OF_FF_OBJECTIVE_WEAPON_CLASSES; index++)
			best = MAX(best, classes[index]);
		for (index = 0; index < NUMBER_OF_UNIT_GRENADE_TYPES; index++)
			best = MAX(best, grenades[index]);
		if (!best)
			return FALSE;
		/* (what the most carry, so some of them change) */
		for (index = 0; index < NUMBER_OF_FF_OBJECTIVE_WEAPON_CLASSES; index++)
		{
			if (classes[index] == best)
				choices[count++] = index;
		}
		for (index = 0; index < NUMBER_OF_UNIT_GRENADE_TYPES; index++)
		{
			if (grenades[index] == best)
				choices[count++] = (short)(_ff_objective_use_grenade + index);
		}
		g_view.round_param = (byte)choices[ffo_rand() % (unsigned long)count];
		return TRUE;
	}
	case _ff_round_objective_vehicle_kills:
	{
		long types[16];
		short count = firefight_objective_vehicle_types(types, 16);
		long vehicle;

		if (!count || !ffo_living_players(NULL, NULL, NULL, 1))
			return FALSE;
		g.round_vehicle_tag = types[ffo_rand() % (unsigned long)count];
		/* (one for them, if the map has none of it there) */
		vehicle = firefight_objective_give_vehicle(g.round_vehicle_tag);
		if (vehicle == NONE)
			return FALSE;
		ffo_tag_label(g.round_vehicle_tag, g_view.round_label, sizeof(g_view.round_label));
		g_view.round_target = ffo_absolute_of_object(vehicle);
		g_view.round_goal = (short)PIN((long)(3.0f * MIN(players_scale, 3.0f) + 0.5f), 1, 99);
		return TRUE;
	}
	case _ff_round_objective_travel:
	{
		short site = ffo_pick_site_of(FFO_ANY_SITE, 15.0f, FALSE);

		if (site == NONE || ffo_point_taken(&g.sites[site].point, FALSE))
			return FALSE;
		g_view.round_point = g.sites[site].point;
		return TRUE;
	}
	case _ff_round_objective_weapon_only:
	{
		long avoid[16];
		short avoid_count = 0;
		struct data_iterator iterator;
		struct player_datum *player;

		if (!ffo_living_players(NULL, NULL, NULL, 1) || !player_data)
			return FALSE;
		/* (one none of them has: what they start with isn't it) */
		data_iterator_new(&iterator, player_data);
		while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
		{
			struct unit_datum *unit = ffo_living_unit(player->unit_index);
			short slot;

			for (slot = 0; unit && slot < MAXIMUM_WEAPONS_PER_UNIT && avoid_count < (short)NUMBEROF(avoid); slot++)
			{
				struct object_datum *weapon = unit->unit.weapon_object_indices[slot] != NONE ?
					object_try_and_get_and_verify_type(unit->unit.weapon_object_indices[slot], _object_mask_weapon) : NULL;

				if (weapon)
					avoid[avoid_count++] = weapon->definition_index;
			}
		}
		g.round_weapon_tag = firefight_objective_ordnance_weapon(avoid, avoid_count);
		if (g.round_weapon_tag == NONE)
			g.round_weapon_tag = firefight_objective_ordnance_weapon(NULL, 0);
		if (g.round_weapon_tag == NONE)
			return FALSE;
		g.round_armed = FALSE;
		g.round_arm_time = game_time_get() + FFO_USE_ONLY_GRACE_TICKS;
		g.round_redrop_time = g.round_arm_time;
		ffo_tag_label(g.round_weapon_tag, g_view.round_label, sizeof(g_view.round_label));
		/* (a pod of it for each of them, as an ordnance drop's) */
		firefight_spawn_ordnance_weapon_for_all(g.round_weapon_tag);
		return TRUE;
	}
	default:
		return FALSE;
	}
}

static void ffo_round_text(char *text, size_t size)
{
	short param = g_view.round_param;

	switch (g_view.round_kind)
	{
	case _ff_round_objective_kills:
		snprintf(text, size, "%d %s KILLS", (int)g_view.round_goal,
			param < NUMBER_OF_FF_OBJECTIVE_KILLS ? ffo_kill_names[param] : "");
		break;
	case _ff_round_objective_dont_use:
		if (param >= _ff_objective_use_grenade)
		{
			short type = (short)(param - _ff_objective_use_grenade);

			snprintf(text, size, "DON'T THROW %s", type == 0 ? "FRAG GRENADES" : type == 1 ? "PLASMA GRENADES" : "GRENADES");
		}
		else
			snprintf(text, size, "DON'T FIRE %s", param < NUMBER_OF_FF_OBJECTIVE_WEAPON_CLASSES ? ffo_class_names[param] : "");
		break;
	case _ff_round_objective_vehicle_kills:
		snprintf(text, size, "%d KILLS WITH THE %s", (int)g_view.round_goal, g_view.round_label);
		break;
	case _ff_round_objective_travel:
		snprintf(text, size, "GET TO THE MARKED POINT");
		break;
	case _ff_round_objective_weapon_only:
		snprintf(text, size, "USE ONLY THE %s", g_view.round_label);
		break;
	default:
		snprintf(text, size, "%s", g_view.round_kind < NUMBER_OF_FF_ROUND_OBJECTIVES ? ffo_round_names[g_view.round_kind] : "");
		break;
	}
}

static void ffo_round_roll(void)
{
	short candidates[NUMBER_OF_FF_ROUND_OBJECTIVES];
	short count = 0;
	short kind;
	long chance = PIN(config_integer("game.firefight_objectives_round_chance"), 0, 100);

	g_view.round_kind = _ff_round_objective_none;
	g_view.round_state = _ff_objective_off;
	if (!config_boolean("game.firefight_objectives_round") || (long)(ffo_rand() % 100UL) >= chance)
		return;
	for (kind = 1; kind < NUMBER_OF_FF_ROUND_OBJECTIVES; kind++)
	{
		if (config_boolean(ffo_round_settings[kind]))
			candidates[count++] = kind;
	}
	while (count > 0)
	{
		short pick = (short)(ffo_rand() % (unsigned long)count);
		char text[96];
		char line[96];

		if (ffo_round_setup(candidates[pick]))
		{
			ffo_round_text(text, sizeof(text));
			snprintf(line, sizeof(line), "WAVE OBJECTIVE: %s", text);
			ffo_notice(line, FFO_NOTICE_TICKS + 2 * TICKS_PER_SECOND);
			return;
		}
		candidates[pick] = candidates[--count];
	}
	g_view.round_kind = _ff_round_objective_none;
	g_view.round_state = _ff_objective_off;
}

/* (Shields intact's: each counted player's shield, broken once at most) */
static void ffo_round_shields_update(void)
{
	struct data_iterator iterator;
	struct player_datum *player;

	if (!player_data)
		return;
	data_iterator_new(&iterator, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
	{
		long absolute = (short)DATUM_INDEX_TO_ABSOLUTE_INDEX(iterator.datum_index);
		struct unit_datum *unit = ffo_living_unit(player->unit_index);

		if (absolute >= HALO_PORT_MAXIMUM_NETWORK_PLAYERS)
			continue;
		if (!unit || unit->object.maximum_shield_vitality <= 0.0f)
		{
			g.shield_unit[absolute] = NONE;
			continue;
		}
		if (g.shield_unit[absolute] != player->unit_index)
		{
			g.shield_unit[absolute] = player->unit_index;
			g.shield_last[absolute] = unit->object.shield_vitality;
			continue;
		}
		if (g.shield_last[absolute] > 0.0f && unit->object.shield_vitality <= 0.0f &&
			g_view.round_kind == _ff_round_objective_shields && g_view.round_state == _ff_objective_running &&
			ffo_bit(g.counted, absolute) && !ffo_bit(g.broken, absolute))
		{
			char name[48];
			char text[96];

			ffo_bit_set(g.broken, absolute);
			g_view.round_count++;
			if (g_view.round_count * 2 > g_view.round_goal)
				ffo_round_fail(iterator.datum_index, "BROKE THEIR SHIELDS");
			else
			{
				ffo_player_name(iterator.datum_index, name, sizeof(name));
				snprintf(text, sizeof(text), "%s BROKE THEIR SHIELDS (%d OF %d)", name, (int)g_view.round_count,
					(int)(g_view.round_goal / 2));
				ffo_notice(text, FFO_NOTICE_TICKS);
			}
		}
		g.shield_last[absolute] = unit->object.shield_vitality;
	}
}

/* ---------- the set objective's units */

/* (the list's dead taken out, for room: the marines' and the target's are
kept apart as well) */
static void ffo_units_compact(void)
{
	short kept = 0;
	short index;

	for (index = 0; index < g.unit_count; index++)
	{
		if (!ffo_living_unit(g.units[index]) && g.unit_roles[index] != _ffo_role_marine)
			continue;
		g.units[kept] = g.units[index];
		g.unit_roles[kept] = g.unit_roles[index];
		g.unit_next_order[kept] = g.unit_next_order[index];
		kept++;
	}
	g.unit_count = kept;
}

static short ffo_unit_add(long unit_index, byte role)
{
	if (unit_index != NONE && g.unit_count >= FFO_MAX_UNITS)
		ffo_units_compact();
	if (unit_index == NONE || g.unit_count >= FFO_MAX_UNITS)
		return NONE;
	g.units[g.unit_count] = unit_index;
	g.unit_roles[g.unit_count] = role;
	g.unit_next_order[g.unit_count] = game_time_get() + (long)(ffo_rand() % (unsigned long)(3 * TICKS_PER_SECOND));
	/* (a camp's, and the marines: kept to their place, fighting from it) */
	if (role == _ffo_role_defender || role == _ffo_role_target || role == _ffo_role_marine)
	{
		struct unit_datum *unit = ffo_living_unit(unit_index);

		if (unit && unit->unit.actor_index != NONE)
			firefight_objective_hold_ground(unit->unit.actor_index, TRUE);
	}
	return g.unit_count++;
}

/* (a vehicle and its crew, theirs: the crew are those in it now; `slot` the
players' vehicle slot whose spot it took, NONE for none) */
static void ffo_vehicle_add(long vehicle_index, byte role, short slot)
{
	struct object_iterator iterator;
	struct object_datum *object;

	if (vehicle_index == NONE || g.vehicle_count >= FFO_MAX_VEHICLES)
	{
		firefight_objective_release_vehicle_slot(slot);
		return;
	}
	g.vehicle_slots[g.vehicle_count] = slot;
	g.vehicles[g.vehicle_count++] = vehicle_index;
	object_iterator_new(&iterator, _object_mask_unit, 0);
	while ((object = (struct object_datum *)object_iterator_next(&iterator)) != NULL)
	{
		if (object->object.parent_object_index == vehicle_index &&
			((struct unit_datum *)object)->unit.player_index == NONE)
		{
			ffo_unit_add(iterator.index, role);
		}
	}
}

static short ffo_units_alive(byte role, boolean any_role)
{
	short alive = 0;
	short index;

	for (index = 0; index < g.unit_count; index++)
	{
		if ((any_role || g.unit_roles[index] == role) && ffo_living_unit(g.units[index]))
			alive++;
	}
	return alive;
}

static boolean ffo_vehicle_has_player(long vehicle_index)
{
	struct object_iterator iterator;
	struct object_datum *object;

	object_iterator_new(&iterator, _object_mask_unit, 0);
	while ((object = (struct object_datum *)object_iterator_next(&iterator)) != NULL)
	{
		if (object->object.parent_object_index == vehicle_index &&
			((struct unit_datum *)object)->unit.player_index != NONE)
		{
			return TRUE;
		}
	}
	return FALSE;
}

/* (the players' vehicle spots taken: each given back once its enemy vehicle
is destroyed, or `all` of them, the set over) */
static void ffo_vehicle_slots_update(boolean all)
{
	short index;

	for (index = 0; index < g.vehicle_count; index++)
	{
		struct object_datum *vehicle = g.vehicles[index] != NONE ?
			object_try_and_get_and_verify_type(g.vehicles[index], _object_mask_vehicle) : NULL;

		if (g.vehicle_slots[index] == NONE)
			continue;
		if (all || !vehicle || TEST_FLAG(vehicle->object.damage_flags, _object_dead_bit))
		{
			firefight_objective_release_vehicle_slot(g.vehicle_slots[index]);
			g.vehicle_slots[index] = NONE;
		}
	}
}

/* the camp's units leaving: cloaked now, gone a moment later
(ffo_teleport_update); the marines stay */
static void ffo_teleport_out(void)
{
	short index;

	for (index = 0; index < g.unit_count; index++)
	{
		struct unit_datum *unit = ffo_living_unit(g.units[index]);

		if (!unit || g.unit_roles[index] == _ffo_role_marine)
			continue;
		SET_FLAG(unit->unit.flags, _unit_active_camouflaged_bit, TRUE);
		unit->unit.active_camouflage = 1.0f;
	}
	g.teleport_until = game_time_get() + FFO_TELEPORT_TICKS;
}

static void ffo_teleport_update(void)
{
	short index;

	if (g.teleport_until == NONE || game_time_get() < g.teleport_until)
		return;
	g.teleport_until = NONE;
	for (index = 0; index < g.unit_count; index++)
	{
		struct unit_datum *unit = ffo_living_unit(g.units[index]);

		if (!unit || g.unit_roles[index] == _ffo_role_marine)
			continue;
		/* (a crew's, with its vehicle) */
		if (unit->object.parent_object_index != NONE)
			continue;
		object_delete(g.units[index]);
		g.units[index] = NONE;
	}
	for (index = 0; index < g.vehicle_count; index++)
	{
		if (g.vehicles[index] != NONE && object_try_and_get(g.vehicles[index]) &&
			!ffo_vehicle_has_player(g.vehicles[index]))
		{
			object_delete(g.vehicles[index]);
		}
		g.vehicles[index] = NONE;
	}
	ffo_vehicle_slots_update(TRUE);
	for (index = 0; index < g.unit_count; index++)
	{
		if (g.unit_roles[index] != _ffo_role_marine)
			g.units[index] = NONE;
	}
}

/* ---------- the enemy generator (Enemy Generator's) */

static void ffo_generator_spawn(real_point3d const *at, boolean fresh)
{
	long unit_tag = firefight_generator_unit_tag();
	long scenery_tag = firefight_generator_scenery_tag();
	short team = global_variant.game_engine_variant.firefight.wave_faction == _firefight_faction_flood ?
		_game_team_flood : _game_team_covenant;
	struct firefight_options const *opts = firefight_options_get();

	g.target_unit = NONE;
	g.generator_scenery = NONE;
	if (scenery_tag != NONE)
	{
		struct object_placement_data placement;

		object_placement_data_new(&placement, scenery_tag, NONE);
		placement.position = *at;
		placement.owner_team_index = team;
		g.generator_scenery = object_new(&placement);
		if (g.generator_scenery != NONE)
		{
			struct object_datum *scenery = object_get(g.generator_scenery);

			SET_FLAG(scenery->object.flags, _object_cannot_be_garbage_bit, TRUE);
			SET_FLAG(scenery->object.flags, _object_no_collisions_bit, TRUE);
			object_set_automatic_deactivation(g.generator_scenery, FALSE);
		}
	}
	if (unit_tag != NONE)
	{
		struct object_placement_data placement;
		struct unit_datum *unit;

		object_placement_data_new(&placement, unit_tag, NONE);
		placement.position = *at;
		placement.owner_team_index = team;
		g.target_unit = object_new(&placement);
		unit = g.target_unit != NONE ? unit_try_and_get(g.target_unit) : NULL;
		if (unit)
		{
			unit->object.owner_team_index = team;
			unit->object.body_vitality = 1.0f;
			unit->object.shield_vitality = 0.0f;
			SET_FLAG(unit->object.flags, _object_cannot_be_garbage_bit, TRUE);
			SET_FLAG(unit->object.flags, _object_invisible_bit, FALSE);
			SET_FLAG(unit->unit.control_flags, _unit_control_crouch_modifier_bit, TRUE);
			unit_delete_all_weapons(g.target_unit);
			object_set_automatic_deactivation(g.target_unit, FALSE);
		}
	}
	if (fresh)
	{
		/* (a third of the variant's GENERATOR HEALTH, about: a round's work,
		not more) */
		g_view.set_goal = ((long)(opts && opts->firefight_generator_health >= 500 ?
			opts->firefight_generator_health : 2000) * FFO_GENERATOR_HEALTH_PERCENT / 100);
		g_view.set_progress = g_view.set_goal;
	}
}

/* the generator going up: a plasma blast, harmless, at it and round it
(each machine's own) */
static void ffo_generator_blast(real_point3d const *at)
{
	static char const *const effects[] =
	{
		"weapons\\plasma grenade\\effects\\explosion",
		"weapons\\plasma_cannon\\effects\\plasma_cannon explosion",
		"effects\\explosions\\medium explosion"
	};
	short index;

	for (index = 0; index < 5; index++)
	{
		real angle = (real)index * 0.4f * _pi;
		real_point3d point = *at;

		if (index)
		{
			point.x += cosine(angle) * 1.5f;
			point.y += sine(angle) * 1.5f;
		}
		point.z += 0.5f + 0.4f * (real)(index & 1);
		firefight_objective_effect(effects, (short)NUMBEROF(effects), &point);
	}
}

static void ffo_generator_remove(void)
{
	if (g.target_unit != NONE && object_try_and_get(g.target_unit))
		object_delete(g.target_unit);
	if (g.generator_scenery != NONE && object_try_and_get(g.generator_scenery))
		object_delete(g.generator_scenery);
	g.target_unit = NONE;
	g.generator_scenery = NONE;
}

boolean firefight_objectives_generator_hidden(long object_index)
{
	if (object_index == NONE || g_view.set_kind != _ff_set_objective_generator || g_view.set_state != _ff_objective_running)
		return FALSE;
	if (!network_game_distributed_client())
		return object_index == g.target_unit;
	return g_view.set_target != NONE && ffo_absolute_of_object(object_index) == g_view.set_target;
}

/* (a client's own looks for the host's generator, as the pods are each
machine's) */
static void ffo_client_generator_update(void)
{
	boolean wanted = g_view.set_kind == _ff_set_objective_generator && g_view.set_state == _ff_objective_running;

	if (wanted && (g_local_generator_scenery == NONE || !object_try_and_get(g_local_generator_scenery)))
	{
		long scenery_tag = firefight_generator_scenery_tag();
		struct object_placement_data placement;

		g_local_generator_scenery = NONE;
		if (scenery_tag == NONE)
			return;
		object_placement_data_new(&placement, scenery_tag, NONE);
		placement.position = g_view.set_site;
		g_local_generator_scenery = object_new(&placement);
		if (g_local_generator_scenery != NONE)
		{
			struct object_datum *scenery = object_get(g_local_generator_scenery);

			SET_FLAG(scenery->object.flags, _object_cannot_be_garbage_bit, TRUE);
			SET_FLAG(scenery->object.flags, _object_no_collisions_bit, TRUE);
		}
	}
	else if (!wanted && g_local_generator_scenery != NONE)
	{
		if (object_try_and_get(g_local_generator_scenery))
			object_delete(g_local_generator_scenery);
		g_local_generator_scenery = NONE;
	}
}

/* ---------- the set objective */

static void ffo_tokens_clear(void);
static boolean ffo_collect_start(void);
static void ffo_collect_update(void);
static boolean ffo_bomb_start(void);
static void ffo_bomb_update(void);

static void ffo_set_clear_units(void)
{
	ffo_vehicle_slots_update(TRUE);
	g.unit_count = 0;
	g.vehicle_count = 0;
	g.marine_count = 0;
	g.attackers_spawned = 0;
	g.target_unit = NONE;
	g.item_object = NONE;
	g.teleport_until = NONE;
	g.camp_aware = FALSE;
	g.camp_alarm_until = 0;
	g.token_count = 0;
	g.deposit_skull = NONE;
	g_view.deposit_object = NONE;
	g.bomb_planted = NONE;
	g.disarm_ticks = 0;
}

static void ffo_item_remove(void)
{
	struct object_datum *item = g.item_object != NONE ? object_try_and_get(g.item_object) : NULL;

	if (item)
	{
		/* (out of whoever's inventory it is in first: a rider's, put away) */
		struct data_iterator iterator;
		struct player_datum *player;

		data_iterator_new(&iterator, player_data);
		while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
		{
			if (player->unit_index != NONE && unit_try_and_get(player->unit_index) &&
				unit_has_weapon(player->unit_index, g.item_object))
			{
				unit_drop_weapon_object(player->unit_index, g.item_object);
			}
		}
		if (item->object.parent_object_index != NONE && unit_try_and_get(item->object.parent_object_index))
			unit_drop_weapon_object(item->object.parent_object_index, g.item_object);
		if (object_try_and_get(g.item_object))
			object_delete(g.item_object);
	}
	g.item_object = NONE;
}

static void ffo_set_end(byte state)
{
	char text[96];

	if (g_view.set_state != _ff_objective_running)
		return;
	g_view.set_state = state;
	g_view.set_result_ticks = FFO_RESULT_TICKS;
	g_view.set_ticks_left = NONE;
	g_view.set_carrier = NONE;
	if (state == _ff_objective_won)
	{
		ffo_award(g.set_contribution, FFO_SET_POINTS, TRUE, FALSE);
		snprintf(text, sizeof(text), "%s COMPLETE: +%d", ffo_set_names[g_view.set_kind], (int)FFO_SET_POINTS);
	}
	else
	{
		snprintf(text, sizeof(text), "%s FAILED", ffo_set_names[g_view.set_kind]);
	}
	ffo_notice(text, FFO_NOTICE_TICKS);
	switch (g_view.set_kind)
	{
	case _ff_set_objective_target:
	case _ff_set_objective_purge:
		if (state != _ff_objective_won)
			ffo_teleport_out();
		break;
	case _ff_set_objective_generator:
		ffo_generator_remove();
		if (state != _ff_objective_won)
			ffo_teleport_out();
		break;
	case _ff_set_objective_flag:
	case _ff_set_objective_oddball:
		ffo_item_remove();
		break;
	case _ff_set_objective_collect:
		ffo_tokens_clear();
		break;
	case _ff_set_objective_bomb:
		ffo_item_remove();
		if (g.bomb_planted != NONE && object_try_and_get(g.bomb_planted))
			object_delete(g.bomb_planted);
		g.bomb_planted = NONE;
		break;
	default:
		break;
	}
	g_view.set_target = NONE;
}

/* a camp round a site: its defenders, its vehicles (those the map places in
it), and its target (a VIP, a generator, or none) */
static boolean ffo_camp_start(short kind)
{
	/* (the VIP's and Purge's at a base, a flag's spot; the generator's
	anywhere) */
	boolean purge = kind == _ff_set_objective_purge;
	real radius = purge ? FFO_PURGE_RADIUS : FFO_CAMP_RADIUS;
	short site = kind == _ff_set_objective_generator ? ffo_pick_site(FFO_ANY_SITE, radius * 1.3f) :
		ffo_pick_base_site(radius * 1.3f);
	real_point3d center;
	real_point3d spots[FFO_MAX_VEHICLES];
	real facings[FFO_MAX_VEHICLES];
	short slots[FFO_MAX_VEHICLES];
	short vehicles;
	/* (Purge's: more of them, its vehicles at the map's vehicle spots) */
	short defenders = purge ? (short)MIN(16, 10 + 3 * MAX(0, g.counted_count - 1)) :
		(short)MIN(9, 5 + 2 * MAX(0, g.counted_count - 1));
	short index;
	short missed_spots = 0;
	short taken = 0;
	short made_vehicles = 0;

	if (site == NONE)
		return FALSE;
	center = g.sites[site].point;
	firefight_objective_ground(&center, &center);
	g_view.set_site = center;
	g_view.set_radius = radius;
	if (kind == _ff_set_objective_target)
	{
		long vip = firefight_objective_spawn_enemy(&center, ffo_rand_range(0.0f, 2.0f * _pi), _firefight_objective_rank_target);
		struct unit_datum *unit = ffo_living_unit(vip);

		if (!unit)
			return FALSE;
		/* (worth the trouble: shields to spare) */
		unit->object.maximum_shield_vitality = MAX(unit->object.maximum_shield_vitality, 75.0f) * 3.0f;
		unit->object.shield_vitality = 1.0f;
		g.target_unit = vip;
		ffo_unit_add(vip, _ffo_role_target);
	}
	else if (kind == _ff_set_objective_generator)
	{
		ffo_generator_spawn(&center, TRUE);
		if (g.target_unit == NONE)
			return FALSE;
	}
	for (index = 0; index < defenders; index++)
	{
		real_point3d spot;
		short rank = kind == _ff_set_objective_target || (index & 1) ? _firefight_objective_rank_heavy :
			_firefight_objective_rank_regular;

		if (!ffo_camp_spot(&center, 3.0f, radius * 0.75f, &spot))
			missed_spots++;
		ffo_unit_add(firefight_objective_spawn_enemy(&spot, ffo_rand_range(0.0f, 2.0f * _pi), rank), _ffo_role_defender);
	}
	/* (Purge's take the players' vehicles' spots in it too, those given
	back once its vehicle there is destroyed; then the map's free spots) */
	vehicles = purge ? firefight_objective_take_vehicle_spots(&center, radius, spots, facings, slots, FFO_MAX_VEHICLES) : 0;
	taken = vehicles;
	{
		real_point3d free_spots[FFO_MAX_VEHICLES];
		real free_facings[FFO_MAX_VEHICLES];
		short free_count = firefight_objective_vehicle_spots(&center, radius, free_spots, free_facings,
			purge ? FFO_MAX_VEHICLES : 2);
		short free_index;

		for (free_index = 0; free_index < free_count && vehicles < (purge ? FFO_MAX_VEHICLES : 2); free_index++)
		{
			short other;

			for (other = 0; other < vehicles && distance3d(&spots[other], &free_spots[free_index]) > 4.0f; other++)
				;
			if (other < vehicles)
				continue;
			spots[vehicles] = free_spots[free_index];
			facings[vehicles] = free_facings[free_index];
			slots[vehicles] = NONE;
			vehicles++;
		}
	}
	for (index = 0; index < vehicles; index++)
	{
		long vehicle = firefight_objective_spawn_vehicle(&spots[index], facings[index], index == 0);

		made_vehicles += vehicle != NONE;
		ffo_vehicle_add(vehicle, _ffo_role_defender, slots[index]);
	}
	/* (each vehicle not had, two more on foot: Purge's whole count, which
	no spot, a failed one or a faction with no drivers takes; the others'
	failed ones) */
	for (index = (purge ? FFO_MAX_VEHICLES : vehicles) - made_vehicles; index > 0; index--)
	{
		short extra;

		for (extra = 0; extra < 2; extra++)
		{
			real_point3d spot;

			if (!ffo_camp_spot(&center, 3.0f, radius * 0.75f, &spot))
				missed_spots++;
			ffo_unit_add(firefight_objective_spawn_enemy(&spot, ffo_rand_range(0.0f, 2.0f * _pi),
				(extra & 1) ? _firefight_objective_rank_heavy : _firefight_objective_rank_regular), _ffo_role_defender);
			defenders++;
		}
	}
	error(_error_silent, "firefight objectives: %s camp at (%.1f %.1f %.1f), radius %.0f: %d defenders (%d at the "
		"middle, no spot found), %d vehicle spots (%d of the players' taken), %d vehicles made",
		ffo_set_names[kind], center.x, center.y, center.z, radius, (int)defenders, (int)missed_spots, (int)vehicles,
		(int)taken, (int)made_vehicles);
	g_view.set_total = ffo_units_alive(0, TRUE);
	g_view.set_count = g_view.set_total;
	g_view.set_target = ffo_absolute_of_object(g.target_unit);
	/* (the round's time: it ends with the round anyway) */
	if (kind != _ff_set_objective_purge)
		g.set_deadline = game_time_get() + ffo_set_ticks();
	return g_view.set_total > 0 || g.target_unit != NONE;
}

/* (Data Link's: a squad sent for the hill, from out past it) */
static void ffo_raid(real_point3d const *at, short count, byte role)
{
	short index;

	for (index = 0; index < count; index++)
	{
		real_point3d spot;
		short slot;

		/* (room for them, and no more than so many at once) */
		if (g.unit_count >= FFO_MAX_UNITS)
			ffo_units_compact();
		if (g.unit_count >= FFO_MAX_UNITS || ffo_units_alive(role, FALSE) >= 8)
			break;
		ffo_spot_near(at, 12.0f, 20.0f, &spot);
		slot = ffo_unit_add(firefight_objective_spawn_enemy(&spot, 0.0f, _firefight_objective_rank_regular), role);
		if (slot != NONE)
			g.unit_next_order[slot] = game_time_get();
	}
}

static boolean ffo_data_link_start(void)
{
	short candidates[FFO_MAX_HILLS];
	short count = 0;
	short index;

	for (index = 0; index < g.hill_count; index++)
	{
		if (ffo_nearest_player_distance(&g.hills[index].center, NULL) > g.hills[index].radius + 6.0f &&
			!ffo_point_taken(&g.hills[index].center, TRUE))
		{
			candidates[count++] = index;
		}
	}
	for (index = 0; !count && index < g.hill_count; index++)
	{
		if (!ffo_point_taken(&g.hills[index].center, TRUE))
			candidates[count++] = index;
	}
	for (index = 0; !count && index < g.hill_count; index++)
		candidates[count++] = index;
	if (!count)
		return FALSE;
	g.hill_index = candidates[ffo_rand() % (unsigned long)count];
	g_view.set_site = g.hills[g.hill_index].center;
	g_view.set_radius = g.hills[g.hill_index].radius;
	g_view.set_goal = ffo_set_ticks() / 4;
	g_view.set_progress = 0;
	g.set_deadline = game_time_get() + ffo_set_ticks();
	/* (no squad of its own: the waves' enemies contest it) */
	return TRUE;
}

static boolean ffo_item_spawn(long definition, real_point3d const *at)
{
	struct object_placement_data placement;
	struct object_datum *item;

	if (definition == NONE)
		return FALSE;
	object_placement_data_new(&placement, definition, NONE);
	placement.position = *at;
	placement.position.z += 0.3f;
	g.item_object = object_new(&placement);
	if (g.item_object == NONE)
		return FALSE;
	item = object_get(g.item_object);
	SET_FLAG(item->object.flags, _object_cannot_be_garbage_bit, TRUE);
	g.item_home = *at;
	g.item_still_since = game_time_get();
	g_view.set_target = ffo_absolute_of_object(g.item_object);
	return TRUE;
}

static boolean ffo_flag_start(void)
{
	short from = (short)(ffo_rand() % (unsigned long)MAX(1, g.flag_count));
	short to = NONE;
	real farthest = 0.0f;
	short pass;
	short index;

	if (g.flag_count < 2)
		return FALSE;
	/* (the other base: the farthest of the other team's flags, else of any) */
	for (pass = 0; pass < 2 && to == NONE; pass++)
	{
		for (index = 0; index < g.flag_count; index++)
		{
			real distance = distance3d(&g.flags[index], &g.flags[from]);

			if (index == from || distance < 4.0f || (pass == 0 && g.flag_teams[index] == g.flag_teams[from]))
				continue;
			if (to == NONE || distance > farthest)
			{
				to = index;
				farthest = distance;
			}
		}
	}
	if (to == NONE || !ffo_item_spawn(get_flag_definition_index(), &g.flags[from]))
		return FALSE;
	g_view.set_site = g.flags[from];
	g_view.set_goal_point = g.flags[to];
	g_view.set_radius = FFO_CAPTURE_RADIUS;
	return TRUE;
}

static boolean ffo_rescue_start(void)
{
	short site = ffo_pick_base_site(FFO_RESCUE_ARRIVE * 2.0f);
	real_point3d center;
	short index;

	if (site == NONE || !firefight_objective_allies_available())
		return FALSE;
	center = g.sites[site].point;
	firefight_objective_ground(&center, &center);
	for (index = 0; index < FFO_MAX_MARINES; index++)
	{
		real_point3d spot;
		long marine;

		ffo_spot_near(&center, 1.5f, 4.0f, &spot);
		marine = firefight_objective_spawn_ally(&spot, ffo_rand_range(0.0f, 2.0f * _pi));
		if (marine != NONE && g.marine_count < FFO_MAX_MARINES)
		{
			g.marines[g.marine_count++] = marine;
			ffo_unit_add(marine, _ffo_role_marine);
		}
	}
	if (!g.marine_count)
		return FALSE;
	g_view.set_site = center;
	g_view.set_radius = FFO_RESCUE_ARRIVE;
	/* (their life drains at 40% of a hold's pace: 2.5 times as long) */
	g_view.set_goal = ffo_set_ticks() / 4 * 5 / 2;
	g_view.set_progress = g_view.set_goal;
	g_view.set_total = g.marine_count;
	g_view.set_count = g.marine_count;
	return TRUE;
}

static boolean ffo_oddball_start(void)
{
	real_point3d center;
	long boss;
	struct unit_datum *unit;

	if (!g.ball_count)
		return FALSE;
	center = g.balls[ffo_rand() % (unsigned long)g.ball_count];
	firefight_objective_ground(&center, &center);
	boss = firefight_objective_spawn_enemy(&center, ffo_rand_range(0.0f, 2.0f * _pi), _firefight_objective_rank_target);
	unit = ffo_living_unit(boss);
	if (!unit)
		return FALSE;
	unit->object.maximum_shield_vitality = MAX(unit->object.maximum_shield_vitality, 75.0f) * 2.0f;
	unit->object.shield_vitality = 1.0f;
	g.target_unit = boss;
	ffo_unit_add(boss, _ffo_role_boss);
	g_view.set_site = center;
	g_view.set_radius = 6.0f;
	g_view.set_goal = ffo_set_ticks() / 4;
	g_view.set_progress = 0;
	g_view.set_target = ffo_absolute_of_object(boss);
	return TRUE;
}

static void ffo_set_start(short kind)
{
	boolean started = FALSE;
	char text[96];

	ffo_set_clear_units();
	csmemset(g.set_contribution, 0, sizeof(g.set_contribution));
	g_view.set_kind = (byte)kind;
	g_view.set_state = _ff_objective_running;
	g_view.set_flags = 0;
	g_view.set_target = NONE;
	g_view.set_carrier = NONE;
	g_view.set_progress = 0;
	g_view.set_goal = 0;
	g_view.set_count = 0;
	g_view.set_total = 0;
	g_view.set_result_ticks = 0;
	g_view.set_radius = 0.0f;
	g.set_deadline = NONE;
	g.last_raid_tick = game_time_get();
	switch (kind)
	{
	case _ff_set_objective_target:
	case _ff_set_objective_generator:
	case _ff_set_objective_purge:
		started = ffo_camp_start(kind);
		break;
	case _ff_set_objective_data_link:
		started = ffo_data_link_start();
		break;
	case _ff_set_objective_flag:
		started = ffo_flag_start();
		break;
	case _ff_set_objective_rescue:
		started = ffo_rescue_start();
		break;
	case _ff_set_objective_oddball:
		started = ffo_oddball_start();
		break;
	case _ff_set_objective_collect:
		started = ffo_collect_start();
		break;
	case _ff_set_objective_bomb:
		started = ffo_bomb_start();
		break;
	default:
		break;
	}
	if (!started)
	{
		error(_error_silent, "firefight objectives: %s could not start here", ffo_set_names[kind]);
		ffo_teleport_out();
		ffo_generator_remove();
		ffo_item_remove();
		g_view.set_kind = _ff_set_objective_none;
		g_view.set_state = _ff_objective_off;
		return;
	}
	snprintf(text, sizeof(text), "ROUND OBJECTIVE: %s", ffo_set_names[kind]);
	ffo_notice(text, FFO_NOTICE_TICKS + 2 * TICKS_PER_SECOND);
}

static void ffo_set_roll_for_round(void)
{
	long chance = PIN(config_integer("game.firefight_objectives_set_chance"), 0, 100);

	g.set_start_round = 0;
	if (!config_boolean("game.firefight_objectives_set") || (long)(ffo_rand() % 100UL) >= chance)
		return;
	g.set_start_round = (short)(1 + ffo_rand() % 2UL);
}

static void ffo_set_roll_start(void)
{
	short candidates[NUMBER_OF_FF_SET_OBJECTIVES];
	short count = 0;
	short kind;

	for (kind = 1; kind < NUMBER_OF_FF_SET_OBJECTIVES; kind++)
	{
		if (config_boolean(ffo_set_settings[kind]) && (g.set_available & (1UL << kind)))
			candidates[count++] = kind;
	}
	while (count > 0)
	{
		short pick = (short)(ffo_rand() % (unsigned long)count);

		ffo_set_start(candidates[pick]);
		if (g_view.set_state == _ff_objective_running)
			return;
		candidates[pick] = candidates[--count];
	}
}

/* (each unit's orders, in turn: defenders patrol their camp, the VIP keeps
to its middle, attackers go for the hill or the carrier, marines keep to
their spot and, rescued, to a player) */
static void ffo_units_update(void)
{
	long now = game_time_get();
	long carrier = firefight_objectives_priority_target();
	short index;

	for (index = 0; index < g.unit_count; index++)
	{
		long unit_index = g.units[index];
		struct unit_datum *unit = ffo_living_unit(unit_index);
		real_point3d at;
		real from_site;

		if (!unit || now < g.unit_next_order[index] || g.teleport_until != NONE)
			continue;
		g.unit_next_order[index] = now + FFO_ORDER_TICKS + (long)(ffo_rand() % (unsigned long)FFO_ORDER_TICKS);
		object_get_origin(unit_index, &at);
		from_site = distance3d(&at, &g_view.set_site);
		switch (g.unit_roles[index])
		{
		case _ffo_role_target:
			if (from_site > 2.5f)
				ffo_order_move(unit_index, &g_view.set_site);
			break;
		case _ffo_role_defender:
			if (carrier != NONE && unit->unit.actor_index != NONE)
			{
				firefight_objective_hold_ground(unit->unit.actor_index, FALSE);
				firefight_objective_direct_at(unit->unit.actor_index, carrier);
			}
			/* (kept to the camp: out of it, back, fighting or not; in it,
			about it now and then while nothing is in sight) */
			else
			{
				if (unit->unit.actor_index != NONE)
					firefight_objective_hold_ground(unit->unit.actor_index, TRUE);
				if (from_site > g_view.set_radius * 0.8f ||
					(!ffo_actor_fighting(unit_index) && (ffo_rand() % (g.camp_aware ? 5UL : 3UL)) == 0))
				{
					real_point3d spot;

					ffo_camp_spot(&g_view.set_site, 2.0f, g_view.set_radius * 0.7f, &spot);
					ffo_order_move(unit_index, &spot);
					/* (sooner back while it is out) */
					if (from_site > g_view.set_radius * 0.8f)
						g.unit_next_order[index] = now + FFO_ORDER_TICKS / 2;
				}
			}
			break;
		case _ffo_role_attacker:
			if (carrier != NONE && unit->unit.actor_index != NONE)
				firefight_objective_direct_at(unit->unit.actor_index, carrier);
			else if (g_view.set_kind == _ff_set_objective_rescue && g.marine_count > 0 && unit->unit.actor_index != NONE)
			{
				long marine = g.marines[ffo_rand() % (unsigned long)g.marine_count];

				if (ffo_living_unit(marine))
					firefight_objective_direct_at(unit->unit.actor_index, marine);
			}
			else if (from_site > 2.0f && !ffo_actor_fighting(unit_index))
				ffo_order_move(unit_index, &g_view.set_site);
			break;
		case _ffo_role_raider:
		case _ffo_role_boss:
			if (carrier != NONE && unit->unit.actor_index != NONE)
				firefight_objective_direct_at(unit->unit.actor_index, carrier);
			else if (g.unit_roles[index] == _ffo_role_boss && from_site > 8.0f && !ffo_actor_fighting(unit_index))
				ffo_order_move(unit_index, &g_view.set_site);
			break;
		case _ffo_role_marine:
			/* (a player come for them: with the nearest player from then, as
			Forge's FOLLOW; before, at their spot) */
			if (g_view.set_flags & (FF_OBJECTIVE_ARRIVED | FF_OBJECTIVE_RESCUED))
				firefight_objective_follow(unit_index);
			else if (from_site > 5.0f)
				ffo_order_move(unit_index, &g_view.set_site);
			break;
		default:
			break;
		}
	}
}

/* the players in a hill and whether any enemy is */
static void ffo_hill_occupants(struct ffo_hill const *hill, short *players, boolean *enemies, long *inside, short maximum)
{
	struct object_iterator iterator;
	struct object_datum *object;

	*players = 0;
	*enemies = FALSE;
	object_iterator_new(&iterator, _object_mask_unit, 0);
	while ((object = (struct object_datum *)object_iterator_next(&iterator)) != NULL)
	{
		struct unit_datum *unit = (struct unit_datum *)object;

		if (TEST_FLAG(object->object.damage_flags, _object_dead_bit) || !ffo_in_hill(hill, &object->object.position))
			continue;
		if (unit->unit.player_index != NONE)
		{
			if (*players < maximum)
				inside[*players] = unit->unit.player_index;
			(*players)++;
		}
		else if (iterator.index != g.target_unit && unit->unit.actor_index != NONE &&
			firefight_team_is_hostile(object->object.owner_team_index))
		{
			*enemies = TRUE;
		}
	}
}

static void ffo_data_link_update(void)
{
	struct ffo_hill const *hill = &g.hills[PIN(g.hill_index, 0, FFO_MAX_HILLS - 1)];
	long inside[16];
	short players;
	boolean enemies;
	short index;

	ffo_hill_occupants(hill, &players, &enemies, inside, 16);
	g_view.set_flags &= ~(FF_OBJECTIVE_CONTESTED | FF_OBJECTIVE_HELD);
	if (players > 0 && enemies)
		g_view.set_flags |= FF_OBJECTIVE_CONTESTED;
	else if (players > 0)
	{
		g_view.set_flags |= FF_OBJECTIVE_HELD;
		g_view.set_progress++;
		for (index = 0; index < MIN(players, 16); index++)
		{
			long absolute = ffo_absolute(inside[index]);

			if (absolute >= 0 && absolute < HALO_PORT_MAXIMUM_NETWORK_PLAYERS)
				g.set_contribution[absolute]++;
		}
	}
	if (g_view.set_progress >= g_view.set_goal)
		ffo_set_end(_ff_objective_won);
}

/* (the flag's or the ball's: who has it, and back home when it lies too long
or is gone) */
/* (the player whose unit has it, in hand or put away: riding in a vehicle,
a carried flag is out of the hands and off the map, its place where it was,
as CTF's ctf_find_flag_carrier knows) */
static long ffo_item_carrier(void)
{
	struct object_datum *item = g.item_object != NONE ? object_try_and_get(g.item_object) : NULL;
	struct data_iterator iterator;
	struct player_datum *player;

	if (!item || !player_data)
		return NONE;
	data_iterator_new(&iterator, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
	{
		if (ffo_living_unit(player->unit_index) && unit_has_weapon(player->unit_index, g.item_object))
			return iterator.datum_index;
	}
	return NONE;
}

static void ffo_item_update(long definition, long return_ticks)
{
	struct object_datum *item = g.item_object != NONE ? object_try_and_get(g.item_object) : NULL;
	long now = game_time_get();

	if (!item)
	{
		ffo_item_spawn(definition, &g.item_home);
		return;
	}
	if (item->object.parent_object_index != NONE || ffo_item_carrier() != NONE)
	{
		g.item_still_since = now;
		return;
	}
	if (now - g.item_still_since > return_ticks)
	{
		object_delete(g.item_object);
		g.item_object = NONE;
		ffo_item_spawn(definition, &g.item_home);
	}
}

static void ffo_flag_update(void)
{
	long carrier = ffo_item_carrier();
	struct player_datum *player = carrier != NONE ? player_try_and_get(carrier) : NULL;

	ffo_item_update(get_flag_definition_index(), FFO_ITEM_RETURN_TICKS);
	g_view.set_carrier = carrier != NONE ? (short)ffo_absolute(carrier) : NONE;
	g_view.set_target = ffo_absolute_of_object(g.item_object);
	if (!player || player->unit_index == NONE)
		return;
	{
		long absolute = ffo_absolute(carrier);
		real_point3d at;

		if (absolute >= 0 && absolute < HALO_PORT_MAXIMUM_NETWORK_PLAYERS)
			g.set_contribution[absolute]++;
		object_get_origin(player->unit_index, &at);
		if (distance2d((real_point2d const *)&at, (real_point2d const *)&g_view.set_goal_point) < FFO_CAPTURE_RADIUS &&
			fabs(at.z - g_view.set_goal_point.z) < 3.0f)
		{
			if (absolute >= 0 && absolute < HALO_PORT_MAXIMUM_NETWORK_PLAYERS)
				g.set_contribution[absolute] += FFO_CAPTURE_BONUS_TICKS;
			ffo_set_end(_ff_objective_won);
		}
	}
}

static void ffo_rescue_update(void)
{
	short alive = 0;
	short index;

	for (index = 0; index < g.marine_count; index++)
	{
		if (ffo_living_unit(g.marines[index]))
			alive++;
	}
	g_view.set_count = alive;
	if (!alive)
	{
		ffo_set_end(_ff_objective_failed);
		return;
	}
	if (!(g_view.set_flags & FF_OBJECTIVE_ARRIVED))
	{
		short attackers;

		if (ffo_nearest_player_distance(&g_view.set_site, NULL) > FFO_RESCUE_ARRIVE)
		{
			/* (their patience running out: then they're gone) */
			if (--g_view.set_progress <= 0)
			{
				for (index = 0; index < g.marine_count; index++)
				{
					if (ffo_living_unit(g.marines[index]))
						unit_kill(g.marines[index]);
				}
				g_view.set_count = 0;
				ffo_set_end(_ff_objective_failed);
			}
			return;
		}
		g_view.set_flags |= FF_OBJECTIVE_ARRIVED;
		attackers = (short)MIN(8, 4 + g.counted_count);
		ffo_raid(&g_view.set_site, attackers, _ffo_role_attacker);
		g.attackers_spawned = ffo_units_alive(_ffo_role_attacker, FALSE);
		if (!g.attackers_spawned)
			g.attackers_spawned = 1;
		return;
	}
	if (!(g_view.set_flags & FF_OBJECTIVE_RESCUED) && ffo_units_alive(_ffo_role_attacker, FALSE) == 0)
	{
		/* (done: each marine alive paid for now, and with the players from
		now, as the friendly reinforcements are) */
		short alive = 0;

		g_view.set_flags |= FF_OBJECTIVE_RESCUED;
		for (index = 0; index < g.marine_count; index++)
		{
			if (ffo_living_unit(g.marines[index]))
			{
				alive++;
				firefight_adopt_friendly(g.marines[index]);
			}
		}
		ffo_set_end(_ff_objective_won);
		if (alive)
		{
			char text[96];

			ffo_award(g.set_contribution, (long)alive * FFO_MARINE_POINTS, FALSE, FALSE);
			snprintf(text, sizeof(text), "%d MARINES RESCUED: +%d", (int)alive, (int)(alive * FFO_MARINE_POINTS));
			ffo_notice(text, FFO_NOTICE_TICKS);
		}
		g.marine_count = 0;
	}
}

static void ffo_oddball_update(void)
{
	long now = game_time_get();
	long carrier;

	if (g.item_object == NONE)
	{
		/* (the boss alive: nothing to hold yet; gone with no kill told, its
		skull where the boss was put) */
		if (g.target_unit != NONE && ffo_living_unit(g.target_unit))
			return;
		g.target_unit = NONE;
		if (!ffo_item_spawn(get_ball_definition_index(), &g_view.set_site))
			return;
	}
	ffo_item_update(get_ball_definition_index(), FFO_BALL_RETURN_TICKS);
	carrier = ffo_item_carrier();
	g_view.set_carrier = carrier != NONE ? (short)ffo_absolute(carrier) : NONE;
	g_view.set_target = ffo_absolute_of_object(g.item_object);
	g_view.set_flags &= ~FF_OBJECTIVE_HELD;
	if (carrier == NONE)
		return;
	g_view.set_flags |= FF_OBJECTIVE_HELD;
	g_view.set_progress++;
	{
		long absolute = ffo_absolute(carrier);
		struct player_datum *player = player_try_and_get(carrier);

		if (absolute >= 0 && absolute < HALO_PORT_MAXIMUM_NETWORK_PLAYERS)
			g.set_contribution[absolute]++;
		/* (more of them, for the carrier) */
		if (player && player->unit_index != NONE && now - g.last_raid_tick >= FFO_ODDBALL_SPAWN_TICKS)
		{
			real_point3d at;

			g.last_raid_tick = now;
			object_get_origin(player->unit_index, &at);
			ffo_raid(&at, 2, _ffo_role_raider);
		}
	}
	if (g_view.set_progress >= g_view.set_goal)
		ffo_set_end(_ff_objective_won);
}

static void ffo_camp_update(void)
{
	short alive = ffo_units_alive(0, TRUE);
	long now = game_time_get();
	short index;

	g_view.set_count = alive;
	ffo_vehicle_slots_update(FALSE);
	/* (noticing the players: one near the camp, or any of them hurt lately,
	which keeps them on it a while) */
	for (index = 0; index < g.unit_count; index++)
	{
		struct unit_datum *unit = ffo_living_unit(g.units[index]);

		if (unit && g.unit_roles[index] != _ffo_role_marine &&
			(unit->object.recent_body_damage > 0.0f || unit->object.recent_shield_damage > 0.0f))
		{
			g.camp_alarm_until = now + 10 * TICKS_PER_SECOND;
		}
	}
	g.camp_aware = now < g.camp_alarm_until ||
		ffo_nearest_player_distance(&g_view.set_site, NULL) <= g_view.set_radius + FFO_CAMP_NOTICE_MARGIN;
	if (g_view.set_kind == _ff_set_objective_generator)
	{
		struct object_datum *object = g.target_unit != NONE ? object_try_and_get(g.target_unit) : NULL;

		if (object)
		{
			/* (held where it is, as Generator Defense's are) */
			object->object.position = g_view.set_site;
			object->object.translational_velocity.i = 0.0f;
			object->object.translational_velocity.j = 0.0f;
			object->object.translational_velocity.k = 0.0f;
			SET_FLAG(object->object.damage_flags, _object_dead_bit, FALSE);
			object->object.body_vitality = (real)MAX(g_view.set_progress, 1) / (real)MAX(g_view.set_goal, 1);
		}
		else
		{
			/* (gone: made again, its damage kept) */
			if (g.generator_scenery != NONE && object_try_and_get(g.generator_scenery))
				object_delete(g.generator_scenery);
			ffo_generator_spawn(&g_view.set_site, FALSE);
		}
		g_view.set_target = ffo_absolute_of_object(g.target_unit);
	}
	else if (g_view.set_kind == _ff_set_objective_purge && alive == 0)
	{
		ffo_set_end(_ff_objective_won);
	}
}

static void ffo_set_update(void)
{
	long now = game_time_get();

	ffo_teleport_update();
	if (g_view.set_state != _ff_objective_running)
		return;
	g_view.set_ticks_left = g.set_deadline != NONE ? MAX(0L, g.set_deadline - now) : NONE;
	if (g.set_deadline != NONE && now >= g.set_deadline)
	{
		ffo_set_end(_ff_objective_failed);
		return;
	}
	switch (g_view.set_kind)
	{
	case _ff_set_objective_target:
	case _ff_set_objective_generator:
	case _ff_set_objective_purge:
		ffo_camp_update();
		break;
	case _ff_set_objective_data_link:
		ffo_data_link_update();
		break;
	case _ff_set_objective_flag:
		ffo_flag_update();
		break;
	case _ff_set_objective_rescue:
		ffo_rescue_update();
		break;
	case _ff_set_objective_oddball:
		ffo_oddball_update();
		break;
	case _ff_set_objective_collect:
		ffo_collect_update();
		break;
	case _ff_set_objective_bomb:
		ffo_bomb_update();
		break;
	default:
		break;
	}
	if (g_view.set_state == _ff_objective_running)
		ffo_units_update();
}

/* ---------- Travel (a round's): a counted player at its point */

static void ffo_round_travel_update(void)
{
	long players[16];
	real_point3d positions[16];
	short count = ffo_living_players(players, NULL, positions, 16);
	short index;

	for (index = 0; index < count; index++)
	{
		if (!ffo_counted(players[index]) ||
			distance2d((real_point2d const *)&positions[index], (real_point2d const *)&g_view.round_point) > FFO_TRAVEL_RADIUS ||
			fabs(positions[index].z - g_view.round_point.z) > 3.0f)
		{
			continue;
		}
		g.round_contribution[ffo_absolute(players[index])] = 1;
		ffo_round_end(_ff_objective_won);
		return;
	}
}

/* ---------- Collect and Bomb (set objectives) */

/* the living players and hostile AI within `radius` of a point */
static void ffo_count_near(real_point3d const *point, real radius, short *players, short *enemies)
{
	struct object_iterator iterator;
	struct object_datum *object;

	*players = 0;
	*enemies = 0;
	object_iterator_new(&iterator, _object_mask_unit, 0);
	while ((object = (struct object_datum *)object_iterator_next(&iterator)) != NULL)
	{
		struct unit_datum *unit = (struct unit_datum *)object;

		if (TEST_FLAG(object->object.damage_flags, _object_dead_bit) || iterator.index == g.target_unit ||
			distance3d(&object->object.position, point) > radius)
		{
			continue;
		}
		if (unit->unit.player_index != NONE)
			(*players)++;
		else if (unit->unit.actor_index != NONE && firefight_team_is_hostile(object->object.owner_team_index))
			(*enemies)++;
	}
}

/* (Collect's deposit, a hill's shape or round a flag's spot) */
static boolean ffo_in_deposit(real_point3d const *point)
{
	if (g.hill_index >= 0 && g.hill_index < g.hill_count)
		return ffo_in_hill(&g.hills[g.hill_index], point);
	return distance2d((real_point2d const *)point, (real_point2d const *)&g_view.set_site) < g_view.set_radius &&
		fabs(point->z - g_view.set_site.z) < 3.0f;
}

/* a skull on the ground worth `value` (the oldest gone for it, if too many),
`dropped` a player's who died, which is marked */
static void ffo_token_spawn(real_point3d const *at, short value, boolean dropped)
{
	long definition = get_ball_definition_index();
	struct object_placement_data placement;
	long object_index;

	if (definition == NONE || value <= 0)
		return;
	if (g.token_count >= FFO_MAX_TOKENS)
	{
		short index;

		if (object_try_and_get(g.tokens[0]))
			object_delete(g.tokens[0]);
		for (index = 1; index < g.token_count; index++)
		{
			g.tokens[index - 1] = g.tokens[index];
			g.token_values[index - 1] = g.token_values[index];
			g.token_times[index - 1] = g.token_times[index];
			g.token_dropped[index - 1] = g.token_dropped[index];
		}
		g.token_count--;
	}
	object_placement_data_new(&placement, definition, NONE);
	placement.position = *at;
	placement.position.z += 0.4f;
	object_index = object_new(&placement);
	if (object_index == NONE)
		return;
	SET_FLAG(object_get(object_index)->object.flags, _object_cannot_be_garbage_bit, TRUE);
	g.tokens[g.token_count] = object_index;
	g.token_values[g.token_count] = value;
	g.token_times[g.token_count] = game_time_get();
	g.token_dropped[g.token_count] = dropped;
	g.token_count++;
}

static void ffo_tokens_clear(void)
{
	short index;

	for (index = 0; index < g.token_count; index++)
	{
		if (object_try_and_get(g.tokens[index]))
			object_delete(g.tokens[index]);
	}
	g.token_count = 0;
	csmemset(g_view.carried, 0, sizeof(g_view.carried));
	if (g.deposit_skull != NONE && object_try_and_get(g.deposit_skull))
		object_delete(g.deposit_skull);
	g.deposit_skull = NONE;
	g_view.deposit_object = NONE;
}

/* (Collect's deposit: a skull on its spot, never taken, made again when
gone, back on its spot when knocked off it; each machine's fire round it,
firefight_objectives_update) */
static void ffo_deposit_update(void)
{
	struct object_datum *skull = g.deposit_skull != NONE ? object_try_and_get(g.deposit_skull) : NULL;
	real_point3d spot = g_view.set_site;

	firefight_objective_ground(&spot, &spot);
	if (!skull)
	{
		long definition = get_ball_definition_index();
		struct object_placement_data placement;

		g.deposit_skull = NONE;
		if (definition == NONE)
			return;
		object_placement_data_new(&placement, definition, NONE);
		placement.position = spot;
		placement.position.z += 0.3f;
		g.deposit_skull = object_new(&placement);
		if (g.deposit_skull == NONE)
			return;
		SET_FLAG(object_get(g.deposit_skull)->object.flags, _object_cannot_be_garbage_bit, TRUE);
	}
	else if (skull->object.parent_object_index == NONE && distance3d(&skull->object.position, &spot) > 1.5f)
	{
		real_point3d at = spot;

		at.z += 0.3f;
		object_set_position(g.deposit_skull, &at, NULL, NULL);
	}
	g_view.deposit_object = ffo_absolute_of_object(g.deposit_skull);
}

/* (Collect's) how many skulls an enemy drops: by its kind (grunts, jackals,
sentinels, carrier forms 1; elites, marines, combat forms 2; hunters 3;
infection forms none) and one more for each rank past minor (major 1,
ultra / spec ops / stealth 2, commander / zealot 3), from its actor's kind and
its variant's name, else its unit's name */
static short ffo_skull_value(long actor_index, long unit_index)
{
	struct actor_datum *actor = actor_index != NONE ? actor_try_and_get(actor_index) : NULL;
	struct object_datum *unit = unit_index != NONE ? object_try_and_get(unit_index) : NULL;
	char name[256];
	short type = NONE;
	short value = 1;
	short index;

	name[0] = 0;
	if (actor)
	{
		struct actor_definition *definition = actor->meta.definition_index != NONE ?
			actor_definition_get(actor->meta.definition_index) : NULL;

		type = definition ? definition->type : NONE;
		if (actor->meta.variant_definition_index != NONE)
			snprintf(name, sizeof(name), "%s", tag_get_name(actor->meta.variant_definition_index));
	}
	if (!name[0] && unit)
		snprintf(name, sizeof(name), "%s", tag_get_name(unit->definition_index));
	for (index = 0; name[index]; index++)
		name[index] = (char)tolower((unsigned char)name[index]);
	if (type == NONE)
	{
		type = strstr(name, "infection") ? _actor_infection_form : strstr(name, "hunter") ? _actor_hunter :
			strstr(name, "carrier") ? _actor_carrier_form : strstr(name, "flood") ? _actor_combat_form :
			strstr(name, "elite") ? _actor_elite : strstr(name, "marine") ? _actor_marine : _actor_grunt;
	}
	switch (type)
	{
	case _actor_infection_form:
		return 0;
	case _actor_hunter:
		value = 3;
		break;
	case _actor_elite:
	case _actor_marine:
	case _actor_crew:
	case _actor_combat_form:
		value = 2;
		break;
	default:
		value = 1;
		break;
	}
	if (strstr(name, "commander") || strstr(name, "zealot") || strstr(name, "captain"))
		value += 3;
	else if (strstr(name, "ultra") || strstr(name, "specops") || strstr(name, "spec ops") || strstr(name, "stealth"))
		value += 2;
	else if (strstr(name, "major"))
		value += 1;
	return value;
}

static boolean ffo_collect_start(void)
{
	if (get_ball_definition_index() == NONE || (!g.hill_count && !g.flag_count))
		return FALSE;
	/* (a hill, else a flag's spot: not Travel's) */
	if (g.hill_count)
	{
		short tries;

		g.hill_index = (long)(ffo_rand() % (unsigned long)g.hill_count);
		for (tries = 0; tries < g.hill_count && ffo_point_taken(&g.hills[g.hill_index].center, TRUE); tries++)
			g.hill_index = (g.hill_index + 1) % g.hill_count;
		g_view.set_site = g.hills[g.hill_index].center;
		g_view.set_radius = g.hills[g.hill_index].radius;
	}
	else
	{
		short flag = (short)(ffo_rand() % (unsigned long)g.flag_count);
		short tries;

		for (tries = 0; tries < g.flag_count && ffo_point_taken(&g.flags[flag], TRUE); tries++)
			flag = (short)((flag + 1) % g.flag_count);
		g.hill_index = NONE;
		g_view.set_site = g.flags[flag];
		g_view.set_radius = FFO_CAPTURE_RADIUS;
	}
	/* (more than one a kill now, by the enemies' kinds and ranks: ffo_skull_value) */
	g_view.set_goal = MIN(150L, 50L + 20L * MAX(0, g.counted_count - 1));
	g_view.set_progress = 0;
	ffo_tokens_clear();
	ffo_deposit_update();
	return TRUE;
}

static void ffo_collect_update(void)
{
	long now = game_time_get();
	long players[16];
	long units[16];
	real_point3d positions[16];
	short count = ffo_living_players(players, units, positions, 16);
	short index;

	if (now % 3)
		return;
	ffo_deposit_update();
	/* (skulls lying too long go; one walked into is taken) */
	for (index = 0; index < g.token_count; index++)
	{
		struct object_datum *token = object_try_and_get(g.tokens[index]);
		short taker = NONE;
		short p;

		if (token)
		{
			for (p = 0; p < count && taker == NONE; p++)
			{
				if (distance3d(&positions[p], &token->object.position) < FFO_TOKEN_REACH + 0.6f)
					taker = p;
			}
		}
		if (token && taker == NONE && now - g.token_times[index] < (g.token_dropped[index] ? 2 : 1) * FFO_TOKEN_TICKS)
			continue;
		if (taker != NONE)
		{
			long absolute = ffo_absolute(players[taker]);

			if (absolute >= 0 && absolute < HALO_PORT_MAXIMUM_NETWORK_PLAYERS)
				g_view.carried[absolute] = (byte)MIN(255, g_view.carried[absolute] + g.token_values[index]);
		}
		if (token)
			object_delete(g.tokens[index]);
		g.tokens[index] = g.tokens[--g.token_count];
		g.token_values[index] = g.token_values[g.token_count];
		g.token_times[index] = g.token_times[g.token_count];
		g.token_dropped[index] = g.token_dropped[g.token_count];
		index--;
	}
	/* (what a carrier brings to the deposit, banked) */
	for (index = 0; index < count; index++)
	{
		long absolute = ffo_absolute(players[index]);

		if (absolute < 0 || absolute >= HALO_PORT_MAXIMUM_NETWORK_PLAYERS || !g_view.carried[absolute] ||
			!ffo_in_deposit(&positions[index]))
		{
			continue;
		}
		{
			char name[48];
			char text[96];
			short banked = g_view.carried[absolute];

			g_view.set_progress += banked;
			g.set_contribution[absolute] += banked;
			g_view.carried[absolute] = 0;
			ffo_player_name(players[index], name, sizeof(name));
			snprintf(text, sizeof(text), "%s BANKED %d (%ld / %ld)", name, (int)banked, g_view.set_progress,
				g_view.set_goal);
			ffo_notice(text, FFO_NOTICE_TICKS);
		}
	}
	(void)units;
	if (g_view.set_progress >= g_view.set_goal)
		ffo_set_end(_ff_objective_won);
}

static boolean ffo_bomb_start(void)
{
	short from = NONE;
	short to;
	real nearest = 0.0f;
	short index;

	if (get_ball_definition_index() == NONE || g.site_count < 2)
		return FALSE;
	/* (it waits by the players, and goes far from them) */
	for (index = 0; index < g.site_count; index++)
	{
		real distance = ffo_nearest_player_distance(&g.sites[index].point, NULL);

		if (from == NONE || distance < nearest)
		{
			from = index;
			nearest = distance;
		}
	}
	to = ffo_pick_site(FFO_ANY_SITE, 20.0f);
	if (to == NONE || to == from || distance3d(&g.sites[to].point, &g.sites[from].point) < 10.0f)
	{
		real farthest = 0.0f;

		to = NONE;
		for (index = 0; index < g.site_count; index++)
		{
			real distance = distance3d(&g.sites[index].point, &g.sites[from].point);

			if (index != from && distance > farthest)
			{
				to = index;
				farthest = distance;
			}
		}
	}
	if (to == NONE)
		return FALSE;
	g.bomb_home = g.sites[from].point;
	firefight_objective_ground(&g.bomb_home, &g.bomb_home);
	if (!ffo_item_spawn(get_ball_definition_index(), &g.bomb_home))
		return FALSE;
	g_view.set_site = g.sites[to].point;
	firefight_objective_ground(&g_view.set_site, &g_view.set_site);
	g_view.set_radius = FFO_BOMB_RADIUS;
	g_view.set_goal = ffo_set_ticks() / 6;
	g_view.set_progress = 0;
	g_view.set_total = FFO_DISARM_TICKS;
	g_view.set_count = 0;
	return TRUE;
}

static char const *const ffo_blast_effects[] =
{
	"vehicles\\scorpion\\effects\\scorpion shell explosion",
	"vehicles\\scorpion\\effects\\shell explosion",
	"effects\\explosions\\large explosion",
	"vehicles\\wraith\\effects\\wraith explosion",
	"effects\\explosions\\medium explosion",
	"weapons\\rocket launcher\\effects\\impact explosion"
};

/* a big blast's looks on this machine (each machine's own): at its middle
and round it */
static void ffo_big_blast(real_point3d const *at)
{
	short index;

	firefight_objective_effect(ffo_blast_effects, (short)NUMBEROF(ffo_blast_effects), at);
	for (index = 0; index < 4; index++)
	{
		real angle = (real)index * 0.5f * _pi;
		real_point3d around = *at;

		around.x += cosine(angle) * 3.0f;
		around.y += sine(angle) * 3.0f;
		firefight_objective_effect(ffo_blast_effects, (short)NUMBEROF(ffo_blast_effects), &around);
	}
}

static void ffo_bomb_update(void)
{
	long now = game_time_get();

	if (!(g_view.set_flags & FF_OBJECTIVE_PLANTED))
	{
		long carrier;

		ffo_item_update(get_ball_definition_index(), FFO_ITEM_RETURN_TICKS);
		g.item_home = g.bomb_home;
		carrier = ffo_item_carrier();
		g_view.set_carrier = carrier != NONE ? (short)ffo_absolute(carrier) : NONE;
		g_view.set_target = ffo_absolute_of_object(g.item_object);
		if (carrier == NONE)
			return;
		{
			long absolute = ffo_absolute(carrier);
			struct player_datum *player = player_try_and_get(carrier);
			real_point3d at;
			struct object_placement_data placement;

			if (absolute >= 0 && absolute < HALO_PORT_MAXIMUM_NETWORK_PLAYERS)
				g.set_contribution[absolute]++;
			if (!player || player->unit_index == NONE)
				return;
			object_get_origin(player->unit_index, &at);
			if (distance2d((real_point2d const *)&at, (real_point2d const *)&g_view.set_site) > g_view.set_radius ||
				fabs(at.z - g_view.set_site.z) > 3.0f)
			{
				return;
			}
			/* (planted: out of the carrier's hands, on the spot) */
			ffo_item_remove();
			object_placement_data_new(&placement, get_ball_definition_index(), NONE);
			placement.position = g_view.set_site;
			placement.position.z += 0.3f;
			g.bomb_planted = object_new(&placement);
			if (g.bomb_planted != NONE)
				SET_FLAG(object_get(g.bomb_planted)->object.flags, _object_cannot_be_garbage_bit, TRUE);
			if (absolute >= 0 && absolute < HALO_PORT_MAXIMUM_NETWORK_PLAYERS)
				g.set_contribution[absolute] += FFO_CAPTURE_BONUS_TICKS;
			g_view.set_flags |= FF_OBJECTIVE_PLANTED;
			g_view.set_carrier = NONE;
			g_view.set_target = ffo_absolute_of_object(g.bomb_planted);
			g_view.set_progress = 0;
			g.disarm_ticks = 0;
			g.last_raid_tick = now;
			ffo_notice("BOMB PLANTED: DEFEND IT", FFO_NOTICE_TICKS);
		}
		return;
	}
	/* planted: arming, the enemies at it, its defenders */
	if (g.bomb_planted == NONE || !object_try_and_get(g.bomb_planted))
	{
		struct object_placement_data placement;

		object_placement_data_new(&placement, get_ball_definition_index(), NONE);
		placement.position = g_view.set_site;
		placement.position.z += 0.3f;
		g.bomb_planted = object_new(&placement);
		g_view.set_target = ffo_absolute_of_object(g.bomb_planted);
	}
	g_view.set_progress++;
	{
		short players;
		short enemies;
		long defenders[16];
		real_point3d positions[16];
		short count = ffo_living_players(defenders, NULL, positions, 16);
		short index;

		for (index = 0; index < count; index++)
		{
			long absolute = ffo_absolute(defenders[index]);

			if (distance3d(&positions[index], &g_view.set_site) < 8.0f &&
				absolute >= 0 && absolute < HALO_PORT_MAXIMUM_NETWORK_PLAYERS)
			{
				g.set_contribution[absolute]++;
			}
		}
		ffo_count_near(&g_view.set_site, FFO_DISARM_RADIUS, &players, &enemies);
		g_view.set_flags &= ~FF_OBJECTIVE_DISARMING;
		if (enemies > 0 && players == 0)
		{
			g.disarm_ticks++;
			g_view.set_flags |= FF_OBJECTIVE_DISARMING;
		}
		else
			g.disarm_ticks = MAX(0L, g.disarm_ticks - 2);
		g_view.set_count = (short)MIN(g.disarm_ticks, (long)FFO_DISARM_TICKS);
	}
	if (g.disarm_ticks >= FFO_DISARM_TICKS)
	{
		/* (disarmed: back where it waited, to be planted again) */
		if (object_try_and_get(g.bomb_planted))
			object_delete(g.bomb_planted);
		g.bomb_planted = NONE;
		g.disarm_ticks = 0;
		g_view.set_flags &= ~(FF_OBJECTIVE_PLANTED | FF_OBJECTIVE_DISARMING);
		g_view.set_progress = 0;
		g_view.set_count = 0;
		ffo_item_spawn(get_ball_definition_index(), &g.bomb_home);
		ffo_notice("THE BOMB WAS DISARMED", FFO_NOTICE_TICKS);
		return;
	}
	if (g_view.set_progress >= g_view.set_goal)
	{
		/* (it goes off: a tank shell's blast, and more round it, which
		hurts all it reaches, the players too) */
		ffo_big_blast(&g_view.set_site);
		firefight_objective_blast_damage(&g_view.set_site, FFO_BLAST_RADIUS * 0.4f);
		if (object_try_and_get(g.bomb_planted))
			object_delete(g.bomb_planted);
		g.bomb_planted = NONE;
		ffo_set_end(_ff_objective_won);
	}
}

/* ---------- the marks (the view's, which every machine's HUD shows) */

static void ffo_marks_clear(void)
{
	short index;

	for (index = 0; index < FF_OBJECTIVE_WEAPON_MARKS; index++)
		g_view.round_marks[index] = NONE;
	for (index = 0; index < FF_OBJECTIVE_UNIT_MARKS; index++)
		g_view.set_units[index] = NONE;
	for (index = 0; index < FF_OBJECTIVE_DROP_MARKS; index++)
	{
		g_view.set_drops[index] = NONE;
		g_view.set_drop_values[index] = 0;
	}
}

/* (Use X only's) its weapons lying about, kept (never taken as garbage)
while it plays: a dead player's, a pod's; and, with none about, a pod of it
again for each counted player alive without one */
static void ffo_use_only_marks_update(void)
{
	struct object_iterator iterator;
	struct object_datum *object;
	short marks = 0;
	long now = game_time_get();

	object_iterator_new(&iterator, _object_mask_weapon, 0);
	while ((object = (struct object_datum *)object_iterator_next(&iterator)) != NULL)
	{
		if (object->definition_index != g.round_weapon_tag || object->object.parent_object_index != NONE)
			continue;
		SET_FLAG(object->object.flags, _object_cannot_be_garbage_bit, TRUE);
		if (marks < FF_OBJECTIVE_WEAPON_MARKS)
			g_view.round_marks[marks++] = ffo_absolute_of_object(iterator.index);
	}
	if (marks || !g.round_armed || now < g.round_redrop_time ||
		firefight_globals_get()->ordnance_pod_drop_time > 0)
	{
		return;
	}
	{
		unsigned long players[FFO_PLAYER_WORDS];
		struct data_iterator players_iterator;
		struct player_datum *player;
		short wanting = 0;

		csmemset(players, 0, sizeof(players));
		data_iterator_new(&players_iterator, player_data);
		while ((player = (struct player_datum *)data_iterator_next(&players_iterator)) != NULL)
		{
			short absolute = (short)DATUM_INDEX_TO_ABSOLUTE_INDEX(players_iterator.datum_index);

			if (player->quit_out_of_game || !ffo_bit(g.counted, absolute) || !ffo_living_unit(player->unit_index) ||
				unit_has_weapon_definition_index(player->unit_index, g.round_weapon_tag))
			{
				continue;
			}
			ffo_bit_set(players, absolute);
			wanting++;
		}
		if (!wanting)
			return;
		g.round_redrop_time = now + FFO_REDROP_TICKS;
		error(_error_silent, "firefight objectives: USE ONLY's %s dropped again for %d players without one",
			g_view.round_label, (int)wanting);
		firefight_spawn_ordnance_weapon_for(players, FFO_PLAYER_WORDS, g.round_weapon_tag);
	}
}

/* (the host's) the marks, so often */
static void ffo_marks_update(void)
{
	long now = game_time_get();
	short index;

	if (now < g.marks_next && now >= g.marks_next - FFO_MARKS_TICKS)
		return;
	g.marks_next = now + FFO_MARKS_TICKS;
	ffo_marks_clear();
	if (g_view.round_state == _ff_objective_running && g_view.round_kind == _ff_round_objective_weapon_only &&
		g.round_weapon_tag != NONE)
	{
		ffo_use_only_marks_update();
	}
	if (g_view.set_state != _ff_objective_running)
		return;
	/* (Purge's base's enemies, each) */
	if (g_view.set_kind == _ff_set_objective_purge)
	{
		short marks = 0;
		real distances[FF_OBJECTIVE_UNIT_MARKS];

		/* (more than it marks: the nearest a player) */
		for (index = 0; index < g.unit_count; index++)
		{
			struct object_datum *unit;
			real distance;
			short at;

			if (g.unit_roles[index] == _ffo_role_marine || !ffo_living_unit(g.units[index]))
				continue;
			unit = object_get(g.units[index]);
			distance = ffo_nearest_player_distance(&unit->object.position, NULL);
			for (at = marks; at > 0 && distances[at - 1] > distance; at--)
			{
				if (at < FF_OBJECTIVE_UNIT_MARKS)
				{
					distances[at] = distances[at - 1];
					g_view.set_units[at] = g_view.set_units[at - 1];
				}
			}
			if (at >= FF_OBJECTIVE_UNIT_MARKS)
				continue;
			distances[at] = distance;
			g_view.set_units[at] = ffo_absolute_of_object(g.units[index]);
			if (marks < FF_OBJECTIVE_UNIT_MARKS)
				marks++;
		}
	}
	/* (Collect's skulls a player dropped) */
	if (g_view.set_kind == _ff_set_objective_collect)
	{
		short marks = 0;

		for (index = 0; index < g.token_count && marks < FF_OBJECTIVE_DROP_MARKS; index++)
		{
			if (g.token_dropped[index] && object_try_and_get(g.tokens[index]))
			{
				g_view.set_drop_values[marks] = (byte)MIN(255, g.token_values[index]);
				g_view.set_drops[marks++] = ffo_absolute_of_object(g.tokens[index]);
			}
		}
	}
}

/* ---------- public code */

/* (firefight.c's post_rasterize_objects, every machine's) Data Link's hill
drawn as King of the Hill draws its own: a band round its markers, the map's
hill shader; a hill of fewer than 3 markers a ring of its size */
void firefight_objectives_render(void)
{
	struct game_globals *globals = scenario_get_game_globals();
	struct game_globals_multiplayer_information *information;
	struct ffo_hill const *hill = NULL;
	real_point3d points[FFO_RING_POINTS];
	real best = 4.0f * 4.0f;
	short count;
	short index;

	boolean camp = g_view.set_kind == _ff_set_objective_target || g_view.set_kind == _ff_set_objective_generator ||
		g_view.set_kind == _ff_set_objective_purge;

	if (!firefight_active() || (g_view.set_kind != _ff_set_objective_data_link && !camp) ||
		g_view.set_state != _ff_objective_running || !globals || globals->multiplayer_information.count < 1)
	{
		return;
	}
	information = TAG_BLOCK_GET_ELEMENT(&globals->multiplayer_information, 0,
		struct game_globals_multiplayer_information);
	if (information->hill_shader.index == NONE)
		return;
	/* (a camp's: a ring of its size on the ground round it, each point's
	height the ground's there, found once for each camp) */
	if (camp)
	{
		static real_point3d ring[FFO_RING_POINTS];
		static real_point3d ring_site;
		static real ring_radius = 0.0f;

		if (ring_radius != g_view.set_radius || distance3d(&ring_site, &g_view.set_site) > 0.1f)
		{
			ring_site = g_view.set_site;
			ring_radius = g_view.set_radius;
			for (index = 0; index < FFO_RING_POINTS; index++)
			{
				real angle = 2.0f * _pi * (real)index / (real)FFO_RING_POINTS;
				static real const heights[] = { 1.0f, 5.0f, 10.0f };
				short height;

				ring[index] = g_view.set_site;
				ring[index].x += cosine(angle) * g_view.set_radius;
				ring[index].y += sine(angle) * g_view.set_radius;
				for (height = 0; height < (short)NUMBEROF(heights); height++)
				{
					real_point3d from = ring[index];
					real_point3d ground;

					from.z = g_view.set_site.z + heights[height];
					if (firefight_objective_ground(&from, &ground) != NONE)
					{
						ring[index].z = ground.z + 0.05f;
						break;
					}
				}
			}
		}
		count = FFO_RING_POINTS;
		csmemcpy(points, ring, sizeof(ring));
	}
	/* (the hill the view's middle is: the host picked it) */
	for (index = 0; !camp && index < g.hill_count; index++)
	{
		real dx = g.hills[index].center.x - g_view.set_site.x;
		real dy = g.hills[index].center.y - g_view.set_site.y;

		if (dx * dx + dy * dy < best)
		{
			best = dx * dx + dy * dy;
			hill = &g.hills[index];
		}
	}
	if (camp)
	{
	}
	else if (hill && hill->point_count >= 3)
	{
		count = hill->point_count;
		for (index = 0; index < count; index++)
		{
			points[index].x = hill->points[index].x;
			points[index].y = hill->points[index].y;
			points[index].z = hill->point_z[index];
		}
	}
	else
	{
		count = FFO_HILL_POINTS;
		for (index = 0; index < count; index++)
		{
			real angle = 2.0f * _pi * (real)index / (real)count;

			points[index].x = g_view.set_site.x + cosine(angle) * g_view.set_radius;
			points[index].y = g_view.set_site.y + sine(angle) * g_view.set_radius;
			points[index].z = g_view.set_site.z;
		}
	}
	render_dynamic_quad_initialize();
	for (index = 0; index < count; index++)
	{
		struct model_vertex_uncompressed vertices[4];
		real_point3d const *a = &points[index];
		real_point3d const *b = &points[index + 1 == count ? 0 : index + 1];
		real_vector3d sides[2];
		real_vector3d normal;
		real length = distance3d(a, b);

		if (length < 0.01f)
			continue;
		csmemset(vertices, 0, sizeof(vertices));
		vertices[0].position = *a;
		vertices[1].position = *a;
		vertices[1].position.z += 0.8f;
		vertices[2].position = *b;
		vertices[2].position.z += 0.8f;
		vertices[3].position = *b;
		vector_from_points3d(&vertices[0].position, &vertices[1].position, &sides[0]);
		vector_from_points3d(&vertices[1].position, &vertices[2].position, &sides[1]);
		cross_product3d(&sides[0], &sides[1], &normal);
		normalize3d(&normal);
		vertices[0].normal = vertices[1].normal = vertices[2].normal = vertices[3].normal = normal;
		vertices[0].texcoord.x = 0.0f;
		vertices[0].texcoord.y = 1.0f;
		vertices[1].texcoord.x = 0.0f;
		vertices[1].texcoord.y = 0.2f;
		vertices[2].texcoord.x = 1.0f;
		vertices[2].texcoord.y = 0.2f;
		vertices[3].texcoord.x = 1.0f;
		vertices[3].texcoord.y = 1.0f;
		render_dynamic_quad(vertices, information->hill_shader.index, NULL, NULL, MAX(1.0f, length), 1.0f);
	}
}

/* (actor_perception.c) whether an actor sees nothing: a camp's unit (a
defender, the VIP) while the camp has not noticed the players (ffo_camp_update);
it hears and feels as ever */
boolean firefight_objectives_actor_unaware(long actor_index)
{
	struct actor_datum *actor;
	short index;

	if (g_view.set_state != _ff_objective_running || g.camp_aware || actor_index == NONE ||
		(g_view.set_kind != _ff_set_objective_target && g_view.set_kind != _ff_set_objective_generator &&
			g_view.set_kind != _ff_set_objective_purge) || !firefight_active())
	{
		return FALSE;
	}
	actor = actor_try_and_get(actor_index);
	if (!actor || actor->meta.unit_index == NONE)
		return FALSE;
	for (index = 0; index < g.unit_count; index++)
	{
		if (g.units[index] == actor->meta.unit_index)
			return g.unit_roles[index] == _ffo_role_defender || g.unit_roles[index] == _ffo_role_target;
	}
	return FALSE;
}

struct firefight_objectives_view const *firefight_objectives_view_get(void)
{
	return &g_view;
}

void firefight_objectives_view_set(struct firefight_objectives_view const *view)
{
	if (view)
		g_view = *view;
}

static void ffo_reset(void)
{
	short index;

	csmemset(&g_view, 0, sizeof(g_view));
	g_view.round_target = NONE;
	g_view.set_target = NONE;
	g_view.set_carrier = NONE;
	g_view.set_ticks_left = NONE;
	g_view.deposit_object = NONE;
	ffo_marks_clear();
	csmemset(&g, 0, sizeof(g));
	g.deposit_skull = NONE;
	ffo_set_clear_units();
	g.round_target_unit = NONE;
	g.generator_scenery = NONE;
	g.set_deadline = NONE;
	for (index = 0; index < HALO_PORT_MAXIMUM_NETWORK_PLAYERS; index++)
		g.shield_unit[index] = NONE;
	g_local_generator_scenery = NONE;
	g_notice_sticky_until = NONE;
	g_notice_waiting_ticks = 0;
}

void firefight_objectives_initialize_for_new_map(void)
{
	ffo_reset();
	g.rng = 0x2545F491UL ^ (unsigned long)system_milliseconds();
	/* (every machine's: its hills' outlines, firefight_objectives_render) */
	ffo_map_scan();
	if (network_game_distributed_client())
		return;
	if (global_scenario_index != NONE)
		ffo_map_file_write(tag_get_name(global_scenario_index), g.set_available);
	/* (the game says which of those on cannot play here) */
	if (config_boolean("game.firefight_objectives_set"))
	{
		char text[FF_OBJECTIVE_NOTICE_LENGTH + 2];
		short kind;
		short missing = 0;

		snprintf(text, sizeof(text), "NOT ON THIS MAP:");
		for (kind = 1; kind < NUMBER_OF_FF_SET_OBJECTIVES; kind++)
		{
			if (config_boolean(ffo_set_settings[kind]) && !(g.set_available & (1UL << kind)))
			{
				size_t length = strlen(text);

				snprintf(text + length, sizeof(text) - length, "%s %s", missing ? "," : "", ffo_set_names[kind]);
				missing++;
			}
		}
		if (missing)
		{
			ffo_notice(text, FFO_MAP_NOTICE_TICKS);
			g_notice_sticky_until = game_time_get() + FFO_MAP_NOTICE_TICKS;
		}
	}
}

void firefight_objectives_dispose(void)
{
	ffo_reset();
}

void firefight_objectives_round_starting(short round_num)
{
	(void)round_num;
	if (!ffo_host())
		return;
	firefight_objectives_wave_ended();
	firefight_objectives_round_ended();
	ffo_set_roll_for_round();
}

void firefight_objectives_wave_starting(short wave_num)
{
	if (!ffo_host())
		return;
	firefight_objectives_wave_ended();
	ffo_count_players();
	ffo_round_roll();
	if (wave_num == g.set_start_round && g_view.set_state != _ff_objective_running)
		ffo_set_roll_start();
}

void firefight_objectives_wave_spawned(short wave_num)
{
	struct firefight_globals const *ff = firefight_globals_get();
	short best = NONE;
	short index;

	(void)wave_num;
	if (!ffo_host() || g_view.round_kind != _ff_round_objective_kill_first ||
		g_view.round_state != _ff_objective_running || g.round_target_unit != NONE)
	{
		return;
	}
	/* (the wave's highest rank, any of them) */
	for (index = 0; index < FIREFIGHT_MAX_TRACKED_ENEMIES; index++)
	{
		if (ff->tracked_enemy_units[index] != NONE && ffo_living_unit(ff->tracked_enemy_units[index]) &&
			(best == NONE || ff->tracked_enemy_tiers[index] > ff->tracked_enemy_tiers[best] ||
				(ff->tracked_enemy_tiers[index] == ff->tracked_enemy_tiers[best] && (ffo_rand() & 1))))
		{
			best = index;
		}
	}
	if (best == NONE)
	{
		ffo_round_end(_ff_objective_tied);
		return;
	}
	g.round_target_unit = ff->tracked_enemy_units[best];
	g_view.round_target = ffo_absolute_of_object(g.round_target_unit);
}

void firefight_objectives_wave_ended(void)
{
	if (!ffo_host() || g_view.round_state != _ff_objective_running)
		return;
	switch (g_view.round_kind)
	{
	case _ff_round_objective_no_deaths:
	case _ff_round_objective_shields:
	case _ff_round_objective_dont_use:
	case _ff_round_objective_weapon_only:
		ffo_round_end(_ff_objective_won);
		break;
	case _ff_round_objective_kill_first:
		/* (its target never killed) */
		ffo_round_end(_ff_objective_failed);
		break;
	default:
		ffo_round_end(_ff_objective_failed);
		break;
	}
}

void firefight_objectives_round_ended(void)
{
	if (!ffo_host())
		return;
	g.set_start_round = 0;
	/* (Rescue's marines, rescued, were paid for as it was done) */
	ffo_set_end(_ff_objective_failed);
	/* (what is left of a camp or a raid goes with the set) */
	if (g.unit_count > 0 && g.teleport_until == NONE)
		ffo_teleport_out();
}

void firefight_objectives_update(void)
{
	struct firefight_globals const *ff = firefight_globals_get();

	if (!firefight_active())
		return;
	if (g_view.notice_ticks > 0)
		g_view.notice_ticks--;
	/* (Collect's deposit's skull, big, on every machine: its size is not
	sent) */
	if (g_view.set_kind == _ff_set_objective_collect && g_view.set_state == _ff_objective_running &&
		g_view.deposit_object != NONE)
	{
		long skull = firefight_objectives_object(g_view.deposit_object);
		struct object_datum *object = skull != NONE ? object_try_and_get(skull) : NULL;

		if (object)
			object->object.scale = FFO_DEPOSIT_SCALE;
	}
	/* (Collect's deposit's fire, each machine's own, harmless) */
	if (g_view.set_kind == _ff_set_objective_collect && g_view.set_state == _ff_objective_running &&
		game_time_get() % FFO_FIRE_TICKS == 0)
	{
		real angle = ffo_rand_range(0.0f, 2.0f * _pi);
		real distance = ffo_rand_range(1.0f, 2.2f);
		real_point3d at = g_view.set_site;

		firefight_objective_ground(&at, &at);
		at.x += cosine(angle) * distance;
		at.y += sine(angle) * distance;
		firefight_objective_fire_effect(&at);
	}
	if (network_game_distributed_client())
	{
		static boolean was_planted;

		ffo_client_generator_update();
		/* (the bomb going off: each machine's own blast) */
		if (g_view.set_kind == _ff_set_objective_bomb && g_view.set_state == _ff_objective_won && was_planted)
			ffo_big_blast(&g_view.set_site);
		was_planted = g_view.set_kind == _ff_set_objective_bomb && g_view.set_state == _ff_objective_running &&
			(g_view.set_flags & FF_OBJECTIVE_PLANTED) != 0;
		/* (the generator destroyed: each machine's own plasma blast) */
		{
			static boolean was_generator;

			if (g_view.set_kind == _ff_set_objective_generator && g_view.set_state == _ff_objective_won && was_generator)
				ffo_generator_blast(&g_view.set_site);
			was_generator = g_view.set_kind == _ff_set_objective_generator && g_view.set_state == _ff_objective_running;
		}
		return;
	}
	ffo_notice_update();
	if (g_view.round_result_ticks > 0)
		g_view.round_result_ticks--;
	if (g_view.set_result_ticks > 0)
		g_view.set_result_ticks--;
	if (ff->match_lost || ff->match_won)
	{
		ffo_marks_clear();
		if (g_view.round_state == _ff_objective_running)
			g_view.round_state = _ff_objective_failed;
		if (g_view.set_state == _ff_objective_running)
		{
			g_view.set_state = _ff_objective_failed;
			ffo_generator_remove();
		}
		return;
	}
	ffo_round_shields_update();
	ffo_marks_update();
	/* (Use X only's rule, once its pods are down) */
	if (g_view.round_kind == _ff_round_objective_weapon_only && g_view.round_state == _ff_objective_running &&
		!g.round_armed && game_time_get() >= g.round_arm_time)
	{
		char text[96];

		g.round_armed = TRUE;
		snprintf(text, sizeof(text), "USE ONLY THE %s FROM NOW", g_view.round_label);
		ffo_notice(text, FFO_NOTICE_TICKS);
	}
	if (g_view.round_kind == _ff_round_objective_travel && g_view.round_state == _ff_objective_running)
		ffo_round_travel_update();
	/* (Kill X first's target gone without a kill: a wipe, a fall) */
	if (g_view.round_kind == _ff_round_objective_kill_first && g_view.round_state == _ff_objective_running &&
		g.round_target_unit != NONE && !object_try_and_get(g.round_target_unit))
	{
		ffo_round_end(_ff_objective_tied);
	}
	ffo_set_update();
}

void firefight_objectives_enemy_killed(long actor_index, long dead_unit_index, long killer_player_index, boolean headshot,
	boolean melee, boolean grenade, boolean splatter)
{
	long absolute = ffo_absolute(killer_player_index);
	boolean by_player = killer_player_index != NONE && absolute >= 0 && absolute < HALO_PORT_MAXIMUM_NETWORK_PLAYERS;
	short index;

	if (!ffo_host())
		return;
	/* the round's */
	if (g_view.round_state == _ff_objective_running)
	{
		if (g_view.round_kind == _ff_round_objective_kills && by_player && ffo_counted(killer_player_index))
		{
			short param = g_view.round_param;
			boolean counts = FALSE;

			switch (param)
			{
			case _ff_objective_kill_headshot: counts = headshot; break;
			case _ff_objective_kill_melee: counts = melee; break;
			case _ff_objective_kill_grenade: counts = grenade; break;
			case _ff_objective_kill_splatter: counts = splatter; break;
			default:
				if (!melee && !grenade && !splatter)
				{
					struct player_datum *player = player_try_and_get(killer_player_index);
					short weapon_class;

					counts = player && ffo_unit_weapon_class(ffo_living_unit(player->unit_index), &weapon_class) &&
						weapon_class == param - _ff_objective_kill_weapon;
				}
				break;
			}
			if (counts)
			{
				g.round_contribution[absolute]++;
				if (++g_view.round_count >= g_view.round_goal)
					ffo_round_end(_ff_objective_won);
			}
		}
		else if (g_view.round_kind == _ff_round_objective_vehicle_kills && by_player && ffo_counted(killer_player_index))
		{
			struct player_datum *player = player_try_and_get(killer_player_index);
			struct unit_datum *unit = player ? unit_try_and_get(player->unit_index) : NULL;
			struct object_datum *vehicle = unit && unit->object.parent_object_index != NONE ?
				object_try_and_get_and_verify_type(unit->object.parent_object_index, _object_mask_vehicle) : NULL;

			if (vehicle && vehicle->definition_index == g.round_vehicle_tag)
			{
				g.round_contribution[absolute]++;
				if (++g_view.round_count >= g_view.round_goal)
					ffo_round_end(_ff_objective_won);
			}
		}
		else if (g_view.round_kind == _ff_round_objective_kill_first && g.round_target_unit != NONE)
		{
			if (dead_unit_index == g.round_target_unit)
			{
				if (by_player && ffo_counted(killer_player_index))
				{
					g.round_contribution[absolute] = 1;
					ffo_round_end(_ff_objective_won);
				}
				else
					ffo_round_end(_ff_objective_tied);
			}
			else if (by_player && ffo_counted(killer_player_index))
			{
				ffo_round_fail(killer_player_index, "KILLED ANOTHER ENEMY FIRST");
			}
		}
	}
	/* the set's */
	if (g_view.set_state != _ff_objective_running)
		return;
	/* (Collect's: an enemy a player killed drops a skull) */
	if (g_view.set_kind == _ff_set_objective_collect && by_player)
	{
		struct object_datum *dead = object_try_and_get(dead_unit_index);

		if (dead && firefight_team_is_hostile(dead->object.owner_team_index))
		{
			real_point3d at;

			object_get_origin(dead_unit_index, &at);
			ffo_token_spawn(&at, ffo_skull_value(actor_index, dead_unit_index), FALSE);
		}
	}
	for (index = 0; index < g.unit_count && g.units[index] != dead_unit_index; index++)
		;
	if (index == g.unit_count)
		return;
	if (by_player)
		g.set_contribution[absolute] += g.unit_roles[index] == _ffo_role_target || g.unit_roles[index] == _ffo_role_boss ? 5 : 1;
	if (g_view.set_kind == _ff_set_objective_target && dead_unit_index == g.target_unit)
	{
		ffo_set_end(_ff_objective_won);
	}
	else if (g_view.set_kind == _ff_set_objective_oddball && dead_unit_index == g.target_unit)
	{
		/* (its skull where it fell) */
		real_point3d at;

		object_get_origin(dead_unit_index, &at);
		firefight_objective_ground(&at, &at);
		g.target_unit = NONE;
		g_view.set_site = at;
		ffo_item_spawn(get_ball_definition_index(), &at);
		ffo_notice("THE SKULL IS DOWN: HOLD IT", FFO_NOTICE_TICKS);
	}
}

void firefight_objectives_player_died(long dead_player_index)
{
	long absolute = ffo_absolute(dead_player_index);

	/* (Collect's: what they carried, on the ground where they fell, for anyone) */
	if (ffo_host() && g_view.set_kind == _ff_set_objective_collect && g_view.set_state == _ff_objective_running &&
		absolute >= 0 && absolute < HALO_PORT_MAXIMUM_NETWORK_PLAYERS && g_view.carried[absolute])
	{
		struct player_datum *player = player_try_and_get(dead_player_index);

		if (player && player->unit_index != NONE && object_try_and_get(player->unit_index))
		{
			real_point3d at;

			object_get_origin(player->unit_index, &at);
			ffo_token_spawn(&at, g_view.carried[absolute], TRUE);
		}
		{
			char name[48];
			char text[96];

			ffo_player_name(dead_player_index, name, sizeof(name));
			snprintf(text, sizeof(text), "%s DROPPED %d SKULL%s", name, (int)g_view.carried[absolute],
				g_view.carried[absolute] == 1 ? "" : "S");
			ffo_notice(text, FFO_NOTICE_TICKS);
		}
		g_view.carried[absolute] = 0;
	}
	if (!ffo_host() || g_view.round_state != _ff_objective_running ||
		g_view.round_kind != _ff_round_objective_no_deaths || !ffo_counted(dead_player_index))
	{
		return;
	}
	ffo_round_fail(dead_player_index, "DIED");
}

void firefight_objectives_weapon_fired(long weapon_index, long owner_unit_index)
{
	struct unit_datum *unit;
	struct object_datum *weapon;
	short weapon_class;
	char what[64];

	if (g_view.round_kind == _ff_round_objective_weapon_only && g_view.round_state == _ff_objective_running &&
		g.round_armed && owner_unit_index != NONE && ffo_host())
	{
		unit = unit_try_and_get(owner_unit_index);
		weapon = weapon_index != NONE ? object_try_and_get_and_verify_type(weapon_index, _object_mask_weapon) : NULL;
		if (unit && weapon && unit->unit.player_index != NONE && ffo_counted(unit->unit.player_index) &&
			weapon->definition_index != g.round_weapon_tag)
		{
			ffo_round_fail(unit->unit.player_index, "FIRED ANOTHER WEAPON");
		}
		/* (a vehicle's or a turret's gun: the player in it) */
		else if (unit && weapon && unit->unit.player_index == NONE)
		{
			struct object_iterator iterator;
			struct object_datum *object;

			object_iterator_new(&iterator, _object_mask_unit, 0);
			while ((object = (struct object_datum *)object_iterator_next(&iterator)) != NULL)
			{
				long rider = ((struct unit_datum *)object)->unit.player_index;

				if (object->object.parent_object_index == owner_unit_index && rider != NONE && ffo_counted(rider))
				{
					ffo_round_fail(rider, "FIRED A VEHICLE'S GUN");
					break;
				}
			}
		}
		return;
	}
	if (g_view.round_kind != _ff_round_objective_dont_use || g_view.round_state != _ff_objective_running ||
		g_view.round_param >= _ff_objective_use_grenade || !ffo_host() || owner_unit_index == NONE)
	{
		return;
	}
	unit = unit_try_and_get(owner_unit_index);
	weapon = weapon_index != NONE ? object_try_and_get_and_verify_type(weapon_index, _object_mask_weapon) : NULL;
	if (!unit || !weapon || unit->unit.player_index == NONE || !ffo_counted(unit->unit.player_index) ||
		!ffo_weapon_class(weapon->definition_index, &weapon_class) || weapon_class != g_view.round_param)
	{
		return;
	}
	snprintf(what, sizeof(what), "FIRED %s", ffo_class_names[weapon_class]);
	ffo_round_fail(unit->unit.player_index, what);
}

void firefight_objectives_grenade_thrown(long unit_index, short grenade_type)
{
	struct unit_datum *unit;

	/* (Use X only's: any grenade) */
	if (g_view.round_kind == _ff_round_objective_weapon_only && g_view.round_state == _ff_objective_running &&
		g.round_armed && unit_index != NONE && ffo_host())
	{
		unit = unit_try_and_get(unit_index);
		if (unit && unit->unit.player_index != NONE && ffo_counted(unit->unit.player_index))
			ffo_round_fail(unit->unit.player_index, "THREW A GRENADE");
		return;
	}
	if (g_view.round_kind != _ff_round_objective_dont_use || g_view.round_state != _ff_objective_running ||
		g_view.round_param != _ff_objective_use_grenade + grenade_type || !ffo_host() || unit_index == NONE)
	{
		return;
	}
	unit = unit_try_and_get(unit_index);
	if (!unit || unit->unit.player_index == NONE || !ffo_counted(unit->unit.player_index))
		return;
	ffo_round_fail(unit->unit.player_index, "THREW A GRENADE");
}

boolean firefight_objectives_apply_damage(long object_index, long attacker_player_index, short attacker_team_index,
	real damage_amount)
{
	long absolute;
	long points;

	if (g_view.set_kind != _ff_set_objective_generator || g_view.set_state != _ff_objective_running ||
		object_index == NONE || (object_index != g.target_unit && object_index != g.generator_scenery) || !ffo_host())
	{
		return FALSE;
	}
	/* (the players' and their allies' alone) */
	if (attacker_player_index == NONE && attacker_team_index != _game_team_player &&
		!(attacker_team_index == _game_team_human && !firefight_team_is_hostile(_game_team_human)))
	{
		return TRUE;
	}
	points = MAX(1L, (long)(damage_amount * 0.45f));
	g.camp_alarm_until = game_time_get() + 10 * TICKS_PER_SECOND;
	g_view.set_progress = MAX(0L, g_view.set_progress - points);
	absolute = ffo_absolute(attacker_player_index);
	if (absolute >= 0 && absolute < HALO_PORT_MAXIMUM_NETWORK_PLAYERS)
		g.set_contribution[absolute] += points;
	if (g_view.set_progress <= 0)
	{
		ffo_generator_blast(&g_view.set_site);
		ffo_set_end(_ff_objective_won);
	}
	return TRUE;
}

real firefight_objectives_damage_scale(long object_index)
{
	short index;
	short attackers;

	if (g_view.set_kind != _ff_set_objective_rescue || g_view.set_state != _ff_objective_running ||
		object_index == NONE || !ffo_host())
	{
		return 1.0f;
	}
	for (index = 0; index < g.marine_count && g.marines[index] != object_index; index++)
		;
	if (index == g.marine_count)
		return 1.0f;
	/* (before anyone came, nothing hurts them: their patience is their life) */
	if (!(g_view.set_flags & FF_OBJECTIVE_ARRIVED))
		return 0.0f;
	attackers = ffo_units_alive(_ffo_role_attacker, FALSE);
	return 1.0f - 0.8f * (real)attackers / (real)MAX(1, g.attackers_spawned);
}

boolean firefight_objectives_pickup_blocked(long weapon_index)
{
	short index;

	if (weapon_index == NONE || g_view.set_state != _ff_objective_running)
		return FALSE;
	if (g_view.set_kind == _ff_set_objective_bomb)
	{
		if (network_game_distributed_client())
			return (g_view.set_flags & FF_OBJECTIVE_PLANTED) && ffo_absolute_of_object(weapon_index) == g_view.set_target;
		return weapon_index == g.bomb_planted;
	}
	if (g_view.set_kind != _ff_set_objective_collect)
		return FALSE;
	if (weapon_index == g.deposit_skull)
		return TRUE;
	/* (a skull is taken by walking into it, never held: on a client, any of
	the map's balls while Collect plays) */
	if (network_game_distributed_client())
	{
		struct object_datum *object = object_try_and_get(weapon_index);

		return object && object->definition_index == get_ball_definition_index();
	}
	for (index = 0; index < g.token_count; index++)
	{
		if (g.tokens[index] == weapon_index)
			return TRUE;
	}
	return FALSE;
}

boolean firefight_objectives_spawn_blocked(real_point3d const *point)
{
	if (!point || g_view.set_state != _ff_objective_running || g_view.set_radius <= 0.0f)
		return FALSE;
	switch (g_view.set_kind)
	{
	case _ff_set_objective_target:
	case _ff_set_objective_generator:
	case _ff_set_objective_purge:
		return distance3d(point, &g_view.set_site) < g_view.set_radius;
	default:
		return FALSE;
	}
}

long firefight_objectives_priority_target(void)
{
	struct player_datum *player;

	if (g_view.set_state != _ff_objective_running || g_view.set_carrier == NONE ||
		(g_view.set_kind != _ff_set_objective_oddball && g_view.set_kind != _ff_set_objective_flag &&
			g_view.set_kind != _ff_set_objective_bomb) || !player_data)
	{
		return NONE;
	}
	{
		struct data_iterator iterator;

		data_iterator_new(&iterator, player_data);
		while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
		{
			if ((short)DATUM_INDEX_TO_ABSOLUTE_INDEX(iterator.datum_index) == g_view.set_carrier)
				return ffo_living_unit(player->unit_index) ? player->unit_index : NONE;
		}
	}
	return NONE;
}

/* ---------- the HUD's words (firefight_hud.c): the cards' title and status,
from the view (every machine's) */

static void ffo_time_text(long ticks, char *text, size_t size)
{
	long seconds = MAX(0L, (ticks + TICKS_PER_SECOND - 1) / TICKS_PER_SECOND);

	snprintf(text, size, "%ld:%02ld", seconds / 60, seconds % 60);
}

static char const *ffo_result_text(byte state)
{
	switch (state)
	{
	case _ff_objective_won: return "COMPLETE";
	case _ff_objective_failed: return "FAILED";
	case _ff_objective_tied: return "TIE";
	default: return "";
	}
}

boolean firefight_objectives_round_card(char *title, size_t title_size, char *status, size_t status_size)
{
	if (g_view.round_kind == _ff_round_objective_none ||
		(g_view.round_state != _ff_objective_running && g_view.round_result_ticks <= 0))
	{
		return FALSE;
	}
	ffo_round_text(title, title_size);
	status[0] = 0;
	if (g_view.round_state != _ff_objective_running)
	{
		snprintf(status, status_size, "%s", ffo_result_text(g_view.round_state));
		return TRUE;
	}
	switch (g_view.round_kind)
	{
	case _ff_round_objective_kills:
		snprintf(status, status_size, "%d / %d", (int)g_view.round_count, (int)g_view.round_goal);
		break;
	case _ff_round_objective_shields:
		snprintf(status, status_size, "BROKEN %d / %d", (int)g_view.round_count, (int)(g_view.round_goal / 2));
		break;
	case _ff_round_objective_kill_first:
		snprintf(status, status_size, "%s", g_view.round_target != NONE ? "MARKED" : "WAITING");
		break;
	case _ff_round_objective_vehicle_kills:
		snprintf(status, status_size, "%d / %d", (int)g_view.round_count, (int)g_view.round_goal);
		break;
	case _ff_round_objective_travel:
		snprintf(status, status_size, "GO");
		break;
	default:
		snprintf(status, status_size, "THIS WAVE");
		break;
	}
	return TRUE;
}

boolean firefight_objectives_set_card(char *title, size_t title_size, char *status, size_t status_size,
	short local_player_absolute)
{
	char left[16];
	char held[16];
	char goal[16];

	if (g_view.set_kind == _ff_set_objective_none ||
		(g_view.set_state != _ff_objective_running && g_view.set_result_ticks <= 0))
	{
		return FALSE;
	}
	snprintf(title, title_size, "%s", ffo_set_names[g_view.set_kind]);
	status[0] = 0;
	left[0] = 0;
	if (g_view.set_ticks_left != NONE && g_view.set_state == _ff_objective_running)
		ffo_time_text(g_view.set_ticks_left, left, sizeof(left));
	if (g_view.set_state != _ff_objective_running)
	{
		snprintf(status, status_size, "%s", ffo_result_text(g_view.set_state));
		return TRUE;
	}
	switch (g_view.set_kind)
	{
	case _ff_set_objective_target:
		snprintf(status, status_size, "KILL THE VIP  %s", left);
		break;
	case _ff_set_objective_generator:
		snprintf(status, status_size, "DESTROY IT  %d%%  %s",
			(int)(100L * g_view.set_progress / MAX(1L, g_view.set_goal)), left);
		break;
	case _ff_set_objective_purge:
		snprintf(status, status_size, "%d LEFT", (int)g_view.set_count);
		break;
	case _ff_set_objective_data_link:
		ffo_time_text(g_view.set_progress, held, sizeof(held));
		ffo_time_text(g_view.set_goal, goal, sizeof(goal));
		snprintf(status, status_size, "%s / %s%s  %s", held, goal,
			(g_view.set_flags & FF_OBJECTIVE_CONTESTED) ? " CONTESTED" : "", left);
		break;
	case _ff_set_objective_flag:
		snprintf(status, status_size, "%s", g_view.set_carrier != NONE ? "TAKE IT TO THE OTHER BASE" : "GET THE FLAG");
		break;
	case _ff_set_objective_rescue:
		if (g_view.set_flags & FF_OBJECTIVE_ARRIVED)
			snprintf(status, status_size, "MARINES %d / %d", (int)g_view.set_count, (int)g_view.set_total);
		else
			snprintf(status, status_size, "THEIR LIFE %d%%", (int)(100L * g_view.set_progress / MAX(1L, g_view.set_goal)));
		break;
	case _ff_set_objective_oddball:
		if (g_view.set_progress == 0 && !(g_view.set_flags & FF_OBJECTIVE_HELD) &&
			firefight_objectives_object(g_view.set_target) != NONE &&
			object_try_and_get_and_verify_type(firefight_objectives_object(g_view.set_target), _object_mask_unit))
		{
			snprintf(status, status_size, "KILL THE SKULL CARRIER");
		}
		else
		{
			ffo_time_text(g_view.set_progress, held, sizeof(held));
			ffo_time_text(g_view.set_goal, goal, sizeof(goal));
			snprintf(status, status_size, "HOLD %s / %s", held, goal);
		}
		break;
	case _ff_set_objective_collect:
	{
		short carried = local_player_absolute >= 0 && local_player_absolute < HALO_PORT_MAXIMUM_NETWORK_PLAYERS ?
			g_view.carried[local_player_absolute] : 0;

		if (carried)
			snprintf(status, status_size, "%ld / %ld  CARRYING %d", g_view.set_progress, g_view.set_goal, (int)carried);
		else
			snprintf(status, status_size, "BANKED %ld / %ld", g_view.set_progress, g_view.set_goal);
		break;
	}
	case _ff_set_objective_bomb:
		if (g_view.set_flags & FF_OBJECTIVE_PLANTED)
		{
			ffo_time_text(g_view.set_progress, held, sizeof(held));
			ffo_time_text(g_view.set_goal, goal, sizeof(goal));
			snprintf(status, status_size, "ARMING %s / %s%s", held, goal,
				(g_view.set_flags & FF_OBJECTIVE_DISARMING) ? " DISARMING" : "");
		}
		else
			snprintf(status, status_size, "%s", g_view.set_carrier != NONE ? "PLANT IT AT THE TARGET" : "GET THE BOMB");
		break;
	default:
		break;
	}
	return TRUE;
}
