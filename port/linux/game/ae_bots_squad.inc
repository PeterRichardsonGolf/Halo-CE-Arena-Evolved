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
#include "saved games/game_state.h"

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
		/* AN ORDER'S POINT (ae_bots_squad_order_*): the order it has a place for, the place, walking to it, standing at it */
		long order_serial;
		real_point3d order_place;
		boolean order_walking;
		boolean order_arrived;
		/* (said once an order: no path, walking straight; no path and the line blocked) */
		boolean order_straight;
		boolean order_blocked;
		/* (the point man's stand for the others, in ticks this order) */
		long order_waited;
		/* (what it does under the order, said as it changes; a new order's first tick searches its path at once) */
		int order_state;
		boolean order_fresh;
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
		/* the last jump logged (bot events) */
		long jump_logged;
		/* RUN-UP (ae_bots_squad_runup_phase): since when (0 none), the way at the barrier, the tick it jumped (0 not yet) */
		long runup_since;
		real runup_way;
		long runup_jumped;
		/* JUMP UNDER A LOW CEILING (ae_bots_squad_jump_plan): the tick it jumped (0 none) and its way */
		long jump_tick;
		real jump_way;
		/* JUMP UNDER A LOW CEILING: the tick a crouched walk at a lip began (0 none), where it stood then and its way */
		long creep_since;
		real_point3d creep_at;
		real creep_way;
		/* the walk's goal on the player's track, kept while a newer goal would be behind it (ae_bots_squad_goal_keeps) */
		boolean goal_valid;
		real_point3d goal;
		/* WATCH WHILE MOVING (ae_bots_squad_watch_pick): the yaw of the opening it watches (valid), the next look for one */
		boolean watch_valid;
		real watch_yaw;
		long watch_tick;
		struct player_action action;
		/* A SQUAD WALK (ae_squad_walk_to): its goal, whether it walks, the goal and cluster it last searched for, and until when a
		failed path sends it along the player's track */
		boolean walk_valid;
		boolean walking;
		real_point3d walk_goal;
		real_point3d walk_searched;
		long walk_cluster;
		long walk_failed_until;
		/* PRECEDENCE: since when the other of track and slot is wanted; OSCILLATION: the heading it last turned to, and when */
		long mode_want_since;
		real turned_heading;
		long turned_tick;
		/* (the tick it was last in the player's way, for the bot events' line on what it did) */
		long in_way_tick;
		/* MAKE WAY: the side it steps to and until when it keeps it (no left, right, left as the room either side evens out) */
		int way_side;
		long way_side_until;
		/* THE WARP, LAST (ae_companion_rules.h): the way left at its best, the track crumb it walked to, the last progress */
		real left_best;
		real_point3d crumb;
		long progress_tick;
		real_point3d progress_at;
		/* OUTCOME (ae_bots_squad_rules.h): its ticks stuck and at a lip (a run-up or a crouched walk at one), the tick it last
		saw a lip, and its gaps to the player */
		long stuck_ticks;
		long lip_ticks;
		long lip_tick;
		long gaps[AE_SQUAD_GAP_BUCKETS];
		/* the tick its stay at a lip began, its way to the player not shrinking since (0 none) */
		long lip_since;
		/* the least gap to the player while he stood AE_SQUAD_FORMATION_HALT_TICKS or more (0 none yet) */
		real near_still;
		/* the track crumb it walks to, its least distance to it and since when (a crumb it cannot reach is passed over) */
		real_point3d crumb_goal;
		real crumb_best;
		long crumb_since;
		/* LINE UP (ae_bots_squad_rules.h): the point on the player's line it walks to first, the crumb it lined up for, since when
		(0 not lining up) */
		real_point3d lineup_at;
		real_point3d lineup_crumb;
		long lineup_since;
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
	/* WATCH WHILE MOVING: looks that found an opening to watch */
	long watches;
	/* RUN-UP: run-ups taken at a barrier; JUMP UNDER A LOW CEILING: crouched walks on into a tunnel instead of a jump */
	long runups;
	long creeps;
	/* THE SQUAD'S LOG (ae_bots_squad_rules.h): the counters of its four short lines */
	long stats[AE_SQUAD_STAT_COUNT];
	/* the bsp the walkable surfaces were prepared for, and when */
	short prepared_bsp;
	long prepared_tick;
	/* ONE AT A TIME (ae_bots_squad_rules.h): the member crossing at a jump crumb (-1 none), the crumb, the crumb after it
	(the crossing's far end), since when */
	long crossing;
	real_point3d crossing_at;
	real_point3d crossing_to;
	long crossing_since;
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
static void ae_squad_door_tick(long now);
static void ae_squad_door_forget(void);
static boolean ae_squad_low_line_blocked(real_point3d const *from, real_point3d const *to, real_point3d *hit);

/* once a game tick, before the first member: the walkable surfaces (again on a new BSP of the level), the skill, the searches */
static void ae_squad_paths_service(long now);
static void ae_squad_paths_forget(void);
static boolean ae_squad_paths_mine(void);
static void ae_squad_grid_service(long now);
static void ae_squad_grid_forget(void);
static void ae_squad_slots_service(long now);
static void ae_squad_slots_drop(void);
static boolean ae_squad_slot_of(long member, real_point3d *at);
static boolean ae_squad_slots_pending(void);
static long ae_squad_player_turned_tick(void);
static real ae_squad_player_yaw(void);
static real ae_squad_facing_dot(real yaw, real_point3d const *a, real_point3d const *b);
static void ae_squad_make_way(struct bots_tick *tick, long slot);
static void ae_squad_oscillation_note(struct bots_tick const *tick, long slot);
static void ae_squad_progress_note(struct bots_bot const *bot, struct bots_tick const *tick, long slot);

/* a member's point order: ATTACK, DEFEND, WATCH or MOVE with a point; kind and point may be NULL */
static boolean ae_squad_point_order(long slot, short *kind, real_point3d *point)
{
	long fireteam = slot >= 0 && slot < AE_COMPANION_MOST ? ae_fireteam_of_brain(slot) : NONE;
	short order = fireteam != NONE ? ae_fireteam_order_of(fireteam) : (short)_ae_order_none;
	real_point3d at;

	if (order != _ae_order_attack && order != _ae_order_defend && order != _ae_order_watch && order != _ae_order_go)
		return FALSE;
	if (!ae_fireteam_order_point_of(fireteam, &at))
		return FALSE;
	if (kind)
		*kind = order;
	if (point)
		*point = at;
	return TRUE;
}

/* ON THE LEASH (ae_bots_squad_on_leash): the pull to the player holds only with no point order and no vehicle it was sent to */
static boolean ae_squad_on_leash(long slot)
{
	return (boolean)ae_bots_squad_on_leash(ae_squad_point_order(slot, NULL, NULL), ae_companion_mount_of(slot) != NONE);
}

int bots_squad_member_on_leash(long slot)
{
	return slot >= 0 && slot < AE_COMPANION_MOST && ae_squad_on_leash(slot);
}

static void ae_squad_tick_begin(void)
{
	long now = game_time_get();
	struct structure_bsp *structure;

	if (ae_squad.tick_valid && now == ae_squad.tick)
		return;
	/* (a new game, the game's time back, or the squad back after a pause (a test's level loaded again, a saved point loaded,
	every member gone): the members start again, their own numbers too) */
	if (!ae_squad.tick_valid || now < ae_squad.tick || now - ae_squad.tick > TICKS_PER_SECOND)
	{
		/* (the game's random seed, which the members' own numbers are drawn from: a test run's numbers name it) */
		platform_log("squad seed: %lu", (unsigned long)network_game_get_random_seed());
		csmemset(ae_squad.members, 0, sizeof(ae_squad.members));
		ae_squad.crossing = NONE;
		ae_squad_door_forget();
		/* (the enemy the player called out belongs to the game that ended: its index names another object now) */
		ae_squad_focus.unit_index = NONE;
		ae_squad_focus.until = 0;
	}
	ae_squad.tick = now;
	ae_squad.tick_valid = TRUE;
	structure = global_structure_bsp_get();
	/* (a new bsp, by its index: the walkable surfaces, the grid, the slots and the doors' records prepared again; a revert: the
	grid, slots and walks forgotten) */
	if (game_state_reverted())
	{
		long member;

		ae_squad_grid_forget();
		ae_squad_slots_drop();
		for (member = 0; member < AE_COMPANION_MOST; member++)
		{
			ae_squad.members[member].walk_valid = FALSE;
			ae_squad.members[member].walk_failed_until = 0;
			ae_squad.members[member].progress_tick = 0;
		}
		platform_log("squad: flushed (revert)");
	}
	if (!bots_globals.map_ready || (bots_paths.source && bots_paths.source != structure) ||
		global_structure_bsp_index_get() != ae_squad.prepared_bsp)
	{
		/* (the level moved on to another section: an order's point is behind it, the squad is with the player again) */
		if (ae_squad.prepared_bsp >= 0 && global_structure_bsp_index_get() != ae_squad.prepared_bsp)
		{
			platform_log("squad order: the level moved on: the squad falls in");
			ae_fireteam_campaign_reset();
		}
		ae_squad.prepared_bsp = global_structure_bsp_index_get();
		ae_squad.prepared_tick = now;
		bots_paths_release();
		bots_globals.map_ready = TRUE;
		bots_globals.roam_point_count = 0;
		ae_squad_paths_forget();
		ae_squad_grid_forget();
		ae_squad_slots_drop();
		bots_prepare_paths();
		platform_log("squad paths: prepared bsp %d, %ld surfaces", (int)ae_squad.prepared_bsp,
			structure ? (long)structure->pathfinding_surfaces.count : 0L);
	}
	bots_globals.skill = ae_squad_skill();
	bots_globals.tick_searches = 0;
	bots_paths_scan();
	ae_squad_paths_service(now);
	ae_squad_grid_service(now);
	ae_squad_slots_service(now);
	/* (the squad's one fireteam, its roster and its standing order) */
	ae_fireteam_campaign_tick();
	ae_squad_team_tick(now);
	ae_squad_door_tick(now);
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
		/* (out of view: unless close by, it shot the member, it is the one fought, the player called it out, or the member is a
		seated gunner) */
		if (!ae_bots_squad_foe_in_view(bearing, half_view, distance, hurt, current, focused, gunner))
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
	/* (held by THE EDGE in this check, a squad mate or the player close by (single file, the one ahead stopped), or standing on
	purpose (ONE AT A TIME: waiting its turn at a crossing): no jump) */
	if (now - ae_squad.members[slot].edge_tick <= AE_SQUAD_TRACK_STUCK_TICKS || ae_squad_someone_close(tick, slot) || !tick->moving)
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
			if (ae_bots_events_on())
				platform_log("companion: %ld's jump held at %.2f %.2f %.2f, way %.0f, speed %.2f (%s)", slot + 1, tick->position.x,
					tick->position.y, tick->position.z, way / BOTS_DEGREES, speed, ae_falls_probe_note);
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
		ae_squad_focus.until = game_time_get() + AE_SQUAD_FOCUS_STANDS_SECONDS * TICKS_PER_SECOND;
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

/* WATCH WHILE MOVING (ae_bots_squad_rules.h): indoors out of a fight (on), every AE_SQUAD_WATCH_TICKS, the opening a member
watches: its probes cast level from its eye; its way is towards the player, or for the last member with the player standing back
down the track (away from them); the player's own look and its mates' openings are left to them */
static void ae_squad_watch_update(struct bots_tick const *tick, long slot, long leader, real_point3d const *leader_at, boolean on)
{
	float lengths[AE_SQUAD_WATCH_PROBES], taken[AE_COMPANION_MOST];
	real step = 2.0f * BOTS_PI / (real)AE_SQUAD_WATCH_PROBES, way, player_yaw;
	real_point3d start = tick->position;
	struct unit_datum *unit = leader != NONE ? (struct unit_datum *)object_try_and_get_and_verify_type(leader, _object_mask_unit) : NULL;
	long other, last = slot;
	int probe, count = 0, pick;

	if (!on || !unit)
	{
		ae_squad.members[slot].watch_valid = FALSE;
		return;
	}
	if (tick->now < ae_squad.members[slot].watch_tick && ae_squad.members[slot].watch_tick - tick->now <= AE_SQUAD_WATCH_TICKS)
		return;
	ae_squad.members[slot].watch_tick = tick->now + AE_SQUAD_WATCH_TICKS;
	start.z += 0.6f;
	for (probe = 0; probe < AE_SQUAD_WATCH_PROBES; probe++)
	{
		struct collision_result collision;
		real_vector3d vector;

		vector.i = (real)cos(step * (real)probe) * AE_SQUAD_WATCH_RANGE;
		vector.j = (real)sin(step * (real)probe) * AE_SQUAD_WATCH_RANGE;
		vector.k = 0.0f;
		lengths[probe] = collision_test_vector(FLAG(_collision_test_structure_bit) | FLAG(_collision_test_front_facing_surfaces_bit),
			&start, &vector, NONE, &collision) ? (float)(collision.t * AE_SQUAD_WATCH_RANGE) : AE_SQUAD_WATCH_RANGE;
	}
	for (other = 0; other < AE_COMPANION_MOST; other++)
	{
		if (other == slot || !ae_squad.members[other].valid)
			continue;
		if (other > last)
			last = other;
		if (ae_squad.members[other].watch_valid)
			taken[count++] = (float)ae_squad.members[other].watch_yaw;
	}
	way = (real)atan2(leader_at->y - tick->position.y, leader_at->x - tick->position.x);
	if (last == slot && !ae_squad_leader_moving(leader))
		way += BOTS_PI;
	player_yaw = (real)atan2(unit->unit.aiming_vector.j, unit->unit.aiming_vector.i);
	pick = ae_bots_squad_watch_pick(AE_SQUAD_WATCH_PROBES, lengths, 0.0f, (float)way, (float)player_yaw, taken, count);
	ae_squad.members[slot].watch_valid = (boolean)(pick >= 0);
	if (pick >= 0)
	{
		ae_squad.members[slot].watch_yaw = step * (real)pick;
		ae_squad.watches++;
	}
}

/* a member standing out of a fight: an enemy called out is looked towards, under WATCH the order's point, indoors the opening it
watches */
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
	{
		if (ae_squad.members[slot].watch_valid)
			bot->yaw += PIN(bots_angle_difference(ae_squad.members[slot].watch_yaw, bot->yaw), -turn, turn);
		return;
	}
	if (bots_distance2d(&tick->position, &toward) < 1.0f)
		return;
	bot->yaw += PIN(bots_angle_difference((real)atan2(toward.y - tick->position.y, toward.x - tick->position.x), bot->yaw), -turn, turn);
}

/* the walk with the player: out of a fight it turns to the way it walks, indoors to the opening it watches; in one the fight's
aim and trigger stay on the enemy (it shoots on the move) */
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
		real look = ae_squad.members[slot].watch_valid ? ae_squad.members[slot].watch_yaw : direction;

		bot->yaw += PIN(bots_angle_difference(look, bot->yaw), -turn, turn);
		bot->pitch += PIN(-bot->pitch, -0.05f, 0.05f);
	}
}

/* A SQUAD WALK (ae_squad_walk_to, ae_bots_squad_rules.h): a member's own walk to a goal along the bots' path search. It arrives within
AE_SQUAD_ARRIVE and stands until the goal is AE_SQUAD_SET_OFF away; a path that fails (ae_bots_squad_path_verdict) is never
walked straight at: the walk answers FAILED and the member takes the player's track for AE_SQUAD_WALK_FAILED_TICKS */
enum { AE_SQUAD_WALK_STAND, AE_SQUAD_WALK_GO, AE_SQUAD_WALK_FAILED };
#define AE_SQUAD_WALK_FAILED_TICKS (2 * TICKS_PER_SECOND)

