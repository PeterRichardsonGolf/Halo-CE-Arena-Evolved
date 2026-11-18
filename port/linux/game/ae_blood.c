/*
AE_BLOOD.C

Dynamic blood (SETTINGS > VIDEO > DYNAMIC BLOOD, config.toml's game.dynamic_blood, off by default; ae_blood.h), with
the game's own blood (each character's: its collision model's area damage effect, "effects\blood aoe elite", and the
splats of its kind, "effects\decals\blood splats\blood elite splat"):

- A body hit throws blood onto the wall or slope behind it, and blood lands on the level's two-sided surfaces too (a
  tree, a fence).
- Blood on a wall (any blood decal landing on a steep surface) runs: drips, each a trail of thin streaks laid down the
  surface as it runs, slowing, thinning and stopping at the surface's edge, a bead at its head, and drops falling from
  edges and undersides (ae_blood_drops.c draws those). No animated pictures: the trails are the game's own blood
  splats, drawn as they run (ae_blood_decals.inc).

Bullet holes and scorch marks are laid as the game lays them. Everything here is this machine's own (local random
numbers, nothing over the network). The pure rules (which surfaces run, a run's speed, thinning and stop, which tags
are blood) are ae_blood_rules.c's. Based on OpenCE PR #325 by jcompto95-tech (AGPL-3.0; closed unmerged); here
behind AE hooks.
Units: world units (1 = 3.048 m) and ticks (1/30 s).
*/

#include "cseries.h"
#include "cseries/errors.h"
#include "math/real_math.h"
#include "objects/objects.h"
#include "objects/damage.h"
#include "effects/decals.h"
#include "effects/decal_definitions.h"
#include "effects/effect_definitions.h"
#include "physics/collision_model_definitions.h"
#include "physics/collisions.h"
#include "physics/collision_usage.h"
#include "tag_files/tag_files.h"
#include "game/game.h"
#include "units/bipeds.h"

#include "ae_blood.h"
#include "ae_blood_rules.h"

#include <math.h>
#include <string.h>

/* cache_files.c */
long tag_loaded(long group_tag, char const *name);
/* render_interpolation.c: how far between the last tick and the next this frame is */
real render_interpolation_fraction(void);
/* port_config.c */
int config_boolean(char const *name);
unsigned long config_changes(void);

/* ---------- constants */

/* (a run: the splat's picture nearly whole, stretched along it, so its ragged round outline is the run's edge and
ends, short stretches overlapping in a chain; a bead: its picture whole, round. The picture's edge is see-through, so
what shows is narrower than the quad: AE_BLOOD_RUN_WIDEN) */
#define AE_BLOOD_RUN_CORE_ALONG 0.85f
#define AE_BLOOD_RUN_CORE_ACROSS 1.f
#define AE_BLOOD_BEAD_CORE 0.9f
#define AE_BLOOD_RUN_WIDEN 1.6f
/* (a drip's run sits this far off its surface) */
#define AE_BLOOD_LIFT 0.004f
/* (how often a hit throws blood, how much, how long runs are: as tuned on PR #325) */
#define AE_BLOOD_HIT_CHANCE 0.85f
#define AE_BLOOD_DRIP_LENGTH 1.f

enum
{
	AE_BLOOD_MAXIMUM_KINDS = 32,
	AE_BLOOD_MAXIMUM_DRIPS = 160,
	AE_BLOOD_MAXIMUM_FALLING_DROPS = 48,
};

/* ---------- structures */

/* a biped's blood: its splats */
struct ae_blood_kind
{
	long biped_definition_index; /* NONE: free */
	long splat; /* decal; NONE: no blood (a sentinel, a robot) */
	long splat_large;
	long splat_small;
};

/* blood running down a surface */
struct ae_blood_drip
{
	boolean active;
	long decal_definition_index;
	short cluster_index;
	short pause; /* ticks it holds still, gathering (a bead) */
	struct ae_blood_run run;
	real_point3d position;
	real_vector3d normal;
	real_point3d laid; /* where the last streak ended */
	real wander; /* radians off straight down it runs, wandering */
	/* (its live streak and bead, moved as it runs) */
	long live_decal;
	long live_bead;
	real_point3d head_before; /* its head a tick ago and now (world) */
	real_point3d head;
	real_vector3d live_normal;
	real live_half_width;
};

/* ---------- globals */

int ae_blood_two_sided = FALSE;

static boolean ae_blood_setting = FALSE;
static unsigned long ae_blood_setting_changes = (unsigned long)-1;
static boolean ae_blood_logged = FALSE;
static struct ae_blood_kind ae_blood_kinds[AE_BLOOD_MAXIMUM_KINDS];
static struct ae_blood_drip ae_blood_drips[AE_BLOOD_MAXIMUM_DRIPS];
static long ae_blood_last_time = NONE;
/* (blood's own decals landing: no drips of drips) */
static boolean ae_blood_laying = FALSE;
/* a decal definition known to be blood, or not: the last asked */
static long ae_blood_checked_definition = NONE;
static boolean ae_blood_checked_is_blood = FALSE;
/* (what it did, logged each minute it did something: splats thrown, runs started, quads laid, drops let go) */
static struct
{
	long splats;
	long runs;
	long quads;
	long drops;
	long logged_time;
} ae_blood_counts;

