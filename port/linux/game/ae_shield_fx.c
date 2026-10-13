/*
AE_SHIELD_FX.C

HIT FEEDBACK: ARENA EVOLVED (ae_hit_feedback.h), what is drawn: a player's
shield shows its state on the body's own shield flare.

- A hit's flare is a short pulse (the game's value, which fades over two
  seconds, raised to a power), so quick hits read as a flicker.
- The POP, once a shield, armed again by recharging: shown as the shield
  falls to where one finishing hit to the head would kill (or as it truly
  breaks, if it never got there): white, the whole body, then draining from
  the head to the feet, with a white light and the map's own burst.
- While the shield is down (or popped) and not yet recharging: thin bands of
  the flare flicker at heights of their own on the body.
- Every recharge: a band rising from the feet whose height is the shield's
  real level, white turning pale yellow, then a flash of the whole body as
  it fills; on the first-person arms a swell as the band passes them.
- A shield brought to exactly nothing counts as broken (stock tests "more
  than what is left").
- The bloodshot: the round that breaks the last of a shield and goes on
  shows the body's hit effect, in the world where it struck.

Looks only: no damage value changes. Drawn by the GPU through a fade by
height on the model (port/linux/src/ae_height_fade.h).

The play-tested prototype, moved here as it was approved (2026-10-12), its
tuning switches fixed at the approved numbers (tuning()). The rise of the
flare up a body and its bookkeeping follow OpenCE's open pull request #320
(animated overshield, by jcompto95-tech), which this file does not turn on:
only its recharge band, pop and flicker paths run. To tidy: the dormant
overshield and per-viewer paths, and the finishing hit's points (25, the
magnum's, typed in: to be read from the map's weapons).
*/

#include "cseries.h"
#include "real_math.h"
#include "objects/objects.h"
#include "objects/object_definitions.h"
#include "models/model_definitions.h"
#include "rasterizer/rasterizer.h"
#include "shaders/shader_definitions.h"
#include "shaders/shaders.h"
#include "units/units.h"
#include "effects/effect_definitions.h"
#include "effects/decal_definitions.h"
#include "effects/contrail_definitions.h"
#include "effects/particle_system_definitions.h"
#include "effects/particles.h"
#include "sound/sound_definitions.h"
#include "objects/light_definitions.h"
#include "objects/damage.h"
#include "effects/effects.h"
#include "game/game_engine.h"
#include "tag_files/tag_files.h"
#include "cseries/errors.h"

#include "networking/network_game_globals.h"

#include "ae_hit_feedback.h"
#include "ae_shield_fx.h"
#include "../src/ae_height_fade.h"

/* port/linux/game/view_fov.c */
boolean viewmodel_shield_is_visible(void);

enum
{
	_shader_type_transparent_plasma_glow = 10, /* (shader_get_and_verify_type's plasma, as shield_glow.c has it) */
	AE_SHIELD_FX_MAXIMUM = 16,
	AE_SHIELD_FX_MODELS = 8,
};

/* seconds the rise takes: from a full shield (or any but a down one), and
from a down one (the charge's own times: 2 and 3 of shield a second) */
#define AE_SHIELD_FX_RISE 2.f
#define AE_SHIELD_FX_RISE_FROM_DOWN 3.f
/* a shield at most this when the overshield begins is down */
#define AE_SHIELD_FX_DOWN 0.12f
/* the flare's energy's speed */
#define AE_SHIELD_FX_ENERGY_SPEED 0.3f
/* the soft edge, as a share of the body's height */
#define AE_SHIELD_FX_EDGE 0.12f
/* where on the rise the arms come up, and over how much of it */
#define AE_SHIELD_FX_ARMS_START 0.55f
#define AE_SHIELD_FX_ARMS_LENGTH 0.3f

/* the overshield's color: the powerup's */
static real_rgb_color const ae_shield_fx_rgb = { 0.992f, 0.306f, 0.455f }; /* (253, 78, 116) */

/* the plasma shader's fields after its header (rasterizer_xbox_plasma_energy.c's) */
struct ae_shield_fx_plasma
{
	byte reserved00[4];
	short intensity_exponent_source;
};

struct ae_shield_fx
{
	long unit_index; /* NONE: free */
	real start; /* the game's seconds (render_interpolation_game_time_sec) */
	real rise;
	real last_seen;
	/* the values the unit's modifier shader is drawn with, its intensity's
	full: one copy for the body, one for the arms */
	real body_values[4];
	real arms_values[4];
	/* this frame's: how far up (0..1, eased), and the model's heights */
	real progress;
	real bottom;
	real top;
	boolean recharge; /* a recharge band, not an overshield */
	real start_vitality; /* (the shield as the band began) */
	real end; /* (when the shield was full again: the band's last flash; 0: not yet) */
};

static struct ae_shield_fx ae_shield_fxs[AE_SHIELD_FX_MAXIMUM];
boolean ae_shield_fx_pop_was_early(long unit_index);
real ae_shield_fx_band(real const *values);
static short ae_shield_fx_intensity_source(long unit_index);
static boolean ae_shield_fxs_initialized = FALSE;

/* the heights of a model's skeleton at rest, its lowest node's and its
highest's (from each node's default inverse, the model's space to the
node's) */
static struct
{
	long model_index;
	real bottom;
	real top;
} ae_shield_fx_models[AE_SHIELD_FX_MODELS];
static short ae_shield_fx_next_model = 0;

/* port/linux/src/port_config.c */
int config_boolean(const char *name);
long config_integer(const char *name);
unsigned long config_changes(void);

/* (the overshield's own rise is not Arena Evolved's: never on here) */
static boolean ae_shield_fx_enabled(
	void)
{
	return FALSE;
}

