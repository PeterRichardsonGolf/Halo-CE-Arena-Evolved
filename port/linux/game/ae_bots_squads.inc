/* ae_bots_squads.inc: MINI SQUADS (ae_bots_team_rules.h): a big team's bots go out in squads of 3 or 4, each squad by its
own route on the way to where they were going: a ledge, a tunnel, either side or straight on. A bot takes its squad's
route once a life, on the first goal it chooses. Fights come first and do not count against the route: its clock is
the time spent walking, the way there at 1.9 units a second and 10 s more. It drops a place it has walked past, or has
come no nearer to in 6 s of walking; a place dropped so twice is left out for the rest of the game.
The ledges and tunnels are found on the map's own floor: a grid over the roaming places, a little each tick; a ledge
stands 2.5 to 9 units over the map's middle ground, a tunnel has a roof, none within 12 units of a CTF flag (a base is
no route). Only floor joined to the roaming places counts, so hidden floor stays out. */

#define AE_SQUADS_ROUTE_POINTS 40
#define AE_SQUADS_PER_KIND 24
#define AE_SQUADS_GRID 6.0f
#define AE_SQUADS_CELLS_A_TICK 30
#define AE_SQUADS_APART 12.0f
#define AE_SQUADS_WALK_SPEED 1.9f
#define AE_SQUADS_SPARE_SECONDS 10
#define AE_SQUADS_NO_PROGRESS_SECONDS 6
#define AE_SQUADS_DROPS_TO_LEAVE_OUT 2
#define AE_SQUADS_NEAR_FLAG 12.0f
#define AE_SQUADS_EPOCH_SECONDS 120

static long ae_travel_piece(long surface);

struct ae_squads_point { real_point3d point; long surface; int kind; };

static char const *const ae_squads_kind_names[AE_BOTS_ROUTES] = { "ledge", "tunnel", "left", "right", "straight" };

static struct
{
	long last_tick;
	boolean bounds_known, scanned;
	real low_x, low_y, high_x, high_y, ground;
	long main_piece;
	long cell_x, cell_y;
	struct ae_squads_point points[AE_SQUADS_ROUTE_POINTS];
	int count, kinds[2];
	long routes[AE_BOTS_ROUTES], reached, dropped, passed;
	long last_report_tick;
	int drops[AE_SQUADS_ROUTE_POINTS];
	struct
	{
		long unit_index;
		boolean active;
		int crew;		/* 1 and up; 0 none */
		int kind;
		real_point3d point;
		long surface;
		int place;		/* its index in points, NONE for a side's roaming place */
		long last_tick, walk_ticks, walk_limit, progress_walk;
		real nearest;
	} bots[BOTS_BRAINS];
} ae_squads;

static boolean ae_squads_near_flag(real_point3d const *point)
{
	struct scenario *scenario = global_scenario_get();
	int index;

	for (index = 0; index < scenario->netgame_flags.count; index++)
	{
		struct scenario_netgame_flag *flag = TAG_BLOCK_GET_ELEMENT(&scenario->netgame_flags, index, struct scenario_netgame_flag);

		if (flag->type == _netgame_flag_ctf_flag && bots_distance2d(&flag->position, point) < AE_SQUADS_NEAR_FLAG)
			return TRUE;
	}
	return FALSE;
}

static boolean ae_squads_roofed(real_point3d const *point)
{
	struct collision_result collision;
	real_point3d start = *point;
	real_vector3d up = { 0.0f, 0.0f, 8.0f };

	start.z += 0.2f;
	return collision_test_vector(FLAG(_collision_test_structure_bit) | FLAG(_collision_test_front_facing_surfaces_bit) | FLAG(_collision_test_back_facing_surfaces_bit),
		&start, &up, NONE, &collision) && collision.type == _collision_result_structure;
}

static int ae_squads_compare(void const *a, void const *b)
{
	real left = *(real const *)a, right = *(real const *)b;

	return left < right ? -1 : left > right;
}

