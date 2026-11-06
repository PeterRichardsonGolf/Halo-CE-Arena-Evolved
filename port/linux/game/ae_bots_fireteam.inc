/* ae_bots_fireteam.inc: (fireteams, ae_fireteam.c) a bot's fireteam has a standing order, and the order sets the bot's
goal. It sits under a fight's own aim, the flag carried and the bot's vehicle goals, and above the marks, the objective
and roaming. FOLLOW holds the bot inside the leash around its leader; REGROUP brings every member within the re-form
radius once, then the order clears itself. ATTACK presses the bot to its point again once it has drifted past 1.5 leashes;
DEFEND and WATCH hold an anchor around the point (a goal a fight moved is set back after it); GOING stands at the point
and lets go 15 s after the first member arrived. A fight, a carried flag or ball and the bot's vehicle goals come first:
the order only reads the goal again when they are over. Host only: bots live there. */

#include "errors.h"

/* a member that could not reach its slot stands where it is until (per brain) */
static long ae_fireteam_settle_until[BOTS_BRAINS];
/* the last tick the tether held a member still (in its slot, or stopped at the radius's edge): standing there is not being stuck */
static long ae_fireteam_hold_tick[BOTS_BRAINS];

/* (a goal set every tick restarts the path search: a moving leader moves the goal only past this) */
#define AE_BOTS_FIRETEAM_GOAL_STEP 2.0f
/* (a leader far ahead or in a vehicle moves fast: the goal follows in steps of this share of the distance, so the path is not
searched again every few ticks) */
#define AE_BOTS_FIRETEAM_GOAL_STEP_SHARE 0.2f
/* a REGROUP nobody can finish (a member dead for the round, stuck or seated) lets go after this many seconds; a GOING nobody
reached, after the second */

static boolean ae_bots_fireteam_leader_position(long fireteam, long brain, real_point3d *position)
{
	long leader = ae_fireteam_leader_player(fireteam);
	struct player_datum const *player;

	if (leader == NONE)
	{
		/* a bot-led squad: its lowest brain leads (that one leads itself, and follows no one) */
		short lead = ae_fireteam_lead_brain_of(fireteam);

		if (lead < 0 || lead == (short)brain || bots_globals.bots[lead].player_index == NONE)
			return FALSE;
		leader = bots_globals.bots[lead].player_index;
	}
	player = player_try_and_get(leader);
	if (!player || player->unit_index == NONE || !object_try_and_get(player->unit_index))
		return FALSE;
	*position = object_get(player->unit_index)->object.position;
	return TRUE;
}

/* a leader's own order gives way to the objective while the objective needs hands: the planted bomb to be defused (the
defenders), the bomb to be fetched (the attackers), a flag lying loose to be returned or taken. The squads' roles never need it
(their points are the objective's) */
static boolean ae_bots_fireteam_objective_window(long fireteam)
{
	struct ae_bots_objective_snapshot const *snapshot = ae_bots_objective_snapshot();
	short team = ae_fireteam_team_of(fireteam);
	long index;

	if (!snapshot || team < 0 || team > 1)
		return FALSE;
	switch (snapshot->mode)
	{
	case AE_BOTS_MODE_BOMB:
		if (!ae_bomb_snd_on())
			return FALSE;
		if (snapshot->bomb_planted_goal != NONE)
			return snapshot->bomb_attacking_team != team;
		return snapshot->bomb_takeable && snapshot->bomb_attacking_team == team;
	case AE_BOTS_MODE_CTF:
		for (index = 0; index < 2; index++)
			if (snapshot->flags[index].exists && !snapshot->flags[index].home && snapshot->flags[index].carrier_player_index == NONE)
				return TRUE;
		return FALSE;
	}
	return FALSE;
}

