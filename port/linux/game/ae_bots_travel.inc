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

A via is dropped when the goal moves more than 3 units (the objective path correction threshold) or turns
to a weapon to fetch, and a link that failed (the walk to it, the pad blocked, a jump missed) is not tried again for a
while. Nothing here draws a random number: the bots' runs stay the same for the same input.
*/

#include "ae_bots_travel.h"
#include "ae_bots_travel_rules.h"
#include "physics/collision_bsp.h"

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
/* a fighting bot faster than this (world units a tick; a run is about 0.075) strafes, not pinned */
#define AE_TRAVEL_STRAFING_SPEED 0.025f
/* a bot this long no nearer its goal while it walks has a stuck spot (a 0.3 s wrong path
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
	long walks;
	long walks_missed;
	long links_tried;
	long links_taken;
	long links_declined;
	/* links chosen by the route between the walkable surfaces' pieces */
	long routes;
	long pack_detours;
	long packs_taken;
	long far_goals;
	long far_goal_ticks;
	long near_goals;
	long near_goal_ticks;
	long goals_left;
	long no_progress;
	/* searches made again to a via on a long way */
	long long_ways;
} ae_travel;

static struct ae_travel_bot ae_travel_bots[BOTS_BRAINS];

/* WALK LINKS (ae_bots_travel_rules.h): the samples of the bigger pieces, a few looked across from a tick */
#define AE_TRAVEL_WALK_SAMPLES 4096
/* a piece with fewer surfaces (a stair's tread, a ledge's sliver) gets no ends of its own */
#define AE_TRAVEL_WALK_BIG 3
/* walk links from one piece to another at most, and this far apart */
#define AE_TRAVEL_WALKS_A_PAIR 3
#define AE_TRAVEL_WALK_SPACING 2.5f
/* samples looked across a tick; surfaces sampled a tick */
#define AE_TRAVEL_WALK_A_TICK 3
#define AE_TRAVEL_WALK_BUILD_A_TICK 256
/* the ground a walk link keeps to either side of its line */
#define AE_TRAVEL_WALK_MARGIN 0.4f
/* the other pieces each sample tries */
#define AE_TRAVEL_WALK_TRIES 3

static struct
{
	struct structure_bsp const *source;
	/* the samples taken over a few ticks: counted first (every surface of a bigger piece), then taken a stride
	apart over the whole map, so that a big map's are not all at its first surfaces */
	boolean built;
	boolean counted;
	long build_surface;
	long build_total;
	long build_stride;
	long build_seen;
	boolean done;
	long count;
	real_point3d point[AE_TRAVEL_WALK_SAMPLES];
	long surface[AE_TRAVEL_WALK_SAMPLES];
	long piece[AE_TRAVEL_WALK_SAMPLES];
	long next;
	long tests;
	long rays;
	long found;
	long ticks;
	double seconds;
	/* each game's links (ae_bots_travel_prepare): the links' pieces known again */
	long generation;
} ae_travel_walks;
/* (a search made from here: bots_search's hook lets it through) */
static boolean ae_travel_searching;
/* the bot whose tick runs now, and its tick (the jump's button, the throttle onto a pad) */
static struct bots_bot *ae_travel_tick_bot;
static struct bots_tick *ae_travel_tick_now;

/* (debug.bot_trace) a line for each via chosen, dropped, made */
static void ae_travel_trace(struct bots_bot *bot, char const *what, long via, real a, real b)
{
	if (ae_bots_trace_level() <= 0)
		return;
	platform_log("bots: travel trace %ld bot %ld %s %ld (%.1f %.1f)", game_time_get(), (long)(bot - bots_globals.bots) + 1,
		what, via, (double)a, (double)b);
}

static struct ae_travel_bot *ae_travel_state(struct bots_bot *bot)
{
	long index = (long)(bot - bots_globals.bots);

	return index >= 0 && index < BOTS_BRAINS ? &ae_travel_bots[index] : NULL;
}

static void ae_travel_check_progress(struct bots_bot *bot, struct ae_travel_bot *travel, struct bots_tick *tick);
static long ae_travel_piece(long surface);
static long ae_travel_link_piece(long index, boolean exit);
static void ae_travel_walks_scan(void);
static boolean ae_travel_long_walk(struct bots_bot const *bot, real *walk);
static void ae_travel_ways_release(void);

/* a link's hop: a teleporter's cost, a jump's, a walk link's walk */
static real ae_travel_hop(struct ae_bots_travel_link const *link)
{
	real length = bots_distance3d(&link->entry, &link->exit);

	if (link->kind == AE_BOTS_TRAVEL_LINK_WALK)
		return length * AE_BOTS_TRAVEL_WALK_STRETCH;
	return ae_bots_travel_hop_cost((boolean)(link->kind == AE_BOTS_TRAVEL_LINK_JUMP), length);
}

