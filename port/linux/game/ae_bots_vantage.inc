/* ae_bots_vantage.inc: HOLDING A VANTAGE SPOT (ae_bots_vantage.h, ae_bots_vantage_rules.h): a bot with the sniper rifle or a heavy
weapon (a rocket launcher; the precision rifles are utility weapons, 0.7.8) takes the best free spot of the map, goes to it and holds it 30 to
60 seconds, looking from place to place, fighting from it without strafing or chasing. It leaves at once when its weapon changes, when
it is hurt where it cannot see who did it (shot from behind: flanked), when an enemy comes within 8 units, when it is hurt below half,
low on ammunition, or its time is up (then it rests 20 s), and when a human order, the objective, a power item or a vehicle takes its
goal: bots_choose_goal asks the vantage hold after all of those, so it only applies to a bot they left free, and a bot that is not
free is let go within four ticks (ae_bots_vantage_tick). At most a third of a team's bots (a half with long weapons) hold spots at
once, never two on one; not the spot it held last, not one it found no way to. A King of the Hill bot holds over the hill: only a spot
that sees it. bots.vantage false: none of this. Host only, as every bot decision; no network message. Included after the fireteams'. */

#include "ae_bots_vantage.h"

enum { AE_VANTAGE_STATE_NONE, AE_VANTAGE_STATE_GOING, AE_VANTAGE_STATE_HOLDING };

static struct ae_vantage_bot
{
	long unit_index;
	short state;
	short spot;			/* NONE for none */
	short weapon;			/* the class it holds for */
	short last_spot;
	long start_tick, arrive_tick, hold_ticks, rest_until, next_check, goal_tick, fight_tick;
	unsigned long failed;
	long failed_until[AE_VANTAGE_MAXIMUM];
	real last_vitality;
	long hurt_tick;
	short look_place;
	long look_until;
	boolean arrived_once;
} ae_vantage_bots[BOTS_BRAINS];

static struct
{
	long claims[AE_VANTAGE_MAXIMUM];
	long went, arrived, leaves[AE_VANTAGE_LEFT_REASONS], failed, last_report, last_tick;
	boolean claims_ready;
} ae_vantage_totals;

static void ae_vantage_claims_clear(void)
{
	int index;

	for (index = 0; index < AE_VANTAGE_MAXIMUM; index++)
		ae_vantage_totals.claims[index] = NONE;
	ae_vantage_totals.claims_ready = TRUE;
}

/* (a map's start) all the holds forgotten */
static void ae_bots_vantage_holds_reset(void)
{
	csmemset(ae_vantage_bots, 0, sizeof(ae_vantage_bots));
	csmemset(&ae_vantage_totals, 0, sizeof(ae_vantage_totals));
	ae_vantage_claims_clear();
}

/* whether a spot is held or gone to by a bot now (one dead, or spawned anew, no longer is) */
static boolean ae_vantage_claimed_by_other(long spot, long bot_index)
{
	long claim = ae_vantage_totals.claims[spot];

	if (claim == NONE || claim == bot_index || claim < 0 || claim >= BOTS_BRAINS)
		return FALSE;
	return bots_globals.bots[claim].unit_index != NONE && bots_globals.bots[claim].unit_index == ae_vantage_bots[claim].unit_index;
}

static char const *ae_vantage_weapon_name(int weapon)
{
	return weapon == AE_VANTAGE_WEAPON_HEAVY ? "heavy" : weapon == AE_VANTAGE_WEAPON_RIFLE ? "rifle" :
		weapon == AE_VANTAGE_WEAPON_ANY ? "any" : "long";
}

