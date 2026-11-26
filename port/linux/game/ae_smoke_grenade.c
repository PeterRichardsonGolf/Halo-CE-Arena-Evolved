/*
AE_SMOKE_GRENADE.C

THE AE SMOKE GRENADE (ae_smoke_grenade.h; the volume it makes: ae_smoke_volume.c, ae_smoke_rules.h). The map's plasma
grenade projectile is changed in place for as long as the running game asks for the smoke: its detonation effect
is the smoke pack's cloud (which has no damage), its attached and impact damage, its warning flash and every surface hit's own effect are none, its model is the frag
grenade's, nothing glows or hums on it, and the bots' fear of its blast is gone (danger radius 0). The tag's own
numbers are kept when the map loads and put back when the rule is off, so a stock gametype played after an AE one on
the same map throws Halo CE's plasma grenade.

Who throws it: whoever holds plasma grenades, which the gametype gives (AE SEARCH AND DESTROY's classes will give
this slot; until the classes, the gametype's plasma count). The projectile is the map's, synced as any grenade, so the
detonation happens where every machine has it; a machine without the pack or the rule throws and sees the plasma
grenade.
*/

#include "cseries.h"
#include "cseries/errors.h"
#include "game/game.h"
#include "game/game_engine.h"
#include "game/players.h"
#include "game/local_players.h"
#include "objects/objects.h"
#include "units/units.h"
#include "items/projectile_definitions.h"
#include "items/projectiles.h"
#include "tag_files/tag_files.h"

#include "ae_bomb_snd_rules.h"
#include "ae_smoke_rules.h"
#include "ae_smoke_grenade.h"
#include "ae_smoke_volume.h"
#include "ae_weapon_pack.h"
#include "../src/port_config.h"

long tag_loaded(long group_tag, const char *name);

#define AE_SMOKE_EFFECT_GROUP 0x65666665 /* 'effe' */

/* the numbers changed, as the map's tag has them */
struct ae_smoke_grenade_numbers
{
	long effect;
	long attached_damage;
	long impact_damage;
	long detonation_started;
	long model;
	long attachments;
	real danger_radius;
};

/* the material hits' own effects (a plasma grenade's scorch and dirt where it bursts on a surface): none of them for the
smoke, kept to put back */
enum { AE_SMOKE_GRENADE_MATERIALS = 64 };

static struct
{
	long projectile_index;
	long frag_projectile_index;
	long stock_material_effects[AE_SMOKE_GRENADE_MATERIALS];
	struct ae_smoke_grenade_numbers stock;
	boolean tuned;
} ae_smoke_grenade = { NONE, NONE };

static struct projectile_definition *ae_smoke_grenade_definition(void)
{
	return projectile_definition_get(ae_smoke_grenade.projectile_index);
}

boolean ae_smoke_asked(struct game_variant const *variant)
{
	if (!variant || !variant->game_engine_index)
		return FALSE;
	if (config_boolean("game.smoke_grenade"))
		return TRUE;
	return variant->game_engine_index == game_engine_bomb && ae_bomb_snd_flag(variant->game_engine_variant.bomb.flags);
}

void ae_smoke_grenade_new_map(void)
{
	struct projectile_definition *definition;
	long index;

	ae_smoke_grenade.projectile_index = tag_loaded(PROJECTILE_DEFINITION_TAG, AE_SMOKE_PROJECTILE_TAG);
	ae_smoke_grenade.frag_projectile_index = tag_loaded(PROJECTILE_DEFINITION_TAG, AE_SMOKE_MODEL_TAG);
	ae_smoke_grenade.tuned = FALSE;
	ae_smoke_volume_reset();
	if (ae_smoke_grenade.projectile_index == NONE || ae_smoke_grenade.frag_projectile_index == NONE)
	{
		ae_smoke_grenade.projectile_index = NONE;
		return;
	}
	definition = ae_smoke_grenade_definition();
	ae_smoke_grenade.stock.effect = definition->projectile.effect.index;
	ae_smoke_grenade.stock.attached_damage = definition->projectile.attached_detonation_damage.index;
	ae_smoke_grenade.stock.impact_damage = definition->projectile.impact_damage.index;
	ae_smoke_grenade.stock.detonation_started = definition->projectile.detonation_started.index;
	ae_smoke_grenade.stock.model = definition->object.model.index;
	ae_smoke_grenade.stock.attachments = definition->object.attachments.count;
	ae_smoke_grenade.stock.danger_radius = definition->projectile.danger_radius;
	for (index = 0; index < definition->projectile.material_responses.count && index < AE_SMOKE_GRENADE_MATERIALS; index++)
		ae_smoke_grenade.stock_material_effects[index] = TAG_BLOCK_GET_ELEMENT(&definition->projectile.material_responses,
			index, struct projectile_material_response_definition)->detonation_effect.index;
}