static struct
{
	boolean active;
	long decal_definition_index;
	real_point3d previous;
	real_point3d position;
	real_vector3d velocity; /* world units a tick */
	real radius;
	short age;
} ae_blood_falling_drops[AE_BLOOD_MAXIMUM_FALLING_DROPS];

/* ---------- small helpers */

static real ae_blood_dot(real_vector3d const *a, real_vector3d const *b)
{
	return a->i * b->i + a->j * b->j + a->k * b->k;
}

static real ae_blood_normalize(real_vector3d *v)
{
	real length = (real)sqrt(ae_blood_dot(v, v));

	if (length > 1e-6f && valid_real(length))
	{
		v->i /= length;
		v->j /= length;
		v->k /= length;
		return length;
	}
	return 0.f;
}

static void ae_blood_add(real_point3d *p, real_vector3d const *v, real scale)
{
	p->x += v->i * scale;
	p->y += v->j * scale;
	p->z += v->k * scale;
}

/* down a surface: gravity along it (none on a floor) */
static boolean ae_blood_downhill(real_vector3d const *normal, real_vector3d *down)
{
	float n[3] = { normal->i, normal->j, normal->k };
	float d[3];

	if (!ae_blood_rule_downhill(n, d))
		return FALSE;
	down->i = d[0];
	down->j = d[1];
	down->k = d[2];
	return TRUE;
}

static char const *ae_blood_tag_name(long tag_index)
{
	return tag_index != NONE && tag_index != 0 ? tag_get_name(tag_index) : NULL;
}

/* a decal by its name, or NONE */
static long ae_blood_decal_named(char const *name)
{
	return name[0] ? tag_loaded(DECAL_GROUP_TAG, name) : NONE;
}

static struct ae_blood_kind const *ae_blood_kind_get(long biped_index)
{
	struct object_datum *object = object_try_and_get(biped_index);
	struct object_definition *definition;
	struct ae_blood_kind *kind = NULL;
	short index;

	if (!object)
		return NULL;
	for (index = 0; index < AE_BLOOD_MAXIMUM_KINDS; index++)
	{
		if (ae_blood_kinds[index].biped_definition_index == object->definition_index)
			return &ae_blood_kinds[index];
		if (!kind && ae_blood_kinds[index].biped_definition_index == NONE)
			kind = &ae_blood_kinds[index];
	}
	if (!kind)
		kind = &ae_blood_kinds[(unsigned long)object->definition_index % AE_BLOOD_MAXIMUM_KINDS];

	kind->biped_definition_index = object->definition_index;
	kind->splat = kind->splat_large = kind->splat_small = NONE;
	definition = object_definition_get(object->definition_index);

	/* its splats: by the area damage effect of its collision model ("effects\blood aoe elite") */
	if (definition && definition->object.collision_model.index != NONE)
	{
		struct collision_model *model = collision_model_definition_get(definition->object.collision_model.index);
		char splat[256];
		char large[256];
		char small[256];

		if (model && ae_blood_splat_names(ae_blood_tag_name(model->resistance.area_damage_effect.index), splat, large,
			small, sizeof(splat)))
		{
			kind->splat = ae_blood_decal_named(splat);
			kind->splat_large = ae_blood_decal_named(large);
			kind->splat_small = ae_blood_decal_named(small);
		}
	}
	if (kind->splat_large == NONE)
		kind->splat_large = kind->splat;
	if (kind->splat_small == NONE)
		kind->splat_small = kind->splat;
	return kind;
}

int ae_blood_on(void)
{
	return ae_blood_setting && decals_enabled;
}

/* the setting (config.toml's game.dynamic_blood), read again whenever a setting changes: once a frame, never per hit
(config_changes looks at config.toml's time) */
static void ae_blood_read_setting(void)
{
	if (ae_blood_setting_changes != config_changes())
	{
		ae_blood_setting_changes = config_changes();
		ae_blood_setting = config_boolean("game.dynamic_blood") != 0;
		if (ae_blood_setting && !ae_blood_logged)
		{
			ae_blood_logged = TRUE;
			error(_error_silent, "AE dynamic blood: on");
		}
	}
}

static void ae_blood_drop_fall(long decal_definition_index, real_point3d const *point, real radius);

/* ---------- a drip's quads */

