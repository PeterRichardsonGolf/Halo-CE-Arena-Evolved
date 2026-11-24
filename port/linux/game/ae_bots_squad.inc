/*
AE_BOTS_SQUAD.INC

Included by ae_bots_lan.inc, so at the end of bots.c: the brain of the campaign AI squad (ae_companion.c). A squad member is a
bot machine's co-op player of a campaign game, no game engine running: its controls come from the host's update (ae_companion_action
-> bots_squad_drive) as a LAN bot's do, from this brain.

What is the multiplayer bots' own: the fight (aim by the level, bursts, strafes, a grenade, a blow: bots_bot_fight and the fight by
level's glue), the path search and the walk (bots_follow_path), the unstick's idea and the weapon swap.
What is the squad's: the enemies are the level's AI actors (ae_companion_foes), no players. A member's target is then an enemy
unit held in bot->foe_unit_index, and the fight's code reads the stand-in player (bot->foe_player) bots_target_player gives it. The
squad is one fireteam led by the human player (ae_fireteam.c's campaign squad): its follow, slots, leash and orders are a big team
game's person-led squad's (ae_bots_fireteam.inc), the marks and the comm wheel its calls. On a deck (ae_companion.h) it walks the
player's track in single file instead (ae_bots_squad_rules.h). The other layers of a multiplayer game (objective, roles, power
weapons, vehicles' driving, flanks) do not run; a member in a vehicle's seat aims and fires and does not move.
*/

#include "ae_bots_squad_rules.h"
#include "ae_companion.h"
#include "cutscene/cinematics.h"

#define AE_SQUAD_CANDIDATES 4

static struct
{
	long tick;
	boolean tick_valid;
	struct
	{
		long tick;
		boolean valid;
		boolean moving;
		/* on an object's ground (a deck): logged as it changes; held until deck_until after the deck was last seen */
		boolean deck;
		long deck_until;
		/* on the track: the last check's tick and place, and whether it walked every tick since (ae_bots_squad_track_jump) */
		long track_check_tick;
		real_point3d track_check_at;
		boolean track_walked;
		/* the last tick THE EDGE held it (a stop there is not a stop at something to jump over) */
		long edge_tick;
		/* DRY: the weapon on the ground it goes for (NONE: none), since when, the next look for one, since when none is in reach
		(0: one is, or it is not dry), and the fallback's last step */
		long pickup_item;
		long pickup_tick;
		long pickup_check_tick;
		long no_pickup_since;
		long refill_tick;
		/* it walked with the player this tick (ae_squad_walk) */
		boolean walked;
		struct player_action action;
	} members[AE_COMPANION_MOST];
	/* halo.log's counts */
	long targets_found;
	long fight_ticks;
	long trigger_ticks;
	long gunner_trigger_ticks;
	long grenades;
	long blows;
	long follow_ticks;
	long unsticks;
	long sidesteps;
	long edges_held;
	long walk_fight_ticks;
	long jumps_held;
	long track_jumps;
	long empty_ticks;
	long refills;
	long pickups_gone_for;
} ae_squad;

/* an enemy the player called out (a mark on it, ENEMY, ATTACK on it): fought first until focus_until */
static struct
{
	long unit_index;
	long until;
	long calls;
	long focuses;
} ae_squad_focus = { NONE, 0, 0, 0 };

/* the setting's level for the squad (game.companions_skill) */
static short ae_squad_skill(void)
{
	return bots_skill_named(config_string("game.companions_skill"));
}

/* once a game tick, before the first member: the walkable surfaces (again on a new BSP of the level), the skill, the searches */
static void ae_squad_tick_begin(void)
{
	long now = game_time_get();
	struct structure_bsp *structure;

	if (ae_squad.tick_valid && now == ae_squad.tick)
		return;
	if (!ae_squad.tick_valid || now < ae_squad.tick)
		csmemset(ae_squad.members, 0, sizeof(ae_squad.members));
	ae_squad.tick = now;
	ae_squad.tick_valid = TRUE;
	structure = global_structure_bsp_get();
	if (!bots_globals.map_ready || (bots_paths.source && bots_paths.source != structure))
	{
		bots_paths_release();
		bots_globals.map_ready = TRUE;
		bots_globals.roam_point_count = 0;
		bots_prepare_paths();
		platform_log("companion: the squad's walkable surfaces are prepared");
	}
	bots_globals.skill = ae_squad_skill();
	bots_globals.tick_searches = 0;
	bots_paths_scan();
	/* (the squad's one fireteam, its roster and its standing order) */
	ae_fireteam_campaign_tick();
	bots_globals.ticks++;
}