#define RECHARGE_FLASH_SECONDS 0.3f
/* the recharge band (SHIELDS=1; BAND_SECONDS, _WIDTH, _STRENGTH) */
#include <stdlib.h>
#include <string.h>
#include <math.h>
/* the prototype's switches, fixed at the numbers approved in play; any other keeps its own */
static real tuning(char const *name, real fallback)
{
	static struct { char const *name; real value; } const approved[] =
	{
		{ "BAND_SECONDS", 1.2f }, { "BAND_WIDTH", 0.65f }, { "BAND_STRENGTH", 1.0f }, { "POP_SECONDS", 0.6f },
		{ "POP_AT", 0.5f }, { "POP_VIEWER", 0.f }, { "SPARKS", 0.8f }, { "HIT_SHAPE", 3.f },
	};
	short index;

	for (index = 0; index < (short)(sizeof(approved) / sizeof(approved[0])); index++)
	{
		if (!strcmp(name, approved[index].name))
			return approved[index].value;
	}
	return fallback;
}
static boolean recharge_band_enabled(void)
{
	return ae_hit_feedback_active();
}
static real recharge_band_seconds(void) { return tuning("BAND_SECONDS", 1.0f); }
static real recharge_band_width(void) { return tuning("BAND_WIDTH", 0.45f); }
static real recharge_band_strength(void) { return tuning("BAND_STRENGTH", 1.0f); }

/* the shield pop: the body's flare whole for a moment as the shield
breaks (and its glow, shield_glow.c), on the blow that kills too */
struct shield_pop
{
	long unit_index;
	real start;
	real values[4];
	/* (the pop's own shape: a white flash of the whole body, then a band of it
	falling from the head to the feet: the shield coming off) */
	real t;
	real bottom;
	real top;
};
/* (the first 8: pops the game's damage starts, in turn; the rest: pops as each
local player's view sees them, POP_VIEWER) */
#define SHIELD_POPS 48
static struct shield_pop shield_pops[SHIELD_POPS];
static boolean shield_pop_viewer_mode(void);
static struct shield_pop *shield_pop_view_get(long unit_index, real *value);
static struct
{
	short viewer; /* local player */
	boolean down; /* this viewer's weapon finishes the unit with one hit to the head */
	real last_seen;
} shield_pop_views[SHIELD_POPS];
static short shield_pop_next = 0;
static boolean shield_pops_initialized = FALSE;
static real shield_pop_seconds(void) { return tuning("POP_SECONDS", 0.45f); }

void ae_shield_fx_pop(
	long unit_index)
{
	short index;

	if (!recharge_band_enabled() || shield_pop_viewer_mode())
		return;
	if (!shield_pops_initialized)
	{
		for (index = 0; index < SHIELD_POPS; index++)
			shield_pops[index].unit_index = NONE;
		shield_pops_initialized = TRUE;
	}
	shield_pops[shield_pop_next].unit_index = unit_index;
	shield_pops[shield_pop_next].start = global_frame_parameters.game_time_sec;
	shield_pop_next = (short)((shield_pop_next + 1) % 8);
}

/* the pop shown early, as the shield falls to POP_AT of itself or
under (visual only: the shield is the game's); once until it has recharged past it */
static long shield_pop_early_units[32];
static boolean shield_pop_early_initialized = FALSE;
real ae_shield_fx_pop_at(void)
{
	return recharge_band_enabled() && !shield_pop_viewer_mode() ? tuning("POP_AT", 0.f) : 0.f;
}
static long *shield_pop_early_find(long unit_index)
{
	short index;

	if (!shield_pop_early_initialized)
	{
		for (index = 0; index < 32; index++)
			shield_pop_early_units[index] = NONE;
		shield_pop_early_initialized = TRUE;
	}
	for (index = 0; index < 32; index++)
	{
		if (shield_pop_early_units[index] == unit_index)
			return &shield_pop_early_units[index];
	}
	return NULL;
}
/* TRUE: the unit's pop was shown early (so not again as its shield breaks) */
boolean ae_shield_fx_pop_was_early(long unit_index)
{
	return shield_pop_early_find(unit_index) != NULL;
}
void ae_shield_fx_pop_early(long unit_index)
{
	long *free_unit;

	if (ae_shield_fx_pop_at() <= 0.f || shield_pop_early_find(unit_index))
		return;
	free_unit = shield_pop_early_find(NONE);
	if (free_unit)
		*free_unit = unit_index;
	ae_shield_fx_pop(unit_index);
}
void ae_shield_fx_pop_rearm(long unit_index)
{
	long *unit = unit_index != NONE ? shield_pop_early_find(unit_index) : NULL;

	if (unit)
		*unit = NONE;
}

/* the shield down: while a unit's shield is down (or its pop was
shown early) and not yet recharging, thin bands of its flare flicker at
heights of their own up and down the body (a crackle) */
struct shield_crackle
{
	long unit_index;
	real values[4];
	real height; /* 0..1 of the body */
	real bottom;
	real top;
	real last_seen;
};
static struct shield_crackle shield_crackles[16];
static boolean shield_crackles_initialized = FALSE;
static real shield_crackle_strength(void) { return tuning("SPARKS", 0.8f); }

static struct shield_crackle *shield_crackle_for_values(
	real const *values)
{
	short index;

	for (index = 0; values && shield_crackles_initialized && index < 16; index++)
	{
		if (shield_crackles[index].unit_index != NONE && values == shield_crackles[index].values)
			return &shield_crackles[index];
	}
	return NULL;
}

/* option A (off): the pop as each local player's view sees it: a unit pops for
a viewer as the weapon that viewer holds comes to finish it with one hit to the
head (FINISH_PISTOL points, 25; FINISH_RIFLE, 20.6; any other
weapon: the pistol's), or as it dies or its shield truly falls */
#include "render/render.h"
#include "game/players.h"
#include "tag_files/tag_files.h"
/* a machine that is not the host: no damage is dealt here (the host deals it and sends each unit's shield), so the
pop and the shield-down flicker are read from the shield's level as it arrives, each view's own */
static boolean guest_machine(void)
{
	return global_network_game_server_get() == NULL;
}