/* a drip's quad: its run from one point to another (a thin slice down the middle of the splat's picture stretched
along it, soft across), or a round bead at a point (the picture's round middle); its corners as
ae_blood_decal_new_quad winds them, clockwise seen from in front */
static void ae_blood_drip_corners(struct ae_blood_drip const *drip, real_point3d const *from, real_point3d const *to,
	boolean bead, real lift, real_point3d *middle, real_vector3d *forward, real *half_length, real *half_width,
	real_point3d corners[4])
{
	real_vector3d const *normal = &drip->live_normal;
	real_vector3d left;
	real length;
	real dot;
	short corner;

	forward->i = to->x - from->x;
	forward->j = to->y - from->y;
	forward->k = to->z - from->z;
	dot = ae_blood_dot(forward, normal);
	forward->i -= normal->i * dot;
	forward->j -= normal->j * dot;
	forward->k -= normal->k * dot;
	length = ae_blood_normalize(forward);
	if (length == 0.f && !ae_blood_downhill(normal, forward))
		perpendicular3d(normal, forward);
	left.i = normal->j * forward->k - normal->k * forward->j;
	left.j = normal->k * forward->i - normal->i * forward->k;
	left.k = normal->i * forward->j - normal->j * forward->i;
	ae_blood_normalize(&left);
	middle->x = (from->x + to->x) * 0.5f + normal->i * lift;
	middle->y = (from->y + to->y) * 0.5f + normal->j * lift;
	middle->z = (from->z + to->z) * 0.5f + normal->k * lift;
	if (bead)
	{
		*half_length = drip->live_half_width * AE_BLOOD_RUN_WIDEN * 1.25f;
		*half_width = drip->live_half_width * AE_BLOOD_RUN_WIDEN * 1.15f;
	}
	else
	{
		*half_length = length * 0.5f + drip->live_half_width * AE_BLOOD_RUN_WIDEN * 0.7f;
		*half_width = drip->live_half_width * AE_BLOOD_RUN_WIDEN;
	}
	for (corner = 0; corner < 4; corner++)
	{
		real x = (corner >= 2) ? *half_length : -*half_length;
		real y = (corner == 1 || corner == 2) ? *half_width : -*half_width;

		corners[corner].x = middle->x + forward->i * x + left.i * y;
		corners[corner].y = middle->y + forward->j * x + left.j * y;
		corners[corner].z = middle->z + forward->k * x + left.k * y;
	}
}

static long ae_blood_drip_quad(struct ae_blood_drip const *drip, real_point3d const *from, real_point3d const *to,
	boolean bead, real lift)
{
	real_point3d middle;
	real_vector3d forward;
	real half_length;
	real half_width;
	real_point3d corners[4];
	long decal_index;

	ae_blood_drip_corners(drip, from, to, bead, lift, &middle, &forward, &half_length, &half_width, corners);
	if (half_length <= 0.f || half_width <= 0.f || drip->cluster_index == NONE)
		return NONE;
	ae_blood_texture_core_along = bead ? AE_BLOOD_BEAD_CORE : AE_BLOOD_RUN_CORE_ALONG;
	ae_blood_texture_core_across = bead ? AE_BLOOD_BEAD_CORE : AE_BLOOD_RUN_CORE_ACROSS;
	ae_blood_full_intensity = TRUE;
	decal_index = ae_blood_decal_new_quad(drip->decal_definition_index, &middle, &drip->live_normal, &forward,
		half_length, half_width, drip->cluster_index, corners);
	if (decal_index != NONE)
		ae_blood_counts.quads++;
	ae_blood_texture_core_along = ae_blood_texture_core_across = 1.f;
	ae_blood_full_intensity = FALSE;
	return decal_index;
}

/* its live run (from where its last kept run ended to its head) and the bead at its head, moved there, or made if
gone */
static void ae_blood_drip_place(struct ae_blood_drip *drip, real_point3d const *from, real_point3d const *head)
{
	real_point3d middle;
	real_vector3d forward;
	real half_length;
	real half_width;
	real_point3d corners[4];

	if (drip->cluster_index == NONE || drip->live_half_width <= 0.f)
		return;
	if (distance3d(from, head) > drip->live_half_width * 0.2f)
	{
		ae_blood_drip_corners(drip, from, head, FALSE, AE_BLOOD_LIFT * 0.5f, &middle, &forward, &half_length,
			&half_width, corners);
		if (drip->live_decal == NONE ||
			!ae_blood_decal_move_quad(drip->live_decal, drip->decal_definition_index, corners, drip->cluster_index))
		{
			drip->live_decal = ae_blood_drip_quad(drip, from, head, FALSE, AE_BLOOD_LIFT * 0.5f);
		}
	}
	ae_blood_drip_corners(drip, head, head, TRUE, AE_BLOOD_LIFT, &middle, &forward, &half_length, &half_width,
		corners);
	if (drip->live_bead == NONE ||
		!ae_blood_decal_move_quad(drip->live_bead, drip->decal_definition_index, corners, drip->cluster_index))
	{
		drip->live_bead = ae_blood_drip_quad(drip, head, head, TRUE, AE_BLOOD_LIFT);
	}
}

void ae_blood_render_update(void)
{
	real fraction;
	short index;

	if (!ae_blood_on())
		return;
	/* (each running drip drawn out to where its head is between ticks: smooth, not a stretch a tick) */
	fraction = PIN(render_interpolation_fraction(), 0.f, 1.f);
	for (index = 0; index < AE_BLOOD_MAXIMUM_DRIPS; index++)
	{
		struct ae_blood_drip *drip = &ae_blood_drips[index];
		real_point3d head;

		if (!drip->active || drip->cluster_index == NONE || (drip->live_decal == NONE && drip->live_bead == NONE))
			continue;
		head.x = drip->head_before.x + (drip->head.x - drip->head_before.x) * fraction;
		head.y = drip->head_before.y + (drip->head.y - drip->head_before.y) * fraction;
		head.z = drip->head_before.z + (drip->head.z - drip->head_before.z) * fraction;
		ae_blood_laying = TRUE;
		ae_blood_drip_place(drip, &drip->laid, &head);
		ae_blood_laying = FALSE;
	}
}

/* ---------- drips */