/* what the weapon in the bot's hands is for the holds */
static int ae_vantage_weapon_of(struct unit_datum *unit, long *rounds_left, long *rounds_most)
{
	long weapon_index = bots_current_weapon(unit);
	struct weapon_datum *weapon;
	struct weapon_definition *definition;
	char const *name;

	*rounds_left = *rounds_most = 0;
	if (weapon_index == NONE)
		return AE_VANTAGE_WEAPON_NONE;
	weapon = weapon_get(weapon_index);
	name = tag_get_name(weapon->definition_index);
	definition = weapon_definition_get(weapon->definition_index);
	if (definition->weapon.magazines.count > 0)
	{
		struct weapon_magazine_definition *magazine = TAG_BLOCK_GET_ELEMENT(&definition->weapon.magazines, 0, struct weapon_magazine_definition);

		*rounds_left = weapon->weapon.magazines[0].rounds_loaded + weapon->weapon.magazines[0].rounds_total;
		*rounds_most = magazine->rounds_loaded_maximum + magazine->rounds_total_maximum;
	}
	/* (the sniper rifle is long, a rocket launcher heavy. 0.7.8, owner: the precision rifles (the battle rifle, the DMR, the
	carbine, the EVO rifle) are utility weapons, as the pistols: they hold nothing) */
	/* (0.9.0, HIGH GROUND WHEN FREE: any other weapon holds too, bots.high_ground) */
	return ae_vantage_free_class(ae_vantage_weapon_class(name && strstr(name, "sniper"), FALSE, name && strstr(name, "rocket")), TRUE,
		ae_bots_high_ground_setting());
}

/* the spot let go: its claim, the rest, the event; why is a reason of ae_vantage_leave's */
static void ae_vantage_release(long bot_index, long now, int why, boolean failed)
{
	struct ae_vantage_bot *state = &ae_vantage_bots[bot_index];

	if (state->state == AE_VANTAGE_STATE_NONE)
		return;
	if (state->spot >= 0 && state->spot < AE_VANTAGE_MAXIMUM && ae_vantage_totals.claims[state->spot] == bot_index)
		ae_vantage_totals.claims[state->spot] = NONE;
	if (state->state == AE_VANTAGE_STATE_HOLDING)
	{
		ae_vantage_totals.leaves[why]++;
		if (ae_bots_events_on())
		{
			char extra[80];

			snprintf(extra, sizeof(extra), "spot=%d why=%s held=%.1f goal=%d src=%d", (int)state->spot + 1, ae_vantage_leave_name(why),
				(double)(now - state->arrive_tick) / TICKS_PER_SECOND, (int)bots_globals.bots[bot_index].goal_kind,
				(int)ae_bots_goal_source_of(bot_index));
			ae_bots_event_line(bot_index, now, "vantage_end", -1, -1.0f, extra);
		}
	}
	else if (ae_bots_events_on())
	{
		char extra[80];

		snprintf(extra, sizeof(extra), "spot=%d why=%s goal=%d src=%d", (int)state->spot + 1, failed ? "no_way" : ae_vantage_leave_name(why),
			(int)bots_globals.bots[bot_index].goal_kind, (int)ae_bots_goal_source_of(bot_index));
		ae_bots_event_line(bot_index, now, "vantage_stop", -1, -1.0f, extra);
	}
	if (failed && state->spot >= 0)
	{
		SET_FLAG(state->failed, state->spot, TRUE);
		state->failed_until[state->spot] = now + AE_VANTAGE_FAILED_SECONDS * TICKS_PER_SECOND;
		ae_vantage_totals.failed++;
	}
	state->last_spot = state->spot;
	state->arrived_once = FALSE;
	state->state = AE_VANTAGE_STATE_NONE;
	state->spot = (short)NONE;
	state->rest_until = now + ae_vantage_rest_ticks(why, TICKS_PER_SECOND);
}

/* the bots of this bot's team and those of them with a spot (a game without teams: all the bots) */
static void ae_vantage_team_counts(struct player_datum const *player, int *team_bots, int *holders)
{
	long index;

	*team_bots = 0;
	*holders = 0;
	for (index = 0; index < BOTS_BRAINS; index++)
	{
		struct player_datum const *other = bots_globals.bots[index].player_index != NONE ?
			player_try_and_get(bots_globals.bots[index].player_index) : NULL;

		if (!other || (game_engine_has_teams() && other->team_index != player->team_index))
			continue;
		(*team_bots)++;
		*holders += ae_vantage_bots[index].state != AE_VANTAGE_STATE_NONE &&
			ae_vantage_bots[index].unit_index == bots_globals.bots[index].unit_index;
	}
}

