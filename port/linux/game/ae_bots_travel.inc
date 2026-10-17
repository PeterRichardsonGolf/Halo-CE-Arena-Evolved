/*
AE_BOTS_TRAVEL.INC

Included by ae_bots_lan.inc, so at the end of bots.c, where its state and static functions are seen: the glue of the
bots' getting around (ae_bots_travel.h) in bots.c's path search, path following, tick and unstick (their AE hook
lines). A bot's way may go by a VIA, a place its searches and steps go to before its goal, which stays as bots.c
chose it:

- a LINK (a teleporter pad, a layout file's jump): chosen after a search to the goal found a long walk (or none),
  when a link's estimate beats it and the walk to the link's entry, searched at once, still does (ae_bots_travel_rules.h);
  near the entry the bot walks onto the pad's centre (and is sent on) or faces the jump's landing and jumps; then it
  searches anew from where it came out;
- a PACK (a health pack, HALO CE / REACH health): a detour when hurt and not fighting.

A via is dropped when the goal moves more than 3 units (Halo Infinite's objective path correction threshold) or turns
to a weapon to fetch, and a link that failed (the walk to it, the pad blocked, a jump missed) is not tried again for a
while. Nothing here draws a random number: the bots' runs stay the same for the same input.
*/

#include "ae_bots_travel.h"
#include "ae_bots_travel_rules.h"

#define AE_TRAVEL_VIA_NONE (-1)
#define AE_TRAVEL_VIA_PACK (-2)

enum
{
	_ae_travel_phase_to_entry = 0,
	_ae_travel_phase_at_entry,
	_ae_travel_phase_in_air
};

/* how near the entry the last steps are walked straight: a pad's, a jump's, a pack's */
#define AE_TRAVEL_PAD_APPROACH 2.0f
#define AE_TRAVEL_JUMP_APPROACH 1.0f
#define AE_TRAVEL_PACK_APPROACH 2.5f
/* a goal moved this far is another (the via chosen for it dropped) */
#define AE_TRAVEL_GOAL_MOVED 3.0f
/* a jump that ends this near its landing is made */
#define AE_TRAVEL_LANDED 1.5f
/* a roaming place this far off is a far goal (halo.log's goal times) */
#define AE_TRAVEL_FAR_GOAL 15.0f
/* a bot stuck takes a layout jump this near */
#define AE_TRAVEL_STUCK_JUMP_REACH 8.0f
/* a bot this long no nearer its goal while it walks has a stuck spot (Halo Infinite's bots have a 0.3 s wrong path
direction timeout; ours walk paths with corners and dodge other bots: longer) */
#define AE_TRAVEL_NO_PROGRESS_TICKS (6 * TICKS_PER_SECOND)

struct ae_travel_bot
{
	long unit_index;
	/* its via: a link's index, AE_TRAVEL_VIA_PACK or AE_TRAVEL_VIA_NONE; how far along; for which goal; since when */
	short via;
	short phase;
	long goal_tick;
	real_point3d goal_point;
	long via_tick;
	/* walked to straight, no path (a jump taken to get out of where the paths fail) */
	boolean direct;
	long approach_tick;
	long jump_tick;
	/* the pack it goes to, and its health when it set off */
	long pack_object;
	real_point3d pack_point;
	long pack_surface;
	real pack_health;
	long pack_banned;
	long pack_banned_until;
	/* no pack looked for before then (one it found no way to) */
	long pack_look_tick;
	/* links not tried again until then */
	long declined_until[AE_BOTS_TRAVEL_MAXIMUM_LINKS];
	/* where it last came out of a link, when, and which */
	boolean arrived;
	real_point3d arrival;
	long arrival_tick;
	short arrival_link;
	/* the goal timed: from when, how far it was */
	long timed_goal_tick;
	real_point3d timed_point;
	real timed_straight;
	long timed_start_tick;
	boolean timed_done;
	/* how near its goal it has come, and when it last came nearer (a bot pacing back and forth) */
	long progress_goal_tick;
	real progress_best;
	long progress_tick;
};

static struct
{
	boolean ready;
	long link_count;
	long teleporter_count;
	struct ae_bots_travel_link links[AE_BOTS_TRAVEL_MAXIMUM_LINKS];
	real_point3d entry_ground[AE_BOTS_TRAVEL_MAXIMUM_LINKS];
	long entry_surface[AE_BOTS_TRAVEL_MAXIMUM_LINKS];
	long avoid_count;
	struct ae_bots_travel_avoid avoids[AE_BOTS_TRAVEL_MAXIMUM_AVOIDS];
	/* the health packs lying about now, looked for once a second, and the gametype's HEALTH then */
	long packs_tick;
	int health_style;
	long pack_count;
	long pack_objects[AE_BOTS_TRAVEL_MAXIMUM_PACKS];
	real_point3d pack_points[AE_BOTS_TRAVEL_MAXIMUM_PACKS];
	struct ae_bots_travel_stuck_spots stuck;
	long last_report_tick;
	/* the game's counts (halo.log's minute line) */
	long teleports;
	long teleports_planned;
	long pads_missed;
	long jumps;
	long jumps_missed;
	long links_tried;
	long links_taken;
	long links_declined;
	long pack_detours;
	long packs_taken;
	long far_goals;
	long far_goal_ticks;
	long near_goals;
	long near_goal_ticks;
	long goals_left;
	long no_progress;
} ae_travel;

static struct ae_travel_bot ae_travel_bots[BOTS_MAXIMUM];
/* (a search made from here: bots_search's hook lets it through) */
static boolean ae_travel_searching;
/* the bot whose tick runs now, and its tick (the jump's button, the throttle onto a pad) */
static struct bots_bot *ae_travel_tick_bot;
static struct bots_tick *ae_travel_tick_now;

