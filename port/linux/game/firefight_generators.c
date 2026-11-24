/*
FIREFIGHT_GENERATORS.C

port: Generator Defense Sub-Mode (Alpha, Beta, Gamma Generators):
- Spawns 1-3 targetable player-team Generator units at strategic map objective points
- Tracks generator health (body vitality), rechargeable shields, and 30s Lockdown blast shields
- Attracts AI enemy aggro and checks Defend Any vs. Defend All victory/defeat conditions
*/

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "cseries/cseries.h"
#include "cseries/errors.h"
#include "ai/actors.h"
#include "ai/props.h"
#include "cache/cache_files.h"
#include "game/game.h"
#include "game/game_engine.h"
#include "game/players.h"
#include "math/real_math.h"
#include "memory/data.h"
#include "objects/damage.h"
#include "objects/objects.h"
#include "physics/collisions.h"
#include "scenario/scenario.h"
#include "scenario/scenario_definitions.h"
#include "tag_files/tag_groups.h"
#include "units/biped_definitions.h"
#include "units/units.h"
#include "units/vehicle_definitions.h"

#include "halo_firefight.h"

/* ---------- constants */

enum
{
	GENERATOR_LOCKDOWN_DURATION_TICKS = 30 * TICKS_PER_SECOND,
	GENERATOR_LOCKDOWN_COOLDOWN_TICKS = 60 * TICKS_PER_SECOND,
	GENERATOR_SHIELD_STUN_TICKS = 6 * TICKS_PER_SECOND,
	/* netgame flags: Forge's Firefight markers, and its generator objective
	(forge_engine.h's FORGE_NETGAME_FLAG_FIREFIGHT_*) */
	GENERATOR_NETGAME_FLAG_FIREFIGHT_SPAWN = 15,
	GENERATOR_NETGAME_FLAG_OBJECTIVE = 16
};

extern struct game_variant global_variant;

static long g_generator_scenery_indices[FIREFIGHT_MAX_GENERATORS] = { NONE, NONE, NONE };
/* (each one's facing: an objective's, as Forge shows it, else none's) */
static real g_generator_facings[FIREFIGHT_MAX_GENERATORS];

/* ---------- helpers */

static long find_generator_scenery_tag(void)
{
	static char const *const candidate_scenery[] = {
		"scenery\\c_uplink\\c_uplink",
		"scenery\\c_field_generator\\c_field_generator",
		"scenery\\c_beacon\\c_beacon",
		"scenery\\c_storage\\c_storage",
		"scenery\\flag_base\\flag_base"
	};
	short i;

	for (i = 0; i < (short)NUMBEROF(candidate_scenery); i++)
	{
		long idx = tag_loaded('scen', candidate_scenery[i]);
		if (idx != NONE)
			return idx;
	}
	{
		static char const *const fallback_turrets[] = {
			"vehicles\\c gun turret\\c gun turret_mp",
			"vehicles\\c_gun_turret\\c_gun_turret_mp",
			"vehicles\\c gun turret\\c gun turret",
			"vehicles\\c_gun_turret\\c_gun_turret"
		};
		for (i = 0; i < (short)NUMBEROF(fallback_turrets); i++)
		{
			long idx = tag_loaded(VEHICLE_DEFINITION_TAG, fallback_turrets[i]);
			if (idx != NONE)
				return idx;
		}
	}
	return NONE;
}

static long find_generator_unit_tag(void)
{
	static char const *const candidate_bipeds[] = {
		"characters\\cyborg_mp\\cyborg_mp",
		"characters\\cyborg\\cyborg",
		"characters\\marine_armored\\marine_armored",
		"characters\\marine\\marine",
		"characters\\sentinel\\sentinel",
		"characters\\elite\\elite"
	};
	static char const *const candidate_vehicles[] = {
		"vehicles\\c gun turret\\c gun turret_mp",
		"vehicles\\c_gun_turret\\c_gun_turret_mp",
		"vehicles\\c gun turret\\c gun turret",
		"vehicles\\c_gun_turret\\c_gun_turret",
		"vehicles\\ghost\\ghost_mp",
		"vehicles\\warthog\\mp_warthog"
	};
	struct tag_iterator iter;
	long tag_idx;
	short i;

	/* Prioritize biped tags first because Halo CE AI target props (prop_datum) only exist for _object_type_biped! */
	for (i = 0; i < (short)NUMBEROF(candidate_bipeds); i++)
	{
		long idx = tag_loaded(BIPED_DEFINITION_TAG, candidate_bipeds[i]);
		if (idx != NONE)
			return idx;
	}
	tag_iterator_new(&iter, BIPED_DEFINITION_TAG);
	tag_idx = tag_iterator_next(&iter);
	if (tag_idx != NONE)
		return tag_idx;

	for (i = 0; i < (short)NUMBEROF(candidate_vehicles); i++)
	{
		long idx = tag_loaded(VEHICLE_DEFINITION_TAG, candidate_vehicles[i]);
		if (idx != NONE)
			return idx;
	}
	return NONE;
}

