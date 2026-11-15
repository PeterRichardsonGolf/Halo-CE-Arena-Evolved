/*
AE_BOTS_COVER.INC

Included by ae_bots_lan.inc, so at the end of bots.c, before the objective play (ae_bots_objective.inc, which calls it):
an objective carrier (the Oddball's holder, the juggernaut breaking away, Reverse Tag's "it") hides and holds
(ae_bots_cover_rules.h). Owner 2026-10-25: dynamic, from the map's own geometry, any map with no per-map work.

- PLACES: the bots' walkable surfaces (bots.c's copy, once its scan for surfaces with no room above is done) sampled at
  their middles and halfway to their corners, a few dozen a tick from the first carrier's game, each probed with the
  level's collision (eight sides at the chest, one up); the covered ones kept (AE_BOTS_COVER_PLACES at most, a spacing
  apart). Nothing is written anywhere; a new map starts again. Logged once: how many, the rays and the time.
- KNOWLEDGE: the enemies the carrier may know of, as a player does: on its motion tracker (within its reach), the one
  it last saw (bots.c's target, ten seconds), those its teammates' bots saw in the last three seconds (as
  ae_bots_objective_spotted shares them). Never where an enemy is that nothing told it of.
- SEEN: whether enemies have a line to the carrier now (ae_bots_team_exposed's test, every ae_bots_cover_watch_ticks).
- A test switch HALO_BOTS_BALL_COVER=0 keeps the old roaming carrier (A/B games) and does no scan or probe; the carries
  and ball time are counted either way and logged a minute at a time ("bots: cover ...").
*/

#include "ae_bots_cover_rules.h"
#include "ae_bots_objective.h"

/* (ae_bots_travel.inc's, after this file) */
static boolean ae_bots_travel_near_pad(real_point3d const *point, real radius);
static long ae_travel_piece(long surface);

/* the enemies a carrier's motion tracker shows (as ae_bots_objective.inc's holder keeps away from) */
#define AE_BOTS_COVER_TRACKER 25.0f

#define AE_BOTS_COVER_PLACES 1024
/* the scan's probes a tick (ten a sample) */
#define AE_BOTS_COVER_RAYS_A_TICK 320
/* a place given up (not reached, or just left) is not picked again for this long */
#define AE_BOTS_COVER_BAN_TICKS (10 * TICKS_PER_SECOND)
#define AE_BOTS_COVER_BANS 4
/* a spot not reached in this long is given up */
#define AE_BOTS_COVER_REACH_TICKS (12 * TICKS_PER_SECOND)
/* at the spot: this near across, this much up or down */
#define AE_BOTS_COVER_THERE 0.8f
/* the carrier's own counts sampled this often */
#define AE_BOTS_COVER_SAMPLE_TICKS 15

static struct
{
	struct structure_bsp const *source;
	boolean done;
	boolean logged;
	long surface_index;
	long sample;
	long samples;
	long rays;
	long ticks;
	double seconds;
	long count;
	real_point3d point[AE_BOTS_COVER_PLACES];
	long surface[AE_BOTS_COVER_PLACES];
	unsigned char walls[AE_BOTS_COVER_PLACES];
	unsigned char roof[AE_BOTS_COVER_PLACES];
	unsigned char score[AE_BOTS_COVER_PLACES];
} ae_bots_cover;

/* the counts of the minute's line (the game's so far) */
static struct
{
	long last_tick;
	long report_tick;
	long carry_ticks;
	long samples_carrying;
	long samples_covered;
	long samples_seen;
	long samples_holding;
	long carries;
	long picks;
	long picks_unseen;
	long fallbacks;
	long leaves[5];
	long team_ticks[2];
	long ffa_ticks;
} ae_bots_cover_counts;

struct ae_bots_cover_bot
{
	long player_index;
	/* the place it goes to or holds (an index of the places), NONE for none */
	long place;
	long chosen_tick;
	long arrived_tick;
	long check_tick;
	/* the enemies with a line to it, and those of them near (for a camouflaged carrier), counted then */
	long watch_tick;
	short seen_by;
	short seen_by_near;
	/* places it gave up a while */
	long banned[AE_BOTS_COVER_BANS];
	long banned_until[AE_BOTS_COVER_BANS];
	long banned_next;
	/* it carried when last counted (a new carry counted), and that tick */
	boolean carried;
	long carry_tick;
	long sample_tick;
	/* where it looks while it holds (radians) */
	real look;
	/* no spot found: not looked for again before then (the places' loop is a thousand long) */
	long pick_again_tick;
};

