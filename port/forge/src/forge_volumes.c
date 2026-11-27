/*
FORGE_VOLUMES.C

Forge's damage, kill, gravity and one-way volumes (forge_volumes.h): what each is,
kept by its live instance's index; their rows in Object Settings, their saved
lines and packets; the map's deadly trigger volumes adopted and written back;
the units in them hurt each tick; the gravity at a point, for the engine's
physics, and the bipeds kept from falling damage; the ONE WAY floors; their edges drawn in Forge mode; and the emitters whose DEAL DAMAGE
is off.
*/

#include "cseries.h"
#include "cseries/errors.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "game/game.h"
#include "objects/objects.h"
#include "objects/object_definitions.h"
#include "units/units.h"
#include "effects/effect_definitions.h"
#include "scenario/scenario.h"
#include "scenario/scenario_definitions.h"
#include "tag_files/tag_groups.h"

#include "forge_engine.h"
#include "forge_volumes.h"

/* ---------- externs */

/* death_volumes.c's: a trigger volume the map kills in */
extern boolean death_volume_is_deadly(short volume_index);

/* ---------- constants */

#define OBJECT_DEFINITION_GROUP_TAG 'obje'
#define EFFECT_GROUP_TAG 'effe'
#define DAMAGE_EFFECT_GROUP_TAG 'jpt!'

/* the units in a volume of APPLY DAMAGE last tick, so a unit is hurt once
as it comes into one */
#define MAXIMUM_VOLUME_OCCUPANTS 256

/* (the line colors: forge_runtime_tags.c's for each) */
enum
{
	_volume_color_damage = 0,
	_volume_color_kill,
	_volume_color_off,
	_volume_color_gravity,
	_volume_color_one_way,
	_volume_color_patrol,
	/* (an AI PATROL volume's PURSUIT RANGE, round it) */
	_volume_color_pursuit
};

/* GRAVITY's STRENGTH (hundredths of the world's gravity): a new one's, and
the most */
#define VOLUME_GRAVITY_DEFAULT 50.0f
#define VOLUME_GRAVITY_MAXIMUM 1000.0f
/* (the GRAVITY volumes the engine's physics asks of, a tick; and the ONE
WAY ones) */
#define MAXIMUM_GRAVITY_VOLUMES 256
#define MAXIMUM_ONE_WAY_VOLUMES 256
/* (the bipeds kept from falling damage till they land) */
#define MAXIMUM_FALL_PROTECTED 64
/* (the volumes with a FALL DAMAGE or REMOVE INERTIA of their own; and the
bipeds in REMOVE INERTIA ones, each with the volume and the tick it was last
in it) */
#define MAXIMUM_FLAGGED_VOLUMES 256
#define MAXIMUM_INERTIA_OCCUPANTS 128
/* (how far under a ONE WAY top a biped's pill may start and still be on it:
a walk's push into the ground, a slope's step) */
#define ONE_WAY_TOLERANCE 0.06f

/* ---------- globals */

static struct forge_volume s_volumes[FORGE_MAX_LIVE_INSTANCES];

static struct
{
	long unit_index;
	int32_t live_idx;
} s_occupants[MAXIMUM_VOLUME_OCCUPANTS], s_occupants_next[MAXIMUM_VOLUME_OCCUPANTS];
static int s_occupant_count;
static long s_last_tick = -1;

/* the GRAVITY volumes, as the physics asks of them: their frames and pulls,
made again each tick (or when one changes), since everything that moves
asks */
static struct gravity_volume
{
	uint8_t shape;
	float middle[3];
	float forward[3], left[3], up[3];
	float half[3];
	float bound;
	/* (units of the world's gravity, the way things fall) */
	float pull[3];
	/* (FORGE_VOLUME_FLAG_*) */
	uint8_t flags;
	int32_t live_idx;
} s_gravity[MAXIMUM_GRAVITY_VOLUMES], s_one_way[MAXIMUM_ONE_WAY_VOLUMES], s_flagged[MAXIMUM_FLAGGED_VOLUMES];
static int s_gravity_count;
static int s_one_way_count;
static int s_flagged_count;
static struct
{
	long biped_index;
	int32_t live_idx;
	long tick;
} s_inertia_occupants[MAXIMUM_INERTIA_OCCUPANTS];
/* the bipeds out of a FALL DAMAGE OFF volume, not yet landed (their
indices; NONE: a free slot) */
static long s_fall_protected[MAXIMUM_FALL_PROTECTED];
static int s_fall_protected_next;
static int s_fall_protected_ready;
static long s_gravity_time = -1;
static int s_gravity_dirty = 1;
/* (things at rest in them woken: when one changes, and once a second) */
static int s_gravity_wake = 1;
static long s_gravity_wake_time = -1;

static char const *const s_shape_names[NUMBER_OF_FORGE_VOLUME_SHAPES] = { "CUBE", "CYLINDER", "SPHERE", "MAP'S OWN" };
static char const *const s_effect_names[NUMBER_OF_FORGE_VOLUME_EFFECTS] = {
	"APPLY DAMAGE", "CONSTANT DAMAGE", "INSTA KILL", "MAP SCRIPT", "OFF", "GRAVITY", "ONE WAY", "AI PATROL"
};

/* ---------- private code */

static struct forge_live_instance *volume_instance(int32_t live_idx)
{
	struct forge_live_instance *inst;

	if (live_idx < 0 || live_idx >= g_forge.live_instance_count)
		return NULL;
	inst = &g_forge.live_instances[live_idx];
	return inst->active && inst->group_tag == FORGE_GROUP_VOLUME && s_volumes[live_idx].used ? inst : NULL;
}

static float size_clamp(float size)
{
	return size < FORGE_VOLUME_SIZE_MIN ? FORGE_VOLUME_SIZE_MIN : size > FORGE_VOLUME_SIZE_MAX ? FORGE_VOLUME_SIZE_MAX : size;
}

/* (the panel's pages: forge_volume_rows) */
enum
{
	VOLUME_PAGE_MAIN = 0,
	VOLUME_PAGE_EFFECT,
	VOLUME_PAGE_SIZE
};

static int32_t s_page_live_idx = -1;
static int s_page = VOLUME_PAGE_MAIN;
static int s_page_return_row = _forge_volume_row_effect_page;
/* KEEP RATIO: on for one volume, its sizes as it was turned on and the
factor they are times */
static int32_t s_ratio_live_idx = -1;
static float s_ratio_base[3];
static float s_ratio_factor = 1.0f;

/* the shape a path places */
static int path_shape(char const *tag_path)
{
	if (!tag_path)
		return _forge_volume_box;
	if (!_stricmp(tag_path, FORGE_VOLUME_CYLINDER_PATH))
		return _forge_volume_cylinder;
	if (!_stricmp(tag_path, FORGE_VOLUME_SPHERE_PATH))
		return _forge_volume_sphere;
	if (!_stricmp(tag_path, FORGE_VOLUME_MAP_PATH))
		return _forge_volume_map_box;
	return _forge_volume_box;
}

static char const *shape_path(int shape)
{
	switch (shape)
	{
	case _forge_volume_cylinder: return FORGE_VOLUME_CYLINDER_PATH;
	case _forge_volume_sphere: return FORGE_VOLUME_SPHERE_PATH;
	case _forge_volume_map_box: return FORGE_VOLUME_MAP_PATH;
	default: return FORGE_VOLUME_BOX_PATH;
	}
}

/* (forge_3d_mode.c's, for an instance's turn) */
static void euler_basis(float yaw, float pitch, float roll, real_vector3d *forward, real_vector3d *left, real_vector3d *up)
{
	float cy = (float)cos((double)yaw), sy = (float)sin((double)yaw);
	float cp = (float)cos((double)pitch), sp = (float)sin((double)pitch);
	float cr = (float)cos((double)roll), sr = (float)sin((double)roll);

	forward->i = cy * cp;
	forward->j = sy * cp;
	forward->k = sp;
	up->i = -cy * sp * cr + sy * sr;
	up->j = -sy * sp * cr - cy * sr;
	up->k = cp * cr;
	left->i = up->j * forward->k - up->k * forward->j;
	left->j = up->k * forward->i - up->i * forward->k;
	left->k = up->i * forward->j - up->j * forward->i;
}

static void basis_to_euler(real_vector3d const *forward, real_vector3d const *up, float *yaw, float *pitch, float *roll)
{
	float fk = PIN(forward->k, -1.0f, 1.0f);

	*pitch = (float)asin((double)fk);
	if (fk > 0.9995f || fk < -0.9995f)
	{
		*yaw = fk > 0.0f ? (float)atan2((double)-up->j, (double)-up->i) : (float)atan2((double)up->j, (double)up->i);
		*roll = 0.0f;
	}
	else
	{
		float cy, sy, cp, sp;

		*yaw = (float)atan2((double)forward->j, (double)forward->i);
		cy = (float)cos((double)*yaw);
		sy = (float)sin((double)*yaw);
		cp = (float)cos((double)*pitch);
		sp = (float)sin((double)*pitch);
		/* (up = -cy*sp*cr + sy*sr, -sy*sp*cr - cy*sr, cp*cr) */
		*roll = (float)atan2((double)(sy * up->i - cy * up->j), (double)(up->k / (cp != 0.0f ? cp : 1.0f)));
		(void)sp;
	}
}

/* a point in the volume's own frame: along its forward, left and up from
its middle */
static void local_point(struct forge_live_instance const *inst, real_point3d const *point, float local[3])
{
	real_vector3d forward, left, up;
	float dx = point->x - inst->x, dy = point->y - inst->y, dz = point->z - inst->z;

	euler_basis(inst->yaw, inst->pitch, inst->roll, &forward, &left, &up);
	local[0] = dx * forward.i + dy * forward.j + dz * forward.k;
	local[1] = dx * left.i + dy * left.j + dz * left.k;
	local[2] = dx * up.i + dy * up.j + dz * up.k;
}

static int volume_contains(struct forge_live_instance const *inst, struct forge_volume const *volume,
	real_point3d const *point)
{
	float local[3];
	float hx = volume->size[0] * 0.5f, hy = volume->size[1] * 0.5f, hz = volume->size[2] * 0.5f;
	float bound = hx > hy ? hx : hy;

	bound = bound > hz ? bound : hz;
	/* (far from its middle: in none of its shapes) */
	if (fabs((double)(point->x - inst->x)) > bound * 1.75f || fabs((double)(point->y - inst->y)) > bound * 1.75f ||
		fabs((double)(point->z - inst->z)) > bound * 1.75f)
	{
		return 0;
	}
	local_point(inst, point, local);
	switch (volume->shape)
	{
	case _forge_volume_cylinder:
	{
		float u = local[0] / hx, v = local[1] / hy;

		return u * u + v * v <= 1.0f && fabs((double)local[2]) <= hz;
	}
	case _forge_volume_sphere:
	{
		float u = local[0] / hx, v = local[1] / hy, w = local[2] / hz;

		return u * u + v * v + w * w <= 1.0f;
	}
	default:
		return fabs((double)local[0]) <= hx && fabs((double)local[1]) <= hy && fabs((double)local[2]) <= hz;
	}
}