/* the point orders: attack, defend, watch, go */
static boolean ae_bots_fireteam_point_goal(struct bots_bot *bot, long bot_index, long player_index, long fireteam,
	short kind, real_point3d const *position, boolean fighting)
{
	real_point3d spot, ground;
	long surface_index;
	real distance, goal_distance;

	if (fighting || !ae_fireteam_order_point_of(fireteam, &spot) || ae_bots_vehicle_carrying(bot, bot_index) ||
		ae_bots_objective_player_carries(ae_bots_objective_snapshot(), player_index))
		return FALSE;
	distance = bots_distance3d(position, &spot);
	if (kind == _ae_order_go && ae_fireteam_go_report(bot_index, distance, (real)game_time_get() / (real)TICKS_PER_SECOND))
		return FALSE;
	/* (a member at the point, or in a crowd of bots (its squad's or any other, all in its way) about it, stands there with no goal (the crowd at one
	flag stand all pressed on to the very point and stood stuck, Blood Gulch 11-05); the standing is not being stuck) */
	if (kind == _ae_order_defend || kind == _ae_order_watch)
	{
		long other, mates = 0;

		for (other = 0; other < BOTS_BRAINS; other++)
			if (other != bot_index && bots_globals.bots[other].player_index != NONE && bots_globals.bots[other].unit_index != NONE)
			{
				struct object_datum const *mate = (struct object_datum const *)object_try_and_get_and_verify_type(
					bots_globals.bots[other].unit_index, _object_mask_unit);

				if (mate && bots_distance2d(&mate->object.position, position) < 1.6f && fabs(mate->object.position.z - position->z) < 1.5f)
					mates++;
			}
		if (ae_fireteam_settled(kind, distance, (int)mates))
		{
			bot->goal_kind = _bots_goal_none;
			bot->path.valid = FALSE;
			ae_fireteam_hold_tick[bot_index] = game_time_get();
			return TRUE;
		}
	}
	goal_distance = bot->goal_kind == _bots_goal_none ? 1.0e9f : bots_distance3d(&bot->goal_point, &spot);
	if (ae_fireteam_press_wanted(kind, distance, goal_distance))
	{
		surface_index = bots_surface_below(&spot, &ground);
		if (surface_index == NONE)
			return FALSE;
		if (bot->goal_kind != _bots_goal_roam || bots_distance3d(&bot->goal_point, &ground) > AE_BOTS_FIRETEAM_GOAL_STEP)
			bots_set_goal(bot, _bots_goal_roam, &ground, surface_index);
		return TRUE;
	}
	/* (inside the hold with its goal in it: held, the roam does not take the bot off the point; an attack is left to roam) */
	return kind != _ae_order_attack;
}

/* ---------- STRICT FOLLOW AND FORMATION (ae_fireteam_rules.h): the members of a squad a person leads stay inside the
follow radius, fight from their slot and take no detours */

/* the facing the formation is read at, per fireteam: the leader's walk or aim, moved only past the re-slot turn */
static real ae_fireteam_held_facing[AE_FIRETEAM_MAX];
static long ae_fireteam_held_tick[AE_FIRETEAM_MAX];

/* the formation slot of a member (its index among its fireteam's brains) round the leader */
static real_point3d ae_bots_fireteam_slot_point(long fireteam, long brain, real_point3d const *leader_point)
{
	long leader = ae_fireteam_leader_player(fireteam), other, index = 0, now = game_time_get();
	struct player_datum const *player = leader != NONE ? player_try_and_get(leader) : NULL;
	real_point3d slot = *leader_point;
	real facing = 0.0f, dx, dy;
	boolean first;

	for (other = 0; other < brain && other < BOTS_BRAINS; other++)
		if (ae_fireteam_of_brain(other) == fireteam)
			index++;
	if (player && player->unit_index != NONE)
	{
		struct unit_datum const *unit = (struct unit_datum const *)object_try_and_get_and_verify_type(player->unit_index, _object_mask_unit);

		if (unit)
		{
			real_vector3d const *v = &unit->object.translational_velocity;

			/* (walking: the way it goes; standing: the way it aims) */
			if (v->i * v->i + v->j * v->j > 0.03f * 0.03f)
				facing = (real)atan2(v->j, v->i);
			else
				facing = (real)atan2(unit->unit.aiming_vector.j, unit->unit.aiming_vector.i);
		}
	}
	if (fireteam >= 0 && fireteam < AE_FIRETEAM_MAX)
	{
		first = ae_fireteam_held_tick[fireteam] == 0 || now < ae_fireteam_held_tick[fireteam] ||
			now - ae_fireteam_held_tick[fireteam] > 3 * TICKS_PER_SECOND;
		facing = ae_fireteam_formation_facing(ae_fireteam_held_facing[fireteam], facing, first);
		ae_fireteam_held_facing[fireteam] = facing;
		ae_fireteam_held_tick[fireteam] = now;
	}
	ae_fireteam_formation_slot((int)index, facing, &dx, &dy);
	slot.x += dx;
	slot.y += dy;
	return slot;
}

