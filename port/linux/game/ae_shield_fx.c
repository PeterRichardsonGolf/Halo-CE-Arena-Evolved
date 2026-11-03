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
(animated overshield, by jcompto95-tech); only the recharge band, pop and
flicker are kept here (the overshield's own rise and the prototype's
per-weapon pop for each viewer were never turned on, and were taken out
since). To do: the finishing hit's points (25, the magnum's, typed
in: to be read from the map's weapons).
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
#include "objects/damage.h"
#include "effects/effects.h"
#include "physics/collision_model_definitions.h"
#include "game/game_engine.h"
#include "tag_files/tag_files.h"
#include "cseries/errors.h"

#include "networking/network_game_globals.h"

#include "ae_hit_feedback.h"
#include "ae_burst_rifle.h"
#include "ae_rifle_profiles.h"
#include "ae_shield_fx.h"
#include "../src/ae_height_fade.h"

enum
{
	_shader_type_transparent_plasma_glow = 10, /* (shader_get_and_verify_type's plasma, as shield_glow.c has it) */
	AE_SHIELD_FX_MAXIMUM = 16,
	AE_SHIELD_FX_MODELS = 8,
};

/* the plasma shader's fields after its header (rasterizer_xbox_plasma_energy.c's) */
struct ae_shield_fx_plasma
{
	byte reserved00[4];
	short intensity_exponent_source;
};