/* a map's volume put back in its trigger volume: where it is, as big as it
is; with an effect of Forge's (or OFF), empty, so its scripts kill in it no
more */
static void map_volume_write(int32_t live_idx)
{
	struct scenario *scenario = global_scenario_try_and_get();
	struct forge_live_instance const *inst = volume_instance(live_idx);
	struct forge_volume *volume = &s_volumes[live_idx];
	struct scenario_trigger_volume *trigger;
	real_vector3d forward, left, up;

	if (!scenario || !inst || volume->map_index < 0 || volume->map_index >= scenario->trigger_volumes.count)
		return;
	trigger = TAG_BLOCK_GET_ELEMENT(&scenario->trigger_volumes, volume->map_index, struct scenario_trigger_volume);
	euler_basis(inst->yaw, inst->pitch, inst->roll, &forward, &left, &up);
	trigger->type = _scenario_trigger_volume_type_oriented;
	trigger->forward = forward;
	trigger->up = up;
	trigger->position.x = inst->x - 0.5f * (forward.i * volume->size[0] + left.i * volume->size[1] + up.i * volume->size[2]);
	trigger->position.y = inst->y - 0.5f * (forward.j * volume->size[0] + left.j * volume->size[1] + up.j * volume->size[2]);
	trigger->position.z = inst->z - 0.5f * (forward.k * volume->size[0] + left.k * volume->size[1] + up.k * volume->size[2]);
	if (volume->effect == _forge_volume_map_script)
	{
		trigger->extents.i = volume->size[0];
		trigger->extents.j = volume->size[1];
		trigger->extents.k = volume->size[2];
	}
	else
	{
		trigger->extents.i = trigger->extents.j = trigger->extents.k = 0.0f;
	}
}

static void volume_changed(int32_t live_idx)
{
	s_volumes[live_idx].edited = 1;
	s_gravity_dirty = 1;
	s_gravity_wake = 1;
	if (s_volumes[live_idx].map_index >= 0)
		map_volume_write(live_idx);
}

/* a unit's health taken by a part of all it has (shields first), killed
when none is left */
static void unit_hurt(long unit_index, struct unit_datum *unit, float fraction)
{
	float body_max = unit->object.maximum_body_vitality > 0.0f ? unit->object.maximum_body_vitality : 1.0f;
	float shield_max = unit->object.maximum_shield_vitality > 0.0f ? unit->object.maximum_shield_vitality : 0.0f;
	float points = fraction * (body_max + shield_max);

	if (points <= 0.0f)
		return;
	if (unit->object.shield_stun_ticks < TICKS_PER_SECOND)
		unit->object.shield_stun_ticks = TICKS_PER_SECOND;
	if (shield_max > 0.0f && unit->object.shield_vitality > 0.0f)
	{
		float shield = unit->object.shield_vitality * shield_max;

		if (shield >= points)
		{
			unit->object.shield_vitality = (shield - points) / shield_max;
			return;
		}
		points -= shield;
		unit->object.shield_vitality = 0.0f;
	}
	unit->object.body_vitality -= points / body_max;
	if (unit->object.body_vitality <= 0.0f)
	{
		unit->object.body_vitality = 0.0f;
		unit_kill(unit_index);
	}
}

static int occupant_was(long unit_index, int32_t live_idx)
{
	int k;

	for (k = 0; k < s_occupant_count; k++)
	{
		if (s_occupants[k].unit_index == unit_index && s_occupants[k].live_idx == live_idx)
			return 1;
	}
	return 0;
}

/* the units in the volumes hurt, for `ticks` ticks */
static void volumes_hurt(int ticks)
{
	struct object_iterator iterator;
	struct unit_datum *unit;
	int next_count = 0;
	int32_t i;
	int any = 0;

	for (i = 0; i < g_forge.live_instance_count && !any; i++)
	{
		struct forge_live_instance const *inst = volume_instance(i);

		any = inst && s_volumes[i].effect <= _forge_volume_insta_kill;
	}
	if (!any)
	{
		s_occupant_count = 0;
		return;
	}
	object_iterator_new(&iterator, _object_mask_biped, 0);
	while ((unit = (struct unit_datum *)object_iterator_next(&iterator)) != NULL)
	{
		long unit_index = iterator.index;

		if (TEST_FLAG(unit->object.damage_flags, _object_dead_bit) ||
			TEST_FLAG(unit->object.damage_flags, _object_cannot_take_damage_bit))
		{
			continue;
		}
		for (i = 0; i < g_forge.live_instance_count; i++)
		{
			struct forge_live_instance const *inst = volume_instance(i);
			struct forge_volume const *volume = &s_volumes[i];

			if (!inst || volume->effect > _forge_volume_insta_kill ||
				!volume_contains(inst, volume, &unit->object.bounding_sphere_center))
			{
				continue;
			}
			if (volume->effect == _forge_volume_insta_kill)
			{
				unit->object.shield_vitality = 0.0f;
				unit->object.body_vitality = 0.0f;
				unit_kill(unit_index);
				error(_error_silent, "forge volumes: a kill volume killed a unit at %.2f %.2f %.2f", unit->object.position.x, unit->object.position.y, unit->object.position.z); /* AE hook */
				break;
			}
			if (volume->effect == _forge_volume_constant_damage)
			{
				unit_hurt(unit_index, unit, volume->damage / 100.0f / (float)TICKS_PER_SECOND * (float)ticks);
			}
			else
			{
				if (!occupant_was(unit_index, i))
					unit_hurt(unit_index, unit, volume->damage / 100.0f);
				if (next_count < MAXIMUM_VOLUME_OCCUPANTS)
				{
					s_occupants_next[next_count].unit_index = unit_index;
					s_occupants_next[next_count].live_idx = i;
					next_count++;
				}
			}
			if (TEST_FLAG(unit->object.damage_flags, _object_dead_bit))
				break;
		}
	}
	memcpy(s_occupants, s_occupants_next, (size_t)next_count * sizeof(s_occupants[0]));
	s_occupant_count = next_count;
}

/* the bipeds kept from falling damage forgotten */
static void fall_protected_clear(void)
{
	int k;

	for (k = 0; k < MAXIMUM_FALL_PROTECTED; k++)
		s_fall_protected[k] = NONE;
	s_fall_protected_next = 0;
	for (k = 0; k < MAXIMUM_INERTIA_OCCUPANTS; k++)
	{
		s_inertia_occupants[k].biped_index = NONE;
		s_inertia_occupants[k].live_idx = -1;
		s_inertia_occupants[k].tick = -1;
	}
	s_fall_protected_ready = 1;
}

/* ---------- public code */

struct forge_volume const *forge_volume_get(int32_t live_idx)
{
	return volume_instance(live_idx) ? &s_volumes[live_idx] : NULL;
}

int forge_volume_is(int32_t live_idx)
{
	return volume_instance(live_idx) != NULL;
}

void forge_volume_spawned(int32_t live_idx, char const *tag_path)
{
	struct forge_volume *volume;
	int shape = path_shape(tag_path);

	if (live_idx < 0 || live_idx >= FORGE_MAX_LIVE_INSTANCES)
		return;
	volume = &s_volumes[live_idx];
	/* (a map's, made again at its place by a peer's spawn: as adopted) */
	if (shape == _forge_volume_map_box && volume->used && volume->map_index >= 0)
		return;
	memset(volume, 0, sizeof(*volume));
	volume->used = 1;
	volume->shape = (uint8_t)(shape == _forge_volume_map_box ? _forge_volume_box : shape);
	volume->effect = _forge_volume_insta_kill;
	volume->map_index = NONE;
	volume->size[0] = volume->size[1] = volume->size[2] = FORGE_VOLUME_SIZE_DEFAULT;
	volume->damage = 25.0f;
}

void forge_volume_moved(int32_t live_idx)
{
	if (volume_instance(live_idx))
		volume_changed(live_idx);
}

void forge_volume_apply(int32_t live_idx, int shape, int effect, float damage, float const size[3], int flags)
{
	struct forge_volume *volume;
	int k;

	if (!volume_instance(live_idx))
		return;
	volume = &s_volumes[live_idx];
	/* (a map's keeps its box; Forge's are one of the three) */
	if (volume->map_index < 0 && shape >= 0 && shape < _forge_volume_map_box)
		volume->shape = (uint8_t)shape;
	if (effect >= 0 && effect < NUMBER_OF_FORGE_VOLUME_EFFECTS &&
		(effect != _forge_volume_map_script || volume->map_index >= 0))
	{
		volume->effect = (uint8_t)effect;
	}
	volume->damage = PIN(damage, 0.0f, VOLUME_GRAVITY_MAXIMUM);
	volume->flags = (uint8_t)(flags & FORGE_VOLUME_FLAGS_ALL);
	for (k = 0; k < 3; k++)
		volume->size[k] = size_clamp(size[k]);
	volume_changed(live_idx);
}

void forge_volume_set(int32_t live_idx, struct forge_volume const *settings, int broadcast)
{
	if (!settings || !volume_instance(live_idx))
		return;
	forge_volume_apply(live_idx, settings->shape, settings->effect, settings->damage, settings->size, settings->flags);
	if (broadcast)
		forge_network_send_volume(live_idx);
}

void forge_volumes_level_reset(void)
{
	memset(s_volumes, 0, sizeof(s_volumes));
	s_page_live_idx = -1;
	s_page = VOLUME_PAGE_MAIN;
	s_ratio_live_idx = -1;
	s_occupant_count = 0;
	s_last_tick = -1;
	s_gravity_count = 0;
	s_one_way_count = 0;
	s_flagged_count = 0;
	s_gravity_time = -1;
	s_gravity_dirty = 1;
	s_gravity_wake = 1;
	s_gravity_wake_time = -1;
	fall_protected_clear();
}