/* the spot a free bot takes, NONE for none */
static short ae_vantage_pick_spot(struct bots_bot *bot, long bot_index, struct player_datum *player, real_point3d const *position, long now,
	int weapon)
{
	float scores[AE_VANTAGE_MAXIMUM], distances[AE_VANTAGE_MAXIMUM], noise[AE_VANTAGE_MAXIMUM];
	struct ae_vantage_bot *state = &ae_vantage_bots[bot_index];
	unsigned long excluded = 0;
	long hill_place = NONE, index;
	boolean king = game_engine_running() && game_engine_get_variant() && game_engine_get_variant()->game_engine_index == game_engine_king;
	int count = (int)ae_bots_vantage_count();

	/* (a failed spot is tried again after a minute) */
	for (index = 0; index < count; index++)
		if (TEST_FLAG(state->failed, index) && now >= state->failed_until[index])
			SET_FLAG(state->failed, index, FALSE);
	/* (King of the Hill: over the hill, a spot that sees it within 40 units) */
	if (king)
	{
		if (!ae_bots_vantage_named_place("hill", &hill_place))
			return (short)NONE;
	}
	for (index = 0; index < count; index++)
	{
		struct ae_vantage_spot const *spot = ae_bots_vantage_spot(index);
		struct ae_vantage_place place;
		real_point3d at = { spot->x, spot->y, spot->z };

		scores[index] = spot->score;
		distances[index] = bots_distance3d(&at, position);
		noise[index] = (real)(bots_random(bot) & 0xFF) / 128.0f;
		if (ae_vantage_claimed_by_other(index, bot_index))
			excluded |= 1UL << index;
		if (hill_place != NONE && (!(spot->sees & (1UL << hill_place)) || !ae_bots_vantage_place(hill_place, &place, NULL) ||
			(real)sqrt((place.x - spot->x) * (place.x - spot->x) + (place.y - spot->y) * (place.y - spot->y)) > 40.0f))
			excluded |= 1UL << index;
	}
	(void)player;
	return (short)ae_vantage_pick(count, scores, distances, excluded, state->last_spot, state->failed,
		ae_vantage_max_distance(weapon), noise);
}