static struct ae_bots_cover_bot ae_bots_cover_bots[BOTS_BRAINS];

static boolean ae_bots_cover_on(void)
{
	static int on = -1;

	if (on < 0)
	{
		char const *text = getenv("HALO_BOTS_BALL_COVER");

		on = text && text[0] == '0' ? 0 : 1;
	}
	return on > 0;
}

static double ae_bots_cover_clock(void)
{
	LARGE_INTEGER now, frequency;

	QueryPerformanceCounter(&now);
	QueryPerformanceFrequency(&frequency);
	return (double)now.QuadPart / (double)frequency.QuadPart;
}

/* the eight sides and the roof over a ground point: the walls' bitmask, the roof; FALSE for no room to stand there */
static boolean ae_bots_cover_probe(real_point3d const *ground, unsigned *walls, boolean *roof, long *rays)
{
	unsigned long flags = FLAG(_collision_test_structure_bit) | FLAG(_collision_test_front_facing_surfaces_bit) |
		FLAG(_collision_test_objects_bit) | FLAG(_collision_test_objects_scenery_bit) |
		FLAG(_collision_test_objects_machines_bit);
	struct collision_result collision;
	real_point3d start = *ground;
	real_vector3d vector = { 0.0f, 0.0f, AE_BOTS_COVER_HEADROOM };
	long side;

	start.z += 0.05f;
	(*rays)++;
	if (collision_test_vector(flags | FLAG(_collision_test_back_facing_surfaces_bit), &start, &vector, NONE, &collision))
		return FALSE;
	vector.k = AE_BOTS_COVER_ROOF_REACH;
	(*rays)++;
	*roof = collision_test_vector(flags, &start, &vector, NONE, &collision) &&
		collision.type == _collision_result_structure;
	*walls = 0;
	start.z = ground->z + AE_BOTS_COVER_CHEST;
	for (side = 0; side < AE_BOTS_COVER_SIDES; side++)
	{
		real angle = (real)side * 2.0f * BOTS_PI / (real)AE_BOTS_COVER_SIDES;

		vector.i = (real)cos(angle) * AE_BOTS_COVER_WALL_REACH;
		vector.j = (real)sin(angle) * AE_BOTS_COVER_WALL_REACH;
		vector.k = 0.0f;
		(*rays)++;
		if (collision_test_vector(flags, &start, &vector, NONE, &collision))
			*walls |= 1u << side;
	}
	return TRUE;
}

