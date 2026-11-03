/* ae_bots_marks.inc: (the mark spike) bots answer a teammate's mark. A new mark gives a few bots of the team an order,
its marker's squad first: an enemy is somewhere to look (a place to go, as a heard shot is: never an aim or knowledge of
where the enemy is now), a weapon or item to fetch, a vehicle to get into (as a honk), a place to go. A fight, the flag
carried and the bot's own vehicle goals come first; an order that runs out is dropped. Host only: bots live there. */

enum { AE_BOTS_MARK_NONE = -1, AE_BOTS_MARK_VEHICLE = AE_MARK_KIND_COUNT, AE_BOTS_MARK_RIDE };

/* NEED A RIDE: how far a driver or a vehicle is looked for; where a driver stops by the caller; how long the order stands */
#define AE_BOTS_RIDE_REACH 80.0f
#define AE_BOTS_RIDE_STOP 6.0f
#define AE_BOTS_RIDE_SECONDS 30

#define AE_BOTS_MARK_ENEMY_REACH 60.0f
#define AE_BOTS_MARK_ARRIVED 3.0f
#define AE_BOTS_MARK_SQUAD_BONUS 25.0f

static struct ae_bots_mark_order
{
	short kind;
	long object_index;
	real_point3d point;
	long until;
	/* (NEED A RIDE) who to fetch */
	long player_index;
} ae_bots_mark_orders[BOTS_BRAINS];

static void ae_bots_mark_answer(struct player_datum const *bot_player, struct player_datum const *marker, short kind)
{
	wchar_t text[48];
	wchar_t const *line = kind == AE_BOTS_MARK_VEHICLE || kind == AE_BOTS_MARK_RIDE ? L": coming" : kind == AE_MARK_KIND_ITEM ? L": I'll get it" :
		kind == AE_MARK_KIND_ENEMY ? L": I'll check it" : L": on my way";
	short length = 0, at;
	short local;

	for (at = 0; at < 12 && bot_player->name[at] && length < 24; at++)
		text[length++] = bot_player->name[at];
	for (at = 0; line[at] && length < 47; at++)
		text[length++] = line[at];
	text[length] = 0;
	for (local = local_player_get_next(NONE); local != NONE; local = local_player_get_next(local))
	{
		long player_index = local_player_get_player_index(local);

		if (player_index != NONE && player_get(player_index)->team_index == marker->team_index)
			hud_print_message(local, text);
	}
}

/* (the host) a mark was made: the bots it goes to */
void ae_bots_marks_heard(struct player_datum const *marker, short mark_kind, long object_index, real_point3d const *spot)
{
	struct object_datum const *object = object_index != NONE ? (struct object_datum const *)object_try_and_get(object_index) : NULL;
	short kind = mark_kind;
	short wanted, picked = 0;
	boolean taken[BOTS_BRAINS];
	long now = game_time_get();
	long index;

	if (!game_engine_has_teams())
		return;
	if (kind == AE_MARK_KIND_ITEM && object && TEST_FLAG(_object_mask_vehicle, object->object.type))
		kind = AE_BOTS_MARK_VEHICLE;
	wanted = kind == AE_MARK_KIND_ENEMY ? 3 : kind == AE_MARK_KIND_ITEM ? 1 : 2;
	csmemset(taken, 0, sizeof(taken));
	while (picked < wanted)
	{
		long best = NONE;
		real best_score = 0.0f;

		for (index = 0; index < BOTS_BRAINS; index++)
		{
			struct bots_bot *bot = &bots_globals.bots[index];
			struct player_datum *player = bot->player_index != NONE ? player_try_and_get(bot->player_index) : NULL;
			real distance, score;

			if (taken[index] || !player || player == marker || player->team_index != marker->team_index ||
				!ae_bots_team_alive(player) || ae_bots_vehicle_carrying(bot, index))
			{
				continue;
			}
			distance = bots_distance3d(&object_get(player->unit_index)->object.position, spot);
			if (kind == AE_MARK_KIND_ENEMY && distance > AE_BOTS_MARK_ENEMY_REACH)
				continue;
			score = distance;
			if (ae_bots_team_wingman_of(bot) != NONE && player_try_and_get(ae_bots_team_wingman_of(bot)) == marker)
				score -= AE_BOTS_MARK_SQUAD_BONUS;
			if (best == NONE || score < best_score)
			{
				best = index;
				best_score = score;
			}
		}
		if (best == NONE)
			break;
		taken[best] = TRUE;
		{
			struct ae_bots_mark_order *order = &ae_bots_mark_orders[best];

			order->kind = kind;
			order->object_index = object_index;
			order->point = *spot;
			order->until = now + TICKS_PER_SECOND * (kind == AE_MARK_KIND_ENEMY ? 8 : 20);
			platform_log("bots: mark: bot %ld gets kind %d at %.1f %.1f %.1f", (long)(best + 1), (int)kind, (double)spot->x,
				(double)spot->y, (double)spot->z);
			if (picked == 0)
				ae_bots_mark_answer(player_get(bots_globals.bots[best].player_index), marker, kind);
		}
		picked++;
	}
}