static long ae_squad_point_cluster(real_point3d const *point)
{
	struct location location;

	scenario_location_from_point(&location, point);
	return location.cluster_index;
}

/* the path's verdict against the goal: its length, where it ends, whether the search reached the goal */
static int ae_squad_walk_verdict(struct bots_bot const *bot, real_point3d const *from, real_point3d const *goal, long goal_surface)
{
	real length = 0.0f, end_off;
	real_point3d const *last = from;
	long step;

	/* (no path: it ends where the member stands, the whole way short) */
	if (!bot->path.valid || bot->path.step_count <= 0)
		return ae_bots_squad_path_verdict(0, (float)bots_distance3d(from, goal), 0.0f, (float)bots_distance3d(from, goal),
			goal_surface != NONE);
	for (step = 0; step < bot->path.step_count; step++)
	{
		length += bots_distance3d(last, &bot->path.steps[step].point);
		last = &bot->path.steps[step].point;
	}
	end_off = bots_distance3d(last, goal);
	return ae_bots_squad_path_verdict(!bot->path_short, end_off, length, bots_distance3d(from, goal), goal_surface != NONE);
}

static int ae_squad_walk_to(struct bots_bot *bot, long slot, long unit_index, real_point3d const *position, real_point3d const *goal,
	real *direction)
{
	long now = game_time_get();
	real_point3d ground;
	long goal_surface, cluster;
	real to_goal, moved;

	if (slot < 0 || slot >= AE_COMPANION_MOST)
		return AE_SQUAD_WALK_FAILED;
	if (now < ae_squad.members[slot].walk_failed_until)
		return AE_SQUAD_WALK_FAILED;
	if (!ae_squad.members[slot].walk_valid ||
		ae_bots_squad_goal_moves(ae_bots_squad_goal_shift(goal->x - ae_squad.members[slot].walk_goal.x,
			goal->y - ae_squad.members[slot].walk_goal.y, goal->z - ae_squad.members[slot].walk_goal.z)))
	{
		ae_squad.members[slot].walk_goal = *goal;
		if (!ae_squad.members[slot].walk_valid)
		{
			ae_squad.members[slot].walk_searched.x = ae_squad.members[slot].walk_searched.y = 1.0e6f;
			ae_squad.members[slot].walk_searched.z = 0.0f;
		}
		ae_squad.members[slot].walk_valid = TRUE;
	}
	to_goal = ae_bots_squad_goal_shift(position->x - ae_squad.members[slot].walk_goal.x,
		position->y - ae_squad.members[slot].walk_goal.y, position->z - ae_squad.members[slot].walk_goal.z);
	ae_squad.members[slot].walking = (boolean)ae_bots_squad_walk_state(ae_squad.members[slot].walking, to_goal);
	if (!ae_squad.members[slot].walking)
		return AE_SQUAD_WALK_STAND;
	goal_surface = bots_surface_below(&ae_squad.members[slot].walk_goal, &ground);
	if (goal_surface == NONE)
		ground = ae_squad.members[slot].walk_goal;
	cluster = ae_squad_point_cluster(&ae_squad.members[slot].walk_goal);
	moved = bots_distance3d(&ae_squad.members[slot].walk_searched, &ae_squad.members[slot].walk_goal);
	if (ae_bots_squad_research(moved, cluster != ae_squad.members[slot].walk_cluster) ||
		(!bot->path.valid && now - bot->path_tick > TICKS_PER_SECOND / 3))
	{
		int verdict;
		long searched_tick = bot->path_tick;

		bots_set_goal(bot, _bots_goal_roam, &ground, goal_surface);
		bot->climb_until_tick = 0;
		bots_search(bot, unit_index);
		/* (no search this tick, the tick's searches spent: judged next tick) */
		if (bot->path_tick == searched_tick && !bot->path.valid)
			return AE_SQUAD_WALK_STAND;
		ae_squad.members[slot].walk_searched = ae_squad.members[slot].walk_goal;
		ae_squad.members[slot].walk_cluster = cluster;
		verdict = ae_squad_walk_verdict(bot, position, &ground, goal_surface);
		if (verdict == AE_SQUAD_PATH_FAILED)
		{
			ae_squad.stats[bot->path.valid ? AE_SQUAD_STAT_PARTIAL : AE_SQUAD_STAT_FAILED]++;
			ae_squad.members[slot].walk_failed_until = now + AE_SQUAD_WALK_FAILED_TICKS;
			bot->path.valid = FALSE;
			return AE_SQUAD_WALK_FAILED;
		}
		ae_squad.stats[AE_SQUAD_STAT_REACHED]++;
	}
	if (bot->path.valid)
	{
		while (bot->path.step_index < bot->path.step_count &&
			bots_distance2d(&bot->path.steps[(long)bot->path.step_index].point, position) <= BOTS_STEP_REACHED)
		{
			bot->path.step_index++;
		}
		if (bot->path.step_index < bot->path.step_count)
		{
			struct path_step const *step = &bot->path.steps[(long)bot->path.step_index];

			*direction = (real)atan2(step->point.y - position->y, step->point.x - position->x);
			return AE_SQUAD_WALK_GO;
		}
		bot->path.valid = FALSE;
	}
	/* (the path walked: the last of the way to the goal, a step's length at most, is straight) */
	if (bots_distance2d(&ground, position) <= BOTS_STEP_REACHED * 3.0f)
	{
		*direction = (real)atan2(ground.y - position->y, ground.x - position->x);
		return AE_SQUAD_WALK_GO;
	}
	return AE_SQUAD_WALK_STAND;
}

/* the tether's walk for a campaign squad member out of a fight (ae_bots_fireteam.inc): -1 when the member is not the squad's */
static int ae_squad_walk_tether(struct bots_bot *bot, struct bots_tick *tick, real_point3d const *goal)
{
	long slot = bot - bots_globals.bots;
	real direction = 0.0f;
	real_point3d own;
	int walk;

	if (!ae_squad_paths_mine() || slot < 0 || slot >= AE_COMPANION_MOST)
		return -1;
	if (ae_squad_slot_of(slot, &own))
		goal = &own;
	walk = ae_squad_walk_to(bot, slot, tick->player->unit_index, &tick->position, goal, &direction);
	tick->moving = walk == AE_SQUAD_WALK_GO;
	if (tick->moving)
	{
		real_point3d leader_at;
		long leader = ae_companion_leader(&tick->position, &leader_at);
		real speed = 0.0f;

		if (leader != NONE)
		{
			real_vector3d const *velocity = &object_get(object_get_ultimate_parent(leader))->object.translational_velocity;

			speed = (real)sqrt(velocity->i * velocity->i + velocity->j * velocity->j) * TICKS_PER_SECOND;
		}
		tick->move_direction = direction;
		/* (PACE: the player's own near the slot, a run when far) */
		tick->throttle_scale = ae_bots_squad_throttle((float)speed, AE_SQUAD_PLAYER_RUN, (float)bots_distance3d(&tick->position, goal),
			leader != NONE ? (float)bots_distance3d(&tick->position, &leader_at) : 99.0f);
		if (tick->throttle_scale <= 0.0f)
			tick->moving = FALSE;
	}
	return walk;
}

/* (ae_bots_fireteam.inc) a campaign squad member's leash in a fight: AE_SQUAD_FIGHT_NARROW indoors, else the fireteam's */
static real ae_squad_fight_radius(struct bots_bot *bot, real radius)
{
	long slot = bot - bots_globals.bots;

	if (!ae_squad_paths_mine() || slot < 0 || slot >= AE_COMPANION_MOST)
		return radius;
	return MIN(radius, (real)ae_bots_squad_fight_leash(ae_squad.indoors, ae_squad.indoors));
}

static boolean ae_squad_low_line_blocked(real_point3d const *from, real_point3d const *to, real_point3d *hit);

static long ae_squad_nearest_slot(real_point3d const *point, real *distance);

/* THE POINT MAN of the order (ae_bots_squad_rules.h): the member, the order it was chosen for; NONE with none */
static long ae_squad_order_lead = NONE;
static long ae_squad_order_lead_serial;

/* (the standing report of an order's point: the tick of the last line) */
static long ae_squad_order_said;

static char const *ae_squad_order_name(short kind)
{
	return kind == _ae_order_attack ? "attack" : kind == _ae_order_defend ? "defend" : kind == _ae_order_watch ? "watch" : "move";
}

/* TRUE while member slot has a living body in the game */
static boolean ae_squad_member_lives(long slot)
{
	struct player_datum const *player = bots_globals.bots[slot].player_index != NONE ?
		player_try_and_get(bots_globals.bots[slot].player_index) : NULL;

	return player && player->unit_index != NONE && object_try_and_get(player->unit_index) &&
		!TEST_FLAG(object_get(player->unit_index)->object.damage_flags, _object_dead_bit);
}

/* what a member does under an order, said in the log as it changes (a test and a review read the order's course by it) */
enum { AE_SQUAD_DOES_NOTHING, AE_SQUAD_DOES_WALK, AE_SQUAD_DOES_STAND, AE_SQUAD_DOES_FIGHT, AE_SQUAD_DOES_WAIT, AE_SQUAD_DOES_STRAIGHT,
	AE_SQUAD_DOES_BLOCKED, AE_SQUAD_DOES_SEARCH, AE_SQUAD_DOES_MOUNT, AE_SQUAD_DOES_FETCH, AE_SQUAD_DOES_DOOR, AE_SQUAD_DOES_COVER };

static void ae_squad_order_does(long slot, int does, real to_place)
{
	static char const *const words[] = { "", "walks to its place", "stands at its place", "fights about its place",
		"stands for the others", "walks straight at its place (no path)", "has no way to its place", "waits for a path",
		"goes to a vehicle", "goes for a weapon", "is at a door", "takes cover" };

	if (slot < 0 || slot >= AE_COMPANION_MOST || ae_squad.members[slot].order_state == does)
		return;
	ae_squad.members[slot].order_state = does;
	if (does != AE_SQUAD_DOES_NOTHING)
		platform_log("squad order: %ld %s (%.1f away) at tick %ld", slot + 1, words[does], to_place, game_time_get());
}

/* AN ORDER'S POINT, the member's place at it (ae_bots_squad_order_offset): chosen once an order; a WATCH's is where it stands */
static void ae_squad_order_place(struct bots_tick const *tick, long slot, short kind, real_point3d const *point)
{
	long fireteam = ae_fireteam_of_brain(slot);
	long serial = fireteam != NONE ? ae_fireteam_order_serial_of(fireteam) + 1 : 0;
	real_point3d place = *point, probe, floor;
	float dx = 0.0f, dy = 0.0f;

	if (serial == ae_squad.members[slot].order_serial)
		return;
	ae_squad.members[slot].order_serial = serial;
	ae_squad.members[slot].order_walking = FALSE;
	ae_squad.members[slot].order_arrived = FALSE;
	ae_squad.members[slot].cover_valid = FALSE;
	ae_squad.members[slot].order_straight = FALSE;
	ae_squad.members[slot].order_blocked = FALSE;
	ae_squad.members[slot].walk_failed_until = 0;
	ae_squad.members[slot].order_state = AE_SQUAD_DOES_NOTHING;
	ae_squad.members[slot].order_fresh = TRUE;
	ae_squad.members[slot].walk_valid = FALSE;
	ae_squad.members[slot].order_waited = 0;
	/* (the point man: the living member nearest the point as the order comes) */
	if (ae_squad_order_lead_serial != serial)
	{
		real nearest = 0.0f;

		ae_squad_order_lead_serial = serial;
		ae_squad_order_lead = kind == _ae_order_watch ? NONE : ae_squad_nearest_slot(point, &nearest);
		if (ae_squad_order_lead != NONE)
			platform_log("squad order: %s: %ld is the point man (%.1f from the point)", ae_squad_order_name(kind),
				ae_squad_order_lead + 1, nearest);
	}
	if (kind == _ae_order_watch)
		place = tick->position;
	else
	{
		long other, rank = 0;

		if (bots_surface_below(point, &floor) != NONE)
			place = floor;
		/* (its rank in the wedge: the point man 0, the others in their numbers' order) */
		if (slot != ae_squad_order_lead)
			for (other = 0, rank = 1; other < slot; other++)
				rank += other != ae_squad_order_lead && ae_fireteam_of_brain(other) == fireteam &&
					bots_globals.bots[other].player_index != NONE;
		/* (the way the squad comes: the player's line to the point, one way for every member; a point at the player's own
		feet: the way they face) */
		{
			real_point3d leader_at;
			long leader = ae_companion_leader(&tick->position, &leader_at);
			real way = 0.0f;

			if (leader != NONE && bots_distance2d(&leader_at, point) > 1.0f)
				way = (real)atan2(point->y - leader_at.y, point->x - leader_at.x);
			else if (leader != NONE)
				way = (real)atan2(object_get(leader)->object.forward.j, object_get(leader)->object.forward.i);
			ae_bots_squad_order_offset((int)rank, (float)way, &dx, &dy);
		}
		probe = place;
		probe.x += dx;
		probe.y += dy;
		probe.z += 0.5f;
		if (bots_surface_below(&probe, &floor) != NONE && fabs(floor.z - place.z) <= AE_SQUAD_SPOT_STEP)
			place = floor;
	}
	ae_squad.members[slot].order_place = place;
	platform_log("squad order: %s: %ld's place %.2f %.2f %.2f (%.1f away)", ae_squad_order_name(kind), slot + 1, place.x, place.y,
		place.z, bots_distance3d(&tick->position, &place));
}