/* the enemy the member sees best, NONE for none: of the level's AI enemies near the player and in its sight's reach and view, the
one of the least score a line of sight reaches (the nearest few are tried) */
static long ae_squad_find_foe(struct bots_bot *bot, struct bots_tick *tick, boolean have_leader, real_point3d const *leader_at)
{
	struct bots_skill_definition const *skill = ae_bots_skill_definition(ae_bots_shooting_skill(bots_globals.skill));
	real_point3d const *eye = bots_tick_eye(tick);
	boolean hurt = tick->now - bot->hurt_tick < TICKS_PER_SECOND * 2;
	real half_view = 0.5f * skill->field_of_view_degrees * BOTS_DEGREES;
	long foes[256], count = ae_companion_foes(foes, 256), index, kept = 0, pick;
	struct
	{
		long unit;
		real score;
		real_point3d point;
	} best[AE_SQUAD_CANDIDATES];

	for (index = 0; index < count; index++)
	{
		struct object_datum *object = (struct object_datum *)object_try_and_get_and_verify_type(foes[index], _object_mask_unit);
		real_point3d point;
		real distance, bearing, reach, score;
		boolean current, focused;
		long place;

		if (!object || TEST_FLAG(object->object.damage_flags, _object_dead_bit))
			continue;
		point = object->object.bounding_sphere_center;
		focused = foes[index] == ae_squad_focus.unit_index && tick->now < ae_squad_focus.until;
		if (have_leader && !focused && !ae_bots_squad_foe_in_reach(bots_distance3d(&point, leader_at)))
			continue;
		current = (boolean)(bot->foe_unit_index == foes[index] && bot->target_player_index == BOTS_FOE_TOKEN);
		distance = bots_distance3d(eye, &point);
		reach = skill->sight_distance * (current ? 1.25f : 1.0f);
		if (TEST_FLAG(unit_get(foes[index])->unit.flags, _unit_active_camouflaged_bit))
			reach *= 0.3f;
		if (distance > reach)
			continue;
		bearing = (real)fabs(bots_angle_difference((real)atan2(point.y - eye->y, point.x - eye->x), bot->yaw));
		/* (out of view: unless close by, it shot the member, it is the one fought or the player called it out) */
		if (bearing > half_view && distance > 8.0f && !hurt && !current && !focused)
			continue;
		score = ae_bots_squad_foe_score(distance, bearing, current) - (focused ? AE_SQUAD_FOCUS_BONUS : 0.0f);
		for (place = kept; place > 0 && best[place - 1].score > score; place--)
		{
			if (place < AE_SQUAD_CANDIDATES)
				best[place] = best[place - 1];
		}
		if (place < AE_SQUAD_CANDIDATES)
		{
			best[place].unit = foes[index];
			best[place].score = score;
			best[place].point = point;
			if (kept < AE_SQUAD_CANDIDATES)
				kept++;
		}
	}
	for (pick = 0; pick < kept; pick++)
	{
		if (bots_line_of_sight(tick->player->unit_index, eye, &best[pick].point, best[pick].unit))
			return best[pick].unit;
	}
	return NONE;
}

/* every third tick the member looks for an enemy; one it fights is dropped when it is dead or unseen for ten seconds */
static void ae_squad_look(struct bots_bot *bot, struct bots_tick *tick, boolean have_leader, real_point3d const *leader_at)
{
	if (tick->now >= bot->next_look_tick)
	{
		long foe = ae_squad_find_foe(bot, tick, have_leader, leader_at);

		bot->next_look_tick = tick->now + AE_SQUAD_LOOK_TICKS;
		if (foe != NONE)
		{
			struct object_datum *object = object_get(foe);
			real_point3d ground;
			long surface_index;

			/* (a new target: the aim's error as it first turns to it) */
			if (bot->target_player_index != BOTS_FOE_TOKEN || bot->foe_unit_index != foe ||
				tick->now - bot->target_seen_tick > TICKS_PER_SECOND * 2)
			{
				real error = tick->skill->aim_error_degrees * BOTS_DEGREES;

				bot->target_first_seen_tick = tick->now;
				bot->aim_error_yaw = bots_random_signed(bot) * error;
				bot->aim_error_pitch = bots_random_signed(bot) * error * 0.6f;
				ae_squad.targets_found++;
			}
			bot->target_player_index = BOTS_FOE_TOKEN;
			bot->foe_unit_index = foe;
			csmemset(&bot->foe_player, 0, sizeof(bot->foe_player));
			bot->foe_player.unit_index = foe;
			bot->foe_player.local_player_index = NONE;
			bot->target_seen_tick = tick->now;
			bot->target_last_position = object->object.position;
			surface_index = bots_unit_surface(foe, &ground);
			if (surface_index != NONE)
				bot->target_last_surface_index = surface_index;
		}
	}
	if (bot->target_player_index != NONE)
	{
		struct object_datum *object = bot->foe_unit_index != NONE ?
			(struct object_datum *)object_try_and_get_and_verify_type(bot->foe_unit_index, _object_mask_unit) : NULL;

		if (bot->target_player_index != BOTS_FOE_TOKEN || !object || TEST_FLAG(object->object.damage_flags, _object_dead_bit) ||
			tick->now - bot->target_seen_tick > TICKS_PER_SECOND * 10)
		{
			bot->target_player_index = NONE;
			bot->foe_unit_index = NONE;
		}
		else
		{
			/* (the stand-in keeps pointing at the unit: the fight reads its place now) */
			bot->foe_player.unit_index = bot->foe_unit_index;
		}
	}
}

/* the goal is the player's feet */
static void ae_squad_set_goal(struct bots_bot *bot, real_point3d const *point, long surface_index)
{
	if (bot->goal_kind == _bots_goal_enemy && bot->goal_surface_index == surface_index &&
		bots_distance3d(&bot->goal_point, point) < 1.5f)
		return;
	bot->goal_kind = _bots_goal_enemy;
	bot->goal_point = *point;
	bot->goal_surface_index = surface_index;
	bot->goal_tick = game_time_get();
	bot->goal_stuck_count = 0;
	bot->path.valid = FALSE;
	bot->path_failures = 0;
	bot->climb_until_tick = 0;
}

/* HOLD AND SIDESTEP (ae_bots_combat_rules.h): a member holding fire for a friend in its line (the player it stands behind,
another member) steps sideways, away from the friend, to clear the line; the other side if that one has no floor a step
out, neither: it holds where it stands */
static void ae_squad_sidestep(struct bots_bot *bot, struct bots_tick *tick, long slot)
{
	struct ae_bots_combat_bot *state = ae_bots_fight_state(bot, tick);
	struct player_datum *target = bots_target_player_try(bot);
	real_point3d target_at;
	real line;
	int side, attempt;

	if (!state->friendly_fire_held || !target || target->unit_index == NONE)
		return;
	target_at = object_get(target->unit_index)->object.position;
	line = (real)atan2(target_at.y - tick->position.y, target_at.x - tick->position.x);
	side = ae_bots_combat_sidestep_side(tick->position.x, tick->position.y, target_at.x, target_at.y,
		state->friendly_fire_blocker_at.x, state->friendly_fire_blocker_at.y, slot % 2 ? 1 : -1);
	for (attempt = 0; attempt < 2; attempt++, side = -side)
	{
		real direction = line + (real)side * 1.5707963f;
		real_point3d probe = tick->position, ground;

		probe.x += 1.2f * (real)cos(direction);
		probe.y += 1.2f * (real)sin(direction);
		if (bots_surface_below(&probe, &ground) == NONE || fabs(ground.z - tick->position.z) > 1.0f)
			continue;
		tick->move_direction = direction;
		tick->moving = TRUE;
		tick->throttle_scale = 1.0f;
		ae_squad.sidesteps++;
		return;
	}
}