/* the bot of a player, NONE for a human */
static long ae_bots_marks_bot_of(long player_index)
{
	long index;

	for (index = 0; index < BOTS_BRAINS; index++)
	{
		if (bots_globals.bots[index].player_index == player_index)
			return index;
	}
	return NONE;
}

/* the nearest free bot on foot of the marker's team, to this point */
static long ae_bots_marks_nearest_on_foot(struct player_datum const *marker, real_point3d const *point)
{
	long index, best = NONE;
	real best_distance = AE_BOTS_RIDE_REACH;

	for (index = 0; index < BOTS_BRAINS; index++)
	{
		struct bots_bot *bot = &bots_globals.bots[index];
		struct player_datum *player = bot->player_index != NONE ? player_try_and_get(bot->player_index) : NULL;
		real distance;

		if (!player || player == marker || player->team_index != marker->team_index || !ae_bots_team_alive(player) ||
			ae_bots_vehicle_carrying(bot, index) || object_get(player->unit_index)->object.parent_object_index != NONE)
		{
			continue;
		}
		distance = bots_distance3d(&object_get(player->unit_index)->object.position, point);
		if (distance < best_distance)
		{
			best = index;
			best_distance = distance;
		}
	}
	return best;
}

static void ae_bots_marks_ride_order(long bot_index, short kind, long vehicle_index, struct player_datum const *marker,
	long marker_index, real_point3d const *point)
{
	struct ae_bots_mark_order *order = &ae_bots_mark_orders[bot_index];

	order->kind = kind;
	order->object_index = vehicle_index;
	order->point = *point;
	order->player_index = marker_index;
	order->until = game_time_get() + TICKS_PER_SECOND * AE_BOTS_RIDE_SECONDS;
	platform_log("bots: ride: bot %ld gets kind %d, vehicle %ld", bot_index + 1, (int)kind, vehicle_index);
	ae_bots_mark_answer(player_get(bots_globals.bots[bot_index].player_index), marker, kind);
}