/* a unit's recharge band */
struct ae_shield_fx
{
	long unit_index; /* NONE: free */
	real last_seen; /* the game's seconds (render_interpolation_game_time_sec) */
	/* the values the unit's modifier shader is drawn with: one copy for the
	body, one for the arms */
	real body_values[4];
	real arms_values[4];
	/* this frame's: how far up (0..1: as far as the shield is back), and the
	model's heights */
	real progress;
	real bottom;
	real top;
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

#define RECHARGE_FLASH_SECONDS 0.3f
/* the recharge band (BAND_WIDTH, BAND_STRENGTH) */
#include <stdlib.h>
#include <string.h>
#include <math.h>
/* the prototype's switches, fixed at the numbers approved in play; any other keeps its own */
static real tuning(char const *name, real fallback)
{
	static struct { char const *name; real value; } const approved[] =
	{
		{ "BAND_WIDTH", 0.65f }, { "BAND_STRENGTH", 1.0f }, { "POP_SECONDS", 0.8f }, { "POP_HOLD", 0.4f },
		{ "POP_AT", 0.5f }, { "SPARKS", 0.8f }, { "HIT_SHAPE", 3.f },
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
local player's view sees them, on a machine that is not the host) */
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
/* the pop's flare at t (0..1 of its time): full for its first half, then away (owner 2026-10-15: "doesn't seem as
visible": was full for a third of 0.6 s, the body white for a quarter, the light at 2.2 times the hit glow's reach) */
static real shield_pop_strength(real t) { return t < 0.5f ? 1.f : 1.f - (t - 0.5f) / 0.5f; }

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

#include "render/render.h"
/* a machine that is not the host: no damage is dealt here (the host deals it and sends each unit's shield), so the
pop and the shield-down flicker are read from the shield's level as it arrives, each view's own */
static boolean guest_machine(void)
{
	return global_network_game_server_get() == NULL;
}

static boolean shield_pop_viewer_mode(void)
{
	return recharge_band_enabled() && guest_machine();
}

/* the finishing hit's points against a shield: the magnum's (the strongest weapon that kills with one hit to
the head) */
#define FINISHER_POINTS 25.f

/* (a guest: the one finishing hit for every view, as the host reckons it, with room for the rounding of the
shield it was sent) */
#define GUEST_FINISHER_POINTS (FINISHER_POINTS + 0.02f)

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
			GUEST_FINISHER_POINTS - unit->object.maximum_shield_vitality * unit->object.shield_vitality > 0.f);
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
	*value = shield_pop_strength(t);
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
		*value = shield_pop_strength(t);
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
				value = MAX(value, shield_pop_strength(t));
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

/* a unit's recharge band, while its shield recharges (and the flash as it is
full); NULL for none */
static struct ae_shield_fx *ae_shield_fx_get_any(
	long unit_index)
{
	struct unit_datum *unit = unit_try_and_get(unit_index);
	struct ae_shield_fx *effect = NULL;
	struct ae_shield_fx *free_effect = NULL;
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
	if (!unit || !recharge_band_enabled())
		return NULL;
	recharging = unit->object.shield_vitality < 1.f && TEST_FLAG(unit->object.damage_flags, _object_shield_charging_bit);
	if (effect && !recharging && !(unit->object.shield_vitality >= 1.f))
	{
		/* (the recharge stopped short: hit again) */
		effect->unit_index = NONE;
		free_effect = effect;
		effect = NULL;
	}
	if (TEST_FLAG(unit->object.damage_flags, _object_dead_bit) ||
		!(recharging || (effect && (effect->end <= 0.f || now - effect->end < RECHARGE_FLASH_SECONDS))))
	{
		if (effect)
			effect->unit_index = NONE;
		return NULL;
	}
	if (!effect)
	{
		if (!free_effect)
			return NULL;
		effect = free_effect;
		effect->unit_index = unit_index;
		effect->start_vitality = unit->object.shield_vitality;
		effect->end = 0.f;
	}
	effect->last_seen = now;
	{
		/* (as far up the body as the shield is back, so its top reaches the head as the shield is full; then a
		last flash) */
		real from = MIN(effect->start_vitality, 0.95f);

		effect->progress = PIN((unit->object.shield_vitality - from) / (1.f - from), 0.f, 1.f);
		if (effect->progress >= 1.f && effect->end <= 0.f)
			effect->end = now;
	}
	return effect;
}

/* ---------- public code */

/* the values a unit's modifier shader is drawn with, given its own: a pop's,
a recharge band's, a crackle's or a hit's shaped pulse, as a copy that
ae_shield_fx_draw knows again; otherwise its own */
real const *ae_shield_fx_values(
	long unit_index,
	real const *values,
	boolean arms)
{
	struct ae_shield_fx *effect;
	short source;
	real *copy;

	if (!values || !recharge_band_enabled())
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
	if (effect && effect->progress >= 1.f)
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
	if (effect && arms)
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
	/* (the band on the body: the arms' swell is above) */
	copy = effect->body_values;
	csmemcpy(copy, values, sizeof(effect->body_values));
	copy[source] = MAX(values[source], recharge_band_strength());
	if (!ae_shield_fx_model_heights(
		object_definition_get(object_get(unit_index)->definition_index)->object.model.index,
		&effect->bottom, &effect->top))
	{
		effect->bottom = effect->top = 0.f;
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
		*energy_speed = 1.f;
		*inverse_softness = 0.f;
		*height = 0.f;
		if (values == effect->arms_values || effect->progress >= 1.f)
			return TRUE;
		if (effect->top > effect->bottom)
		{
			real range = effect->top - effect->bottom;
			real band = recharge_band_width() * range;
			real edge = 0.03f * range; /* (a sharp front; the band's own ramp is its tail) */

			/* (the band's top from the feet to its own width over the head) */
			*inverse_softness = 1.f / edge;
			*height = effect->bottom + (range + band + edge) * effect->progress;
		}
		return TRUE;
	}
	return FALSE;
}

/* a recharging unit's band, for its glow: 0..1 over the rise (0: none) */
real ae_shield_fx_recharge_value(
	long unit_index)
{
	short index;

	for (index = 0; index < AE_SHIELD_FX_MAXIMUM; index++)
	{
		struct ae_shield_fx const *effect = &ae_shield_fxs[index];

		if (effect->unit_index == unit_index && effect->progress < 1.f &&
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

		if (effect->unit_index != NONE && values == effect->body_values && effect->top > effect->bottom)
			return recharge_band_width() * (effect->top - effect->bottom);
	}
	return 0.f;
}

/* the color a plasma shader drawn with values from ae_shield_fx_values
is drawn in (a pop's white, a recharge band's tint); FALSE for the shield's
own (values not from it, a crackle's) */
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

			if (effect->unit_index != NONE &&
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
	return FALSE;
}

/* ---------- what the AE hook lines call (ae_shield_fx.h) */

static boolean early_pop_now = FALSE;

/* (0.7.0, the owner: "check that all of our shield pop indicators are correct") the hit being dealt was a single-shot
AE rifle's round (the EVO RIFLE, the DMR): its finishing head shot needs the shield gone (4 rounds empty it, the 5th to
the head kills), so its pop is the shield's own break, never early: a pop at the 3rd round's 24 % said one shot too
soon. Set by ae_shield_fx_rifle_finish, which the damage code calls first for the same hit. */
static boolean hit_needs_empty_shield = FALSE;

/* the owner's rule of 2026-10-12 ("what you built is the better way"): the pop says "one more headshot kills", and
that holds for the rifle too. A magnum's pop leaves exactly its own 25 points; three of the rifle's rounds take 20.6,
so a burst to the head would not finish. Against a shield whose pop was shown, a round of the pack's rifle takes a
third of the finishing hit's points at least (and a hair, for the rounding). No other weapon and no unpopped shield
is touched: the battle rifle's rounds alone (ae_burst_rifle_damage), not the other packs' (the SMGs' falloff, the
carbine's single rounds) */
real ae_shield_fx_rifle_finish(
	long object_index,
	long damage_definition_index,
	real shield_damage)
{
	real least = FINISHER_POINTS / 3.f + 0.005f;
	struct ae_rifle_profile const *profile = damage_definition_index == NONE ? NULL :
		ae_burst_rifle_damage_profile(damage_definition_index);

	hit_needs_empty_shield = profile && profile->rounds_a_pull == 1;
	if (!recharge_band_enabled() || damage_definition_index == NONE || !ae_burst_rifle_damage(damage_definition_index) ||
		!ae_shield_fx_pop_was_early(object_index))
	{
		return shield_damage;
	}
	return shield_damage < least ? least : shield_damage;
}

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
	{
		error(_error_silent, "hit feedback: pop, unit %08x, shield broken", (unsigned int)object_index);
		ae_shield_fx_pop(object_index);
	}
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
		!hit_needs_empty_shield && !ae_shield_fx_pop_was_early(object_index) &&
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

/* the blood pop is this many bursts of the body's hit effect, each thrown a little apart (the game's single one
is hard to see; played at 1, 3 and 5: 3 reads on every finishing shot, 5 is too much) */
#define BLOOD_POP_BURSTS 3

short ae_shield_fx_bloodshot(
	long object_index,
	unsigned long damage_flags,
	long owner_object_index,
	long hit_effect_index,
	real body_damage,
	unsigned long being_damaged_flags)
{
	struct object_datum *object = recharge_band_enabled() ? object_try_and_get(object_index) : NULL;

	boolean breaks, finishes;

	if (!object || object->object.type != _object_type_biped || hit_effect_index == NONE ||
		TEST_FLAG(damage_flags, _damage_area_of_effect_bit) || owner_object_index == NONE)
	{
		return 0;
	}
	/* (real damage to the body, or the headshot that kills: not the crumb a round that only empties a shield
	leaves; the round that broke the shield and went on) */
	breaks = (body_damage > 0.05f || TEST_FLAG(being_damaged_flags, _object_being_damaged_killed_instantly_bit)) &&
		TEST_FLAG(being_damaged_flags, _object_being_damaged_shield_depleted_bit);
	/* (or the round that kills a body whose shield was already brought to nothing and its pop shown: a harder hitter
	(the juggernaut's extra damage) empties the shield exactly with one round, and the next, the finishing one,
	meets no shield; owner 2026-10-15: the AE JUGGER pistol kills had no blood pop) */
	finishes = (TEST_FLAG(being_damaged_flags, _object_being_damaged_killed_instantly_bit) ||
		TEST_FLAG(being_damaged_flags, _object_being_damaged_body_depleted_bit)) &&
		object->object.shield_vitality <= 0.0001f && ae_shield_fx_pop_was_early(object_index);
	if (!breaks && !finishes)
		return 0;
	error(_error_silent, "hit feedback: blood pop, unit %08x, body damage %.3f", (unsigned int)object_index,
		(double)body_damage);
	return BLOOD_POP_BURSTS;
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

/* ---------- the blood pop on a machine that is not the host

No damage is dealt there: the host sends what each hit did (network_damage.c's aftermath events: the flags, the body's
damage, where and from where), so the blood pop is shown from that, by the same test as the host's */

static unsigned long guest_hit_flags = 0;
static real guest_hit_body_damage = 0.f;
static boolean guest_hit_arrived = FALSE;

/* a hit's result as the host sent it, whole (the replay is handed the flags without the kill's) */
void ae_shield_fx_guest_hit(
	unsigned long being_damaged_flags,
	real body_damage)
{
	guest_hit_flags = being_damaged_flags;
	guest_hit_body_damage = body_damage;
	guest_hit_arrived = TRUE;
}

/* the body's own hit effect (its collision model's; NONE: none) */
long ae_shield_fx_hit_effect(
	long object_index)
{
	struct object_datum *object = object_try_and_get(object_index);
	long collision_model_index = object ? object_definition_get(object->definition_index)->object.collision_model.index : NONE;

	return collision_model_index != NONE ?
		collision_model_definition_get(collision_model_index)->resistance.localized_damage_effect.index : NONE;
}

/* the bursts of the blood pop for the hit just handed over, replayed on this object (0: none); once a hit */
short ae_shield_fx_guest_blood_pop(
	long object_index,
	struct damage_data const *damage)
{
	if (!guest_hit_arrived)
		return 0;
	guest_hit_arrived = FALSE;
	return ae_shield_fx_bloodshot(object_index, damage->flags, damage->owner_object_index,
		ae_shield_fx_hit_effect(object_index), guest_hit_body_damage, guest_hit_flags);
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
	return recharge_band_enabled() && ae_shield_fx_pop_value(unit_index) > 0.f ? 3.2f : 1.f;
}