/* not moving for a second while it walks: a jump and a sidestep, another path; the goal dropped after three */
static void ae_squad_unstick(struct bots_bot *bot, struct bots_tick *tick)
{
	long now = tick->now;

	if (now - bot->stuck_tick >= TICKS_PER_SECOND)
	{
		if (tick->moving && bots_distance2d(&tick->position, &bot->stuck_position) < 0.4f &&
			tick->object->object.parent_object_index == NONE)
		{
			ae_squad.unsticks++;
			bot->stuck_count++;
			bot->unstick_until_tick = now + TICKS_PER_SECOND / 2;
			bot->unstick_direction = tick->move_direction + (bots_random_real(bot) < 0.5f ? -1.0f : 1.0f) * BOTS_PI * 0.5f;
			bot->path.valid = FALSE;
			if (bot->stuck_count >= 3)
			{
				bot->goal_kind = _bots_goal_none;
				bot->stuck_count = 0;
			}
		}
		else
			bot->stuck_count = 0;
		bot->stuck_position = tick->position;
		bot->stuck_tick = now;
	}
	if (now < bot->unstick_until_tick)
	{
		tick->move_direction = bot->unstick_direction;
		tick->moving = TRUE;
		if (now == bot->unstick_until_tick - TICKS_PER_SECOND / 2)
			tick->flags |= FLAG(_unit_control_jump_bit);
	}
}