static boolean shield_pop_viewer_mode(void)
{
	return recharge_band_enabled() && (tuning("POP_VIEWER", 0.f) != 0.f || guest_machine());
}

static real shield_pop_viewer_finisher(
	short viewer,
	long target_unit_index)
{
	long player_index = viewer >= 0 && viewer < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS ? local_player_get_player_index(viewer) : NONE;
	long viewer_unit_index = player_index != NONE ? player_get(player_index)->unit_index : NONE;
	real pistol = tuning("FINISH_PISTOL", 25.f);
	struct unit_datum *viewer_unit;
	long weapon_index;
	struct object_datum *weapon;
	char const *name;

	/* (a guest: the one finishing hit for every view, as the host reckons it, with room for the rounding of the
	shield it was sent) */
	if (tuning("POP_VIEWER", 0.f) == 0.f)
		return pistol + 0.02f;

	if (viewer_unit_index == NONE || viewer_unit_index == target_unit_index)
		return pistol;
	viewer_unit = unit_try_and_get(viewer_unit_index);
	if (!viewer_unit || viewer_unit->unit.current_weapon_index < 0)
		return pistol;
	weapon_index = unit_inventory_get_weapon(viewer_unit_index, viewer_unit->unit.current_weapon_index);
	weapon = weapon_index != NONE ? object_try_and_get(weapon_index) : NULL;
	name = weapon ? tag_get_name(weapon->definition_index) : NULL;
	if (name && (strstr(name, "battle") || strstr(name, "_br") || strstr(name, "\\br")))
		return tuning("FINISH_RIFLE", 20.6f);
	return pistol;
}

/* whether the unit is down for the view being drawn, with its entry (NULL: none) */
static struct shield_pop *shield_pop_view_entry(
	long unit_index,
	boolean *down_now)
{
	struct unit_datum *unit = unit_try_and_get(unit_index);
	short viewer = render.local_player_index;
	real now = global_frame_parameters.game_time_sec;
	struct shield_pop *entry = NULL;
	struct shield_pop *free_entry = NULL;
	boolean down;
	short index;

	if (!unit || !(unit->object.maximum_shield_vitality > 0.f) || viewer < 0)
		return NULL;
	if (!shield_pops_initialized)
	{
		for (index = 0; index < SHIELD_POPS; index++)
			shield_pops[index].unit_index = NONE;
		shield_pops_initialized = TRUE;
	}
	down = TEST_FLAG(unit->object.damage_flags, _object_dead_bit) ||
		TEST_FLAG(unit->object.damage_flags, _object_shield_depleted_bit) ||
		(unit->object.shield_vitality < 1.f &&
			shield_pop_viewer_finisher(viewer, unit_index) -
				unit->object.maximum_shield_vitality * unit->object.shield_vitality > 0.f);
	for (index = 8; index < SHIELD_POPS; index++)
	{
		struct shield_pop *each = &shield_pops[index];

		if (each->unit_index != NONE &&
			(now - shield_pop_views[index].last_seen > 2.f || now < shield_pop_views[index].last_seen - 2.f))
		{
			each->unit_index = NONE;
		}
		if (each->unit_index == unit_index && shield_pop_views[index].viewer == viewer)
			entry = each;
		else if (each->unit_index == NONE && !free_entry)
			free_entry = each;
	}
	if (!entry)
	{
		/* (first seen: as it is, with no pop) */
		if (!free_entry)
			return NULL;
		entry = free_entry;
		index = (short)(entry - shield_pops);
		entry->unit_index = unit_index;
		entry->start = now - 100.f;
		shield_pop_views[index].viewer = viewer;
		shield_pop_views[index].down = down;
	}
	index = (short)(entry - shield_pops);
	shield_pop_views[index].last_seen = now;
	if (down && !shield_pop_views[index].down)
		entry->start = now;
	shield_pop_views[index].down = down;
	*down_now = down;
	return entry;
}

static struct shield_pop *shield_pop_view_get(
	long unit_index,
	real *value)
{
	boolean down = FALSE;
	struct shield_pop *pop = shield_pop_view_entry(unit_index, &down);
	real t;

	if (!pop)
		return NULL;
	t = (global_frame_parameters.game_time_sec - pop->start) / shield_pop_seconds();
	if (t < 0.f || t >= 1.f)
		return NULL;
	*value = t < 0.33f ? 1.f : 1.f - (t - 0.33f) / 0.67f;
	pop->t = t;
	return pop;
}

/* whether the unit is down for the view being drawn (its crackle) */
static boolean shield_pop_view_down(
	long unit_index)
{
	boolean down = FALSE;

	return shield_pop_viewer_mode() && shield_pop_view_entry(unit_index, &down) && down;
}

static struct shield_pop *shield_pop_get(
	long unit_index,
	real *value)
{
	short index;

	if (shield_pop_viewer_mode())
		return shield_pop_view_get(unit_index, value);
	for (index = 0; shield_pops_initialized && index < 8; index++)
	{
		struct shield_pop *pop = &shield_pops[index];
		real t;

		if (pop->unit_index != unit_index)
			continue;
		t = (global_frame_parameters.game_time_sec - pop->start) / shield_pop_seconds();
		if (t < 0.f || t >= 1.f)
			continue;
		/* (full for the first third, then away) */
		*value = t < 0.33f ? 1.f : 1.f - (t - 0.33f) / 0.67f;
		pop->t = t;
		return pop;
	}
	return NULL;
}

/* the pop's flare, 0..1 (0: none), for the glow */
real ae_shield_fx_pop_value(
	long unit_index)
{
	real value = 0.f;

	if (shield_pop_viewer_mode())
	{
		short index;

		for (index = 8; shield_pops_initialized && index < SHIELD_POPS; index++)
		{
			real t = (global_frame_parameters.game_time_sec - shield_pops[index].start) / shield_pop_seconds();

			if (shield_pops[index].unit_index == unit_index && t >= 0.f && t < 1.f)
				value = MAX(value, t < 0.33f ? 1.f : 1.f - (t - 0.33f) / 0.67f);
		}
		return value;
	}
	return shield_pop_get(unit_index, &value) ? value : 0.f;
}