void forge_volumes_adopt_map(void)
{
	struct scenario *scenario = global_scenario_try_and_get();
	short index;
	int adopted = 0;
	int32_t i;

	/* (once a level: already adopted, the level's are there) */
	for (i = 0; i < g_forge.live_instance_count; i++)
	{
		if (volume_instance(i) && s_volumes[i].map_index >= 0)
			return;
	}
	forge_volumes_level_reset();
	/* (a multiplayer map's alone: Forge edits no other, and a campaign
	level's triggers are its story's) */
	if (!scenario || scenario->type != 1)
		return;
	for (index = 0; index < scenario->trigger_volumes.count; index++)
	{
		struct scenario_trigger_volume const *trigger =
			TAG_BLOCK_GET_ELEMENT(&scenario->trigger_volumes, index, struct scenario_trigger_volume);
		real_vector3d forward, up, left;
		float size[3];
		float yaw = 0.0f, pitch = 0.0f, roll = 0.0f;
		real_point3d middle;
		int32_t live_idx;

		if (!death_volume_is_deadly(index))
			continue;
		if (trigger->type == _scenario_trigger_volume_type_axis_aligned)
		{
			size[0] = trigger->bounds.x1 - trigger->bounds.x0;
			size[1] = trigger->bounds.y1 - trigger->bounds.y0;
			size[2] = trigger->bounds.z1 - trigger->bounds.z0;
			middle.x = 0.5f * (trigger->bounds.x0 + trigger->bounds.x1);
			middle.y = 0.5f * (trigger->bounds.y0 + trigger->bounds.y1);
			middle.z = 0.5f * (trigger->bounds.z0 + trigger->bounds.z1);
		}
		else
		{
			forward = trigger->forward;
			up = trigger->up;
			left.i = up.j * forward.k - up.k * forward.j;
			left.j = up.k * forward.i - up.i * forward.k;
			left.k = up.i * forward.j - up.j * forward.i;
			size[0] = trigger->extents.i;
			size[1] = trigger->extents.j;
			size[2] = trigger->extents.k;
			middle.x = trigger->position.x + 0.5f * (forward.i * size[0] + left.i * size[1] + up.i * size[2]);
			middle.y = trigger->position.y + 0.5f * (forward.j * size[0] + left.j * size[1] + up.j * size[2]);
			middle.z = trigger->position.z + 0.5f * (forward.k * size[0] + left.k * size[1] + up.k * size[2]);
			basis_to_euler(&forward, &up, &yaw, &pitch, &roll);
		}
		if (size[0] <= 0.0f || size[1] <= 0.0f || size[2] <= 0.0f)
			continue;
		live_idx = forge_3d_spawn_object_at(FORGE_GROUP_VOLUME, FORGE_VOLUME_MAP_PATH, 1, middle.x, middle.y, middle.z,
			yaw, pitch, roll, 0, 0);
		if (live_idx < 0)
			break;
		s_volumes[live_idx].used = 1;
		s_volumes[live_idx].shape = _forge_volume_map_box;
		s_volumes[live_idx].effect = _forge_volume_map_script;
		s_volumes[live_idx].map_index = index;
		s_volumes[live_idx].size[0] = size[0];
		s_volumes[live_idx].size[1] = size[1];
		s_volumes[live_idx].size[2] = size[2];
		s_volumes[live_idx].damage = 25.0f;
		s_volumes[live_idx].edited = 0;
		snprintf(g_forge.live_instances[live_idx].label, sizeof(g_forge.live_instances[live_idx].label), "%s",
			trigger->name);
		adopted++;
	}
	if (adopted)
		error(_error_silent, "forge volumes: %d of the map's deadly trigger volumes adopted", adopted);
}

void forge_volume_describe(int32_t live_idx, char *text, unsigned long size)
{
	struct forge_volume const *volume = forge_volume_get(live_idx);
	struct forge_live_instance const *inst = volume_instance(live_idx);

	if (!volume || !inst)
	{
		snprintf(text, size, "VOLUME");
		return;
	}
	if (volume->map_index >= 0)
	{
		snprintf(text, size, "MAP VOLUME %s%s(%s)", inst->label, inst->label[0] ? " " : "", s_effect_names[volume->effect]);
		return;
	}
	{
		int fall = volume->flags & FORGE_VOLUME_FALL_MASK;
		char extra[48];

		snprintf(extra, sizeof(extra), "%s%s", fall == FORGE_VOLUME_FALL_LANDING ? " (SAFE TILL LANDING)" :
			fall == FORGE_VOLUME_FALL_EXIT ? " (SAFE INSIDE)" : "",
			volume->flags & FORGE_VOLUME_FLAG_STOP_INERTIA ? " (STOPS)" : "");
		if (volume->effect == _forge_volume_gravity)
		{
			snprintf(text, size, "GRAVITY X%.2f %s%s", (double)(volume->damage / 100.0f), s_shape_names[volume->shape],
				extra);
			return;
		}
		if (volume->effect == _forge_volume_ai_patrol)
		{
			snprintf(text, size, "AI PATROL #%d %s (PURSUIT +%.0f)%s", FORGE_VOLUME_PATROL_NUMBER(volume->flags),
				s_shape_names[volume->shape], (double)volume->damage, extra);
			return;
		}
		snprintf(text, size, "%s %s%s", s_effect_names[volume->effect], s_shape_names[volume->shape], extra);
	}
}

/* ---------- the panel's pages */

static int volume_page(int32_t live_idx)
{
	if (s_page_live_idx != live_idx)
		return VOLUME_PAGE_MAIN;
	return s_page;
}

static int volume_ratio_on(int32_t live_idx)
{
	return s_ratio_live_idx >= 0 && s_ratio_live_idx == live_idx;
}

/* whether an effect has an amount (`damage`): its STRENGTH, PURSUIT RANGE or damage */
static int volume_effect_has_amount(int effect)
{
	return effect == _forge_volume_apply_damage || effect == _forge_volume_constant_damage ||
		effect == _forge_volume_gravity || effect == _forge_volume_ai_patrol;
}

int forge_volume_rows(int32_t live_idx, int *rows)
{
	struct forge_volume const *volume = forge_volume_get(live_idx);
	int count = 0;

	if (!volume)
		return 0;
	if (s_page_live_idx >= 0 && s_page_live_idx != live_idx)
	{
		s_page_live_idx = -1;
		s_page = VOLUME_PAGE_MAIN;
	}
	if (s_ratio_live_idx >= 0 && s_ratio_live_idx != live_idx)
		s_ratio_live_idx = -1;
	switch (volume_page(live_idx))
	{
	case VOLUME_PAGE_EFFECT:
		rows[count++] = _forge_volume_row_effect;
		if (volume_effect_has_amount(volume->effect))
			rows[count++] = _forge_volume_row_damage;
		/* (GRAVITY's; any other's that has them set, to be turned off) */
		if (volume->effect == _forge_volume_gravity || (volume->flags & FORGE_VOLUME_FALL_MASK))
			rows[count++] = _forge_volume_row_fall_damage;
		if (volume->effect == _forge_volume_gravity || (volume->flags & FORGE_VOLUME_FLAG_STOP_INERTIA))
			rows[count++] = _forge_volume_row_inertia;
		rows[count++] = _forge_volume_row_back;
		break;
	case VOLUME_PAGE_SIZE:
		rows[count++] = _forge_volume_row_keep_ratio;
		rows[count++] = _forge_volume_row_size_x;
		rows[count++] = _forge_volume_row_size_y;
		rows[count++] = _forge_volume_row_size_z;
		if (volume_ratio_on(live_idx))
			rows[count++] = _forge_volume_row_factor;
		rows[count++] = _forge_volume_row_back;
		break;
	default:
		rows[count++] = _forge_volume_row_shape;
		rows[count++] = _forge_volume_row_effect_page;
		rows[count++] = _forge_volume_row_size_page;
		rows[count++] = _forge_volume_row_rot_snap;
		rows[count++] = _forge_volume_row_floor;
		rows[count++] = _forge_volume_row_delete;
		break;
	}
	return count;
}

int forge_volume_row_disabled(int32_t live_idx, int row)
{
	return volume_ratio_on(live_idx) &&
		(row == _forge_volume_row_size_x || row == _forge_volume_row_size_y || row == _forge_volume_row_size_z);
}

int forge_volume_page_return_row(void)
{
	return s_page_return_row;
}

int forge_volume_page_back(int32_t live_idx)
{
	if (volume_page(live_idx) == VOLUME_PAGE_MAIN)
		return 0;
	s_page_live_idx = -1;
	s_page = VOLUME_PAGE_MAIN;
	return 1;
}

char const *forge_volume_panel_title(int32_t live_idx)
{
	struct forge_volume const *volume = forge_volume_get(live_idx);
	int map_volume = volume && volume->map_index >= 0;

	switch (volume_page(live_idx))
	{
	case VOLUME_PAGE_EFFECT:
		return map_volume ? "MAP VOLUME EFFECT" : "VOLUME EFFECT";
	case VOLUME_PAGE_SIZE:
		return map_volume ? "MAP VOLUME SIZE" : "VOLUME SIZE";
	default:
		return map_volume ? "MAP VOLUME" : "VOLUME";
	}
}