static void snap_generator_to_ground(real_point3d *pt)
{
	struct collision_result coll;
	real_point3d ray_start = *pt;
	real_vector3d ray_down = { 0.0f, 0.0f, -6.0f };

	ray_start.z += 1.0f;
	if (collision_test_vector(FLAG(_collision_test_structure_bit), &ray_start, &ray_down, NONE, &coll))
	{
		pt->z = coll.point.z + 0.05f;
	}
}

static void spawn_generator_objects_for_slot(short slot)
{
	struct firefight_generator_state *gen = &ff_state.generators[slot];
	long scen_tag = find_generator_scenery_tag();
	long gen_tag = find_generator_unit_tag();

	if (scen_tag != NONE && g_generator_scenery_indices[slot] == NONE)
	{
		struct object_placement_data scen_placement;
		long scen_obj_index;

		object_placement_data_new(&scen_placement, scen_tag, NONE);
		scen_placement.position = gen->position;
		vector3d_from_angle(&scen_placement.forward, g_generator_facings[slot]);
		scen_placement.owner_team_index = _game_team_player;
		scen_obj_index = object_new(&scen_placement);
		if (scen_obj_index != NONE)
		{
			struct object_datum *sobj = object_get(scen_obj_index);
			sobj->object.owner_team_index = _game_team_player;
			SET_FLAG(sobj->object.flags, _object_cannot_be_garbage_bit, TRUE);
			/* Allow enemy projectiles, melee, and AOE damage to pass through the outer scenery
			   visual and hit the solid target unit inside the generator */
			SET_FLAG(sobj->object.flags, _object_no_collisions_bit, TRUE);
			object_set_automatic_deactivation(scen_obj_index, FALSE);
			g_generator_scenery_indices[slot] = scen_obj_index;
		}
	}

	if (gen_tag != NONE && gen->object_index == NONE)
	{
		struct object_placement_data placement;
		long obj_index;

		object_placement_data_new(&placement, gen_tag, NONE);
		placement.position = gen->position;
		placement.owner_team_index = _game_team_player;

		obj_index = object_new(&placement);
		if (obj_index != NONE)
		{
			struct object_datum *obj = object_get(obj_index);
			struct unit_datum *u = (struct unit_datum *)object_try_and_get_and_verify_type(obj_index, _object_mask_unit);
			obj->object.owner_team_index = _game_team_player;
			obj->object.body_vitality = 1.0f;
			obj->object.shield_vitality = 1.0f;
			SET_FLAG(obj->object.flags, _object_cannot_be_garbage_bit, TRUE);
			/* Keep _object_invisible_bit FALSE so Halo's projectile raycasts, melee, and AOE
			   explosions collide with and damage the generator target unit; its mesh is hidden
			   in render_model() via firefight_is_generator_hidden_biped() */
			SET_FLAG(obj->object.flags, _object_invisible_bit, FALSE);
			if (u)
			{
				SET_FLAG(u->unit.flags, _unit_preferred_target_bit, FALSE);
				/* Crouch the hidden target biped at ground level so its center-of-mass ("body")
				   and head aim markers sit inside the lower-middle body of the Covenant c_uplink
				   instead of up near the HUD waypoint */
				SET_FLAG(u->unit.control_flags, _unit_control_crouch_modifier_bit, TRUE);
				unit_delete_all_weapons(obj_index);
			}
			object_set_automatic_deactivation(obj_index, FALSE);
			gen->object_index = obj_index;
		}
	}
}