/* the places, a few a tick: FALSE until there are any to pick from (the scan goes on) */
static void ae_bots_cover_scan(void)
{
	struct collision_bsp const *bsp;
	long rays = 0;
	double started;

	if (!bots_paths.walkable || !bots_paths.source)
		return;
	if (ae_bots_cover.source != bots_paths.source)
	{
		csmemset(&ae_bots_cover, 0, sizeof(ae_bots_cover));
		ae_bots_cover.source = bots_paths.source;
		ae_bots_cover.sample = -1;
	}
	if (ae_bots_cover.done || !bots_paths.scan_done)
		return;
	started = ae_bots_cover_clock();
	ae_bots_cover.ticks++;
	bsp = TAG_BLOCK_GET_ELEMENT(&bots_paths.structure.collision_bsp, 0, struct collision_bsp);
	while (rays < AE_BOTS_COVER_RAYS_A_TICK)
	{
		real_point3d corners[BOTS_SURFACE_CORNERS], middle, point, ground;
		long corner_count, index, surface;
		real reach;
		unsigned walls;
		boolean roof, near = FALSE;

		if (ae_bots_cover.surface_index >= bsp->surfaces.count || ae_bots_cover.count >= AE_BOTS_COVER_PLACES)
		{
			ae_bots_cover.done = TRUE;
			break;
		}
		if (!bots_paths_walkable_surface(bsp, ae_bots_cover.surface_index, corners, &corner_count, &middle, &reach) ||
			ae_bots_cover.sample >= corner_count || (ae_bots_cover.sample >= 0 && reach < 1.0f))
		{
			ae_bots_cover.surface_index++;
			ae_bots_cover.sample = -1;
			continue;
		}
		point = middle;
		if (ae_bots_cover.sample >= 0)
		{
			point.x = 0.5f * (middle.x + corners[ae_bots_cover.sample].x);
			point.y = 0.5f * (middle.y + corners[ae_bots_cover.sample].y);
			point.z = 0.5f * (middle.z + corners[ae_bots_cover.sample].z);
		}
		ae_bots_cover.sample++;
		ae_bots_cover.samples++;
		for (index = 0; index < ae_bots_cover.count && !near; index++)
			near = bots_distance3d(&ae_bots_cover.point[index], &point) < AE_BOTS_COVER_SPACING;
		if (near)
			continue;
		rays++;
		surface = bots_surface_below(&point, &ground);
		if (surface == NONE || surface >= bots_paths.structure.pathfinding_surfaces.count ||
			!TEST_FLAG(bots_paths.walkable[surface], BOTS_PATHFINDING_WALKABLE_BIT) ||
			!ae_bots_cover_probe(&ground, &walls, &roof, &rays) || ae_bots_cover_score(walls, roof) <= 0)
		{
			continue;
		}
		index = ae_bots_cover.count++;
		ae_bots_cover.point[index] = ground;
		ae_bots_cover.surface[index] = surface;
		ae_bots_cover.walls[index] = (unsigned char)walls;
		ae_bots_cover.roof[index] = (unsigned char)roof;
		ae_bots_cover.score[index] = (unsigned char)ae_bots_cover_score(walls, roof);
	}
	ae_bots_cover.rays += rays;
	ae_bots_cover.seconds += ae_bots_cover_clock() - started;
	if (ae_bots_cover.done && !ae_bots_cover.logged)
	{
		ae_bots_cover.logged = TRUE;
		platform_log("bots: cover: %ld places of %ld samples (%ld rays, %.1f ms over %ld ticks)", ae_bots_cover.count,
			ae_bots_cover.samples, ae_bots_cover.rays, ae_bots_cover.seconds * 1000.0, ae_bots_cover.ticks);
	}
}

/* the enemies the bot knows of now (where it knows them to be), at most max; their count */
static long ae_bots_cover_known(struct bots_bot *bot, struct player_datum *player, real_point3d const *position,
	real_point3d *known, long max)
{
	struct data_iterator iterator;
	struct player_datum *other;
	long now = game_time_get(), count = 0;

	data_iterator_new(&iterator, player_data);
	while ((other = (struct player_datum *)data_iterator_next(&iterator)) != NULL && count < max)
	{
		real_point3d where;
		boolean found = FALSE;
		long index;

		if (!bots_is_enemy(player, other) || !ae_bots_team_alive(other))
			continue;
		where = ae_bots_vehicle_point(other->unit_index);
		/* (its motion tracker) */
		if (bots_distance3d(&where, position) < AE_BOTS_COVER_TRACKER)
			found = TRUE;
		/* (its own sight, then its teammates' bots', as they share it; never a sighting of the enemy's last life: where it
		was before it died says nothing of where it is now) */
		else if (bot->target_player_index == iterator.datum_index && now - bot->target_seen_tick < TICKS_PER_SECOND * 10 &&
			bot->target_seen_tick > other->death_time)
		{
			where = bot->target_last_position;
			found = TRUE;
		}
		for (index = 0; !found && index < BOTS_BRAINS; index++)
		{
			struct bots_bot *mate = &bots_globals.bots[index];
			struct player_datum *mate_player;

			if (mate == bot || mate->player_index == NONE || mate->target_player_index != iterator.datum_index ||
				now - mate->target_seen_tick > TICKS_PER_SECOND * 3 || mate->target_seen_tick <= other->death_time ||
				(mate_player = player_try_and_get(mate->player_index)) == NULL || bots_is_enemy(player, mate_player))
			{
				continue;
			}
			where = mate->target_last_position;
			found = TRUE;
		}
		if (found)
			known[count++] = where;
	}
	return count;
}