void forge_volume_row_text(int32_t live_idx, int row, char *label, unsigned long label_size, char *value,
	unsigned long value_size)
{
	struct forge_volume const *volume = forge_volume_get(live_idx);
	static char const *const axes[3] = { "LENGTH", "WIDTH", "HEIGHT" };

	label[0] = 0;
	value[0] = 0;
	if (!volume)
		return;
	switch (row)
	{
	case _forge_volume_row_shape:
		snprintf(label, label_size, "SHAPE:");
		snprintf(value, value_size, "%s", s_shape_names[volume->shape]);
		break;
	case _forge_volume_row_effect:
		snprintf(label, label_size, "EFFECT:");
		if (volume->effect == _forge_volume_ai_patrol)
			snprintf(value, value_size, "AI PATROL #%d", FORGE_VOLUME_PATROL_NUMBER(volume->flags));
		else
			snprintf(value, value_size, "%s", s_effect_names[volume->effect]);
		break;
	case _forge_volume_row_damage:
		snprintf(label, label_size, volume->effect == _forge_volume_gravity ? "STRENGTH:" :
			volume->effect == _forge_volume_ai_patrol ? "PURSUIT RANGE:" : "DAMAGE:");
		if (volume->effect == _forge_volume_ai_patrol)
			snprintf(value, value_size, "+%.0f UNITS", (double)volume->damage);
		else if (volume->effect == _forge_volume_gravity)
			snprintf(value, value_size, "X%.2f", (double)(volume->damage / 100.0f));
		else if (volume->effect == _forge_volume_apply_damage)
			snprintf(value, value_size, "%.0f%% ONCE", (double)volume->damage);
		else if (volume->effect == _forge_volume_constant_damage)
			snprintf(value, value_size, "%.0f%% A SECOND", (double)volume->damage);
		else
			snprintf(value, value_size, "-");
		break;
	case _forge_volume_row_fall_damage:
	{
		int fall = volume->flags & FORGE_VOLUME_FALL_MASK;

		snprintf(label, label_size, "FALL DAMAGE:");
		snprintf(value, value_size, "%s", fall == FORGE_VOLUME_FALL_LANDING ? "LANDING" :
			fall == FORGE_VOLUME_FALL_EXIT ? "EXIT" : "VANILLA");
		break;
	}
	case _forge_volume_row_inertia:
		snprintf(label, label_size, "REMOVE INERTIA:");
		snprintf(value, value_size, "%s", volume->flags & FORGE_VOLUME_FLAG_STOP_INERTIA ? "ON" : "OFF");
		break;
	case _forge_volume_row_effect_page:
		snprintf(label, label_size, "EFFECT");
		if (volume->effect == _forge_volume_ai_patrol)
			snprintf(value, value_size, "AI PATROL #%d", FORGE_VOLUME_PATROL_NUMBER(volume->flags));
		else
			snprintf(value, value_size, "%s", s_effect_names[volume->effect]);
		break;
	case _forge_volume_row_size_page:
		snprintf(label, label_size, "SIZE");
		snprintf(value, value_size, "%.2f x %.2f x %.2f", (double)volume->size[0], (double)volume->size[1],
			(double)volume->size[2]);
		break;
	case _forge_volume_row_keep_ratio:
		snprintf(label, label_size, "KEEP RATIO:");
		snprintf(value, value_size, "%s", volume_ratio_on(live_idx) ? "ON" : "OFF");
		break;
	case _forge_volume_row_factor:
		snprintf(label, label_size, "FACTOR:");
		snprintf(value, value_size, "X%.2f", (double)s_ratio_factor);
		break;
	case _forge_volume_row_back:
		snprintf(label, label_size, "< BACK");
		break;
	case _forge_volume_row_size_x:
	case _forge_volume_row_size_y:
	case _forge_volume_row_size_z:
	{
		int axis = row - _forge_volume_row_size_x;

		/* (a cylinder's and a sphere's are diameters) */
		snprintf(label, label_size, "%s:",
			volume->shape == _forge_volume_sphere || (volume->shape == _forge_volume_cylinder && axis < 2) ?
			(axis == 0 ? "DIAMETER X" : axis == 1 ? "DIAMETER Y" : "DIAMETER Z") : axes[axis]);
		snprintf(value, value_size, "%.2f", (double)volume->size[axis]);
		break;
	}
	default:
		break;
	}
}

/* what a volume's `damage` is to an effect: 1 GRAVITY's STRENGTH, 2 AI
PATROL's PURSUIT RANGE, 0 an amount of damage */
static int volume_amount_kind(int effect)
{
	return effect == _forge_volume_gravity ? 1 : effect == _forge_volume_ai_patrol ? 2 : 0;
}

/* GRAVITY's next STRENGTH (hundredths): x0.05 to x1, x0.1 to x2, x0.5 past */
static float gravity_step(float strength, int dir)
{
	float step = strength < 100.0f - 0.01f || (dir < 0 && strength <= 100.0f + 0.01f) ? 5.0f :
		strength < 200.0f - 0.01f || (dir < 0 && strength <= 200.0f + 0.01f) ? 10.0f : 50.0f;
	float next = (float)floor((double)((strength + step * (float)dir) / step) + 0.5) * step;

	return PIN(next, 0.0f, VOLUME_GRAVITY_MAXIMUM);
}

/* a size's next step: quarters to 4, halves to 10, wholes to 50, fives past */
static float size_step(float size, int dir)
{
	float step = size < 4.0f - 0.001f || (dir < 0 && size <= 4.0f + 0.001f) ? 0.25f :
		size < 10.0f - 0.001f || (dir < 0 && size <= 10.0f + 0.001f) ? 0.5f :
		size < 50.0f - 0.001f || (dir < 0 && size <= 50.0f + 0.001f) ? 1.0f : 5.0f;
	float next = size + step * (float)dir;

	return size_clamp((float)floor((double)(next / step) + 0.5) * step);
}

int forge_volume_row_step(int32_t live_idx, int row, int dir)
{
	struct forge_volume const *now = forge_volume_get(live_idx);
	struct forge_volume settings;

	if (!now || dir == 0)
		return 0;
	settings = *now;
	switch (row)
	{
	case _forge_volume_row_effect_page:
	case _forge_volume_row_size_page:
		s_page_live_idx = live_idx;
		s_page = row == _forge_volume_row_effect_page ? VOLUME_PAGE_EFFECT : VOLUME_PAGE_SIZE;
		s_page_return_row = row;
		return FORGE_VOLUME_PRESS_PAGE_OPENED;
	case _forge_volume_row_back:
		forge_volume_page_back(live_idx);
		return FORGE_VOLUME_PRESS_PAGE_CLOSED;
	case _forge_volume_row_keep_ratio:
		if (volume_ratio_on(live_idx))
		{
			/* (off: the sizes stay as the factor made them) */
			s_ratio_live_idx = -1;
			forge_set_status("Keep ratio off: each size on its own");
		}
		else
		{
			s_ratio_live_idx = live_idx;
			s_ratio_base[0] = settings.size[0];
			s_ratio_base[1] = settings.size[1];
			s_ratio_base[2] = settings.size[2];
			s_ratio_factor = 1.0f;
			forge_set_status("Keep ratio on: FACTOR scales all three sizes together");
		}
		return 1;
	case _forge_volume_row_factor:
	{
		/* (tenths to 2, quarters to 5, halves past) */
		float factor = s_ratio_factor;
		float step = factor < 2.0f - 0.001f || (dir < 0 && factor <= 2.0f + 0.001f) ? 0.1f :
			factor < 5.0f - 0.001f || (dir < 0 && factor <= 5.0f + 0.001f) ? 0.25f : 0.5f;
		int k;

		if (!volume_ratio_on(live_idx))
			return 0;
		factor = (float)floor((double)((factor + step * (float)dir) / step) + 0.5) * step;
		if (factor < 0.1f)
			factor = 0.1f;
		/* (no further than a size's least or most allows) */
		for (k = 0; k < 3; k++)
		{
			float size = s_ratio_base[k] * factor;

			if (size < FORGE_VOLUME_SIZE_MIN - 0.001f || size > FORGE_VOLUME_SIZE_MAX + 0.001f)
			{
				forge_set_status("A size would go past %s", size < FORGE_VOLUME_SIZE_MIN ? "its least" : "its most");
				return 0;
			}
		}
		s_ratio_factor = factor;
		for (k = 0; k < 3; k++)
			settings.size[k] = size_clamp(s_ratio_base[k] * factor);
		break;
	}
	case _forge_volume_row_shape:
		if (settings.map_index >= 0)
		{
			forge_set_status("A map's volume keeps its own shape");
			return 0;
		}
		settings.shape = (uint8_t)((settings.shape + _forge_volume_map_box + dir) % _forge_volume_map_box);
		break;
	case _forge_volume_row_effect:
	{
		/* (MAP SCRIPT a map's alone) */
		int count = NUMBER_OF_FORGE_VOLUME_EFFECTS;
		int effect = settings.effect;

		do
		{
			effect = (effect + count + dir) % count;
		} while (effect == _forge_volume_map_script && settings.map_index < 0);
		/* (GRAVITY's STRENGTH, AI PATROL's PURSUIT RANGE and the damage share
		a field: each starts at its own default) */
		if (volume_amount_kind(effect) != volume_amount_kind(settings.effect))
		{
			settings.damage = effect == _forge_volume_gravity ? VOLUME_GRAVITY_DEFAULT :
				effect == _forge_volume_ai_patrol ? FORGE_VOLUME_PURSUIT_DEFAULT : 25.0f;
		}
		/* (an AI PATROL volume's number: given as it becomes one, gone as it
		stops being one; the AI linked to it by it lose it) */
		if (effect == _forge_volume_ai_patrol && settings.effect != _forge_volume_ai_patrol)
		{
			int number = forge_volume_patrol_free_number();

			if (number == 0)
			{
				forge_set_status("No Free AI Patrol Numbers (%d in this map)", FORGE_VOLUME_PATROL_MAX);
				effect = (effect + count + dir) % count;
			}
			else
			{
				settings.flags = (uint8_t)((settings.flags & ~FORGE_VOLUME_PATROL_MASK) |
					(number << FORGE_VOLUME_PATROL_SHIFT));
			}
		}
		if (effect != _forge_volume_ai_patrol)
			settings.flags &= (uint8_t)~FORGE_VOLUME_PATROL_MASK;
		settings.effect = (uint8_t)effect;
		break;
	}
	case _forge_volume_row_damage:
		if (settings.effect == _forge_volume_gravity)
		{
			settings.damage = gravity_step(settings.damage, dir);
			break;
		}
		if (settings.effect == _forge_volume_ai_patrol)
		{
			float step = settings.damage < 10.0f || (dir < 0 && settings.damage <= 10.0f) ? 1.0f : 5.0f;

			settings.damage = PIN((float)floor((double)((settings.damage + step * (float)dir) / step) + 0.5) * step,
				0.0f, FORGE_VOLUME_PURSUIT_MAX);
			forge_set_status("AI linked to it chase whoever they fight up to %.0f units past its sides",
				(double)settings.damage);
			break;
		}
		if (settings.effect != _forge_volume_apply_damage && settings.effect != _forge_volume_constant_damage)
		{
			forge_set_status("Only APPLY DAMAGE, CONSTANT DAMAGE, GRAVITY and AI PATROL have an amount");
			return 0;
		}
		settings.damage = PIN(settings.damage + (float)dir * (settings.damage < 10.0f || (dir < 0 && settings.damage <= 10.0f) ?
			1.0f : 5.0f), 1.0f, 100.0f);
		break;
	case _forge_volume_row_fall_damage:
	{
		int fall = ((settings.flags & FORGE_VOLUME_FALL_MASK) + NUMBER_OF_FORGE_VOLUME_FALL_MODES + dir) %
			NUMBER_OF_FORGE_VOLUME_FALL_MODES;

		settings.flags = (uint8_t)((settings.flags & ~FORGE_VOLUME_FALL_MASK) | fall);
		forge_set_status("%s", fall == FORGE_VOLUME_FALL_LANDING ?
			"Fall damage LANDING: whoever was in it takes none until they next land" :
			fall == FORGE_VOLUME_FALL_EXIT ? "Fall damage EXIT: none while they are in it" :
			"Fall damage VANILLA: as the game does");
		break;
	}
	case _forge_volume_row_inertia:
		settings.flags ^= FORGE_VOLUME_FLAG_STOP_INERTIA;
		forge_set_status("%s", settings.flags & FORGE_VOLUME_FLAG_STOP_INERTIA ?
			"Remove inertia on: a player coming into it stops dead" : "Remove inertia off");
		break;
	case _forge_volume_row_size_x:
	case _forge_volume_row_size_y:
	case _forge_volume_row_size_z:
		if (forge_volume_row_disabled(live_idx, row))
		{
			forge_set_status("Keep ratio is on: FACTOR sizes it");
			return 0;
		}
		settings.size[row - _forge_volume_row_size_x] = size_step(settings.size[row - _forge_volume_row_size_x], dir);
		break;
	default:
		return 0;
	}
	forge_volume_set(live_idx, &settings, 1);
	return 1;
}