/* (debug.bot_trace) a line for each via chosen, dropped, made */
static void ae_travel_trace(struct bots_bot *bot, char const *what, long via, real a, real b)
{
	static long trace = -1;

	if (trace < 0)
		trace = MAX(config_integer("debug.bot_trace"), 0);
	if (trace <= 0)
		return;
	platform_log("bots: travel trace %ld bot %ld %s %ld (%.1f %.1f)", game_time_get(), (long)(bot - bots_globals.bots) + 1,
		what, via, (double)a, (double)b);
}

static struct ae_travel_bot *ae_travel_state(struct bots_bot *bot)
{
	long index = (long)(bot - bots_globals.bots);

	return index >= 0 && index < BOTS_MAXIMUM ? &ae_travel_bots[index] : NULL;
}

static void ae_travel_check_progress(struct bots_bot *bot, struct ae_travel_bot *travel, struct bots_tick *tick);

/* how near a link's entry its last steps are walked straight */
static real ae_travel_approach(struct ae_bots_travel_link const *link)
{
	return link->kind == AE_BOTS_TRAVEL_LINK_JUMP ? AE_TRAVEL_JUMP_APPROACH : AE_TRAVEL_PAD_APPROACH;
}

static void ae_travel_bot_reset(struct ae_travel_bot *travel, long unit_index)
{
	csmemset(travel, 0, sizeof(*travel));
	travel->unit_index = unit_index;
	travel->via = AE_TRAVEL_VIA_NONE;
	travel->pack_object = NONE;
	travel->pack_banned = NONE;
	travel->arrival_link = AE_TRAVEL_VIA_NONE;
	travel->timed_goal_tick = NONE;
	travel->progress_goal_tick = NONE;
}

/* ---------- the map */

void ae_bots_travel_prepare(void)
{
	struct ae_bots_travel_link read[AE_BOTS_TRAVEL_MAXIMUM_LINKS];
	long count, index, jumps = 0;

	csmemset(&ae_travel, 0, sizeof(ae_travel));
	for (index = 0; index < BOTS_MAXIMUM; index++)
		ae_travel_bot_reset(&ae_travel_bots[index], NONE);
	/* the other lanes' bots start over too: a player index repeats from game to game (the player data is
	remade each map with the same identifiers), so a state kept by player index would carry the last game's
	pauses, patrol points and timers into this one */
	csmemset(ae_bots_objective_bots, 0, sizeof(ae_bots_objective_bots));
	csmemset(ae_bots_vehicle_bots, 0, sizeof(ae_bots_vehicle_bots));
	for (index = 0; index < BOTS_MAXIMUM; index++)
	{
		ae_bots_objective_bots[index].player_index = NONE;
		ae_bots_vehicle_bots[index].player_index = NONE;
		ae_bots_vehicle_bots[index].unit_index = NONE;
	}
	ae_bots_combat_reset();
	count = ae_bots_travel_read_links(read, AE_BOTS_TRAVEL_MAXIMUM_LINKS);
	for (index = 0; index < count; index++)
	{
		struct ae_bots_travel_link const *link = &read[index];
		real_point3d ground;
		long surface = bots_surface_below(&link->entry, &ground);

		if (surface == NONE)
		{
			platform_log("bots: travel: %s at (%.1f %.1f %.1f) has no ground under it (not used)",
				link->kind == AE_BOTS_TRAVEL_LINK_JUMP ? "a layout jump" : "a teleporter pad", link->entry.x,
				link->entry.y, link->entry.z);
			continue;
		}
		ae_travel.links[ae_travel.link_count] = *link;
		ae_travel.entry_ground[ae_travel.link_count] = ground;
		ae_travel.entry_surface[ae_travel.link_count] = surface;
		ae_travel.link_count++;
		if (link->kind == AE_BOTS_TRAVEL_LINK_TELEPORTER)
		{
			ae_travel.teleporter_count++;
			platform_log("bots: travel: teleporter %ld -> %ld: (%.1f %.1f %.1f) to (%.1f %.1f %.1f), %.0f units", link->source,
				link->target, link->entry.x, link->entry.y, link->entry.z, link->exit.x, link->exit.y, link->exit.z,
				(double)bots_distance3d(&link->entry, &link->exit));
		}
		else
			jumps++;
	}
	ae_travel.avoid_count = ae_bots_travel_read_avoids(ae_travel.avoids, AE_BOTS_TRAVEL_MAXIMUM_AVOIDS);
	for (index = 0; index < ae_travel.avoid_count; index++)
	{
		struct ae_bots_travel_avoid const *avoid = &ae_travel.avoids[index];
		long closed = 0, dropped = 0, roam;

		if (bots_paths_copy())
		{
			struct collision_bsp const *bsp = TAG_BLOCK_GET_ELEMENT(&bots_paths.structure.collision_bsp, 0, struct collision_bsp);
			long surface_index;

			for (surface_index = 0; surface_index < bsp->surfaces.count; surface_index++)
			{
				real_point3d corners[BOTS_SURFACE_CORNERS], middle;
				real reach;
				long corner_count;

				if (bots_paths_walkable_surface(bsp, surface_index, corners, &corner_count, &middle, &reach) &&
					bots_distance3d(&middle, &avoid->point) <= avoid->radius)
				{
					bots_paths.walkable[surface_index] &= (byte)~FLAG(BOTS_PATHFINDING_WALKABLE_BIT);
					closed++;
				}
			}
		}
		for (roam = 0; roam < bots_globals.roam_point_count;)
		{
			if (bots_distance3d(&bots_globals.roam_points[roam], &avoid->point) <= avoid->radius)
			{
				bots_globals.roam_points[roam] = bots_globals.roam_points[bots_globals.roam_point_count - 1];
				bots_globals.roam_surfaces[roam] = bots_globals.roam_surfaces[bots_globals.roam_point_count - 1];
				bots_globals.roam_point_count--;
				dropped++;
			}
			else
				roam++;
		}
		platform_log("bots: travel: layout line %d: avoid (%.1f %.1f %.1f) radius %.1f: %ld walkable surfaces closed, "
			"%ld roaming places dropped", (int)avoid->line, avoid->point.x, avoid->point.y, avoid->point.z,
			avoid->radius, closed, dropped);
	}
	ae_travel.packs_tick = NONE;
	ae_travel.last_report_tick = game_time_get();
	ae_travel.ready = TRUE;
	platform_log("bots: travel: %ld links (%ld teleporter pads, %ld layout jumps), %ld avoid zones; links %s",
		ae_travel.link_count, ae_travel.teleporter_count, jumps, ae_travel.avoid_count,
		ae_bots_travel_links_on() ? "taken" : "off (debug.bot_links)");
}