static boolean ae_shield_fx_model_heights(
	long model_index,
	real *bottom,
	real *top)
{
	struct model *model;
	short index;

	if (model_index == NONE)
		return FALSE;
	for (index = 0; index < AE_SHIELD_FX_MODELS; index++)
	{
		if (ae_shield_fx_models[index].model_index == model_index)
		{
			*bottom = ae_shield_fx_models[index].bottom;
			*top = ae_shield_fx_models[index].top;
			return *top > *bottom;
		}
	}
	model = model_definition_get(model_index);
	if (!model || model->nodes.count <= 0)
		return FALSE;
	*bottom = REAL_MAX;
	*top = -REAL_MAX;
	for (index = 0; index < model->nodes.count; index++)
	{
		struct model_node const *node = TAG_BLOCK_GET_ELEMENT(&model->nodes, index, struct model_node);
		real_matrix4x3 const *inverse = &node->runtime_default_inverse_matrix;
		/* (the node's origin, p, has inverse p = 0: p = -R^T t / scale) */
		real z = -(inverse->up.i * inverse->position.x + inverse->up.j * inverse->position.y +
			inverse->up.k * inverse->position.z);

		if (inverse->scale > _real_epsilon)
			z /= inverse->scale;
		if (!valid_real(z))
			continue;
		*bottom = MIN(*bottom, z);
		*top = MAX(*top, z);
	}
	if (!(*top > *bottom))
		return FALSE;
	/* (the skin past the nodes: the soles below the toes, the helmet over
	the head) */
	{
		real height = *top - *bottom;

		*bottom -= 0.08f * height;
		*top += 0.18f * height;
	}
	index = ae_shield_fx_next_model;
	ae_shield_fx_next_model = (short)((ae_shield_fx_next_model + 1) % AE_SHIELD_FX_MODELS);
	ae_shield_fx_models[index].model_index = model_index;
	ae_shield_fx_models[index].bottom = *bottom;
	ae_shield_fx_models[index].top = *top;
	return TRUE;
}

/* the source of the intensity of the unit's modifier shader (a plasma one),
0..3; NONE for none */
static short ae_shield_fx_intensity_source(
	long unit_index)
{
	struct object_definition *definition = object_definition_get(object_get(unit_index)->definition_index);
	struct shader *shader;
	short source;

	if (!definition || definition->object.modifier_shader.index == NONE)
		return NONE;
	shader = shader_definition_get(definition->object.modifier_shader.index);
	if (!shader || shader->base.type != _shader_type_transparent_plasma_glow)
		return NONE;
	source = ((struct ae_shield_fx_plasma const *)((byte const *)shader + sizeof(struct shader)))->
		intensity_exponent_source;
	return source >= 1 && source <= 4 ? (short)(source - 1) : NONE;
}

/* a unit's overshield, while it has one and lives (its shield over 1, or
charging past it); NULL for none */
static struct ae_shield_fx *ae_shield_fx_get_any(
	long unit_index)
{
	struct unit_datum *unit = unit_try_and_get(unit_index);
	struct ae_shield_fx *effect = NULL;
	struct ae_shield_fx *free_effect = NULL;
	boolean charging;
	boolean recharging;
	real now;
	short index;

	if (!ae_shield_fxs_initialized)
	{
		for (index = 0; index < AE_SHIELD_FX_MAXIMUM; index++)
			ae_shield_fxs[index].unit_index = NONE;
		for (index = 0; index < AE_SHIELD_FX_MODELS; index++)
			ae_shield_fx_models[index].model_index = NONE;
		ae_shield_fxs_initialized = TRUE;
	}
	now = global_frame_parameters.game_time_sec;
	for (index = 0; index < AE_SHIELD_FX_MAXIMUM; index++)
	{
		struct ae_shield_fx *each = &ae_shield_fxs[index];

		/* (one not looked at for a while: its unit gone, or out of sight) */
		if (each->unit_index != NONE && (now - each->last_seen > 1.f || now < each->last_seen - 1.f))
			each->unit_index = NONE;
		if (each->unit_index == unit_index)
			effect = each;
		else if (each->unit_index == NONE && !free_effect)
			free_effect = each;
	}
	if (!unit || !(ae_shield_fx_enabled() || recharge_band_enabled()))
		return NULL;
	charging = TEST_FLAG(unit->object.damage_flags, _object_shield_over_charging_bit);
	if (!ae_shield_fx_enabled())
		charging = FALSE;
	recharging = recharge_band_enabled() && !charging && unit->object.shield_vitality < 1.f &&
		TEST_FLAG(unit->object.damage_flags, _object_shield_charging_bit);
	if (effect && effect->recharge != recharging && !(effect->recharge && unit->object.shield_vitality >= 1.f && !charging))
	{
		/* (one kind became the other) */
		effect->unit_index = NONE;
		free_effect = effect;
		effect = NULL;
	}
	if (TEST_FLAG(unit->object.damage_flags, _object_dead_bit) ||
		!(charging || recharging || (effect && effect->recharge && (effect->end <= 0.f || now - effect->end < RECHARGE_FLASH_SECONDS)) ||
			(ae_shield_fx_enabled() && unit->object.shield_vitality > 1.f)))
	{
		if (effect)
			effect->unit_index = NONE;
		return NULL;
	}
	if (!effect)
	{
		real shield = unit->object.shield_vitality;

		if (!free_effect)
			return NULL;
		effect = free_effect;
		effect->unit_index = unit_index;
		effect->recharge = recharging;
		effect->start_vitality = unit->object.shield_vitality;
		effect->end = 0.f;
		effect->rise = recharging ? recharge_band_seconds() : charging && shield <= AE_SHIELD_FX_DOWN ?
			AE_SHIELD_FX_RISE_FROM_DOWN : AE_SHIELD_FX_RISE;
		/* (seen part of the way through its charge: as far on; seen charged,
		risen) */
		effect->start = now;
		if (recharging)
			;
		else if (!charging)
			effect->start = now - effect->rise;
		else if (shield > 1.f)
			effect->start = now - MIN(shield - 1.f, effect->rise);
	}
	effect->last_seen = now;
	{
		real t = PIN((now - effect->start) / effect->rise, 0.f, 1.f);

		/* (smooth at both ends) */
		effect->progress = t * t * (3.f - 2.f * t);
		/* (a recharge band: as far up the body as the shield is back, so its top
		reaches the head as the shield is full; then a last flash) */
		if (effect->recharge)
		{
			real from = MIN(effect->start_vitality, 0.95f);

			effect->progress = PIN((unit->object.shield_vitality - from) / (1.f - from), 0.f, 1.f);
			if (effect->progress >= 1.f && effect->end <= 0.f)
				effect->end = now;
		}
	}
	return effect;
}