int forge_volumes_save_lines(char *text, unsigned long size)
{
	int length = 0;
	int32_t i;

	if (!text || !size)
		return 0;
	text[0] = 0;
	for (i = 0; i < g_forge.live_instance_count; i++)
	{
		struct forge_live_instance const *inst = volume_instance(i);
		struct forge_volume const *volume = &s_volumes[i];
		int written;

		/* (a map's as it was is the map's own) */
		if (!inst || (volume->map_index >= 0 && !volume->edited))
			continue;
		written = snprintf(text + length, size - (unsigned long)length,
			"VOLUME|%d|%d|%.1f|%.4f|%.4f|%.4f|%.4f|%.4f|%.4f|%.3f|%.3f|%.3f|%d|%d\r\n",
			(int)volume->shape, (int)volume->effect, (double)volume->damage, inst->x, inst->y, inst->z, inst->yaw,
			inst->pitch, inst->roll, (double)volume->size[0], (double)volume->size[1], (double)volume->size[2],
			(int)volume->map_index, (int)volume->flags);
		if (written <= 0 || length + written >= (int)size)
			break;
		length += written;
	}
	return length;
}

void forge_volumes_load_line(char const *line)
{
	int shape = 0, effect = 0, map_index = NONE, flags = 0;
	float damage = 0.0f, x = 0.0f, y = 0.0f, z = 0.0f, yaw = 0.0f, pitch = 0.0f, roll = 0.0f;
	float size[3] = { FORGE_VOLUME_SIZE_DEFAULT, FORGE_VOLUME_SIZE_DEFAULT, FORGE_VOLUME_SIZE_DEFAULT };
	int32_t live_idx = -1;
	int32_t i;

	if (!line || strncmp(line, "VOLUME|", 7))
		return;
	/* (the flags after: a line saved before them has none) */
	if (sscanf(line + 7, "%d|%d|%f|%f|%f|%f|%f|%f|%f|%f|%f|%f|%d|%d", &shape, &effect, &damage, &x, &y, &z, &yaw, &pitch,
		&roll, &size[0], &size[1], &size[2], &map_index, &flags) < 13)
	{
		return;
	}
	if (map_index >= 0)
	{
		/* (the map's, adopted as the level began: moved and set) */
		for (i = 0; i < g_forge.live_instance_count && live_idx < 0; i++)
		{
			if (volume_instance(i) && s_volumes[i].map_index == map_index)
				live_idx = i;
		}
		if (live_idx < 0)
			return;
		forge_3d_move_instance(live_idx, x, y, z, yaw, pitch, roll, 0);
	}
	else
	{
		live_idx = forge_3d_spawn_object_at(FORGE_GROUP_VOLUME, shape_path(shape), 1, x, y, z, yaw, pitch, roll, 0, 0);
		if (live_idx < 0)
			return;
	}
	forge_volume_apply(live_idx, shape, effect, damage, size, flags);
}

int forge_volume_packet(int32_t live_idx, char *text, unsigned long size)
{
	struct forge_volume const *volume = forge_volume_get(live_idx);

	if (!volume)
		return 0;
	/* (the chat line's 64 bytes: sizes in hundredths; the flags last, which
	a build of before reads none of) */
	snprintf(text, size, "!FW:%d:%d:%d:%d:%d:%d:%d:%d", (int)live_idx, (int)volume->shape, (int)volume->effect,
		(int)(volume->damage + 0.5f), (int)(volume->size[0] * 100.0f + 0.5f), (int)(volume->size[1] * 100.0f + 0.5f),
		(int)(volume->size[2] * 100.0f + 0.5f), (int)volume->flags);
	return 1;
}

/* a volume's frame as the physics asks of it, pulling `strength` times the
world's gravity down its own down */
static void gravity_volume_fill(struct gravity_volume *gravity, int32_t live_idx, struct forge_live_instance const *inst,
	struct forge_volume const *volume, int shape, float strength)
{
	real_vector3d forward, left, up;

	euler_basis(inst->yaw, inst->pitch, inst->roll, &forward, &left, &up);
	gravity->live_idx = live_idx;
	gravity->flags = volume->flags;
	gravity->shape = (uint8_t)shape;
	gravity->middle[0] = inst->x;
	gravity->middle[1] = inst->y;
	gravity->middle[2] = inst->z;
	gravity->forward[0] = forward.i;
	gravity->forward[1] = forward.j;
	gravity->forward[2] = forward.k;
	gravity->left[0] = left.i;
	gravity->left[1] = left.j;
	gravity->left[2] = left.k;
	gravity->up[0] = up.i;
	gravity->up[1] = up.j;
	gravity->up[2] = up.k;
	gravity->half[0] = volume->size[0] * 0.5f;
	gravity->half[1] = volume->size[1] * 0.5f;
	gravity->half[2] = volume->size[2] * 0.5f;
	gravity->bound = MAX(gravity->half[0], MAX(gravity->half[1], gravity->half[2])) * 1.75f;
	gravity->pull[0] = -up.i * strength;
	gravity->pull[1] = -up.j * strength;
	gravity->pull[2] = -up.k * strength;
}

/* the GRAVITY volumes made again: their frames, and their pulls along their
own down; the ONE WAY volumes' frames; and those with a FALL DAMAGE or
REMOVE INERTIA of their own, whatever their effect */
static void gravity_volumes_make(void)
{
	int32_t i;

	s_gravity_count = 0;
	s_one_way_count = 0;
	s_flagged_count = 0;
	for (i = 0; i < g_forge.live_instance_count; i++)
	{
		struct forge_live_instance const *inst = volume_instance(i);
		struct forge_volume const *volume = &s_volumes[i];

		if (!inst)
			continue;
		if (volume->effect == _forge_volume_gravity && s_gravity_count < MAXIMUM_GRAVITY_VOLUMES)
		{
			gravity_volume_fill(&s_gravity[s_gravity_count++], i, inst, volume, volume->shape, volume->damage / 100.0f);
		}
		else if (volume->effect == _forge_volume_one_way && s_one_way_count < MAXIMUM_ONE_WAY_VOLUMES)
		{
			/* (a ONE WAY sphere is the cylinder around it) */
			gravity_volume_fill(&s_one_way[s_one_way_count++], i, inst, volume,
				volume->shape == _forge_volume_sphere ? _forge_volume_cylinder : volume->shape, 0.0f);
		}
		if ((volume->flags & FORGE_VOLUME_FLAGS_ALL) && s_flagged_count < MAXIMUM_FLAGGED_VOLUMES)
			gravity_volume_fill(&s_flagged[s_flagged_count++], i, inst, volume, volume->shape, 0.0f);
	}
}

static int gravity_volume_contains(struct gravity_volume const *gravity, float x, float y, float z)
{
	float dx = x - gravity->middle[0], dy = y - gravity->middle[1], dz = z - gravity->middle[2];
	float a, b, c;

	if (fabs((double)dx) > gravity->bound || fabs((double)dy) > gravity->bound || fabs((double)dz) > gravity->bound)
		return 0;
	a = (dx * gravity->forward[0] + dy * gravity->forward[1] + dz * gravity->forward[2]) / gravity->half[0];
	b = (dx * gravity->left[0] + dy * gravity->left[1] + dz * gravity->left[2]) / gravity->half[1];
	c = (dx * gravity->up[0] + dy * gravity->up[1] + dz * gravity->up[2]) / gravity->half[2];
	switch (gravity->shape)
	{
	case _forge_volume_cylinder:
		return a * a + b * b <= 1.0f && fabs((double)c) <= 1.0f;
	case _forge_volume_sphere:
		return a * a + b * b + c * c <= 1.0f;
	default:
		return fabs((double)a) <= 1.0f && fabs((double)b) <= 1.0f && fabs((double)c) <= 1.0f;
	}
}

/* how many GRAVITY volumes there are (and s_one_way_count the ONE WAY
ones), made again once a tick or after a change */
static int gravity_volumes_refresh(void)
{
	long now;

	if (!g_forge.initialized || g_forge.live_instance_count <= 0)
	{
		s_gravity_count = s_one_way_count = s_flagged_count = 0;
		return 0;
	}
	now = game_time_get();
	if (s_gravity_dirty || now != s_gravity_time)
	{
		gravity_volumes_make();
		s_gravity_dirty = 0;
		s_gravity_time = now;
	}
	return s_gravity_count;
}

int forge_volumes_gravity_at(float x, float y, float z, float out_pull[3])
{
	int inside = 0;
	int k;
	float sum[3] = { 0.0f, 0.0f, 0.0f };

	if (!gravity_volumes_refresh())
		return 0;
	for (k = 0; k < s_gravity_count; k++)
	{
		struct gravity_volume const *gravity = &s_gravity[k];

		if (!gravity_volume_contains(gravity, x, y, z))
			continue;
		inside = 1;
		sum[0] += gravity->pull[0];
		sum[1] += gravity->pull[1];
		sum[2] += gravity->pull[2];
	}
	/* (outside them all, out_pull is left as it was) */
	if (inside)
	{
		out_pull[0] = sum[0];
		out_pull[1] = sum[1];
		out_pull[2] = sum[2];
	}
	return inside;
}