/* (bots_choose_goal, after the orders, the objective, the power items and the vehicles) a bot they left free: to a spot and holding it */
static boolean ae_bots_vantage_goal(struct bots_bot *bot, long player_index, real_point3d const *position, boolean fighting)
{
	long bot_index = (long)(bot - bots_globals.bots), now = game_time_get(), rounds_left, rounds_most;
	struct player_datum *player = player_try_and_get(player_index);
	struct ae_vantage_bot *state;
	struct unit_datum *unit;
	struct ae_vantage_spot const *spot;
	real_point3d at, ground;
	int weapon, team_bots, holders;

	if (fighting || !player || player->unit_index == NONE || bot_index < 0 || bot_index >= BOTS_BRAINS || !ae_bots_vantage_setting() ||
		!ae_bots_vantage_ready() || ae_bots_vantage_count() <= 0)
		return FALSE;
	/* (a game started again on the same map: the holds forgotten) */
	if (!ae_vantage_totals.claims_ready || now < ae_vantage_totals.last_tick)
		ae_bots_vantage_holds_reset();
	ae_vantage_totals.last_tick = now;
	state = &ae_vantage_bots[bot_index];
	if (state->unit_index != player->unit_index)
	{
		memset(state, 0, sizeof(*state));
		state->unit_index = player->unit_index;
		state->spot = (short)NONE;
		state->last_spot = (short)NONE;
		state->rest_until = now + (long)(bots_random(bot) % (unsigned long)(AE_VANTAGE_REST_SECONDS * TICKS_PER_SECOND));
		state->last_vitality = 2.0f;
	}
	state->goal_tick = now;
	/* (a wingman of a human goes where the human goes; a bot driving, seated or carrying keeps to that; a bot of a fireteam with an order
	keeps to the order, even where the fireteam's ask leaves it free: an attack at its point, a follow near its leader) */
	if (ae_bots_team_wingman_of(bot) != NONE || ae_bots_vehicle_carrying(bot, bot_index) ||
		(ae_fireteam_of_brain(bot_index) != NONE && ae_fireteam_order_of(ae_fireteam_of_brain(bot_index)) != _ae_order_none))
	{
		ae_vantage_release(bot_index, now, AE_VANTAGE_LEFT_ORDER, FALSE);
		return FALSE;
	}
	unit = unit_get(player->unit_index);
	weapon = ae_vantage_weapon_of(unit, &rounds_left, &rounds_most);
	if (state->state == AE_VANTAGE_STATE_NONE)
	{
		short chosen;

		if (weapon == AE_VANTAGE_WEAPON_NONE || now < state->rest_until || now < state->next_check)
			return FALSE;
		state->next_check = now + TICKS_PER_SECOND;
		ae_vantage_team_counts(player, &team_bots, &holders);
		if (!ae_vantage_may_take(holders, team_bots, weapon))
			return FALSE;
		chosen = ae_vantage_pick_spot(bot, bot_index, player, position, now, weapon);
		if (chosen == NONE)
			return FALSE;
		spot = ae_bots_vantage_spot(chosen);
		state->state = AE_VANTAGE_STATE_GOING;
		state->spot = chosen;
		state->weapon = (short)weapon;
		state->start_tick = now;
		state->hurt_tick = 0;
		ae_vantage_totals.claims[chosen] = bot_index;
		ae_vantage_totals.went++;
		if (ae_bots_events_on())
		{
			char extra[96];

			snprintf(extra, sizeof(extra), "spot=%d x=%.1f y=%.1f z=%.1f weapon=%s", (int)chosen + 1, (double)spot->x, (double)spot->y,
				(double)spot->z, ae_vantage_weapon_name(weapon));
			ae_bots_event_line(bot_index, now, "vantage_go", -1, -1.0f, extra);
		}
	}
	spot = ae_bots_vantage_spot(state->spot);
	if (!spot)
	{
		ae_vantage_release(bot_index, now, AE_VANTAGE_LEFT_OBJECTIVE, FALSE);
		return FALSE;
	}
	at.x = spot->x;
	at.y = spot->y;
	at.z = spot->z;
	if (state->state == AE_VANTAGE_STATE_GOING)
	{
		/* (no way to it, or too long on the way: another spot, and not this one for a minute) */
		if (now - state->start_tick > AE_VANTAGE_GO_SECONDS * TICKS_PER_SECOND ||
			(bot->goal_kind == _bots_goal_roam && bot->path_failures >= 2 && bots_distance3d(&bot->goal_point, &at) < 0.5f))
		{
			ae_vantage_release(bot_index, now, AE_VANTAGE_LEFT_TIME, TRUE);
			return FALSE;
		}
		if (bots_distance2d(&at, position) < AE_VANTAGE_ARRIVED && (real)fabs(at.z - position->z) < 2.5f)
		{
			state->state = AE_VANTAGE_STATE_HOLDING;
			state->last_vitality = object_get(player->unit_index)->object.body_vitality + object_get(player->unit_index)->object.shield_vitality;
			if (!state->arrived_once)
			{
				state->arrived_once = TRUE;
				state->arrive_tick = now;
				state->hold_ticks = ae_vantage_hold_ticks_for(state->weapon, (real)(bots_random(bot) & 0xFFFF) / 65535.0f, TICKS_PER_SECOND);
			}
			state->look_until = 0;
			ae_vantage_totals.arrived++;
			if (ae_bots_events_on())
			{
				char extra[96];

				snprintf(extra, sizeof(extra), "spot=%d weapon=%s hold=%.0f", (int)state->spot + 1,
					ae_vantage_weapon_name(state->weapon), (double)state->hold_ticks / TICKS_PER_SECOND);
				ae_bots_event_line(bot_index, now, "vantage_hold", -1, -1.0f, extra);
			}
		}
		else
		{
			if (bots_surface_below(&at, &ground) == NONE)
			{
				ae_vantage_release(bot_index, now, AE_VANTAGE_LEFT_TIME, TRUE);
				return FALSE;
			}
			if (bot->goal_kind != _bots_goal_roam || bots_distance3d(&bot->goal_point, &at) > 1.0f)
				bots_set_goal(bot, _bots_goal_roam, &at, spot->surface);
			return TRUE;
		}
	}
	/* (holding: standing there; pushed off it by a fight, back to it) */
	if (bots_distance2d(&at, position) > 3.0f || (real)fabs(at.z - position->z) > 3.0f)
	{
		state->state = AE_VANTAGE_STATE_GOING;
		state->start_tick = now;
		bots_set_goal(bot, _bots_goal_roam, &at, spot->surface);
		return TRUE;
	}
	bot->goal_kind = _bots_goal_none;
	bot->path.valid = FALSE;
	/* (the minute's line) */
	if (now - ae_vantage_totals.last_report >= 60 * TICKS_PER_SECOND)
	{
		ae_vantage_totals.last_report = now;
		platform_log("bots: vantage holds tick %ld: went %ld, arrived %ld, no way %ld; left by time %ld, enemy %ld, hurt %ld, flanked %ld, ammo %ld, "
			"weapon %ld, order %ld, objective %ld, power %ld, died %ld", now, ae_vantage_totals.went, ae_vantage_totals.arrived, ae_vantage_totals.failed,
			ae_vantage_totals.leaves[AE_VANTAGE_LEFT_TIME], ae_vantage_totals.leaves[AE_VANTAGE_LEFT_ENEMY],
			ae_vantage_totals.leaves[AE_VANTAGE_LEFT_HURT], ae_vantage_totals.leaves[AE_VANTAGE_LEFT_FLANKED],
			ae_vantage_totals.leaves[AE_VANTAGE_LEFT_AMMO], ae_vantage_totals.leaves[AE_VANTAGE_LEFT_WEAPON],
			ae_vantage_totals.leaves[AE_VANTAGE_LEFT_ORDER], ae_vantage_totals.leaves[AE_VANTAGE_LEFT_OBJECTIVE],
			ae_vantage_totals.leaves[AE_VANTAGE_LEFT_POWER], ae_vantage_totals.leaves[AE_VANTAGE_LEFT_DIED]);
	}
	return TRUE;
}