/* how near a link's entry its last steps are walked straight */
static real ae_travel_approach(struct ae_bots_travel_link const *link)
{
	return link->kind == AE_BOTS_TRAVEL_LINK_TELEPORTER ? AE_TRAVEL_PAD_APPROACH : AE_TRAVEL_JUMP_APPROACH;
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
	/* (a new game: the long ways' walks made again, the surfaces closed anew) */
	ae_travel_ways_release();
	/* (a new game: its walk links found again, after its links read here) */
	ae_travel_walks.source = NULL;
	ae_travel_walks.generation++;
	for (index = 0; index < BOTS_BRAINS; index++)
		ae_travel_bot_reset(&ae_travel_bots[index], NONE);
	/* the other lanes' bots start over too: a player index repeats from game to game (the player data is
	remade each map with the same identifiers), so a state kept by player index would carry the last game's
	pauses, patrol points and timers into this one */
	csmemset(ae_bots_objective_bots, 0, sizeof(ae_bots_objective_bots));
	csmemset(ae_bots_objective_routed_tick, 0, sizeof(ae_bots_objective_routed_tick));
	csmemset(ae_bots_cover_bots, 0, sizeof(ae_bots_cover_bots));
	csmemset(ae_bots_vehicle_bots, 0, sizeof(ae_bots_vehicle_bots));
	for (index = 0; index < BOTS_BRAINS; index++)
	{
		ae_bots_objective_bots[index].player_index = NONE;
		ae_bots_cover_bots[index].player_index = NONE;
		ae_bots_cover_bots[index].place = NONE;
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

		/* (health up with the pack gone: taken. Health up with it still there is REACH health's own regeneration, not a
		pack: the detour goes on until the pack is not needed) */
		if (object && object->object.body_vitality > travel->pack_health + 0.05f &&
			!ae_bots_travel_pack_there(travel->pack_object))
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
		(bot->goal_kind == _bots_goal_item && ae_travel.links[travel->via].kind == AE_BOTS_TRAVEL_LINK_TELEPORTER))
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
				ae_travel_hop(link), bots_distance3d(&link->exit, &bot->goal_point));

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
	long start_piece = NONE, goal_piece = ae_travel_piece(bot->goal_surface_index);
	boolean routed = FALSE;

	/* (the goal on another piece of the walkable surfaces: no walk reaches it, whatever the search's estimate; the
	links' route between the pieces, the fewest links, its first one) */
	{
		real_point3d ground;
		long surface = bots_unit_surface(unit_index, &ground);

		start_piece = surface != NONE ? ae_travel_piece(surface) : NONE;
	}
	if (start_piece != NONE && goal_piece != NONE && start_piece != goal_piece)
	{
		long from[AE_BOTS_TRAVEL_MAXIMUM_LINKS], to[AE_BOTS_TRAVEL_MAXIMUM_LINKS];
		unsigned char usable[AE_BOTS_TRAVEL_MAXIMUM_LINKS];

		for (index = 0; index < ae_travel.link_count; index++)
		{
			struct ae_bots_travel_link const *link = &ae_travel.links[index];

			from[index] = ae_travel_link_piece(index, FALSE);
			to[index] = ae_travel_link_piece(index, TRUE);
			usable[index] = (unsigned char)(now >= travel->declined_until[index] &&
				ae_bots_travel_link_allowed(link->entry.x, link->entry.y, link->entry.z, travel->arrived, travel->arrival.x,
					travel->arrival.y, travel->arrival.z, now - travel->arrival_tick, (boolean)(travel->arrival_link == index)));
			if (from[index] == NONE)
				from[index] = -1;
			if (to[index] == NONE)
				to[index] = -1;
		}
		best = ae_bots_travel_route((int)ae_travel.link_count, from, to, usable, start_piece, goal_piece);
		if (best >= 0)
		{
			routed = TRUE;
			best_estimate = ae_bots_travel_link_estimate(bots_distance3d(position, &ae_travel.links[best].entry),
				ae_travel_hop(&ae_travel.links[best]), bots_distance3d(&ae_travel.links[best].exit, &bot->goal_point));
			ae_travel.routes++;
		}
		else
			best = NONE;
	}
	for (index = 0; !routed && index < ae_travel.link_count; index++)
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
			ae_travel_hop(link), exit_to_goal);
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
		real hop = ae_travel_hop(link);

		ae_travel.links_tried++;
		if (ae_travel_search_to(bot, unit_index, &ae_travel.entry_ground[best], ae_travel.entry_surface[best]) &&
			ae_travel_path_reaches(bot, &ae_travel.entry_ground[best], ae_travel_approach(link)) && (routed ||
			ae_bots_travel_link_taken(ae_travel_walk_found(position, &ae_travel.entry_ground[best], ae_travel.entry_surface[best],
				ae_travel_approach(link)), hop, bots_distance3d(&link->exit, &bot->goal_point), walk)))
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

	/* (only the bot's own feet: the driving spike's search for its vehicle, debug.bot_vehicles 2, gets no teleporter
	or pack on the way) */
	if (ae_travel_searching || !ae_travel.ready || bot->goal_surface_index == NONE || unit_index != bot->unit_index ||
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
		real walk;

		/* (a search gone to a via on a long way: that way's walk) */
		if (!ae_travel_long_walk(bot, &walk))
			walk = ae_travel_walk_found(&position, &bot->goal_point, bot->goal_surface_index, 0.0f);
		ae_travel_consider_links(bot, travel, unit_index, &position, walk);
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

boolean ae_bots_travel_climb_blocked(struct bots_bot *bot, real_point3d const *position)
{
	real_point3d const *goal = &bot->goal_point;
	real across = (real)sqrt((goal->x - position->x) * (goal->x - position->x) + (goal->y - position->y) * (goal->y - position->y));
	static const real ahead[2] = { 1.0f, 2.5f };
	long which;

	if (across < 0.5f)
		return FALSE;
	for (which = 0; which < 2; which++)
	{
		real_point3d point = *position, ground;
		real reach = MIN(ahead[which], across);

		point.x += (goal->x - position->x) / across * reach;
		point.y += (goal->y - position->y) / across * reach;
		if (ae_bots_travel_climb_drops(bots_surface_below(&point, &ground) != NONE, position->z, ground.z, goal->z))
		{
			/* (counted in halo.log: a climb that would have gone on over the drop) */
			ae_bots_event_line((long)(bot - bots_globals.bots), game_time_get(), "climb_drop", -1, position->z, "");
			return TRUE;
		}
	}
	return FALSE;
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
			if (distance > ae_travel_approach(link) || fabs(position->z - point->z) > 1.5f)
				return FALSE;
			travel->phase = _ae_travel_phase_at_entry;
			travel->approach_tick = now;
		}
		if (link->kind == AE_BOTS_TRAVEL_LINK_WALK)
		{
			/* (a walk link: straight on from its entry to its exit, up the stairs or the ramp the searches miss) */
			real across = bots_distance2d(&link->entry, &link->exit), left = bots_distance2d(position, &link->exit);

			if (travel->phase == _ae_travel_phase_at_entry)
			{
				travel->phase = _ae_travel_phase_in_air;
				travel->jump_tick = now;
				ae_travel.walks++;
			}
			if (left < 0.6f && fabs(position->z - link->exit.z) < 0.8f)
			{
				travel->arrived = TRUE;
				travel->arrival = *position;
				travel->arrival_tick = now;
				travel->arrival_link = travel->via;
				ae_travel_drop_via(bot, travel, 0, "walked");
				return FALSE;
			}
			if (now - travel->jump_tick > (long)((across / 1.2f + 2.0f) * TICKS_PER_SECOND))
			{
				ae_travel.walks_missed++;
				ae_travel_drop_via(bot, travel, TICKS_PER_SECOND * 30, "walk not made");
				return FALSE;
			}
			*direction = (real)atan2(link->exit.y - position->y, link->exit.x - position->x);
			return TRUE;
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

/* ---------- a goal stuck on, a moving machine (bots.c's hooks, 0.5.0) */

/* one more stuck report of the bot's goal (ae_bots_travel_goal_stuck_next) */
static void ae_bots_travel_goal_stuck_count(struct bots_bot *bot, long now)
{
	bot->goal_stuck_count = (short)ae_bots_travel_goal_stuck_next(bot->goal_stuck_count, bot->goal_stuck_tick, now);
	bot->goal_stuck_tick = now;
}

/* its goal given up (goal_kind none, goal_point still the old goal): a roam goal again, not the same place: the first
of up to 4 picks more than 3 units from the old goal, else the farthest of them (review 10-29: none at all let the old
goal be picked again the next tick) */
static void ae_bots_travel_repick(struct bots_bot *bot, real_point3d const *position)
{
	real_point3d old_goal = bot->goal_point, best_point = bot->goal_point;
	short best_kind = _bots_goal_none;
	long best_surface = NONE, tries;
	real best_distance = -1.0f;

	if (bot->goal_stuck_count >= AE_BOTS_TRAVEL_GOAL_STUCK_GIVE_UP)
	{
		platform_log("bots: bot %ld gave up goal (%.1f %.1f %.1f) after %d stucks", (long)(bot - bots_globals.bots),
			old_goal.x, old_goal.y, old_goal.z, (int)bot->goal_stuck_count);
	}
	bot->goal_stuck_count = 0;
	for (tries = 0; tries < 4; tries++)
	{
		real distance;

		bot->goal_kind = _bots_goal_none;
		bots_pick_roam_goal(bot, position);
		if (bot->goal_kind == _bots_goal_none)
			continue;
		distance = bots_distance3d(&bot->goal_point, &old_goal);
		if (distance > 3.0f)
			return;
		if (distance > best_distance)
		{
			best_distance = distance;
			best_point = bot->goal_point;
			best_kind = bot->goal_kind;
			best_surface = bot->goal_surface_index;
		}
	}
	bot->goal_kind = _bots_goal_none;
	if (best_kind != _bots_goal_none)
		bots_set_goal(bot, best_kind, &best_point, best_surface);
}

/* a surface's middle (its corners' mean), open or closed; FALSE for one with fewer than 3 corners */
static boolean ae_travel_surface_middle(struct collision_bsp const *bsp, long surface_index, real_point3d *middle)
{
	real_point3d corners[BOTS_SURFACE_CORNERS];
	long corner, count = bots_surface_corners(bsp, surface_index, corners);

	if (count < 3)
		return FALSE;
	middle->x = middle->y = middle->z = 0.0f;
	for (corner = 0; corner < count; corner++)
	{
		middle->x += corners[corner].x / (real)count;
		middle->y += corners[corner].y / (real)count;
		middle->z += corners[corner].z / (real)count;
	}
	return TRUE;
}

/* a moving machine learnt in the way whose rays closed nothing (a fan's blades): the walkable surfaces whose middle lies
within its block radius (ae_bots_travel_device_block_radius) of its centre closed; none when a surface the map has
walkable there is closed already (a layout's bot avoid, an earlier learn: review 10-29, it then closed the floor about
a place already kept out of). The count closed */
static long ae_bots_travel_close_device(long object_index, struct collision_bsp const *bsp, real block_radius)
{
	struct object_datum const *object = object_get(object_index);
	byte const *map_walkable = (byte const *)xbox_pointer(bots_paths.source->pathfinding_surfaces.address);
	real radius = ae_bots_travel_device_block_radius(block_radius);
	long count = bots_paths.structure.pathfinding_surfaces.count, surface_index, closed = 0;
	real_point3d middle;

	for (surface_index = 0; surface_index < count && surface_index < bsp->surfaces.count; surface_index++)
	{
		if (TEST_FLAG(map_walkable[surface_index], BOTS_PATHFINDING_WALKABLE_BIT) &&
			!TEST_FLAG(bots_paths.walkable[surface_index], BOTS_PATHFINDING_WALKABLE_BIT) &&
			ae_travel_surface_middle(bsp, surface_index, &middle) &&
			bots_distance3d(&middle, &object->object.bounding_sphere_center) < radius)
		{
			return 0;
		}
	}
	for (surface_index = 0; surface_index < count && surface_index < bsp->surfaces.count; surface_index++)
	{
		if (TEST_FLAG(bots_paths.walkable[surface_index], BOTS_PATHFINDING_WALKABLE_BIT) &&
			ae_travel_surface_middle(bsp, surface_index, &middle) &&
			bots_distance3d(&middle, &object->object.bounding_sphere_center) < radius)
		{
			bots_paths.walkable[surface_index] &= (byte)~FLAG(BOTS_PATHFINDING_WALKABLE_BIT);
			closed++;
		}
	}
	return closed;
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
	platform_log("bots: travel tick %ld: teleports %ld (%ld planned, %ld pads missed), jumps %ld (%ld missed), walks %ld (%ld missed), "
		"links tried %ld taken %ld declined %ld (routed %ld), packs taken %ld of %ld detours, far goals %ld (%.1f s each), "
		"near goals %ld (%.1f s each), goals left %ld, no progress %ld, long ways %ld; stuck spots this minute:%s", now, ae_travel.teleports,
		ae_travel.teleports_planned, ae_travel.pads_missed, ae_travel.jumps, ae_travel.jumps_missed, ae_travel.walks,
		ae_travel.walks_missed, ae_travel.links_tried, ae_travel.links_taken, ae_travel.links_declined, ae_travel.routes,
		ae_travel.packs_taken,
		ae_travel.pack_detours, ae_travel.far_goals,
		ae_travel.far_goals ? (double)ae_travel.far_goal_ticks / ae_travel.far_goals / TICKS_PER_SECOND : 0.0,
		ae_travel.near_goals,
		ae_travel.near_goals ? (double)ae_travel.near_goal_ticks / ae_travel.near_goals / TICKS_PER_SECOND : 0.0,
		ae_travel.goals_left, ae_travel.no_progress, ae_travel.long_ways, spots[0] ? spots : " none");
}

/* the tick's bot and controls let go at the end of its update (ae_bots_events_end): no later caller of
bots_follow_path finds another bot's stack */
static void ae_bots_travel_tick_end(void)
{
	ae_travel_tick_bot = NULL;
	ae_travel_tick_now = NULL;
}

void ae_bots_travel_tick(struct bots_bot *bot, long bot_index, struct bots_tick *tick)
{
	struct ae_travel_bot *travel;

	ae_travel_tick_bot = bot;
	ae_travel_tick_now = tick;
	/* (the countdown holds every player where they stand: the stuck check counts from its end, not from a spot taken
	inside it, else every bot is "stuck" at its spawn on the first tick it can walk, each round: Weld 10-29, 12 of 41) */
	if (game_engine_pregame_countdown_ticks_left() > 0)
	{
		bot->stuck_position = tick->position;
		bot->stuck_tick = tick->now;
	}
	if (!ae_travel.ready)
		return;
	/* (the walk links, once a tick: the first bot's) */
	{
		static long walks_tick = NONE;

		if (walks_tick != tick->now && ae_bots_travel_links_on())
		{
			walks_tick = tick->now;
			ae_travel_walks_scan();
		}
	}
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

/* the walkable surfaces' pieces: surfaces joined by an edge both are walkable on make one piece (the bots' searches
reach no other piece by walking); made once the scan for surfaces with no room above is done. NONE for none */
static struct
{
	struct structure_bsp const *source;
	long count;
	long *piece;
	/* each piece's surfaces (by its root) */
	long *size;
	long pieces;
} ae_travel_pieces;

static long ae_travel_piece_root(long surface)
{
	while (ae_travel_pieces.piece[surface] != surface)
	{
		ae_travel_pieces.piece[surface] = ae_travel_pieces.piece[ae_travel_pieces.piece[surface]];
		surface = ae_travel_pieces.piece[surface];
	}
	return surface;
}

static long ae_travel_piece(long surface)
{
	struct collision_bsp const *bsp;
	long index;

	if (!bots_paths.walkable || !bots_paths.scan_done)
		return NONE;
	if (ae_travel_pieces.source != bots_paths.source)
	{
		if (ae_travel_pieces.piece)
			free(ae_travel_pieces.piece);
		if (ae_travel_pieces.size)
			free(ae_travel_pieces.size);
		csmemset(&ae_travel_pieces, 0, sizeof(ae_travel_pieces));
		bsp = TAG_BLOCK_GET_ELEMENT(&bots_paths.structure.collision_bsp, 0, struct collision_bsp);
		ae_travel_pieces.count = MIN(bsp->surfaces.count, bots_paths.structure.pathfinding_surfaces.count);
		ae_travel_pieces.piece = (long *)malloc(sizeof(long) * (size_t)MAX(ae_travel_pieces.count, 1));
		ae_travel_pieces.size = (long *)malloc(sizeof(long) * (size_t)MAX(ae_travel_pieces.count, 1));
		/* (none: tried again next time; the map's pieces only once both are had) */
		if (!ae_travel_pieces.piece || !ae_travel_pieces.size)
		{
			if (ae_travel_pieces.piece)
				free(ae_travel_pieces.piece);
			if (ae_travel_pieces.size)
				free(ae_travel_pieces.size);
			csmemset(&ae_travel_pieces, 0, sizeof(ae_travel_pieces));
			return NONE;
		}
		ae_travel_pieces.source = bots_paths.source;
		for (index = 0; index < ae_travel_pieces.count; index++)
			ae_travel_pieces.piece[index] = index;
		for (index = 0; index < bsp->edges.count; index++)
		{
			struct collision_edge const *edge = TAG_BLOCK_GET_ELEMENT(&bsp->edges, index, struct collision_edge);
			long a = edge->surface_indices[0], b = edge->surface_indices[1];

			if (!VALID_INDEX(a, ae_travel_pieces.count) || !VALID_INDEX(b, ae_travel_pieces.count) ||
				!TEST_FLAG(bots_paths.walkable[a], BOTS_PATHFINDING_WALKABLE_BIT) ||
				!TEST_FLAG(bots_paths.walkable[b], BOTS_PATHFINDING_WALKABLE_BIT))
			{
				continue;
			}
			a = ae_travel_piece_root(a);
			b = ae_travel_piece_root(b);
			if (a != b)
				ae_travel_pieces.piece[a] = b;
		}
		for (index = 0; index < ae_travel_pieces.count; index++)
			ae_travel_pieces.size[index] = 0;
		for (index = 0; index < ae_travel_pieces.count; index++)
		{
			if (!TEST_FLAG(bots_paths.walkable[index], BOTS_PATHFINDING_WALKABLE_BIT))
				continue;
			ae_travel_pieces.size[ae_travel_piece_root(index)]++;
			if (ae_travel_piece_root(index) == index)
				ae_travel_pieces.pieces++;
		}
		platform_log("bots: travel: walkable surfaces in %ld pieces", ae_travel_pieces.pieces);
	}
	if (!ae_travel_pieces.piece || !VALID_INDEX(surface, ae_travel_pieces.count) ||
		!TEST_FLAG(bots_paths.walkable[surface], BOTS_PATHFINDING_WALKABLE_BIT))
	{
		return NONE;
	}
	return ae_travel_piece_root(surface);
}

/* the piece of a link's entry or exit ground (NONE for none known) */
static long ae_travel_link_piece(long index, boolean exit)
{
	/* (kept: a link's ends stay on their pieces; a new map's pieces start the cache again) */
	static long generation = -1;
	static long pieces[AE_BOTS_TRAVEL_MAXIMUM_LINKS][2];
	static boolean known[AE_BOTS_TRAVEL_MAXIMUM_LINKS];
	real_point3d ground;
	long surface;

	if (!VALID_INDEX(index, ae_travel.link_count) || ae_travel_piece(ae_travel.entry_surface[index]) == NONE)
		return NONE;
	if (generation != ae_travel_walks.generation)
	{
		generation = ae_travel_walks.generation;
		csmemset(known, 0, sizeof(known));
	}
	if (!known[index])
	{
		surface = bots_surface_below(&ae_travel.links[index].exit, &ground);
		pieces[index][0] = ae_travel_piece(ae_travel.entry_surface[index]);
		pieces[index][1] = surface != NONE ? ae_travel_piece(surface) : NONE;
		known[index] = TRUE;
	}
	return pieces[index][exit ? 1 : 0];
}


static double ae_travel_clock(void)
{
	LARGE_INTEGER now, frequency;

	QueryPerformanceCounter(&now);
	QueryPerformanceFrequency(&frequency);
	return (double)now.QuadPart / (double)frequency.QuadPart;
}

/* whether a biped walks straight from a to b (ground points): ground under every step, each step a stair's, a clear
way at the knees and the chest, room above; the rays counted */
static boolean ae_travel_walk_line(real_point3d const *a, real_point3d const *b, long *rays)
{
	unsigned long flags = FLAG(_collision_test_structure_bit) | FLAG(_collision_test_front_facing_surfaces_bit) |
		FLAG(_collision_test_objects_bit) | FLAG(_collision_test_objects_scenery_bit) |
		FLAG(_collision_test_objects_machines_bit);
	real across = bots_distance2d(a, b);
	long steps = (long)(across / AE_BOTS_TRAVEL_WALK_STEP) + 1, step;
	real_point3d last = *a;

	for (step = 1; step <= steps; step++)
	{
		real t = (real)step / (real)steps;
		struct collision_result collision;
		real_point3d point, ground, start;
		real_vector3d vector;

		point.x = a->x + (b->x - a->x) * t;
		point.y = a->y + (b->y - a->y) * t;
		point.z = MAX(last.z, a->z + (b->z - a->z) * t) + 0.5f;
		/* (ground: from above the step's own rise down; and to either side, a step's width off the line, no drop: a
		walk beside a pit's edge, nudged, falls in) */
		(*rays)++;
		if (bots_surface_below(&point, &ground) == NONE || !ae_bots_travel_walk_step(ground.z - last.z))
			return FALSE;
		{
			real side_x = -(b->y - a->y) / across * AE_TRAVEL_WALK_MARGIN, side_y = (b->x - a->x) / across * AE_TRAVEL_WALK_MARGIN;
			long side;

			for (side = -1; side <= 1; side += 2)
			{
				real_point3d beside = point, beside_ground;

				beside.x += side_x * (real)side;
				beside.y += side_y * (real)side;
				(*rays)++;
				if (bots_surface_below(&beside, &beside_ground) == NONE || ground.z - beside_ground.z > AE_BOTS_TRAVEL_WALK_FALL)
					return FALSE;
			}
		}
		/* (the way from the last ground: the knees, over a stair's rise, and the chest) */
		start = last;
		start.z += AE_BOTS_TRAVEL_WALK_RISE + 0.1f;
		vector.i = ground.x - last.x;
		vector.j = ground.y - last.y;
		vector.k = ground.z - last.z;
		(*rays)++;
		if (collision_test_vector(flags, &start, &vector, NONE, &collision))
			return FALSE;
		start.z = last.z + 0.9f;
		(*rays)++;
		if (collision_test_vector(flags, &start, &vector, NONE, &collision))
			return FALSE;
		/* (room to stand) */
		start = ground;
		start.z += 0.05f;
		vector.i = vector.j = 0.0f;
		vector.k = 0.7f;
		(*rays)++;
		if (collision_test_vector(flags | FLAG(_collision_test_back_facing_surfaces_bit), &start, &vector, NONE, &collision))
			return FALSE;
		last = ground;
	}
	return (boolean)(fabs(last.z - b->z) < 0.3f && bots_distance2d(&last, b) < 0.3f);
}

/* whether a walk link from piece to piece starts near the point already, and how many there are */
static long ae_travel_walks_between(long from, long to, real_point3d const *entry, boolean *near)
{
	long index, count = 0;

	*near = FALSE;
	for (index = 0; index < ae_travel.link_count; index++)
	{
		if (ae_travel.links[index].kind != AE_BOTS_TRAVEL_LINK_WALK || ae_travel.links[index].source != from ||
			ae_travel.links[index].target != to)
		{
			continue;
		}
		count++;
		if (bots_distance3d(&ae_travel.links[index].entry, entry) < AE_TRAVEL_WALK_SPACING)
			*near = TRUE;
	}
	return count;
}

static void ae_travel_walk_add(long sample, long other)
{
	long index = ae_travel.link_count;
	struct ae_bots_travel_link *link = &ae_travel.links[index];

	/* (the pieces kept in the link's source and target: a walk link has no netgame flag or layout line) */
	link->kind = AE_BOTS_TRAVEL_LINK_WALK;
	link->source = ae_travel_walks.piece[sample];
	link->target = ae_travel_walks.piece[other];
	link->entry = ae_travel_walks.point[sample];
	link->exit = ae_travel_walks.point[other];
	ae_travel.entry_ground[index] = ae_travel_walks.point[sample];
	ae_travel.entry_surface[index] = ae_travel_walks.surface[sample];
	ae_travel.link_count++;
	ae_travel_walks.found++;
}

/* the walk links, a few samples a tick once the walkable surfaces' pieces are known */
static void ae_travel_walks_scan(void)
{
	struct collision_bsp const *bsp;
	double started;
	long looked;

	if (!ae_travel.ready || !bots_paths.walkable || !bots_paths.scan_done)
		return;
	if (ae_travel_walks.source != bots_paths.source)
	{
		long generation = ae_travel_walks.generation;

		csmemset(&ae_travel_walks, 0, sizeof(ae_travel_walks));
		ae_travel_walks.source = bots_paths.source;
		ae_travel_walks.generation = generation;
	}
	if (ae_travel_walks.done)
		return;
	started = ae_travel_clock();
	ae_travel_walks.ticks++;
	bsp = TAG_BLOCK_GET_ELEMENT(&bots_paths.structure.collision_bsp, 0, struct collision_bsp);
	if (!ae_travel_walks.built)
	{
		long surface, last = MIN(bsp->surfaces.count, ae_travel_walks.build_surface + AE_TRAVEL_WALK_BUILD_A_TICK);

		for (surface = ae_travel_walks.build_surface; surface < last; surface++)
		{
			real_point3d corners[BOTS_SURFACE_CORNERS], middle;
			long corner_count, sample, piece = ae_travel_piece(surface);
			real reach;

			if (piece == NONE || ae_travel_pieces.size[piece] < AE_TRAVEL_WALK_BIG ||
				!bots_paths_walkable_surface(bsp, surface, corners, &corner_count, &middle, &reach))
			{
				continue;
			}
			for (sample = -1; sample < corner_count && ae_travel_walks.count < AE_TRAVEL_WALK_SAMPLES; sample++)
			{
				real_point3d point = middle, ground;
				long index = ae_travel_walks.count;

				if (sample >= 0 && reach < 1.0f)
					break;
				/* (counting: each one; taking: one a stride) */
				if (!ae_travel_walks.counted)
				{
					ae_travel_walks.build_total++;
					continue;
				}
				if (ae_travel_walks.build_seen++ % ae_travel_walks.build_stride)
					continue;
				if (sample >= 0)
				{
					point.x = 0.5f * (middle.x + corners[sample].x);
					point.y = 0.5f * (middle.y + corners[sample].y);
					point.z = 0.5f * (middle.z + corners[sample].z);
				}
				if (bots_surface_below(&point, &ground) != surface)
					continue;
				ae_travel_walks.point[index] = ground;
				ae_travel_walks.surface[index] = surface;
				ae_travel_walks.piece[index] = piece;
				ae_travel_walks.count++;
			}
		}
		ae_travel_walks.build_surface = last;
		if (last >= bsp->surfaces.count)
		{
			ae_travel_walks.build_surface = 0;
			if (!ae_travel_walks.counted)
			{
				ae_travel_walks.counted = TRUE;
				ae_travel_walks.build_stride = ae_travel_walks.build_total / AE_TRAVEL_WALK_SAMPLES + 1;
			}
			else
				ae_travel_walks.built = TRUE;
		}
		ae_travel_walks.seconds += ae_travel_clock() - started;
		return;
	}
	for (looked = 0; looked < AE_TRAVEL_WALK_A_TICK && ae_travel_walks.next < ae_travel_walks.count; looked++)
	{
		long sample = ae_travel_walks.next++, other, tries[AE_TRAVEL_WALK_TRIES], try_count = 0, index;
		real nearest[AE_TRAVEL_WALK_TRIES];

		/* (the nearest end in each of the nearest other pieces) */
		for (other = 0; other < ae_travel_walks.count; other++)
		{
			long piece = ae_travel_walks.piece[other], slot = NONE;
			real across = bots_distance2d(&ae_travel_walks.point[sample], &ae_travel_walks.point[other]);

			if (piece == ae_travel_walks.piece[sample] ||
				!ae_bots_travel_walk_ends(across, ae_travel_walks.point[other].z - ae_travel_walks.point[sample].z))
			{
				continue;
			}
			for (index = 0; index < try_count; index++)
			{
				if (ae_travel_walks.piece[tries[index]] == piece)
					slot = index;
			}
			if (slot == NONE)
			{
				if (try_count < AE_TRAVEL_WALK_TRIES)
					slot = try_count++;
				else
				{
					for (index = 0, slot = 0; index < try_count; index++)
					{
						if (nearest[index] > nearest[slot])
							slot = index;
					}
					if (across >= nearest[slot])
						continue;
				}
			}
			else if (across >= nearest[slot])
				continue;
			tries[slot] = other;
			nearest[slot] = across;
		}
		for (index = 0; index < try_count && ae_travel.link_count < AE_BOTS_TRAVEL_MAXIMUM_LINKS; index++)
		{
			long from = ae_travel_walks.piece[sample], to = ae_travel_walks.piece[tries[index]];
			boolean near;

			if (ae_travel_walks_between(from, to, &ae_travel_walks.point[sample], &near) >= AE_TRAVEL_WALKS_A_PAIR || near)
				continue;
			ae_travel_walks.tests++;
			if (ae_travel_walk_line(&ae_travel_walks.point[sample], &ae_travel_walks.point[tries[index]], &ae_travel_walks.rays))
				ae_travel_walk_add(sample, tries[index]);
		}
	}
	if (ae_travel_walks.next >= ae_travel_walks.count || ae_travel.link_count >= AE_BOTS_TRAVEL_MAXIMUM_LINKS)
		ae_travel_walks.done = TRUE;
	ae_travel_walks.seconds += ae_travel_clock() - started;
	if (ae_travel_walks.done)
	{
		platform_log("bots: travel: %ld walk links between the walkable surfaces' %ld pieces (%ld samples, %ld tried, %ld rays, "
			"%.1f ms over %ld ticks)", ae_travel_walks.found, ae_travel_pieces.pieces, ae_travel_walks.count,
			ae_travel_walks.tests, ae_travel_walks.rays, ae_travel_walks.seconds * 1000.0, ae_travel_walks.ticks);
	}
}

/* (debug.bot_events) a stuck bot's line: where it goes (its goal, its path's next step), the way it walks, and what is
ahead of it at its knees, waist and head (the distance to the level or scenery, 9 for nothing within 0.6 units), and
the ground half a unit ahead (its drop, 9 for none within 6 units): a step, a railing, a doorway's beam, a ledge */
static void ae_travel_stuck_line(struct bots_bot *bot, struct bots_tick *tick)
{
	static real const heights[] = { 0.15f, 0.45f, 0.75f };
	unsigned long flags = FLAG(_collision_test_structure_bit) | FLAG(_collision_test_front_facing_surfaces_bit) |
		FLAG(_collision_test_objects_bit) | FLAG(_collision_test_objects_scenery_bit) |
		FLAG(_collision_test_objects_machines_bit);
	real ahead[3], drop = 9.0f, step_x = 0.0f, step_y = 0.0f, step_z = 0.0f;
	struct collision_result collision;
	real_point3d ground, start;
	char extra[240], pieces[40];
	long index;

	if (!ae_bots_events_on())
		return;
	for (index = 0; index < 3; index++)
	{
		real_vector3d vector;

		start = tick->position;
		start.z += heights[index];
		vector.i = (real)cos(tick->move_direction) * 0.6f;
		vector.j = (real)sin(tick->move_direction) * 0.6f;
		vector.k = 0.0f;
		ahead[index] = collision_test_vector(flags, &start, &vector, tick->player->unit_index, &collision) ?
			collision.t * 0.6f : 9.0f;
	}
	start = tick->position;
	start.x += (real)cos(tick->move_direction) * 0.5f;
	start.y += (real)sin(tick->move_direction) * 0.5f;
	start.z += 0.3f;
	if (bots_surface_below(&start, &ground) != NONE)
		drop = tick->position.z - ground.z;
	if (bot->path.valid && bot->path.step_index < bot->path.step_count)
	{
		struct path_step const *step = &bot->path.steps[(long)bot->path.step_index];

		step_x = step->point.x;
		step_y = step->point.y;
		step_z = step->point.z;
	}
	{
		real_point3d own;
		long own_surface = bots_unit_surface(tick->player->unit_index, &own);

		snprintf(pieces, sizeof(pieces), " pieces=%ld/%ld", own_surface != NONE ? ae_travel_piece(own_surface) : -2L,
			bot->goal_surface_index != NONE ? ae_travel_piece(bot->goal_surface_index) : -2L);
	}
	snprintf(extra, sizeof(extra), "goal=%d gx=%.1f gy=%.1f gz=%.1f path=%s%s%s step=%.1f,%.1f,%.1f dir=%.0f "
		"ahead=%.2f/%.2f/%.2f drop=%.2f x=%.1f y=%.1f z=%.1f", (int)bot->goal_kind, bot->goal_point.x, bot->goal_point.y,
		bot->goal_point.z, bot->path.valid ? "yes" : "no", bot->path_short ? "+short" : "",
		bot->climb_until_tick && tick->now < bot->climb_until_tick ? "+climb" : "", step_x, step_y, step_z,
		tick->move_direction / BOTS_DEGREES, ahead[0], ahead[1], ahead[2], drop, tick->position.x, tick->position.y,
		tick->position.z);
	strncat(extra, pieces, sizeof(extra) - strlen(extra) - 1);
	ae_bots_event_line((long)(bot - bots_globals.bots), tick->now, "stuck", -1, (real)bot->stuck_count, extra);
}

boolean ae_bots_travel_may_stick(struct bots_bot *bot, struct bots_tick *tick)
{
	long bot_index = (long)(bot - bots_globals.bots);

	/* (the countdown holds every player where they stand) */
	if (game_engine_pregame_countdown_ticks_left() > 0)
		return FALSE;
	/* (a fight's strafing goes to and fro by design: in an objective's mode, a bot that fights and strafes (it moves
	now, not walking its objective's path) is not stuck: Madhouse 10-25, a third of the stuck counts were fights, and
	the third stuck in a row gave up the hill. One pinned on the level in a fight (hardly moving) is stuck as ever, and
	Slayer's fights are counted as they always were) */
	if (tick->fighting && bot_index >= 0 && bot_index < BOTS_BRAINS &&
		tick->now - ae_bots_objective_routed_tick[bot_index] > TICKS_PER_SECOND / 2)
	{
		struct ae_bots_objective_snapshot const *snapshot = ae_bots_objective_snapshot();
		real_vector3d const *velocity = &tick->object->object.translational_velocity;

		if (snapshot && snapshot->mode != AE_BOTS_MODE_SLAYER &&
			velocity->i * velocity->i + velocity->j * velocity->j > AE_TRAVEL_STRAFING_SPEED * AE_TRAVEL_STRAFING_SPEED)
		{
			return FALSE;
		}
	}
	return TRUE;
}

void ae_bots_travel_stuck(struct bots_bot *bot, struct bots_tick *tick)
{
	struct ae_travel_bot *travel;

	if (!ae_travel.ready)
		return;
	ae_travel_stuck_line(bot, tick);
	ae_bots_travel_stuck_add(&ae_travel.stuck, tick->position.x, tick->position.y, tick->position.z);
	travel = ae_travel_state(bot);
	if (travel)
		ae_travel_jump_out(bot, travel, tick, "stuck: jump");
}

static boolean ae_bots_travel_near_pad(real_point3d const *point, real radius)
{
	long index;

	for (index = 0; index < ae_travel.link_count; index++)
	{
		if (ae_travel.links[index].kind == AE_BOTS_TRAVEL_LINK_TELEPORTER &&
			bots_distance3d(&ae_travel.links[index].entry, point) < radius)
		{
			return TRUE;
		}
	}
	return FALSE;
}

/* ---------- long ways (ae_bots_travel_rules.h): the walk to a goal over the whole piece, a via on it */

/* the goals' walks kept at once, and those made in a tick at most (a new one waits: the bot keeps the search's own
path this time) */
#define AE_TRAVEL_WAYS 6
#define AE_TRAVEL_WAYS_A_TICK 1
/* the most surfaces of a way looked along for its via */
#define AE_TRAVEL_WAY_STEPS 256

struct ae_travel_way
{
	long goal_surface;
	long used_tick;
	/* each surface's walk to the goal (negative: none), the surface it goes on to, the middle of the edge between */
	real *walk;
	long *next;
	real_point3d *exit;
};

static struct
{
	struct structure_bsp const *source;
	long count;
	/* the bots' closed surfaces when the walks were made: more closed, all made again */
	long closed;
	struct ae_travel_way ways[AE_TRAVEL_WAYS];
	long *heap;
	long heap_size;
	long tick;
	long made_this_tick;
	/* the search just made went to a via (the bot's path then ends there, not at its goal) */
	boolean via_searched;
	/* the walk to the goal the last via was searched on, its bot and tick */
	real via_walk;
	struct bots_bot const *via_bot;
	long via_tick;
} ae_travel_ways;

static long ae_travel_ways_closed(void)
{
	return bots_paths.closed_count + bots_paths.roofed_count + (bots_paths.scan_done ? 1 : 0);
}

static void ae_travel_ways_release(void)
{
	long index;

	for (index = 0; index < AE_TRAVEL_WAYS; index++)
	{
		struct ae_travel_way *way = &ae_travel_ways.ways[index];

		if (way->walk)
			free(way->walk);
		if (way->next)
			free(way->next);
		if (way->exit)
			free(way->exit);
	}
	if (ae_travel_ways.heap)
		free(ae_travel_ways.heap);
	csmemset(&ae_travel_ways, 0, sizeof(ae_travel_ways));
}

/* the heap of surfaces by their walk (a binary heap; a surface may be in it more than once, its old places skipped) */
static void ae_travel_heap_push(real const *walk, long *count, long surface)
{
	long at = (*count)++;

	while (at > 0 && walk[ae_travel_ways.heap[(at - 1) / 2]] > walk[surface])
	{
		ae_travel_ways.heap[at] = ae_travel_ways.heap[(at - 1) / 2];
		at = (at - 1) / 2;
	}
	ae_travel_ways.heap[at] = surface;
}

static long ae_travel_heap_pop(real const *walk, long *count)
{
	long top = ae_travel_ways.heap[0], last = ae_travel_ways.heap[--(*count)], at = 0;

	while (TRUE)
	{
		long child = at * 2 + 1;

		if (child >= *count)
			break;
		if (child + 1 < *count && walk[ae_travel_ways.heap[child + 1]] < walk[ae_travel_ways.heap[child]])
			child++;
		if (walk[ae_travel_ways.heap[child]] >= walk[last])
			break;
		ae_travel_ways.heap[at] = ae_travel_ways.heap[child];
		at = child;
	}
	if (*count > 0)
		ae_travel_ways.heap[at] = last;
	return top;
}

/* the walk from every surface of the goal's piece to the goal's surface: from the goal outwards over the walkable
surfaces the bots' searches read, each surface's ring of edges as path.c walks it, the middles of the edges crossed
(path.c's entry points). FALSE when not made */
static boolean ae_travel_way_make(struct ae_travel_way *way, long goal_surface, real_point3d const *goal_point)
{
	struct collision_bsp const *bsp = TAG_BLOCK_GET_ELEMENT(&bots_paths.structure.collision_bsp, 0, struct collision_bsp);
	long count = ae_travel_ways.count, surface, heap_count = 0;
	unsigned char *done;

	if (!way->walk)
		way->walk = (real *)malloc(sizeof(real) * (size_t)count);
	if (!way->next)
		way->next = (long *)malloc(sizeof(long) * (size_t)count);
	if (!way->exit)
		way->exit = (real_point3d *)malloc(sizeof(real_point3d) * (size_t)count);
	done = (unsigned char *)malloc((size_t)count);
	if (!way->walk || !way->next || !way->exit || !done || !ae_travel_ways.heap)
	{
		if (done)
			free(done);
		way->goal_surface = NONE;
		return FALSE;
	}
	for (surface = 0; surface < count; surface++)
	{
		way->walk[surface] = REAL_MAX;
		way->next[surface] = NONE;
	}
	csmemset(done, 0, (size_t)count);
	way->exit[goal_surface] = *goal_point;
	way->walk[goal_surface] = 0.0f;
	ae_travel_heap_push(way->walk, &heap_count, goal_surface);
	while (heap_count > 0)
	{
		struct collision_surface const *ring;
		long edge_index;
		short edge_count = 0;

		surface = ae_travel_heap_pop(way->walk, &heap_count);
		if (done[surface])
			continue;
		done[surface] = TRUE;
		ring = TAG_BLOCK_GET_ELEMENT(&bsp->surfaces, surface, struct collision_surface);
		edge_index = ring->first_edge_index;
		do
		{
			struct collision_edge const *edge;
			boolean right;
			long other;

			if (!collision_surface_edge_ring_continues(bsp, edge_index, edge_count))
				break;
			edge = TAG_BLOCK_GET_ELEMENT(&bsp->edges, edge_index, struct collision_edge);
			right = surface == edge->surface_indices[1];
			other = edge->surface_indices[!right];
			edge_count++;
			if (VALID_INDEX(other, count) && !done[other] &&
				TEST_FLAG(bots_paths.walkable[other], BOTS_PATHFINDING_WALKABLE_BIT) &&
				VALID_INDEX(edge->vertex_indices[0], bsp->vertices.count) &&
				VALID_INDEX(edge->vertex_indices[1], bsp->vertices.count))
			{
				real_point3d const *a = &TAG_BLOCK_GET_ELEMENT(&bsp->vertices, edge->vertex_indices[0], struct collision_vertex)->point;
				real_point3d const *b = &TAG_BLOCK_GET_ELEMENT(&bsp->vertices, edge->vertex_indices[1], struct collision_vertex)->point;
				real_point3d middle;
				real walk;

				middle.x = 0.5f * (a->x + b->x);
				middle.y = 0.5f * (a->y + b->y);
				middle.z = 0.5f * (a->z + b->z);
				walk = way->walk[surface] + bots_distance3d(&way->exit[surface], &middle);
				if (walk < way->walk[other] && heap_count < ae_travel_ways.heap_size)
				{
					way->walk[other] = walk;
					way->next[other] = surface;
					way->exit[other] = middle;
					ae_travel_heap_push(way->walk, &heap_count, other);
				}
			}
			if (edge_count >= MAXIMUM_PATH_EDGES_PER_COLLISION_SURFACE)
				break;
			edge_index = edge->edge_indices[right];
		} while (edge_index != ring->first_edge_index);
	}
	free(done);
	for (surface = 0; surface < count; surface++)
	{
		if (way->walk[surface] == REAL_MAX)
			way->walk[surface] = -1.0f;
	}
	way->goal_surface = goal_surface;
	return TRUE;
}

/* the walk to the goal's surface from every surface (kept for the goal while nothing more is closed); NULL when there
is none (another made this tick, no memory) */
static struct ae_travel_way *ae_travel_way(long goal_surface, real_point3d const *goal_point)
{
	struct collision_bsp const *bsp;
	long index, oldest = 0, now = game_time_get();
	struct ae_travel_way *way;

	if (!bots_paths.walkable || !bots_paths.source)
		return NULL;
	bsp = TAG_BLOCK_GET_ELEMENT(&bots_paths.structure.collision_bsp, 0, struct collision_bsp);
	if (ae_travel_ways.source != bots_paths.source || ae_travel_ways.closed != ae_travel_ways_closed())
	{
		long count = MIN(bsp->surfaces.count, bots_paths.structure.pathfinding_surfaces.count);

		if (ae_travel_ways.source != bots_paths.source || ae_travel_ways.count != count)
		{
			ae_travel_ways_release();
			ae_travel_ways.heap_size = MAX(bsp->edges.count * 2, 1) + 16;
			ae_travel_ways.heap = (long *)malloc(sizeof(long) * (size_t)ae_travel_ways.heap_size);
			ae_travel_ways.source = bots_paths.source;
			ae_travel_ways.count = count;
		}
		for (index = 0; index < AE_TRAVEL_WAYS; index++)
			ae_travel_ways.ways[index].goal_surface = NONE;
		ae_travel_ways.closed = ae_travel_ways_closed();
	}
	if (!VALID_INDEX(goal_surface, ae_travel_ways.count))
		return NULL;
	for (index = 0; index < AE_TRAVEL_WAYS; index++)
	{
		way = &ae_travel_ways.ways[index];
		if (way->goal_surface == goal_surface)
		{
			way->used_tick = now;
			return way;
		}
		/* (the slot for a new one: an empty one, else the one used longest ago) */
		if (ae_travel_ways.ways[oldest].goal_surface != NONE &&
			(way->goal_surface == NONE || way->used_tick < ae_travel_ways.ways[oldest].used_tick))
		{
			oldest = index;
		}
	}
	if (ae_travel_ways.tick != now)
	{
		ae_travel_ways.tick = now;
		ae_travel_ways.made_this_tick = 0;
	}
	if (ae_travel_ways.made_this_tick >= AE_TRAVEL_WAYS_A_TICK)
		return NULL;
	ae_travel_ways.made_this_tick++;
	way = &ae_travel_ways.ways[oldest];
	if (!ae_travel_way_make(way, goal_surface, goal_point))
		return NULL;
	way->used_tick = now;
	return way;
}

/* (bots.c's bots_search, after the search) a search that ended short of its goal, no nearer it by the walk (path.c
gave up on the way, too long against the straight line, or more surfaces than it holds), while a walk there is: the
search again, to a via on the walk */
void ae_bots_travel_long_way(struct bots_bot *bot, struct path_state *state, real_point3d const *start, long start_surface,
	boolean reached)
{
	struct ae_travel_way *way;
	struct path_input input;
	struct structure_bsp *structure;
	long surface, end_surface, goal_piece, count = 0, surfaces[AE_TRAVEL_WAY_STEPS];
	real walked[AE_TRAVEL_WAY_STEPS], length = 0.0f;
	real_point3d previous = *start;
	int via;

	ae_travel_ways.via_searched = FALSE;
	if (reached || bot->goal_kind == _bots_goal_item || bot->goal_surface_index == NONE || state->node_count <= 0 ||
		state->closest_node_index < 0 || state->closest_node_index >= state->node_count)
	{
		return;
	}
	/* (the goal on the bot's own piece of the walkable surfaces: a walk there is) */
	goal_piece = ae_travel_piece(bot->goal_surface_index);
	if (goal_piece == NONE || goal_piece != ae_travel_piece(start_surface))
		return;
	way = ae_travel_way(bot->goal_surface_index, &bot->goal_point);
	end_surface = state->node_list[state->closest_node_index].surface_index;
	if (!way || !VALID_INDEX(start_surface, ae_travel_ways.count) || !VALID_INDEX(end_surface, ae_travel_ways.count) ||
		ae_bots_travel_partial_gains(way->walk[start_surface], way->walk[end_surface]))
	{
		return;
	}
	/* (along the walk from the bot's surface: each edge it crosses, the walk to it) */
	for (surface = start_surface; count < AE_TRAVEL_WAY_STEPS && VALID_INDEX(surface, ae_travel_ways.count) &&
		way->next[surface] != NONE; surface = way->next[surface])
	{
		length += bots_distance3d(&previous, &way->exit[surface]);
		previous = way->exit[surface];
		walked[count] = length;
		surfaces[count++] = surface;
	}
	via = ae_bots_travel_way_via(walked, (int)count, AE_BOTS_TRAVEL_VIA_REACH);
	if (via < 0)
		return;
	/* (the search again, from the same place, to the via: the next surface's edge) */
	input = state->input;
	structure = state->structure;
	path_state_new(&input, state, NULL);
	state->structure = structure;
	path_state_destination(state, &way->exit[surfaces[via]], way->next[surfaces[via]], 1000.0f);
	path_state_find(state);
	bots_globals.tick_searches++;
	bots_globals.searches++;
	bot->searches++;
	ae_travel_ways.via_searched = TRUE;
	ae_travel_ways.via_walk = way->walk[start_surface];
	ae_travel_ways.via_bot = bot;
	ae_travel_ways.via_tick = game_time_get();
	ae_travel.long_ways++;
}

static boolean ae_travel_long_walk(struct bots_bot const *bot, real *walk)
{
	if (ae_travel_ways.via_bot != bot || ae_travel_ways.via_tick != game_time_get() || ae_travel_ways.via_walk < 0.0f)
		return FALSE;
	*walk = ae_travel_ways.via_walk;
	return TRUE;
}

/* (bots.c's bots_search, with the path made) a path to a via ends there: the bot searches again at its end */
void ae_bots_travel_long_way_path(struct bots_bot *bot)
{
	if (!ae_travel_ways.via_searched)
		return;
	ae_travel_ways.via_searched = FALSE;
	bot->path.steps_finish_path = FALSE;
	bot->climb_until_tick = 0;
}