int forge_volumes_fall_protected(long biped_index, float x, float y, float z, int airborne)
{
	int inside = 0;
	int exit_safe = 0;
	int found = -1;
	int k;

	if (biped_index == NONE)
		return 0;
	if (!s_fall_protected_ready)
		fall_protected_clear();
	gravity_volumes_refresh();
	for (k = 0; k < s_flagged_count && !inside; k++)
	{
		int fall = s_flagged[k].flags & FORGE_VOLUME_FALL_MASK;

		if (fall != FORGE_VOLUME_FALL_VANILLA && gravity_volume_contains(&s_flagged[k], x, y, z))
		{
			if (fall == FORGE_VOLUME_FALL_LANDING)
				inside = 1;
			else
				exit_safe = 1;
		}
	}
	for (k = 0; k < MAXIMUM_FALL_PROTECTED && found < 0; k++)
	{
		if (s_fall_protected[k] == biped_index)
			found = k;
	}
	/* (EXIT: safe in it, and no more once out of it) */
	if (exit_safe && !inside)
	{
		if (found >= 0 && !airborne)
			s_fall_protected[found] = NONE;
		return 1;
	}
	if (inside)
	{
		if (found < 0)
		{
			/* (a free slot, else the oldest given up) */
			for (k = 0; k < MAXIMUM_FALL_PROTECTED && found < 0; k++)
			{
				if (s_fall_protected[k] == NONE)
					found = k;
			}
			if (found < 0)
			{
				found = s_fall_protected_next;
				s_fall_protected_next = (s_fall_protected_next + 1) % MAXIMUM_FALL_PROTECTED;
			}
			s_fall_protected[found] = biped_index;
		}
		return 1;
	}
	if (found < 0)
		return 0;
	/* (landed out of them: this landing is the last it is kept from) */
	if (!airborne)
		s_fall_protected[found] = NONE;
	return 1;
}

int forge_volumes_stop_inertia(long biped_index, float x, float y, float z)
{
	long now;
	int stop = 0;
	int k;

	if (biped_index == NONE)
		return 0;
	if (!s_fall_protected_ready)
		fall_protected_clear();
	gravity_volumes_refresh();
	if (!s_flagged_count)
		return 0;
	now = game_time_get();
	for (k = 0; k < s_flagged_count; k++)
	{
		struct gravity_volume const *volume = &s_flagged[k];
		int slot = -1, free_slot = -1, oldest = 0;
		int j;

		if (!(volume->flags & FORGE_VOLUME_FLAG_STOP_INERTIA) || !gravity_volume_contains(volume, x, y, z))
			continue;
		for (j = 0; j < MAXIMUM_INERTIA_OCCUPANTS; j++)
		{
			if (s_inertia_occupants[j].biped_index == biped_index && s_inertia_occupants[j].live_idx == volume->live_idx)
				slot = j;
			else if (free_slot < 0 && (s_inertia_occupants[j].biped_index == NONE ||
				s_inertia_occupants[j].tick < now - 1 || s_inertia_occupants[j].tick > now))
			{
				free_slot = j;
			}
			if (s_inertia_occupants[j].tick < s_inertia_occupants[oldest].tick)
				oldest = j;
		}
		/* (in it last tick too: it came in before, and was stopped then) */
		if (slot >= 0 && s_inertia_occupants[slot].tick >= now - 1 && s_inertia_occupants[slot].tick <= now)
		{
			s_inertia_occupants[slot].tick = now;
			continue;
		}
		stop = 1;
		if (slot < 0)
			slot = free_slot >= 0 ? free_slot : oldest;
		s_inertia_occupants[slot].biped_index = biped_index;
		s_inertia_occupants[slot].live_idx = volume->live_idx;
		s_inertia_occupants[slot].tick = now;
	}
	return stop;
}

int forge_volumes_one_way_floor(float const from[3], float to[3], float velocity[3], float radius, float out_normal[3],
	float *out_d)
{
	int best = -1;
	float best_s1 = 0.0f;
	int k;

	gravity_volumes_refresh();
	for (k = 0; k < s_one_way_count; k++)
	{
		struct gravity_volume const *floor = &s_one_way[k];
		float const *up = floor->up;
		float top = floor->up[0] * floor->middle[0] + floor->up[1] * floor->middle[1] + floor->up[2] * floor->middle[2] +
			floor->half[2];
		/* (the pill's bottom sphere over the top: 0 resting on it) */
		float s0 = up[0] * from[0] + up[1] * from[1] + up[2] * from[2] - top - radius;
		float s1 = up[0] * to[0] + up[1] * to[1] + up[2] * to[2] - top - radius;
		float t, cross[3], dx, dy, dz, a, b;

		/* (it came from above it, or off it, and went into it; moving up
		through it, it passes) */
		if (s0 < -ONE_WAY_TOLERANCE || s1 >= 0.0f ||
			velocity[0] * up[0] + velocity[1] * up[1] + velocity[2] * up[2] > 0.001f)
		{
			continue;
		}
		/* (where it crossed the top: over the top's face?) */
		t = s0 > 0.0f ? s0 / (s0 - s1) : 0.0f;
		cross[0] = from[0] + (to[0] - from[0]) * t;
		cross[1] = from[1] + (to[1] - from[1]) * t;
		cross[2] = from[2] + (to[2] - from[2]) * t;
		dx = cross[0] - floor->middle[0];
		dy = cross[1] - floor->middle[1];
		dz = cross[2] - floor->middle[2];
		if (fabs((double)dx) > floor->bound + radius || fabs((double)dy) > floor->bound + radius ||
			fabs((double)dz) > floor->bound + radius)
		{
			continue;
		}
		a = (dx * floor->forward[0] + dy * floor->forward[1] + dz * floor->forward[2]) / floor->half[0];
		b = (dx * floor->left[0] + dy * floor->left[1] + dz * floor->left[2]) / floor->half[1];
		if (floor->shape == _forge_volume_cylinder ? a * a + b * b > 1.0f : fabs((double)a) > 1.0f || fabs((double)b) > 1.0f)
			continue;
		/* (the highest of those it stands on: the one it went least into) */
		if (best < 0 || s1 > best_s1)
		{
			best = k;
			best_s1 = s1;
		}
	}
	if (best < 0)
		return 0;
	{
		struct gravity_volume const *floor = &s_one_way[best];
		float const *up = floor->up;
		float into = velocity[0] * up[0] + velocity[1] * up[1] + velocity[2] * up[2];

		to[0] -= up[0] * best_s1;
		to[1] -= up[1] * best_s1;
		to[2] -= up[2] * best_s1;
		if (into < 0.0f)
		{
			velocity[0] -= up[0] * into;
			velocity[1] -= up[1] * into;
			velocity[2] -= up[2] * into;
		}
		out_normal[0] = up[0];
		out_normal[1] = up[1];
		out_normal[2] = up[2];
		*out_d = up[0] * floor->middle[0] + up[1] * floor->middle[1] + up[2] * floor->middle[2] + floor->half[2];
	}
	return 1;
}

/* vehicles, items and grenades at rest in a GRAVITY volume that pulls them other than
down woken, so they fall its way (a volume placed or turned over them, a
weapon spawned in one) */
static void gravity_volumes_wake(void)
{
	struct object_iterator iterator;
	struct object_datum *object;
	long now = game_time_get();

	if (!s_gravity_wake && s_gravity_wake_time >= 0 && now >= s_gravity_wake_time &&
		now - s_gravity_wake_time < TICKS_PER_SECOND)
	{
		return;
	}
	s_gravity_wake = 0;
	s_gravity_wake_time = now;
	object_iterator_new(&iterator, _object_mask_vehicle | _object_mask_item | _object_mask_projectile, 0);
	while ((object = (struct object_datum *)object_iterator_next(&iterator)) != NULL)
	{
		float pull[3];
		float magnitude;

		if (!TEST_FLAG(object->object.flags, _object_at_rest_bit) ||
			!forge_volumes_gravity_at(object->object.bounding_sphere_center.x, object->object.bounding_sphere_center.y,
				object->object.bounding_sphere_center.z, pull))
		{
			continue;
		}
		magnitude = (float)sqrt((double)(pull[0] * pull[0] + pull[1] * pull[1] + pull[2] * pull[2]));
		/* (still, it stays: no pull, or one mostly down holds it where it lies) */
		if (magnitude > 0.01f && -pull[2] < 0.9f * magnitude)
			SET_FLAG(object->object.flags, _object_at_rest_bit, FALSE);
	}
}

void forge_volumes_update(void)
{
	long now = game_time_get();
	int ticks;
	int32_t i;

	/* (a map's moved or set: its trigger volume follows, a peer's change
	too) */
	for (i = 0; i < g_forge.live_instance_count; i++)
	{
		if (volume_instance(i) && s_volumes[i].map_index >= 0 && s_volumes[i].edited)
			map_volume_write(i);
	}
	if (s_last_tick < 0 || now < s_last_tick)
		s_last_tick = now;
	ticks = (int)(now - s_last_tick);
	s_last_tick = now;
	if (ticks <= 0)
		return;
	/* (a pause, a load: one tick's) */
	if (ticks > TICKS_PER_SECOND)
		ticks = 1;
	/* (every machine: its own copies move) */
	if (gravity_volumes_refresh())
		gravity_volumes_wake();
	/* (the host hurts; the network carries the units' health) */
	if (game_connection() == _game_connection_network_client)
		return;
	volumes_hurt(ticks);
}

/* ---------- the shapes' geometry: edges (lines) and faces (parallelograms:
a corner and its two sides), in the world, for drawing and picking */

enum
{
	VOLUME_RING_SEGMENTS = 20,
	/* (a sphere's faces: bands of latitude, each of segments; a cylinder's
	caps: strips across) */
	VOLUME_SPHERE_BANDS = 8,
	VOLUME_SPHERE_SEGMENTS = 14,
	VOLUME_CAP_STRIPS = 8
};

struct volume_frame
{
	struct forge_live_instance const *inst;
	real_vector3d forward, left, up;
	float hx, hy, hz;
};

static void volume_frame_make(struct forge_live_instance const *inst, struct forge_volume const *volume,
	struct volume_frame *frame)
{
	frame->inst = inst;
	euler_basis(inst->yaw, inst->pitch, inst->roll, &frame->forward, &frame->left, &frame->up);
	frame->hx = volume->size[0] * 0.5f;
	frame->hy = volume->size[1] * 0.5f;
	frame->hz = volume->size[2] * 0.5f;
}

/* a point of the volume's frame (along forward, left, up from its middle) in the world */
static void frame_point(struct volume_frame const *frame, float a, float b, float c, float out[3])
{
	out[0] = frame->inst->x + frame->forward.i * a + frame->left.i * b + frame->up.i * c;
	out[1] = frame->inst->y + frame->forward.j * a + frame->left.j * b + frame->up.j * c;
	out[2] = frame->inst->z + frame->forward.k * a + frame->left.k * b + frame->up.k * c;
}

/* a direction of the frame in the world */
static void frame_vector(struct volume_frame const *frame, float a, float b, float c, float out[3])
{
	out[0] = frame->forward.i * a + frame->left.i * b + frame->up.i * c;
	out[1] = frame->forward.j * a + frame->left.j * b + frame->up.j * c;
	out[2] = frame->forward.k * a + frame->left.k * b + frame->up.k * c;
}