/* the patterns a splat runs in: each splat picks one, and each of its drips its own length, speed, width, wander and
moment to start */
enum
{
	_ae_blood_pattern_single, /* one run */
	_ae_blood_pattern_pair, /* two, side by side */
	_ae_blood_pattern_curtain, /* a row across the splat, of all lengths */
	_ae_blood_pattern_fan, /* from one spot, spreading as they run */
	_ae_blood_pattern_trickle, /* many short stubs */
	_ae_blood_pattern_runner, /* one long heavy run, short ones beside it */
	_ae_blood_pattern_late, /* one, then more after a while (still seeping) */
	_ae_blood_pattern_cascade, /* from heights down the splat, one under another */
	_ae_blood_pattern_drizzle, /* many fine quick threads */
	_ae_blood_pattern_sheet, /* one wide slow run */
	_ae_blood_pattern_wander, /* a few that weave as they run */
	_ae_blood_pattern_burst, /* a spray of runs, starting through some seconds */
	_ae_blood_pattern_twin, /* two long runs that spread apart */
	NUMBER_OF_AE_BLOOD_PATTERNS
};

static struct ae_blood_drip *ae_blood_drip_free(void)
{
	short index;

	for (index = 0; index < AE_BLOOD_MAXIMUM_DRIPS; index++)
	{
		if (!ae_blood_drips[index].active)
			return &ae_blood_drips[index];
	}
	return NULL;
}