static boolean ae_bots_round_revive_pending(struct bots_bot *bot); /* (ae_bots_round.inc) */

/* a member of a squad led by a person, under FOLLOW or REGROUP, on foot and free of the objective: tethered */
static boolean ae_bots_fireteam_tethered(struct bots_bot *bot, long bot_index, long player_index, long fireteam)
{
	short kind;

	if (fireteam == NONE || ae_fireteam_leader_player(fireteam) == NONE)
		return FALSE;
	kind = ae_fireteam_order_of(fireteam);
	if (kind != _ae_order_follow && kind != _ae_order_regroup)
		return FALSE;
	if (ae_fireteam_order_explicit_of(fireteam) && ae_bots_fireteam_objective_window(fireteam))
		return FALSE;
	/* (a downed teammate's orb is the call of a bot that is free to go: above the tether) */
	if (ae_bots_round_revive_pending(bot))
		return FALSE;
	return !ae_bots_vehicle_carrying(bot, bot_index) && !ae_bots_objective_player_carries(ae_bots_objective_snapshot(), player_index);
}

/* (ae_bots_falls_guard, before the dodge and the squads' roles) the tether: TRUE for a tethered member, whose walk is its
slot's and whom the roles leave alone */
static boolean ae_bots_fireteam_tether(struct bots_bot *bot, struct bots_tick *tick)
{
	long bot_index = (long)(bot - bots_globals.bots), fireteam;
	real_point3d leader, slot, ground;
	real leader_distance, slot_distance, cosine = 2.0f, tolerance;
	long surface_index;
	int action;

	if (bot_index < 0 || bot_index >= BOTS_BRAINS || !tick->object || tick->object->object.parent_object_index != NONE)
		return FALSE;
	fireteam = ae_fireteam_of_brain(bot_index);
	if (!ae_bots_fireteam_tethered(bot, bot_index, bot->player_index, fireteam) ||
		!ae_bots_fireteam_leader_position(fireteam, bot_index, &leader))
		return FALSE;
	slot = ae_bots_fireteam_slot_point(fireteam, bot_index, &leader);
	leader_distance = bots_distance3d(&tick->position, &leader);
	slot_distance = bots_distance3d(&tick->position, &slot);
	tolerance = ae_fireteam_order_of(fireteam) == _ae_order_regroup ? AE_FIRETEAM_REGROUP_TOLERANCE : AE_FIRETEAM_FOLLOW_TOLERANCE;
	if (tick->moving && tick->throttle_scale > 0.0f)
	{
		real out = (real)atan2(tick->position.y - leader.y, tick->position.x - leader.x);

		cosine = (real)cos(tick->move_direction - out);
	}
	/* (a recovery from being stuck is the unstick's step: not overwritten. A slot a member cannot reach (in a wall, behind a
	crowd) is given up for five seconds once it was stuck on the way: it stands where it is, inside the radius) */
	if (tick->now < bot->unstick_until_tick)
	{
		if (slot_distance > tolerance && leader_distance <= AE_FIRETEAM_FOLLOW_RADIUS)
			ae_fireteam_settle_until[bot_index] = tick->now + 5 * TICKS_PER_SECOND;
		return TRUE;
	}
	if (tick->now < ae_fireteam_settle_until[bot_index] && ae_fireteam_settle_until[bot_index] - tick->now <= 5 * TICKS_PER_SECOND &&
		leader_distance <= AE_FIRETEAM_FOLLOW_RADIUS)
		slot_distance = 0.0f;
	action = ae_fireteam_tether(leader_distance, slot_distance, tick->fighting, cosine, tolerance);
	if (action == AE_TETHER_STOP || action == AE_TETHER_HOLD)
	{
		tick->moving = FALSE;
		ae_fireteam_hold_tick[bot_index] = tick->now;
		return TRUE;
	}
	if (action == AE_TETHER_GO)
	{
		real direction;
		real_point3d const *aim = &slot;

		surface_index = bots_surface_below(&slot, &ground);
		if (surface_index == NONE)
		{
			surface_index = bots_surface_below(&leader, &ground);
			aim = &leader;
		}
		if (surface_index != NONE)
		{
			/* (a tight leash: the goal follows the slot in steps of 1.5 wu, not the walker's wide ones) */
			if (bot->goal_kind != _bots_goal_roam || bots_distance3d(&bot->goal_point, &ground) > 1.5f)
				bots_set_goal(bot, _bots_goal_roam, &ground, surface_index);
			if (!bot->path.valid && tick->now - bot->path_tick > TICKS_PER_SECOND / 3)
				bots_search(bot, tick->player->unit_index);
		}
		if (surface_index != NONE && bot->path.valid && bots_follow_path(bot, tick->player->unit_index, &tick->position, &direction))
			tick->move_direction = direction;
		else
			tick->move_direction = (real)atan2(aim->y - tick->position.y, aim->x - tick->position.x);
		tick->moving = TRUE;
		tick->throttle_scale = 1.0f;
		if (tick->fighting && ae_bots_events_on() && tick->now % TICKS_PER_SECOND == 0)
			platform_log("bots: ev=tether_go bot=%ld leader_distance=%.1f slot_distance=%.1f", bot_index + 1, leader_distance,
				slot_distance);
	}
	return TRUE;
}