/* a point on an ellipsoid's surface: at latitude lat (-pi/2 to pi/2) and longitude lon */
static void sphere_point(struct volume_frame const *frame, float lat, float lon, float out[3])
{
	float c = (float)cos((double)lat);

	frame_point(frame, frame->hx * c * (float)cos((double)lon), frame->hy * c * (float)sin((double)lon),
		frame->hz * (float)sin((double)lat), out);
}

/* each edge of a volume */
static void volume_edges(struct forge_volume const *volume, struct volume_frame const *frame,
	void (*edge)(void *context, float const *from, float const *to), void *context)
{
	float hx = frame->hx, hy = frame->hy, hz = frame->hz;
	int k;

	if (volume->shape == _forge_volume_box || volume->shape == _forge_volume_map_box)
	{
		static signed char const corners[8][3] = {
			{ -1, -1, -1 }, { 1, -1, -1 }, { 1, 1, -1 }, { -1, 1, -1 },
			{ -1, -1, 1 }, { 1, -1, 1 }, { 1, 1, 1 }, { -1, 1, 1 }
		};
		static unsigned char const edges[12][2] = {
			{ 0, 1 }, { 1, 2 }, { 2, 3 }, { 3, 0 }, { 4, 5 }, { 5, 6 }, { 6, 7 }, { 7, 4 },
			{ 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 }
		};

		for (k = 0; k < 12; k++)
		{
			float from[3], to[3];
			signed char const *a = corners[edges[k][0]];
			signed char const *b = corners[edges[k][1]];

			frame_point(frame, hx * a[0], hy * a[1], hz * a[2], from);
			frame_point(frame, hx * b[0], hy * b[1], hz * b[2], to);
			edge(context, from, to);
		}
		return;
	}
	{
		/* (a cylinder: its two rings and four sides; a sphere: its three great rings) */
		int rings = volume->shape == _forge_volume_cylinder ? 2 : 3;
		int ring;

		for (ring = 0; ring < rings; ring++)
		{
			for (k = 0; k < VOLUME_RING_SEGMENTS; k++)
			{
				float a0 = (float)k * 6.2831853f / (float)VOLUME_RING_SEGMENTS;
				float a1 = (float)(k + 1) * 6.2831853f / (float)VOLUME_RING_SEGMENTS;
				float c0 = (float)cos((double)a0), s0 = (float)sin((double)a0);
				float c1 = (float)cos((double)a1), s1 = (float)sin((double)a1);
				float from[3], to[3];

				if (volume->shape == _forge_volume_cylinder)
				{
					float h = ring == 0 ? -hz : hz;

					frame_point(frame, hx * c0, hy * s0, h, from);
					frame_point(frame, hx * c1, hy * s1, h, to);
				}
				else if (ring == 0)
				{
					frame_point(frame, hx * c0, hy * s0, 0.0f, from);
					frame_point(frame, hx * c1, hy * s1, 0.0f, to);
				}
				else if (ring == 1)
				{
					frame_point(frame, hx * c0, 0.0f, hz * s0, from);
					frame_point(frame, hx * c1, 0.0f, hz * s1, to);
				}
				else
				{
					frame_point(frame, 0.0f, hy * c0, hz * s0, from);
					frame_point(frame, 0.0f, hy * c1, hz * s1, to);
				}
				edge(context, from, to);
			}
		}
		if (volume->shape == _forge_volume_cylinder)
		{
			for (k = 0; k < 4; k++)
			{
				float angle = (float)k * 1.5707963f;
				float from[3], to[3];

				frame_point(frame, hx * (float)cos((double)angle), hy * (float)sin((double)angle), -hz, from);
				frame_point(frame, hx * (float)cos((double)angle), hy * (float)sin((double)angle), hz, to);
				edge(context, from, to);
			}
		}
	}
}

/* each face of a volume, as parallelograms (a corner, and its two sides from
it): a box's six, a cylinder's side in panels and its caps in strips, a
sphere's in bands of panels */
static void volume_faces(struct forge_volume const *volume, struct volume_frame const *frame,
	void (*face)(void *context, float const *corner, float const *side_a, float const *side_b), void *context)
{
	float hx = frame->hx, hy = frame->hy, hz = frame->hz;
	float corner[3], a[3], b[3];
	int k;

	if (volume->shape == _forge_volume_box || volume->shape == _forge_volume_map_box)
	{
		int axis, sign;

		for (axis = 0; axis < 3; axis++)
		{
			for (sign = -1; sign <= 1; sign += 2)
			{
				float half[3] = { hx, hy, hz };
				float at[3], side_a[3] = { 0.0f, 0.0f, 0.0f }, side_b[3] = { 0.0f, 0.0f, 0.0f };
				int u = (axis + 1) % 3, v = (axis + 2) % 3;

				at[axis] = half[axis] * (float)sign;
				at[u] = -half[u];
				at[v] = -half[v];
				side_a[u] = 2.0f * half[u];
				side_b[v] = 2.0f * half[v];
				frame_point(frame, at[0], at[1], at[2], corner);
				frame_vector(frame, side_a[0], side_a[1], side_a[2], a);
				frame_vector(frame, side_b[0], side_b[1], side_b[2], b);
				face(context, corner, a, b);
			}
		}
		return;
	}
	if (volume->shape == _forge_volume_cylinder)
	{
		for (k = 0; k < VOLUME_RING_SEGMENTS; k++)
		{
			float a0 = (float)k * 6.2831853f / (float)VOLUME_RING_SEGMENTS;
			float a1 = (float)(k + 1) * 6.2831853f / (float)VOLUME_RING_SEGMENTS;
			float next[3];

			frame_point(frame, hx * (float)cos((double)a0), hy * (float)sin((double)a0), -hz, corner);
			frame_point(frame, hx * (float)cos((double)a1), hy * (float)sin((double)a1), -hz, next);
			a[0] = next[0] - corner[0];
			a[1] = next[1] - corner[1];
			a[2] = next[2] - corner[2];
			frame_vector(frame, 0.0f, 0.0f, 2.0f * hz, b);
			face(context, corner, a, b);
		}
		/* (each cap in strips across its forward axis, each as wide as the
		ellipse at its middle) */
		for (k = 0; k < VOLUME_CAP_STRIPS; k++)
		{
			float u0 = -1.0f + 2.0f * (float)k / (float)VOLUME_CAP_STRIPS;
			float um = u0 + 1.0f / (float)VOLUME_CAP_STRIPS;
			float w = (float)sqrt((double)MAX(0.0f, 1.0f - um * um));
			int cap;

			for (cap = -1; cap <= 1; cap += 2)
			{
				frame_point(frame, hx * u0, -hy * w, hz * (float)cap, corner);
				frame_vector(frame, hx * 2.0f / (float)VOLUME_CAP_STRIPS, 0.0f, 0.0f, a);
				frame_vector(frame, 0.0f, 2.0f * hy * w, 0.0f, b);
				face(context, corner, a, b);
			}
		}
		return;
	}
	{
		int band;

		for (band = 0; band < VOLUME_SPHERE_BANDS; band++)
		{
			float lat0 = -1.5707963f + 3.1415927f * (float)band / (float)VOLUME_SPHERE_BANDS;
			float lat1 = -1.5707963f + 3.1415927f * (float)(band + 1) / (float)VOLUME_SPHERE_BANDS;

			for (k = 0; k < VOLUME_SPHERE_SEGMENTS; k++)
			{
				float lon0 = 6.2831853f * (float)k / (float)VOLUME_SPHERE_SEGMENTS;
				float lon1 = 6.2831853f * (float)(k + 1) / (float)VOLUME_SPHERE_SEGMENTS;
				float p01[3], p10[3];

				/* (a band's panel from its wider edge's corner: the poles' bands
				are triangles, drawn as their wide edge's panel narrowing nowhere,
				so they're a little over) */
				if (band < VOLUME_SPHERE_BANDS / 2)
				{
					sphere_point(frame, lat1, lon0, corner);
					sphere_point(frame, lat1, lon1, p01);
					sphere_point(frame, lat0, (lon0 + lon1) * 0.5f, p10);
				}
				else
				{
					sphere_point(frame, lat0, lon0, corner);
					sphere_point(frame, lat0, lon1, p01);
					sphere_point(frame, lat1, (lon0 + lon1) * 0.5f, p10);
				}
				a[0] = p01[0] - corner[0];
				a[1] = p01[1] - corner[1];
				a[2] = p01[2] - corner[2];
				/* (towards the other edge, from the side's middle) */
				b[0] = p10[0] - (corner[0] + a[0] * 0.5f);
				b[1] = p10[1] - (corner[1] + a[1] * 0.5f);
				b[2] = p10[2] - (corner[2] + a[2] * 0.5f);
				face(context, corner, a, b);
			}
		}
	}
}

/* the drawing's: what each volume's lines and faces are drawn with */
struct volume_draw_context
{
	void (*draw_line)(float const *from, float const *to, int color, int highlighted);
	void (*draw_face)(float const *corner, float const *side_a, float const *side_b, int color, int highlighted);
	int color;
	int highlighted;
};

static void volume_draw_edge(void *context, float const *from, float const *to)
{
	struct volume_draw_context const *draw = (struct volume_draw_context const *)context;

	draw->draw_line(from, to, draw->color, draw->highlighted);
}

static void volume_draw_face(void *context, float const *corner, float const *side_a, float const *side_b)
{
	struct volume_draw_context const *draw = (struct volume_draw_context const *)context;

	draw->draw_face(corner, side_a, side_b, draw->color, draw->highlighted);
}