/* whether a living squad mate or a player stands within AE_SQUAD_TRACK_CLOSE of the member */
static boolean ae_squad_someone_close(struct bots_tick const *tick, long slot)
{
	struct data_iterator iterator;
	struct player_datum *player;

	data_iterator_new(&iterator, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
	{
		real_point3d at;

		if (player == tick->player || player->unit_index == NONE ||
			TEST_FLAG(object_get(player->unit_index)->object.damage_flags, _object_dead_bit))
			continue;
		object_get_origin(player->unit_index, &at);
		if (bots_distance3d(&at, &tick->position) < AE_SQUAD_TRACK_CLOSE)
			return TRUE;
	}
	(void)slot;
	return FALSE;
}

/* ON THE TRACK, after its walk: a member that walked the whole check and stopped where the player went on (something the
player jumped over) jumps on along the track; the edge still holds a jump with a drop ahead */
static void ae_squad_track_jump(struct bots_tick *tick, long slot)
{
	long now = tick->now;

	if (!ae_squad.members[slot].track_walked || now < ae_squad.members[slot].track_check_tick ||
		now - ae_squad.members[slot].track_check_tick > AE_SQUAD_TRACK_STUCK_TICKS * 2)
	{
		ae_squad.members[slot].track_walked = TRUE;
		ae_squad.members[slot].track_check_tick = now;
		ae_squad.members[slot].track_check_at = tick->position;
		return;
	}
	if (now - ae_squad.members[slot].track_check_tick < AE_SQUAD_TRACK_STUCK_TICKS)
		return;
	/* (held by THE EDGE in this check, or a squad mate or the player close by (single file, the one ahead stopped): no jump) */
	if (now - ae_squad.members[slot].edge_tick <= AE_SQUAD_TRACK_STUCK_TICKS || ae_squad_someone_close(tick, slot))
	{
		ae_squad.members[slot].track_check_tick = now;
		ae_squad.members[slot].track_check_at = tick->position;
		return;
	}
	if (tick->object->object.parent_object_index == NONE &&
		ae_bots_squad_track_jump(TRUE, bots_distance2d(&tick->position, &ae_squad.members[slot].track_check_at)))
	{
		tick->flags |= FLAG(_unit_control_jump_bit);
		ae_squad.track_jumps++;
	}
	ae_squad.members[slot].track_check_tick = now;
	ae_squad.members[slot].track_check_at = tick->position;
}

/* THE EDGE, the last word on where a member walks this tick (whatever moved it: the fight's strafe and jumps, the sidestep,
the fireteam's walk, the unstick, the yield): a step whose floor drops more than AE_SQUAD_EDGE_DROP (on a deck AE_SQUAD_DECK_DROP)
is not taken, the other way is when its floor holds, else it stands; where its speed carries it is probed too, and a drop
there brakes it; a jump is not made with such a drop where the jump would carry it */
static void ae_squad_edge(struct bots_tick *tick, long slot, boolean deck)
{
	real drop = deck ? AE_SQUAD_DECK_DROP : AE_SQUAD_EDGE_DROP;
	real lookahead = ae_bots_edge_lookahead(ae_bots_level_of_skill(ae_bots_movement_skill(bots_globals.skill)));
	long unit_index = tick->player->unit_index;
	real_vector3d const *velocity = &tick->object->object.translational_velocity;
	real speed = (real)sqrt(velocity->i * velocity->i + velocity->j * velocity->j) * (real)TICKS_PER_SECOND;
	real back;

	/* (the jump: along the way it walks, or its carry) */
	if (TEST_FLAG(tick->flags, _unit_control_jump_bit))
	{
		real way = tick->moving ? tick->move_direction : (real)atan2(velocity->j, velocity->i);

		if (ae_falls_way_lethal(unit_index, &tick->position, way, lookahead * 2.0f, drop) ||
			(speed > 0.5f && ae_falls_way_lethal(unit_index, &tick->position, (real)atan2(velocity->j, velocity->i), lookahead * 2.0f, drop)))
		{
			tick->flags &= ~FLAG(_unit_control_jump_bit);
			ae_squad.jumps_held++;
		}
	}
	/* (its carry: a drop where its speed takes it brakes it, walking against the carry) */
	if (speed > AE_SQUAD_CARRY_SPEED &&
		ae_falls_way_lethal(unit_index, &tick->position, (real)atan2(velocity->j, velocity->i), lookahead, drop))
	{
		back = (real)atan2(-velocity->j, -velocity->i);
		ae_squad.edges_held++;
		ae_squad.members[slot].edge_tick = tick->now;
		if (!ae_falls_way_lethal(unit_index, &tick->position, back, lookahead, drop))
		{
			tick->move_direction = back;
			tick->moving = TRUE;
			tick->throttle_scale = 1.0f;
		}
		else
			tick->moving = FALSE;
		return;
	}
	if (!tick->moving || tick->throttle_scale <= 0.0f ||
		!ae_falls_way_lethal(unit_index, &tick->position, tick->move_direction, lookahead, drop))
		return;
	ae_squad.edges_held++;
	ae_squad.members[slot].edge_tick = tick->now;
	back = tick->move_direction + BOTS_PI;
	if (!deck && !ae_falls_way_lethal(unit_index, &tick->position, back, lookahead, drop))
		tick->move_direction = back;
	else
		tick->moving = FALSE;
	if (ae_bots_events_on() && tick->now % TICKS_PER_SECOND == 0)
		platform_log("companion: %ld held at an edge at %.1f %.1f %.1f%s", slot + 1, tick->position.x, tick->position.y,
			tick->position.z, deck ? " (deck)" : "");
}

static void ae_squad_walk(struct bots_bot *bot, struct bots_tick *tick, long slot, real direction);

/* the weapon in hand empty and none other loaded (a swap finds none) */
static boolean ae_squad_dry(struct bots_tick const *tick)
{
	short each;

	if (tick->weapon_index == NONE || !bots_weapon_empty(tick->weapon_index) || tick->object->object.parent_object_index != NONE)
		return FALSE;
	for (each = 0; each < MAXIMUM_WEAPONS_PER_UNIT; each++)
	{
		long other = tick->unit->unit.weapon_object_indices[each];

		if (each != tick->unit->unit.current_weapon_index && other != NONE && !bots_weapon_empty(other))
			return FALSE;
	}
	return TRUE;
}

/* the nearest loaded weapon on the ground a dry member goes for (ae_bots_squad_pickup_wanted), NONE with none */
static long ae_squad_find_pickup(struct bots_bot *bot, struct bots_tick const *tick, real_point3d const *leader_at)
{
	struct object_iterator iterator;
	long best = NONE;
	real best_distance = REAL_MAX;

	object_iterator_new(&iterator, _object_mask_weapon, 0);
	while (object_iterator_next(&iterator))
	{
		struct item_datum *item = item_get(iterator.index);
		real distance;

		if (item->object.parent_object_index != NONE || bots_item_unreachable(bot, iterator.index) || ae_bots_item_hung(iterator.index) ||
			!TEST_FLAG(item->object.flags, _object_connected_to_map_bit) || TEST_FLAG(item->item.flags, _item_attached_to_unit_bit) ||
			bots_weapon_empty(iterator.index))
			continue;
		distance = bots_distance3d(&item->object.position, &tick->position);
		if (!ae_bots_squad_pickup_wanted(distance, bots_distance3d(&item->object.position, leader_at), tick->fighting) ||
			distance >= best_distance)
			continue;
		best = iterator.index;
		best_distance = distance;
	}
	return best;
}

/* DRY, the honest way first: a dry member walks to a loaded weapon in reach (its ammunition is taken walking over one it holds,
another is picked up with the action button, as a multiplayer bot's fetch does); TRUE while it goes for one */
static boolean ae_squad_fetch(struct bots_bot *bot, struct bots_tick *tick, long slot, real_point3d const *leader_at)
{
	struct object_datum *item;
	real direction;

	if (!ae_squad_dry(tick))
	{
		ae_squad.members[slot].pickup_item = NONE;
		ae_squad.members[slot].no_pickup_since = 0;
		return FALSE;
	}
	item = ae_squad.members[slot].pickup_item == NONE ? NULL :
		(struct object_datum *)object_try_and_get_and_verify_type(ae_squad.members[slot].pickup_item, _object_mask_weapon);
	if (item && (item->object.parent_object_index != NONE || tick->now - ae_squad.members[slot].pickup_tick > AE_SQUAD_PICKUP_GIVE_UP_TICKS))
	{
		if (item->object.parent_object_index == NONE)
			bots_item_set_unreachable(bot, ae_squad.members[slot].pickup_item);
		item = NULL;
	}
	if (!item)
	{
		long found = tick->now >= ae_squad.members[slot].pickup_check_tick ? ae_squad_find_pickup(bot, tick, leader_at) : NONE;
		real_point3d ground;
		long surface;

		if (tick->now >= ae_squad.members[slot].pickup_check_tick)
			ae_squad.members[slot].pickup_check_tick = tick->now + TICKS_PER_SECOND;
		ae_squad.members[slot].pickup_item = NONE;
		if (found == NONE || (surface = bots_surface_below(&object_get(found)->object.position, &ground)) == NONE)
		{
			if (!ae_squad.members[slot].no_pickup_since)
				ae_squad.members[slot].no_pickup_since = tick->now > 0 ? tick->now : 1;
			return FALSE;
		}
		item = object_get(found);
		ae_squad.members[slot].pickup_item = found;
		ae_squad.members[slot].pickup_tick = tick->now;
		ae_squad.members[slot].no_pickup_since = 0;
		bots_set_goal(bot, _bots_goal_item, &ground, surface);
		bot->item_index = found;
		ae_squad.pickups_gone_for++;
		{
			char const *name = tag_get_name(item->definition_index);

			platform_log("companion: %ld is dry: goes for %s %.1f units away", slot + 1, strrchr(name, '\\') ? strrchr(name, '\\') + 1 : name,
				bots_distance3d(&item->object.position, &tick->position));
		}
	}
	if (bots_distance2d(&item->object.position, &tick->position) < 0.8f && fabs(item->object.position.z - tick->position.z) < 1.2f)
	{
		tick->moving = FALSE;
		if (tick->now - bot->item_press_tick > TICKS_PER_SECOND)
			bot->item_press_tick = tick->now;
		if (tick->now - bot->item_press_tick < TICKS_PER_SECOND / 2 && tick->player->action_object_index == ae_squad.members[slot].pickup_item &&
			(tick->player->action_result == _player_action_result_pickup_weapon ||
				tick->player->action_result == _player_action_result_swap_for_weapon ||
				tick->player->action_result == _player_action_result_add_weapon_to_inventory))
			tick->flags |= FLAG(_unit_control_swap_weapons_bit);
		return TRUE;
	}
	if (!bot->path.valid && tick->now - bot->path_tick > TICKS_PER_SECOND / 3)
		bots_search(bot, tick->player->unit_index);
	if (bot->path.valid && bots_follow_path(bot, tick->player->unit_index, &tick->position, &direction))
		ae_squad_walk(bot, tick, slot, direction);
	else
		ae_squad_walk(bot, tick, slot, (real)atan2(item->object.position.y - tick->position.y, item->object.position.x - tick->position.x));
	return TRUE;
}

/* DRY, the fallback (ae_bots_squad_refill_step): no pickup in reach a while and no enemy seen a while, the weapon in hand gets a
magazine back (a battery a quarter of its charge), a step at a time */
static void ae_squad_refill_step(struct bots_tick *tick, struct bots_bot *bot, long slot)
{
	struct weapon_datum *weapon;
	struct weapon_definition *definition;

	if (!ae_squad_dry(tick) || !ae_squad.members[slot].no_pickup_since ||
		!ae_bots_squad_refill_step(tick->now - ae_squad.members[slot].no_pickup_since, tick->now - bot->target_seen_tick,
			tick->now - ae_squad.members[slot].refill_tick))
		return;
	ae_squad.members[slot].refill_tick = tick->now;
	weapon = weapon_get(tick->weapon_index);
	definition = weapon_definition_get(weapon->definition_index);
	if (definition->weapon.magazines.count <= 0)
		weapon->weapon.age = weapon->weapon.age > 0.25f ? weapon->weapon.age - 0.25f : 0.0f;
	else
	{
		struct weapon_magazine_definition *kind = TAG_BLOCK_GET_ELEMENT(&definition->weapon.magazines, 0, struct weapon_magazine_definition);
		long rounds = weapon->weapon.magazines[0].rounds_total + (kind->rounds_loaded_maximum > 0 ? kind->rounds_loaded_maximum : 1);

		weapon->weapon.magazines[0].rounds_total = (short)(kind->rounds_total_maximum > 0 && rounds > kind->rounds_total_maximum ?
			kind->rounds_total_maximum : rounds);
	}
	ae_squad.refills++;
	platform_log("companion: %ld is dry with no weapon in reach: a magazine back", slot + 1);
}

/* DRY: the weapon in hand empty, a loaded other one is taken; halo.log's squad line counts the dry ticks */
static void ae_squad_weapons(struct bots_tick *tick, struct bots_bot *bot, long slot)
{
	short each;

	if (tick->weapon_index == NONE || !bots_weapon_empty(tick->weapon_index) || tick->object->object.parent_object_index != NONE)
		return;
	for (each = 0; each < MAXIMUM_WEAPONS_PER_UNIT; each++)
	{
		long other = tick->unit->unit.weapon_object_indices[each];

		if (each != tick->unit->unit.current_weapon_index && other != NONE && !bots_weapon_empty(other))
		{
			tick->action->desired_weapon_index = each;
			return;
		}
	}
	ae_squad.empty_ticks++;
	ae_squad_refill_step(tick, bot, slot);
}

/* the player walks or runs (not standing, not seated still): the squad keeps pace with them, fighting or not */
static boolean ae_squad_leader_moving(long leader)
{
	struct object_datum const *object = (struct object_datum const *)object_try_and_get(leader);
	real_vector3d const *velocity;

	if (!object)
		return FALSE;
	velocity = &object_get(object_get_ultimate_parent(leader))->object.translational_velocity;
	return (boolean)((velocity->i * velocity->i + velocity->j * velocity->j) * (real)(TICKS_PER_SECOND * TICKS_PER_SECOND) >
		AE_SQUAD_PACE_SPEED * AE_SQUAD_PACE_SPEED);
}

/* the living member nearest a point: its player, NULL with none */
static struct player_datum *ae_squad_nearest_member(real_point3d const *point)
{
	struct player_datum *best = NULL;
	real best_distance = 0.0f;
	long slot;

	for (slot = 0; slot < AE_COMPANION_MOST; slot++)
	{
		struct bots_bot *bot = &bots_globals.bots[slot];
		struct player_datum *player = bot->player_index != NONE ? player_try_and_get(bot->player_index) : NULL;
		real distance;

		if (!player || !ae_bots_team_alive(player) || !bots_machine_is_bot(player->network_player_data.machine_index))
			continue;
		distance = bots_distance3d(&object_get(player->unit_index)->object.position, point);
		if (!best || distance < best_distance)
		{
			best = player;
			best_distance = distance;
		}
	}
	return best;
}

/* (ae_bots_marks_heard, the host) a call of the player in a campaign game, its order taken by the squad's fireteam: an enemy
marked (a tap on it, ENEMY, ATTACK on it) is fought first for AE_SQUAD_FOCUS_SECONDS; the member nearest the mark answers */
static void ae_squad_heard(struct player_datum const *marker, short mark_kind, long object_index, real_point3d const *spot,
	short wheel_entry, short order)
{
	struct player_datum *answerer = ae_squad_nearest_member(spot);
	short answer = AE_BOTS_MARK_NONE;

	ae_squad_focus.calls++;
	if (mark_kind == AE_MARK_KIND_ENEMY && object_index != NONE &&
		object_try_and_get_and_verify_type(object_index, _object_mask_unit | _object_mask_vehicle))
	{
		ae_squad_focus.unit_index = object_index;
		ae_squad_focus.until = game_time_get() + AE_SQUAD_FOCUS_SECONDS * TICKS_PER_SECOND;
		ae_squad_focus.focuses++;
		answer = AE_MARK_KIND_ENEMY;
		platform_log("companion: the squad fights the enemy called out at %.1f %.1f %.1f first", spot->x, spot->y, spot->z);
	}
	else if (order == _ae_order_follow || order == _ae_order_regroup)
		answer = AE_BOTS_MARK_RECRUITED;
	else if (order != _ae_order_none)
		answer = AE_MARK_KIND_PLACE;
	platform_log("companion: call %d kind %d: the squad's order %d", (int)wheel_entry, (int)mark_kind, (int)order);
	if (answerer && answer != AE_BOTS_MARK_NONE)
		ae_bots_mark_answer(answerer, marker, answer);
}

/* a member standing out of a fight: an enemy called out is looked towards, under WATCH the order's point */
static void ae_squad_face(struct bots_bot *bot, struct bots_tick const *tick, long slot)
{
	long fireteam = ae_fireteam_of_brain(slot);
	real_point3d toward;
	real turn = 8.0f * BOTS_DEGREES;
	struct object_datum *focus = ae_squad_focus.unit_index != NONE && tick->now < ae_squad_focus.until ?
		(struct object_datum *)object_try_and_get_and_verify_type(ae_squad_focus.unit_index, _object_mask_unit) : NULL;

	if (focus && !TEST_FLAG(focus->object.damage_flags, _object_dead_bit))
		toward = focus->object.bounding_sphere_center;
	else if (fireteam == NONE || ae_fireteam_order_of(fireteam) != _ae_order_watch || !ae_fireteam_order_point_of(fireteam, &toward))
		return;
	if (bots_distance2d(&tick->position, &toward) < 1.0f)
		return;
	bot->yaw += PIN(bots_angle_difference((real)atan2(toward.y - tick->position.y, toward.x - tick->position.x), bot->yaw), -turn, turn);
}

/* the walk with the player: out of a fight it turns to the way it walks; in one the fight's aim and trigger stay on the enemy
(it shoots on the move) */
static void ae_squad_walk(struct bots_bot *bot, struct bots_tick *tick, long slot, real direction)
{
	tick->move_direction = direction;
	tick->moving = TRUE;
	tick->throttle_scale = 1.0f;
	ae_squad.follow_ticks++;
	ae_squad.members[slot].walked = TRUE;
	if (tick->fighting)
		ae_squad.walk_fight_ticks++;
	if (!tick->fighting)
	{
		real turn = 12.0f * BOTS_DEGREES;

		bot->yaw += PIN(bots_angle_difference(direction, bot->yaw), -turn, turn);
		bot->pitch += PIN(-bot->pitch, -0.05f, 0.05f);
	}
}

/* THE SQUAD IS A FIRETEAM (ae_fireteam.c's campaign squad, ae_bots_fireteam.inc): FOLLOW and REGROUP hold the member to its slot
about the player on the tether, as a big team game's person-led squad (the leash, the slot, the hold in it); ATTACK, DEFEND, WATCH
and GOING HERE give it a goal at the order's point, walked here */
static void ae_squad_fireteam(struct bots_bot *bot, struct bots_tick *tick, long slot, boolean keep_pace)
{
	boolean goal, fighting = tick->fighting, tethered;
	real direction;

	/* (a seat call or a carried objective of an earlier game is not this member's) */
	(void)ae_bots_vehicle_state(bot, slot);
	if (ae_bots_objective_bots[slot].player_index == bot->player_index)
		ae_bots_objective_bots[slot].in_hands = FALSE;
	if (ae_fireteam_of_brain(slot) == NONE)
	{
		if (!tick->fighting)
			tick->moving = FALSE;
		return;
	}
	goal = ae_bots_fireteam_goal(bot, bot->player_index, &tick->position, tick->fighting);
	/* (KEEP PACE: the player moving on, a fighting member walks its slot as out of a fight, the fight's aim and trigger on the
	enemy: it shoots on the move instead of standing to shoot) */
	if (keep_pace)
		tick->fighting = FALSE;
	tethered = ae_bots_fireteam_tether(bot, tick);
	tick->fighting = fighting;
	if (tethered)
	{
		if (tick->moving && (!fighting || keep_pace))
			ae_squad_walk(bot, tick, slot, tick->move_direction);
		return;
	}
	if (goal && !tick->fighting && bot->goal_kind != _bots_goal_none)
	{
		if (!bot->path.valid && tick->now - bot->path_tick > TICKS_PER_SECOND / 3)
			bots_search(bot, tick->player->unit_index);
		if (bot->path.valid && bots_follow_path(bot, tick->player->unit_index, &tick->position, &direction))
			ae_squad_walk(bot, tick, slot, direction);
		else if (bots_distance2d(&tick->position, &bot->goal_point) > 1.5f)
			ae_squad_walk(bot, tick, slot, (real)atan2(bot->goal_point.y - tick->position.y, bot->goal_point.x - tick->position.x));
		else
			tick->moving = FALSE;
		return;
	}
	if (!tick->fighting)
	{
		bot->goal_kind = _bots_goal_none;
		tick->moving = FALSE;
	}
}

/* a member's tick: what it knows of itself, and the controls it builds */
static void ae_squad_update(struct bots_bot *bot, long slot, struct player_action *action)
{
	struct bots_tick tick;
	real_point3d leader_at;
	long leader;
	boolean have_leader, in_vehicle, deck, keep_pace;
	real vitality, distance = 0.0f;

	csmemset(&tick, 0, sizeof(tick));
	tick.skill = ae_bots_tick_skill(bots_globals.skill);
	tick.player = player_get(bot->player_index);
	tick.now = game_time_get();
	tick.throttle_scale = 1.0f;
	tick.action = action;
	csmemset(action, 0, sizeof(*action));
	action->desired_weapon_index = NONE;
	action->desired_grenade_index = NONE;
	action->desired_zoom_level = NONE;
	action->desired_facing.yaw = bot->yaw;
	action->desired_facing.pitch = bot->pitch;

	if (tick.player->unit_index == NONE || TEST_FLAG(object_get(tick.player->unit_index)->object.damage_flags, _object_dead_bit))
	{
		/* (dead, or its body dying: it forgets its fight and its path; the game respawns it) */
		bot->unit_index = NONE;
		bot->target_player_index = NONE;
		bot->foe_unit_index = NONE;
		bot->path.valid = FALSE;
		bot->goal_kind = _bots_goal_none;
		bot->previous_control_flags = 0;
		return;
	}
	tick.unit = unit_get(tick.player->unit_index);
	tick.object = object_get(tick.player->unit_index);
	tick.position = tick.object->object.position;
	/* (seated, its position is the vehicle's own: the world's point instead, as a multiplayer bot's, for its eye and aim) */
	ae_bots_vehicle_origin(&tick);
	if (bot->unit_index != tick.player->unit_index)
	{
		/* (spawned: it looks the way its unit faces) */
		bot->unit_index = tick.player->unit_index;
		bot->yaw = (real)atan2(tick.object->object.forward.j, tick.object->object.forward.i);
		bot->pitch = 0.0f;
		bot->last_vitality = tick.object->object.body_vitality + tick.object->object.shield_vitality;
		bot->stuck_position = tick.position;
		bot->stuck_tick = tick.now;
		bot->stuck_count = 0;
		bot->goal_kind = _bots_goal_none;
		bot->next_look_tick = tick.now + (slot % 3);
		bot->climb_until_tick = 0;
		bot->path_short = FALSE;
		bot->escape_until_tick = 0;
		bot->last_position = tick.position;
		bot->target_player_index = NONE;
		bot->foe_unit_index = NONE;
		bots_level_reset(&bot->combat);
	}
	vitality = tick.object->object.body_vitality + tick.object->object.shield_vitality;
	if (vitality < bot->last_vitality - 0.01f)
		bot->hurt_tick = tick.now;
	bot->last_vitality = vitality;
	bot->last_position = tick.position;
	in_vehicle = (boolean)(tick.object->object.parent_object_index != NONE);

	leader = ae_companion_leader(&tick.position, &leader_at);
	have_leader = (boolean)(leader != NONE);
	if (have_leader)
		distance = bots_distance3d(&tick.position, &leader_at);

	/* (ON A DECK: the player or this member on an object's ground, a lift, or on narrow ground, a bridge deck: a path there
	finds the ground under the object, or a step aside is a step off; the member walks the player's own track in single
	file, never strafes, steps aside or unsticks sideways) */
	deck = (boolean)(have_leader && !in_vehicle && (ae_companion_on_deck(leader) || ae_companion_on_deck(tick.player->unit_index)));
	/* (held a while after it was last seen: a bridge's far end, a wider piece of it, does not flip it off and on) */
	if (!in_vehicle && have_leader)
		deck = (boolean)ae_bots_squad_deck_held(deck, tick.now, &ae_squad.members[slot].deck_until);
	if (deck != ae_squad.members[slot].deck)
	{
		ae_squad.members[slot].deck = deck;
		platform_log("companion: %ld %s", slot + 1, deck ? "follows the player on an object or narrow ground, on their track" : "is off the deck");
	}
	ae_squad_look(bot, &tick, have_leader, &leader_at);
	tick.weapon_index = bots_current_weapon(tick.unit);
	/* (the gunner's seat: it fights with the vehicle's gun, as a multiplayer bot's gunner does) */
	ae_bots_vehicle_weapon(bot, &tick);
	if (bot->target_player_index != NONE && tick.now - bot->target_seen_tick <= TICKS_PER_SECOND / 2)
	{
		bots_bot_fight(bot, &tick);
		ae_squad.fight_ticks++;
		/* (on a deck no fight's move is taken: it walks the track below or stands; elsewhere a friend in its line is
		stepped past) */
		if (deck)
			tick.moving = FALSE;
		else
			ae_squad_sidestep(bot, &tick, slot);
	}
	ae_squad.members[slot].walked = FALSE;
	keep_pace = (boolean)(have_leader && ae_squad_leader_moving(leader));
	if (in_vehicle)
	{
		/* (a seat's own rules move it: it aims and fires) */
		tick.moving = FALSE;
	}
	/* (no fetch on a deck: a step off it for a weapon is a fall) */
	else if (have_leader && !deck && ae_squad_fetch(bot, &tick, slot, &leader_at))
		ae_squad.members[slot].track_walked = FALSE;
	else if (!have_leader)
	{
		if (!tick.fighting)
			tick.moving = FALSE;
	}
	else if (deck)
	{
		/* (the next spot of the player's track, in single file by number; in a fight too while the player moves on, shooting
		on the move: along the track, never off it) */
		if ((!tick.fighting || keep_pace) && ae_bots_squad_follow_wants_move(distance, ae_squad.members[slot].moving, slot))
		{
			real_point3d step = leader_at;

			ae_companion_track_goal(&tick.position, &step);
			bot->goal_kind = _bots_goal_none;
			bot->path.valid = FALSE;
			ae_squad_walk(bot, &tick, slot, (real)atan2(step.y - tick.position.y, step.x - tick.position.x));
			ae_squad_track_jump(&tick, slot);
		}
		else
		{
			ae_squad.members[slot].track_walked = FALSE;
			if (!tick.fighting)
				tick.moving = FALSE;
		}
	}
	else
	{
		ae_squad.members[slot].track_walked = FALSE;
		ae_squad_fireteam(bot, &tick, slot, keep_pace);
	}
	if (!in_vehicle && !tick.fighting && !tick.moving)
		ae_squad_face(bot, &tick, slot);
	ae_squad.members[slot].moving = ae_squad.members[slot].walked;
	if (!tick.fighting && tick.weapon_index != NONE && bots_weapon_loaded(tick.weapon_index) < 0.4f &&
		tick.now - bot->target_seen_tick > TICKS_PER_SECOND * 2)
		tick.flags |= FLAG(_unit_control_weapon_reload_bit);
	ae_squad_weapons(&tick, bot, slot);
	if (!in_vehicle)
	{
		if (!tick.fighting && !deck)
			bots_bot_yield(slot, &tick);
		if (!deck)
			ae_squad_unstick(bot, &tick);
	}

	/* (THE EDGE before the throttle is made of the walk: its turn back, its brake and its stand are what the unit gets) */
	if (!in_vehicle)
		ae_squad_edge(&tick, slot, deck);
	if (tick.moving)
	{
		real relative = bots_angle_difference(tick.move_direction, bot->yaw);

		action->throttle.i = (real)cos(relative) * tick.throttle_scale;
		action->throttle.j = (real)sin(relative) * tick.throttle_scale;
	}
	bot->pitch = PIN(bot->pitch, -1.4f, 1.4f);
	bot->yaw = bots_angle_difference(bot->yaw, 0.0f);
	action->desired_facing.yaw = bot->yaw;
	action->desired_facing.pitch = bot->pitch;
	/* a press is one tick held and the next released (the jump, the grenade, the blow, the reload), as a player's buttons are */
	{
		unsigned long pressed = FLAG(_unit_control_jump_bit) | FLAG(_unit_control_throw_grenade_bit) |
			FLAG(_unit_control_use_equipment_bit) | FLAG(_unit_control_weapon_reload_bit);

		tick.flags &= ~(bot->previous_control_flags & pressed);
		bot->previous_control_flags = tick.flags;
	}
	if (TEST_FLAG(tick.flags, _unit_control_weapon_primary_trigger_bit))
	{
		ae_squad.trigger_ticks++;
		if (in_vehicle)
			ae_squad.gunner_trigger_ticks++;
	}
	if (TEST_FLAG(tick.flags, _unit_control_throw_grenade_bit))
		ae_squad.grenades++;
	if (TEST_FLAG(tick.flags, _unit_control_use_equipment_bit))
		ae_squad.blows++;
	action->control_flags = tick.flags;
	action->primary_trigger = TEST_FLAG(tick.flags, _unit_control_weapon_primary_trigger_bit) ? 1.0f : 0.0f;
}

/* the member's controls this tick (an update built again in a tick gets the same ones) */
void bots_squad_drive(long slot, long player_index, struct player_action *action)
{
	struct bots_bot *bot;

	if (slot < 0 || slot >= AE_COMPANION_MOST)
		return;
	ae_squad_tick_begin();
	if (ae_squad.members[slot].valid && ae_squad.members[slot].tick == ae_squad.tick)
	{
		*action = ae_squad.members[slot].action;
		return;
	}
	bot = &bots_globals.bots[slot];
	if (bot->player_index != player_index)
		bots_bot_reset(bot, player_index, slot);
	/* (the level's intro and cutscenes, until it lets the players go: the member waits, as a player's own controls would) */
	if (ae_companion_intro_holds())
	{
		/* (no fight carried out of it) */
		bot->target_player_index = NONE;
		bot->foe_unit_index = NONE;
		csmemset(action, 0, sizeof(*action));
		action->desired_weapon_index = NONE;
		action->desired_grenade_index = NONE;
		action->desired_zoom_level = NONE;
		action->desired_facing.yaw = bot->yaw;
		action->desired_facing.pitch = bot->pitch;
	}
	else
		ae_squad_update(bot, slot, action);
	ae_squad.members[slot].action = *action;
	ae_squad.members[slot].tick = ae_squad.tick;
	ae_squad.members[slot].valid = TRUE;
}

void bots_squad_reset(long slot)
{
	if (slot < 0 || slot >= AE_COMPANION_MOST)
		return;
	ae_squad.members[slot].valid = FALSE;
	ae_squad.members[slot].moving = FALSE;
	ae_squad.members[slot].pickup_item = NONE;
	ae_squad.members[slot].no_pickup_since = 0;
	/* (the next update takes the body as a new one: its look, goal, path, target and stuck watch from now) */
	bots_globals.bots[slot].unit_index = NONE;
	bots_globals.bots[slot].path.valid = FALSE;
}

/* the brain's counts since the level's start, for halo.log's line */
void bots_squad_report(char *text, unsigned int size)
{
	snprintf(text, size, "brain: %ld targets found, fight ticks %ld, trigger %ld, grenades %ld, blows %ld, follow ticks %ld, unsticks %ld, "
		"held fire %ld ticks, sidesteps %ld, edges held %ld, jumps held %ld, track jumps %ld, walked fighting %ld ticks, "
		"seated trigger %ld, dry ticks %ld, pickups gone for %ld, refill steps %ld",
		ae_squad.targets_found, ae_squad.fight_ticks, ae_squad.trigger_ticks, ae_squad.grenades, ae_squad.blows,
		ae_squad.follow_ticks, ae_squad.unsticks, ae_bots_friendly_fire_held_ticks, ae_squad.sidesteps,
		ae_squad.edges_held, ae_squad.jumps_held, ae_squad.track_jumps, ae_squad.walk_fight_ticks, ae_squad.gunner_trigger_ticks,
		ae_squad.empty_ticks, ae_squad.pickups_gone_for, ae_squad.refills);
}