/* ---------- squad roles: each mode hands its fireteams a default standing order (ae_fireteam_rules.h's role table) */

/* the mode's roles for a team, from the objective's snapshot (the same probes the bots' objective code reads), or none */
static int ae_bots_fireteam_role_mode(struct ae_bots_objective_snapshot const *snapshot, short team)
{
	switch (snapshot->mode)
	{
	case AE_BOTS_MODE_CTF:
		return snapshot->flags[0].exists && snapshot->flags[1].exists ? AE_ROLE_MODE_CTF : AE_ROLE_MODE_NONE;
	case AE_BOTS_MODE_BOMB:
		if (!ae_bomb_snd_on())
			return AE_ROLE_MODE_NONE;
		return snapshot->bomb_attacking_team == team ? AE_ROLE_MODE_SND_ATTACKERS : AE_ROLE_MODE_SND_DEFENDERS;
	case AE_BOTS_MODE_KING:
		return snapshot->hill ? AE_ROLE_MODE_KING : AE_ROLE_MODE_NONE;
	case AE_BOTS_MODE_ODDBALL:
		return AE_ROLE_MODE_ODDBALL;
	case AE_BOTS_MODE_SLAYER:
		return AE_ROLE_MODE_SLAYER;
	}
	return AE_ROLE_MODE_NONE;
}

/* where a role's order points now; FALSE when the objective gives it none (the bots' own goals serve it: a bomb loose to be
fetched, a bomb planted to be defused, a ball carried) */
static boolean ae_bots_fireteam_role_point(struct ae_bots_objective_snapshot const *snapshot, short team, int role,
	real_point3d *point)
{
	short other = (short)(1 - team);

	switch (role)
	{
	case AE_ROLE_DEFEND_OURS:
	{
		struct ae_bots_objective_flag const *flag = &snapshot->flags[team];

		if (!flag->exists)
			return FALSE;
		/* (a flag lying loose is gone back for; one carried off or home, its stand is held) */
		*point = flag->home || flag->carrier_player_index != NONE ? flag->stand : flag->position;
		return TRUE;
	}
	case AE_ROLE_PUSH_THEIRS:
	{
		struct ae_bots_objective_flag const *flag = &snapshot->flags[other];
		struct player_datum const *carrier = flag->carrier_player_index != NONE ? player_try_and_get(flag->carrier_player_index) : NULL;

		if (!flag->exists)
			return FALSE;
		/* (our team has it: on to our stand to score it) */
		*point = carrier && carrier->team_index == team && snapshot->flags[team].exists ? snapshot->flags[team].stand : flag->position;
		return TRUE;
	}
	case AE_ROLE_SITE_A:
	case AE_ROLE_SITE_B:
		if (snapshot->bomb_planted_goal != NONE || !snapshot->bomb_has_goal[team])
			return FALSE;
		*point = snapshot->bomb_goals[team];
		return TRUE;
	case AE_ROLE_ENTRY:
		if (snapshot->bomb_takeable || !snapshot->bomb_has_goal[other])
			return FALSE;
		*point = snapshot->bomb_goals[other];
		return TRUE;
	case AE_ROLE_HILL:
	case AE_ROLE_APPROACH:
		if (!snapshot->hill)
			return FALSE;
		*point = snapshot->hill_centre;
		return TRUE;
	case AE_ROLE_PROTECT:
	case AE_ROLE_HUNT:
		if (snapshot->ball_count <= 0)
			return FALSE;
		*point = snapshot->ball_positions[0];
		return TRUE;
	}
	return FALSE;
}