/* an overshield only (a recharge band is not one) */
static struct ae_shield_fx *ae_shield_fx_get(
	long unit_index)
{
	struct ae_shield_fx *effect = ae_shield_fx_get_any(unit_index);

	return effect && !effect->recharge ? effect : NULL;
}

/* ---------- public code */

/* whether the unit has an overshield, as this machine draws it */
boolean ae_shield_fx_active(
	long object_index)
{
	struct object_datum *object = object_try_and_get(object_index);

	return object && TEST_FLAG(_object_mask_unit, object->object.type) && ae_shield_fx_get(object_index) != NULL;
}

/* the values a unit's modifier shader is drawn with, given its own: with an
overshield, a copy whose intensity is full (on the body, or the arms) that
ae_shield_fx_draw knows again; otherwise its own */
real const *ae_shield_fx_values(
	long unit_index,
	real const *values,
	boolean arms)
{
	struct ae_shield_fx *effect;
	short source;
	real *copy;

	if (!values || !(ae_shield_fx_enabled() || recharge_band_enabled()))
		return values;
	{
		real pop_value = 0.f;
		struct shield_pop *pop = shield_pop_get(unit_index, &pop_value);
		short pop_source = pop ? ae_shield_fx_intensity_source(unit_index) : NONE;

		if (pop && pop_source != NONE)
		{
			csmemcpy(pop->values, values, sizeof(pop->values));
			/* (full all the way: its fall is what ends it) */
			pop->values[pop_source] = 1.f;
			if (arms || !ae_shield_fx_model_heights(
				object_definition_get(object_get(unit_index)->definition_index)->object.model.index,
				&pop->bottom, &pop->top))
			{
				pop->bottom = pop->top = 0.f;
				pop->values[pop_source] = pop_value;
			}
			return pop->values;
		}
	}
	effect = ae_shield_fx_get_any(unit_index);
	if (effect && effect->recharge && effect->progress >= 1.f)
	{
		short flash_source = ae_shield_fx_intensity_source(unit_index);
		real u = effect->end > 0.f ? (global_frame_parameters.game_time_sec - effect->end) / RECHARGE_FLASH_SECONDS : 1.f;
		real *flash = arms ? effect->arms_values : effect->body_values;

		if (flash_source == NONE || u < 0.f || u >= 1.f || tuning("BAND_FLASH", 1.f) == 0.f)
			return values;
		csmemcpy(flash, values, sizeof(effect->body_values));
		flash[flash_source] = MAX(values[flash_source], (1.f - u) * tuning("BAND_FLASH", 1.f));
		return flash;
	}
	/* the recharge band on the first-person arms: a swell as the band
	passes their height on the body */
	if (effect && effect->recharge && arms)
	{
		short arms_source = ae_shield_fx_intensity_source(unit_index);
		real centre = tuning("ARMS_AT", 0.55f);
		real half = 0.28f;
		real swell = 1.f - (real)fabs((double)(effect->progress - centre)) / half;

		if (arms_source == NONE || swell <= 0.f)
			return values;
		csmemcpy(effect->arms_values, values, sizeof(effect->arms_values));
		effect->arms_values[arms_source] = MAX(values[arms_source], recharge_band_strength() * swell);
		return effect->arms_values;
	}
	/* the crackle of a shield that is down */
	if (!effect && !arms && recharge_band_enabled() && shield_crackle_strength() > 0.f)
	{
		struct unit_datum *unit = unit_try_and_get(unit_index);
		short crackle_source = unit ? ae_shield_fx_intensity_source(unit_index) : NONE;

		if (unit && crackle_source != NONE && !TEST_FLAG(unit->object.damage_flags, _object_dead_bit) &&
			unit->object.maximum_shield_vitality > 0.f &&
			!TEST_FLAG(unit->object.damage_flags, _object_shield_charging_bit) &&
			(shield_pop_viewer_mode() ? shield_pop_view_down(unit_index) :
				(TEST_FLAG(unit->object.damage_flags, _object_shield_depleted_bit) || ae_shield_fx_pop_was_early(unit_index))))
		{
			real now = global_frame_parameters.game_time_sec;
			unsigned long slot = (unsigned long)(now * 14.f);
			unsigned long hash = ((unsigned long)unit_index * 2654435761u) ^ (slot * 40503u);
			struct shield_crackle *crackle = NULL;
			struct shield_crackle *free_crackle = NULL;
			short index;

			hash ^= hash >> 13; hash *= 0x5bd1e995u; hash ^= hash >> 15;
			if (!shield_crackles_initialized)
			{
				for (index = 0; index < 16; index++)
					shield_crackles[index].unit_index = NONE;
				shield_crackles_initialized = TRUE;
			}
			for (index = 0; index < 16; index++)
			{
				struct shield_crackle *each = &shield_crackles[index];

				if (each->unit_index != NONE && (now - each->last_seen > 1.f || now < each->last_seen - 1.f))
					each->unit_index = NONE;
				if (each->unit_index == unit_index)
					crackle = each;
				else if (each->unit_index == NONE && !free_crackle)
					free_crackle = each;
			}
			if (!crackle)
				crackle = free_crackle;
			/* (lit a little over half the time) */
			if (crackle && (hash % 100u) < (unsigned long)tuning("SPARKS_PERCENT", 22.f) && ae_shield_fx_model_heights(
				object_definition_get(object_get(unit_index)->definition_index)->object.model.index,
				&crackle->bottom, &crackle->top))
			{
				crackle->unit_index = unit_index;
				crackle->last_seen = now;
				crackle->height = (real)((hash >> 8) % 100u) / 100.f;
				csmemcpy(crackle->values, values, sizeof(crackle->values));
				crackle->values[crackle_source] = MAX(values[crackle_source],
					shield_crackle_strength() * (0.45f + 0.55f * (real)((hash >> 16) % 100u) / 100.f));
				return crackle->values;
			}
		}
	}
	source = effect ? ae_shield_fx_intensity_source(unit_index) : NONE;
	/* a hit's flare as a short pulse (drawn only: the game's value, which
	fades over two seconds, raised to HIT_SHAPE) */
	if ((!effect || source == NONE) && recharge_band_enabled() && !arms)
	{
		static real shaped[64][4];
		short hit_source = ae_shield_fx_intensity_source(unit_index);
		real shape = tuning("HIT_SHAPE", 3.f);

		if (hit_source != NONE && shape > 1.f && values[hit_source] > 0.f && values[hit_source] < 1.f)
		{
			real *copy_values = shaped[(unsigned long)unit_index & 63u];

			csmemcpy(copy_values, values, sizeof(shaped[0]));
			copy_values[hit_source] = (real)pow((double)values[hit_source], (double)shape);
			return copy_values;
		}
	}
	if (!effect || source == NONE)
		return values;
	copy = arms ? effect->arms_values : effect->body_values;
	csmemcpy(copy, values, sizeof(effect->body_values));
	if (arms)
	{
		copy[source] = PIN((effect->progress - AE_SHIELD_FX_ARMS_START) / AE_SHIELD_FX_ARMS_LENGTH, 0.f, 1.f);
	}
	else
	{
		copy[source] = effect->recharge ? MAX(values[source], recharge_band_strength()) : 1.f;
		if (!ae_shield_fx_model_heights(
			object_definition_get(object_get(unit_index)->definition_index)->object.model.index,
			&effect->bottom, &effect->top))
		{
			effect->bottom = effect->top = 0.f;
		}
	}
	return copy;
}