static void ae_blood_drip_start(long decal_definition_index, real_point3d const *point, real_vector3d const *normal,
	real radius, short cluster_index)
{
	short pattern = (short)local_random_range(0, NUMBER_OF_AE_BLOOD_PATTERNS);
	short count;
	short made;
	real fan_spot = real_local_random_range(-0.3f, 0.3f);
	float speed_scale;
	float thickness_scale;

	switch (pattern)
	{
	case _ae_blood_pattern_single: count = 1; break;
	case _ae_blood_pattern_pair: count = 2; break;
	case _ae_blood_pattern_curtain: count = (short)local_random_range(3, 7); break;
	case _ae_blood_pattern_fan: count = (short)local_random_range(2, 5); break;
	case _ae_blood_pattern_trickle: count = (short)local_random_range(4, 8); break;
	case _ae_blood_pattern_runner: count = (short)local_random_range(2, 4); break;
	case _ae_blood_pattern_cascade: count = (short)local_random_range(3, 6); break;
	case _ae_blood_pattern_drizzle: count = (short)local_random_range(6, 11); break;
	case _ae_blood_pattern_sheet: count = 1; break;
	case _ae_blood_pattern_wander: count = (short)local_random_range(2, 4); break;
	case _ae_blood_pattern_burst: count = (short)local_random_range(5, 10); break;
	case _ae_blood_pattern_twin: count = 2; break;
	default: count = (short)local_random_range(2, 4); break;
	}
	/* (each kind's blood runs its own way: an Elite's and a Jackal's quicker, a Grunt's slow and thick, and as many
	runs from a small splat) */
	ae_blood_viscosity(ae_blood_tag_name(decal_definition_index), &speed_scale, &thickness_scale);
	if (radius < 0.05f && thickness_scale <= 1.f)
		count = (short)MAX(1, count / 2);

	for (made = 0; made < count; made++)
	{
		struct ae_blood_drip *drip = ae_blood_drip_free();
		real_vector3d down;
		real_vector3d across;
		real_point3d start = *point;
		real length_scale = real_local_random_range(1.f, 3.5f);
		real width_scale = real_local_random_range(0.08f, 0.16f);
		real speed = real_local_random_range(0.25f, 0.6f);
		real wander = real_local_random_range(-0.15f, 0.15f);
		real offset_across = real_local_random_range(-0.5f, 0.5f);
		real offset_down = real_local_random_range(0.f, 0.4f);
		short pause = (short)local_random_range(0, 8);

		if (!drip || !ae_blood_downhill(normal, &down))
			return;

		switch (pattern)
		{
		case _ae_blood_pattern_single:
			length_scale = real_local_random_range(1.5f, 4.5f);
			break;
		case _ae_blood_pattern_pair:
			offset_across = (made ? 1.f : -1.f) * real_local_random_range(0.1f, 0.35f);
			pause = (short)(made * local_random_range(0, 20));
			break;
		case _ae_blood_pattern_curtain:
			/* (spread evenly across, some long, most short) */
			offset_across = -0.6f + 1.2f * (made + real_local_random_range(0.2f, 0.8f)) / count;
			length_scale = local_random_range(0, 3) ? real_local_random_range(0.4f, 1.6f) :
				real_local_random_range(2.f, 4.f);
			width_scale = real_local_random_range(0.05f, 0.11f);
			pause = (short)local_random_range(0, 25);
			break;
		case _ae_blood_pattern_fan:
			offset_across = fan_spot + real_local_random_range(-0.05f, 0.05f);
			wander = -0.35f + 0.7f * (made + 0.5f) / count + real_local_random_range(-0.05f, 0.05f);
			length_scale = real_local_random_range(1.f, 3.f);
			break;
		case _ae_blood_pattern_trickle:
			length_scale = real_local_random_range(0.3f, 1.f);
			width_scale = real_local_random_range(0.05f, 0.1f);
			speed = real_local_random_range(0.15f, 0.35f);
			pause = (short)local_random_range(0, 40);
			break;
		case _ae_blood_pattern_runner:
			if (made == 0)
			{
				length_scale = real_local_random_range(3.5f, 6.f);
				width_scale = real_local_random_range(0.15f, 0.22f);
				speed = real_local_random_range(0.4f, 0.8f);
				offset_across = real_local_random_range(-0.15f, 0.15f);
			}
			else
			{
				length_scale = real_local_random_range(0.3f, 1.2f);
				width_scale = real_local_random_range(0.05f, 0.1f);
				pause = (short)local_random_range(5, 30);
			}
			break;
		case _ae_blood_pattern_late:
			/* (the first at once, the rest seeping out seconds later, slowly) */
			if (made > 0)
			{
				pause = (short)local_random_range(30, 120);
				speed = real_local_random_range(0.12f, 0.3f);
				length_scale = real_local_random_range(0.6f, 2.5f);
			}
			break;
		case _ae_blood_pattern_cascade:
			offset_down = 0.9f * made / count + real_local_random_range(0.f, 0.15f);
			offset_across = real_local_random_range(-0.35f, 0.35f);
			length_scale = real_local_random_range(0.8f, 2.8f);
			pause = (short)(made * local_random_range(4, 14));
			break;
		case _ae_blood_pattern_drizzle:
			width_scale = real_local_random_range(0.03f, 0.06f);
			length_scale = real_local_random_range(0.6f, 2.2f);
			speed = real_local_random_range(0.45f, 0.9f);
			offset_across = real_local_random_range(-0.7f, 0.7f);
			pause = (short)local_random_range(0, 15);
			break;
		case _ae_blood_pattern_sheet:
			width_scale = real_local_random_range(0.25f, 0.4f);
			length_scale = real_local_random_range(2.f, 4.5f);
			speed = real_local_random_range(0.12f, 0.25f);
			offset_across = real_local_random_range(-0.1f, 0.1f);
			break;
		case _ae_blood_pattern_wander:
			wander = (made & 1 ? 1.f : -1.f) * real_local_random_range(0.25f, 0.45f);
			length_scale = real_local_random_range(1.5f, 3.5f);
			break;
		case _ae_blood_pattern_burst:
			offset_across = real_local_random_range(-0.8f, 0.8f);
			offset_down = real_local_random_range(0.f, 0.7f);
			length_scale = real_local_random_range(0.3f, 3.f);
			width_scale = real_local_random_range(0.04f, 0.14f);
			pause = (short)local_random_range(0, 90);
			break;
		case _ae_blood_pattern_twin:
			offset_across = (made ? 1.f : -1.f) * real_local_random_range(0.05f, 0.15f);
			wander = (made ? 1.f : -1.f) * real_local_random_range(0.1f, 0.25f);
			length_scale = real_local_random_range(3.f, 5.5f);
			width_scale = real_local_random_range(0.1f, 0.18f);
			break;
		}

		/* (from somewhere in the splat's lower half) */
		across.i = normal->j * down.k - normal->k * down.j;
		across.j = normal->k * down.i - normal->i * down.k;
		across.k = normal->i * down.j - normal->j * down.i;
		ae_blood_add(&start, &across, offset_across * radius);
		ae_blood_add(&start, &down, offset_down * radius);

		ae_blood_counts.runs++;
		drip->active = TRUE;
		drip->decal_definition_index = decal_definition_index;
		drip->cluster_index = cluster_index;
		ae_blood_run_start(&drip->run, radius, length_scale, width_scale * thickness_scale, speed * speed_scale,
			thickness_scale, AE_BLOOD_DRIP_LENGTH);
		drip->wander = PIN(wander, -0.45f, 0.45f);
		drip->pause = pause; /* (a moment gathering before it runs) */
		drip->live_decal = NONE;
		drip->live_bead = NONE;
		drip->head_before = drip->head = start;
		drip->live_normal = *normal;
		drip->live_half_width = 0.f;
		drip->position = start;
		drip->normal = *normal;
		drip->laid = drip->position;
	}
}

void ae_blood_decal_landed(long decal_definition_index, real_point3d const *point, real_vector3d const *normal,
	real radius, short cluster_index)
{
	if (ae_blood_laying || !ae_blood_on() || !ae_blood_surface_runs(normal->k) || radius <= 0.f)
		return;
	if (decal_definition_index != ae_blood_checked_definition)
	{
		ae_blood_checked_definition = decal_definition_index;
		ae_blood_checked_is_blood = ae_blood_name_is_blood(ae_blood_tag_name(decal_definition_index));
	}
	if (ae_blood_checked_is_blood)
		ae_blood_drip_start(decal_definition_index, point, normal, radius, cluster_index);
}

/* a game decal of blood's at a point, facing out of a surface (a pool, a landed drop) */
static void ae_blood_splat_at(long decal_definition_index, real_point3d const *point, real_vector3d const *normal,
	real radius)
{
	real definition_radius = ae_blood_decal_radius(decal_definition_index);
	real_point3d above = *point;
	real_vector3d into;

	if (radius <= 0.f || definition_radius <= 0.f)
		return;
	ae_blood_add(&above, normal, 0.05f);
	scale_vector3d(normal, -0.15f, &into);
	ae_blood_laying = TRUE;
	decal_new(decal_definition_index, &above, &into, radius / definition_radius, FALSE, NONE, NULL);
	ae_blood_laying = FALSE;
}

