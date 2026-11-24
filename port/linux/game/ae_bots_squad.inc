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
#include "ae_companion_rules.h"
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
		/* CUT OFF (ae_bots_squad_cut_off): a barrier between it and the player, the next test of the line, held until */
		boolean cut_off;
		long cut_check_tick;
		long cut_until;
		/* TRACK FOLLOW (ae_bots_squad_track_follows): indoors it walks the player's track; logged as it changes */
		boolean track_follow;
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
		/* TAKE: the pickup was ordered by a SMART tap (it is gone for though the member is not dry) */
		boolean pickup_ordered;
		/* COVER: the spot it took (valid), its exposure, the next check, since when it walks to it */
		boolean cover_valid;
		real_point3d cover_spot;
		real cover_exposure;
		long cover_check_tick;
		long cover_since;
		/* it walked with the player this tick (ae_squad_walk) */
		boolean walked;
		struct player_action action;
	} members[AE_COMPANION_MOST];
	/* halo.log's counts */
	long targets_found;
	long fight_ticks;
	long trigger_ticks;
	long gunner_trigger_ticks;
	/* member ticks held by the level's intro (ae_companion_intro_holds) */
	long hold_ticks;
	/* the gunner's held trigger: the farthest target it fired at (world units) */
	real gunner_farthest;
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
	/* COVER: spots taken, ticks fought from one */
	long covers_taken;
	long cover_ticks;
	long pickups_gone_for;
	/* TRACK FOLLOW: the player under a roof (ae_bots_squad_indoors) at the last test, the next test; member ticks on the track,
	jumps and crouches taken from its cues */
	boolean indoors;
	long indoors_check_tick;
	long track_follow_ticks;
	long cue_jumps;
	long cue_crouches;
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