/* every material hit's detonation effect: none (NONE) or the map's own again */
static void ae_smoke_grenade_materials(boolean none)
{
	struct projectile_definition *definition = ae_smoke_grenade_definition();
	long index;

	for (index = 0; index < definition->projectile.material_responses.count && index < AE_SMOKE_GRENADE_MATERIALS; index++)
		TAG_BLOCK_GET_ELEMENT(&definition->projectile.material_responses, index,
			struct projectile_material_response_definition)->detonation_effect.index =
			none ? NONE : ae_smoke_grenade.stock_material_effects[index];
}

static void ae_smoke_grenade_put(struct ae_smoke_grenade_numbers const *numbers)
{
	struct projectile_definition *definition = ae_smoke_grenade_definition();

	definition->projectile.effect.index = numbers->effect;
	definition->projectile.attached_detonation_damage.index = numbers->attached_damage;
	definition->projectile.impact_damage.index = numbers->impact_damage;
	definition->projectile.detonation_started.index = numbers->detonation_started;
	definition->object.model.index = numbers->model;
	definition->object.attachments.count = numbers->attachments;
	definition->projectile.danger_radius = numbers->danger_radius;
}

boolean ae_smoke_grenade_is_smoke(long projectile_definition_index)
{
	return ae_smoke_grenade.tuned && projectile_definition_index == ae_smoke_grenade.projectile_index;
}

/* debug.smoke_test_drop: the grenade dropped in front of the first local player once, for a picture of the cloud */
static void ae_smoke_grenade_test_drop(void)
{
	static long dropped_game = -1;
	long seconds = config_integer("debug.smoke_test_drop");
	long player_index, unit_index, projectile_index;
	struct object_placement_data placement;
	real_point3d eye;
	real_vector3d aim;

	if (seconds <= 0 || game_time_get() < seconds * TICKS_PER_SECOND || game_time_get() > seconds * TICKS_PER_SECOND + 60)
		return;
	if (dropped_game == (long)seconds)
		return;
	player_index = local_player_get_player_index(0);
	if (player_index == NONE || (unit_index = player_get(player_index)->unit_index) == NONE)
		return;
	dropped_game = seconds;
	unit_get_camera_position(unit_index, &eye);
	unit_get_aiming_vector(unit_index, &aim);
	object_placement_data_new(&placement, ae_smoke_grenade.projectile_index, NONE);
	placement.position.x = eye.x + aim.i * 7.0f;
	placement.position.y = eye.y + aim.j * 7.0f;
	placement.position.z = eye.z + aim.k * 7.0f;
	placement.forward = *global_down3d;
	placement.up = *global_forward3d;
	projectile_index = object_new(&placement);
	/* (it goes off at once: a grenade set down at rest would wait for a bounce) */
	if (projectile_index != NONE)
	{
		/* (the datum's counting-down bit, 5: projectiles.c's) */
		SET_FLAG(projectile_get(projectile_index)->projectile.flags, 5, TRUE);
		projectile_get(projectile_index)->projectile.detonation_timer = 1.0f;
	}
	error(_error_silent, "ae smoke: test drop at %.2f %.2f %.2f (projectile %ld)", (double)placement.position.x,
		(double)placement.position.y, (double)placement.position.z, projectile_index);
}

void ae_smoke_grenade_update(void)
{
	struct game_variant const *variant;
	long cloud_index = NONE;
	boolean want;

	if (ae_smoke_grenade.projectile_index == NONE)
		return;
	variant = game_engine_running() ? game_engine_get_variant() : NULL;
	want = ae_smoke_asked(variant) && ae_weapon_pack_smoke_pack_added();
	if (want)
	{
		cloud_index = tag_loaded(AE_SMOKE_EFFECT_GROUP, AE_SMOKE_EFFECT_NAME);
		want = cloud_index != NONE;
	}
	if (want == ae_smoke_grenade.tuned)
	{
		if (want)
			ae_smoke_grenade_test_drop();
		return;
	}
	if (want)
	{
		struct ae_smoke_grenade_numbers tuned = ae_smoke_grenade.stock;

		tuned.effect = cloud_index;
		tuned.attached_damage = NONE;
		tuned.impact_damage = NONE;
		tuned.detonation_started = NONE;
		tuned.model = projectile_definition_get(ae_smoke_grenade.frag_projectile_index)->object.model.index;
		tuned.attachments = 0;
		tuned.danger_radius = 0.0f;
		ae_smoke_grenade_put(&tuned);
		ae_smoke_grenade_materials(TRUE);
		ae_smoke_grenade.tuned = TRUE;
		error(_error_silent, "ae smoke: the plasma grenade is the smoke grenade (cloud tag %ld, volume radius %.2f, holds "
			"%ld ticks, gone at %ld)", cloud_index, (double)AE_SMOKE_RADIUS, (long)AE_SMOKE_HOLD_TICKS,
			(long)AE_SMOKE_LIFE_TICKS);
	}
	else
	{
		ae_smoke_grenade_put(&ae_smoke_grenade.stock);
		ae_smoke_grenade_materials(FALSE);
		ae_smoke_grenade.tuned = FALSE;
		error(_error_silent, "ae smoke: the plasma grenade is the map's own again");
	}
}