/* ---------- searches */

/* the walk the search just made found from start to the goal (the search's raw steps); a way that ends within near
of the goal (across, and 1.5 up or down) counts as reaching it, the rest walked straight (a pad on its base, a pack
on a shelf the walkable surfaces stop short of) */
static real ae_travel_walk_found(real_point3d const *start, real_point3d const *goal, long goal_surface, real near)
{
	struct path_state *state = bots_globals.search;
	struct path_step raw_steps[64];
	short raw_step_count, step;
	boolean steps_finish_path, reached;
	real_point3d endpoint, previous = *start;
	real length = 0.0f;

	if (!state || !bots_path_raw_steps(state, raw_steps, &raw_step_count, &steps_finish_path, &endpoint))
		return ae_bots_travel_walk(FALSE, 0.0f, FALSE, 0.0f, bots_distance3d(start, goal));
	reached = path_node_from_hash_table(state, goal_surface) != NONE;
	for (step = 0; step < raw_step_count; step++)
	{
		length += bots_distance3d(&previous, &raw_steps[step].point);
		previous = raw_steps[step].point;
	}
	if (!reached && steps_finish_path && bots_distance2d(&previous, goal) <= near && fabs(previous.z - goal->z) <= 1.5f)
		return length + bots_distance3d(&previous, goal);
	return ae_bots_travel_walk(TRUE, length, (boolean)(reached && steps_finish_path), bots_distance3d(&previous, goal),
		bots_distance3d(start, goal));
}

/* whether the bot's path (just searched to a via) takes it there, or within near of it (its end then walked
straight: the via's approach) */
static boolean ae_travel_path_reaches(struct bots_bot const *bot, real_point3d const *point, real near)
{
	if (!bot->path.valid)
		return FALSE;
	if (!bot->path_short)
		return TRUE;
	return (boolean)(bot->path.steps_finish_path && bots_distance2d(&bot->path.endpoint.point, point) <= near &&
		fabs(bot->path.endpoint.point.z - point->z) <= 1.5f);
}

/* a search to the point (a via) in place of the goal: TRUE when one was made */
static boolean ae_travel_search_to(struct bots_bot *bot, long unit_index, real_point3d const *point, long surface)
{
	real_point3d goal_point = bot->goal_point;
	long goal_surface = bot->goal_surface_index, before = bots_globals.searches;

	bot->goal_point = *point;
	bot->goal_surface_index = surface;
	ae_travel_searching = TRUE;
	bots_search(bot, unit_index);
	ae_travel_searching = FALSE;
	bot->goal_point = goal_point;
	bot->goal_surface_index = goal_surface;
	return (boolean)(bots_globals.searches != before);
}

/* the via let go: the bot searches for its goal at once */
static void ae_travel_drop_via(struct bots_bot *bot, struct ae_travel_bot *travel, long decline_ticks, char const *why)
{
	long now = game_time_get();

	ae_travel_trace(bot, why, travel->via, (real)decline_ticks, bots_distance3d(&bot->goal_point, &travel->goal_point));
	if (travel->via >= 0 && decline_ticks > 0)
		travel->declined_until[travel->via] = now + decline_ticks;
	travel->via = AE_TRAVEL_VIA_NONE;
	travel->phase = _ae_travel_phase_to_entry;
	travel->direct = FALSE;
	travel->pack_object = NONE;
	bot->path.valid = FALSE;
	bot->path_tick = now - TICKS_PER_SECOND;
	bot->climb_until_tick = 0;
}