/* a branch off a run: from where it is, with some of what is left of it, wandering off its own way */
static void ae_blood_drip_branch(struct ae_blood_drip *drip, real_point3d const *point, real_vector3d const *normal)
{
	struct ae_blood_drip *branch = ae_blood_drip_free();

	if (!branch)
		return;
	*branch = *drip;
	branch->run.remaining = drip->run.remaining * 0.5f;
	branch->run.length = drip->run.length * 0.6f;
	branch->run.width = drip->run.width * 0.7f;
	branch->run.start_width = branch->run.width;
	branch->wander = drip->wander + (local_random_range(0, 2) ? 0.35f : -0.35f);
	branch->pause = 0;
	branch->live_decal = NONE;
	branch->live_bead = NONE;
	drip->run.remaining *= 0.7f;
	/* (where it is now: the branch's run starts there) */
	branch->position = *point;
	branch->normal = *normal;
	branch->laid = branch->position;
}

/* a drip's step: it runs on down its surface, laying a streak behind it, until it has run its length, slowed to
nothing or come to the surface's edge */
static void ae_blood_drip_update(struct ae_blood_drip *drip)
{
	real_point3d world = drip->position;
	real_vector3d normal = drip->normal;
	real_point3d laid_world = drip->laid;
	real_vector3d down;
	real_point3d next;
	real_point3d origin;
	real_vector3d ray;
	boolean on_surface;
	real laid_distance;
	real_point3d hit_point;
	real_vector3d hit_normal;

	if (!ae_blood_downhill(&normal, &down) || ae_blood_surface_ends(normal.k))
	{
		/* (its surface turned level: what is left of it pools there, or drops from an underside) */
		if (normal.k < -0.3f)
			ae_blood_drop_fall(drip->decal_definition_index, &world, drip->run.width * 0.7f);
		else
			ae_blood_splat_at(drip->decal_definition_index, &world, &normal, ae_blood_run_pool_radius(&drip->run));
		drip->active = FALSE;
		return;
	}
	/* (a gathering bead: it holds, then lets go faster) */
	if (drip->pause > 0)
	{
		drip->pause--;
		if (drip->pause == 0)
			drip->run.speed *= 1.6f;
		/* (its head held where it is, frame to frame) */
		drip->head_before = drip->head;
		return;
	}
	/* (not straight down: wandering a little either way, as a run finds its way over a surface) */
	{
		real_vector3d across;

		across.i = normal.j * down.k - normal.k * down.j;
		across.j = normal.k * down.i - normal.i * down.k;
		across.k = normal.i * down.j - normal.j * down.i;
		drip->wander = PIN(drip->wander + real_local_random_range(-0.09f, 0.09f), -0.4f, 0.4f);
		down.i = down.i * (real)cos(drip->wander) + across.i * (real)sin(drip->wander);
		down.j = down.j * (real)cos(drip->wander) + across.j * (real)sin(drip->wander);
		down.k = down.k * (real)cos(drip->wander) + across.k * (real)sin(drip->wander);
	}

	next = world;
	ae_blood_add(&next, &down, drip->run.speed * real_local_random_range(0.75f, 1.25f));

	/* still on its surface? */
	origin = next;
	ae_blood_add(&origin, &normal, 0.04f);
	scale_vector3d(&normal, -0.1f, &ray);
	{
		struct collision_result collision;

		match_assert("ae_blood.c", __LINE__, global_current_collision_user_depth < MAXIMUM_COLLISION_USER_STACK_DEPTH);
		global_current_collision_users[global_current_collision_user_depth++] = _collision_user_decals;
		on_surface = collision_test_vector(
			FLAG(_collision_test_front_facing_surfaces_bit) | FLAG(_collision_test_structure_bit),
			&origin, &ray, NONE, &collision) &&
			collision.type == _collision_result_structure && valid_real_plane3d(&collision.plane);
		global_current_collision_user_depth--;
		if (on_surface)
		{
			hit_point = collision.point;
			hit_normal = collision.plane.n;
			drip->cluster_index = collision.location.cluster_index;
		}
	}
	on_surface = on_surface && ae_blood_dot(&hit_normal, &normal) > 0.9f;
	if (on_surface)
	{
		next = hit_point;
		normal = hit_normal;
	}
	else
	{
		/* (over the surface's edge: it stops where it was, at the edge; what is left of it gathers there and falls) */
		next = world;
		if (drip->run.remaining > drip->run.length * 0.15f)
			ae_blood_drop_fall(drip->decal_definition_index, &world, drip->run.width * 0.6f);
	}

	/* a streak from where the last ended, once it has run a few times its width (few decals: a trail is a handful of
	long thin quads), or stops */
	laid_distance = distance3d(&laid_world, &next);
	ae_blood_run_step(&drip->run);
	/* (now and then it stops to gather, a bead at its head; now and then a branch runs off it with some of what is
	left) */
	if (on_surface && drip->run.remaining > drip->run.length * 0.3f && local_random_range(0, 40) == 0)
		drip->pause = (short)local_random_range(4, 16);
	if (on_surface && drip->run.remaining > drip->run.length * 0.45f && local_random_range(0, 70) == 0)
		ae_blood_drip_branch(drip, &next, &normal);
	if (drip->cluster_index != NONE)
	{
		/* a live run, its streak from where the last ended to its head and a bead at its head, both moved as it
		runs (and, between ticks, frame by frame: ae_blood_render_update); kept and a new one begun every so often,
		or where it stops or gathers */
		boolean stopping = !on_surface || drip->run.remaining <= 0.f;
		boolean keep = stopping || drip->pause > 0 || laid_distance >= MAX(drip->run.width * 3.5f, 0.03f);

		drip->head_before = drip->live_decal != NONE || drip->live_bead != NONE ? drip->head : next;
		drip->head = next;
		drip->live_normal = normal;
		drip->live_half_width = ae_blood_run_half_width(&drip->run);
		ae_blood_laying = TRUE;
		ae_blood_drip_place(drip, &laid_world, &next);
		if (keep)
		{
			/* (its run kept, laid over again for depth) */
			ae_blood_drip_quad(drip, &laid_world, &next, FALSE, AE_BLOOD_LIFT * 0.75f);
			if (stopping || drip->pause > 0)
				ae_blood_drip_quad(drip, &next, &next, TRUE, AE_BLOOD_LIFT);
			drip->live_decal = NONE;
			drip->head_before = next;
			if (stopping)
				drip->live_bead = NONE;
			laid_world = next;
		}
		ae_blood_laying = FALSE;
		if (stopping)
		{
			drip->active = FALSE;
			return;
		}
	}
	else if (!on_surface || drip->run.remaining <= 0.f)
	{
		drip->active = FALSE;
		return;
	}

	drip->position = next;
	drip->normal = normal;
	drip->laid = laid_world;
}