/* what the objective changing shows: a flag taken, dropped, returned or carried on, the hill moved, the bomb carried, planted
or loose, a ball picked up or put down (a dropped flag and a ball in cells of 8 wu, the hill in cells of 4: a drifting
position is no flip) */
static unsigned long ae_bots_fireteam_signature(struct ae_bots_objective_snapshot const *snapshot)
{
	unsigned long hash = 2166136261UL;
	long team, values[24], count = 0, index;

	for (team = 0; team < 2; team++)
	{
		struct ae_bots_objective_flag const *flag = &snapshot->flags[team];
		boolean loose = flag->exists && !flag->home && flag->carrier_player_index == NONE;

		values[count++] = flag->exists + 2 * flag->home + 4 * (flag->carrier_player_index != NONE);
		values[count++] = loose ? (long)floorf(flag->position.x / 8.0f) : 0;
		values[count++] = loose ? (long)floorf(flag->position.y / 8.0f) : 0;
	}
	values[count++] = snapshot->hill ? (long)floorf(snapshot->hill_centre.x / 4.0f) : 0;
	values[count++] = snapshot->hill ? (long)floorf(snapshot->hill_centre.y / 4.0f) : 0;
	values[count++] = snapshot->bomb_carrier_player_index != NONE;
	values[count++] = snapshot->bomb_planted_goal;
	values[count++] = snapshot->bomb_takeable;
	values[count++] = snapshot->bomb_attacking_team;
	values[count++] = snapshot->ball_count;
	values[count++] = snapshot->ball_count > 0 ? (long)floorf(snapshot->ball_positions[0].x / 8.0f) : 0;
	values[count++] = snapshot->ball_count > 0 ? (long)floorf(snapshot->ball_positions[0].y / 8.0f) : 0;
	for (index = 0; index < count; index++)
		hash = (hash ^ (unsigned long)values[index]) * 16777619UL;
	return hash & 0xFFFFFFFFUL;
}