/* the via still holds: its goal (a link's), its pack, how long it has taken */
static void ae_travel_check_via(struct bots_bot *bot, struct ae_travel_bot *travel, struct object_datum *object)
{
	long now = game_time_get();

	if (travel->via == AE_TRAVEL_VIA_NONE)
		return;
	if (travel->via == AE_TRAVEL_VIA_PACK)
	{
		char const *why = NULL;

		/* (health up: a pack taken, this one or one beside it) */
		if (object && object->object.body_vitality > travel->pack_health + 0.05f)
		{
			ae_travel_trace(bot, "pack health", travel->pack_object, travel->pack_health, object->object.body_vitality);
			ae_travel.packs_taken++;
			ae_travel_drop_via(bot, travel, 0, "pack taken");
			return;
		}
		/* (taken by another, not back yet) */
		if (!ae_bots_travel_pack_there(travel->pack_object))
		{
			ae_travel_drop_via(bot, travel, 0, "pack gone");
			return;
		}
		if (now - travel->via_tick > TICKS_PER_SECOND * 25)
			why = "pack too far";
		else if (travel->phase == _ae_travel_phase_at_entry && now - travel->approach_tick > TICKS_PER_SECOND * 4)
			why = "pack not reached";
		else if (object && ae_bots_travel_pack_need(ae_travel.health_style, object->object.body_vitality) <= 0.0f)
			why = "pack not needed";
		if (why)
		{
			travel->pack_banned = travel->pack_object;
			travel->pack_banned_until = now + TICKS_PER_SECOND * 20;
			ae_travel_drop_via(bot, travel, 0, why);
		}
		return;
	}
	if (bot->goal_kind == _bots_goal_none ||
		(bot->goal_kind == _bots_goal_item && ae_travel.links[travel->via].kind != AE_BOTS_TRAVEL_LINK_JUMP))
	{
		ae_travel_drop_via(bot, travel, 0, "goal gone");
		return;
	}
	if (bot->goal_tick != travel->goal_tick)
	{
		/* (another goal: the link kept while it still beats walking straight there) */
		struct ae_bots_travel_link const *link = &ae_travel.links[travel->via];

		if (travel->phase == _ae_travel_phase_to_entry &&
			bots_distance3d(&bot->goal_point, &travel->goal_point) > AE_TRAVEL_GOAL_MOVED)
		{
			real straight = object ? bots_distance3d(&object->object.position, &bot->goal_point) : 0.0f;
			real estimate = !object ? REAL_MAX : ae_bots_travel_link_estimate(
				bots_distance3d(&object->object.position, &link->entry),
				ae_bots_travel_hop_cost((boolean)(link->kind == AE_BOTS_TRAVEL_LINK_JUMP),
					bots_distance3d(&link->entry, &link->exit)), bots_distance3d(&link->exit, &bot->goal_point));

			if (estimate >= straight * AE_BOTS_TRAVEL_WALK_STRETCH)
			{
				ae_travel_drop_via(bot, travel, 0, "goal moved");
				return;
			}
		}
		travel->goal_tick = bot->goal_tick;
		travel->goal_point = bot->goal_point;
	}
	/* (half a minute to reach the entry: not this link again a while) */
	if (travel->phase == _ae_travel_phase_to_entry && now - travel->via_tick > TICKS_PER_SECOND * 30)
	{
		ae_travel.links_declined++;
		ae_travel_drop_via(bot, travel, TICKS_PER_SECOND * 20, "entry too far");
	}
}

/* a link looked at, after the search to the goal found this walk: the best worth a try is searched to at once, and
taken if the walk to it still beats the walk */
static void ae_travel_consider_links(struct bots_bot *bot, struct ae_travel_bot *travel, long unit_index,
	real_point3d const *position, real walk)
{
	long now = game_time_get(), index, best = NONE;
	real best_estimate = REAL_MAX, straight = bots_distance3d(position, &bot->goal_point);

	for (index = 0; index < ae_travel.link_count; index++)
	{
		struct ae_bots_travel_link const *link = &ae_travel.links[index];
		real exit_to_goal = bots_distance3d(&link->exit, &bot->goal_point), estimate;

		if (now < travel->declined_until[index] || exit_to_goal >= straight ||
			!ae_bots_travel_link_allowed(link->entry.x, link->entry.y, link->entry.z, travel->arrived, travel->arrival.x,
				travel->arrival.y, travel->arrival.z, now - travel->arrival_tick, (boolean)(travel->arrival_link == index)))
		{
			continue;
		}
		estimate = ae_bots_travel_link_estimate(bots_distance3d(position, &link->entry),
			ae_bots_travel_hop_cost((boolean)(link->kind == AE_BOTS_TRAVEL_LINK_JUMP), bots_distance3d(&link->entry, &link->exit)),
			exit_to_goal);
		if (ae_bots_travel_link_worth(estimate, walk) && estimate < best_estimate)
		{
			best_estimate = estimate;
			best = index;
		}
	}
	if (best == NONE || bots_globals.tick_searches >= BOTS_SEARCHES_A_TICK)
		return;
	{
		struct ae_bots_travel_link const *link = &ae_travel.links[best];
		struct path_result path = bot->path;
		boolean path_short = bot->path_short;
		long path_failures = bot->path_failures, path_tick = bot->path_tick, climb_until_tick = bot->climb_until_tick;
		real hop = ae_bots_travel_hop_cost((boolean)(link->kind == AE_BOTS_TRAVEL_LINK_JUMP),
			bots_distance3d(&link->entry, &link->exit));

		ae_travel.links_tried++;
		if (ae_travel_search_to(bot, unit_index, &ae_travel.entry_ground[best], ae_travel.entry_surface[best]) &&
			ae_travel_path_reaches(bot, &ae_travel.entry_ground[best], ae_travel_approach(link)) &&
			ae_bots_travel_link_taken(ae_travel_walk_found(position, &ae_travel.entry_ground[best], ae_travel.entry_surface[best],
				ae_travel_approach(link)), hop, bots_distance3d(&link->exit, &bot->goal_point), walk))
		{
			travel->via = (short)best;
			travel->phase = _ae_travel_phase_to_entry;
			travel->goal_tick = bot->goal_tick;
			travel->goal_point = bot->goal_point;
			travel->via_tick = now;
			ae_travel.links_taken++;
			ae_travel_trace(bot, "link taken", best, walk, best_estimate);
			return;
		}
		/* (the walk there is long, or there is none: the goal's path back, the link not looked at a while) */
		bot->path = path;
		bot->path_short = path_short;
		bot->path_failures = path_failures;
		bot->path_tick = path_tick;
		bot->climb_until_tick = climb_until_tick;
		travel->declined_until[best] = now + TICKS_PER_SECOND * 8;
		ae_travel.links_declined++;
		ae_travel_trace(bot, "link declined", best, walk, best_estimate);
	}
}