/* a plasma shader drawn with values from ae_shield_fx_values: the
speed its energy moves at, and the height it fades above on its model (the
fade's inverse softness and height: 0 and 0 for none); FALSE for values not
from it */
boolean ae_shield_fx_draw(
	real const *values,
	real *energy_speed,
	real *inverse_softness,
	real *height)
{
	short index;

	if (!values)
		return FALSE;
	{
		short pop_index;

		for (pop_index = 0; shield_pops_initialized && pop_index < SHIELD_POPS; pop_index++)
		{
			struct shield_pop const *pop = &shield_pops[pop_index];

			if (pop->unit_index != NONE && values == pop->values)
			{
				real range = pop->top - pop->bottom;
				real hold = tuning("POP_HOLD", 0.25f);

				*energy_speed = 5.f;
				*inverse_softness = 0.f;
				*height = 0.f;
				/* (whole for the hold, then the band's top falls from over the head
				to under the feet) */
				if (range > 0.f && pop->t > hold)
				{
					real fall = (pop->t - hold) / (1.f - hold);

					*inverse_softness = 1.f / (0.10f * range);
					*height = pop->top + 0.1f * range - (range * 1.2f) * fall * fall;
				}
				return TRUE;
			}
		}
	}
	{
		struct shield_crackle *crackle = shield_crackle_for_values(values);

		if (crackle)
		{
			real range = crackle->top - crackle->bottom;

			*energy_speed = 4.f;
			*inverse_softness = 1.f / (0.04f * range);
			*height = crackle->bottom + range * (0.1f + 0.95f * crackle->height);
			return TRUE;
		}
	}
	for (index = 0; index < AE_SHIELD_FX_MAXIMUM; index++)
	{
		struct ae_shield_fx const *effect = &ae_shield_fxs[index];

		if (effect->unit_index == NONE || (values != effect->body_values && values != effect->arms_values))
			continue;
		*energy_speed = effect->recharge ? 1.f : AE_SHIELD_FX_ENERGY_SPEED;
		*inverse_softness = 0.f;
		*height = 0.f;
		if (effect->recharge && (values == effect->arms_values || effect->progress >= 1.f))
			return TRUE;
		if (effect->recharge && effect->top > effect->bottom)
		{
			real range = effect->top - effect->bottom;
			real band = recharge_band_width() * range;
			real edge = 0.03f * range; /* (a sharp front; the band's own ramp is its tail) */

			/* (the band's top from the feet to its own width over the head) */
			*inverse_softness = 1.f / edge;
			*height = effect->bottom + (range + band + edge) * effect->progress;
			return TRUE;
		}
		if (values == effect->body_values && effect->top > effect->bottom && effect->progress < 1.f)
		{
			real edge = AE_SHIELD_FX_EDGE * (effect->top - effect->bottom);

			/* (from all of the edge below the feet to all of it above the
			head) */
			*inverse_softness = 1.f / edge;
			*height = effect->bottom + (effect->top - effect->bottom + edge) * effect->progress;
		}
		return TRUE;
	}
	return FALSE;
}

/* the unit's overshield's flare, for its glow (shield_glow.c): 0..1, or
NONE (-1) for no overshield */
real ae_shield_fx_flare(
	long unit_index)
{
	struct ae_shield_fx *effect = ae_shield_fx_get(unit_index);

	return effect ? effect->progress : -1.f;
}