/* (the host) NEED A RIDE. A caller in a seat with no one at the wheel: the nearest bot on foot comes to drive. A caller on
foot: the nearest teammate driving with a seat free, a bot driving over or a human told so; with no one driving, the nearest
bot takes the nearest empty vehicle and comes. FALSE when none of that can be: the call is then a place, as any other. */
boolean ae_bots_marks_ride(struct player_datum const *marker)
{
	struct object_datum *object = marker && marker->unit_index != NONE ? (struct object_datum *)object_try_and_get(marker->unit_index) : NULL;
	struct object_iterator iterator;
	struct object_datum *candidate;
	long marker_index = NONE, index;
	long best_driven = NONE, best_empty = NONE, bot_index;
	real best_driven_distance = AE_BOTS_RIDE_REACH, best_empty_distance = AE_BOTS_RIDE_REACH;
	real_point3d point;

	if (!object || !game_engine_has_teams() || !ae_bots_vehicle_setting())
		return FALSE;
	{
		struct data_iterator players;
		struct player_datum *player;

		data_iterator_new(&players, player_data);
		while ((player = (struct player_datum *)data_iterator_next(&players)) != NULL)
		{
			if (player == marker)
				marker_index = players.datum_index;
		}
	}
	if (marker_index == NONE)
		return FALSE;
	point = object->object.position;
	/* (in a seat) */
	if (object->object.parent_object_index != NONE)
	{
		struct unit_datum *vehicle = (struct unit_datum *)ae_bots_vehicle_get(object->object.parent_object_index);

		if (!vehicle)
			return FALSE;
		/* (it has a driver, the caller perhaps: there is nothing to fetch) */
		if (vehicle->unit.driver_object_index != NONE)
			return TRUE;
		bot_index = ae_bots_marks_nearest_on_foot(marker, &vehicle->object.position);
		if (bot_index == NONE)
			return FALSE;
		ae_bots_marks_ride_order(bot_index, AE_BOTS_MARK_VEHICLE, object->object.parent_object_index, marker, marker_index, &point);
		return TRUE;
	}
	object_iterator_new(&iterator, _object_mask_vehicle, 0);
	while ((candidate = (struct object_datum *)object_iterator_next(&iterator)) != NULL)
	{
		struct unit_datum *vehicle = (struct unit_datum *)candidate;
		real distance = bots_distance3d(&candidate->object.position, &point);
		short seat_count = ae_bots_vehicle_seat_count(iterator.index), seat_index;
		boolean free_seat = FALSE;
		boolean driver_seat = FALSE;

		if (!ae_bots_vehicle_alive(candidate) || !ae_bots_vehicle_upright(candidate))
			continue;
		for (seat_index = 0; seat_index < seat_count; seat_index++)
		{
			short role = (short)ae_bots_vehicle_seat_role(iterator.index, seat_index);
			long occupant = NONE;

			if (role == AE_BOTS_SEAT_DRIVER)
				driver_seat = TRUE;
			else if (role != AE_BOTS_SEAT_NONE && unit_can_enter_seat(marker->unit_index, iterator.index, seat_index, &occupant))
				free_seat = TRUE;
		}
		if (!free_seat)
			continue;
		if (vehicle->unit.driver_object_index != NONE)
		{
			struct unit_datum *driver = unit_try_and_get(vehicle->unit.driver_object_index);
			struct player_datum *driver_player = driver && driver->unit.player_index != NONE ?
				player_try_and_get(driver->unit.player_index) : NULL;

			if (driver_player && driver_player != marker && driver_player->team_index == marker->team_index &&
				distance < best_driven_distance)
			{
				best_driven = iterator.index;
				best_driven_distance = distance;
			}
		}
		else if (driver_seat && distance < best_empty_distance)
		{
			best_empty = iterator.index;
			best_empty_distance = distance;
		}
	}
	if (best_driven != NONE)
	{
		long driver_player_index = unit_get(((struct unit_datum *)object_get(best_driven))->unit.driver_object_index)->unit.player_index;
		struct player_datum *driver_player = player_get(driver_player_index);

		bot_index = ae_bots_marks_bot_of(driver_player_index);
		if (bot_index != NONE)
		{
			ae_bots_marks_ride_order(bot_index, AE_BOTS_MARK_RIDE, best_driven, marker, marker_index, &point);
			return TRUE;
		}
		/* (a human at the wheel on this machine is told; a remote one has the call's marker and line) */
		if (driver_player->local_player_index != NONE)
		{
			wchar_t text[48];
			wchar_t const *line = L" needs a ride";
			short length = 0, at;

			for (at = 0; at < 12 && marker->name[at]; at++)
				text[length++] = marker->name[at];
			for (at = 0; line[at] && length < 47; at++)
				text[length++] = line[at];
			text[length] = 0;
			hud_print_message(driver_player->local_player_index, text);
		}
		return TRUE;
	}
	if (best_empty == NONE)
		return FALSE;
	bot_index = ae_bots_marks_nearest_on_foot(marker, &object_get(best_empty)->object.position);
	if (bot_index == NONE)
		return FALSE;
	ae_bots_marks_ride_order(bot_index, AE_BOTS_MARK_RIDE, best_empty, marker, marker_index, &point);
	(void)index;
	return TRUE;
}

/* the order still standing: NULL when none (a new game's times start again: an order past its length is an old one) */
static struct ae_bots_mark_order *ae_bots_mark_order_of(struct bots_bot *bot)
{
	long bot_index = (long)(bot - bots_globals.bots);
	long now = game_time_get();
	struct ae_bots_mark_order *order;