boolean ae_bots_travel_search(struct bots_bot *bot, long unit_index)
{
	struct ae_travel_bot *travel;
	struct object_datum *object;
	real_point3d position;
	long before;

	if (ae_travel_searching || !ae_travel.ready || bot->goal_surface_index == NONE ||
		bots_globals.tick_searches >= BOTS_SEARCHES_A_TICK)
	{
		return FALSE;
	}
	travel = ae_travel_state(bot);
	object = (struct object_datum *)object_try_and_get_and_verify_type(unit_index, _object_mask_unit);
	if (!travel || !object)
		return FALSE;
	ae_travel_check_via(bot, travel, object);
	if (travel->via == AE_TRAVEL_VIA_PACK)
	{
		if (ae_travel_search_to(bot, unit_index, &travel->pack_point, travel->pack_surface) &&
			!ae_travel_path_reaches(bot, &travel->pack_point, AE_TRAVEL_PACK_APPROACH))
		{
			ae_travel_trace(bot, "pack no way: end off", travel->pack_object,
				bot->path.valid ? bots_distance2d(&bot->path.endpoint.point, &travel->pack_point) : -1.0f,
				bot->path.valid ? bot->path.endpoint.point.z - travel->pack_point.z : 0.0f);
			travel->pack_banned = travel->pack_object;
			travel->pack_banned_until = game_time_get() + TICKS_PER_SECOND * 60;
			travel->pack_look_tick = game_time_get() + TICKS_PER_SECOND * 5;
			ae_travel_drop_via(bot, travel, 0, "pack no way");
		}
		return TRUE;
	}
	if (travel->via >= 0 && travel->direct)
		return TRUE;
	if (travel->via >= 0)
	{
		if (ae_travel_search_to(bot, unit_index, &ae_travel.entry_ground[travel->via], ae_travel.entry_surface[travel->via]) &&
			!ae_travel_path_reaches(bot, &ae_travel.entry_ground[travel->via], ae_travel_approach(&ae_travel.links[travel->via])))
		{
			ae_travel.links_declined++;
			ae_travel_drop_via(bot, travel, TICKS_PER_SECOND * 20, "entry no way");
		}
		return TRUE;
	}
	if (!ae_travel.link_count || !ae_bots_travel_links_on() || bot->goal_kind == _bots_goal_item)
		return FALSE;
	/* (the goal's search, then the links against the walk it found) */
	position = object->object.position;
	before = bots_globals.searches;
	ae_travel_searching = TRUE;
	bots_search(bot, unit_index);
	ae_travel_searching = FALSE;
	if (bots_globals.searches != before && bot->goal_kind != _bots_goal_none && bot->goal_surface_index != NONE)
	{
		ae_travel_consider_links(bot, travel, unit_index, &position,
			ae_travel_walk_found(&position, &bot->goal_point, bot->goal_surface_index, 0.0f));
	}
	return TRUE;
}

/* ---------- following */

static void ae_travel_press_jump(struct bots_bot *bot)
{
	if (ae_travel_tick_bot == bot && ae_travel_tick_now)
		ae_travel_tick_now->flags |= FLAG(_unit_control_jump_bit);
}

/* a layout jump the bot's way passes by, moving towards its landing: taken now */
static void ae_travel_jump_on_the_way(struct bots_bot *bot, struct ae_travel_bot *travel, real_point3d const *position)
{
	long now = game_time_get(), index;
	struct path_step const *step;

	if (!bot->path.valid || bot->path.step_index >= bot->path.step_count || !ae_bots_travel_links_on() ||
		bot->goal_kind == _bots_goal_item || bot->goal_kind == _bots_goal_none)
	{
		return;
	}
	step = &bot->path.steps[(long)bot->path.step_index];
	for (index = 0; index < ae_travel.link_count; index++)
	{
		struct ae_bots_travel_link const *link = &ae_travel.links[index];

		if (link->kind != AE_BOTS_TRAVEL_LINK_JUMP || now < travel->declined_until[index] ||
			bots_distance2d(position, &link->entry) > AE_TRAVEL_JUMP_APPROACH ||
			fabs(position->z - link->entry.z) > 1.0f ||
			!ae_bots_travel_jump_aligned(step->point.x - position->x, step->point.y - position->y,
				link->exit.x - link->entry.x, link->exit.y - link->entry.y) ||
			bots_distance3d(&link->exit, &bot->goal_point) + 2.0f > bots_distance3d(position, &bot->goal_point))
		{
			continue;
		}
		travel->via = (short)index;
		travel->phase = _ae_travel_phase_at_entry;
		travel->goal_tick = bot->goal_tick;
		travel->goal_point = bot->goal_point;
		travel->via_tick = now;
		travel->approach_tick = now;
		return;
	}
}