/* ---------- falling drops (drawn by ae_blood_drops.c) */

/* a drop let go from a point (a run over an edge, an underside) */
static void ae_blood_drop_fall(long decal_definition_index, real_point3d const *point, real radius)
{
	short index;

	if (decal_definition_index == NONE || radius <= 0.f)
		return;
	for (index = 0; index < AE_BLOOD_MAXIMUM_FALLING_DROPS; index++)
	{
		if (!ae_blood_falling_drops[index].active)
		{
			ae_blood_counts.drops++;
			ae_blood_falling_drops[index].active = TRUE;
			ae_blood_falling_drops[index].decal_definition_index = decal_definition_index;
			ae_blood_falling_drops[index].previous = *point;
			ae_blood_falling_drops[index].position = *point;
			ae_blood_falling_drops[index].velocity.i = 0.f;
			ae_blood_falling_drops[index].velocity.j = 0.f;
			ae_blood_falling_drops[index].velocity.k = 0.f;
			ae_blood_falling_drops[index].radius = PIN(radius, 0.004f, 0.03f);
			/* (a moment hanging, swelling, before it lets go) */
			ae_blood_falling_drops[index].age = (short)-local_random_range(4, 12);
			return;
		}
	}
}

/* (a tick: hanging, then falling; where it lands, a small splat) */
static void ae_blood_drop_update(short index)
{
	real_vector3d step;
	boolean landed = FALSE;

	ae_blood_falling_drops[index].previous = ae_blood_falling_drops[index].position;
	if (ae_blood_falling_drops[index].age++ < 0)
		return;
	/* (gravity: about 9.8 m/s/s, in world units a tick each tick) */
	ae_blood_falling_drops[index].velocity.k -= 9.8f / (AE_BLOOD_METRES * 30.f * 30.f);
	step = ae_blood_falling_drops[index].velocity;
	if (ae_blood_falling_drops[index].age > 90)
	{
		ae_blood_falling_drops[index].active = FALSE;
		return;
	}
	{
		struct collision_result collision;
		boolean hit;

		match_assert("ae_blood.c", __LINE__, global_current_collision_user_depth < MAXIMUM_COLLISION_USER_STACK_DEPTH);
		global_current_collision_users[global_current_collision_user_depth++] = _collision_user_decals;
		hit = collision_test_vector(FLAG(_collision_test_front_facing_surfaces_bit) |
			FLAG(_collision_test_structure_bit), &ae_blood_falling_drops[index].position, &step, NONE, &collision) &&
			collision.type == _collision_result_structure;
		global_current_collision_user_depth--;
		if (hit)
		{
			landed = TRUE;
			ae_blood_splat_at(ae_blood_falling_drops[index].decal_definition_index, &collision.point,
				&collision.plane.n, ae_blood_falling_drops[index].radius * 1.6f);
		}
	}
	if (landed)
	{
		ae_blood_falling_drops[index].active = FALSE;
		return;
	}
	ae_blood_add(&ae_blood_falling_drops[index].position, &step, 1.f);
}