static void select_generator_locations(short count, real_point3d *out_points)
{
	struct scenario *scen = global_scenario_get();
	short found = 0;
	short i;

	csmemset(g_generator_facings, 0, sizeof(g_generator_facings));
	if (scen)
	{
		/* those placed for it in Forge first, where they were placed; the
		map's other places make up the number */
		for (i = 0; i < scen->netgame_flags.count && found < count; i++)
		{
			struct scenario_netgame_flag *flag = TAG_BLOCK_GET_ELEMENT(
				&scen->netgame_flags, i, struct scenario_netgame_flag);

			if (flag && flag->type == GENERATOR_NETGAME_FLAG_OBJECTIVE)
			{
				g_generator_facings[found] = flag->facing;
				out_points[found++] = flag->position;
			}
		}

		for (i = 0; i < scen->netgame_flags.count && found < count; i++)
		{
			struct scenario_netgame_flag *flag = TAG_BLOCK_GET_ELEMENT(
				&scen->netgame_flags, i, struct scenario_netgame_flag);
			short j;
			boolean too_close = FALSE;

			if (!flag || flag->type == GENERATOR_NETGAME_FLAG_OBJECTIVE ||
				flag->type == GENERATOR_NETGAME_FLAG_FIREFIGHT_SPAWN)
				continue;

			for (j = 0; j < found; j++)
			{
				if (distance3d(&out_points[j], &flag->position) < 6.0f)
				{
					too_close = TRUE;
					break;
				}
			}
			if (!too_close)
				out_points[found++] = flag->position;
		}

		for (i = 0; i < scen->players.count && found < count; i++)
		{
			struct player_starting_location *loc = TAG_BLOCK_GET_ELEMENT(
				&scen->players, i, struct player_starting_location);
			short j;
			boolean too_close = FALSE;

			if (!loc)
				continue;

			for (j = 0; j < found; j++)
			{
				if (distance3d(&out_points[j], &loc->position) < 6.0f)
				{
					too_close = TRUE;
					break;
				}
			}
			if (!too_close)
				out_points[found++] = loc->position;
		}
	}

	while (found < count)
	{
		real_point3d fallback = { 0.0f, 0.0f, 0.5f };
		if (found > 0)
		{
			fallback = out_points[0];
			fallback.x += (real)found * 6.0f;
			fallback.y += (real)found * 4.0f;
		}
		out_points[found++] = fallback;
	}
}

/* ---------- public code */

/* (firefight_objectives.c's enemy generator: the same looks and target) */
long firefight_generator_scenery_tag(void)
{
	return find_generator_scenery_tag();
}

long firefight_generator_unit_tag(void)
{
	return find_generator_unit_tag();
}

void firefight_generators_initialize(
	short count,
	short max_health)
{
	real_point3d positions[FIREFIGHT_MAX_GENERATORS];
	short i;

	count = (short)PIN(count, 0, FIREFIGHT_MAX_GENERATORS);
	max_health = (short)MAX(max_health, 500);
	ff_state.generator_count = count;
	ff_state.generators_alive = count;

	for (i = 0; i < FIREFIGHT_MAX_GENERATORS; i++)
	{
		csmemset(&ff_state.generators[i], 0, sizeof(ff_state.generators[i]));
		ff_state.generators[i].object_index = NONE;
		g_generator_scenery_indices[i] = NONE;
	}

	if (count <= 0)
		return;

	select_generator_locations(count, positions);

	for (i = 0; i < count; i++)
	{
		struct firefight_generator_state *gen = &ff_state.generators[i];

		gen->position = positions[i];
		snap_generator_to_ground(&gen->position);
		gen->max_health = max_health;
		gen->health = max_health;
		gen->max_shield = (short)(max_health / 2);
		gen->shield = gen->max_shield;
		gen->active = TRUE;
		gen->alive = TRUE;
		gen->object_index = NONE;
		g_generator_scenery_indices[i] = NONE;

		spawn_generator_objects_for_slot(i);
	}
}

boolean firefight_is_generator_object(
	long object_index,
	short *generator_index_out)
{
	short i;

	if (object_index == NONE || ff_state.generator_count <= 0)
		return FALSE;

	for (i = 0; i < ff_state.generator_count; i++)
	{
		if (ff_state.generators[i].object_index == object_index ||
			g_generator_scenery_indices[i] == object_index)
		{
			if (generator_index_out)
				*generator_index_out = i;
			return TRUE;
		}
	}
	return FALSE;
}

boolean firefight_is_generator_hidden_biped(
	long object_index)
{
	short i;

	if (object_index == NONE || !firefight_active())
		return FALSE;
	/* (and the enemy's, a set objective's: firefight_objectives.c) */
	if (firefight_objectives_generator_hidden(object_index))
		return TRUE;
	if (ff_state.generator_count <= 0)
		return FALSE;

	for (i = 0; i < ff_state.generator_count; i++)
	{
		if (ff_state.generators[i].object_index == object_index &&
			g_generator_scenery_indices[i] != NONE)
		{
			return TRUE;
		}
	}
	return FALSE;
}