boolean ae_bots_travel_follow(struct bots_bot *bot, long unit_index, real_point3d const *position, real *direction)
{
	struct ae_travel_bot *travel;
	struct object_datum *object;
	long now = game_time_get();
	real_point3d const *point;
	real distance;

	if (!ae_travel.ready)
		return FALSE;
	travel = ae_travel_state(bot);
	if (!travel || (travel->via == AE_TRAVEL_VIA_NONE && !ae_travel.link_count))
		return FALSE;
	object = (struct object_datum *)object_try_and_get_and_verify_type(unit_index, _object_mask_unit);
	if (!object)
		return FALSE;
	ae_travel_check_via(bot, travel, object);
	if (travel->via == AE_TRAVEL_VIA_NONE)
		ae_travel_jump_on_the_way(bot, travel, position);
	if (travel->via == AE_TRAVEL_VIA_NONE)
		return FALSE;
	if (travel->via == AE_TRAVEL_VIA_PACK)
	{
		point = &travel->pack_point;
		distance = bots_distance2d(position, point);
		if (travel->phase == _ae_travel_phase_to_entry)
		{
			if (distance > AE_TRAVEL_PACK_APPROACH || fabs(position->z - point->z) > 1.5f)
				return FALSE;
			travel->phase = _ae_travel_phase_at_entry;
			travel->approach_tick = now;
		}
		*direction = (real)atan2(point->y - position->y, point->x - position->x);
		return TRUE;
	}
	{
		struct ae_bots_travel_link const *link = &ae_travel.links[travel->via];
		boolean jump = (boolean)(link->kind == AE_BOTS_TRAVEL_LINK_JUMP);

		point = &link->entry;
		distance = bots_distance2d(position, point);
		if (travel->phase == _ae_travel_phase_to_entry)
		{
			if (travel->direct && (distance > AE_TRAVEL_JUMP_APPROACH || fabs(position->z - point->z) > 1.5f))
			{
				if (now - travel->via_tick > TICKS_PER_SECOND * 8)
				{
					ae_travel.jumps_missed++;
					ae_travel_drop_via(bot, travel, TICKS_PER_SECOND * 30, "jump not reached");
					return FALSE;
				}
				*direction = (real)atan2(point->y - position->y, point->x - position->x);
				return TRUE;
			}
			if (distance > (jump ? AE_TRAVEL_JUMP_APPROACH : AE_TRAVEL_PAD_APPROACH) || fabs(position->z - point->z) > 1.5f)
				return FALSE;
			travel->phase = _ae_travel_phase_at_entry;
			travel->approach_tick = now;
		}
		if (travel->phase == _ae_travel_phase_at_entry && !jump)
		{
			/* (onto the pad's centre: the engine sends a player within half a unit of it; slower at the last) */
			if (now - travel->approach_tick > TICKS_PER_SECOND * 4)
			{
				ae_travel.pads_missed++;
				ae_travel_drop_via(bot, travel, TICKS_PER_SECOND * 20, "pad not reached");
				return FALSE;
			}
			if (distance < 0.6f && ae_travel_tick_bot == bot && ae_travel_tick_now)
				ae_travel_tick_now->throttle_scale = 0.6f;
			*direction = (real)atan2(point->y - position->y, point->x - position->x);
			return TRUE;
		}
		if (travel->phase == _ae_travel_phase_at_entry)
		{
			/* (a jump: towards the landing, the button once it moves that way) */
			if (distance > AE_TRAVEL_JUMP_APPROACH * 2.5f || now - travel->approach_tick > TICKS_PER_SECOND * 2)
			{
				ae_travel.jumps_missed++;
				ae_travel_drop_via(bot, travel, TICKS_PER_SECOND * 30, "jump not started");
				return FALSE;
			}
			if (ae_bots_travel_jump_aligned(object->object.translational_velocity.i, object->object.translational_velocity.j,
					link->exit.x - link->entry.x, link->exit.y - link->entry.y) ||
				now - travel->approach_tick > TICKS_PER_SECOND * 2 / 3)
			{
				ae_travel_press_jump(bot);
				travel->phase = _ae_travel_phase_in_air;
				travel->jump_tick = now;
				ae_travel.jumps++;
			}
			*direction = (real)atan2(link->exit.y - position->y, link->exit.x - position->x);
			return TRUE;
		}
		{
			/* (in the air: on towards the landing; landed near it, the jump is made) */
			struct biped_datum *biped = biped_try_and_get(unit_index);
			boolean airborne = (boolean)(biped && TEST_FLAG(biped->biped.flags, _biped_airborne_bit));
			real landing = bots_distance2d(position, &link->exit);

			if (!airborne && now - travel->jump_tick > TICKS_PER_SECOND / 4)
			{
				/* (made: near the landing, or down at its height when it is a drop) */
				if ((landing < AE_TRAVEL_LANDED && fabs(position->z - link->exit.z) < AE_TRAVEL_LANDED) ||
					(link->exit.z < link->entry.z - 1.0f && fabs(position->z - link->exit.z) < 1.0f &&
						landing < AE_TRAVEL_LANDED * 3.0f))
				{
					travel->arrived = TRUE;
					travel->arrival = *position;
					travel->arrival_tick = now;
					travel->arrival_link = travel->via;
					ae_travel_drop_via(bot, travel, 0, "jumped");
				}
				else
				{
					ae_travel.jumps_missed++;
					ae_travel_drop_via(bot, travel, TICKS_PER_SECOND * 30, "jump landed off");
				}
				return FALSE;
			}
			if (now - travel->jump_tick > TICKS_PER_SECOND * 3)
			{
				ae_travel.jumps_missed++;
				ae_travel_drop_via(bot, travel, TICKS_PER_SECOND * 30, "jump too long");
				return FALSE;
			}
			*direction = (real)atan2(link->exit.y - position->y, link->exit.x - position->x);
			return TRUE;
		}
	}
}

/* ---------- the tick */

static void ae_travel_refresh_packs(long now)
{
	if (ae_travel.packs_tick != NONE && now - ae_travel.packs_tick < TICKS_PER_SECOND && now >= ae_travel.packs_tick)
		return;
	ae_travel.packs_tick = now;
	ae_travel.health_style = ae_bots_travel_health_style();
	ae_travel.pack_count = ae_travel.health_style == AE_BOTS_TRAVEL_HEALTH_FULL ? 0 :
		ae_bots_travel_read_packs(ae_travel.pack_objects, ae_travel.pack_points, AE_BOTS_TRAVEL_MAXIMUM_PACKS);
}

/* a hurt bot, not fighting, looks for a pack in reach (once a second) */
/* a defender's pack: no farther than this from it (world units) */
#define AE_TRAVEL_DEFENDER_PACK_DISTANCE 8.0f

static void ae_travel_look_for_pack(struct bots_bot *bot, long bot_index, struct ae_travel_bot *travel,
	struct bots_tick *tick)
{
	long now = tick->now, index, best = NONE;
	real need, best_score = 0.0f;
	boolean defending = FALSE;