static void ae_squad_team_tick(long now);
static boolean ae_squad_low_line_blocked(real_point3d const *from, real_point3d const *to, real_point3d *hit);

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
	ae_squad_team_tick(now);
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
	struct unit_datum const *seat = tick->object->object.parent_object_index != NONE ?
		(struct unit_datum const *)object_try_and_get_and_verify_type(tick->object->object.parent_object_index, _object_mask_unit) : NULL;
	/* (in a gunner's seat it sees, and fights, as far as a human gunner hits) */
	boolean gunner = (boolean)(seat && seat->unit.gunner_object_index == tick->player->unit_index);
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
		if (have_leader && !focused && !ae_bots_squad_foe_in_reach(bots_distance3d(&point, leader_at) /
				ae_bots_squad_gunner_reach(1.0f, gunner)))
			continue;
		current = (boolean)(bot->foe_unit_index == foes[index] && bot->target_player_index == BOTS_FOE_TOKEN);
		distance = bots_distance3d(eye, &point);
		reach = ae_bots_squad_gunner_reach(skill->sight_distance * (current ? 1.25f : 1.0f), gunner);
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
	real_point3d leader_at;
	real me_to_player;

	if (ae_companion_leader(&tick->position, &leader_at) == NONE)
		return FALSE;
	me_to_player = bots_distance3d(&tick->position, &leader_at);
	data_iterator_new(&iterator, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
	{
		real_point3d at;

		if (player == tick->player || player->unit_index == NONE ||
			TEST_FLAG(object_get(player->unit_index)->object.damage_flags, _object_dead_bit))
			continue;
		object_get_origin(player->unit_index, &at);
		/* (one ahead on the way to the player stopped: single file; one beside or behind does not hold the jump) */
		if (ae_bots_squad_track_blocked(bots_distance3d(&at, &tick->position), bots_distance3d(&at, &leader_at), me_to_player))
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

	if (!ae_squad_dry(tick) && !ae_squad.members[slot].pickup_ordered)
	{
		ae_squad.members[slot].pickup_item = NONE;
		ae_squad.members[slot].no_pickup_since = 0;
		return FALSE;
	}
	item = ae_squad.members[slot].pickup_item == NONE ? NULL :
		(struct object_datum *)object_try_and_get_and_verify_type(ae_squad.members[slot].pickup_item, _object_mask_weapon);
	/* (an ordered weapon taken, gone or given up: the order is done) */
	if (ae_squad.members[slot].pickup_ordered && (!item || item->object.parent_object_index != NONE ||
		tick->now - ae_squad.members[slot].pickup_tick > AE_SQUAD_PICKUP_GIVE_UP_TICKS))
	{
		ae_squad.members[slot].pickup_ordered = FALSE;
		ae_squad.members[slot].pickup_item = NONE;
		return FALSE;
	}
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

/* FORMATION: the spots of the squad's members near the player (ae_bots_squad_spot_*), chosen at tick, the player at at */
static struct
{
	long tick;
	real_point3d at;
	boolean valid[AE_COMPANION_MOST];
	real_point3d spot[AE_COMPANION_MOST];
	long choices;
	long none_found;
} ae_squad_form = { -1 };

/* the spots chosen again: for each member in turn the first of its candidates with a floor near the player's height, a line from
the player, and room (ae_bots_squad_spot_crowded) */
static void ae_squad_formation_choose(long fireteam, real_point3d const *leader_point, real facing, long now)
{
	long leader = ae_fireteam_leader_player(fireteam), slot, leader_unit = NONE;
	struct player_datum const *player = leader != NONE ? player_try_and_get(leader) : NULL;
	real_point3d eye = *leader_point, leader_ground;
	boolean have_ground;

	if (player)
		leader_unit = player->unit_index;
	eye.z += 0.6f;
	have_ground = (boolean)(bots_surface_below(leader_point, &leader_ground) != NONE);
	if (!have_ground)
		leader_ground = *leader_point;
	for (slot = 0; slot < AE_COMPANION_MOST; slot++)
	{
		float dx[AE_SQUAD_SPOT_CANDIDATES], dy[AE_SQUAD_SPOT_CANDIDATES];
		int count = ae_bots_squad_spot_candidates(slot, facing, dx, dy, AE_SQUAD_SPOT_CANDIDATES), each;

		ae_squad_form.valid[slot] = FALSE;
		for (each = 0; each < count; each++)
		{
			real_point3d probe = *leader_point, ground, seen;
			real nearest = 1000.0f;
			long other;

			probe.x += dx[each];
			probe.y += dy[each];
			if (bots_surface_below(&probe, &ground) == NONE || fabs(ground.z - leader_ground.z) > AE_SQUAD_SPOT_STEP)
				continue;
			for (other = 0; other < slot; other++)
				if (ae_squad_form.valid[other] && bots_distance3d(&ae_squad_form.spot[other], &ground) < nearest)
					nearest = bots_distance3d(&ae_squad_form.spot[other], &ground);
			if (ae_bots_squad_spot_crowded(nearest, bots_distance2d(&ground, leader_point)))
				continue;
			seen = ground;
			seen.z += 0.6f;
			if (!bots_line_of_sight(leader_unit, &eye, &seen, NONE))
				continue;
			/* (nor behind a waist-high barrier from the player: a doorway's pipes they jumped) */
			seen.z = ground.z + AE_SQUAD_CUT_OFF_HEIGHT;
			if (ae_squad_low_line_blocked(leader_point, &seen, NULL))
				continue;
			ae_squad_form.spot[slot] = ground;
			ae_squad_form.valid[slot] = TRUE;
			break;
		}
		if (!ae_squad_form.valid[slot])
			ae_squad_form.none_found++;
	}
	ae_squad_form.tick = now;
	ae_squad_form.at = *leader_point;
	ae_squad_form.choices++;
}

static boolean ae_squad_formation_spot(long fireteam, long brain, real_point3d const *leader_point, real facing, real_point3d *spot)
{
	long now = game_time_get();

	if (!ae_fireteam_campaign_on() || brain < 0 || brain >= AE_COMPANION_MOST)
		return FALSE;
	if (ae_bots_squad_spot_refresh(ae_squad_form.tick < 0 || now < ae_squad_form.tick ? -1 : now - ae_squad_form.tick,
			bots_distance3d(leader_point, &ae_squad_form.at)))
		ae_squad_formation_choose(fireteam, leader_point, facing, now);
	if (!ae_squad_form.valid[brain])
		return FALSE;
	*spot = ae_squad_form.spot[brain];
	return TRUE;
}

/* TEAM LAYER: the squad's state (ae_bots_squad_team_state), the tick it began, the REGROUP's start, each member's time out of
position and its last "catching up" */
static struct
{
	int state;
	long since;
	long regroup_since;
	long out_since[AE_COMPANION_MOST];
	long said[AE_COMPANION_MOST];
	long changes;
} ae_squad_team;

/* a member's line in the players' chat (its name, then the words) */
static void ae_squad_say(struct player_datum const *member, wchar_t const *words)
{
	wchar_t text[48];
	short length = 0, at, local;

	for (at = 0; at < 12 && member->name[at] && length < 24; at++)
		text[length++] = member->name[at];
	for (at = 0; words[at] && length < 47; at++)
		text[length++] = words[at];
	text[length] = 0;
	for (local = local_player_get_next(NONE); local != NONE; local = local_player_get_next(local))
		hud_print_message(local, text);
}

static void ae_squad_team_tick(long now)
{
	long fireteam = ae_fireteam_of_brain(0), leader, slot, leader_unit = NONE;
	struct player_datum const *leader_player;
	short kind;
	int order, members = 0, in_position = 0, state;

	if (!ae_fireteam_campaign_on() || fireteam == NONE)
		return;
	leader = ae_fireteam_leader_player(fireteam);
	leader_player = leader != NONE ? player_try_and_get(leader) : NULL;
	if (leader_player)
		leader_unit = leader_player->unit_index;
	kind = ae_fireteam_order_of(fireteam);
	order = kind == _ae_order_follow ? AE_SQUAD_ORDER_FOLLOW : kind == _ae_order_regroup ? AE_SQUAD_ORDER_REGROUP : AE_SQUAD_ORDER_HOLD;
	for (slot = 0; slot < AE_COMPANION_MOST; slot++)
	{
		struct bots_bot const *bot = &bots_globals.bots[slot];
		struct player_datum const *player = bot->player_index != NONE ? player_try_and_get(bot->player_index) : NULL;
		real_point3d at;
		real off;

		if (!player || player->unit_index == NONE || ae_fireteam_of_brain(slot) != fireteam ||
			TEST_FLAG(object_get(player->unit_index)->object.damage_flags, _object_dead_bit) ||
			object_get(player->unit_index)->object.parent_object_index != NONE)
		{
			ae_squad_team.out_since[slot] = 0;
			continue;
		}
		members++;
		object_get_origin(player->unit_index, &at);
		off = ae_squad_form.valid[slot] ? bots_distance3d(&at, &ae_squad_form.spot[slot]) : 0.0f;
		/* (on the player's track indoors: its place in the single file, not a formation spot) */
		if (ae_squad.members[slot].track_follow && leader_unit != NONE)
		{
			real_point3d leader_at;

			object_get_origin(leader_unit, &leader_at);
			off = ae_bots_squad_track_off(bots_distance3d(&at, &leader_at), slot);
		}
		if (ae_bots_squad_in_position(order, off))
			in_position++;
		/* (out of position under a FOLLOW: "catching up" after a while) */
		if (order != AE_SQUAD_ORDER_FOLLOW || off <= AE_SQUAD_SPOT_MOVED * 2.0f)
			ae_squad_team.out_since[slot] = 0;
		else if (!ae_squad_team.out_since[slot])
			ae_squad_team.out_since[slot] = now > 0 ? now : 1;
		if (ae_squad_team.out_since[slot] && ae_bots_squad_keep_up_due(now - ae_squad_team.out_since[slot],
			ae_squad_team.said[slot] ? now - ae_squad_team.said[slot] : -1))
		{
			ae_squad_team.said[slot] = now > 0 ? now : 1;
			ae_squad_say(player, L": catching up");
			platform_log("ae squad: %ld is catching up, %.1f units from its spot", slot + 1, off);
		}
	}
	state = ae_bots_squad_team_state(order, leader_unit != NONE && ae_squad_leader_moving(leader_unit), members, in_position);
	/* (a REGROUP waits for every member, 10 s at most: then the squad follows again with who is in) */
	if (order == AE_SQUAD_ORDER_REGROUP && state == AE_SQUAD_TEAM_REGROUPING)
	{
		if (!ae_squad_team.regroup_since || now < ae_squad_team.regroup_since)
			ae_squad_team.regroup_since = now > 0 ? now : 1;
		else if (ae_bots_squad_regroup_timed_out(now - ae_squad_team.regroup_since))
		{
			platform_log("ae squad: the regroup gave up after %d s with %d of %d in position: follow", AE_SQUAD_REGROUP_TIMEOUT_TICKS / 30,
				in_position, members);
			ae_squad_team.regroup_since = 0;
			ae_fireteam_campaign_reset();
		}
	}
	else
		ae_squad_team.regroup_since = 0;
	if (state != ae_squad_team.state)
	{
		ae_squad_team.changes++;
		platform_log("ae squad: team %s (%d of %d in position, after %ld s %s)", ae_bots_squad_team_state_name(state), in_position, members,
			ae_squad_team.since && now >= ae_squad_team.since ? (now - ae_squad_team.since) / 30 : 0,
			ae_bots_squad_team_state_name(ae_squad_team.state));
		ae_squad_team.state = state;
		ae_squad_team.since = now > 0 ? now : 1;
	}
}

/* SMART (ae_bots_squad_smart_pick): the nearest living member on foot to a point, its slot (NONE with none) and how far */
static long ae_squad_nearest_slot(real_point3d const *point, real *distance)
{
	long slot, best = NONE;
	real best_distance = REAL_MAX;

	for (slot = 0; slot < AE_COMPANION_MOST; slot++)
	{
		struct bots_bot const *bot = &bots_globals.bots[slot];
		struct player_datum const *player = bot->player_index != NONE ? player_try_and_get(bot->player_index) : NULL;
		real_point3d at;
		real d;

		if (!player || player->unit_index == NONE || TEST_FLAG(object_get(player->unit_index)->object.damage_flags, _object_dead_bit))
			continue;
		object_get_origin(player->unit_index, &at);
		d = bots_distance3d(&at, point);
		if (d < best_distance)
		{
			best = slot;
			best_distance = d;
		}
	}
	*distance = best_distance;
	return best;
}

/* whether a member was sent to man this vehicle */
static boolean ae_squad_mounted(long vehicle)
{
	long slot;

	for (slot = 0; slot < AE_COMPANION_MOST; slot++)
		if (vehicle != NONE && ae_companion_mount_of(slot) == vehicle)
			return TRUE;
	return FALSE;
}

static boolean ae_squad_smart(struct player_datum const *marker, short mark_kind, long object_index, real_point3d const *spot, long led)
{
	struct object_datum *object = object_index != NONE ? (struct object_datum *)object_try_and_get(object_index) : NULL;
	short kind = ae_fireteam_order_of(led);
	real_point3d order_point;
	boolean point_order = (boolean)(kind != _ae_order_follow && kind != _ae_order_regroup && kind != _ae_order_none &&
		ae_fireteam_order_point_of(led, &order_point));
	boolean vehicle = (boolean)(object && (ae_companion_has_free_seat(object_index) || ae_squad_mounted(object_index)));
	boolean weapon = (boolean)(object && object_try_and_get_and_verify_type(object_index, _object_mask_weapon) &&
		object->object.parent_object_index == NONE && !bots_weapon_empty(object_index));
	int pick = ae_bots_squad_smart_pick(mark_kind == AE_MARK_KIND_ENEMY && object, point_order,
		point_order ? bots_distance3d(&order_point, spot) : REAL_MAX, vehicle, weapon);
	real distance = 0.0f;
	long slot;

	if (pick == AE_SQUAD_SMART_FOCUS || pick == AE_SQUAD_SMART_MOVE)
		return FALSE;
	if (pick == AE_SQUAD_SMART_CANCEL)
	{
		for (slot = 0; slot < AE_COMPANION_MOST; slot++)
			ae_companion_mount(slot, NONE);
		ae_fireteam_order((int)led, _ae_order_follow, NULL, NONE);
		platform_log("ae squad: SMART on the squad's own marker: the order is cancelled, follow");
		slot = ae_squad_nearest_slot(spot, &distance);
		if (slot != NONE)
			ae_squad_say(player_get(bots_globals.bots[slot].player_index), L": on you");
		return TRUE;
	}
	slot = ae_squad_nearest_slot(&object->object.position, &distance);
	if (slot == NONE || !ae_bots_squad_smart_in_reach(pick, distance))
	{
		platform_log("ae squad: SMART %s: no member in reach", pick == AE_SQUAD_SMART_MOUNT ? "on a vehicle" : "on a weapon");
		return TRUE;
	}
	if (pick == AE_SQUAD_SMART_MOUNT)
	{
		long other;

		/* (a vehicle a member was sent to: it is let go, the member gets out) */
		for (other = 0; other < AE_COMPANION_MOST; other++)
			if (ae_companion_mount_of(other) == object_index)
			{
				ae_companion_mount(other, NONE);
				platform_log("ae squad: SMART on the vehicle %ld mans: it gets out", other + 1);
				ae_squad_say(player_get(bots_globals.bots[other].player_index), L": on you");
				return TRUE;
			}
		if (!ae_companion_mount(slot, object_index))
			return TRUE;
		platform_log("ae squad: SMART on a vehicle: %ld mans it (%.1f units away)", slot + 1, distance);
	}
	else
	{
		ae_squad.members[slot].pickup_item = object_index;
		ae_squad.members[slot].pickup_tick = game_time_get();
		ae_squad.members[slot].pickup_ordered = TRUE;
		{
			real_point3d ground;
			long surface = bots_surface_below(&object->object.position, &ground);

			if (surface != NONE)
				bots_set_goal(&bots_globals.bots[slot], _bots_goal_item, &ground, surface);
			bots_globals.bots[slot].item_index = object_index;
		}
		bots_globals.bots[slot].path.valid = FALSE;
		platform_log("ae squad: SMART on a weapon: %ld takes it (%.1f units away)", slot + 1, distance);
	}
	ae_squad_say(player_get(bots_globals.bots[slot].player_index), L": on it");
	(void)marker;
	return TRUE;
}

/* COVER (ae_bots_squad_cover_*): the exposure of a floor point to an enemy's eye, by its lines to a crouch's body and to the eyes */
static real ae_squad_cover_rate(long member_unit, real_point3d const *eye, real_point3d const *floor)
{
	real_point3d body = *floor, eyes = *floor;

	body.z += AE_SQUAD_COVER_LOW;
	eyes.z += AE_SQUAD_COVER_HIGH;
	return ae_bots_squad_cover_exposure(bots_line_of_sight(member_unit, eye, &body, NONE), bots_line_of_sight(member_unit, eye, &eyes, NONE));
}

/* whether a floor point is too near another member (where it stands or the cover it took) */
static boolean ae_squad_cover_crowded(long slot, real_point3d const *point)
{
	long other;

	for (other = 0; other < AE_COMPANION_MOST; other++)
	{
		struct player_datum const *player = other != slot && bots_globals.bots[other].player_index != NONE ?
			player_try_and_get(bots_globals.bots[other].player_index) : NULL;
		real_point3d at;

		if (ae_squad.members[other].cover_valid && other != slot &&
			bots_distance3d(&ae_squad.members[other].cover_spot, point) < AE_SQUAD_SPOT_SPACING)
			return TRUE;
		if (!player || player->unit_index == NONE)
			continue;
		object_get_origin(player->unit_index, &at);
		if (bots_distance3d(&at, point) < AE_SQUAD_SPOT_SPACING)
			return TRUE;
	}
	return FALSE;
}

/* a member fighting under FOLLOW, the player standing: it takes cover from the enemy it fights and fights from there; TRUE while
it walks to it or holds it */
static boolean ae_squad_cover(struct bots_bot *bot, struct bots_tick *tick, long slot, real_point3d const *leader_at)
{
	long fireteam = ae_fireteam_of_brain(slot);
	struct object_datum *foe = bot->target_player_index == BOTS_FOE_TOKEN && bot->foe_unit_index != NONE ?
		(struct object_datum *)object_try_and_get_and_verify_type(bot->foe_unit_index, _object_mask_unit) : NULL;
	real_point3d eye, ground;
	real direction, dx, dy, length;

	if (!tick->fighting || !foe || fireteam == NONE || ae_fireteam_order_of(fireteam) != _ae_order_follow)
	{
		ae_squad.members[slot].cover_valid = FALSE;
		return FALSE;
	}
	/* (the enemy's eye, a step towards the member: its own body is not the cover) */
	eye = foe->object.bounding_sphere_center;
	eye.z += 0.3f;
	dx = tick->position.x - eye.x;
	dy = tick->position.y - eye.y;
	length = (real)sqrt(dx * dx + dy * dy);
	if (length > 1.0f)
	{
		eye.x += dx / length * 0.6f;
		eye.y += dy / length * 0.6f;
	}
	if (tick->now >= ae_squad.members[slot].cover_check_tick || tick->now < ae_squad.members[slot].cover_check_tick -
		AE_SQUAD_COVER_CHECK_TICKS)
	{
		real here;

		ae_squad.members[slot].cover_check_tick = tick->now + AE_SQUAD_COVER_CHECK_TICKS;
		if (bots_surface_below(&tick->position, &ground) == NONE)
			return ae_squad.members[slot].cover_valid;
		/* (the spot it took, or where it stands: kept while its exposure is under the leave threshold) */
		if (ae_squad.members[slot].cover_valid)
		{
			ae_squad.members[slot].cover_exposure = ae_squad_cover_rate(tick->player->unit_index, &eye, &ae_squad.members[slot].cover_spot);
			if (ae_bots_squad_cover_leaves(ae_squad.members[slot].cover_exposure) ||
				bots_distance2d(&ae_squad.members[slot].cover_spot, leader_at) > AE_SQUAD_COVER_LEASH)
				ae_squad.members[slot].cover_valid = FALSE;
		}
		here = ae_squad_cover_rate(tick->player->unit_index, &eye, &ground);
		if (!ae_squad.members[slot].cover_valid && ae_bots_squad_cover_leaves(here))
		{
			real best_value = REAL_MAX;
			real_point3d best, chest = tick->position;
			int ring, each;

			chest.z += 0.6f;
			for (ring = 0; ring < 2; ring++)
				for (each = 0; each < AE_SQUAD_COVER_ANGLES; each++)
				{
					real radius = ring ? AE_SQUAD_COVER_RING_FAR : AE_SQUAD_COVER_RING_NEAR;
					real angle = (real)each * 2.0f * 3.14159265f / AE_SQUAD_COVER_ANGLES;
					real_point3d probe = tick->position, floor, seen;
					real exposure, value;

					probe.x += radius * (real)cos(angle);
					probe.y += radius * (real)sin(angle);
					if (bots_surface_below(&probe, &floor) == NONE || fabs(floor.z - ground.z) > AE_SQUAD_SPOT_STEP ||
						bots_distance2d(&floor, leader_at) > AE_SQUAD_COVER_LEASH ||
						bots_distance2d(&floor, leader_at) < AE_SQUAD_SPOT_LEADER_GAP || ae_squad_cover_crowded(slot, &floor))
						continue;
					/* (a spot behind a wall from the member is no walk) */
					seen = floor;
					seen.z += 0.6f;
					if (!bots_line_of_sight(tick->player->unit_index, &chest, &seen, NONE))
						continue;
					exposure = ae_squad_cover_rate(tick->player->unit_index, &eye, &floor);
					value = ae_bots_squad_cover_value(exposure, radius);
					if (!ae_bots_squad_cover_takes(exposure) || value >= best_value)
						continue;
					best_value = value;
					best = floor;
					ae_squad.members[slot].cover_exposure = exposure;
				}
			if (best_value < REAL_MAX)
			{
				ae_squad.members[slot].cover_valid = TRUE;
				ae_squad.members[slot].cover_spot = best;
				ae_squad.members[slot].cover_since = tick->now;
				ae_squad.covers_taken++;
				platform_log("companion: %ld takes cover %.1f units away (%s cover)", slot + 1, bots_distance3d(&best, &tick->position),
					ae_squad.members[slot].cover_exposure > 0.25f ? "half" : "full");
			}
		}
	}
	if (!ae_squad.members[slot].cover_valid)
		return FALSE;
	if (bots_distance2d(&tick->position, &ae_squad.members[slot].cover_spot) < AE_SQUAD_COVER_ARRIVED)
	{
		tick->moving = FALSE;
		ae_squad.cover_ticks++;
		return TRUE;
	}
	/* (not reached in 4 s: given up) */
	if (tick->now - ae_squad.members[slot].cover_since > 4 * TICKS_PER_SECOND)
	{
		ae_squad.members[slot].cover_valid = FALSE;
		return FALSE;
	}
	if (bot->goal_kind != _bots_goal_roam || bots_distance3d(&bot->goal_point, &ae_squad.members[slot].cover_spot) > 1.0f)
	{
		long surface = bots_surface_below(&ae_squad.members[slot].cover_spot, &ground);

		if (surface != NONE)
			bots_set_goal(bot, _bots_goal_roam, &ground, surface);
	}
	if (!bot->path.valid && tick->now - bot->path_tick > TICKS_PER_SECOND / 3)
		bots_search(bot, tick->player->unit_index);
	if (bot->path.valid && bots_follow_path(bot, tick->player->unit_index, &tick->position, &direction))
		ae_squad_walk(bot, tick, slot, direction);
	else
		ae_squad_walk(bot, tick, slot, (real)atan2(ae_squad.members[slot].cover_spot.y - tick->position.y,
			ae_squad.members[slot].cover_spot.x - tick->position.x));
	return TRUE;
}

/* MOUNT: a member sent to a vehicle walks to it (ae_companion gets it in once near); TRUE while it goes */
static boolean ae_squad_go_mount(struct bots_bot *bot, struct bots_tick *tick, long slot)
{
	long vehicle = ae_companion_mount_of(slot);
	struct object_datum *object;
	real direction;

	if (vehicle == NONE || !(object = (struct object_datum *)object_try_and_get(vehicle)))
		return FALSE;
	if (bots_distance3d(&object->object.position, &tick->position) < AE_SQUAD_MOUNT_ENTER)
	{
		tick->moving = FALSE;
		return TRUE;
	}
	if (bot->goal_kind != _bots_goal_roam || bots_distance3d(&bot->goal_point, &object->object.position) > 2.0f)
	{
		real_point3d ground;
		long surface = bots_surface_below(&object->object.position, &ground);

		if (surface != NONE)
			bots_set_goal(bot, _bots_goal_roam, &ground, surface);
	}
	if (!bot->path.valid && tick->now - bot->path_tick > TICKS_PER_SECOND / 3)
		bots_search(bot, tick->player->unit_index);
	if (bot->path.valid && bots_follow_path(bot, tick->player->unit_index, &tick->position, &direction))
		ae_squad_walk(bot, tick, slot, direction);
	else
		ae_squad_walk(bot, tick, slot, (real)atan2(object->object.position.y - tick->position.y, object->object.position.x - tick->position.x));
	return TRUE;
}

/* a member in a vehicle's gunner seat (its gun the vehicle's: ae_bots_vehicle_weapon) fires as a multiplayer bot gunner does
(ae_bots_ut_gunner): the fight's bursts are dropped, the trigger held while the aim is on its target (ae_bots_squad_gunner_holds) */
static void ae_squad_gunner_fire(struct bots_bot *bot, struct bots_tick *tick, long slot)
{
	struct unit_datum *vehicle = (struct unit_datum *)object_try_and_get_and_verify_type(tick->object->object.parent_object_index,
		_object_mask_unit);
	real_point3d const *eye;
	real dx, dy, dz, horizontal, distance;
	static long opened[AE_COMPANION_MOST];

	if (!vehicle || vehicle->unit.gunner_object_index != tick->player->unit_index)
		return;
	tick->flags &= ~FLAG(_unit_control_weapon_primary_trigger_bit);
	if (!tick->fighting || bot->target_player_index == NONE || slot < 0 || slot >= AE_COMPANION_MOST)
		return;
	eye = bots_tick_eye(tick);
	dx = bot->target_last_position.x - eye->x;
	dy = bot->target_last_position.y - eye->y;
	dz = bot->target_last_position.z - eye->z;
	horizontal = (real)sqrt(dx * dx + dy * dy);
	distance = MAX(horizontal, 0.5f);
	if (!ae_bots_squad_gunner_holds(bots_angle_difference((real)atan2(dy, dx), bot->yaw), (real)atan2(dz, horizontal) - bot->pitch,
			distance, tick->now - bot->target_seen_tick, tick->now - bot->target_first_seen_tick) ||
		!ae_bots_fight_friendly_fire_clear(bot, tick))
		return;
	tick->flags |= FLAG(_unit_control_weapon_primary_trigger_bit);
	if (distance > ae_squad.gunner_farthest)
		ae_squad.gunner_farthest = distance;
	/* (once a target: the range it opened fire at) */
	if (opened[slot] != bot->target_first_seen_tick)
	{
		opened[slot] = bot->target_first_seen_tick;
		platform_log("companion: %ld's gun opens fire at %.1f units", slot + 1, distance);
	}
}

/* whether the line between two points at a barrier's height above them is blocked by the level */
static boolean ae_squad_line_blocked_at(real_point3d const *from, real_point3d const *to, real height, boolean both_faces,
	real_point3d *hit)
{
	struct collision_result collision;
	real_point3d start = *from;
	real_vector3d vector;
	unsigned long flags = FLAG(_collision_test_structure_bit) | FLAG(_collision_test_front_facing_surfaces_bit);

	if (both_faces)
		flags |= FLAG(_collision_test_back_facing_surfaces_bit);
	start.z += height;
	vector.i = to->x - start.x;
	vector.j = to->y - start.y;
	vector.k = to->z - start.z;
	if (!collision_test_vector(flags, &start, &vector, NONE, &collision))
		return FALSE;
	if (hit)
		*hit = collision.point;
	return TRUE;
}

static boolean ae_squad_low_line_blocked(real_point3d const *from, real_point3d const *to, real_point3d *hit)
{
	return ae_squad_line_blocked_at(from, to, AE_SQUAD_CUT_OFF_HEIGHT, FALSE, hit);
}

/* CUT OFF: the line to the player tested every AE_SQUAD_CUT_OFF_CHECK_TICKS; a cut-off is held as a deck is (the far side's first
steps) */
static void ae_squad_cut_off_check(struct bots_tick const *tick, long slot, real_point3d const *leader_at, real distance)
{
	boolean was = ae_squad.members[slot].cut_off;

	if (tick->now >= ae_squad.members[slot].cut_check_tick || tick->now < ae_squad.members[slot].cut_check_tick - AE_SQUAD_CUT_OFF_CHECK_TICKS)
	{
		real_point3d to = *leader_at, low = *leader_at, sloped = *leader_at;
		boolean front = FALSE, faces = FALSE, lower = FALSE, slope = FALSE;

		ae_squad.members[slot].cut_check_tick = tick->now + AE_SQUAD_CUT_OFF_CHECK_TICKS;
		to.z += AE_SQUAD_CUT_OFF_HEIGHT;
		low.z += AE_SQUAD_CUT_OFF_LOW;
		if (distance >= AE_SQUAD_CUT_OFF_GAP)
		{
			front = ae_squad_low_line_blocked(&tick->position, &to, NULL);
			/* (for halo.log, beside the level line: one rising to twice the height at the player) */
			sloped.z += AE_SQUAD_CUT_OFF_HEIGHT * 2.0f;
			slope = ae_squad_line_blocked_at(&tick->position, &sloped, AE_SQUAD_CUT_OFF_HEIGHT, FALSE, NULL);
			/* (indoors also a barrier's far side, the line's back faces, and one lower down: a bundle of pipes lower than the
			line) */
			if (!front && ae_squad.indoors)
			{
				faces = ae_squad_line_blocked_at(&tick->position, &to, AE_SQUAD_CUT_OFF_HEIGHT, TRUE, NULL);
				lower = ae_squad_line_blocked_at(&tick->position, &low, AE_SQUAD_CUT_OFF_LOW, TRUE, NULL);
			}
		}
		if ((front || faces || lower) && !ae_squad.members[slot].cut_off)
			platform_log("companion: %ld's line to the player %.1f away: level %s, back faces %s, low %s; sloped line %s", slot + 1,
				distance, front ? "blocked" : "clear", faces ? "blocked" : front ? "-" : "clear", lower ? "blocked" : front ? "-" : "clear",
				slope ? "blocked" : "clear");
		ae_squad.members[slot].cut_off = (boolean)ae_bots_squad_deck_held(
			ae_bots_squad_cut_off(front || faces || lower, distance), tick->now, &ae_squad.members[slot].cut_until);
	}
	if (ae_squad.members[slot].cut_off != was)
		platform_log("companion: %ld %s", slot + 1, ae_squad.members[slot].cut_off ?
			"is cut off from the player by a barrier: it follows their track over it" : "is with the player again");
}

/* (ae_bots_fireteam.inc) a campaign formation slot behind a barrier from the player is pulled to the player's side */
static void ae_squad_slot_this_side(real_point3d const *leader_point, real_point3d *slot)
{
	real_point3d to = *slot, hit;
	real dx, dy, length;

	to.z = leader_point->z + AE_SQUAD_CUT_OFF_HEIGHT;
	if (!ae_squad_low_line_blocked(leader_point, &to, &hit))
		return;
	dx = hit.x - leader_point->x;
	dy = hit.y - leader_point->y;
	length = (real)sqrt(dx * dx + dy * dy);
	if (length <= AE_SQUAD_CUT_OFF_PULL)
	{
		*slot = *leader_point;
		return;
	}
	slot->x = leader_point->x + dx * (length - AE_SQUAD_CUT_OFF_PULL) / length;
	slot->y = leader_point->y + dy * (length - AE_SQUAD_CUT_OFF_PULL) / length;
	slot->z = leader_point->z;
}

/* TRACK FOLLOW: whether the player (their unit at at) is under a roof, tested every AE_SQUAD_ROOF_CHECK_TICKS (a ray straight
up meeting the level within AE_SQUAD_ROOF) */
static boolean ae_squad_player_indoors(long now, real_point3d const *at)
{
	if (now >= ae_squad.indoors_check_tick || now < ae_squad.indoors_check_tick - AE_SQUAD_ROOF_CHECK_TICKS)
	{
		struct collision_result collision;
		real_point3d start = *at;
		real_vector3d up;
		boolean hit, was = ae_squad.indoors;

		up.i = 0.0f;
		up.j = 0.0f;
		up.k = AE_SQUAD_ROOF;
		start.z += 0.7f;
		hit = (boolean)collision_test_vector(FLAG(_collision_test_structure_bit) | FLAG(_collision_test_front_facing_surfaces_bit) |
			FLAG(_collision_test_back_facing_surfaces_bit), &start, &up, NONE, &collision);
		ae_squad.indoors = (boolean)ae_bots_squad_indoors(hit, hit ? collision.point.z - start.z : 0.0f);
		ae_squad.indoors_check_tick = now + AE_SQUAD_ROOF_CHECK_TICKS;
		if (ae_squad.indoors != was)
			platform_log("companion: the player is %s", ae_squad.indoors ? "indoors: the squad walks their track in single file" :
				"outdoors: the squad keeps its formation");
	}
	return ae_squad.indoors;
}

/* whether a member follows the player's track this tick (TRACK FOLLOW): indoors, a crumb near it, under FOLLOW (or no order) */
static boolean ae_squad_track_follow_on(struct bots_tick const *tick, long slot, real_point3d const *leader_at)
{
	long fireteam = ae_fireteam_of_brain(slot);
	short order = fireteam != NONE ? ae_fireteam_order_of(fireteam) : _ae_order_none;
	real_point3d step;
	boolean on = (boolean)ae_bots_squad_track_follows(ae_squad_player_indoors(tick->now, leader_at),
		ae_companion_track_goal(&tick->position, &step), order == _ae_order_follow || order == _ae_order_none);

	if (on != ae_squad.members[slot].track_follow)
	{
		ae_squad.members[slot].track_follow = on;
		platform_log("companion: %ld %s", slot + 1, on ? "walks the player's track (indoors)" : "leaves the player's track");
	}
	return on;
}

/* a member's tick: what it knows of itself, and the controls it builds */
static void ae_squad_update(struct bots_bot *bot, long slot, struct player_action *action)
{
	struct bots_tick tick;
	real_point3d leader_at;
	long leader;
	boolean have_leader, in_vehicle, deck, keep_pace, track_follow;
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
	/* (CUT OFF: a barrier between it and the player, the pipes of a doorway the player jumped: it walks the player's track over it,
	as on a deck; the formation's spot behind the player is on the barrier's far side) */
	if (have_leader)
		ae_squad_player_indoors(tick.now, &leader_at);
	if (have_leader && !in_vehicle)
		ae_squad_cut_off_check(&tick, slot, &leader_at, distance);
	if (ae_squad.members[slot].cut_off)
		deck = TRUE;
	/* (held a while after it was last seen: a bridge's far end, a wider piece of it, does not flip it off and on) */
	if (!in_vehicle && have_leader)
		deck = (boolean)ae_bots_squad_deck_held(deck, tick.now, &ae_squad.members[slot].deck_until);
	if (deck != ae_squad.members[slot].deck)
	{
		ae_squad.members[slot].deck = deck;
		platform_log("companion: %ld %s", slot + 1, deck ? "follows the player on an object or narrow ground, on their track" : "is off the deck");
	}
	/* (TRACK FOLLOW: indoors, off a deck, it walks the player's track in single file instead of going to a formation spot) */
	track_follow = (boolean)(have_leader && !in_vehicle && !deck && ae_squad_track_follow_on(&tick, slot, &leader_at));
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
		ae_squad_gunner_fire(bot, &tick, slot);
	}
	/* (MOUNT: a member sent to a vehicle walks to it) */
	else if (have_leader && !deck && ae_squad_go_mount(bot, &tick, slot))
		ae_squad.members[slot].track_walked = FALSE;
	/* (no fetch on a deck: a step off it for a weapon is a fall) */
	else if (have_leader && !deck && ae_squad_fetch(bot, &tick, slot, &leader_at))
		ae_squad.members[slot].track_walked = FALSE;
	/* (COVER: in a fight with the player standing, a spot out of the enemy's sight) */
	else if (have_leader && !deck && !keep_pace && ae_squad_cover(bot, &tick, slot, &leader_at))
		ae_squad.members[slot].track_walked = FALSE;
	else if (!have_leader)
	{
		if (!tick.fighting)
			tick.moving = FALSE;
	}
	else if (deck || track_follow)
	{
		/* (the next spot of the player's track, in single file by number; in a fight too while the player moves on, shooting
		on the move: along the track, never off it; indoors the single file's own spacing) */
		if ((!tick.fighting || keep_pace) && (deck ? ae_bots_squad_follow_wants_move(distance, ae_squad.members[slot].moving, slot) ||
			(ae_squad.members[slot].cut_off && distance >= AE_SQUAD_CUT_OFF_GAP) :
			(ae_bots_squad_track_wants_move(distance, ae_squad.members[slot].moving, slot) ||
			ae_bots_squad_perched(tick.position.z - leader_at.z, distance))))
		{
			real_point3d step = leader_at;
			int cue = 0;

			ae_companion_track_goal_cue(&tick.position, &step, &cue);
			bot->goal_kind = _bots_goal_none;
			bot->path.valid = FALSE;
			ae_squad_walk(bot, &tick, slot, (real)atan2(step.y - tick.position.y, step.x - tick.position.x));
			/* (the player's own jump and crouch where they made them; else a stop is jumped after its check) */
			if ((cue & AE_COMPANION_TRACK_JUMPED) && tick.object->object.parent_object_index == NONE &&
				!TEST_FLAG(tick.flags, _unit_control_jump_bit))
			{
				tick.flags |= FLAG(_unit_control_jump_bit);
				ae_squad.cue_jumps++;
			}
			if (cue & AE_COMPANION_TRACK_CROUCHED)
			{
				tick.flags |= FLAG(_unit_control_crouch_modifier_bit);
				ae_squad.cue_crouches++;
			}
			ae_squad_track_jump(&tick, slot);
			if (track_follow)
				ae_squad.track_follow_ticks++;
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
	{
		static int held_why = -1;
		int why = ae_companion_intro_hold_reason();

		if (why != held_why)
		{
			platform_log("companion: the squad %s", why == AE_COMPANION_HOLD_CINEMATIC ? "waits: a cinematic plays" :
				why == AE_COMPANION_HOLD_LETTERBOX ? "waits: the letterbox is shown" :
				why == AE_COMPANION_HOLD_INPUT ? "waits: the players' input is off" : "is let go (no intro hold)");
			held_why = why;
		}
		if (why != AE_COMPANION_HOLD_NONE)
			ae_squad.hold_ticks++;
	}
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
	ae_squad.members[slot].pickup_ordered = FALSE;
	ae_squad.members[slot].cover_valid = FALSE;
	/* (the next update takes the body as a new one: its look, goal, path, target and stuck watch from now) */
	bots_globals.bots[slot].unit_index = NONE;
	bots_globals.bots[slot].path.valid = FALSE;
}

int bots_squad_indoors(void)
{
	return ae_squad.indoors;
}

int bots_squad_member_cut_off(long slot)
{
	return slot >= 0 && slot < AE_COMPANION_MOST && ae_squad.members[slot].cut_off;
}

int bots_squad_member_fighting(long slot)
{
	struct bots_bot const *bot;

	if (slot < 0 || slot >= AE_COMPANION_MOST)
		return 0;
	bot = &bots_globals.bots[slot];
	return bot->target_player_index != NONE && game_time_get() - bot->target_seen_tick <= TICKS_PER_SECOND / 2;
}

/* the brain's counts since the level's start, for halo.log's line */
void bots_squad_report(char *text, unsigned int size)
{
	snprintf(text, size, "brain: %ld targets found, fight ticks %ld, trigger %ld, grenades %ld, blows %ld, follow ticks %ld, unsticks %ld, "
		"held fire %ld ticks, sidesteps %ld, edges held %ld, jumps held %ld, track jumps %ld, walked fighting %ld ticks, "
		"seated trigger %ld (farthest %.0f), dry ticks %ld, pickups gone for %ld, refill steps %ld, intro hold ticks %ld, spots chosen %ld (none found %ld), team changes %ld, covers taken %ld, cover ticks %ld, "
		"track follow ticks %ld, track cue jumps %ld, crouches %ld",
		ae_squad.targets_found, ae_squad.fight_ticks, ae_squad.trigger_ticks, ae_squad.grenades, ae_squad.blows,
		ae_squad.follow_ticks, ae_squad.unsticks, ae_bots_friendly_fire_held_ticks, ae_squad.sidesteps,
		ae_squad.edges_held, ae_squad.jumps_held, ae_squad.track_jumps, ae_squad.walk_fight_ticks, ae_squad.gunner_trigger_ticks,
		(double)ae_squad.gunner_farthest, ae_squad.empty_ticks, ae_squad.pickups_gone_for, ae_squad.refills,
		ae_squad.hold_ticks, ae_squad_form.choices, ae_squad_form.none_found, ae_squad_team.changes, ae_squad.covers_taken, ae_squad.cover_ticks,
		ae_squad.track_follow_ticks, ae_squad.cue_jumps, ae_squad.cue_crouches);
}