	if (bot_index < 0 || bot_index >= BOTS_BRAINS)
		return NULL;
	order = &ae_bots_mark_orders[bot_index];
	if (!order->until)
		return NULL;
	if (now >= order->until || order->until - now > TICKS_PER_SECOND * AE_BOTS_RIDE_SECONDS)
	{
		order->until = 0;
		return NULL;
	}
	return order;
}

/* the vehicle a mark calls this bot to (ae_bots_vehicle_called) */
static long ae_bots_marks_vehicle(struct bots_bot *bot)
{
	struct ae_bots_mark_order *order = ae_bots_mark_order_of(bot);

	if (!order || (order->kind != AE_BOTS_MARK_VEHICLE && order->kind != AE_BOTS_MARK_RIDE) ||
		!object_try_and_get_and_verify_type(order->object_index, _object_mask_vehicle))
	{
		return NONE;
	}
	return order->object_index;
}

/* (a driver with NEED A RIDE's order) to the caller where they stand now, and it waits beside them; FALSE when it has no
such order, or the caller is aboard or gone */
static boolean ae_bots_marks_ride_goal(struct bots_bot *bot, real_point3d const *position)
{
	struct ae_bots_mark_order *order = ae_bots_mark_order_of(bot);
	struct player_datum const *caller;
	real_point3d ground, at;
	long surface_index;

	if (!order || order->kind != AE_BOTS_MARK_RIDE)
		return FALSE;
	caller = player_try_and_get(order->player_index);
	if (!caller || !ae_bots_team_alive(caller) || object_get(caller->unit_index)->object.parent_object_index != NONE)
	{
		order->until = 0;
		return FALSE;
	}
	at = object_get(caller->unit_index)->object.position;
	if (bots_distance2d(&at, position) < AE_BOTS_RIDE_STOP)
	{
		bot->goal_kind = _bots_goal_none;
		bot->path.valid = FALSE;
		return TRUE;
	}
	surface_index = bots_surface_below(&at, &ground);
	if (surface_index == NONE)
	{
		order->until = 0;
		return FALSE;
	}
	if (bot->goal_kind != _bots_goal_roam || bots_distance3d(&bot->goal_point, &ground) > 3.0f)
		bots_set_goal(bot, _bots_goal_roam, &ground, surface_index);
	return TRUE;
}

static boolean ae_bots_marks_riding(struct bots_bot *bot)
{
	struct ae_bots_mark_order *order = ae_bots_mark_order_of(bot);

	return (boolean)(order && order->kind == AE_BOTS_MARK_RIDE);
}

static boolean ae_bots_marks_goal(struct bots_bot *bot, long player_index, real_point3d const *position, boolean fighting)
{
	struct ae_bots_mark_order *order = ae_bots_mark_order_of(bot);
	long bot_index = (long)(bot - bots_globals.bots);
	short goal_kind = _bots_goal_roam;
	real_point3d ground;
	long surface_index;

	if (!order || fighting || order->kind == AE_BOTS_MARK_VEHICLE || order->kind == AE_BOTS_MARK_RIDE)
		return FALSE;
	if (ae_bots_vehicle_carrying(bot, bot_index))
	{
		order->until = 0;
		return FALSE;
	}
	if (order->kind == AE_MARK_KIND_ITEM)
	{
		struct object_datum const *item = object_try_and_get_and_verify_type(order->object_index,
			_object_mask_weapon | _object_mask_equipment);

		if (!item || item->object.parent_object_index != NONE)
		{
			order->until = 0;
			return FALSE;
		}
		surface_index = bots_surface_below(&item->object.position, &ground);
		if (TEST_FLAG(_object_mask_weapon, item->object.type))
			goal_kind = _bots_goal_item;
	}
	else
	{
		surface_index = bots_surface_below(&order->point, &ground);
		if (bots_distance2d(&ground, position) < AE_BOTS_MARK_ARRIVED)
		{
			order->until = 0;
			return FALSE;
		}
	}
	if (surface_index == NONE)
	{
		order->until = 0;
		return FALSE;
	}
	/* (the path kept while the goal stands: a goal set every tick restarts the search and the bot never walks) */
	if (bot->goal_kind != goal_kind || bots_distance3d(&bot->goal_point, &ground) > 1.0f)
	{
		bots_set_goal(bot, goal_kind, &ground, surface_index);
		if (goal_kind == _bots_goal_item)
			bot->item_index = order->object_index;
	}
	(void)player_index;
	return TRUE;
}
