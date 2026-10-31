/*
AE_SPAWN_MODERN.C

(Arena Evolved) AE MODERN SPAWNS: a spawn choice on the map's own spawn points
(ae_spawn_modern.h). A host rule, off unless
HALO_AE_SPAWN_RULE=modern. HALO_AE_SPAWN_MODERN_SCALE sets the distance scale (0.65 unless set); weights do not scale.
HALO_AE_SPAWN_LOG=1 logs each search and spawn.

- Eligible: CE's own rating above 0 (game type, team, the engine's own zero, a vehicle on the point).
- Score: the sum of the shapes around the point. Stacking shapes all count; a non-stacking group counts its strongest.
- Choose: the highest step of floor(score / 4) holding a point no body stands on and no player sees; one uniform draw
  among those. Nothing in sight rules: again without sight, then with corpses allowed. Every point held by a living
  unit: no spawn this tick, the player waits: units placed on an occupied point stack up into the air.
- Sight: a player within range in the cone the spawn would face, or the point within the cone the player aims along,
  with a clear ray through the structure. Rays stop at 256 a search; the search then goes on without sight.
- Sides: in a team game whose eligible points hold both sides, a point on the player's own side gets +30: the base
  stays home unless enemies crowd it. A point's side is the team of the nearest CTF flag; a map's team indices on its
  non-CTF points can be swapped (Blood Gulch's team game points are). Without flags, the point's team index. Eligible
  points with one team index only get nothing.
- Flip: a flag team's base held by enemies (one or more within 25 units of its flag, as many as its own there, for 10 s, a gap of 3 s
  forgiven) sends that team's spawns away from both flags: any of the map's points at least 20 units from every CTF flag, whatever
  its team or game type, when the map has 2 or more of them. No enemy holding it for 3 s: back home. Logged "ae spawn flip".
- Objectives: the ball and the hill push spawns out of their inner rings and pull them lightly to their outer rings.
*/

#include "cseries.h"
#include "cseries/errors.h"
#include "game/game.h"
#include "game/game_engine.h"
#include "game/players.h"
#include "game/players_runtime.h"
#include "memory/data.h"
#include "objects/objects.h"
#include "items/projectiles.h"
#include "items/projectile_definitions.h"
#include "math/real_math.h"
#include "physics/collisions.h"
#include "scenario/scenario.h"
#include "scenario/scenario_definitions.h"
#include "tag_files/tag_files.h"
#include "units/units.h"
#include "ae_bots_objective.h"
#include "ae_spawn_modern.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

void platform_log(char const *format, ...);

enum { SHAPES = 1024, PLAYERS = 256, POINTS = 256, RAY_CAP = 256 };
enum { STACKS, GROUP_ALLY, GROUP_ALLY_NEAR, GROUP_COMBAT, GROUPS };
enum { CURVE_FLAT, CURVE_ALLY, CURVE_ENEMY, CURVE_COMBAT };
enum { SHAPE_CYLINDER, SHAPE_SPHERE, SHAPE_CONE, SHAPE_RING };

#define STEP 4.0f
#define SIGHT_RANGE 18.0f
#define SIGHT_RANGE_ALLY 2.0f
/* cos(88 degrees) */
#define SIGHT_CONE 0.0349f
#define SIGHT_FACTOR_SCOPED 10.0f
#define POINT_TOP 0.6f
#define DEATH_SECONDS 12
#define COMBAT_SECONDS 3
#define BODY_RADIUS 0.6f
#define BODY_HEIGHT 1.5f
#define OBJECTIVE_WEIGHT 20.0f
#define OBJECTIVE_HEIGHT 3.0f
#define SIDE_WEIGHT 30.0f
#define FLIP_BASE_RADIUS 25.0f
#define FLIP_HOLD_SECONDS 10
#define FLIP_GAP_SECONDS 3
#define FLIP_FLAG_DISTANCE 20.0f
#define FLIP_POINTS_NEEDED 2

struct shape
{
	int kind, curve, group;
	real_point3d at;
	real_vector3d axis;
	/* cylinder: radius, height; sphere: radius; cone: radius, length; ring: inner, outer */
	real a, b, weight;
};