/* the enemies (their eyes as their bounding spheres' centres) with a line to the point, within the sight's reach; and
how many of those are within near */
static short ae_bots_cover_seen(struct player_datum *player, real_point3d const *point, real near, short *seen_near)
{
	struct data_iterator iterator;
	struct player_datum *other;
	short seen = 0;

	*seen_near = 0;
	data_iterator_new(&iterator, player_data);
	while ((other = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
	{
		struct object_datum const *object;
		real distance;

		if (!bots_is_enemy(player, other) || !ae_bots_team_alive(other))
			continue;
		object = object_get(other->unit_index);
		distance = bots_distance3d(point, &object->object.bounding_sphere_center);
		if (distance > AE_BOTS_COVER_SIGHT)
			continue;
		if (bots_line_of_sight(other->unit_index, &object->object.bounding_sphere_center, point, NONE))
		{
			seen++;
			if (distance < near)
				(*seen_near)++;
		}
	}
	return seen;
}

static boolean ae_bots_cover_banned(struct ae_bots_cover_bot *state, long place, long now)
{
	long index;

	for (index = 0; index < AE_BOTS_COVER_BANS; index++)
	{
		if (state->banned[index] == place && now < state->banned_until[index])
			return TRUE;
	}
	return FALSE;
}

static void ae_bots_cover_ban(struct ae_bots_cover_bot *state, long place, long now)
{
	if (place == NONE)
		return;
	state->banned[state->banned_next] = place;
	state->banned_until[state->banned_next] = now + AE_BOTS_COVER_BAN_TICKS;
	state->banned_next = (state->banned_next + 1) % AE_BOTS_COVER_BANS;
}

/* the distance from the point to the segment a-b, across */
static real ae_bots_cover_segment_distance(real_point3d const *point, real_point3d const *a, real_point3d const *b)
{
	real dx = b->x - a->x, dy = b->y - a->y, length = dx * dx + dy * dy, t;
	real_point3d nearest = *a;

	if (length > 0.0001f)
	{
		t = ((point->x - a->x) * dx + (point->y - a->y) * dy) / length;
		t = t < 0.0f ? 0.0f : t > 1.0f ? 1.0f : t;
		nearest.x = a->x + dx * t;
		nearest.y = a->y + dy * t;
	}
	return bots_distance2d(point, &nearest);
}

/* the hide spot for a carrier: NONE for none (no places, or none near) */
#define AE_BOTS_COVER_MAXIMUM_CANDIDATES 12
static long ae_bots_cover_pick(struct bots_bot *bot, struct ae_bots_cover_bot *state, struct player_datum *player,
	real_point3d const *position, int level)
{
	real_point3d known[BOTS_BRAINS + MAXIMUM_LOCAL_PLAYERS];
	long known_count = ae_bots_cover_known(bot, player, position, known, (long)NUMBEROF(known));
	long candidates[AE_BOTS_COVER_MAXIMUM_CANDIDATES];
	real scores[AE_BOTS_COVER_MAXIMUM_CANDIDATES];
	long candidate_count = 0, wanted = ae_bots_cover_candidates(level), index, best = NONE;
	long now = game_time_get(), own_piece = NONE, pass;
	real best_score = -REAL_MAX;

	if (wanted > AE_BOTS_COVER_MAXIMUM_CANDIDATES)
		wanted = AE_BOTS_COVER_MAXIMUM_CANDIDATES;
	/* (the places on the carrier's own piece of the walkable surfaces first, which its searches reach; the others only
	when it has none near) */
	{
		real_point3d ground;
		long surface = player->unit_index != NONE ? bots_unit_surface(player->unit_index, &ground) : NONE;

		own_piece = surface != NONE ? ae_travel_piece(surface) : NONE;
	}
	for (pass = own_piece != NONE ? 0 : 1; pass < 2 && !candidate_count; pass++)
	{
		for (index = 0; index < ae_bots_cover.count; index++)
		{
			real_point3d const *point = &ae_bots_cover.point[index];
			real own = bots_distance2d(point, position), threat = -1.0f, score;
			int allies = 0, through = 0;
			long enemy, slot;
			struct data_iterator iterator;
			struct player_datum *other;

			if (own < AE_BOTS_COVER_NEAREST || own > AE_BOTS_COVER_FARTHEST || fabs(point->z - position->z) > AE_BOTS_COVER_HEIGHT ||
				index == state->place || ae_bots_cover_banned(state, index, now) || ae_bots_travel_near_pad(point, 1.0f) ||
				(!pass && ae_travel_piece(ae_bots_cover.surface[index]) != own_piece))
			{
				continue;
			}
			for (enemy = 0; enemy < known_count; enemy++)
			{
				real d = bots_distance3d(point, &known[enemy]);

				if (threat < 0.0f || d < threat)
					threat = d;
				/* (the way there passes by it, and the enemy is nearer the spot than the carrier is) */
				if (ae_bots_cover_segment_distance(&known[enemy], position, point) < 4.0f &&
					bots_distance2d(&known[enemy], position) > 1.0f)
				{
					through = 1;
				}
			}
			if (game_engine_has_teams())
			{
				data_iterator_new(&iterator, player_data);
				while ((other = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
				{
					real_point3d where;

					if (other == player || bots_is_enemy(player, other) || other->unit_index == NONE)
						continue;
					where = ae_bots_vehicle_point(other->unit_index);
					if (bots_distance3d(&where, point) < 8.0f)
						allies++;
				}
			}
			score = ae_bots_cover_spot_score(level, own, threat, ae_bots_cover.score[index], allies, through,
				bots_random_real(bot));
			/* (kept: the best few, best first) */
			if (candidate_count == wanted && score <= scores[candidate_count - 1])
				continue;
			slot = candidate_count < wanted ? candidate_count++ : candidate_count - 1;
			while (slot > 0 && scores[slot - 1] < score)
			{
				scores[slot] = scores[slot - 1];
				candidates[slot] = candidates[slot - 1];
				slot--;
			}
			scores[slot] = score;
			candidates[slot] = index;
		}
	}
	/* (the best few tried with the enemies' lines: an unseen one ends the look) */
	for (index = 0; index < candidate_count; index++)
	{
		real_point3d eye = ae_bots_cover.point[candidates[index]];
		short seen, seen_near;
		real score;

		eye.z += AE_BOTS_COVER_CHEST + 0.1f;
		seen = ae_bots_cover_seen(player, &eye, 0.0f, &seen_near);
		score = scores[index] - ae_bots_cover_seen_penalty(level, seen);
		if (score > best_score)
		{
			best_score = score;
			best = candidates[index];
		}
		if (!seen)
		{
			ae_bots_cover_counts.picks_unseen++;
			break;
		}
	}
	if (best != NONE)
		ae_bots_cover_counts.picks++;
	return best;
}

/* the carrier's state, NULL for none */
static struct ae_bots_cover_bot *ae_bots_cover_state(struct bots_bot *bot)
{
	long bot_index = (long)(bot - bots_globals.bots);
	struct ae_bots_cover_bot *state;

	if (bot_index < 0 || bot_index >= BOTS_BRAINS)
		return NULL;
	state = &ae_bots_cover_bots[bot_index];
	if (state->player_index != bot->player_index)
	{
		csmemset(state, 0, sizeof(*state));
		state->player_index = bot->player_index;
		state->place = NONE;
	}
	return state;
}

/* the carrier's goal (ae_bots_objective_set_goal): 0 none here (the old roaming away), 1 go to *point, 2 hold where it is */
static int ae_bots_cover_goal(struct bots_bot *bot, real_point3d const *position, real_point3d *point)
{
	struct ae_bots_cover_bot *state = ae_bots_cover_state(bot);
	struct player_datum *player = player_try_and_get(bot->player_index);
	int level = ae_bots_level_of_skill(bots_globals.skill);
	long now = game_time_get();
	boolean camouflaged;

	if (!state || !player || player->unit_index == NONE || !ae_bots_cover_on())
		return 0;
	ae_bots_cover_scan();
	if (!ae_bots_cover.count)
		return 0;
	camouflaged = TEST_FLAG(unit_get(player->unit_index)->unit.flags, _unit_active_camouflaged_bit);
	/* (a spot went bad: given up) */
	if (state->place != NONE && state->place < ae_bots_cover.count)
	{
		real_point3d const *spot = &ae_bots_cover.point[state->place];
		boolean there = bots_distance2d(spot, position) < AE_BOTS_COVER_THERE && fabs(spot->z - position->z) < 1.5f;

		if (there)
		{
			int leave;

			if (!state->arrived_tick)
			{
				state->arrived_tick = now;
				state->watch_tick = 0;
				state->seen_by = state->seen_by_near = 0;
			}
			/* (seen: looked at again now and then) */
			if (now >= state->watch_tick)
			{
				real_point3d eye;

				unit_get_head_position(player->unit_index, &eye);
				state->seen_by = ae_bots_cover_seen(player, &eye, 8.0f, &state->seen_by_near);
				state->watch_tick = now + ae_bots_cover_watch_ticks(level);
			}
			{
				real_point3d known[BOTS_BRAINS + MAXIMUM_LOCAL_PLAYERS];
				long known_count = ae_bots_cover_known(bot, player, position, known, (long)NUMBEROF(known)), index;
				real nearest = -1.0f;

				for (index = 0; index < known_count; index++)
				{
					real d = bots_distance3d(&known[index], position);

					if (nearest < 0.0f || d < nearest)
					{
						nearest = d;
						state->look = (real)atan2(known[index].y - position->y, known[index].x - position->x);
					}
				}
				if (nearest < 0.0f || nearest > 30.0f)
				{
					int side = ae_bots_cover_open_side(ae_bots_cover.walls[state->place]);

					if (side >= 0)
						state->look = (real)side * 2.0f * BOTS_PI / (real)AE_BOTS_COVER_SIDES;
				}
				leave = ae_bots_cover_leave(level, nearest, state->seen_by, state->seen_by_near,
					now - bot->hurt_tick < TICKS_PER_SECOND / 2 && bot->hurt_tick >= state->arrived_tick,
					now - state->arrived_tick, camouflaged);
			}
			if (!leave)
				return 2;
			ae_bots_cover_counts.leaves[leave]++;
			ae_bots_cover_ban(state, state->place, now);
			state->place = NONE;
		}
		else
		{
			state->arrived_tick = 0;
			/* (not reached in time, stuck on the way, or an enemy it knows of by it now: another) */
			if (now - state->chosen_tick > AE_BOTS_COVER_REACH_TICKS || bot->stuck_count >= 2)
			{
				ae_bots_cover_ban(state, state->place, now);
				state->place = NONE;
			}
			else if (now >= state->check_tick)
			{
				real_point3d known[BOTS_BRAINS + MAXIMUM_LOCAL_PLAYERS];
				long known_count = ae_bots_cover_known(bot, player, position, known, (long)NUMBEROF(known)), index;

				state->check_tick = now + TICKS_PER_SECOND;
				for (index = 0; index < known_count && state->place != NONE; index++)
				{
					if (bots_distance3d(&known[index], spot) < AE_BOTS_COVER_THREAT_NEAR * 0.75f)
					{
						ae_bots_cover_ban(state, state->place, now);
						state->place = NONE;
					}
				}
			}
		}
	}
	else
		state->place = NONE;
	if (state->place == NONE)
	{
		if (now < state->pick_again_tick)
			return 0;
		state->place = ae_bots_cover_pick(bot, state, player, position, level);
		state->chosen_tick = now;
		state->arrived_tick = 0;
		state->check_tick = now + TICKS_PER_SECOND;
		if (state->place == NONE)
		{
			state->pick_again_tick = now + TICKS_PER_SECOND;
			ae_bots_cover_counts.fallbacks++;
			return 0;
		}
	}
	*point = ae_bots_cover.point[state->place];
	return 1;
}

/* The carrier decision reads the cover already chosen; it does not start a scan or choose a new one. */
static boolean ae_bots_cover_carrier_place(struct bots_bot *bot, real_point3d const *position, real *distance,
	boolean *holding)
{
	struct ae_bots_cover_bot *state = ae_bots_cover_state(bot);

	*distance = -1.0f;
	*holding = FALSE;
	if (!state || state->place == NONE || state->place >= ae_bots_cover.count)
		return FALSE;
	*distance = bots_distance3d(position, &ae_bots_cover.point[state->place]);
	*holding = state->arrived_tick != 0;
	return TRUE;
}

/* the carrier holding its spot (ae_bots_objective_move, its goal none): crouched, looking out (not in a fight) */
static void ae_bots_cover_pose(struct bots_bot *bot, struct bots_tick *tick)
{
	struct ae_bots_cover_bot *state = ae_bots_cover_state(bot);
	real turn = 6.0f * BOTS_DEGREES, off;

	if (!state || state->place == NONE || !state->arrived_tick || tick->fighting || bot->goal_kind != _bots_goal_none ||
		!ae_bots_cover_on())
	{
		return;
	}
	tick->moving = FALSE;
	off = bots_angle_difference(state->look, bot->yaw);
	bot->yaw += PIN(off, -turn, turn);
	bot->pitch += PIN(-bot->pitch, -0.05f, 0.05f);
	if (ae_bots_level_of_skill(bots_globals.skill) >= AE_BOTS_LEVEL_NORMAL)
		tick->flags |= FLAG(_unit_control_crouch_modifier_bit);
}

/* an escort's place by a carrier who holds a spot: its open side, two units out, facing out; FALSE when the carrier
holds none (the escort follows it as ever) */
static boolean ae_bots_cover_escort_point(long carrier_player_index, real_point3d *point, real *facing)
{
	long index;

	if (!ae_bots_cover_on())
		return FALSE;
	for (index = 0; index < BOTS_BRAINS; index++)
	{
		struct ae_bots_cover_bot const *state = &ae_bots_cover_bots[index];
		real_point3d spot, ground;
		int side;

		if (bots_globals.bots[index].player_index != carrier_player_index || state->player_index != carrier_player_index ||
			state->place == NONE || state->place >= ae_bots_cover.count || !state->arrived_tick)
		{
			continue;
		}
		side = ae_bots_cover_open_side(ae_bots_cover.walls[state->place]);
		if (side < 0)
			return FALSE;
		*facing = (real)side * 2.0f * BOTS_PI / (real)AE_BOTS_COVER_SIDES;
		spot = ae_bots_cover.point[state->place];
		spot.x += (real)cos(*facing) * 2.0f;
		spot.y += (real)sin(*facing) * 2.0f;
		spot.z += 0.5f;
		if (bots_surface_below(&spot, &ground) == NONE || fabs(ground.z - ae_bots_cover.point[state->place].z) > 1.0f)
			return FALSE;
		*point = ground;
		return TRUE;
	}
	return FALSE;
}

/* an escort at its place by a holding carrier (its goal none, not in a fight): it faces out */
static void ae_bots_cover_escort_pose(struct bots_bot *bot, struct bots_tick *tick, long carrier_player_index)
{
	real_point3d guard;
	real facing, turn = 6.0f * BOTS_DEGREES, off;

	if (tick->fighting || bot->goal_kind != _bots_goal_none ||
		!ae_bots_cover_escort_point(carrier_player_index, &guard, &facing))
	{
		return;
	}
	off = bots_angle_difference(facing, bot->yaw);
	bot->yaw += PIN(off, -turn, turn);
}

/* the counts, once a tick (the first bot's) and a carrier's each tick; the minute's line */
static void ae_bots_cover_count(struct bots_bot *bot, struct bots_tick *tick, boolean carrying)
{
	struct ae_bots_cover_bot *state = ae_bots_cover_state(bot);
	long now = tick->now;

	/* (a new game: its own counts) */
	if (now < ae_bots_cover_counts.last_tick)
		csmemset(&ae_bots_cover_counts, 0, sizeof(ae_bots_cover_counts));
	if (ae_bots_cover_counts.last_tick != now)
	{
		struct ae_bots_objective_snapshot const *snapshot = ae_bots_objective_snapshot();
		struct data_iterator iterator;
		struct player_datum *player;
		/* (the ticks since the last count: a network game's bots are not thought for every tick) */
		long ticks = ae_bots_cover_counts.last_tick > 0 ? MIN(now - ae_bots_cover_counts.last_tick, 10) : 1;

		ae_bots_cover_counts.last_tick = now;
		if (snapshot)
		{
			data_iterator_new(&iterator, player_data);
			while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
			{
				if (!ae_bots_objective_player_carries(snapshot, iterator.datum_index))
					continue;
				if (game_engine_has_teams() && VALID_INDEX(player->team_index, 2))
					ae_bots_cover_counts.team_ticks[player->team_index] += ticks;
				else if (!game_engine_has_teams())
					ae_bots_cover_counts.ffa_ticks += ticks;
			}
		}
		if (now - ae_bots_cover_counts.report_tick >= TICKS_PER_SECOND * 60 && ae_bots_cover_counts.carry_ticks > 0)
		{
			long samples = ae_bots_cover_counts.samples_carrying > 0 ? ae_bots_cover_counts.samples_carrying : 1;

			ae_bots_cover_counts.report_tick = now;
			platform_log("bots: cover tick %ld (%s): carries %ld, %.1f s each; carrying: covered %ld%%, seen %ld%%, "
				"holding %ld%%; picks %ld (unseen %ld), old way %ld; moved on near %ld seen %ld hurt %ld time %ld; "
				"ball time team 0 %.0f s team 1 %.0f s, free for all %.0f s; places %ld", now, ae_bots_cover_on() ? "on" : "off",
				ae_bots_cover_counts.carries, ae_bots_cover_counts.carries ? (double)ae_bots_cover_counts.carry_ticks / ae_bots_cover_counts.carries / TICKS_PER_SECOND : 0.0,
				ae_bots_cover_counts.samples_covered * 100 / samples, ae_bots_cover_counts.samples_seen * 100 / samples,
				ae_bots_cover_counts.samples_holding * 100 / samples, ae_bots_cover_counts.picks, ae_bots_cover_counts.picks_unseen,
				ae_bots_cover_counts.fallbacks, ae_bots_cover_counts.leaves[1], ae_bots_cover_counts.leaves[2], ae_bots_cover_counts.leaves[3],
				ae_bots_cover_counts.leaves[4], (double)ae_bots_cover_counts.team_ticks[0] / TICKS_PER_SECOND,
				(double)ae_bots_cover_counts.team_ticks[1] / TICKS_PER_SECOND, (double)ae_bots_cover_counts.ffa_ticks / TICKS_PER_SECOND,
				ae_bots_cover.count);
		}
	}
	if (!state)
		return;
	if (!carrying)
	{
		state->carried = FALSE;
		state->place = NONE;
		return;
	}
	if (!state->carried)
	{
		state->carried = TRUE;
		state->carry_tick = now;
		ae_bots_cover_counts.carries++;
	}
	ae_bots_cover_counts.carry_ticks += MIN(now - state->carry_tick, 10);
	state->carry_tick = now;
	/* (the cover's switch off: no scan, no probes; the carries and ball time alone) */
	if (now - state->sample_tick < AE_BOTS_COVER_SAMPLE_TICKS || !ae_bots_cover_on())
		return;
	state->sample_tick = now;
	ae_bots_cover_scan();
	{
		real_point3d ground, eye;
		unsigned walls;
		boolean roof;
		long rays = 0;
		short seen, seen_near;

		ae_bots_cover_counts.samples_carrying++;
		if (bots_surface_below(&tick->position, &ground) != NONE && ae_bots_cover_probe(&ground, &walls, &roof, &rays) &&
			ae_bots_cover_score(walls, roof) > 0)
		{
			ae_bots_cover_counts.samples_covered++;
		}
		unit_get_head_position(tick->player->unit_index, &eye);
		seen = ae_bots_cover_seen(tick->player, &eye, 0.0f, &seen_near);
		if (seen)
			ae_bots_cover_counts.samples_seen++;
		if (state->place != NONE && state->arrived_tick)
			ae_bots_cover_counts.samples_holding++;
	}
}