boolean firefight_apply_generator_damage(
	long object_index,
	long attacker_player_index,
	short attacker_team_index,
	real damage_amount)
{
	short slot = NONE;
	struct firefight_generator_state *gen;
	short dmg_points;

	if (!firefight_is_generator_object(object_index, &slot))
		return FALSE;

	/* Players and friendly AI cannot damage friendly generators */
	if (attacker_player_index != NONE ||
		attacker_team_index == _game_team_player ||
		(!ff_state.elite_player_mode && attacker_team_index == _game_team_human))
	{
		return TRUE;
	}

	gen = &ff_state.generators[slot];
	if (!gen->alive || gen->lockdown_ticks > 0)
		return TRUE;

	dmg_points = (short)MAX(1, (long)(damage_amount * 0.45f));
	if (gen->under_attack_ticks <= 0)
		firefight_announce_banner(_ff_banner_generator_under_attack, slot, 3 * TICKS_PER_SECOND);
	gen->under_attack_ticks = GENERATOR_SHIELD_STUN_TICKS;

	if (gen->shield > 0)
	{
		if (dmg_points <= gen->shield)
		{
			gen->shield = (short)(gen->shield - dmg_points);
			dmg_points = 0;
		}
		else
		{
			dmg_points = (short)(dmg_points - gen->shield);
			gen->shield = 0;
		}
	}

	if (dmg_points > 0)
	{
		gen->health = (short)MAX(0, gen->health - dmg_points);
	}

	if (gen->health <= 0)
	{
		byte fail_mode = global_variant.game_engine_variant.firefight.generator_fail_mode;

		gen->health = 0;
		gen->alive = FALSE;
		if (ff_state.generators_alive > 0)
			ff_state.generators_alive--;
		firefight_announce_banner(_ff_banner_generator_destroyed, slot, 4 * TICKS_PER_SECOND);

		if (ff_state.generators_alive <= 0 ||
			(fail_mode == _firefight_generator_fail_any && ff_state.generators_alive < ff_state.generator_count))
		{
			ff_state.match_lost = TRUE;
			ff_state.phase_state = _ff_phase_defeat;
			ff_state.phase_timer_ticks = 5 * TICKS_PER_SECOND;
			firefight_announce_banner(_ff_banner_defeat, 0, 5 * TICKS_PER_SECOND);
			if (global_scenario_index != NONE)
				firefight_stats_record_match_end(tag_get_name(global_scenario_index));
		}
	}

	return TRUE;
}

void firefight_try_trigger_generator_lockdown(
	long player_index)
{
	struct player_datum *player;
	real_point3d player_pos;
	short i;

	if (ff_state.generator_count <= 0 || player_index == NONE)
		return;

	player = player_try_and_get(player_index);
	if (!player || player->unit_index == NONE)
		return;

	object_get_origin(player->unit_index, &player_pos);

	for (i = 0; i < ff_state.generator_count; i++)
	{
		struct firefight_generator_state *gen = &ff_state.generators[i];

		if (!gen->alive || gen->lockdown_ticks > 0 || gen->cooldown_ticks > 0)
			continue;

		if (distance3d(&player_pos, &gen->position) <= 3.5f)
		{
			gen->lockdown_ticks = GENERATOR_LOCKDOWN_DURATION_TICKS;
			gen->cooldown_ticks = GENERATOR_LOCKDOWN_COOLDOWN_TICKS;
			firefight_announce_banner(_ff_banner_generator_locked_down, i, 3 * TICKS_PER_SECOND);
			return;
		}
	}
}