short ae_blood_drops_for_drawing(real_point3d *positions, real_vector3d *directions, real *radii, long *decals,
	short maximum)
{
	real fraction;
	short count = 0;
	short index;

	if (!ae_blood_on())
		return 0;
	fraction = PIN(render_interpolation_fraction(), 0.f, 1.f);
	/* (a run's head is led by its own bead, a decal like the run's: only drops in the air are drawn here; hanging,
	they swell; falling, they stretch along their way) */
	for (index = 0; index < AE_BLOOD_MAXIMUM_FALLING_DROPS && count < maximum; index++)
	{
		if (!ae_blood_falling_drops[index].active)
			continue;
		positions[count].x = ae_blood_falling_drops[index].previous.x +
			(ae_blood_falling_drops[index].position.x - ae_blood_falling_drops[index].previous.x) * fraction;
		positions[count].y = ae_blood_falling_drops[index].previous.y +
			(ae_blood_falling_drops[index].position.y - ae_blood_falling_drops[index].previous.y) * fraction;
		positions[count].z = ae_blood_falling_drops[index].previous.z +
			(ae_blood_falling_drops[index].position.z - ae_blood_falling_drops[index].previous.z) * fraction;
		directions[count] = ae_blood_falling_drops[index].velocity;
		radii[count] = ae_blood_falling_drops[index].radius *
			(ae_blood_falling_drops[index].age < 0 ? 0.8f + 0.04f * (12 + ae_blood_falling_drops[index].age) : 1.f);
		decals[count] = ae_blood_falling_drops[index].decal_definition_index;
		count++;
	}
	return count;
}

/* ---------- the hooks */

static void ae_blood_stop_all(void)
{
	short index;

	for (index = 0; index < AE_BLOOD_MAXIMUM_DRIPS; index++)
		ae_blood_drips[index].active = FALSE;
	for (index = 0; index < AE_BLOOD_MAXIMUM_FALLING_DROPS; index++)
		ae_blood_falling_drops[index].active = FALSE;
}

void ae_blood_new_map(void)
{
	short index;

	for (index = 0; index < AE_BLOOD_MAXIMUM_KINDS; index++)
		ae_blood_kinds[index].biped_definition_index = NONE;
	ae_blood_stop_all();
	ae_blood_last_time = NONE;
	ae_blood_checked_definition = NONE;
	ae_blood_two_sided = FALSE;
	csmemset(&ae_blood_counts, 0, sizeof(ae_blood_counts));
}

static void ae_blood_log_counts(long now)
{
	if (now - ae_blood_counts.logged_time < 60 * 30 || ae_blood_counts.splats + ae_blood_counts.runs == 0)
		return;
	error(_error_silent, "AE dynamic blood: %ld splats thrown, %ld runs, %ld run quads, %ld drops in the last minute",
		ae_blood_counts.splats, ae_blood_counts.runs, ae_blood_counts.quads, ae_blood_counts.drops);
	csmemset(&ae_blood_counts, 0, sizeof(ae_blood_counts));
	ae_blood_counts.logged_time = now;
}

void ae_blood_update(void)
{
	long now;
	long steps;
	short index;

	ae_blood_read_setting();
	/* (decals_update runs at the main menu too, before a game's time is) */
	if (!ae_blood_on() || !game_time_initialized())
		return;
	now = game_time_get();
	/* (a revert, a saved game loaded: the drips stop) */
	if (ae_blood_last_time != NONE && now < ae_blood_last_time)
	{
		ae_blood_stop_all();
		ae_blood_last_time = now - 1;
	}
	/* (each tick once: decals_update runs every frame drawn, several a tick) */
	steps = ae_blood_last_time == NONE ? 1 : now - ae_blood_last_time;
	if (steps <= 0)
		return;
	ae_blood_last_time = now;
	ae_blood_log_counts(now);
	for (steps = MIN(steps, 4); steps > 0; steps--)
	{
		for (index = 0; index < AE_BLOOD_MAXIMUM_DRIPS; index++)
		{
			if (ae_blood_drips[index].active)
				ae_blood_drip_update(&ae_blood_drips[index]);
		}
	}
	for (index = 0; index < AE_BLOOD_MAXIMUM_FALLING_DROPS; index++)
	{
		if (ae_blood_falling_drops[index].active)
			ae_blood_drop_update(index);
	}
}

int ae_blood_decal_two_sided(long decal_definition_index)
{
	return ae_blood_on() && ae_blood_two_sided_takes(ae_blood_tag_name(decal_definition_index), TRUE);
}

void ae_blood_object_damaged(long object_index, struct damage_data const *damage, real damage_done,
	real damage_most)
{
	struct object_datum *object;
	struct ae_blood_kind const *kind;
	real_vector3d through;

	/* most hits on a body throw blood onto the wall or slope behind, which runs: the game's own throws only a little,
	often nowhere. Not through shields */
	if (!ae_blood_on() || !(MAX(damage_done, damage_most) > 0.f) || !valid_real(damage->epicenter.x))
		return;
	object = object_try_and_get(object_index);
	if (!object || object->object.type != _object_type_biped || object->object.shield_vitality > 0.f ||
		real_local_random_range(0.f, 1.f) >= AE_BLOOD_HIT_CHANCE)
	{
		return;
	}
	through = damage->direction;
	kind = ae_blood_kind_get(object_index);
	if (kind && kind->splat != NONE && ae_blood_normalize(&through) > 0.f)
	{
		real_point3d from = damage->epicenter;
		real_vector3d reach;

		ae_blood_add(&from, &through, 0.12f);
		scale_vector3d(&through, 1.8f, &reach);
		ae_blood_counts.splats++;
		decal_new(kind->splat, &from, &reach, real_local_random_range(0.7f, 1.2f), FALSE, NONE, NULL);
	}
}