static struct
{
	int force, log, read;     /* force: 1 modern, -1 classic, 0 the gametype's own */
	real scale;
	real_point3d death_point[PLAYERS];
	long death_tick[PLAYERS];
	long spawn_tick[PLAYERS];
	int spawn_hurt[PLAYERS];
	real spawn_z[PLAYERS];
	int spawn_checked[PLAYERS];
	int rays;
	long waits;
} modern = { 0 };

static void settings_read(void)
{
	char const *rule = getenv("HALO_AE_SPAWN_RULE"), *log = getenv("HALO_AE_SPAWN_LOG");
	char const *scale = getenv("HALO_AE_SPAWN_MODERN_SCALE");

	if (modern.read)
		return;
	modern.read = 1;
	modern.force = rule && !strcmp(rule, "modern") ? 1 : rule && !strcmp(rule, "classic") ? -1 : 0;
	modern.log = log && log[0] == '1';
	modern.scale = scale ? (real)atof(scale) : 0.65f;
	if (modern.force)
		platform_log("ae spawn: %s rule from HALO_AE_SPAWN_RULE, scale %.2f%s", modern.force > 0 ? "modern" : "classic",
			(double)modern.scale, modern.log ? ", spawns logged" : "");
}

void ae_spawn_modern_log_rules(unsigned char byte)
{
	error(_error_silent, "AE spawns: %s", ae_spawns_of(byte) == AE_SPAWNS_MODERN ? "modern" : "classic");
}

int ae_spawn_modern_allowed(void)
{
	settings_read();
	if (!game_engine_running() || modern.force < 0)
		return 0;
	return modern.force > 0 || ae_spawns_of(game_engine_get_variant()->universal_variant.ball_melee) == AE_SPAWNS_MODERN;
}

static int teams(void)
{
	return game_engine_has_teams();
}

static int enemies(struct player_datum const *a, struct player_datum const *b)
{
	return !teams() || a->team_index != b->team_index;
}

static void add(struct shape *list, int *count, int kind, int curve, int group, real_point3d const *at,
	real_vector3d const *axis, real a, real b, real weight)
{
	struct shape *shape;

	if (*count >= SHAPES)
		return;
	shape = &list[(*count)++];
	shape->kind = kind;
	shape->curve = curve;
	shape->group = group;
	shape->at = *at;
	if (axis)
		shape->axis = *axis;
	shape->a = a;
	shape->b = b;
	shape->weight = weight;
}

/* a cylinder or a sphere, scaled, around a point */
static void add_around(struct shape *list, int *count, int kind, int curve, int group, real_point3d const *at,
	real radius, real height, real weight)
{
	add(list, count, kind, curve, group, at, NULL, radius * modern.scale, height * modern.scale, weight);
}

/* whether a tag's name holds a word (tag names are lower case) */
static int weapon_named(long weapon_index, char const *word)
{
	return strstr(tag_get_name(object_get(weapon_index)->definition_index), word) != NULL;
}

/* a player's current weapon along the aim: length, radius, weight (a default for any other weapon) */
static void add_weapon_line(struct shape *list, int *count, long unit_index, real_point3d const *eye)
{
	struct unit_datum *unit = (struct unit_datum *)object_get(unit_index);
	short current = unit->unit.current_weapon_index;
	long weapon = current >= 0 && current < MAXIMUM_WEAPONS_PER_UNIT ? unit->unit.weapon_object_indices[current] : NONE;
	real length = 20.0f, weight = -7.0f;

	if (weapon == NONE)
		return;
	if (weapon_named(weapon, "sniper"))
		length = 28.0f, weight = -100.0f;
	else if (weapon_named(weapon, "rocket"))
		length = 24.0f, weight = -25.0f;
	else if (weapon_named(weapon, "battle rifle") || weapon_named(weapon, "pistol") || weapon_named(weapon, "dmr"))
		length = 24.0f, weight = -16.0f;
	add(list, count, SHAPE_CONE, CURVE_FLAT, STACKS, eye, &unit->unit.aiming_vector, 2.0f * modern.scale, length * modern.scale, weight);
}