void firefight_generators_update(void)
{
	short i;

	if (ff_state.generator_count <= 0)
		return;

	for (i = 0; i < ff_state.generator_count; i++)
	{
		struct firefight_generator_state *gen = &ff_state.generators[i];
		struct object_datum *obj = (gen->object_index != NONE) ?
			object_try_and_get_and_verify_type(gen->object_index, _object_mask_unit) : NULL;

		if (!gen->alive)
		{
			if (gen->object_index != NONE)
			{
				if (object_try_and_get(gen->object_index))
					object_delete(gen->object_index);
				gen->object_index = NONE;
			}
			if (g_generator_scenery_indices[i] != NONE)
			{
				if (object_try_and_get(g_generator_scenery_indices[i]))
					object_delete(g_generator_scenery_indices[i]);
				g_generator_scenery_indices[i] = NONE;
			}
			continue;
		}

		if (!obj)
		{
			gen->object_index = NONE;
			spawn_generator_objects_for_slot(i);
			obj = (gen->object_index != NONE) ?
				object_try_and_get_and_verify_type(gen->object_index, _object_mask_unit) : NULL;
		}
		if (g_generator_scenery_indices[i] != NONE && !object_try_and_get(g_generator_scenery_indices[i]))
		{
			g_generator_scenery_indices[i] = NONE;
			spawn_generator_objects_for_slot(i);
		}

		if (gen->lockdown_ticks > 0)
		{
			gen->lockdown_ticks--;
		}
		else if (gen->cooldown_ticks > 0)
		{
			gen->cooldown_ticks--;
		}

		if (gen->under_attack_ticks > 0)
		{
			gen->under_attack_ticks--;
		}
		else if (gen->shield < gen->max_shield && (game_time_get() % 3) == 0)
		{
			gen->shield = (short)MIN(gen->max_shield, gen->shield + 4);
		}

		/* Allow nearby players holding Action button to trigger Generator Lockdown */
		if (player_data && gen->lockdown_ticks == 0 && gen->cooldown_ticks == 0)
		{
			struct data_iterator iter;
			struct player_datum *player;

			data_iterator_new(&iter, player_data);
			while ((player = data_iterator_next(&iter)) != NULL)
			{
				if (player->unit_index != NONE)
				{
					struct unit_datum *u = (struct unit_datum *)object_try_and_get_and_verify_type(
						player->unit_index, _object_mask_unit);
					if (u && TEST_FLAG(u->unit.control_flags, _unit_control_action_bit))
					{
						firefight_try_trigger_generator_lockdown(iter.datum_index);
					}
				}
			}
		}

		if (obj)
		{
			struct unit_datum *gu = (struct unit_datum *)obj;
			obj->object.position = gen->position;
			obj->object.translational_velocity.i = 0.0f;
			obj->object.translational_velocity.j = 0.0f;
			obj->object.translational_velocity.k = 0.0f;
			SET_FLAG(obj->object.damage_flags, _object_dead_bit, FALSE);
			SET_FLAG(gu->unit.control_flags, _unit_control_crouch_modifier_bit, TRUE);
			obj->object.body_vitality = (real)MAX(gen->health, 1) / (real)MAX(gen->max_health, 1);
			obj->object.shield_vitality = (real)gen->shield / (real)MAX(gen->max_shield, 1);
		}

		/* Ensure enemies that reach close range of an unlocked generator and are not engaged
		   in close-quarters combat with a player deal consistent siege damage every second */
		if (gen->alive && gen->lockdown_ticks == 0 && gen->object_index != NONE &&
			(game_time_get() % TICKS_PER_SECOND) == 0)
		{
			short e;
			short attackers = 0;

			for (e = 0; e < FIREFIGHT_MAX_TRACKED_ENEMIES && attackers < 3; e++)
			{
				long e_idx = ff_state.tracked_enemy_units[e];
				struct unit_datum *eu;
				real_point3d e_pos;

				if (e_idx == NONE)
					continue;
				eu = (struct unit_datum *)object_try_and_get_and_verify_type(e_idx, _object_mask_unit);
				if (!eu || TEST_FLAG(eu->object.damage_flags, _object_dead_bit))
					continue;

				object_get_origin(e_idx, &e_pos);
				if (distance_squared3d(&e_pos, &gen->position) <= 6.5f * 6.5f)
				{
					boolean player_close = FALSE;
					if (player_data)
					{
						struct data_iterator p_iter;
						struct player_datum *pl;
						data_iterator_new(&p_iter, player_data);
						while ((pl = data_iterator_next(&p_iter)) != NULL)
						{
							if (pl->unit_index != NONE)
							{
								real_point3d p_pos;
								object_get_origin(pl->unit_index, &p_pos);
								if (distance_squared3d(&e_pos, &p_pos) <= 9.0f * 9.0f)
								{
									player_close = TRUE;
									break;
								}
							}
						}
					}
					if (!player_close)
						attackers++;
				}
			}

			if (attackers > 0)
			{
				firefight_apply_generator_damage(
					gen->object_index,
					NONE,
					_game_team_covenant,
					(real)attackers * 22.0f);
			}
		}
	}
}