/* once a tick, from the first bot's goal choice: the roles go out when the fireteams were rebuilt (the game's start, a roster
change: a bot-led squad lost its order with its table), the objective flipped, a squad was wiped out (its role moves on) or a
spent order (a regroup done, a going let go) left the squad without its role's */
static void ae_bots_fireteam_roles_update(void)
{
	static long last_tick = -1, last_generation = -1;
	static unsigned long last_signature;
	static byte wiped[AE_FIRETEAM_MAX], away[AE_FIRETEAM_MAX];
	static long timed_serial[AE_FIRETEAM_MAX], timed_since[AE_FIRETEAM_MAX];
	struct ae_bots_objective_snapshot const *snapshot;
	long now = game_time_get(), slot;
	boolean rebuilt, flipped, sample;
	unsigned long signature;
	static long next_sample;

	if (now == last_tick)
		return;
	if (now < last_tick)
	{
		last_generation = -1;
		next_sample = 0;
	}
	last_tick = now;
	ae_fireteam_check_leaders();
	snapshot = ae_bots_objective_snapshot();
	if (!snapshot || ae_fireteam_slot_count() <= 0)
		return;
	rebuilt = ae_fireteam_generation() != last_generation;
	if (rebuilt)
	{
		last_generation = ae_fireteam_generation();
		csmemset(wiped, 0, sizeof(wiped));
		csmemset(away, 0, sizeof(away));
		for (slot = 0; slot < AE_FIRETEAM_MAX; slot++)
			timed_serial[slot] = -1;
	}
	signature = ae_bots_fireteam_signature(snapshot);
	flipped = !rebuilt && signature != last_signature;
	last_signature = signature;
	sample = now >= next_sample;
	if (sample)
		next_sample = now + TICKS_PER_SECOND * 10;
	for (slot = 0; slot < ae_fireteam_slot_count() && slot < AE_FIRETEAM_MAX; slot++)
	{
		short team = ae_fireteam_team_of(slot);
		int mode, role;
		long brain, members = 0, alive = 0, stale = 0, leader;
		boolean wiped_now, leader_alive, has_point, resume;
		real_point3d point;

		if (team < 0 || team > 1)
			continue;
		/* (a REGROUP or GOING holds until its members arrive or the leader calls another; it lets go only when no member can
		arrive: all dead, seated, or stuck, for AE_FIRETEAM_STALL_SECONDS. Distance alone never ends it. The clock is the order's) */
		{
			short order = ae_fireteam_order_of(slot);
			long serial = ae_fireteam_order_serial_of(slot);

			if (order != _ae_order_regroup && order != _ae_order_go)
				timed_serial[slot] = -1;
			else
			{
				long able = 0, who;

				for (who = 0; who < BOTS_BRAINS; who++)
					if (ae_fireteam_of_brain(who) == slot && !(order == _ae_order_regroup && ae_fireteam_brain_arrived_of(who)))
					{
						struct player_datum const *member = bots_globals.bots[who].player_index != NONE ?
							player_try_and_get(bots_globals.bots[who].player_index) : NULL;

						if (member && member->unit_index != NONE && object_try_and_get(member->unit_index) &&
							object_get(member->unit_index)->object.parent_object_index == NONE && bots_globals.bots[who].stuck_count < 2)
							able++;
					}
				if (serial != timed_serial[slot] || able > 0)
				{
					timed_serial[slot] = serial;
					timed_since[slot] = now;
				}
				else if (ae_fireteam_gave_up(order, (int)able, (float)(now - timed_since[slot]) / (float)TICKS_PER_SECOND))
				{
					error(_error_silent, "ae fireteam: fireteam %ld let go of its %s order (nobody could finish it)", slot,
						order == _ae_order_regroup ? "regroup" : "going");
					ae_fireteam_order((int)slot, _ae_order_none, NULL, NONE);
					timed_serial[slot] = -1;
				}
			}
		}
		/* (a human-led squad has no role: with no order it follows its leader) */
		if (ae_fireteam_leader_player(slot) != NONE)
		{
			if (ae_fireteam_order_of(slot) == _ae_order_none)
				ae_fireteam_order((int)slot, _ae_order_follow, NULL, NONE);
		}
		mode = ae_bots_fireteam_role_mode(snapshot, team);
		if (mode == AE_ROLE_MODE_NONE && ae_fireteam_leader_player(slot) == NONE)
			continue;
		for (brain = 0; brain < BOTS_BRAINS; brain++)
			if (ae_fireteam_of_brain(brain) == slot)
			{
				struct player_datum const *player = bots_globals.bots[brain].player_index != NONE ?
					player_try_and_get(bots_globals.bots[brain].player_index) : NULL;

				/* (a brain whose bot is gone (no player: a bot taken out, a game restarted) is a stale member, not a dead
				one: counted apart so it can never make a squad look wiped, owner log 11-05: squads "0 of 2 alive" for
				minutes) */
				if (!player)
				{
					stale++;
					continue;
				}
				members++;
				if (player->unit_index != NONE)
					alive++;
			}
		wiped_now = members > 0 && alive == 0;
		if (wiped_now && !wiped[slot])
			ae_fireteam_role_wiped_fireteam(slot);
		wiped[slot] = (byte)wiped_now;
		leader = ae_fireteam_leader_player(slot);
		leader_alive = leader == NONE || (player_try_and_get(leader) && player_try_and_get(leader)->unit_index != NONE);
		role = ae_fireteam_role_for(slot, mode);
		has_point = ae_bots_fireteam_role_point(snapshot, team, role, &point);
		resume = has_point && ae_fireteam_order_of(slot) == _ae_order_none && ae_fireteam_role_order_kind(role) != _ae_order_none;
		if (rebuilt || flipped || resume || role != ae_fireteam_role_taken(slot))
			ae_fireteam_role_pass(slot, role, has_point ? &point : NULL, leader_alive, flipped);
		if (sample && members > 0)
			error(_error_silent, "ae fireteam: roles fireteam %ld team %d role %d order %d (%ld of %ld alive%s)", slot, (int)team,
				ae_fireteam_role_taken(slot), (int)ae_fireteam_order_of(slot), alive, members, stale ? ", stale members" : "");
		/* (a tethered squad measured, with the events on: every second the living members' mean and furthest distance to the
		leader, and how many stand inside the follow radius) */
		if (ae_bots_events_on() && now % TICKS_PER_SECOND == 0 && alive > 0 && leader != NONE &&
			(ae_fireteam_order_of(slot) == _ae_order_follow || ae_fireteam_order_of(slot) == _ae_order_regroup) &&
			player_try_and_get(leader) && player_try_and_get(leader)->unit_index != NONE &&
			object_try_and_get(player_try_and_get(leader)->unit_index))
		{
			real_point3d where = object_get(player_try_and_get(leader)->unit_index)->object.position;
			double total = 0.0, furthest = 0.0;
			long inside = 0;

			for (brain = 0; brain < BOTS_BRAINS; brain++)
				if (ae_fireteam_of_brain(brain) == slot && bots_globals.bots[brain].player_index != NONE)
				{
					struct player_datum const *player = player_try_and_get(bots_globals.bots[brain].player_index);

					if (player && player->unit_index != NONE && object_try_and_get(player->unit_index))
					{
						double d = bots_distance3d(&object_get(player->unit_index)->object.position, &where);

						total += d;
						furthest = d > furthest ? d : furthest;
						inside += d <= AE_FIRETEAM_FOLLOW_RADIUS + 1.0f;
					}
				}
			platform_log("bots: ev=tether_dist fireteam=%ld order=%s alive=%ld mean=%.1f max=%.1f inside=%ld", slot,
				ae_fireteam_order_of(slot) == _ae_order_follow ? "follow" : "regroup", alive, total / (double)alive, furthest, inside);
		}
		/* (FOLLOW kept, in the log every ten seconds: the living members within two leashes of the leader's spot) */
		if (sample && alive > 0 && ae_fireteam_order_of(slot) == _ae_order_follow && leader != NONE &&
			player_try_and_get(leader) && player_try_and_get(leader)->unit_index != NONE &&
			object_try_and_get(player_try_and_get(leader)->unit_index))
		{
			real_point3d where = object_get(player_try_and_get(leader)->unit_index)->object.position;
			long near = 0;

			for (brain = 0; brain < BOTS_BRAINS; brain++)
				if (ae_fireteam_of_brain(brain) == slot && bots_globals.bots[brain].player_index != NONE)
				{
					struct player_datum const *player = player_try_and_get(bots_globals.bots[brain].player_index);

					if (player && player->unit_index != NONE && object_try_and_get(player->unit_index) &&
						bots_distance3d(&object_get(player->unit_index)->object.position, &where) <= AE_FIRETEAM_FOLLOW_RADIUS + 1.0f)
						near++;
				}
			error(_error_silent, "ae fireteam: follow %s fireteam %ld: %ld of %ld alive within %.0f wu of the leader",
				near * 2 >= alive ? "ok" : "away", slot, near, alive, (double)(AE_FIRETEAM_FOLLOW_RADIUS + 1.0f));
		}
		/* (REGROUP, in the log every ten seconds: the living members' mean distance to the leader, which shrinks as they come) */
		if (sample && alive > 0 && ae_fireteam_order_of(slot) == _ae_order_regroup && leader != NONE &&
			player_try_and_get(leader) && player_try_and_get(leader)->unit_index != NONE &&
			object_try_and_get(player_try_and_get(leader)->unit_index))
		{
			real_point3d where = object_get(player_try_and_get(leader)->unit_index)->object.position;
			double total = 0.0;

			for (brain = 0; brain < BOTS_BRAINS; brain++)
				if (ae_fireteam_of_brain(brain) == slot && bots_globals.bots[brain].player_index != NONE)
				{
					struct player_datum const *player = player_try_and_get(bots_globals.bots[brain].player_index);

					if (player && player->unit_index != NONE && object_try_and_get(player->unit_index))
						total += bots_distance3d(&object_get(player->unit_index)->object.position, &where);
				}
			error(_error_silent, "ae fireteam: regroup fireteam %ld: %ld alive, mean %.0f wu from the leader", slot, alive,
				total / (double)alive);
		}
		/* (a DEFEND anchor held, in the log every ten seconds: a living member within 1.5 anchor radii of its point) */
		if (sample && alive > 0 && ae_fireteam_order_of(slot) == _ae_order_defend &&
			ae_fireteam_order_point_of(slot, &point))
		{
			long near = 0;

			for (brain = 0; brain < BOTS_BRAINS; brain++)
				if (ae_fireteam_of_brain(brain) == slot && bots_globals.bots[brain].player_index != NONE)
				{
					struct player_datum const *player = player_try_and_get(bots_globals.bots[brain].player_index);

					if (player && player->unit_index != NONE && object_try_and_get(player->unit_index) &&
						bots_distance3d(&object_get(player->unit_index)->object.position, &point) <= AE_FIRETEAM_ANCHOR_RADIUS * 1.5f)
						near++;
				}
			/* (one sample away is a fight or a flag fetched; two in a row, twenty seconds, the anchor is lost) */
			error(_error_silent, "ae fireteam: anchor %s fireteam %ld: %ld of %ld alive within %.0f wu", near ? "ok" : away[slot] ? "lost" : "away",
				slot, near, alive, (double)(AE_FIRETEAM_ANCHOR_RADIUS * 1.5f));
			away[slot] = (byte)(near == 0);
		}
	}
}