	if (travel->via != AE_TRAVEL_VIA_NONE || !ae_travel.pack_count || (now + bot_index) % TICKS_PER_SECOND ||
		now < travel->pack_look_tick ||
		bot->goal_kind == _bots_goal_item || tick->object->object.parent_object_index != NONE ||
		(bot->target_player_index != NONE && now - bot->target_seen_tick < TICKS_PER_SECOND))
	{
		return;
	}
	need = ae_bots_travel_pack_need(ae_travel.health_style, tick->object->object.body_vitality);
	if (need <= 0.0f)
		return;
	/* (its mode's goal first: a carrier keeps running home hurt or not, and the ball's or the hill's holder or a
	race bot keeps to it; a defender takes only a pack close to where it stands) */
	{
		struct ae_bots_objective_bot const *objective = &ae_bots_objective_bots[bot_index];

		if (objective->player_index == bot->player_index)
		{
			if (objective->behaviour == AE_BOTS_CTF_CARRY || objective->behaviour == AE_BOTS_BALL_HOLD ||
				objective->behaviour == AE_BOTS_HILL_HOLD || objective->behaviour == AE_BOTS_RACE_NEXT)
			{
				return;
			}
			if (objective->behaviour == AE_BOTS_CTF_DEFEND)
				defending = TRUE;
		}
	}
	for (index = 0; index < ae_travel.pack_count; index++)
	{
		real score;

		if (ae_travel.pack_objects[index] == travel->pack_banned && now < travel->pack_banned_until)
			continue;
		if (defending && bots_distance2d(&tick->position, &ae_travel.pack_points[index]) > AE_TRAVEL_DEFENDER_PACK_DISTANCE)
			continue;
		score = ae_bots_travel_pack_score(need, bots_distance2d(&tick->position, &ae_travel.pack_points[index]),
			ae_travel.pack_points[index].z - tick->position.z, (boolean)(bot->goal_kind == _bots_goal_enemy));
		if (score > best_score)
		{
			best_score = score;
			best = index;
		}
	}
	if (best == NONE)
		return;
	{
		real_point3d ground;
		long surface = bots_surface_below(&ae_travel.pack_points[best], &ground);

		if (surface == NONE)
			return;
		travel->via = AE_TRAVEL_VIA_PACK;
		travel->phase = _ae_travel_phase_to_entry;
		travel->via_tick = now;
		travel->pack_object = ae_travel.pack_objects[best];
		travel->pack_point = ae_travel.pack_points[best];
		travel->pack_surface = surface;
		travel->pack_health = tick->object->object.body_vitality;
		bot->path.valid = FALSE;
		bot->path_tick = now - TICKS_PER_SECOND;
		ae_travel.pack_detours++;
		ae_travel_trace(bot, "pack detour", travel->pack_object, need, bots_distance2d(&tick->position, &travel->pack_point));
	}
}

/* how long its roaming places take to reach: those AE_TRAVEL_FAR_GOAL units off or more apart (an enemy's last place moves, a weapon
is fetched near) */
static void ae_travel_time_goal(struct bots_bot *bot, struct ae_travel_bot *travel, struct bots_tick *tick)
{
	if (bot->goal_kind != _bots_goal_roam)
		return;
	if (bot->goal_tick != travel->timed_goal_tick)
	{
		if (travel->timed_goal_tick != NONE && !travel->timed_done)
			ae_travel.goals_left++;
		travel->timed_goal_tick = bot->goal_tick;
		travel->timed_point = bot->goal_point;
		travel->timed_straight = bots_distance3d(&tick->position, &bot->goal_point);
		travel->timed_start_tick = tick->now;
		travel->timed_done = FALSE;
	}
	if (!travel->timed_done && bots_distance2d(&tick->position, &travel->timed_point) < 2.0f &&
		fabs(tick->position.z - travel->timed_point.z) < 2.0f)
	{
		travel->timed_done = TRUE;
		if (travel->timed_straight >= AE_TRAVEL_FAR_GOAL)
		{
			ae_travel.far_goals++;
			ae_travel.far_goal_ticks += tick->now - travel->timed_start_tick;
		}
		else
		{
			ae_travel.near_goals++;
			ae_travel.near_goal_ticks += tick->now - travel->timed_start_tick;
		}
	}
}

static void ae_travel_report(long now)
{
	char spots[400];

	if (now - ae_travel.last_report_tick < TICKS_PER_SECOND * 60 && now >= ae_travel.last_report_tick)
		return;
	ae_travel.last_report_tick = now;
	ae_bots_travel_stuck_minute(&ae_travel.stuck, 8, spots, (int)sizeof(spots));
	platform_log("bots: travel tick %ld: teleports %ld (%ld planned, %ld pads missed), jumps %ld (%ld missed), "
		"links tried %ld taken %ld declined %ld, packs taken %ld of %ld detours, far goals %ld (%.1f s each), "
		"near goals %ld (%.1f s each), goals left %ld, no progress %ld; stuck spots this minute:%s", now, ae_travel.teleports,
		ae_travel.teleports_planned, ae_travel.pads_missed, ae_travel.jumps, ae_travel.jumps_missed,
		ae_travel.links_tried, ae_travel.links_taken, ae_travel.links_declined, ae_travel.packs_taken,
		ae_travel.pack_detours, ae_travel.far_goals,
		ae_travel.far_goals ? (double)ae_travel.far_goal_ticks / ae_travel.far_goals / TICKS_PER_SECOND : 0.0,
		ae_travel.near_goals,
		ae_travel.near_goals ? (double)ae_travel.near_goal_ticks / ae_travel.near_goals / TICKS_PER_SECOND : 0.0,
		ae_travel.goals_left, ae_travel.no_progress, spots[0] ? spots : " none");
}