static void ae_squads_bounds(void)
{
	static real heights[BOTS_MAXIMUM_ROAM_POINTS];
	long pieces[BOTS_MAXIMUM_ROAM_POINTS];
	long index, other, best = 0;

	ae_squads.low_x = ae_squads.low_y = REAL_MAX;
	ae_squads.high_x = ae_squads.high_y = -REAL_MAX;
	for (index = 0; index < bots_globals.roam_point_count; index++)
	{
		real_point3d const *point = &bots_globals.roam_points[index];

		ae_squads.low_x = MIN(ae_squads.low_x, point->x);
		ae_squads.low_y = MIN(ae_squads.low_y, point->y);
		ae_squads.high_x = MAX(ae_squads.high_x, point->x);
		ae_squads.high_y = MAX(ae_squads.high_y, point->y);
		heights[index] = point->z;
		pieces[index] = ae_travel_piece(bots_globals.roam_surfaces[index]);
	}
	qsort(heights, (size_t)bots_globals.roam_point_count, sizeof(heights[0]), ae_squads_compare);
	ae_squads.ground = heights[bots_globals.roam_point_count / 2];
	ae_squads.main_piece = NONE;
	for (index = 0; index < bots_globals.roam_point_count; index++)
	{
		long same = 0;

		for (other = 0; other < bots_globals.roam_point_count; other++)
			same += pieces[other] == pieces[index];
		if (pieces[index] != NONE && same > best)
		{
			best = same;
			ae_squads.main_piece = pieces[index];
		}
	}
	ae_squads.cell_x = ae_squads.cell_y = 0;
	ae_squads.bounds_known = TRUE;
}

static void ae_squads_consider(real_point3d const *from)
{
	real_point3d ground;
	long surface = bots_surface_below(from, &ground);
	int kind, index;

	if (surface == NONE || ground.z > ae_squads.ground + 9.0f || ground.z < ae_squads.ground - 3.0f ||
		ae_travel_piece(surface) != ae_squads.main_piece || ae_squads_near_flag(&ground))
	{
		return;
	}
	if (ground.z > ae_squads.ground + 2.5f)
		kind = AE_BOTS_ROUTE_LEDGE;
	else if (ae_squads_roofed(&ground))
		kind = AE_BOTS_ROUTE_TUNNEL;
	else
		return;
	if (ae_squads.kinds[kind] >= AE_SQUADS_PER_KIND || ae_squads.count >= AE_SQUADS_ROUTE_POINTS)
		return;
	for (index = 0; index < ae_squads.count; index++)
	{
		if (ae_squads.points[index].kind == kind && bots_distance2d(&ae_squads.points[index].point, &ground) < AE_SQUADS_APART)
			return;
	}
	ae_squads.points[ae_squads.count].point = ground;
	ae_squads.points[ae_squads.count].surface = surface;
	ae_squads.points[ae_squads.count].kind = kind;
	ae_squads.count++;
	ae_squads.kinds[kind]++;
}

/* a little of the grid, a new game starting over */
static void ae_squads_scan(long now)
{
	int cell;

	if (now < ae_squads.last_tick)
	{
		long keep = now;

		csmemset(&ae_squads, 0, sizeof(ae_squads));
		ae_squads.last_tick = keep;
	}
	ae_squads.last_tick = now;
	if (ae_squads.scanned || bots_globals.roam_point_count < 2)
		return;
	if (!ae_squads.bounds_known)
		ae_squads_bounds();
	for (cell = 0; cell < AE_SQUADS_CELLS_A_TICK; cell++)
	{
		real_point3d from;
		real heights[3] = { 12.0f, 6.0f, 2.0f };
		int layer;

		from.x = ae_squads.low_x + (real)ae_squads.cell_x * AE_SQUADS_GRID;
		from.y = ae_squads.low_y + (real)ae_squads.cell_y * AE_SQUADS_GRID;
		if (from.y > ae_squads.high_y)
		{
			ae_squads.scanned = TRUE;
			platform_log("bots: mini squads: %d ledges, %d tunnels on this map", ae_squads.kinds[AE_BOTS_ROUTE_LEDGE],
				ae_squads.kinds[AE_BOTS_ROUTE_TUNNEL]);
			for (layer = 0; layer < ae_squads.count; layer++)
			{
				platform_log("bots: mini squads: %s at %.1f %.1f %.1f", ae_squads.points[layer].kind == AE_BOTS_ROUTE_LEDGE ? "ledge" : "tunnel",
					(double)ae_squads.points[layer].point.x, (double)ae_squads.points[layer].point.y, (double)ae_squads.points[layer].point.z);
			}
			return;
		}
		for (layer = 0; layer < 3; layer++)
		{
			from.z = ae_squads.ground + heights[layer];
			ae_squads_consider(&from);
		}
		ae_squads.cell_x++;
		if (ae_squads.low_x + (real)ae_squads.cell_x * AE_SQUADS_GRID > ae_squads.high_x)
		{
			ae_squads.cell_x = 0;
			ae_squads.cell_y++;
		}
	}
}