/* AN ORDER'S POINT, held (ae_bots_squad_order_step): out of a fight the member walks to its place and stands; in one it fights
inside the order's radius with the fight's own moves and its cover, and walks back in, shooting, from outside it */
static void ae_squad_order_hold(struct bots_bot *bot, struct bots_tick *tick, long slot, short kind)
{
	real_point3d place = ae_squad.members[slot].order_place;
	long lead = ae_squad_order_lead;
	real to_place, direction;
	int step;

	/* (the point man dead or gone: no point man; each member goes to its own place and is at it by itself) */
	if (lead != NONE && lead != slot && (lead < 0 || lead >= AE_COMPANION_MOST || !ae_squad_member_lives(lead)))
		lead = NONE;

	/* (AS ONE: until the point man stands at the point, the others' places are theirs about him as he is; FORMATIONS off:
	each walks to its own place) */
	if (ae_bots_squad_moves_as_one(config_boolean("game.companions_formations")) && lead != NONE && lead != slot && lead < AE_COMPANION_MOST && !ae_squad.members[lead].order_arrived &&
		ae_squad.members[lead].order_serial == ae_squad.members[slot].order_serial && bots_globals.bots[lead].player_index != NONE)
	{
		struct player_datum const *man = player_try_and_get(bots_globals.bots[lead].player_index);

		if (man && man->unit_index != NONE && !TEST_FLAG(object_get(man->unit_index)->object.damage_flags, _object_dead_bit))
		{
			real_point3d at, floor;

			object_get_origin(man->unit_index, &at);
			at.x += place.x - ae_squad.members[lead].order_place.x;
			at.y += place.y - ae_squad.members[lead].order_place.y;
			at.z += 0.3f;
			if (bots_surface_below(&at, &floor) != NONE)
				place = floor;
		}
	}
	/* (a new order: its path is searched this tick, whatever the bot was walking) */
	if (ae_squad.members[slot].order_fresh)
	{
		ae_squad.members[slot].order_fresh = FALSE;
		bot->path.valid = FALSE;
		bot->path_tick = 0;
	}
	to_place = bots_distance2d(&tick->position, &place);
	step = ae_bots_squad_order_step(tick->fighting, (float)to_place, ae_bots_squad_order_radius(kind == _ae_order_attack),
		ae_squad.members[slot].order_walking);
	/* (THE ORDER FIRST: a fight inside the order's radius does not keep a member from its place; it goes there, firing on
	the way, and fights about the place once it has been at it) */
	if (step == AE_SQUAD_ORDER_FIGHT && !ae_squad.members[slot].order_arrived)
		step = to_place > AE_SQUAD_ORDER_NEAR ? AE_SQUAD_ORDER_WALK : AE_SQUAD_ORDER_STAND;
	/* (the point man stands for one strung out behind; FORMATIONS off: he walks on) */
	if (lead == slot && step == AE_SQUAD_ORDER_WALK && ae_bots_squad_moves_as_one(config_boolean("game.companions_formations")))
	{
		real worst = 0.0f;
		long other;

		for (other = 0; other < AE_COMPANION_MOST; other++)
			if (other != slot && ae_squad.members[other].order_serial == ae_squad.members[slot].order_serial &&
				bots_globals.bots[other].player_index != NONE)
			{
				struct player_datum const *mate = player_try_and_get(bots_globals.bots[other].player_index);
				real_point3d at;

				if (!mate || mate->unit_index == NONE || TEST_FLAG(object_get(mate->unit_index)->object.damage_flags, _object_dead_bit) ||
					ae_squad.members[other].order_state == AE_SQUAD_DOES_BLOCKED)
					continue;
				object_get_origin(mate->unit_index, &at);
				worst = MAX(worst, bots_distance2d(&at, &tick->position));
			}
		if (ae_bots_squad_order_waits((float)worst, ae_squad.members[slot].order_waited, tick->fighting))
		{
			if (!ae_squad.members[slot].order_waited)
				platform_log("squad order: the point man %ld stands for the others (%.1f behind)", slot + 1, worst);
			ae_squad.members[slot].order_waited++;
			ae_squad.members[slot].order_walking = FALSE;
			tick->moving = FALSE;
			ae_squad_order_does(slot, AE_SQUAD_DOES_WAIT, to_place);
			return;
		}
	}

	if (step == AE_SQUAD_ORDER_FIGHT)
	{
		ae_squad.members[slot].order_walking = FALSE;
		ae_squad_order_does(slot, AE_SQUAD_DOES_FIGHT, to_place);
		return;
	}
	if (step == AE_SQUAD_ORDER_WALK)
	{
		int walk = ae_squad_walk_to(bot, slot, tick->player->unit_index, &tick->position, &place, &direction);
		boolean straight = FALSE;

		/* (no path found to its place: straight at it while nothing stands in the line, the walk's own jumps and steps
		aside on the way; the path is searched again as it nears) */
		if (walk == AE_SQUAD_WALK_FAILED && !ae_squad_low_line_blocked(&tick->position, &place, NULL))
		{
			direction = (real)atan2(place.y - tick->position.y, place.x - tick->position.x);
			walk = AE_SQUAD_WALK_GO;
			straight = TRUE;
		}
		if (walk == AE_SQUAD_WALK_GO)
		{
			ae_squad.members[slot].order_walking = TRUE;
			ae_squad_walk(bot, tick, slot, direction);
			ae_squad_order_does(slot, straight ? AE_SQUAD_DOES_STRAIGHT : AE_SQUAD_DOES_WALK, to_place);
			return;
		}
		/* (left standing by the walk, or with no way on, near its place: it is at it; farther: said, and tried again) */
		if (to_place <= AE_SQUAD_ORDER_NEAR && !tick->fighting)
			step = AE_SQUAD_ORDER_STAND;
		else
			ae_squad_order_does(slot, walk == AE_SQUAD_WALK_FAILED ? AE_SQUAD_DOES_BLOCKED : AE_SQUAD_DOES_SEARCH, to_place);
	}
	ae_squad.members[slot].order_walking = FALSE;
	if (!tick->fighting)
	{
		bot->goal_kind = _bots_goal_none;
		bot->path.valid = FALSE;
		tick->moving = FALSE;
	}
	/* (every 5 s while the order stands: who is at their place, how far the player is; a test measures the order by it) */
	if (tick->now < ae_squad_order_said || tick->now - ae_squad_order_said >= 5 * TICKS_PER_SECOND)
	{
		long other, at_place = 0, members = 0;
		real_point3d leader_at;
		real away = ae_companion_leader(&tick->position, &leader_at) != NONE ? bots_distance3d(&leader_at, &place) : -1.0f;

		ae_squad_order_said = tick->now;
		for (other = 0; other < AE_COMPANION_MOST; other++)
			if (ae_fireteam_of_brain(other) == ae_fireteam_of_brain(slot) && bots_globals.bots[other].player_index != NONE)
			{
				struct player_datum const *mate = player_try_and_get(bots_globals.bots[other].player_index);
				real_point3d at;

				members++;
				if (!mate || mate->unit_index == NONE || !ae_squad.members[other].order_serial)
					continue;
				object_get_origin(mate->unit_index, &at);
				at_place += bots_distance2d(&at, &ae_squad.members[other].order_place) <= ae_bots_squad_order_radius(kind == _ae_order_attack);
			}
		platform_log("squad order: %s stands: %ld of %ld at their places, the player %.0f away", ae_squad_order_name(kind), at_place, members,
			away);
	}
	if (step == AE_SQUAD_ORDER_STAND)
		ae_squad_order_does(slot, AE_SQUAD_DOES_STAND, to_place);
	/* (at its place only once the point man is at the point: a place about him on the way is not the order's) */
	if (step == AE_SQUAD_ORDER_STAND && !ae_squad.members[slot].order_arrived && (lead == NONE || lead == slot ||
		lead >= AE_COMPANION_MOST || ae_squad.members[lead].order_arrived))
	{
		long other, arrived = 0, members = 0;

		ae_squad.members[slot].order_arrived = TRUE;
		for (other = 0; other < AE_COMPANION_MOST; other++)
			if (ae_fireteam_of_brain(other) == ae_fireteam_of_brain(slot) && bots_globals.bots[other].player_index != NONE)
			{
				members++;
				arrived += ae_squad.members[other].order_arrived && ae_squad.members[other].order_serial == ae_squad.members[slot].order_serial;
			}
		platform_log("squad order: %s: %ld in position (%ld of %ld)", ae_squad_order_name(kind), slot + 1, arrived, members);
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
	/* (an order's point: held by the squad's own rules, off the player's leash) */
	{
		short order_kind;

		if (ae_squad_point_order(slot, &order_kind, NULL) && ae_squad.members[slot].order_serial)
		{
			ae_squad_order_hold(bot, tick, slot, order_kind);
			return;
		}
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
	/* (an order's point by the squad's own walk: no straight walk at a wall; a failed path holds it where it is a while) */
	if (goal && !tick->fighting && bot->goal_kind != _bots_goal_none)
	{
		real_point3d order_goal = bot->goal_point;

		if (ae_squad_walk_to(bot, slot, tick->player->unit_index, &tick->position, &order_goal, &direction) == AE_SQUAD_WALK_GO)
			ae_squad_walk(bot, tick, slot, direction);
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
	/* the player's room (cluster) at tick (none before the first choice) */
	long room;
	boolean valid[AE_COMPANION_MOST];
	real_point3d spot[AE_COMPANION_MOST];
	long choices;
	long none_found;
	/* choices made at once for a new room */
	long room_changes;
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
	long now = game_time_get(), leader = ae_fireteam_leader_player(fireteam), room = NONE;
	struct player_datum const *player = leader != NONE ? player_try_and_get(leader) : NULL;
	boolean room_changed;

	if (!ae_fireteam_campaign_on() || brain < 0 || brain >= AE_COMPANION_MOST)
		return FALSE;
	/* (the player's room: into another one the spots are chosen again at once, as the leader's new room asks of a team) */
	if (player && player->unit_index != NONE)
		room = object_get(player->unit_index)->object.location.cluster_index;
	room_changed = (boolean)ae_bots_squad_room_changed(ae_squad_form.tick < 0 ? NONE : ae_squad_form.room, room);
	if (room_changed || ae_bots_squad_spot_refresh(ae_squad_form.tick < 0 || now < ae_squad_form.tick ? -1 : now - ae_squad_form.tick,
			bots_distance3d(leader_point, &ae_squad_form.at)))
	{
		if (room_changed)
			ae_squad_form.room_changes++;
		ae_squad_formation_choose(fireteam, leader_point, facing, now);
		ae_squad_form.room = room;
	}
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

#include "ae_bots_squad_door.inc"
#include "ae_bots_squad_grid.inc"

/* THE WARP, LAST: a member's progress noted each tick: the way left to its goal (its path's steps left, else straight)
shrinking AE_COMPANION_PROGRESS below its best, or the track crumb it walks to moved on */
static void ae_squad_progress_note(struct bots_bot const *bot, struct bots_tick const *tick, long slot)
{
	real left = 0.0f;
	real_point3d step;
	real_point3d const *from = &tick->position;
	boolean crumb_moved = FALSE, at_lip, patience_over, shrunk;
	long index;

	if (bot->path.valid)
	{
		for (index = bot->path.step_index; index < bot->path.step_count; index++)
		{
			left += bots_distance3d(from, &bot->path.steps[index].point);
			from = &bot->path.steps[index].point;
		}
	}
	else if (ae_squad.members[slot].walk_valid)
		left = bots_distance3d(&tick->position, &ae_squad.members[slot].walk_goal);
	if (ae_squad.members[slot].track_follow && ae_companion_track_goal(&tick->position, &step))
	{
		crumb_moved = (boolean)(bots_distance3d(&step, &ae_squad.members[slot].crumb) > 0.05f);
		ae_squad.members[slot].crumb = step;
	}
	/* (at a lip: run-ups or a crouched walk now or within the last second; past its patience only a shrinking way is progress) */
	at_lip = (boolean)(ae_squad.members[slot].runup_since || (ae_squad.members[slot].lip_tick &&
		tick->now - ae_squad.members[slot].lip_tick <= TICKS_PER_SECOND));
	if (!at_lip || tick->now < ae_squad.members[slot].lip_since)
		ae_squad.members[slot].lip_since = 0;
	else if (!ae_squad.members[slot].lip_since)
		ae_squad.members[slot].lip_since = tick->now > 0 ? tick->now : 1;
	patience_over = (boolean)(ae_squad.members[slot].lip_since &&
		ae_bots_squad_lip_patience_over(tick->now - ae_squad.members[slot].lip_since));
	shrunk = (boolean)ae_companion_progress((float)ae_squad.members[slot].left_best, (float)left, 0, 0);
	if (shrunk)
		ae_squad.members[slot].lip_since = 0;
	if (!ae_squad.members[slot].progress_tick || tick->now < ae_squad.members[slot].progress_tick || shrunk ||
		(!patience_over && (crumb_moved ||
		/* (walking on, not losing ground: a swing to and fro behind a barrier loses it) */
		(ae_companion_walked((float)bots_distance3d(&tick->position, &ae_squad.members[slot].progress_at)) &&
			left <= ae_squad.members[slot].left_best + AE_COMPANION_WALKED) ||
		(!tick->moving && !ae_squad.members[slot].walking))))
	{
		ae_squad.members[slot].progress_tick = tick->now > 0 ? tick->now : 1;
		ae_squad.members[slot].left_best = left;
		ae_squad.members[slot].progress_at = tick->position;
	}
}

/* (ae_companion.c) whether a member's walk to the player failed lately and how long since it made progress */
boolean bots_squad_member_stalled(long slot, long now, long *ticks)
{
	long fireteam;
	short order;

	if (slot < 0 || slot >= AE_COMPANION_MOST || !ae_squad.members[slot].progress_tick)
		return FALSE;
	fireteam = ae_fireteam_of_brain(slot);
	order = fireteam != NONE ? ae_fireteam_order_of(fireteam) : _ae_order_none;
	*ticks = now - ae_squad.members[slot].progress_tick;
	return (boolean)ae_bots_squad_stalled(now < ae_squad.members[slot].walk_failed_until + AE_SQUAD_WALK_FAILED_TICKS,
		ae_squad.members[slot].cut_off, ae_squad.members[slot].track_follow, ae_squad.members[slot].cover_valid,
		ae_squad.members[slot].walk_valid && !ae_squad.members[slot].walking && now >= ae_squad.members[slot].walk_failed_until,
		order != _ae_order_none && order != _ae_order_follow && order != _ae_order_regroup);
}

#define AE_SQUAD_PREPARE_TICKS 2
static long cost_histogram[AE_SQUAD_COST_BUCKETS];

/* (ae_companion.c) a test's setup: the cost's gauges start again, each test's own */
void bots_squad_cost_reset(void)
{
	csmemset(cost_histogram, 0, sizeof(cost_histogram));
	ae_squad.stats[AE_SQUAD_STAT_MS_MAX] = 0;
	ae_squad.stats[AE_SQUAD_STAT_MS_P99] = 0;
	ae_squad.stats[AE_SQUAD_STAT_RAYS] = 0;
}

void bots_squad_note_cost(double milliseconds, long tick)
{
	static long cost_tick = -1;
	static double cost_sum;

	if (tick != cost_tick)
	{
		/* (the tick before counted in the histogram, its 99th percentile in the cost line) */
		if (cost_tick >= 0 && cost_sum > 0.0)
		{
			long bucket = (long)(cost_sum * 10.0);

			cost_histogram[bucket < AE_SQUAD_COST_BUCKETS ? bucket : AE_SQUAD_COST_BUCKETS - 1]++;
			ae_squad.stats[AE_SQUAD_STAT_MS_P99] = ae_bots_squad_cost_p99(cost_histogram);
		}
		cost_tick = tick;
		cost_sum = 0.0;
	}
	/* (the tick the walkable surfaces are prepared, a level's start or a new bsp, is no tick of the walk's) */
	if (tick >= ae_squad.prepared_tick && tick - ae_squad.prepared_tick < AE_SQUAD_PREPARE_TICKS)
		return;
	cost_sum += milliseconds;
	if ((long)(cost_sum * 100.0) > ae_squad.stats[AE_SQUAD_STAT_MS_MAX])
	{
		/* (a new busiest tick of 2 ms or more: logged with what the squad did in it) */
		if (cost_sum >= 2.0 && (long)(cost_sum * 100.0) > ae_squad.stats[AE_SQUAD_STAT_MS_MAX] + 50)
			platform_log("squad cost: %.1f ms at tick %ld (grid %s, slots %s, door %s)", cost_sum, tick,
				ae_squad_grid.building ? "building" : "idle", ae_squad_slots_pending() ? "asked" : "-",
				ae_squad_door.door != NONE ? "on" : "-");
		ae_squad.stats[AE_SQUAD_STAT_MS_MAX] = (long)(cost_sum * 100.0);
	}
}

/* (ae_companion.c) a warp counted by its reason */
void bots_squad_count_warp(int reason)
{
	static const int stats[] = { AE_SQUAD_STAT_WARP_STUCK, AE_SQUAD_STAT_WARP_TELEPORT, AE_SQUAD_STAT_WARP_LIFT,
		AE_SQUAD_STAT_WARP_LOCKED, AE_SQUAD_STAT_WARP_DROP, AE_SQUAD_STAT_WARP_CATCH_UP, AE_SQUAD_STAT_WARP_STRANDED };

	if (reason >= 0 && reason < (int)NUMBEROF(stats))
		ae_squad.stats[stats[reason]]++;
}


/* the room beside a point along yaw at knee height, up to 1 */
static real ae_squad_room_toward(real_point3d const *feet, real yaw)
{
	struct collision_result collision;
	real_point3d start = *feet;
	real_vector3d way;

	start.z += AE_SQUAD_GRID_KNEE;
	way.i = (real)cos(yaw);
	way.j = (real)sin(yaw);
	way.k = 0.0f;
	if (collision_test_vector(AE_SQUAD_GRID_RAYS, &start, &way, NONE, &collision))
		return collision.t;
	return 1.0f;
}

/* YIELD (ae_bots_squad_rules.h): behind a squadmate walking on away the member slows on its own way; one standing in its way,
coming back or crossing it, it backs off from */
static void ae_squad_yield(long slot, struct bots_tick *tick)
{
	long index;

	if (!tick->moving || tick->fighting || tick->object->object.parent_object_index != NONE)
		return;
	for (index = 0; index < slot; index++)
	{
		struct bots_bot const *other = &bots_globals.bots[index];
		struct object_datum const *object;
		int yield;

		if (other->player_index == NONE || other->unit_index == NONE)
			continue;
		object = (struct object_datum const *)object_try_and_get_and_verify_type(other->unit_index, _object_mask_unit);
		if (!object || object->object.parent_object_index != NONE)
			continue;
		yield = ae_bots_squad_yield((float)(object->object.position.x - tick->position.x),
			(float)(object->object.position.y - tick->position.y), (float)(object->object.position.z - tick->position.z),
			(float)tick->move_direction, (float)object->object.translational_velocity.i,
			(float)object->object.translational_velocity.j);
		if (yield == AE_SQUAD_YIELD_SLOW)
		{
			tick->throttle_scale = MIN(tick->throttle_scale, AE_SQUAD_YIELD_THROTTLE);
			return;
		}
		if (yield == AE_SQUAD_YIELD_BACK)
		{
			tick->move_direction = (real)atan2(tick->position.y - object->object.position.y,
				tick->position.x - object->object.position.x);
			tick->throttle_scale = 0.6f;
			return;
		}
	}
}

/* MAKE WAY (ae_bots_squad_rules.h): a member in the player's way steps to the roomier side, or walks on ahead of him */
static void ae_squad_make_way(struct bots_tick *tick, long slot)
{
	real_point3d at;
	long leader = ae_companion_leader(&tick->position, &at);
	real_vector3d const *velocity;
	real way, along, across, speed, dx, dy, direction, pushed = 0.0f, moved = 0.0f;
	int choice;

	if (leader == NONE || object_get_ultimate_parent(leader) != leader)
		return;
	velocity = &object_get(leader)->object.translational_velocity;
	speed = (real)sqrt(velocity->i * velocity->i + velocity->j * velocity->j) * TICKS_PER_SECOND;
	way = (real)atan2(velocity->j, velocity->i);
	/* (his intent, the way his stick pushes him: a player walking into a member stands still, but is in its way) */
	{
		struct unit_datum const *unit = (struct unit_datum const *)object_try_and_get_and_verify_type(leader, _object_mask_unit);

		pushed = unit ? (real)sqrt(unit->unit.throttle.i * unit->unit.throttle.i + unit->unit.throttle.j * unit->unit.throttle.j) : 0.0f;
		moved = speed;
		if (pushed > 0.3f)
		{
			/* (the stick's push is the unit's own: ahead along its facing, left across it) */
			way = (real)atan2(object_get(leader)->object.forward.j, object_get(leader)->object.forward.i) +
				(real)atan2(unit->unit.throttle.j, unit->unit.throttle.i);
			speed = MAX(speed, pushed * AE_SQUAD_PLAYER_RUN);
		}
	}
	dx = tick->position.x - at.x;
	dy = tick->position.y - at.y;
	along = dx * (real)cos(way) + dy * (real)sin(way);
	across = -dx * (real)sin(way) + dy * (real)cos(way);
	if (!ae_bots_squad_in_strip((float)along, (float)across, (float)speed, moved < 0.5f * pushed * AE_SQUAD_PLAYER_RUN))
		return;
	ae_squad.stats[AE_SQUAD_STAT_STRIP]++;
	/* (blocked: the member right ahead and him moving under half what his stick asks) */
	if (along < AE_SQUAD_SLOT_APART && moved < 0.5f * pushed * AE_SQUAD_PLAYER_RUN)
		ae_squad.stats[AE_SQUAD_STAT_BLOCKED]++;
	/* (the side chosen is kept a second, unless the room on that side closed) */
	if (tick->now < ae_squad.members[slot].way_side_until)
	{
		choice = ae_bots_squad_make_way_kept(ae_squad.members[slot].way_side,
			(float)ae_squad_room_toward(&tick->position, way + 1.5708f), (float)ae_squad_room_toward(&tick->position, way - 1.5708f));
		if (choice != ae_squad.members[slot].way_side)
		{
			ae_squad.members[slot].way_side = choice;
			ae_squad.members[slot].way_side_until = tick->now + TICKS_PER_SECOND;
		}
	}
	else
	{
		choice = ae_bots_squad_make_way((float)ae_squad_room_toward(&tick->position, way + 1.5708f),
			(float)ae_squad_room_toward(&tick->position, way - 1.5708f));
		ae_squad.members[slot].way_side = choice;
		ae_squad.members[slot].way_side_until = tick->now + TICKS_PER_SECOND;
	}
	direction = choice == AE_SQUAD_WAY_LEFT ? way + 1.5708f : choice == AE_SQUAD_WAY_RIGHT ? way - 1.5708f : way;
	/* (bot events on: a member in his way, where it stood, how far ahead of him and across his way, how fast he moved of what his
	stick asked, whether he crouched or was in the air, the room on its left and right and the way it takes) */
	if (along < AE_SQUAD_SLOT_APART && moved < 0.5f * pushed * AE_SQUAD_PLAYER_RUN && ae_bots_events_on())
	{
		ae_squad.members[slot].in_way_tick = tick->now;
		struct biped_datum const *biped = (struct biped_datum const *)object_try_and_get_and_verify_type(leader, _object_mask_biped);

		platform_log("companion: %ld in the player's way at %.2f %.2f (%.2f ahead, %.2f across, he moves %.2f of %.2f)%s%s, room %.2f "
			"left %.2f right, %s", slot + 1, tick->position.x, tick->position.y, along, across, moved, pushed * AE_SQUAD_PLAYER_RUN,
			biped && biped->biped.crouch > 0.5f ? ", he crouches" : "",
			biped && biped->biped.support_surface_index == NONE ? ", he is in the air" : "",
			ae_squad_room_toward(&tick->position, way + 1.5708f), ae_squad_room_toward(&tick->position, way - 1.5708f),
			choice == AE_SQUAD_WAY_LEFT ? "steps left" : choice == AE_SQUAD_WAY_RIGHT ? "steps right" : "walks on ahead");
	}
	tick->moving = TRUE;
	tick->move_direction = direction;
	tick->throttle_scale = 1.0f;
}

/* OSCILLATION: the walk turned back on itself within a second out of a fight (counted) */
static void ae_squad_oscillation_note(struct bots_tick const *tick, long slot)
{
	real turn = (real)fabs(bots_angle_difference(tick->move_direction, ae_squad.members[slot].turned_heading));

	/* (a run-up backs off and runs in on purpose, as the member crossing at the player's jump does, and an unstick's step back:
	no oscillation) */
	if (ae_squad.members[slot].runup_since || ae_squad.crossing == slot ||
		tick->now < bots_globals.bots[slot].unstick_until_tick + TICKS_PER_SECOND / 2)
	{
		ae_squad.members[slot].turned_heading = tick->move_direction;
		ae_squad.members[slot].turned_tick = tick->now;
		return;
	}
	if (turn < 0.5f)
		return;
	if (ae_bots_squad_oscillation_after((float)ae_squad.members[slot].turned_heading, (float)tick->move_direction,
			tick->now - ae_squad.members[slot].turned_tick, tick->fighting,
			ae_squad_player_turned_tick() ? tick->now - ae_squad_player_turned_tick() : AE_SQUAD_PLAYER_TURN_TICKS))
	{
		ae_squad.stats[AE_SQUAD_STAT_OSCILLATIONS]++;
		/* (bot events on: where and in what it turned back) */
		if (ae_bots_events_on())
			platform_log("companion: %ld turns back at %.2f %.2f (%.0f to %.0f degrees in %ld ticks)%s%s%s%s", slot + 1, tick->position.x,
				tick->position.y, ae_squad.members[slot].turned_heading / BOTS_DEGREES, tick->move_direction / BOTS_DEGREES,
				tick->now - ae_squad.members[slot].turned_tick, ae_squad.members[slot].track_follow ? ", on the track" : "",
				ae_squad.members[slot].cut_off ? ", cut off" : "", ae_squad.members[slot].walking ? ", walking to a slot" : "",
				ae_squad.crossing == slot ? ", crossing" : "");
	}
	ae_squad.members[slot].turned_heading = tick->move_direction;
	ae_squad.members[slot].turned_tick = tick->now;
}


/* FOLLOW SLOTS (ae_bots_squad_rules.h): under FOLLOW each member's slot about the player, picked on the walkable grid flooded
out from his feet; a member walks to its slot by its own walk (ae_squad_walk_to) */
#define AE_SQUAD_SLOT_REACH (AE_SQUAD_RING_OUT + 1.2f)
#define AE_SQUAD_SLOT_MAN_WEIGHT 0.3f
static struct
{
	boolean valid;
	boolean asked;
	real_point3d planned_at;
	long planned_cluster;
	long planned_tick;
	real yaw;
	real planned_yaw;
	boolean reversal;
	/* the player's heading he last held, and when he last turned back from it */
	real held_yaw;
	long turned_tick;
	long aim_since;
	boolean has[AE_COMPANION_MOST];
	real_point3d slot[AE_COMPANION_MOST];
	int point[AE_COMPANION_MOST];
	/* AUTOMATIC FORMATIONS (ae_bots_squad_rules.h): the formation, the one wanted and since when, the width about the player
	and when it was measured, where and since when he stands */
	int formation;
	int formation_want;
	long formation_want_since;
	real width;
	long width_tick;
	real_point3d still_at;
	long still_since;
} ae_squad_slots;
/* a slot's cost for each unit it lies from the member's formation spot */
#define AE_SQUAD_FORMATION_WEIGHT 1.5f
/* the width about the player: a level ray each side this long, measured every half second at a body's middle */
#define AE_SQUAD_WIDTH_RAY 3.0f
#define AE_SQUAD_WIDTH_HEIGHT 0.5f

/* the squad's formation now (AUTOMATIC FORMATIONS) */
static int ae_squad_formation_now(void)
{
	return ae_squad_slots.formation;
}

/* a member's body's feet, FALSE when it has none */
static boolean ae_squad_member_feet(long member, real_point3d *at)
{
	struct player_datum *player;

	if (member < 0 || member >= AE_COMPANION_MOST || bots_globals.bots[member].player_index == NONE)
		return FALSE;
	player = player_try_and_get(bots_globals.bots[member].player_index);
	if (!player || player->unit_index == NONE || !object_try_and_get(player->unit_index) ||
		TEST_FLAG(object_get(player->unit_index)->object.damage_flags, _object_dead_bit))
	{
		return FALSE;
	}
	object_get_origin(player->unit_index, at);
	return TRUE;
}

/* ONE AT A TIME (ae_bots_squad_rules.h): whether the member crossing still holds the others back: not yet near the
crossing's far end, measured along it from the takeoff, and still by the takeoff */
static boolean ae_squad_crossing_holds(long now)
{
	real_point3d at;
	real dx, dy, length, along;

	if (ae_squad.crossing == NONE || !ae_squad_member_feet(ae_squad.crossing, &at))
		return FALSE;
	dx = ae_squad.crossing_to.x - ae_squad.crossing_at.x;
	dy = ae_squad.crossing_to.y - ae_squad.crossing_at.y;
	length = (real)sqrt(dx * dx + dy * dy);
	along = length > 0.01f ? ((at.x - ae_squad.crossing_at.x) * dx + (at.y - ae_squad.crossing_at.y) * dy) / length : 0.0f;
	return (boolean)ae_bots_squad_crossing_waits(1, (float)along, (float)length,
		(float)bots_distance2d(&at, &ae_squad.crossing_at), now - ae_squad.crossing_since);
}

/* the nearest a straight walk from a to b comes to point p (across) */
static real ae_squad_segment_near(real_point3d const *a, real_point3d const *b, real_point3d const *p)
{
	real dx = b->x - a->x, dy = b->y - a->y, length2 = dx * dx + dy * dy, t = 0.0f, x, y;

	if (length2 > 0.0001f)
		t = PIN(((p->x - a->x) * dx + (p->y - a->y) * dy) / length2, 0.0f, 1.0f);
	x = a->x + dx * t - p->x;
	y = a->y + dy * t - p->y;
	return (real)sqrt(x * x + y * y);
}

/* the dot of the facing with the way from a to b (1 ahead, -1 behind) */
static real ae_squad_facing_dot(real yaw, real_point3d const *a, real_point3d const *b)
{
	real dx = b->x - a->x, dy = b->y - a->y, length = (real)sqrt(dx * dx + dy * dy);

	return length < 0.001f ? 0.0f : (real)((cos(yaw) * dx + sin(yaw) * dy) / length);
}

/* when the player last turned back on his way (0 never), and his heading */
static long ae_squad_player_turned_tick(void)
{
	return ae_squad_slots.turned_tick;
}

static real ae_squad_player_yaw(void)
{
	return ae_squad_slots.yaw;
}

/* whether a slot plan is asked for and not given out yet */
static boolean ae_squad_slots_pending(void)
{
	return ae_squad_slots.asked;
}

/* the member's follow slot, FALSE none */
static boolean ae_squad_slot_of(long member, real_point3d *at)
{
	if (member < 0 || member >= AE_COMPANION_MOST || !ae_squad_slots.valid || !ae_squad_slots.has[member])
		return FALSE;
	*at = ae_squad_slots.slot[member];
	return TRUE;
}

static void ae_squad_slots_drop(void)
{
	long member;

	ae_squad_slots.valid = FALSE;
	ae_squad_slots.asked = FALSE;
	for (member = 0; member < AE_COMPANION_MOST; member++)
	{
		ae_squad_slots.has[member] = FALSE;
		ae_squad_slots.point[member] = -1;
	}
}

/* the plan's slots given out: each grid point a slot may be, its cost to each man, the least total */
static void ae_squad_slots_assign(struct ae_squad_grid_plan const *plan, real_point3d const *leader_at, real_point3d const *lead,
	boolean moving, long cluster)
{
	static float cost[AE_COMPANION_MOST][AE_SQUAD_SLOTS_MAX];
	static float where[AE_SQUAD_SLOTS_MAX][2];
	static long point_cluster[AE_SQUAD_SLOTS_MAX];
	static int in_reach[AE_SQUAD_SLOTS_MAX];
	real_point3d door_at[16];
	real door_reach[16];
	long door_count;
	real_point3d member_at[AE_COMPANION_MOST];
	long members[AE_COMPANION_MOST], men = 0, member, point;
	int kept[AE_COMPANION_MOST], slot_of_man[AE_COMPANION_MOST], man, has_spot[AE_COMPANION_MOST];
	float spot[AE_COMPANION_MOST][2], ring_in, ring_out;

	for (member = 0; member < AE_COMPANION_MOST; member++)
	{
		if (ae_squad_member_feet(member, &member_at[men]))
			members[men++] = member;
	}
	if (men == 0 || plan->count <= 0)
		return;
	/* (the doors near the plan, once: their centres and reaches) */
	{
		struct object_iterator iterator;

		door_count = 0;
		object_iterator_new(&iterator, _object_mask_machine, 0);
		while (object_iterator_next(&iterator) && door_count < (long)NUMBEROF(door_at))
		{
			struct object_datum const *object;

			if (!ae_squad_door_is(iterator.index, NULL))
				continue;
			object = object_get(iterator.index);
			if (bots_distance2d(&object->object.bounding_sphere_center, leader_at) > AE_SQUAD_SLOT_REACH + 3.0f ||
				fabs(object->object.position.z - leader_at->z) > 1.5f)
				continue;
			door_at[door_count] = object->object.bounding_sphere_center;
			door_reach[door_count] = ae_squad_door_reach_of(iterator.index);
			door_count++;
		}
	}
	for (point = 0; point < plan->count; point++)
	{
		long door;

		where[point][0] = (float)plan->points[point].floor.x;
		where[point][1] = (float)plan->points[point].floor.y;
		/* (a point outside the ring is no slot: its room is not looked up) */
		point_cluster[point] = plan->points[point].flood < AE_SQUAD_RING_IN || plan->points[point].flood > AE_SQUAD_RING_OUT ?
			cluster : ae_squad_point_cluster(&plan->points[point].floor);
		/* (DOOR KEEP-OUT: no slot in a door's reach, a body there opens it) */
		in_reach[point] = 0;
		for (door = 0; door < door_count && !in_reach[point]; door++)
			in_reach[point] = ae_bots_squad_in_any_door_reach((float)bots_distance2d(&door_at[door], &plan->points[point].floor),
				(float)door_reach[door], 0);
	}
	ae_bots_squad_formation_ring(ae_squad_slots.formation, &ring_in, &ring_out);
	for (man = 0; man < men; man++)
	{
		float along, across;

		/* (its formation spot: behind him on his heading, to his left or right) */
		has_spot[man] = ae_bots_squad_formation_spot(ae_squad_slots.formation, man < 3 ? (int)man : 2, (float)ae_squad_slots.width,
			&along, &across);
		spot[man][0] = (float)(leader_at->x - cos(ae_squad_slots.yaw) * along - sin(ae_squad_slots.yaw) * across);
		spot[man][1] = (float)(leader_at->y - sin(ae_squad_slots.yaw) * along + cos(ae_squad_slots.yaw) * across);
	}
	for (man = 0; man < men; man++)
	{
		/* (its slot kept: the new plan's point at the old slot's place, within a grid step) */
		kept[man] = -1;
		if (ae_squad_slots.has[members[man]])
		{
			real best = AE_SQUAD_GRID_STEP;

			for (point = 0; point < plan->count; point++)
			{
				real distance = bots_distance3d(&plan->points[point].floor, &ae_squad_slots.slot[members[man]]);

				if (distance <= best)
				{
					best = distance;
					kept[man] = (int)point;
				}
			}
		}
		for (point = 0; point < plan->count; point++)
		{
			struct ae_squad_grid_point const *at = &plan->points[point];
			real off = (real)fabs(at->floor.z - leader_at->z);

			/* (FILE REVERSAL: in a narrow corridor he turned back in, ahead of him is as good: none passes him) */
			if (!ae_bots_squad_slot_ok_ring((float)at->flood,
					ae_squad_slots.reversal ? -1.0f : (float)ae_squad_facing_dot(ae_squad_slots.yaw, leader_at, &at->floor),
					ae_squad_slots.reversal ? -1.0f : (float)ae_squad_facing_dot(ae_squad_slots.yaw, lead, &at->floor),
					(float)ae_squad_segment_near(&member_at[man], &at->floor, leader_at), in_reach[point], moving, ring_in, ring_out))
			{
				cost[man][point] = 1.0e6f;
				continue;
			}
			/* (with a formation spot: its distance from the spot, else from the follow ring's preferred distance; another room or
			floor costs as before) */
			cost[man][point] = (has_spot[man] ?
				ae_bots_squad_slot_cost(AE_SQUAD_RING_PREFER, point_cluster[point] != cluster, off > AE_SQUAD_FLOOR_OFF, 0) +
					AE_SQUAD_FORMATION_WEIGHT * (float)sqrt((at->floor.x - spot[man][0]) * (at->floor.x - spot[man][0]) +
						(at->floor.y - spot[man][1]) * (at->floor.y - spot[man][1])) :
				ae_bots_squad_slot_cost((float)at->flood, point_cluster[point] != cluster, off > AE_SQUAD_FLOOR_OFF, 0)) +
				AE_SQUAD_SLOT_MAN_WEIGHT * (float)bots_distance3d(&member_at[man], &at->floor);
		}
	}
	if (!ae_bots_squad_slot_assign((float const (*)[AE_SQUAD_SLOTS_MAX])cost, (float const (*)[2])where, (int)men,
			(int)plan->count, kept, slot_of_man))
	{
		return;
	}
	for (man = 0; man < men; man++)
	{
		member = members[man];
		if (slot_of_man[man] < 0 || cost[man][slot_of_man[man]] >= 1.0e5f)
		{
			ae_squad_slots.has[member] = FALSE;
			ae_squad_slots.point[member] = -1;
			continue;
		}
		/* (a swap: a slot moved while the player stands; moving, slots go behind him as they should) */
		if (!moving && ae_squad_slots.has[member] && kept[man] != slot_of_man[man] &&
			bots_distance3d(&ae_squad_slots.slot[member], &plan->points[slot_of_man[man]].floor) > AE_SQUAD_SLOT_APART)
			ae_squad.stats[AE_SQUAD_STAT_SWAPS]++;
		ae_squad_slots.has[member] = TRUE;
		ae_squad_slots.point[member] = slot_of_man[man];
		ae_squad_slots.slot[member] = plan->points[slot_of_man[man]].floor;
	}
	ae_squad_slots.valid = TRUE;
}

/* once a tick: the player's facing and lead, whether a slot failed, a new plan asked for and given out */
static void ae_squad_slots_service(long now)
{
	real_point3d member_at, leader_at, lead;
	long leader = NONE, member, cluster, fireteam;
	struct biped_datum *biped;
	real_vector3d const *velocity;
	real aim;
	boolean airborne, moving, invalid = FALSE;
	short order;

	for (member = 0; member < AE_COMPANION_MOST && leader == NONE; member++)
	{
		if (ae_squad_member_feet(member, &member_at))
			leader = ae_companion_leader(&member_at, &leader_at);
	}
	fireteam = ae_fireteam_of_brain(0);
	order = fireteam != NONE ? ae_fireteam_order_of(fireteam) : _ae_order_none;
	if (leader == NONE || object_get_ultimate_parent(leader) != leader || (order != _ae_order_follow && order != _ae_order_none))
	{
		ae_squad_slots_drop();
		return;
	}
	biped = biped_try_and_get(leader);
	airborne = (boolean)(biped && biped->biped.support_surface_index == NONE && biped->biped.elevator_object_index == NONE);
	velocity = &object_get(leader)->object.translational_velocity;
	aim = (real)atan2(object_get(leader)->object.forward.j, object_get(leader)->object.forward.i);
	moving = (boolean)(velocity->i * velocity->i + velocity->j * velocity->j > AE_SQUAD_MOVING * AE_SQUAD_MOVING / 900.0f);
	if (fabs(atan2(sin(aim - ae_squad_slots.yaw), cos(aim - ae_squad_slots.yaw))) <= AE_SQUAD_AIM_TURN)
		ae_squad_slots.aim_since = now;
	/* (velocity is a tick's: a second's is 30 times it) */
	ae_squad_slots.yaw = ae_bots_squad_leader_yaw((float)(velocity->i * TICKS_PER_SECOND), (float)(velocity->j * TICKS_PER_SECOND),
		(float)aim, (float)ae_squad_slots.yaw, (int)(now - ae_squad_slots.aim_since));
	/* (his turn back: a heading more than 120 degrees from the one he held; a held heading follows his slowly) */
	if (moving)
	{
		real from_held = (real)fabs(bots_angle_difference(ae_squad_slots.yaw, ae_squad_slots.held_yaw));

		if (from_held > 2.094f)
		{
			ae_squad_slots.turned_tick = now > 0 ? now : 1;
			ae_squad_slots.held_yaw = ae_squad_slots.yaw;
		}
		else if (from_held < 0.3f)
			ae_squad_slots.held_yaw = ae_squad_slots.yaw;
	}
	lead = leader_at;
	if (!airborne)
	{
		lead.x += velocity->i * TICKS_PER_SECOND * 0.5f;
		lead.y += velocity->j * TICKS_PER_SECOND * 0.5f;
	}
	/* (AUTOMATIC FORMATIONS: the width about him every half second, how long he stands, the pick and its hold; a new formation
	plans the slots again) */
	if (!airborne && (now - ae_squad_slots.width_tick >= TICKS_PER_SECOND / 2 || now < ae_squad_slots.width_tick))
	{
		real_point3d middle = leader_at;

		middle.z += AE_SQUAD_WIDTH_HEIGHT;
		ae_squad_slots.width = ae_squad_door_ray(&middle, ae_squad_slots.yaw + 1.5708f, AE_SQUAD_WIDTH_RAY) +
			ae_squad_door_ray(&middle, ae_squad_slots.yaw - 1.5708f, AE_SQUAD_WIDTH_RAY);
		ae_squad_slots.width_tick = now;
	}
	if (bots_distance2d(&leader_at, &ae_squad_slots.still_at) > 0.3f || now < ae_squad_slots.still_since)
	{
		ae_squad_slots.still_at = leader_at;
		ae_squad_slots.still_since = now;
	}
	{
		boolean fighting = FALSE;
		int want, formation;

		for (member = 0; member < AE_COMPANION_MOST; member++)
			if (bots_squad_member_fighting(member))
				fighting = TRUE;
		want = ae_bots_squad_formation_pick(fighting,
			ae_squad_door.state >= AE_SQUAD_DOOR_GRENADE, ae_squad_door.state == AE_SQUAD_DOOR_STACKING ||
				ae_squad_door.state == AE_SQUAD_DOOR_STACKED,
			now - ae_squad_slots.still_since, (float)ae_squad_slots.width, !ae_squad.indoors);
		/* (FORMATIONS off, game.companions_formations or HALO_SQUAD_FORMATIONS=0: none, the follow ring) */
		want = ae_bots_squad_formation_wanted(config_boolean("game.companions_formations"), want);
		if (want != ae_squad_slots.formation_want || now < ae_squad_slots.formation_want_since)
		{
			ae_squad_slots.formation_want = want;
			ae_squad_slots.formation_want_since = now;
		}
		formation = ae_bots_squad_formation_hold(ae_squad_slots.formation, want, now - ae_squad_slots.formation_want_since);
		if (formation != ae_squad_slots.formation)
		{
			platform_log("squad formation: %s (width %.1f)", ae_bots_squad_formation_name(formation), ae_squad_slots.width);
			ae_squad_slots.formation = formation;
			invalid = TRUE;
		}
	}
	for (member = 0; member < AE_COMPANION_MOST; member++)
	{
		if (ae_squad_slots.has[member] && ae_squad_member_feet(member, &member_at) &&
			!ae_bots_squad_slot_ok((float)bots_distance3d(&leader_at, &ae_squad_slots.slot[member]),
				(float)ae_squad_facing_dot(ae_squad_slots.yaw, &leader_at, &ae_squad_slots.slot[member]),
				(float)ae_squad_facing_dot(ae_squad_slots.yaw, &lead, &ae_squad_slots.slot[member]), AE_SQUAD_PATH_CLEAR, 0, moving))
		{
			invalid = TRUE;
		}
	}
	cluster = ae_squad_point_cluster(&leader_at);
	if (ae_squad_slots.valid && !ae_squad_slots.reversal && ae_bots_squad_file_reversal(
			(float)(fabs(bots_angle_difference(ae_squad_slots.yaw, ae_squad_slots.planned_yaw)) * 57.2958f),
			(float)(ae_squad_room_toward(&leader_at, ae_squad_slots.yaw + 1.5708f) +
				ae_squad_room_toward(&leader_at, ae_squad_slots.yaw - 1.5708f))))
	{
		ae_squad_slots.reversal = TRUE;
		invalid = TRUE;
	}
	else if (ae_squad_slots.reversal && !moving)
		ae_squad_slots.reversal = FALSE;
	if (!ae_squad_slots.asked && (!ae_squad_slots.valid ||
		ae_bots_squad_replan((float)bots_distance3d(&leader_at, &ae_squad_slots.planned_at), cluster != ae_squad_slots.planned_cluster,
			(int)(now - ae_squad_slots.planned_tick), invalid, airborne)))
	{
		if (airborne)
			return;
		{
			float ring_in, ring_out;

			/* (the grid out as far as the formation's ring reaches) */
			ae_bots_squad_formation_ring(ae_squad_slots.formation, &ring_in, &ring_out);
			ae_squad_grid_request(&leader_at, MAX(AE_SQUAD_SLOT_REACH, ring_out + 0.6f));
		}
		ae_squad_slots.asked = TRUE;
		ae_squad_slots.planned_at = leader_at;
		ae_squad_slots.planned_cluster = cluster;
		ae_squad_slots.planned_tick = now;
		ae_squad_slots.planned_yaw = ae_squad_slots.yaw;
		ae_squad.stats[AE_SQUAD_STAT_REPLANS]++;
	}
	if (ae_squad_slots.asked)
	{
		struct ae_squad_grid_plan const *plan = ae_squad_grid_ready();

		if (plan && plan->built_tick >= ae_squad_slots.planned_tick)
		{
			ae_squad_slots.asked = FALSE;
			ae_squad_slots_assign(plan, &leader_at, &lead, moving, cluster);
		}
	}
}


/* THE SQUAD'S PATH DATA (ae_bots_squad_rules.h): each door of the level, and each piece of scenery a failed search learnt in the
way, has a record of the walkable surfaces it closed, so that they open again: a door as the level unlocks it, scenery after
AE_SQUAD_LEARN_EXPIRY_TICKS */
#define AE_SQUAD_CLOSURES 48
#define AE_SQUAD_CLOSURE_SURFACES 96
static struct
{
	struct
	{
		long object_index;
		long expires;
		boolean door;
		boolean closed;
		short surface_count;
		long surfaces[AE_SQUAD_CLOSURE_SURFACES];
	} records[AE_SQUAD_CLOSURES];
	long count;
	long checked;
	boolean report;
} ae_squad_paths;

/* whether the bots' path data is the squad's: it ticked this tick or the last, in a campaign level (no multiplayer engine: a
multiplayer game after a campaign's tick shares its tick numbers) */
static boolean ae_squad_paths_mine(void)
{
	long now = game_time_get();

	return ae_squad.tick_valid && ae_squad.tick <= now && now - ae_squad.tick <= 1 && !game_engine_running();
}

static long ae_squad_paths_record(long object_index, boolean door)
{
	long index;

	for (index = 0; index < ae_squad_paths.count; index++)
	{
		if (ae_squad_paths.records[index].object_index == object_index)
			return index;
	}
	if (ae_squad_paths.count >= AE_SQUAD_CLOSURES)
		return NONE;
	index = ae_squad_paths.count++;
	csmemset(&ae_squad_paths.records[index], 0, sizeof(ae_squad_paths.records[index]));
	ae_squad_paths.records[index].object_index = object_index;
	ae_squad_paths.records[index].door = door;
	return index;
}

static boolean ae_squad_paths_keep_open(long object_index)
{
	boolean locked = FALSE;
	long record;

	if (!ae_squad_paths_mine() || !ae_squad_door_is(object_index, &locked))
		return FALSE;
	record = ae_squad_paths_record(object_index, TRUE);
	if (record == NONE)
		return FALSE;
	ae_squad_paths.records[record].closed = (boolean)ae_bots_squad_door_closes_paths(TRUE, locked);
	return !ae_squad_paths.records[record].closed;
}

static boolean ae_squad_paths_may_learn(long object_index)
{
	struct object_datum const *object;
	long expires = 0, record;

	if (!ae_squad_paths_mine())
		return TRUE;
	object = object_get(object_index);
	if (!ae_bots_squad_learn(object->object.type == _object_type_machine, game_time_get(), &expires))
		return FALSE;
	record = ae_squad_paths_record(object_index, FALSE);
	if (record == NONE)
		return FALSE;
	ae_squad_paths.records[record].closed = TRUE;
	ae_squad_paths.records[record].expires = expires;
	ae_squad_paths.records[record].surface_count = 0;
	return TRUE;
}

static void ae_squad_paths_closed(long object_index, long surface_index)
{
	long index;

	if (!ae_squad_paths_mine())
		return;
	for (index = 0; index < ae_squad_paths.count; index++)
	{
		if (ae_squad_paths.records[index].object_index == object_index &&
			ae_squad_paths.records[index].surface_count < AE_SQUAD_CLOSURE_SURFACES)
		{
			ae_squad_paths.records[index].surfaces[ae_squad_paths.records[index].surface_count++] = surface_index;
			return;
		}
	}
}

/* a record's surfaces open to the searches again, its object off the learnt list */
static void ae_squad_paths_open(long record)
{
	long index;

	for (index = 0; index < ae_squad_paths.records[record].surface_count; index++)
	{
		long surface_index = ae_squad_paths.records[record].surfaces[index];

		if (bots_paths.walkable && surface_index >= 0 && surface_index < bots_paths.structure.pathfinding_surfaces.count)
			bots_paths.walkable[surface_index] |= (byte)FLAG(BOTS_PATHFINDING_WALKABLE_BIT);
	}
	for (index = 0; index < bots_paths.learned_count; index++)
	{
		if (bots_paths.learned[index] == ae_squad_paths.records[record].object_index)
		{
			bots_paths.learned[index] = bots_paths.learned[--bots_paths.learned_count];
			break;
		}
	}
	ae_squad_paths.records[record].surface_count = 0;
	ae_squad_paths.records[record].closed = FALSE;
}

/* once a second: each record's door locked or unlocked, scenery's time up */
static void ae_squad_paths_service(long now)
{
	long index, open_doors = 0, shut_doors = 0, changed = 0;

	if (now - ae_squad_paths.checked < TICKS_PER_SECOND && now >= ae_squad_paths.checked)
		return;
	ae_squad_paths.checked = now;
	for (index = 0; index < ae_squad_paths.count; index++)
	{
		boolean locked = FALSE;
		int due;

		/* (its object gone, a script's scenery deleted: its surfaces open again, its record freed) */
		if (!object_try_and_get(ae_squad_paths.records[index].object_index))
		{
			if (ae_squad_paths.records[index].closed)
				ae_squad_paths_open(index);
			ae_squad_paths.records[index] = ae_squad_paths.records[--ae_squad_paths.count];
			index--;
			changed++;
			continue;
		}
		if (ae_squad_paths.records[index].door)
			(void)ae_squad_door_is(ae_squad_paths.records[index].object_index, &locked);
		due = ae_bots_squad_closure_due(ae_squad_paths.records[index].door, locked, ae_squad_paths.records[index].closed,
			ae_squad_paths.records[index].expires, now);
		if (due < 0)
		{
			ae_squad_paths_open(index);
			changed++;
		}
		else if (due > 0)
		{
			ae_squad_paths.records[index].closed = TRUE;
			ae_squad_paths.records[index].surface_count = 0;
			(void)bots_paths_close_object(ae_squad_paths.records[index].object_index, "a door the level shut:", 0.0f);
			changed++;
		}
		if (ae_squad_paths.records[index].door)
		{
			open_doors += !ae_squad_paths.records[index].closed;
			shut_doors += ae_squad_paths.records[index].closed;
		}
	}
	if (changed || ae_squad_paths.report)
		platform_log("squad paths: %ld doors open to searches, %ld closed (locked or unpowered), %ld changed", open_doors, shut_doors,
			changed);
	ae_squad_paths.report = FALSE;
}

static void ae_squad_paths_forget(void)
{
	ae_squad_paths.count = 0;
	ae_squad_paths.checked = 0;
	ae_squad_paths.report = TRUE;
}


/* MOUNT: every living member within AE_SQUAD_SMART_MOUNT_REACH is sent to the vehicle (the gunner's seat goes to the first to
come, the passenger seats to the next); how many were sent */
static long ae_squad_mount_all(long vehicle, real_point3d const *vehicle_at)
{
	long slot, sent = 0;

	for (slot = 0; slot < AE_COMPANION_MOST; slot++)
	{
		struct bots_bot const *bot = &bots_globals.bots[slot];
		struct player_datum const *player = bot->player_index != NONE ? player_try_and_get(bot->player_index) : NULL;
		real_point3d at;
		real away;

		if (!player || player->unit_index == NONE || TEST_FLAG(object_get(player->unit_index)->object.damage_flags, _object_dead_bit))
			continue;
		object_get_origin(player->unit_index, &at);
		away = bots_distance3d(&at, vehicle_at);
		if (!ae_bots_squad_smart_in_reach(AE_SQUAD_SMART_MOUNT, (float)away) || !ae_companion_mount(slot, vehicle))
			continue;
		sent++;
		platform_log("squad order: mount: %ld is sent to the vehicle (%.1f away)", slot + 1, away);
	}
	return sent;
}

/* the vehicle with a seat for a member nearest a point, within reach of it; NONE with none */
static long ae_squad_vehicle_near(real_point3d const *point, real reach)
{
	struct object_iterator iterator;
	long best = NONE;
	real best_away = reach;

	object_iterator_new(&iterator, _object_mask_vehicle, 0);
	while (object_iterator_next(&iterator))
	{
		struct object_datum const *vehicle = object_get(iterator.index);
		real away = bots_distance3d(&vehicle->object.position, point);
		/* (a dropship is boarded only when the player points at it: never the nearest vehicle's pick) */
		char const *name = tag_get_name(vehicle->definition_index);

		if (name && (strstr(name, "pelican") || strstr(name, "dropship") || strstr(name, "lifepod")))
			continue;
		if (away <= best_away && ae_companion_has_free_seat(iterator.index))
		{
			best = iterator.index;
			best_away = away;
		}
	}
	return best;
}

/* THE CAMPAIGN SQUAD'S WHEEL (the squad is off the player's leash under every order but FALL IN): FALL IN (the wheel's REGROUP)
ends the order and every vehicle a member was sent to: follow; MOUNT (the wheel's RIDE) sends the squad to the vehicle called
(the one marked, else the caller's own, else the nearest with a seat), and with a member seated or sent already it gets the squad
out; ATTACK, DEFEND, WATCH and MOVE let go of a vehicle a member was sent to. TRUE when the call was all dealt with here */
static boolean ae_squad_wheel(struct player_datum const *marker, long object_index, real_point3d const *spot, short wheel_entry, long led)
{
	long slot, vehicle = NONE, sent_or_seated = 0;

	for (slot = 0; slot < AE_COMPANION_MOST; slot++)
	{
		struct bots_bot const *bot = &bots_globals.bots[slot];
		struct player_datum const *player = bot->player_index != NONE ? player_try_and_get(bot->player_index) : NULL;

		if (ae_companion_mount_of(slot) != NONE || (player && player->unit_index != NONE &&
			object_get(player->unit_index)->object.parent_object_index != NONE))
			sent_or_seated++;
	}
	if (wheel_entry == AE_WHEEL_REGROUP)
	{
		for (slot = 0; slot < AE_COMPANION_MOST; slot++)
			ae_companion_mount(slot, NONE);
		ae_companion_board_again();
		ae_fireteam_order((int)led, _ae_order_follow, NULL, NONE);
		platform_log("squad order: fall in: the squad follows again");
		return TRUE;
	}
	if (wheel_entry == AE_WHEEL_RIDE)
	{
		if (sent_or_seated)
		{
			for (slot = 0; slot < AE_COMPANION_MOST; slot++)
				ae_companion_mount(slot, NONE);
			ae_companion_dismount();
			platform_log("squad order: mount: the squad gets out (%ld seated or sent)", sent_or_seated);
			return TRUE;
		}
		if (object_index != NONE && ae_companion_has_free_seat(object_index))
			vehicle = object_index;
		else if (marker->unit_index != NONE && object_get(marker->unit_index)->object.parent_object_index != NONE &&
			ae_companion_has_free_seat(object_get_ultimate_parent(marker->unit_index)))
			vehicle = object_get_ultimate_parent(marker->unit_index);
		else
			vehicle = ae_squad_vehicle_near(spot, AE_SQUAD_SMART_MOUNT_REACH);
		if (vehicle == NONE || !ae_squad_mount_all(vehicle, &object_get(vehicle)->object.position))
			platform_log("squad order: mount: no vehicle with a seat in reach");
		return TRUE;
	}
	if (wheel_entry == AE_WHEEL_ATTACK || wheel_entry == AE_WHEEL_DEFEND || wheel_entry == AE_WHEEL_WATCH || wheel_entry == AE_WHEEL_GOING)
		for (slot = 0; slot < AE_COMPANION_MOST; slot++)
			ae_companion_mount(slot, NONE);
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

	/* (DOOR STACK: a place tapped at a door, or the door itself: DEFEND there, which stacks the squad beside it) */
	if (pick == AE_SQUAD_SMART_MOVE)
	{
		long door = object && ae_squad_door_is(object_index, NULL) ? object_index : ae_squad_door_near(spot, AE_SQUAD_DOOR_NEAR, FALSE);

		if (door != NONE)
		{
			real_point3d centre = object_get(door)->object.bounding_sphere_center;

			ae_fireteam_order((int)led, _ae_order_defend, &centre, NONE);
			platform_log("ae squad: SMART at a door: stack up");
			return TRUE;
		}
	}
	if (pick == AE_SQUAD_SMART_FOCUS || pick == AE_SQUAD_SMART_MOVE)
	{
		/* (an object the squad has no use for, a vehicle with no seat left, a weapon held or empty: said; the tap is a place's) */
		if (pick == AE_SQUAD_SMART_MOVE && object)
			platform_log("ae squad: SMART on %s: no seat and no weapon for the squad there, a move to the place",
				tag_get_name(object->definition_index));
		return FALSE;
	}
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
		if (!ae_squad_mount_all(object_index, &object->object.position))
		{
			platform_log("ae squad: SMART on a vehicle: no seat for a member");
			return TRUE;
		}
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

	/* (under FOLLOW about the player; under an order's point about the member's place there) */
	boolean at_order = ae_squad_point_order(slot, NULL, NULL);

	if (!tick->fighting || !foe || fireteam == NONE || (ae_fireteam_order_of(fireteam) != _ae_order_follow && !at_order))
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
						(!at_order && bots_distance2d(&floor, leader_at) < AE_SQUAD_SPOT_LEADER_GAP) || ae_squad_cover_crowded(slot, &floor) ||
						ae_squad_in_door_reach(&floor, 0.0f, NONE, NULL) || fabs(floor.z - leader_at->z) > AE_SQUAD_FLOOR_OFF)
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
	/* (UPTURNED: no seat to take; beside it the member rights it as a player does, the action held on the game's own
	flip prompt, and boards once it stands on its wheels) */
	if (object->object.up.k <= AE_SQUAD_MOUNT_UPRIGHT && bots_distance2d(&object->object.position, &tick->position) < AE_SQUAD_MOUNT_FLIP)
	{
		static long pressed[AE_COMPANION_MOST], said[AE_COMPANION_MOST];

		if (tick->player->action_result == _player_action_result_flip_vehicle && tick->player->action_object_index == vehicle)
		{
			tick->moving = FALSE;
			if (slot >= 0 && slot < AE_COMPANION_MOST && pressed[slot] != tick->now - 1)
			{
				tick->flags |= FLAG(_unit_control_action_bit);
				pressed[slot] = tick->now;
				if (tick->now - said[slot] > 5 * TICKS_PER_SECOND || tick->now < said[slot])
				{
					said[slot] = tick->now;
					platform_log("squad order: mount: %ld flips the vehicle over", slot + 1);
				}
			}
			return TRUE;
		}
		tick->move_direction = (real)atan2(object->object.position.y - tick->position.y, object->object.position.x - tick->position.x);
		tick->moving = TRUE;
		tick->throttle_scale = 0.6f;
		return TRUE;
	}
	if (object->object.up.k > AE_SQUAD_MOUNT_UPRIGHT && bots_distance3d(&object->object.position, &tick->position) < AE_SQUAD_MOUNT_ENTER)
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
	{
		static long said[AE_COMPANION_MOST];

		/* (why the gun is quiet with a target, once in two seconds) */
		if (tick->now - said[slot] >= TICKS_PER_SECOND * 2 || tick->now < said[slot])
		{
			said[slot] = tick->now;
			platform_log("companion: %ld's gun holds at %.1f units: aim %.0f and %.0f degrees off (cone %.0f), seen %ld ticks ago, "
				"for %ld%s", slot + 1, distance,
				fabs(bots_angle_difference((real)atan2(dy, dx), bot->yaw)) / BOTS_DEGREES,
				fabs((real)atan2(dz, horizontal) - bot->pitch) / BOTS_DEGREES, ae_bots_squad_gunner_cone(distance) / BOTS_DEGREES,
				(long)(tick->now - bot->target_seen_tick), (long)(tick->now - bot->target_first_seen_tick),
				ae_bots_squad_gunner_holds(bots_angle_difference((real)atan2(dy, dx), bot->yaw),
					(real)atan2(dz, horizontal) - bot->pitch, distance, tick->now - bot->target_seen_tick,
					tick->now - bot->target_first_seen_tick) ? ", a teammate in the line" : "");
		}
		return;
	}
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
	boolean walk_failed = (boolean)(tick->now < ae_squad.members[slot].walk_failed_until);
	real_point3d own;
	boolean has_slot = ae_squad_slot_of(slot, &own);
	/* (indoors the follow slot is walked to where one stands; the track when none does, the squad is in a file (a narrow way),
	the walk to it failed, or the slot is a storey above or below the member: the player's own way there, his drop or his jump,
	is the way) */
	boolean on = (boolean)ae_bots_squad_track_follows(walk_failed || (has_slot && fabs(own.z - tick->position.z) > AE_SQUAD_FLOOR_OFF) ||
		((!has_slot || ae_squad_formation_now() == AE_SQUAD_FORMATION_FILE) && ae_squad_player_indoors(tick->now, leader_at)),
		ae_companion_track_goal(&tick->position, &step), order == _ae_order_follow || order == _ae_order_none);

	/* (a change between the track and the slot waits until it was wanted AE_SQUAD_MODE_HOLD_TICKS: no flip each tick) */
	if (on == ae_squad.members[slot].track_follow)
		ae_squad.members[slot].mode_want_since = tick->now;
	else if (ae_bots_squad_mode_pick(on ? AE_SQUAD_MODE_TRACK : AE_SQUAD_MODE_SLOT,
			ae_squad.members[slot].track_follow ? AE_SQUAD_MODE_TRACK : AE_SQUAD_MODE_SLOT,
			tick->now - ae_squad.members[slot].mode_want_since) != (on ? AE_SQUAD_MODE_TRACK : AE_SQUAD_MODE_SLOT))
		on = ae_squad.members[slot].track_follow;
	else
		ae_squad.stats[AE_SQUAD_STAT_MODES]++;
	if (on && walk_failed)
		ae_squad.stats[AE_SQUAD_STAT_TRACK]++;
	if (on != ae_squad.members[slot].track_follow)
	{
		ae_squad.members[slot].track_follow = on;
		platform_log("companion: %ld %s", slot + 1, on ? "walks the player's track (indoors)" : "leaves the player's track");
	}
	return on;
}

/* RUN-UP (ae_bots_squad_rules.h): a jump at a barrier from a stand (slower than AE_SQUAD_RUNUP_SPEED) is not made; the member
backs off, runs at it and jumps on the move, pressing on and crouching in the air; not on an object's or narrow ground (a step
back there may be a fall) */
/* the ceiling's height above the feet at a point ahead along way (0: the point itself), up to 2 */
static real ae_squad_ceiling(real_point3d const *feet, real way, real ahead)
{
	struct collision_result collision;
	real_point3d start = *feet;
	real_vector3d up;

	up.i = 0.0f;
	up.j = 0.0f;
	up.k = 1.9f;

	start.x += (real)cos(way) * ahead;
	start.y += (real)sin(way) * ahead;
	start.z += 0.1f;
	if (!collision_test_vector(FLAG(_collision_test_structure_bit) | FLAG(_collision_test_front_facing_surfaces_bit) |
			FLAG(_collision_test_back_facing_surfaces_bit), &start, &up, NONE, &collision))
		return 2.0f;
	return 0.1f + collision.t * 1.9f;
}

/* JUMP UNDER A LOW CEILING (ae_bots_squad_rules.h): after the run-up, the jump it makes is held while a tunnel's lip is ahead (a
lower ceiling there: it walks on into the tunnel crouched), made once it is under the ceiling, and pressed on crouched in the air */
static void ae_squad_jump_gate(struct bots_tick *tick, long slot)
{
	long since = ae_squad.members[slot].jump_tick ? tick->now - ae_squad.members[slot].jump_tick : -1;
	boolean want = (boolean)TEST_FLAG(tick->flags, _unit_control_jump_bit);
	boolean lip = (boolean)(want && ae_bots_squad_ceiling_lip((float)ae_squad_ceiling(&tick->position, tick->move_direction, 0.0f),
		(float)ae_squad_ceiling(&tick->position, tick->move_direction, AE_SQUAD_JUMP_LIP_AHEAD)));
	long creeping = ae_squad.members[slot].creep_since && tick->now >= ae_squad.members[slot].creep_since ?
		tick->now - ae_squad.members[slot].creep_since : -1;
	real gain = (real)(cos(ae_squad.members[slot].creep_way) * (tick->position.x - ae_squad.members[slot].creep_at.x) +
		sin(ae_squad.members[slot].creep_way) * (tick->position.y - ae_squad.members[slot].creep_at.y));

	/* (a crouched walk at a lip: one that gains along its way is a tunnel's and its watch starts again (a run-up's back-off and
	run gain nothing); one that gains nothing is given up and the jump goes) */
	if (lip)
	{
		ae_squad.members[slot].lip_tick = tick->now;
		if (creeping < 0 || gain >= AE_SQUAD_CREEP_GAIN)
		{
			ae_squad.members[slot].creep_since = tick->now > 0 ? tick->now : 1;
			ae_squad.members[slot].creep_at = tick->position;
			ae_squad.members[slot].creep_way = tick->move_direction;
			creeping = 0;
			gain = 0.0f;
		}
		if (ae_bots_squad_creep_gave_up(creeping, (float)gain))
			lip = FALSE;
	}
	switch (ae_bots_squad_jump_plan(want, lip, since))
	{
	case AE_SQUAD_JUMP_CREEP:
		tick->flags &= ~FLAG(_unit_control_jump_bit);
		tick->flags |= FLAG(_unit_control_crouch_modifier_bit);
		tick->moving = TRUE;
		tick->throttle_scale = 1.0f;
		ae_squad.members[slot].runup_since = 0;
		ae_squad.creeps++;
		break;
	case AE_SQUAD_JUMP_GO:
		ae_squad.members[slot].creep_since = 0;
		ae_squad.members[slot].jump_tick = tick->now > 0 ? tick->now : 1;
		ae_squad.members[slot].jump_way = tick->move_direction;
		tick->flags &= ~FLAG(_unit_control_crouch_modifier_bit);
		break;
	case AE_SQUAD_JUMP_AIR:
		tick->flags &= ~FLAG(_unit_control_jump_bit);
		tick->flags |= FLAG(_unit_control_crouch_modifier_bit);
		tick->moving = TRUE;
		tick->throttle_scale = 1.0f;
		tick->move_direction = ae_squad.members[slot].jump_way;
		break;
	default:
		if (since > AE_SQUAD_JUMP_CROUCH_TICKS)
			ae_squad.members[slot].jump_tick = 0;
		break;
	}
}

static void ae_squad_runup(struct bots_tick *tick, long slot, boolean narrow)
{
	real_vector3d const *velocity = &tick->object->object.translational_velocity;
	real speed = (real)sqrt(velocity->i * velocity->i + velocity->j * velocity->j) * (real)TICKS_PER_SECOND;
	long since = ae_squad.members[slot].runup_since ? tick->now - ae_squad.members[slot].runup_since : -1;
	int phase = ae_bots_squad_runup_phase(since);

	if (phase == AE_SQUAD_RUNUP_DONE)
	{
		ae_squad.members[slot].runup_since = 0;
		/* (under a low ceiling, a doorway's tunnel, no run-up: the jump is made where it stands, crouched in the air; but the one
		member crossing at the player's jump (ONE AT A TIME) has the room behind it and runs up) */
		if (!TEST_FLAG(tick->flags, _unit_control_jump_bit) || speed >= AE_SQUAD_RUNUP_SPEED ||
			tick->object->object.parent_object_index != NONE ||
			((narrow || ae_squad_ceiling(&tick->position, 0.0f, 0.0f) < AE_SQUAD_LOW_CEILING) && ae_squad.crossing != slot))
		{
			/* (never crouched on the tick it jumps: the crouch comes in the air) */
			if (TEST_FLAG(tick->flags, _unit_control_jump_bit))
				tick->flags &= ~FLAG(_unit_control_crouch_modifier_bit);
			return;
		}
		ae_squad.members[slot].runup_since = tick->now > 0 ? tick->now : 1;
		ae_squad.members[slot].runup_way = tick->move_direction;
		ae_squad.members[slot].runup_jumped = 0;
		ae_squad.runups++;
		since = 0;
		phase = AE_SQUAD_RUNUP_BACK;
	}
	tick->flags &= ~FLAG(_unit_control_jump_bit);
	tick->moving = TRUE;
	tick->throttle_scale = 1.0f;
	if (phase == AE_SQUAD_RUNUP_BACK)
	{
		tick->move_direction = ae_squad.members[slot].runup_way + BOTS_PI;
		tick->flags &= ~FLAG(_unit_control_crouch_modifier_bit);
		return;
	}
	tick->move_direction = ae_squad.members[slot].runup_way;
	if (!ae_squad.members[slot].runup_jumped)
	{
		tick->flags &= ~FLAG(_unit_control_crouch_modifier_bit);
		if (ae_bots_squad_runup_jump((float)speed, since - AE_SQUAD_RUNUP_BACK_TICKS))
		{
			tick->flags |= FLAG(_unit_control_jump_bit);
			ae_squad.members[slot].runup_jumped = tick->now > 0 ? tick->now : 1;
		}
		return;
	}
	/* (in the air: pressing on, crouched from the third tick) */
	if (tick->now - ae_squad.members[slot].runup_jumped >= 3)
		tick->flags |= FLAG(_unit_control_crouch_modifier_bit);
}

/* a member's tick: what it knows of itself, and the controls it builds */
static void ae_squad_update(struct bots_bot *bot, long slot, struct player_action *action)
{
	struct bots_tick tick;
	real_point3d leader_at;
	long leader;
	boolean have_leader, in_vehicle, deck, narrow, keep_pace, track_follow, door_moved = FALSE, leashed, point_order;
	short order_kind = _ae_order_none;
	real_point3d order_point;
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
	point_order = ae_squad_point_order(slot, &order_kind, &order_point);
	if (point_order)
		ae_squad_order_place(&tick, slot, order_kind, &order_point);
	else
		ae_squad.members[slot].order_serial = 0;

	leader = ae_companion_leader(&tick.position, &leader_at);
	have_leader = (boolean)(leader != NONE);
	if (have_leader)
		distance = bots_distance3d(&tick.position, &leader_at);

	/* (ON A DECK: the player or this member on an object's ground, a lift, or on narrow ground, a bridge deck: a path there
	finds the ground under the object, or a step aside is a step off; the member walks the player's own track in single
	file, never strafes, steps aside or unsticks sideways) */
	leashed = ae_squad_on_leash(slot);
	deck = (boolean)(have_leader && !in_vehicle && leashed &&
		(ae_companion_on_deck(leader) || ae_companion_on_deck(tick.player->unit_index)));
	narrow = deck;
	/* (CUT OFF: a barrier between it and the player, the pipes of a doorway the player jumped: it walks the player's track over it,
	as on a deck; the formation's spot behind the player is on the barrier's far side) */
	if (have_leader)
		ae_squad_player_indoors(tick.now, &leader_at);
	/* (off the leash, under an order's point or sent to a vehicle, a barrier between it and the player is nothing to it) */
	if (have_leader && !in_vehicle && leashed)
		ae_squad_cut_off_check(&tick, slot, &leader_at, distance);
	else if (ae_squad.members[slot].cut_off && !leashed)
	{
		ae_squad.members[slot].cut_off = FALSE;
		platform_log("companion: %ld is off the player's leash: the barrier between them is left", slot + 1);
	}
	if (ae_squad.members[slot].cut_off)
		deck = TRUE;
	/* (held a while after it was last seen: a bridge's far end, a wider piece of it, does not flip it off and on) */
	if (!in_vehicle && have_leader && leashed)
		deck = (boolean)ae_bots_squad_deck_held(deck, tick.now, &ae_squad.members[slot].deck_until);
	if (deck != ae_squad.members[slot].deck)
	{
		ae_squad.members[slot].deck = deck;
		platform_log("companion: %ld %s%s", slot + 1, deck ? "follows the player on an object or narrow ground, on their track" : "is off the deck",
			!deck ? "" : narrow ? " (an object's or narrow ground)" : " (cut off by a barrier)");
	}
	/* (TRACK FOLLOW: indoors, off a deck, it walks the player's track in single file instead of going to a formation spot) */
	track_follow = (boolean)(have_leader && !in_vehicle && !deck && leashed && ae_squad_track_follow_on(&tick, slot, &leader_at));
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
	/* (WATCH WHILE MOVING: indoors, out of a fight, off a deck and out of a seat) */
	ae_squad_watch_update(&tick, slot, have_leader ? leader : NONE, &leader_at,
		(boolean)(have_leader && !in_vehicle && !deck && ae_squad.indoors && !tick.fighting));
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
	{
		ae_squad.members[slot].track_walked = FALSE;
		if (point_order)
			ae_squad_order_does(slot, AE_SQUAD_DOES_MOUNT, 0.0f);
	}
	/* (no fetch on a deck: a step off it for a weapon is a fall) */
	/* (under an order's point only a weapon it was sent for: no walk off its place for one of its own choosing) */
	else if (have_leader && !deck && (!point_order || ae_squad.members[slot].pickup_ordered) && ae_squad_fetch(bot, &tick, slot, &leader_at))
	{
		ae_squad.members[slot].track_walked = FALSE;
		if (point_order)
			ae_squad_order_does(slot, AE_SQUAD_DOES_FETCH, 0.0f);
	}
	/* (DOOR STACK: stacked at a door, going in, inside) */
	else if (have_leader && !deck && (door_moved = ae_squad_door_member(bot, &tick, slot)) != FALSE)
	{
		ae_squad.members[slot].track_walked = FALSE;
		if (point_order)
			ae_squad_order_does(slot, AE_SQUAD_DOES_DOOR, 0.0f);
	}
	/* (COVER: in a fight with the player standing, a spot out of the enemy's sight) */
	else if (have_leader && !deck && (!keep_pace || point_order) &&
		ae_squad_cover(bot, &tick, slot, point_order ? &ae_squad.members[slot].order_place : &leader_at))
	{
		ae_squad.members[slot].track_walked = FALSE;
		if (point_order)
			ae_squad_order_does(slot, AE_SQUAD_DOES_COVER, 0.0f);
	}
	else if (!have_leader)
	{
		if (!tick.fighting)
			tick.moving = FALSE;
	}
	else if (deck || track_follow)
	{
		/* (the next spot of the player's track, in single file by number; in a fight too while the player moves on, shooting
		on the move: along the track, never off it; indoors the single file's own spacing; one standing on a barrier's top, off
		narrow ground, walks on off it) */
		if ((!tick.fighting || keep_pace) && (deck ? ae_bots_squad_follow_wants_move(distance, ae_squad.members[slot].moving, slot) ||
			(ae_squad.members[slot].cut_off && distance >= AE_SQUAD_CUT_OFF_GAP) ||
			(!narrow && ae_bots_squad_perched(tick.position.z - leader_at.z, distance)) :
			(ae_bots_squad_track_wants_move(distance, ae_squad.members[slot].moving, slot) ||
			ae_bots_squad_perched(tick.position.z - leader_at.z, distance))))
		{
			real_point3d step = leader_at, walk_to;
			int cue = 0;

			ae_companion_track_goal_cue_for(slot, &tick.position, &step, &cue);
			/* (never back along the player's track: a goal farther from the player than the one it walks to keeps that one) */
			if (ae_squad.members[slot].goal_valid && bots_distance2d(&tick.position, &ae_squad.members[slot].goal) > 0.4f &&
				ae_bots_squad_goal_keeps((float)bots_distance2d(&step, &leader_at),
					(float)bots_distance2d(&ae_squad.members[slot].goal, &leader_at)))
				step = ae_squad.members[slot].goal;
			else
			{
				ae_squad.members[slot].goal = step;
				ae_squad.members[slot].goal_valid = TRUE;
			}
			/* (a crumb in a door's reach it gained nothing on for 2 s, not one he jumped at, is passed over: the next one on is its
			goal) */
			{
				real to_crumb = bots_distance2d(&tick.position, &step);
				real_point3d from = step;
				float line_x = 0.0f, line_y = 0.0f;

				if (bots_distance2d(&step, &ae_squad.members[slot].crumb_goal) > 0.05f || tick.now < ae_squad.members[slot].crumb_since ||
					to_crumb < ae_squad.members[slot].crumb_best - AE_SQUAD_CRUMB_GAIN)
				{
					ae_squad.members[slot].crumb_goal = step;
					ae_squad.members[slot].crumb_best = to_crumb;
					ae_squad.members[slot].crumb_since = tick.now;
				}
				/* (LINE UP: off the player's line to a crumb it gains nothing on, it walks onto that line first; once a crumb) */
				else if (!(cue & AE_COMPANION_TRACK_JUMPED) && tick.now - ae_squad.members[slot].crumb_since >= AE_SQUAD_LINEUP_TICKS &&
					bots_distance2d(&step, &ae_squad.members[slot].lineup_crumb) > 0.05f &&
					ae_companion_track_cue_crumb(&tick.position, &from) && bots_distance2d(&from, &step) > 0.05f &&
					ae_bots_squad_lineup_point((float)from.x, (float)from.y, (float)step.x, (float)step.y, (float)tick.position.x,
						(float)tick.position.y, &line_x, &line_y))
				{
					ae_squad.members[slot].lineup_at = step;
					ae_squad.members[slot].lineup_at.x = line_x;
					ae_squad.members[slot].lineup_at.y = line_y;
					ae_squad.members[slot].lineup_crumb = step;
					ae_squad.members[slot].lineup_since = tick.now > 0 ? tick.now : 1;
					ae_squad.members[slot].crumb_since = tick.now;
					if (ae_bots_events_on())
						platform_log("companion: %ld lines up on the player's way at %.1f %.1f, to the crumb at %.1f %.1f", slot + 1,
							line_x, line_y, step.x, step.y);
				}
				else if (ae_bots_squad_crumb_unreachable((cue & AE_COMPANION_TRACK_JUMPED) != 0,
						ae_squad_in_door_reach(&step, 0.0f, NONE, NULL), tick.now - ae_squad.members[slot].crumb_since))
				{
					platform_log("companion: %ld passes over a crumb it cannot reach at %.1f %.1f (%.1f away)", slot + 1, step.x, step.y,
						to_crumb);
					ae_companion_track_skip(slot, &step);
					ae_squad.members[slot].goal_valid = FALSE;
					ae_squad.members[slot].crumb_since = tick.now;
					if (ae_companion_track_goal_cue_for(slot, &tick.position, &step, &cue))
						ae_squad.members[slot].crumb_goal = step;
				}
			}
			/* (LINE UP: to the point on his line first, until there, its crumb changed or out of time; its stall watch waits) */
			walk_to = step;
			if (ae_squad.members[slot].lineup_since)
			{
				if (bots_distance2d(&ae_squad.members[slot].lineup_crumb, &step) > 0.05f ||
					tick.now - ae_squad.members[slot].lineup_since >= AE_SQUAD_LINEUP_MOST_TICKS ||
					bots_distance2d(&tick.position, &ae_squad.members[slot].lineup_at) < AE_SQUAD_ARRIVE)
					ae_squad.members[slot].lineup_since = 0;
				else
				{
					walk_to = ae_squad.members[slot].lineup_at;
					ae_squad.members[slot].crumb_since = tick.now;
				}
			}
			bot->goal_kind = _bots_goal_none;
			bot->path.valid = FALSE;
			ae_squad_walk(bot, &tick, slot, (real)atan2(walk_to.y - tick.position.y, walk_to.x - tick.position.x));
			/* (on open ground, after he turned back lately with the member now ahead of him on his way: it stands and lets him by,
			its track folding back past it; MAKE WAY still steps it aside. On narrow ground it walks on ahead: FILE REVERSAL) */
			if (!narrow && ae_squad_player_turned_tick() && tick.now - ae_squad_player_turned_tick() < AE_SQUAD_PLAYER_TURN_TICKS &&
				ae_squad_facing_dot(ae_squad_player_yaw(), &leader_at, &tick.position) > AE_SQUAD_BEHIND &&
				bots_distance2d(&tick.position, &leader_at) < AE_SQUAD_RING_OUT * 2.0f)
				tick.moving = FALSE;
			/* (ONE AT A TIME: at a jump crumb another member is crossing: wait back from it; else this one crosses) */
			if (cue & AE_COMPANION_TRACK_JUMPED)
			{
				long other = ae_squad.crossing;
				real_point3d crumb = step;

				(void)ae_companion_track_cue_crumb(&tick.position, &crumb);
				/* (the one crossing is across, gone, away from it or out of time: the crossing is free) */
				if (other != NONE && !ae_squad_crossing_holds(tick.now))
				{
					ae_squad.crossing = NONE;
					other = NONE;
				}

				if (other != NONE && other != slot && bots_distance2d(&ae_squad.crossing_at, &crumb) < 0.5f)
				{
					if (bots_distance2d(&tick.position, &crumb) < AE_SQUAD_CROSSING_CLEAR + AE_SQUAD_FILE_GAP)
						tick.moving = FALSE;
					cue &= ~AE_COMPANION_TRACK_JUMPED;
					/* (waiting its turn is no stall: its progress watch starts again) */
					ae_squad.members[slot].progress_tick = tick.now > 0 ? tick.now : 1;
				}
				else if (other != slot && bots_distance2d(&tick.position, &crumb) < AE_SQUAD_CROSSING_CLEAR)
				{
					ae_squad.crossing = slot;
					ae_squad.crossing_at = crumb;
					ae_squad.crossing_to = step;
					ae_squad.crossing_since = tick.now;
				}
				/* (the jump at the crumb, not on the way to it) */
				if (!ae_bots_squad_cue_jump_now((float)bots_distance2d(&tick.position, &crumb)))
					cue &= ~AE_COMPANION_TRACK_JUMPED;
			}
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
			ae_squad_runup(&tick, slot, narrow);
			ae_squad_jump_gate(&tick, slot);
			/* (bot events on: each jump with where, how fast, crouched or not, at most every half second) */
			if (ae_bots_events_on() && TEST_FLAG(tick.flags, _unit_control_jump_bit) &&
				tick.now - ae_squad.members[slot].jump_logged >= TICKS_PER_SECOND / 2)
			{
				real_vector3d const *velocity = &tick.object->object.translational_velocity;

				ae_squad.members[slot].jump_logged = tick.now;
				platform_log("companion: %ld jumps at %.2f %.2f %.2f toward %.2f %.2f, speed %.2f%s%s", slot + 1, tick.position.x,
					tick.position.y, tick.position.z, step.x, step.y,
					(real)sqrt(velocity->i * velocity->i + velocity->j * velocity->j) * (real)TICKS_PER_SECOND,
					TEST_FLAG(tick.flags, _unit_control_crouch_modifier_bit) ? ", crouched" : "", (cue & AE_COMPANION_TRACK_JUMPED) ? ", cue" : "");
			}
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
	/* (at a door the door's own facing stands) */
	if (!in_vehicle && !tick.fighting && !tick.moving && !door_moved)
		ae_squad_face(bot, &tick, slot);
	ae_squad.members[slot].moving = ae_squad.members[slot].walked;
	if (!tick.fighting && tick.weapon_index != NONE && bots_weapon_loaded(tick.weapon_index) < 0.4f &&
		tick.now - bot->target_seen_tick > TICKS_PER_SECOND * 2)
		tick.flags |= FLAG(_unit_control_weapon_reload_bit);
	ae_squad_weapons(&tick, bot, slot);
	if (!in_vehicle)
	{
		if (!tick.fighting && !deck)
			ae_squad_yield(slot, &tick);
		if (!deck)
			ae_squad_unstick(bot, &tick);
	}

	/* (DOOR KEEP-OUT: a member walking with the player steps out of a door's reach unless the player went through it) */
	if (have_leader && !in_vehicle && !deck && !door_moved && !tick.fighting)
		(void)ae_squad_door_keep_out(&tick, slot, &leader_at);
	/* (MAKE WAY: out of the player's way, or on ahead of him) */
	if (have_leader && !in_vehicle && !tick.fighting)
		ae_squad_make_way(&tick, slot);
	if (tick.moving)
		ae_squad_oscillation_note(&tick, slot);
	ae_squad_progress_note(bot, &tick, slot);
	/* (THE EDGE before the throttle is made of the walk: its turn back, its brake and its stand are what the unit gets; the
	deck's small drop only on an object's or narrow ground, not for a member cut off by a barrier on a floor, whose jump over
	it would be braked in the air) */
	if (!in_vehicle)
		ae_squad_edge(&tick, slot, narrow);
	if (tick.moving)
	{
		real relative = bots_angle_difference(tick.move_direction, bot->yaw);

		action->throttle.i = (real)cos(relative) * tick.throttle_scale;
		action->throttle.j = (real)sin(relative) * tick.throttle_scale;
	}
	/* (bot events on: what a member in the player's way sends its unit this tick) */
	if (ae_bots_events_on() && ae_squad.members[slot].in_way_tick == tick.now && tick.now)
		platform_log("companion: %ld making way sends %s, throttle %.2f %.2f, way %.0f, facing %.0f", slot + 1,
			tick.moving ? "a walk" : "no walk", action->throttle.i, action->throttle.j, tick.move_direction / BOTS_DEGREES,
			bot->yaw / BOTS_DEGREES);
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
		struct unit_datum const *ridden = in_vehicle ? (struct unit_datum const *)object_try_and_get_and_verify_type(
			tick.object->object.parent_object_index, _object_mask_unit) : NULL;

		ae_squad.trigger_ticks++;
		/* (the gunner's trigger alone: a passenger's own weapon is not the gun) */
		if (ridden && ridden->unit.gunner_object_index == tick.player->unit_index)
			ae_squad.gunner_trigger_ticks++;
	}
	/* (OUTCOME: on foot with the player, its gap to him, a tick stuck, a tick at a lip) */
	if (have_leader && !in_vehicle)
	{
		real_vector3d const *velocity = &tick.object->object.translational_velocity;
		long bucket = (long)(distance * 2.0f);

		ae_squad.members[slot].gaps[bucket < AE_SQUAD_GAP_BUCKETS ? bucket : AE_SQUAD_GAP_BUCKETS - 1]++;
		if (ae_bots_squad_stuck_tick(tick.moving,
				(float)(sqrt(velocity->i * velocity->i + velocity->j * velocity->j) * TICKS_PER_SECOND), tick.fighting))
			ae_squad.members[slot].stuck_ticks++;
		if (ae_squad.members[slot].runup_since || ae_squad.members[slot].lip_tick == tick.now)
			ae_squad.members[slot].lip_ticks++;
		if (tick.now - ae_squad_slots.still_since >= AE_SQUAD_FORMATION_HALT_TICKS && tick.now >= ae_squad_slots.still_since &&
			(ae_squad.members[slot].near_still <= 0.0f || distance < ae_squad.members[slot].near_still))
			ae_squad.members[slot].near_still = distance > 0.01f ? distance : 0.01f;
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
	/* (a warp or a new body: its walk and its progress start again) */
	ae_squad.members[slot].walk_valid = FALSE;
	ae_squad.members[slot].walk_failed_until = 0;
	ae_squad.members[slot].progress_tick = 0;
	ae_squad.members[slot].creep_since = 0;
	ae_companion_track_forget(slot);
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

/* the report's lines (fight, walk, doors, then the stats groups): the text each was last written with and the tick */
#define AE_SQUAD_REPORT_LINES (7 + AE_COMPANION_MOST)
#define AE_SQUAD_REPORT_LENGTH 256
static char ae_squad_report_last[AE_SQUAD_REPORT_LINES][AE_SQUAD_REPORT_LENGTH];
static long ae_squad_report_tick[AE_SQUAD_REPORT_LINES] = { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };

/* a report line written to halo.log only when REPORT (ae_bots_squad_rules.h) says it is due */
static void ae_squad_report_line(int which, char const *text, long now)
{
	if (!ae_bots_squad_report_due(ae_squad_report_last[which], text, ae_squad_report_tick[which], now))
		return;
	platform_log("%s", text);
	strncpy(ae_squad_report_last[which], text, AE_SQUAD_REPORT_LENGTH - 1);
	ae_squad_report_last[which][AE_SQUAD_REPORT_LENGTH - 1] = '\0';
	ae_squad_report_tick[which] = now;
}

/* the brain's counts since the level's start, for halo.log: short lines, none cut off, each written when it changed or every
30 s */
void bots_squad_report_lines(float closest_pair)
{
	static char const *const groups[] = { "path", "slots", "warps", "cost" };
	char line[AE_SQUAD_REPORT_LENGTH];
	long now = game_time_get();
	int group;
	long member;

	snprintf(line, sizeof line, "squad fight: targets found %ld, fight ticks %ld, trigger %ld, grenades %ld, blows %ld, held fire %ld, "
		"sidesteps %ld, walked fighting %ld, seated trigger %ld (farthest %.0f), dry ticks %ld",
		ae_squad.targets_found, ae_squad.fight_ticks, ae_squad.trigger_ticks, ae_squad.grenades, ae_squad.blows,
		ae_bots_friendly_fire_held_ticks, ae_squad.sidesteps, ae_squad.walk_fight_ticks, ae_squad.gunner_trigger_ticks,
		(double)ae_squad.gunner_farthest, ae_squad.empty_ticks);
	ae_squad_report_line(0, line, now);
	snprintf(line, sizeof line, "squad walk: follow ticks %ld, jumps held %ld, track jumps %ld, pickups gone %ld, refills %ld, "
		"intro hold %ld, spots %ld (none %ld), team changes %ld, covers %ld (%ld ticks)",
		ae_squad.follow_ticks, ae_squad.jumps_held, ae_squad.track_jumps, ae_squad.pickups_gone_for, ae_squad.refills,
		ae_squad.hold_ticks, ae_squad_form.choices, ae_squad_form.none_found, ae_squad_team.changes, ae_squad.covers_taken,
		ae_squad.cover_ticks);
	ae_squad_report_line(1, line, now);
	snprintf(line, sizeof line, "squad doors: track ticks %ld, cue jumps %ld, crouches %ld, watches %ld, room changes %ld, stacks %ld, "
		"entries %ld, grenades %ld, locked %ld, run-ups %ld, creeps %ld",
		ae_squad.track_follow_ticks, ae_squad.cue_jumps, ae_squad.cue_crouches, ae_squad.watches, ae_squad_form.room_changes,
		ae_squad_door.stacks, ae_squad_door.entries, ae_squad_door.grenades, ae_squad_door.locked, ae_squad.runups, ae_squad.creeps);
	ae_squad_report_line(2, line, now);
	ae_squad.stats[AE_SQUAD_STAT_UNSTICK] = ae_squad.unsticks;
	ae_squad.stats[AE_SQUAD_STAT_EDGE] = ae_squad.edges_held;
	ae_squad.stats[AE_SQUAD_STAT_KEEPOUT] = ae_squad_keep_out.steps;
	ae_squad.stats[AE_SQUAD_STAT_CLOSEST_PAIR] = (long)(closest_pair * 100.0f);
	for (group = 0; group < (int)NUMBEROF(groups); group++)
		if (ae_bots_squad_stats_line(line, (int)sizeof line, groups[group], ae_squad.stats) > 0)
			ae_squad_report_line(3 + group, line, now);
	/* (each member that has walked with the player: its seconds stuck and at a lip, its 90th percentile gap to him, its least gap
	while he stood (99 not yet)) */
	for (member = 0; member < AE_COMPANION_MOST; member++)
	{
		long walked = 0, bucket;

		for (bucket = 0; bucket < AE_SQUAD_GAP_BUCKETS; bucket++)
			walked += ae_squad.members[member].gaps[bucket];
		if (!walked)
			continue;
		snprintf(line, sizeof line, "squad member %ld: stuck %.1f lip %.1f gap90 %.2f near %.2f", member + 1,
			(double)ae_squad.members[member].stuck_ticks / TICKS_PER_SECOND, (double)ae_squad.members[member].lip_ticks / TICKS_PER_SECOND,
			(double)ae_bots_squad_gap_p90(ae_squad.members[member].gaps) / 100.0,
			ae_squad.members[member].near_still > 0.0f ? (double)ae_squad.members[member].near_still : 99.0);
		ae_squad_report_line(7 + member, line, now);
	}
}