void ae_bots_travel_tick(struct bots_bot *bot, long bot_index, struct bots_tick *tick)
{
	struct ae_travel_bot *travel;

	ae_travel_tick_bot = bot;
	ae_travel_tick_now = tick;
	if (!ae_travel.ready)
		return;
	travel = ae_travel_state(bot);
	if (!travel)
		return;
	if (travel->unit_index != tick->player->unit_index)
	{
		long declined[AE_BOTS_TRAVEL_MAXIMUM_LINKS];

		/* (spawned: a new start, the links it found failing still not tried a while) */
		csmemcpy(declined, travel->declined_until, sizeof(declined));
		ae_travel_bot_reset(travel, tick->player->unit_index);
		csmemcpy(travel->declined_until, declined, sizeof(declined));
	}
	/* (sent on by a teleporter: bots.c searches anew from there; which pad it came out at, and whether it meant to) */
	if (bots_distance3d(&tick->position, &bot->last_position) > 3.0f && tick->object->object.parent_object_index == NONE)
	{
		long index;

		for (index = 0; index < ae_travel.link_count; index++)
		{
			struct ae_bots_travel_link const *link = &ae_travel.links[index];

			if (link->kind != AE_BOTS_TRAVEL_LINK_TELEPORTER || bots_distance3d(&tick->position, &link->exit) > 2.0f)
				continue;
			ae_travel.teleports++;
			if (travel->via >= 0 && ae_travel.links[travel->via].kind == AE_BOTS_TRAVEL_LINK_TELEPORTER &&
				bots_distance3d(&ae_travel.links[travel->via].exit, &link->exit) < 0.5f)
			{
				ae_travel.teleports_planned++;
			}
			ae_travel_trace(bot, "teleported", index, (real)travel->via, 0.0f);
			travel->arrived = TRUE;
			travel->arrival = tick->position;
			travel->arrival_tick = tick->now;
			travel->arrival_link = (short)index;
			if (travel->via >= 0)
				ae_travel_drop_via(bot, travel, 0, "teleported");
			break;
		}
	}
	ae_travel_refresh_packs(tick->now);
	ae_travel_look_for_pack(bot, bot_index, travel, tick);
	ae_travel_time_goal(bot, travel, tick);
	ae_travel_check_progress(bot, travel, tick);
	ae_travel_report(tick->now);
}

/* nothing of the level between the two points at a biped's knees */
static boolean ae_travel_clear_line(real_point3d const *from, real_point3d const *to)
{
	struct collision_result collision;
	real_point3d start = *from;
	real_vector3d vector;

	start.z += 0.5f;
	vector.i = to->x - from->x;
	vector.j = to->y - from->y;
	vector.k = to->z - from->z;
	return (boolean)!collision_test_vector(FLAG(_collision_test_structure_bit) | FLAG(_collision_test_front_facing_surfaces_bit),
		&start, &vector, NONE, &collision);
}

/* a bot that stands stuck or comes no nearer its goal: a layout jump near it, about its height, whose landing is
nearer its goal, is the way out (a weapon on the floor below a ledge the walkable surfaces do not go down from) */
static void ae_travel_jump_out(struct bots_bot *bot, struct ae_travel_bot *travel, struct bots_tick *tick, char const *why)
{
	long index, best = NONE;
	real best_distance = AE_TRAVEL_STUCK_JUMP_REACH;

	if (travel->via != AE_TRAVEL_VIA_NONE || !ae_bots_travel_links_on() || bot->goal_kind == _bots_goal_none ||
		(bot->target_player_index != NONE && tick->now - bot->target_seen_tick < TICKS_PER_SECOND))
	{
		return;
	}
	for (index = 0; index < ae_travel.link_count; index++)
	{
		struct ae_bots_travel_link const *link = &ae_travel.links[index];
		real distance = bots_distance3d(&tick->position, &link->entry);

		if (link->kind != AE_BOTS_TRAVEL_LINK_JUMP || tick->now < travel->declined_until[index] ||
			distance >= best_distance || fabs(link->entry.z - tick->position.z) > 1.0f ||
			bots_distance3d(&link->exit, &bot->goal_point) >= bots_distance3d(&tick->position, &bot->goal_point) ||
			!ae_travel_clear_line(&tick->position, &link->entry))
		{
			continue;
		}
		best_distance = distance;
		best = index;
	}
	if (best == NONE)
		return;
	travel->via = (short)best;
	travel->direct = TRUE;
	travel->phase = _ae_travel_phase_to_entry;
	travel->goal_tick = bot->goal_tick;
	travel->goal_point = bot->goal_point;
	travel->via_tick = tick->now;
	bot->path.valid = FALSE;
	bot->path_tick = tick->now - TICKS_PER_SECOND;
	ae_travel.links_taken++;
	ae_travel_trace(bot, why, best, best_distance, 0.0f);
}

/* no nearer its goal in AE_TRAVEL_NO_PROGRESS_TICKS while it walks: a stuck spot too (it paces: its path goes where
it cannot) */
static void ae_travel_check_progress(struct bots_bot *bot, struct ae_travel_bot *travel, struct bots_tick *tick)
{
	real distance;

	if (bot->goal_kind == _bots_goal_none || travel->via != AE_TRAVEL_VIA_NONE || tick->object->object.parent_object_index != NONE ||
		(bot->target_player_index != NONE && tick->now - bot->target_seen_tick < TICKS_PER_SECOND * 2))
	{
		travel->progress_goal_tick = NONE;
		return;
	}
	distance = bots_distance3d(&tick->position, &bot->goal_point);
	if (travel->progress_goal_tick != bot->goal_tick || distance < travel->progress_best - 1.0f)
	{
		travel->progress_goal_tick = bot->goal_tick;
		travel->progress_best = distance;
		travel->progress_tick = tick->now;
		return;
	}
	if (tick->now - travel->progress_tick < AE_TRAVEL_NO_PROGRESS_TICKS)
		return;
	travel->progress_tick = tick->now;
	ae_travel.no_progress++;
	ae_bots_travel_stuck_add(&ae_travel.stuck, tick->position.x, tick->position.y, tick->position.z);
	ae_travel_jump_out(bot, travel, tick, "no progress: jump");
}

void ae_bots_travel_stuck(struct bots_bot *bot, struct bots_tick *tick)
{
	struct ae_travel_bot *travel;

	if (!ae_travel.ready)
		return;
	ae_bots_travel_stuck_add(&ae_travel.stuck, tick->position.x, tick->position.y, tick->position.z);
	travel = ae_travel_state(bot);
	if (travel)
		ae_travel_jump_out(bot, travel, tick, "stuck: jump");
}