/* its squad's route for this life, the place to go by; 0: none */
static boolean ae_squads_pick(struct bots_bot *bot, long bot_index, struct player_datum *player, real_point3d const *position,
	real_point3d const *target, long now)
{
	int team_bots = 0, free_bots = 0, ordinal = 0, route, kind_index, tries;
	long index, best = NONE;
	real best_score = REAL_MAX;
	float best_lane = -1.0f;

	for (index = 0; index < BOTS_BRAINS; index++)
	{
		struct player_datum *other = bots_globals.bots[index].player_index != NONE ? player_try_and_get(bots_globals.bots[index].player_index) : NULL;

		if (other && other->team_index == player->team_index)
		{
			team_bots++;
			/* (those with a human go where the human goes; a site defender stays) */
			if (ae_bots_team_wingman_of(&bots_globals.bots[index]) != NONE || !ae_bots_team_free_for_human(&bots_globals.bots[index], other))
				continue;
			if (index < bot_index)
				ordinal++;
			free_bots++;
		}
	}
	if (ae_bots_team_mini_squads(free_bots, team_bots) == 0)
		return FALSE;
	route = ae_bots_team_mini_squad_route(ae_bots_team_mini_squad_of(ordinal, free_bots, ae_bots_team_mini_squads(free_bots, team_bots)), now / (AE_SQUADS_EPOCH_SECONDS * TICKS_PER_SECOND),
		player->team_index);
	/* (no such place on this map: the next route) */
	for (tries = 0; tries < AE_BOTS_ROUTES && (route == AE_BOTS_ROUTE_LEDGE || route == AE_BOTS_ROUTE_TUNNEL) && !ae_squads.kinds[route]; tries++)
		route = (route + 1) % AE_BOTS_ROUTES;
	ae_squads.routes[route]++;
	ae_squads.bots[bot_index].crew = ae_bots_team_mini_squad_of(ordinal, free_bots, ae_bots_team_mini_squads(free_bots, team_bots)) + 1;
	ae_squads.bots[bot_index].kind = route;
	if (route == AE_BOTS_ROUTE_STRAIGHT)
	{
		if (ae_bots_events_on())
		{
			char extra[48];

			snprintf(extra, sizeof(extra), "kind=straight crew=%d", ae_squads.bots[bot_index].crew);
			ae_bots_event_line(bot_index, now, "route", -1, -1.0f, extra);
		}
		return FALSE;
	}
	if (route == AE_BOTS_ROUTE_LEFT || route == AE_BOTS_ROUTE_RIGHT)
	{
		for (index = 0; index < bots_globals.roam_point_count; index++)
		{
			real_point3d const *point = &bots_globals.roam_points[index];
			float score;

			if ((real)fabs(point->z - position->z) > 3.0f)
				continue;
			score = ae_bots_lane_score(position->x, position->y, target->x, target->y, point->x, point->y,
				route == AE_BOTS_ROUTE_LEFT ? -1 : 1, REAL_MAX);
			if (score > best_lane)
			{
				best_lane = score;
				best = index;
			}
		}
		if (best == NONE)
			return FALSE;
		ae_squads.bots[bot_index].point = bots_globals.roam_points[best];
		ae_squads.bots[bot_index].surface = bots_globals.roam_surfaces[best];
		ae_squads.bots[bot_index].place = NONE;
	}
	else
	{
		/* (the one of its kind the least out of the way) */
		for (kind_index = 0; kind_index < ae_squads.count; kind_index++)
		{
			struct ae_squads_point const *place = &ae_squads.points[kind_index];
			real score = bots_distance2d(position, &place->point) + bots_distance2d(&place->point, target);

			if (place->kind == route && ae_squads.drops[kind_index] < AE_SQUADS_DROPS_TO_LEAVE_OUT &&
				bots_distance2d(position, &place->point) > 6.0f && score < best_score)
			{
				best_score = score;
				best = kind_index;
			}
		}
		if (best == NONE)
			return FALSE;
		ae_squads.bots[bot_index].point = ae_squads.points[best].point;
		ae_squads.bots[bot_index].surface = ae_squads.points[best].surface;
		ae_squads.bots[bot_index].place = (int)best;
	}
	ae_squads.bots[bot_index].active = TRUE;
	ae_squads.bots[bot_index].last_tick = now;
	ae_squads.bots[bot_index].walk_ticks = 0;
	ae_squads.bots[bot_index].progress_walk = 0;
	ae_squads.bots[bot_index].nearest = bots_distance2d(position, &ae_squads.bots[bot_index].point);
	ae_squads.bots[bot_index].walk_limit = (long)((ae_squads.bots[bot_index].nearest / AE_SQUADS_WALK_SPEED + AE_SQUADS_SPARE_SECONDS) * TICKS_PER_SECOND);
	if (ae_bots_events_on())
	{
		char extra[96];

		snprintf(extra, sizeof(extra), "kind=%s crew=%d x=%.1f y=%.1f z=%.1f far=%.1f", ae_squads_kind_names[route], ae_squads.bots[bot_index].crew,
			(double)ae_squads.bots[bot_index].point.x, (double)ae_squads.bots[bot_index].point.y, (double)ae_squads.bots[bot_index].point.z,
			(double)ae_squads.bots[bot_index].nearest);
		ae_bots_event_line(bot_index, now, "route", -1, -1.0f, extra);
	}
	return TRUE;
}