static int in_combat(long unit_index)
{
	struct unit_datum *unit = (struct unit_datum *)object_get(unit_index);
	long now = game_time_get();
	int slot;

	if (unit->object.recent_shield_damage > 0.0f || unit->object.recent_body_damage > 0.0f)
		return 1;
	for (slot = 0; slot < MAXIMUM_WEAPONS_PER_UNIT; slot++)
	{
		if (unit->unit.weapon_object_indices[slot] != NONE && unit->unit.weapon_last_used_at_game_time[slot] > 0 &&
			now - unit->unit.weapon_last_used_at_game_time[slot] < COMBAT_SECONDS * TICKS_PER_SECOND)
		{
			return 1;
		}
	}
	return 0;
}

static void gather_players(long player_index, struct shape *list, int *count)
{
	struct player_datum *player = player_get(player_index);
	struct data_iterator iterator;
	struct player_datum *other;
	long now = game_time_get();

	data_iterator_new(&iterator, player_data);
	while ((other = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
	{
		long index = DATUM_INDEX_TO_ABSOLUTE_INDEX(iterator.datum_index);

		if (index >= 0 && index < PLAYERS && modern.death_tick[index] > 0 &&
			now - modern.death_tick[index] < DEATH_SECONDS * TICKS_PER_SECOND && (other == player || !enemies(player, other)))
		{
			add_around(list, count, SHAPE_CYLINDER, CURVE_FLAT, STACKS, &modern.death_point[index], 10.0f, 8.0f, -17.0f);
			add_around(list, count, SHAPE_SPHERE, CURVE_FLAT, STACKS, &modern.death_point[index], 5.0f, 0.0f, -3.0f);
		}
		if (other->unit_index == NONE || other == player)
			continue;
		{
			real_point3d origin, eye;

			object_get_origin(other->unit_index, &origin);
			if (enemies(player, other))
			{
				add_around(list, count, SHAPE_CYLINDER, CURVE_ENEMY, STACKS, &origin, 18.0f, 18.0f, -15.0f);
				add_around(list, count, SHAPE_CYLINDER, CURVE_FLAT, STACKS, &origin, 10.0f, 3.75f, -700.0f);
				add_around(list, count, SHAPE_CYLINDER, CURVE_FLAT, STACKS, &origin, 10.0f, 10.0f, -300.0f);
				if (in_combat(other->unit_index))
					add_around(list, count, SHAPE_CYLINDER, CURVE_COMBAT, GROUP_COMBAT, &origin, 13.0f, 10.0f, -16.0f);
				unit_get_camera_position(other->unit_index, &eye);
				add_weapon_line(list, count, other->unit_index, &eye);
			}
			else
			{
				add_around(list, count, SHAPE_CYLINDER, CURVE_ALLY, GROUP_ALLY, &origin, 18.0f, 18.0f, 7.0f);
				add_around(list, count, SHAPE_CYLINDER, CURVE_FLAT, GROUP_ALLY_NEAR, &origin, 8.0f, 5.0f, -50.0f);
			}
		}
	}
}

/* a vehicle an enemy rides */
static void gather_vehicles(long player_index, struct shape *list, int *count)
{
	struct player_datum *player = player_get(player_index);
	struct data_iterator iterator;
	struct player_datum *other;

	data_iterator_new(&iterator, player_data);
	while ((other = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
	{
		long parent;
		real_point3d at;

		if (other->unit_index == NONE || other == player || !enemies(player, other))
			continue;
		parent = object_get(other->unit_index)->object.parent_object_index;
		if (parent == NONE || object_get(parent)->object.type != _object_type_vehicle)
			continue;
		object_get_origin(parent, &at);
		add_around(list, count, SHAPE_CYLINDER, CURVE_FLAT, STACKS, &at, 10.0f, 20.0f, -32.0f);
	}
}

static void gather_grenades(struct shape *list, int *count)
{
	struct object_iterator objects;

	object_iterator_new(&objects, _object_mask_projectile, 0);
	while (object_iterator_next(&objects))
	{
		struct object_datum *object = object_get(objects.index);
		struct projectile_definition *definition = projectile_definition_get(object->definition_index);
		real_point3d at;

		if (!(definition->projectile.arming_time > 0.0f && definition->projectile.danger_radius > 0.0f))
			continue;
		object_get_origin(objects.index, &at);
		add_around(list, count, SHAPE_SPHERE, CURVE_FLAT, STACKS, &at, 3.0f, 0.0f, -300.0f);
	}
}

static void add_objective(struct shape *list, int *count, real_point3d const *at, real inner, real outer, real weight)
{
	add(list, count, SHAPE_RING, CURVE_FLAT, STACKS, at, NULL, inner, outer, weight * OBJECTIVE_WEIGHT);
}

static void gather_objective(struct shape *list, int *count)
{
	struct ae_bots_objective_snapshot const *snapshot = ae_bots_objective_snapshot();
	struct data_iterator iterator;
	struct player_datum *other;
	long index;

	if (!snapshot)
		return;
	if (snapshot->mode == AE_BOTS_MODE_ODDBALL)
	{
		for (index = 0; index < snapshot->ball_count; index++)
		{
			add_objective(list, count, &snapshot->ball_positions[index], 2.0f, 6.0f, -1.0f);
			add_objective(list, count, &snapshot->ball_positions[index], 16.0f, 20.0f, 0.05f);
		}
		data_iterator_new(&iterator, player_data);
		while ((other = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
		{
			real_point3d origin;

			if (other->unit_index == NONE || !ae_bots_objective_player_carries(snapshot, iterator.datum_index))
				continue;
			object_get_origin(other->unit_index, &origin);
			add_objective(list, count, &origin, 2.0f, 6.0f, -1.0f);
			add_objective(list, count, &origin, 16.0f, 20.0f, 0.05f);
		}
	}
	if (snapshot->mode == AE_BOTS_MODE_KING && snapshot->hill)
	{
		add_objective(list, count, &snapshot->hill_centre, 4.0f, 8.0f, -1.0f);
		add_objective(list, count, &snapshot->hill_centre, 16.0f, 20.0f, 0.05f);
	}
}

/* distance over radius to strength; flat is 1 anywhere inside */
static real curve_value(int curve, real x)
{
	real value = 1.0f;

	switch (curve)
	{
	case CURVE_ALLY: value = x < 0.1f ? x * 10.0f : x < 0.7f ? 1.0f : 3.1f - 3.0f * x; break;
	case CURVE_ENEMY: value = x < 0.5f ? 1.0f : 1.9f - 1.8f * x; break;
	case CURVE_COMBAT: value = x < 0.205f ? 1.0f : 1.2579f * (1.0f - x); break;
	}
	return value < 0.0f ? 0.0f : value > 1.0f ? 1.0f : value;
}

static real shape_value(struct shape const *shape, real_point3d const *point)
{
	real dx = point->x - shape->at.x, dy = point->y - shape->at.y, dz = point->z - shape->at.z;
	real flat = dx * dx + dy * dy, distance;

	switch (shape->kind)
	{
	case SHAPE_CYLINDER:
		if (dz > shape->b * 0.5f || dz < -shape->b * 0.5f || flat > shape->a * shape->a)
			return 0.0f;
		return shape->weight * curve_value(shape->curve, (real)sqrt(flat) / shape->a);
	case SHAPE_SPHERE:
		return flat + dz * dz > shape->a * shape->a ? 0.0f : shape->weight;
	case SHAPE_CONE:
		dz += POINT_TOP;
		distance = dx * shape->axis.i + dy * shape->axis.j + dz * shape->axis.k;
		if (distance <= 0.0f || distance > shape->b)
			return 0.0f;
		flat = flat + dz * dz - distance * distance;
		{
			real radius = shape->a * distance / shape->b;

			return flat > radius * radius ? 0.0f : shape->weight;
		}
	case SHAPE_RING:
		if (dz > OBJECTIVE_HEIGHT || dz < -OBJECTIVE_HEIGHT || flat >= shape->b * shape->b)
			return 0.0f;
		if (flat <= shape->a * shape->a)
			return shape->weight;
		return shape->weight * (1.0f - ((real)sqrt(flat) - shape->a) / (shape->b - shape->a));
	}
	return 0.0f;
}

static real score_point(struct shape const *list, int count, real_point3d const *point)
{
	real total = 0.0f, strongest[GROUPS] = { 0.0f };
	int index;

	for (index = 0; index < count; index++)
	{
		real value = shape_value(&list[index], point);

		if (list[index].group == STACKS)
			total += value;
		else if (fabsf(value) > fabsf(strongest[list[index].group]))
			strongest[list[index].group] = value;
	}
	for (index = STACKS + 1; index < GROUPS; index++)
		total += strongest[index];
	return total;
}

/* a biped on the point: a corpse counts unless living_only (a unit placed on a body stands on top of it) */
static int body_on_point(long player_index, real_point3d const *point, int living_only)
{
	long own_unit = player_get(player_index)->unit_index;
	struct object_iterator objects;

	object_iterator_new(&objects, _object_mask_biped, 0);
	while (object_iterator_next(&objects))
	{
		struct object_datum *object = object_get(objects.index);
		real_point3d origin;
		real dx, dy, dz;

		if (objects.index == own_unit || (living_only && TEST_FLAG(object->object.damage_flags, _object_dead_bit)))
			continue;
		object_get_origin(objects.index, &origin);
		dx = origin.x - point->x;
		dy = origin.y - point->y;
		dz = origin.z - point->z;
		if (dx * dx + dy * dy < BODY_RADIUS * BODY_RADIUS && dz > -BODY_HEIGHT && dz < BODY_HEIGHT)
			return 1;
	}
	return 0;
}

/* the largest sight factor of a unit's weapons: a scoped weapon sees ten times as far */
static real sight_factor(long unit_index)
{
	struct unit_datum *unit = (struct unit_datum *)object_get(unit_index);
	int slot;

	for (slot = 0; slot < MAXIMUM_WEAPONS_PER_UNIT; slot++)
	{
		if (unit->unit.weapon_object_indices[slot] != NONE && weapon_named(unit->unit.weapon_object_indices[slot], "sniper"))
			return SIGHT_FACTOR_SCOPED;
	}
	return 1.0f;
}

/* one viewer against one point: the cone tests first, the ray last. Returns 1 when the viewer sees the point */
static int viewer_sees(long unit_index, real range, real_point3d const *top, real_vector3d const *facing)
{
	struct unit_datum *unit = (struct unit_datum *)object_get(unit_index);
	struct collision_result collision;
	real_point3d eye;
	real_vector3d ray;
	real distance;

	unit_get_camera_position(unit_index, &eye);
	ray.i = top->x - eye.x;
	ray.j = top->y - eye.y;
	ray.k = top->z - eye.z;
	distance = (real)sqrt(ray.i * ray.i + ray.j * ray.j + ray.k * ray.k);
	if (distance > range || distance <= 0.0f)
		return 0;
	if ((-(ray.i * facing->i + ray.j * facing->j)) / distance < SIGHT_CONE &&
		(ray.i * unit->unit.aiming_vector.i + ray.j * unit->unit.aiming_vector.j + ray.k * unit->unit.aiming_vector.k) / distance < SIGHT_CONE)
	{
		return 0;
	}
	modern.rays++;
	return !collision_test_vector(FLAG(_collision_test_structure_bit) | FLAG(_collision_test_front_facing_surfaces_bit),
		&eye, &ray, NONE, &collision);
}

static int point_seen(long player_index, struct player_starting_location const *location, int enemies_only)
{
	struct player_datum *player = player_get(player_index);
	struct data_iterator iterator;
	struct player_datum *other;
	real_point3d top = location->position;
	real_vector3d facing;

	top.z += POINT_TOP;
	facing.i = (real)cos(location->facing);
	facing.j = (real)sin(location->facing);
	facing.k = 0.0f;
	data_iterator_new(&iterator, player_data);
	while ((other = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
	{
		int enemy;

		if (other == player || other->unit_index == NONE)
			continue;
		enemy = enemies(player, other);
		if (!enemy && enemies_only)
			continue;
		if (viewer_sees(other->unit_index,
			(enemy ? SIGHT_RANGE * sight_factor(other->unit_index) : SIGHT_RANGE_ALLY) * modern.scale, &top, &facing))
		{
			return 1;
		}
	}
	return 0;
}

/* the team of the nearest CTF flag; the point's own team index on a map without flags */
static short point_side(struct player_starting_location const *location)
{
	struct scenario *scenario = global_scenario_get();
	short side = location->team_index;
	real nearest = 0.0f;
	int index, found = 0;

	for (index = 0; index < scenario->netgame_flags.count; index++)
	{
		struct scenario_netgame_flag *flag = TAG_BLOCK_GET_ELEMENT(&scenario->netgame_flags, index,
			struct scenario_netgame_flag);
		real distance;

		if (flag->type != _netgame_flag_ctf_flag)
			continue;
		distance = distance3d(&flag->position, &location->position);
		if (!found || distance < nearest)
		{
			found = 1;
			nearest = distance;
			side = flag->team_index;
		}
	}
	return side;
}

struct candidate { short index; real score; };

static int candidate_order(void const *a, void const *b)
{
	struct candidate const *left = a, *right = b;

	if (left->score != right->score)
		return left->score > right->score ? -1 : 1;
	return left->index - right->index;
}

/* FLIP: at least this far from every CTF flag */
static int flip_point(struct player_starting_location const *location)
{
	struct scenario *scenario = global_scenario_get();
	short index;

	for (index = 0; index < scenario->netgame_flags.count; index++)
	{
		struct scenario_netgame_flag *flag = TAG_BLOCK_GET_ELEMENT(&scenario->netgame_flags, index,
			struct scenario_netgame_flag);

		if (flag->type == _netgame_flag_ctf_flag && distance3d(&flag->position, &location->position) < FLIP_FLAG_DISTANCE)
			return 0;
	}
	return 1;
}

static struct
{
	long check_tick;
	long since[2], last_held[2];
	int on[2];
} flip;

/* FLIP: whether each flag team's base is held, twice a second */
static void flip_update(void)
{
	struct scenario *scenario = global_scenario_get();
	long now = game_time_get();
	short index;

	if (now < flip.check_tick - TICKS_PER_SECOND)
		csmemset(&flip, 0, sizeof(flip));
	if (now < flip.check_tick || !teams())
		return;
	flip.check_tick = now + TICKS_PER_SECOND / 2;
	for (index = 0; index < scenario->netgame_flags.count; index++)
	{
		struct scenario_netgame_flag *flag = TAG_BLOCK_GET_ELEMENT(&scenario->netgame_flags, index,
			struct scenario_netgame_flag);
		struct data_iterator iterator;
		struct player_datum *player;
		int team = flag->team_index, enemies_there = 0, own_there = 0;

		if (flag->type != _netgame_flag_ctf_flag || team < 0 || team > 1)
			continue;
		data_iterator_new(&iterator, player_data);
		while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
		{
			real_point3d at;

			if (player->unit_index == NONE || TEST_FLAG(object_get(player->unit_index)->object.damage_flags, _object_dead_bit))
				continue;
			object_get_origin(player->unit_index, &at);
			if (distance3d(&at, &flag->position) > FLIP_BASE_RADIUS)
				continue;
			if (player->team_index == team)
				own_there++;
			else
				enemies_there++;
		}
		if (enemies_there > 0 && enemies_there >= own_there)
		{
			flip.last_held[team] = now;
			if (!flip.since[team])
				flip.since[team] = now;
			if (!flip.on[team] && now - flip.since[team] >= FLIP_HOLD_SECONDS * TICKS_PER_SECOND)
			{
				short count = 0, point;

				for (point = 0; point < player_get_starting_location_count(); point++)
					count += flip_point(player_get_starting_location(point)) != 0;
				if (count >= FLIP_POINTS_NEEDED)
				{
					flip.on[team] = 1;
					platform_log("ae spawn flip: team %d's base held, spawns away from the flags at %d points", team, (int)count);
				}
			}
		}
		else if (flip.since[team] && now - flip.last_held[team] > FLIP_GAP_SECONDS * TICKS_PER_SECOND)
		{
			if (flip.on[team])
				platform_log("ae spawn flip: team %d's spawns back home", team);
			flip.since[team] = 0;
			flip.on[team] = 0;
		}
	}
}

/* (HALO_AE_SPAWN_LOG=1) a spawn's nearest enemy, whether an enemy has it in sight; arms the checks after a spawn */
static void ae_spawn_modern_log_spawn(long player_index, long location_index)
{
	struct player_datum *player = player_get(player_index);
	long index = DATUM_INDEX_TO_ABSOLUTE_INDEX(player_index);
	struct player_starting_location const *location = player_get_starting_location((short)location_index);
	struct data_iterator iterator;
	struct player_datum *other;
	real nearest = 1000.0f;

	if (!modern.log || index < 0 || index >= PLAYERS)
		return;
	data_iterator_new(&iterator, player_data);
	while ((other = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
	{
		real_point3d origin;
		real distance;

		if (other == player || other->unit_index == NONE || !enemies(player, other))
			continue;
		object_get_origin(other->unit_index, &origin);
		distance = distance3d(&origin, &location->position);
		if (distance < nearest)
			nearest = distance;
	}
	modern.spawn_tick[index] = game_time_get();
	modern.spawn_hurt[index] = 0;
	modern.spawn_z[index] = location->position.z;
	modern.spawn_checked[index] = 0;
	platform_log("ae spawn: player %ld at %ld, nearest enemy %.1f wu%s%s", index, location_index, (double)nearest,
		nearest < 6.0f ? " close" : "", point_seen(player_index, location, 1) ? " seen" : "");
}

long ae_spawn_modern_choose(long player_index, short location_count)
{
	static struct shape list[SHAPES];
	static struct candidate points[POINTS];
	short valid[POINTS];
	int count = 0, point_count = 0, penalised = 0, sight_rejected = 0, relaxed = 0, first, last, valid_count;
	short index;
	clock_t started = clock();
	long chosen = NONE;
	real chosen_score = 0.0f;
	short own_team = player_get(player_index)->team_index;
	int flipped = teams() && own_team >= 0 && own_team <= 1 && flip.on[own_team];

	gather_players(player_index, list, &count);
	gather_vehicles(player_index, list, &count);
	gather_grenades(list, &count);
	gather_objective(list, &count);
	for (index = 0; index < location_count && point_count < POINTS; index++)
	{
		struct player_starting_location *location = player_get_starting_location(index);

		if (flipped ? !flip_point(location) : !(game_engine_get_starting_location_rating(player_index, location) > 0.0f))
			continue;
		points[point_count].index = index;
		points[point_count].score = score_point(list, count, &location->position);
		penalised += points[point_count].score < 0.0f;
		point_count++;
	}
	/* (a side only where the eligible points hold the player's team and another) */
	if (teams() && !flipped)
	{
		short team = player_get(player_index)->team_index;
		int own = 0, other = 0, marked = 0, point;

		for (point = 0; point < point_count; point++)
		{
			struct player_starting_location *location = player_get_starting_location(points[point].index);

			marked |= location->team_index != player_get_starting_location(points[0].index)->team_index;
			if (point_side(location) == team)
				own++;
			else
				other++;
		}
		if (marked && own > 0 && other > 0)
		{
			for (point = 0; point < point_count; point++)
			{
				if (point_side(player_get_starting_location(points[point].index)) == team)
					points[point].score += SIDE_WEIGHT;
			}
		}
	}
	qsort(points, point_count, sizeof(points[0]), candidate_order);
	modern.rays = 0;
	for (relaxed = 0; relaxed < 3 && chosen == NONE; relaxed++)
	{
		for (first = 0; first < point_count && chosen == NONE; first = last)
		{
			real step = (real)floor(points[first].score / STEP);

			valid_count = 0;
			for (last = first; last < point_count && (real)floor(points[last].score / STEP) == step; last++)
			{
				struct player_starting_location *location = player_get_starting_location(points[last].index);

				if (body_on_point(player_index, &location->position, relaxed == 2))
					continue;
				if (!relaxed && modern.rays < RAY_CAP && point_seen(player_index, location, 0))
				{
					sight_rejected++;
					continue;
				}
				valid[valid_count++] = (short)last;
			}
			if (valid_count > 0)
			{
				int draw = (int)PIN((int)(real_random_range(0.0f, 1.0f) * valid_count), 0, valid_count - 1);

				chosen = points[valid[draw]].index;
				chosen_score = points[valid[draw]].score;
				break;
			}
		}
		if (chosen != NONE)
			break;
	}
	if (chosen == NONE)
		modern.waits++;
	if (chosen != NONE)
		ae_spawn_modern_log_spawn(player_index, chosen);
	if (modern.log && chosen != NONE)
	{
		platform_log("ae spawn modern: player %ld, %ld waits before, %d points, %d with a penalty, %d rejected by sight, %d rays%s, step of "
			"best %.1f, chosen %ld at %.1f, relaxed %d, %.0f us, team %d, point side %d", DATUM_INDEX_TO_ABSOLUTE_INDEX(player_index), modern.waits, point_count,
			penalised, sight_rejected, modern.rays, modern.rays >= RAY_CAP ? " (cap)" : "",
			point_count ? (double)points[0].score : 0.0, chosen, (double)chosen_score, relaxed,
			(double)(clock() - started) * 1000000.0 / CLOCKS_PER_SEC, (int)player_get(player_index)->team_index,
			chosen == NONE ? -1 : (int)point_side(player_get_starting_location((short)chosen)));
	}
	return chosen;
}

void ae_spawn_modern_player_killed(long player_index)
{
	struct player_datum *player = player_get(player_index);
	long index = DATUM_INDEX_TO_ABSOLUTE_INDEX(player_index);

	settings_read();
	if (index < 0 || index >= PLAYERS)
		return;
	if (modern.log && modern.spawn_tick[index] > 0 && game_time_get() - modern.spawn_tick[index] < 5 * TICKS_PER_SECOND)
	{
		platform_log("ae spawn: player %ld died %.1f s after spawning", index,
			(double)(game_time_get() - modern.spawn_tick[index]) / TICKS_PER_SECOND);
	}
	modern.death_tick[index] = 0;
	modern.spawn_tick[index] = 0;
	if (player->unit_index != NONE)
	{
		object_get_origin(player->unit_index, &modern.death_point[index]);
		modern.death_tick[index] = game_time_get();
	}
}

void ae_spawn_modern_update(void)
{
	struct data_iterator iterator;
	struct player_datum *player;
	long now;

	settings_read();
	if (ae_spawn_modern_allowed())
		flip_update();
	if (!modern.log || !game_engine_running())
		return;
	now = game_time_get();
	data_iterator_new(&iterator, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
	{
		long index = DATUM_INDEX_TO_ABSOLUTE_INDEX(iterator.datum_index);
		struct unit_datum *unit;

		/* (the log: a unit well above its point a third of a second on) */
		if (index >= 0 && index < PLAYERS && player->unit_index != NONE && modern.spawn_tick[index] > 0 && !modern.spawn_checked[index] &&
			now - modern.spawn_tick[index] >= TICKS_PER_SECOND / 3)
		{
			real_point3d origin;

			modern.spawn_checked[index] = 1;
			object_get_origin(player->unit_index, &origin);
			if (origin.z - modern.spawn_z[index] > 1.0f)
				platform_log("ae spawn: player %ld placed %.1f wu above its point", index, (double)(origin.z - modern.spawn_z[index]));
		}
		if (index < 0 || index >= PLAYERS || player->unit_index == NONE || modern.spawn_tick[index] <= 0 || modern.spawn_hurt[index] ||
			now - modern.spawn_tick[index] >= 3 * TICKS_PER_SECOND)
		{
			continue;
		}
		unit = (struct unit_datum *)object_get(player->unit_index);
		if (unit->object.recent_shield_damage > 0.0f || unit->object.recent_body_damage > 0.0f)
		{
			modern.spawn_hurt[index] = 1;
			platform_log("ae spawn: player %ld damaged %.1f s after spawning", index,
				(double)(now - modern.spawn_tick[index]) / TICKS_PER_SECOND);
		}
	}
}