/* (ae_bots_travel_tick, every tick of every bot, before its look and fight) the spot let go the moment a reason comes: an order or the
objective took the goal (the goal asks did not come), the weapon, the flank, an enemy near, the health, the ammunition, the time */
static void ae_bots_vantage_tick(struct bots_bot *bot, long bot_index, struct bots_tick *tick)
{
	struct ae_vantage_bot *state;
	struct player_datum *player;
	struct unit_datum *unit;
	long rounds_left, rounds_most, now = tick->now;
	real vitality, enemy_distance = -1.0f;
	boolean flanked = FALSE, fighting;
	int weapon, why;

	if (bot_index < 0 || bot_index >= BOTS_BRAINS)
		return;
	state = &ae_vantage_bots[bot_index];
	if (state->state == AE_VANTAGE_STATE_NONE)
		return;
	if (state->unit_index != bot->unit_index)
	{
		/* (died: the claim goes) */
		ae_vantage_release(bot_index, now, AE_VANTAGE_LEFT_DIED, FALSE);
		return;
	}
	player = tick->player;
	unit = tick->unit;
	fighting = bot->target_player_index != NONE && now - bot->target_seen_tick <= TICKS_PER_SECOND / 2;
	if (fighting)
		state->fight_tick = now;
	/* (an order or a human's ping lets go at once, in a fight too: the hold's stand-still would keep it on the spot) */
	{
		long order_fireteam = ae_fireteam_of_brain(bot_index);

		if ((order_fireteam != NONE && ae_fireteam_order_of(order_fireteam) != _ae_order_none) || ae_bots_marks_wanted(bot))
		{
			ae_vantage_release(bot_index, now, AE_VANTAGE_LEFT_ORDER, FALSE);
			return;
		}
	}
	/* (bots_choose_goal asks the hold each tick a bot is not fighting: three ticks without it, since the last ask and the last fight,
	something took the goal) */
	if (!fighting && now - MAX(state->goal_tick, state->fight_tick) > 3)
	{
		long fireteam = ae_fireteam_of_brain(bot_index);

		/* (0.9.0: a power item or weapon took it: a short rest, then back to a spot) */
		ae_vantage_release(bot_index, now, fireteam != NONE && ae_fireteam_order_of(fireteam) != _ae_order_none ?
			AE_VANTAGE_LEFT_ORDER : ae_bots_goal_source_of(bot_index) == AE_VB_SOURCE_POWER ? AE_VANTAGE_LEFT_POWER :
			AE_VANTAGE_LEFT_OBJECTIVE, FALSE);
		return;
	}
	vitality = tick->object->object.body_vitality + tick->object->object.shield_vitality;
	if (state->state != AE_VANTAGE_STATE_HOLDING)
	{
		state->last_vitality = vitality;
		return;
	}
	weapon = ae_vantage_weapon_of(unit, &rounds_left, &rounds_most);
	/* (hurt with no enemy seen, or in a fight with its back to the one it fights, or twice in a second and a half: shot from behind) */
	if (vitality < state->last_vitality - 0.01f)
	{
		flanked = !fighting || ae_bots_combat_back_turned(tick->player->unit_index, &bot->target_last_position) ||
			(state->hurt_tick && now - state->hurt_tick < TICKS_PER_SECOND * 3 / 2 && bot->target_player_index == NONE);
		state->hurt_tick = now;
	}
	state->last_vitality = vitality;
	if (now % 5 == bot_index % 5)
	{
		/* (the nearest enemy, living, on any floor: a few ticks old at most) */
		struct data_iterator iterator;
		struct player_datum *other;

		data_iterator_new(&iterator, player_data);
		while ((other = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
		{
			if (bots_is_enemy(player, other) && object_try_and_get_and_verify_type(other->unit_index, _object_mask_unit))
			{
				real distance = bots_distance3d(&object_get(other->unit_index)->object.position, &tick->position);

				if (enemy_distance < 0.0f || distance < enemy_distance)
					enemy_distance = distance;
			}
		}
	}
	if (fighting)
	{
		real target_distance = bots_distance3d(&bot->target_last_position, &tick->position);

		if (enemy_distance < 0.0f || target_distance < enemy_distance)
			enemy_distance = target_distance;
	}
	why = ae_vantage_leave(weapon, state->weapon, flanked, enemy_distance, vitality, ae_vantage_ammunition_low((int)rounds_left, (int)rounds_most),
		now - state->arrive_tick, state->hold_ticks);
	if (why != AE_VANTAGE_STAY)
	{
		ae_vantage_release(bot_index, now, why, FALSE);
		/* (a bot hurt or flanked goes after who did it: its goal is its fight's again) */
		bot->goal_kind = _bots_goal_none;
		bot->path.valid = FALSE;
	}
}

/* (ae_bots_fight_move, last) a bot holding its spot keeps to it in a fight: no strafing, no chasing; it turns and shoots. A bot
holding high ground with any weapon (AE_VANTAGE_WEAPON_ANY) fights as anywhere: a rifle stood still is an easy target */
static void ae_bots_vantage_fight(struct bots_bot *bot, struct bots_tick *tick)
{
	long bot_index = (long)(bot - bots_globals.bots);

	if (bot_index < 0 || bot_index >= BOTS_BRAINS || ae_vantage_bots[bot_index].state != AE_VANTAGE_STATE_HOLDING ||
		ae_vantage_bots[bot_index].weapon == AE_VANTAGE_WEAPON_ANY)
		return;
	tick->moving = FALSE;
	tick->throttle_scale = 0.0f;
}

/* (ae_bots_look_about) a bot holding a spot looks from place to place of those it sees: the way it faces, about which it scans */
static boolean ae_bots_vantage_hold_yaw(long bot_index, real *yaw)
{
	struct ae_vantage_bot *state;
	struct ae_vantage_spot const *spot;
	struct ae_vantage_place place;
	long now = game_time_get(), index, count = 0, pick;
	struct bots_bot *bot;

	if (bot_index < 0 || bot_index >= BOTS_BRAINS || ae_vantage_bots[bot_index].state != AE_VANTAGE_STATE_HOLDING)
		return FALSE;
	state = &ae_vantage_bots[bot_index];
	spot = ae_bots_vantage_spot(state->spot);
	bot = &bots_globals.bots[bot_index];
	if (!spot)
		return FALSE;
	if (now >= state->look_until)
	{
		for (index = 0; index < ae_bots_vantage_place_count(); index++)
			count += (spot->sees & (1UL << index)) != 0;
		if (count > 0)
		{
			pick = (long)(bots_random(bot) % (unsigned long)count);
			for (index = 0; index < ae_bots_vantage_place_count(); index++)
				if ((spot->sees & (1UL << index)) && pick-- == 0)
					state->look_place = (short)index;
		}
		state->look_until = now + TICKS_PER_SECOND * 3 + (long)(bots_random(bot) % (unsigned long)(TICKS_PER_SECOND * 4));
	}
	if (!ae_bots_vantage_place(state->look_place, &place, NULL))
		return FALSE;
	*yaw = (real)atan2(place.y - spot->y, place.x - spot->x);
	return TRUE;
}