static void ae_squads_route_end(long bot_index, long now, char const *result, char const *why)
{
	if (ae_bots_events_on())
	{
		char extra[96];

		snprintf(extra, sizeof(extra), "kind=%s crew=%d result=%s%s%s walked=%.1f", ae_squads_kind_names[ae_squads.bots[bot_index].kind],
			ae_squads.bots[bot_index].crew, result, why[0] ? " why=" : "", why,
			(double)ae_squads.bots[bot_index].walk_ticks / TICKS_PER_SECOND);
		ae_bots_event_line(bot_index, now, "route_end", -1, -1.0f, extra);
	}
}

/* the place this bot goes by now, on its way to target */
static boolean ae_bots_squads_route(struct bots_bot *bot, long bot_index, real_point3d const *position, real_point3d const *target,
	real_point3d *point, long *surface)
{
	struct player_datum *player = player_try_and_get(bot->player_index);
	long now = game_time_get();

	if (!player || !game_engine_has_teams() || player->unit_index == NONE || bot_index < 0 || bot_index >= BOTS_BRAINS)
		return FALSE;
	ae_squads_scan(now);
	if (now - ae_squads.last_report_tick >= 60 * TICKS_PER_SECOND)
	{
		ae_squads.last_report_tick = now;
		platform_log("bots: mini squads tick %ld: routes ledge %ld, tunnel %ld, left %ld, right %ld, straight %ld; reached %ld, dropped %ld, passed %ld", now,
			ae_squads.routes[AE_BOTS_ROUTE_LEDGE], ae_squads.routes[AE_BOTS_ROUTE_TUNNEL], ae_squads.routes[AE_BOTS_ROUTE_LEFT],
			ae_squads.routes[AE_BOTS_ROUTE_RIGHT], ae_squads.routes[AE_BOTS_ROUTE_STRAIGHT], ae_squads.reached, ae_squads.dropped, ae_squads.passed);
	}
	if (!ae_squads.scanned)
		return FALSE;
	ae_bots_team_update(now);
	if (ae_bots_team_wingman_of(bot) != NONE)
	{
		ae_squads.bots[bot_index].unit_index = player->unit_index;
		ae_squads.bots[bot_index].active = FALSE;
		return FALSE;
	}
	if (ae_squads.bots[bot_index].unit_index != player->unit_index)
	{
		ae_squads.bots[bot_index].unit_index = player->unit_index;
		ae_squads.bots[bot_index].active = FALSE;
		ae_squads_pick(bot, bot_index, player, position, target, now);
	}
	if (!ae_squads.bots[bot_index].active)
		return FALSE;
	{
		/* (asked only while it walks: a longer gap is a fight, which does not count) */
		long gap = now - ae_squads.bots[bot_index].last_tick;
		real distance = bots_distance2d(position, &ae_squads.bots[bot_index].point);
		int place = ae_squads.bots[bot_index].place;

		ae_squads.bots[bot_index].last_tick = now;
		if (gap > 0 && gap <= TICKS_PER_SECOND / 2)
			ae_squads.bots[bot_index].walk_ticks += gap;
		if (distance < ae_squads.bots[bot_index].nearest - 1.0f)
		{
			ae_squads.bots[bot_index].nearest = distance;
			ae_squads.bots[bot_index].progress_walk = ae_squads.bots[bot_index].walk_ticks;
		}
		if (distance < 2.5f)
		{
			ae_squads.reached++;
			ae_squads_route_end(bot_index, now, "reached", "");
			ae_squads.bots[bot_index].active = FALSE;
			return FALSE;
		}
		if (bots_distance2d(&ae_squads.bots[bot_index].point, target) > bots_distance2d(position, target) + 10.0f)
		{
			ae_squads.passed++;
			ae_squads_route_end(bot_index, now, "passed", "");
			ae_squads.bots[bot_index].active = FALSE;
			return FALSE;
		}
		if (ae_squads.bots[bot_index].walk_ticks > ae_squads.bots[bot_index].walk_limit ||
			ae_squads.bots[bot_index].walk_ticks - ae_squads.bots[bot_index].progress_walk > AE_SQUADS_NO_PROGRESS_SECONDS * TICKS_PER_SECOND)
		{
			ae_squads.dropped++;
			ae_squads_route_end(bot_index, now, "dropped", ae_squads.bots[bot_index].walk_ticks > ae_squads.bots[bot_index].walk_limit ? "time" : "no_progress");
			if (place != NONE)
				ae_squads.drops[place]++;
			ae_squads.bots[bot_index].active = FALSE;
			return FALSE;
		}
	}
	*point = ae_squads.bots[bot_index].point;
	*surface = ae_squads.bots[bot_index].surface;
	return TRUE;
}

/* (bots_choose_goal: a team game's roaming bot) towards the enemies by its route */
static boolean ae_bots_squads_goal(struct bots_bot *bot, long player_index, real_point3d const *position, boolean fighting)
{
	struct player_datum *player = player_get(player_index);
	struct data_iterator iterator;
	struct player_datum *other;
	real_point3d target = { 0.0f, 0.0f, 0.0f }, point;
	long count = 0, surface;

	if (fighting || !game_engine_has_teams())
		return FALSE;
	data_iterator_new(&iterator, player_data);
	while ((other = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
	{
		if (bots_is_enemy(player, other))
		{
			real_point3d origin;

			object_get_origin(other->unit_index, &origin);
			target.x += origin.x;
			target.y += origin.y;
			target.z += origin.z;
			count++;
		}
	}
	if (count == 0)
		return FALSE;
	target.x /= (real)count;
	target.y /= (real)count;
	target.z /= (real)count;
	if (!ae_bots_squads_route(bot, (long)(bot - bots_globals.bots), position, &target, &point, &surface))
		return FALSE;
	bots_set_goal(bot, _bots_goal_roam, &point, surface);
	return TRUE;
}