static boolean ae_bots_fireteam_goal(struct bots_bot *bot, long player_index, real_point3d const *position, boolean fighting)
{
	long bot_index = (long)(bot - bots_globals.bots);
	long fireteam;
	short kind;
	real_point3d spot, ground;
	long surface_index;
	real distance;

	ae_bots_fireteam_roles_update();
	fireteam = ae_fireteam_of_brain(bot_index);
	kind = fireteam != NONE ? ae_fireteam_order_of(fireteam) : (short)_ae_order_none;

	if (kind != _ae_order_none && ae_fireteam_order_explicit_of(fireteam) && ae_bots_fireteam_objective_window(fireteam))
		return FALSE;
	if (kind == _ae_order_attack || kind == _ae_order_defend || kind == _ae_order_watch || kind == _ae_order_go)
		return ae_bots_fireteam_point_goal(bot, bot_index, player_index, fireteam, kind, position, fighting);
	if (kind != _ae_order_follow && kind != _ae_order_regroup)
		return FALSE;
	/* A SQUAD A PERSON LEADS is tethered (ae_bots_fireteam_tether walks it, fight or not): the goal is only the claim that
	keeps items, roaming and the objective from taking it off; its path is the tether's */
	if (ae_bots_fireteam_tethered(bot, bot_index, player_index, fireteam))
	{
		if (!ae_bots_fireteam_leader_position(fireteam, bot_index, &spot))
			return FALSE;
		ae_fireteam_regroup_report(bot_index, bots_distance3d(position, &spot));
		return TRUE;
	}
	if (kind == _ae_order_regroup && ae_fireteam_order_point_of(fireteam, &spot))
		;
	else if (!ae_bots_fireteam_leader_position(fireteam, bot_index, &spot))
		return FALSE;
	distance = bots_distance3d(position, &spot);
	if (kind == _ae_order_regroup)
	{
		if (ae_fireteam_regroup_report(bot_index, distance) || fighting)
			return FALSE;
	}
	else if (!ae_fireteam_follow_wanted(distance))
		return FALSE;
	if (ae_bots_vehicle_carrying(bot, bot_index))
		return FALSE;
	surface_index = bots_surface_below(&spot, &ground);
	if (surface_index == NONE)
		return FALSE;
	if (bot->goal_kind != _bots_goal_roam || bots_distance3d(&bot->goal_point, &ground) >
		AE_BOTS_FIRETEAM_GOAL_STEP + distance * AE_BOTS_FIRETEAM_GOAL_STEP_SHARE)
		bots_set_goal(bot, _bots_goal_roam, &ground, surface_index);
	(void)player_index;
	return TRUE;
}

/* (ae_bots_travel_may_stick) the tether held this member still a moment ago: it stands by design */
static boolean ae_bots_fireteam_holding(long bot_index, long now)
{
	return bot_index >= 0 && bot_index < BOTS_BRAINS && ae_fireteam_hold_tick[bot_index] > 0 &&
		now >= ae_fireteam_hold_tick[bot_index] && now - ae_fireteam_hold_tick[bot_index] <= 3;
}