/* whether an effect a hit on an overshield may play in place of its own:
one that only shows and sounds (sounds, lights, decals, particles,
contrails), so that nothing of the game changes */
boolean ae_shield_fx_hit_effect_allowed(
	long effect_index)
{
	struct effect_definition const *definition;
	short event_index;

	if (effect_index == NONE)
		return FALSE;
	definition = (struct effect_definition const *)tag_get(EFFECT_DEFINITION_TAG, effect_index);
	if (!definition)
		return FALSE;
	for (event_index = 0; event_index < definition->events.count; event_index++)
	{
		struct effect_event_definition const *event =
			TAG_BLOCK_GET_ELEMENT(&definition->events, event_index, struct effect_event_definition);
		short part_index;

		for (part_index = 0; part_index < event->parts.count; part_index++)
		{
			struct effect_part_definition const *part =
				TAG_BLOCK_GET_ELEMENT(&event->parts, part_index, struct effect_part_definition);
			unsigned long group = part->reference.group_tag;

			if (part->reference.index != NONE && group != SOUND_DEFINITION_TAG && group != LIGHT_DEFINITION_TAG &&
				group != DECAL_GROUP_TAG && group != PARTICLE_SYSTEM_DEFINITION_TAG && group != PARTICLE_TAG &&
				group != CONTRAIL_DEFINITION_TAG)
			{
				return FALSE;
			}
		}
	}
	return TRUE;
}

/* a recharging unit's band, for its glow: 0..1 over the rise (0: none) */
real ae_shield_fx_recharge_value(
	long unit_index)
{
	short index;

	for (index = 0; index < AE_SHIELD_FX_MAXIMUM; index++)
	{
		struct ae_shield_fx const *effect = &ae_shield_fxs[index];

		if (effect->unit_index == unit_index && effect->recharge && effect->progress < 1.f &&
			global_frame_parameters.game_time_sec - effect->last_seen < 0.25f)
		{
			return (real)sin(3.14159265 * (double)effect->progress) * tuning("BAND_GLOW", 1.f);
		}
	}
	return 0.f;
}

/* the band's width on the model, for values of a recharge band; 0 otherwise */
real ae_shield_fx_band(
	real const *values)
{
	short index;

	{
		struct shield_crackle *crackle = shield_crackle_for_values(values);

		if (crackle)
			return 0.10f * (crackle->top - crackle->bottom);
	}
	for (index = 0; values && index < AE_SHIELD_FX_MAXIMUM; index++)
	{
		struct ae_shield_fx const *effect = &ae_shield_fxs[index];

		if (effect->unit_index != NONE && effect->recharge && values == effect->body_values && effect->top > effect->bottom)
			return recharge_band_width() * (effect->top - effect->bottom);
	}
	return 0.f;
}

/* the color a plasma shader drawn with values from ae_shield_fx_values
is drawn in, the overshield's own; FALSE for values not from it */
boolean ae_shield_fx_color(
	real const *values,
	real_rgb_color *color)
{
	real energy_speed, inverse_softness, height;

	{
		short pop_index;

		/* (a pop: white, so it is not a hit's gold) */
		for (pop_index = 0; shield_pops_initialized && pop_index < SHIELD_POPS; pop_index++)
		{
			if (shield_pops[pop_index].unit_index != NONE && values == shield_pops[pop_index].values)
			{
				color->red = 1.f;
				color->green = 1.f;
				color->blue = 0.92f;
				return TRUE;
			}
		}
	}
	if (!ae_shield_fx_draw(values, &energy_speed, &inverse_softness, &height))
		return FALSE;
	{
		short index;

		/* (a recharge band: the tint, white to a pale yellow as it rises) */
		for (index = 0; index < AE_SHIELD_FX_MAXIMUM; index++)
		{
			struct ae_shield_fx const *effect = &ae_shield_fxs[index];

			if (effect->unit_index != NONE && effect->recharge &&
				(values == effect->body_values || values == effect->arms_values) &&
				tuning("BAND_TINT", 1.f) != 0.f)
			{
				color->red = 1.f;
				color->green = 1.f - 0.04f * effect->progress;
				color->blue = 1.f - 0.49f * effect->progress;
				return TRUE;
			}
		}
	}
	if (ae_shield_fx_band(values) > 0.f || shield_crackle_for_values(values))
		return FALSE;
	*color = ae_shield_fx_rgb;
	return TRUE;
}

/* the overshield's color, for its glow */
real_rgb_color const *ae_shield_fx_overshield_color(
	void)
{
	return &ae_shield_fx_rgb;
}

/* ---------- what the AE hook lines call (ae_shield_fx.h) */

/* the finishing hit's points against a shield: the magnum's (the strongest weapon that kills with one hit to
the head) */
#define FINISHER_POINTS 25.f

static boolean early_pop_now = FALSE;

boolean ae_shield_fx_no_burst(
	long object_index)
{
	return recharge_band_enabled() && ae_shield_fx_pop_was_early(object_index);
}

void ae_shield_fx_depleted(
	long object_index)
{
	struct object_datum *object = recharge_band_enabled() ? object_try_and_get(object_index) : NULL;

	if (object && TEST_FLAG(_object_mask_unit, object->object.type) && !ae_shield_fx_pop_was_early(object_index))
		ae_shield_fx_pop(object_index);
}