void forge_volumes_render(
	void (*draw_line)(float const *from, float const *to, int color, int highlighted),
	void (*draw_face)(float const *corner, float const *side_a, float const *side_b, int color, int highlighted))
{
	int32_t i;

	if (!draw_line)
		return;
	for (i = 0; i < g_forge.live_instance_count; i++)
	{
		struct forge_live_instance const *inst = volume_instance(i);
		struct forge_volume const *volume = &s_volumes[i];
		struct volume_frame frame;
		struct volume_draw_context draw;

		if (!inst)
			continue;
		volume_frame_make(inst, volume, &frame);
		draw.draw_line = draw_line;
		draw.draw_face = draw_face;
		draw.color = volume->effect == _forge_volume_insta_kill || volume->effect == _forge_volume_map_script ?
			_volume_color_kill : volume->effect == _forge_volume_off ? _volume_color_off :
			volume->effect == _forge_volume_gravity ? _volume_color_gravity :
			volume->effect == _forge_volume_one_way ? _volume_color_one_way :
			volume->effect == _forge_volume_ai_patrol ? _volume_color_patrol : _volume_color_damage;
		draw.highlighted = i == g_forge.grabbed_live_index || i == g_forge.hovered_live_index;
		if (draw_face)
			volume_faces(volume, &frame, volume_draw_face, &draw);
		volume_edges(volume, &frame, volume_draw_edge, &draw);
		/* (AI PATROL: its PURSUIT RANGE, the same shape that much bigger) */
		if (volume->effect == _forge_volume_ai_patrol && volume->damage > 0.0f)
		{
			struct forge_volume pursuit = *volume;
			struct volume_frame pursuit_frame;
			struct volume_draw_context pursuit_draw = draw;
			int k;

			for (k = 0; k < 3; k++)
				pursuit.size[k] = volume->size[k] + volume->damage * 2.0f;
			volume_frame_make(inst, &pursuit, &pursuit_frame);
			pursuit_draw.color = _volume_color_pursuit;
			pursuit_draw.highlighted = 0;
			volume_edges(&pursuit, &pursuit_frame, volume_draw_edge, &pursuit_draw);
		}
		/* (GRAVITY: an arrow down its own down, the way things fall in it;
		ONE WAY: up its own up, the way it lets them through) */
		if ((volume->effect == _forge_volume_gravity && volume->damage > 0.0f) || volume->effect == _forge_volume_one_way)
		{
			float sign = volume->effect == _forge_volume_one_way ? -1.0f : 1.0f;
			float length = MIN(frame.hz, 2.0f) * sign;
			float head = length * 0.35f;
			float spread = (float)fabs((double)head);
			float tail[3], tip[3], barb[3];

			frame_point(&frame, 0.0f, 0.0f, length, tail);
			frame_point(&frame, 0.0f, 0.0f, -length, tip);
			draw_line(tail, tip, draw.color, draw.highlighted);
			frame_point(&frame, spread, 0.0f, head - length, barb);
			draw_line(barb, tip, draw.color, draw.highlighted);
			frame_point(&frame, -spread, 0.0f, head - length, barb);
			draw_line(barb, tip, draw.color, draw.highlighted);
			frame_point(&frame, 0.0f, spread, head - length, barb);
			draw_line(barb, tip, draw.color, draw.highlighted);
			frame_point(&frame, 0.0f, -spread, head - length, barb);
			draw_line(barb, tip, draw.color, draw.highlighted);
		}
	}
}

/* ---------- picking a volume by its edges */

struct volume_pick_context
{
	float origin[3];
	float direction[3]; /* unit */
	float max_along;
	/* the best edge so far: its closeness (its distance from the view over
	what it may be at that far), and the point on the view's ray */
	float score;
	float point[3];
	int found;
};

/* an edge against the view's ray: the closest two points of the line and
the segment, and how far apart they are */
static void volume_pick_edge(void *context, float const *from, float const *to)
{
	struct volume_pick_context *pick = (struct volume_pick_context *)context;
	float const *d1 = pick->direction;
	float d2[3], r[3];
	float b, c, e, f, denominator, along, t;
	float on_ray[3], on_edge[3], gap[3];
	float distance, tolerance;
	int k;

	for (k = 0; k < 3; k++)
	{
		d2[k] = to[k] - from[k];
		r[k] = pick->origin[k] - from[k];
	}
	b = d1[0] * d2[0] + d1[1] * d2[1] + d1[2] * d2[2];
	c = d1[0] * r[0] + d1[1] * r[1] + d1[2] * r[2];
	e = d2[0] * d2[0] + d2[1] * d2[1] + d2[2] * d2[2];
	f = d2[0] * r[0] + d2[1] * r[1] + d2[2] * r[2];
	if (e < 0.0001f)
		return;
	/* (closest points of the ray O + d1 s, s >= 0, and the segment A + d2 t,
	t in 0..1; the ray's direction is unit) */
	denominator = e - b * b;
	{
		float s = denominator > 0.000001f ? (b * f - c * e) / denominator : -c;

		if (s < 0.0f)
			s = 0.0f;
		t = PIN((b * s + f) / e, 0.0f, 1.0f);
		s = b * t - c;
		along = s > 0.0f ? s : 0.0f;
	}
	if (along < 0.5f || along > pick->max_along)
		return;
	for (k = 0; k < 3; k++)
	{
		on_ray[k] = pick->origin[k] + d1[k] * along;
		on_edge[k] = from[k] + d2[k] * t;
		gap[k] = on_ray[k] - on_edge[k];
	}
	distance = (float)sqrt((double)(gap[0] * gap[0] + gap[1] * gap[1] + gap[2] * gap[2]));
	/* (a little wider the further away it is, as it looks the same) */
	tolerance = 0.2f + 0.025f * along;
	if (distance > tolerance || distance / tolerance >= pick->score)
		return;
	pick->score = distance / tolerance;
	pick->found = 1;
	for (k = 0; k < 3; k++)
		pick->point[k] = on_edge[k];
}

int32_t forge_volumes_pick_edge(float const origin[3], float const direction[3], float max_along, float out_point[3])
{
	struct volume_pick_context pick;
	int32_t best = -1;
	int32_t i;
	int k;

	for (k = 0; k < 3; k++)
	{
		pick.origin[k] = origin[k];
		pick.direction[k] = direction[k];
	}
	pick.max_along = max_along;
	pick.score = 1.0f;
	for (i = 0; i < g_forge.live_instance_count; i++)
	{
		struct forge_live_instance const *inst = volume_instance(i);
		struct volume_frame frame;

		if (!inst)
			continue;
		volume_frame_make(inst, &s_volumes[i], &frame);
		pick.found = 0;
		volume_edges(&s_volumes[i], &frame, volume_pick_edge, &pick);
		if (pick.found)
		{
			best = i;
			for (k = 0; k < 3; k++)
				out_point[k] = pick.point[k];
		}
	}
	return best;
}

int forge_volumes_tag_deals_damage(long definition_index)
{
	struct object_definition const *definition;
	long attachment;

	if (definition_index == NONE)
		return 0;
	definition = (struct object_definition const *)tag_get(OBJECT_DEFINITION_GROUP_TAG, definition_index);
	if (!definition)
		return 0;
	for (attachment = 0; attachment < definition->object.attachments.count; attachment++)
	{
		struct object_attachment_definition const *attached = TAG_BLOCK_GET_ELEMENT(&definition->object.attachments,
			attachment, struct object_attachment_definition);
		struct effect_definition const *effect;
		long event;

		if (!attached || attached->type.group_tag != EFFECT_GROUP_TAG || attached->type.index == NONE)
			continue;
		effect = (struct effect_definition const *)tag_get(EFFECT_GROUP_TAG, attached->type.index);
		for (event = 0; effect && event < effect->events.count; event++)
		{
			struct effect_event_definition const *event_definition =
				TAG_BLOCK_GET_ELEMENT(&effect->events, event, struct effect_event_definition);
			long part;

			for (part = 0; event_definition && part < event_definition->parts.count; part++)
			{
				struct effect_part_definition const *part_definition =
					TAG_BLOCK_GET_ELEMENT(&event_definition->parts, part, struct effect_part_definition);

				if (part_definition && part_definition->reference.group_tag == DAMAGE_EFFECT_GROUP_TAG)
					return 1;
			}
		}
	}
	return 0;
}

int forge_volumes_effect_damage_blocked(long object_index)
{
	static int32_t s_last_live = -1;
	int32_t i;

	if (object_index == NONE || !g_forge.initialized)
		return 0;
	/* (the emitter asked of last: its effects' parts come in runs) */
	if (s_last_live >= 0 && s_last_live < g_forge.live_instance_count &&
		g_forge.live_instances[s_last_live].active && g_forge.live_instances[s_last_live].object_index == object_index)
	{
		return g_forge.live_instances[s_last_live].no_damage;
	}
	for (i = 0; i < g_forge.live_instance_count; i++)
	{
		struct forge_live_instance const *inst = &g_forge.live_instances[i];

		if (inst->active && inst->no_damage && inst->object_index == object_index)
		{
			s_last_live = i;
			return 1;
		}
	}
	return 0;
}

/* ---------- AI PATROL volumes */

int32_t forge_volume_patrol_find(int number)
{
	int32_t i;

	if (number <= 0)
		return -1;
	for (i = 0; i < g_forge.live_instance_count; i++)
	{
		if (volume_instance(i) && s_volumes[i].effect == _forge_volume_ai_patrol &&
			FORGE_VOLUME_PATROL_NUMBER(s_volumes[i].flags) == number)
		{
			return i;
		}
	}
	return -1;
}

int forge_volumes_patrol_numbers(int *numbers, int max)
{
	int count = 0, number;

	for (number = 1; number <= FORGE_VOLUME_PATROL_MAX && count < max; number++)
	{
		if (forge_volume_patrol_find(number) >= 0)
			numbers[count++] = number;
	}
	return count;
}

int forge_volume_patrol_free_number(void)
{
	int number;

	for (number = 1; number <= FORGE_VOLUME_PATROL_MAX; number++)
	{
		if (forge_volume_patrol_find(number) < 0)
			return number;
	}
	return 0;
}

int forge_volume_shape_contains(int shape, float const position[3], float yaw, float pitch, float roll,
	float const size[3], float margin, float const point[3])
{
	real_vector3d forward, left, up;
	float dx = point[0] - position[0], dy = point[1] - position[1], dz = point[2] - position[2];
	float hx = size[0] * 0.5f + margin, hy = size[1] * 0.5f + margin, hz = size[2] * 0.5f + margin;
	float local[3];

	if (hx <= 0.0f || hy <= 0.0f || hz <= 0.0f)
		return 0;
	euler_basis(yaw, pitch, roll, &forward, &left, &up);
	local[0] = dx * forward.i + dy * forward.j + dz * forward.k;
	local[1] = dx * left.i + dy * left.j + dz * left.k;
	local[2] = dx * up.i + dy * up.j + dz * up.k;
	switch (shape)
	{
	case _forge_volume_cylinder:
	{
		float u = local[0] / hx, v = local[1] / hy;

		return u * u + v * v <= 1.0f && fabs((double)local[2]) <= hz;
	}
	case _forge_volume_sphere:
	{
		float u = local[0] / hx, v = local[1] / hy, w = local[2] / hz;

		return u * u + v * v + w * w <= 1.0f;
	}
	default:
		return fabs((double)local[0]) <= hx && fabs((double)local[1]) <= hy && fabs((double)local[2]) <= hz;
	}
}