void ae_shield_fx_shield_damaged(
	long object_index,
	long burst_effect_index,
	unsigned long *being_damaged_flags)
{
	struct object_datum *object = recharge_band_enabled() ? object_try_and_get(object_index) : NULL;

	if (!object)
		return;
	/* (the pop, early: only when the finishing hit would go through what is left, reckoned as the shield's own
	damage is: a hit that only empties it kills nobody) */
	if (TEST_FLAG(_object_mask_unit, object->object.type) && object->object.shield_vitality <= ae_shield_fx_pop_at() &&
		!ae_shield_fx_pop_was_early(object_index) &&
		FINISHER_POINTS - object->object.maximum_shield_vitality * object->object.shield_vitality > 0.f)
	{
		/* (the map's own burst for a shield's fall, now) */
		if (burst_effect_index != NONE)
			effect_new_from_object(burst_effect_index, object_index, object_index, NONE, 0.f, 0.f, NULL, NULL);
		error(_error_silent, "hit feedback: pop, unit %08x, shield %.3f", (unsigned int)object_index,
			(double)object->object.shield_vitality);
		ae_shield_fx_pop_early(object_index);
		early_pop_now = TRUE;
	}
	/* (a shield brought to exactly nothing is down: the game's own test, more than what is left, leaves it
	standing at nothing, never broken, with no burst) */
	if (object->object.shield_vitality <= 0.0001f &&
		!TEST_FLAG(object->object.damage_flags, _object_cannot_take_damage_bit) &&
		!TEST_FLAG(object->object.damage_flags, _object_shield_depleted_bit))
	{
		object->object.shield_vitality = 0.f;
		object_deplete_shield(object_index);
		if (being_damaged_flags)
			SET_FLAG(*being_damaged_flags, _object_being_damaged_shield_depleted_bit, TRUE);
	}
}

boolean ae_shield_fx_early_pop_taken(
	void)
{
	boolean taken = early_pop_now;

	early_pop_now = FALSE;
	return taken;
}

void ae_shield_fx_recharging(
	long object_index,
	real shield)
{
	if (recharge_band_enabled() && shield > ae_shield_fx_pop_at() + 0.05f)
		ae_shield_fx_pop_rearm(object_index);
}

short ae_shield_fx_bloodshot(
	long object_index,
	unsigned long damage_flags,
	long owner_object_index,
	long hit_effect_index,
	real body_damage,
	unsigned long being_damaged_flags)
{
	struct object_datum *object = recharge_band_enabled() ? object_try_and_get(object_index) : NULL;

	/* (real damage to the body, or the headshot that kills: not the crumb a round that only empties a shield
	leaves; and only the round the game shows as a shield hit, the one that broke the shield and went on: a
	body with no shield has the game's own blood already) */
	if (!object || object->object.type != _object_type_biped || hit_effect_index == NONE ||
		!(body_damage > 0.05f || TEST_FLAG(being_damaged_flags, _object_being_damaged_killed_instantly_bit)) ||
		!TEST_FLAG(being_damaged_flags, _object_being_damaged_shield_depleted_bit) ||
		TEST_FLAG(damage_flags, _damage_area_of_effect_bit) || owner_object_index == NONE)
	{
		return 0;
	}
	error(_error_silent, "hit feedback: bloodshot, unit %08x, body damage %.3f", (unsigned int)object_index,
		(double)body_damage);
	{
		long bursts = config_integer("debug.hit_feedback_blood");

		return (short)(bursts < 1 ? 1 : bursts > 6 ? 6 : bursts);
	}
}

real_vector3d const *ae_shield_fx_blood_direction(
	real_vector3d const *direction,
	short burst)
{
	static real_vector3d thrown;
	static unsigned long seed = 0x2F6E2B1ul;
	real length;

	if (burst <= 1 || !direction)
		return direction;
	/* (a cone about the hit's own direction, each burst its own way) */
	thrown = *direction;
	seed = (seed * 1664525ul + 1013904223ul) & 0xFFFFFFFFul;
	thrown.i += ((real)((seed >> 8) & 0xFF) / 127.5f - 1.f) * 0.45f;
	thrown.j += ((real)((seed >> 16) & 0xFF) / 127.5f - 1.f) * 0.45f;
	thrown.k += ((real)((seed >> 24) & 0xFF) / 127.5f - 1.f) * 0.3f + 0.1f;
	length = (real)sqrt((double)(thrown.i * thrown.i + thrown.j * thrown.j + thrown.k * thrown.k));
	if (!(length > 0.001f))
		return direction;
	thrown.i /= length; thrown.j /= length; thrown.k /= length;
	return &thrown;
}

long ae_shield_fx_blood_effect(
	long hit_effect_index)
{
	/* (the blood bundle's body impact when the map being played has it; else the body's own) */
	long bundled = recharge_band_enabled() ?
		tag_loaded(EFFECT_DEFINITION_TAG, "cmt\\effects\\impacts\\impact human armored") : NONE;

	return bundled != NONE ? bundled : hit_effect_index;
}

boolean ae_shield_fx_draw_begin(
	real const *values,
	real offset,
	real scale)
{
	real energy_speed = 1.f, inverse_softness = 0.f, height = 0.f;

	if (!values || !recharge_band_enabled() || !ae_shield_fx_draw(values, &energy_speed, &inverse_softness, &height))
		return FALSE;
	if (inverse_softness > 0.f && scale != 0.f)
		ae_height_fade_set(inverse_softness, height, offset, scale, ae_shield_fx_band(values));
	return TRUE;
}

void ae_shield_fx_draw_end(
	void)
{
	ae_height_fade_clear();
}

real ae_shield_fx_glow_value(
	long unit_index,
	real value)
{
	if (recharge_band_enabled())
	{
		real pop = ae_shield_fx_pop_value(unit_index);
		real recharge = ae_shield_fx_recharge_value(unit_index);

		value = MAX(value, MAX(pop, recharge));
	}
	return value;
}

void ae_shield_fx_glow_color(
	long unit_index,
	real_rgb_color *color)
{
	if (!recharge_band_enabled())
		return;
	/* (a pop's light white, a recharge's a pale yellow) */
	if (ae_shield_fx_pop_value(unit_index) > 0.f)
	{
		color->red = 1.f; color->green = 1.f; color->blue = 0.9f;
	}
	else if (ae_shield_fx_recharge_value(unit_index) > 0.f)
	{
		color->red = 1.f; color->green = 0.97f; color->blue = 0.6f;
	}
}

real ae_shield_fx_glow_reach(
	long unit_index)
{
	return recharge_band_enabled() && ae_shield_fx_pop_value(unit_index) > 0.f ? 2.2f : 1.f;
}
