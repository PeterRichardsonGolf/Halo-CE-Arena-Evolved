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
#include "ae_bots_lanes.h"
#include "ae_bots_vantage.h"
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
/* a jump's last steps are driven (ae_travel_jump_drive) from this near its entry: a straight run along its line */
#define AE_TRAVEL_JUMP_RUN_UP 2.5f
/* the jump is made at its entry (this far past it at most) with this speed at least (a run is 0.075 a tick) */
#define AE_TRAVEL_JUMP_SPEED_MIN 0.05f
#define AE_TRAVEL_JUMP_LINE 0.5f
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
	/* TRAPPED: when and where it first stuck off the paths (0: not) */
	long trap_tick;
	real_point3d trap_point;
	/* the item its climbs ended on over a drop, how often, the last when (ae_bots_travel_climb_blocked) */
	long climb_drop_item;
	short climb_drop_count;
	long climb_drop_tick;
	/* how near its goal it has come, and when it last came nearer (a bot pacing back and forth) */
	long progress_goal_tick;
	real progress_best;
	long progress_tick;
	/* link jumps that failed since the last made one (the bot gives its goal up at 3), and the top of the jump in the air */
	short jump_fails;
	real air_top;
	/* the pieces its piece reaches by the links, as of its start piece and the links known then (GOALS NO ROUTE REACHES) */
	long reach_start;
	long reach_links;
	int reach_count;
	long reach[AE_BOTS_TRAVEL_REACH_PIECES];
	/* the goal pieces banned (stuck on a goal no route reaches), and the roaming place it holds to until then */
	struct ae_bots_travel_bans bans;
	long hold_until;
	real_point3d hold_point;
	/* TELEPORTERS IN A FIGHT (ae_bots_combat_rules.h): the pad it heads for to evade or bait (until when), and once
	through, where it waits off the exit facing the pad (until when) */
	short pad_link;
	long pad_until;
	long ambush_until;
	real_point3d ambush_point;
	long ambush_surface;
	real ambush_yaw;
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
	/* (0.9.0) links left as short cuts while fighting */
	long links_fight_skipped;
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
	/* goals cut off from the bot's piece, and those swapped for a place its piece reaches */
	long goals_banned;
	long goals_parked;
	long goals_cut;
	long goals_swapped;
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
static void ae_travel_pad_arrived(struct bots_bot *bot, struct ae_travel_bot *travel, struct bots_tick *tick, long index);
static void ae_travel_ban_stuck_goal(struct bots_bot *bot, long now);
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

/* how far above or below its entry a bot may be and count as there: a jump's entry is the edge it jumps from, so a bot
on the floor beside the platform, a rise under it, is not (Wizard 11-08: they jumped at the platform's foot); a pad's or
a walk link's is looser */
#define AE_TRAVEL_JUMP_ENTRY_RISE 0.7f
static real ae_travel_entry_rise(struct ae_bots_travel_link const *link)
{
	return link->kind == AE_BOTS_TRAVEL_LINK_JUMP ? AE_TRAVEL_JUMP_ENTRY_RISE : 1.5f;
}

/* how near a link's entry its last steps are walked straight */
static real ae_travel_approach(struct ae_bots_travel_link const *link)
{
	return link->kind == AE_BOTS_TRAVEL_LINK_TELEPORTER ? AE_TRAVEL_PAD_APPROACH :
		link->kind == AE_BOTS_TRAVEL_LINK_JUMP ? AE_TRAVEL_JUMP_RUN_UP : AE_TRAVEL_JUMP_APPROACH;
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
static boolean ae_travel_path_reaches(struct bots_bot const *bot, real_point3d const *point, real near, real rise)
{
	if (!bot->path.valid)
		return FALSE;
	if (!bot->path_short)
		return TRUE;
	return (boolean)(bot->path.steps_finish_path && bots_distance2d(&bot->path.endpoint.point, point) <= near &&
		fabs(bot->path.endpoint.point.z - point->z) <= rise);
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

			/* (a jump link's via is held 6 s, and while the goal is on the piece it lands on: KOTH's fight and hill jobs
			swap the goal every second or two, Wizard 11-08, and no bot got to the edge) */
			boolean held = (boolean)(link->kind == AE_BOTS_TRAVEL_LINK_JUMP && (now - travel->via_tick < TICKS_PER_SECOND * 6 ||
				ae_travel_piece(bot->goal_surface_index) == ae_travel_link_piece(travel->via, TRUE)));

			if (estimate >= straight * AE_BOTS_TRAVEL_WALK_STRETCH && !held)
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

static void ae_bots_travel_repick(struct bots_bot *bot, real_point3d const *position);

/* a link's jump that failed: the third in a row gives the bot's goal up (a goal it cannot reach by jumping is not stood
under for good), as a goal stuck on is (ae_bots_travel_repick) */
static void ae_travel_jump_failed(struct bots_bot *bot, struct ae_travel_bot *travel, real_point3d const *position)
{
	if (++travel->jump_fails >= 3)
	{
		travel->jump_fails = 0;
		platform_log("bots: travel: bot %ld gave up goal (%.1f %.1f %.1f) after 3 failed jumps", (long)(bot - bots_globals.bots),
			bot->goal_point.x, bot->goal_point.y, bot->goal_point.z);
		bot->goal_kind = _bots_goal_none;
		ae_bots_travel_repick(bot, position);
	}
}

/* a link looked at, after the search to the goal found this walk: the best worth a try is searched to at once, and
taken if the walk to it still beats the walk */
static void ae_travel_consider_links(struct bots_bot *bot, struct ae_travel_bot *travel, long unit_index,
	real_point3d const *position, real walk)
{
	long now = game_time_get(), index, best = NONE, fight_skipped = NONE;
	real best_estimate = REAL_MAX, straight = bots_distance3d(position, &bot->goal_point), skipped_estimate = REAL_MAX;
	long start_piece = NONE, goal_piece = ae_travel_piece(bot->goal_surface_index);
	boolean routed = FALSE;
	boolean fighting = (boolean)(bot->target_player_index != NONE && now >= bot->target_seen_tick &&
		now - bot->target_seen_tick < AE_BOTS_TRAVEL_FIGHT_SEEN_TICKS);

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
			/* (NO LINKS MID-FIGHT: a short cut that saves too little is left while the bot fights) */
			if (!ae_bots_travel_link_fight_allows(fighting, estimate, walk))
			{
				if (estimate < skipped_estimate)
				{
					skipped_estimate = estimate;
					fight_skipped = index;
				}
				continue;
			}
			best_estimate = estimate;
			best = index;
		}
	}
	if (best == NONE && fight_skipped != NONE)
	{
		/* (not looked at again for 3 s: the fight goes on where it is) */
		travel->declined_until[fight_skipped] = now + TICKS_PER_SECOND * 3;
		ae_travel.links_fight_skipped++;
		platform_log("bots: ev=link_fight_skip t=%ld bot=%ld link=%ld walk=%.1f estimate=%.1f", now,
			(long)(bot - bots_globals.bots) + 1, fight_skipped, (double)walk, (double)skipped_estimate);
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
			ae_travel_path_reaches(bot, &ae_travel.entry_ground[best], ae_travel_approach(link), ae_travel_entry_rise(link)) && (routed ||
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
			!ae_travel_path_reaches(bot, &travel->pack_point, AE_TRAVEL_PACK_APPROACH, 1.5f))
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
			!ae_travel_path_reaches(bot, &ae_travel.entry_ground[travel->via], ae_travel_approach(&ae_travel.links[travel->via]),
				ae_travel_entry_rise(&ae_travel.links[travel->via])))
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

/* a climb ended over a drop on its way to an item: the same item again and again (re-picked each second) is given up a
while, as one the bot did not get */
static void ae_travel_climb_drop_item(struct bots_bot *bot)
{
	struct ae_travel_bot *travel = ae_travel_state(bot);
	long now = game_time_get();

	if (!travel || bot->goal_kind != _bots_goal_item || bot->item_index == NONE)
		return;
	if (travel->climb_drop_item != bot->item_index || now - travel->climb_drop_tick > TICKS_PER_SECOND * 10)
	{
		travel->climb_drop_item = bot->item_index;
		travel->climb_drop_count = 0;
	}
	travel->climb_drop_tick = now;
	if (++travel->climb_drop_count >= AE_BOTS_TRAVEL_CLIMB_DROP_GIVE_UP)
	{
		bots_item_set_unreachable(bot, bot->item_index);
		travel->climb_drop_count = 0;
	}
}

boolean ae_bots_travel_climb_blocked(struct bots_bot *bot, real_point3d const *position)
{
	real_point3d const *goal = &bot->goal_point;
	real across = (real)sqrt((goal->x - position->x) * (goal->x - position->x) + (goal->y - position->y) * (goal->y - position->y));
	static const real ahead[2] = { 1.0f, 2.5f };
	long which;
	boolean hop;

	if (across < 0.5f)
		return FALSE;
	/* (a hop: jumped over the drop, crouched to the landing by ae_bots_crouch_jump_tick) */
	hop = ae_bots_travel_climb_hop(across, goal->z - position->z);
	for (which = 0; which < 2; which++)
	{
		real_point3d point = *position, ground;
		real reach = MIN(ahead[which], across);

		point.x += (goal->x - position->x) / across * reach;
		point.y += (goal->y - position->y) / across * reach;
		if (ae_bots_travel_climb_drops(bots_surface_below(&point, &ground) != NONE, position->z, ground.z, goal->z))
		{
			if (hop)
			{
				if (which == 0)
					ae_travel_press_jump(bot);
				return FALSE;
			}
			/* (counted in halo.log: a climb that would have gone on over the drop) */
			ae_bots_event_line((long)(bot - bots_globals.bots), game_time_get(), "climb_drop", -1, position->z, "");
			ae_travel_climb_drop_item(bot);
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
			if (travel->direct && (distance > AE_TRAVEL_JUMP_APPROACH || fabs(position->z - point->z) > ae_travel_entry_rise(link)))
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
			if (distance > ae_travel_approach(link) || fabs(position->z - point->z) > ae_travel_entry_rise(link))
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
			/* (a jump: the run along its line and the button are ae_travel_jump_drive's; here the way it is given up) */
			if (distance > AE_TRAVEL_JUMP_RUN_UP * 1.5f || now - travel->approach_tick > TICKS_PER_SECOND * 4)
			{
				ae_travel.jumps_missed++;
				ae_travel_jump_failed(bot, travel, position);
				ae_travel_drop_via(bot, travel, TICKS_PER_SECOND * 30, "jump not started");
				return FALSE;
			}
			*direction = (real)atan2(link->exit.y - position->y, link->exit.x - position->x);
			return TRUE;
		}
		{
			/* (in the air: on towards the landing; landed near it, the jump is made) */
			struct biped_datum *biped = biped_try_and_get(unit_index);
			boolean airborne = (boolean)(biped && TEST_FLAG(biped->biped.flags, _biped_airborne_bit));
			real landing = bots_distance2d(position, &link->exit);

			travel->air_top = MAX(travel->air_top, position->z);
			if (!airborne && now - travel->jump_tick > TICKS_PER_SECOND / 4)
			{
				if (ae_bots_trace_level() > 0)
				{
					platform_log("bots: travel trace jump end link %d at (%.2f %.2f %.2f) top %.2f, %.2f from the landing, %ld ticks",
						(int)travel->via, position->x, position->y, position->z, travel->air_top, landing, now - travel->jump_tick);
				}
				/* (made: near the landing, or down at its height when it is a drop) */
				if ((landing < AE_TRAVEL_LANDED && fabs(position->z - link->exit.z) < AE_TRAVEL_LANDED) ||
					(link->exit.z < link->entry.z - 1.0f && fabs(position->z - link->exit.z) < 1.0f &&
						landing < AE_TRAVEL_LANDED * 3.0f))
				{
					travel->arrived = TRUE;
					travel->arrival = *position;
					travel->arrival_tick = now;
					travel->arrival_link = travel->via;
					travel->jump_fails = 0;
					ae_travel_drop_via(bot, travel, 0, "jumped");
				}
				else
				{
					ae_travel.jumps_missed++;
					ae_travel_jump_failed(bot, travel, position);
					ae_travel_drop_via(bot, travel, TICKS_PER_SECOND * 30, "jump landed off");
				}
				return FALSE;
			}
			if (now - travel->jump_tick > TICKS_PER_SECOND * 3)
			{
				ae_travel.jumps_missed++;
				ae_travel_jump_failed(bot, travel, position);
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
	ae_travel_ban_stuck_goal(bot, now);
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
		/* (a pack hung in the air: shot down first, ae_bots_power_shoot) */
		if (ae_bots_travel_pack_there(ae_travel.pack_objects[index]) && ae_bots_item_hung(ae_travel.pack_objects[index]))
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
		"near goals %ld (%.1f s each), goals left %ld, no progress %ld, long ways %ld, goals banned %ld, swapped %ld, parked %ld of %ld; stuck spots this minute:%s", now, ae_travel.teleports,
		ae_travel.teleports_planned, ae_travel.pads_missed, ae_travel.jumps, ae_travel.jumps_missed, ae_travel.walks,
		ae_travel.walks_missed, ae_travel.links_tried, ae_travel.links_taken, ae_travel.links_declined, ae_travel.routes,
		ae_travel.packs_taken,
		ae_travel.pack_detours, ae_travel.far_goals,
		ae_travel.far_goals ? (double)ae_travel.far_goal_ticks / ae_travel.far_goals / TICKS_PER_SECOND : 0.0,
		ae_travel.near_goals,
		ae_travel.near_goals ? (double)ae_travel.near_goal_ticks / ae_travel.near_goals / TICKS_PER_SECOND : 0.0,
		ae_travel.goals_left, ae_travel.no_progress, ae_travel.long_ways, ae_travel.goals_banned, ae_travel.goals_swapped, ae_travel.goals_parked, ae_travel.goals_cut, spots[0] ? spots : " none");
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
	/* (the map's side lanes and vantage spots, a bounded step a tick: ae_bots_lanes.c) */
	ae_bots_lanes_update();
	ae_bots_vantage_update();
	ae_bots_vantage_tick(bot, bot_index, tick);
	/* (the countdown holds every player where they stand: the stuck check counts from its end, not from a spot taken
	inside it, else every bot is "stuck" at its spawn on the first tick it can walk, each round: Weld 10-29, 12 of 41) */
	if (game_engine_pregame_countdown_ticks_left() > 0)
	{
		bot->stuck_position = tick->position;
		bot->stuck_tick = tick->now;
	}
	if (!ae_travel.ready)
		return;
	/* (debug, HALO_BOT_PROBE="x y z radius": once, the level's ground heights on a 0.25 grid around a point, from z down:
	the shape of a step or a pedestal the bots can't get onto) */
	{
		static boolean probed;
		char const *probe = getenv("HALO_BOT_PROBE");
		float px, py, pz, pr;

		if (!probed && probe && sscanf(probe, "%f %f %f %f", &px, &py, &pz, &pr) == 4)
		{
			real x, y;

			probed = TRUE;
			for (y = py + pr; y >= py - pr - 0.01f; y -= 0.25f)
			{
				char row[512];
				int used = snprintf(row, sizeof(row), "bots: probe y=%5.2f:", y);

				for (x = px - pr; x <= px + pr + 0.01f && used < (int)sizeof(row) - 8; x += 0.25f)
				{
					real_point3d at, ground;

					at.x = x;
					at.y = y;
					at.z = pz;
					ground = at;
					used += snprintf(row + used, sizeof(row) - used, bots_surface_below(&at, &ground) != NONE ?
						" %5.2f" : "   -  ", ground.z);
				}
				platform_log("%s", row);
			}
		}
	}
	/* (debug, HALO_BOT_TELEPORT="bot tick x y z": that bot (1 on) put there at that tick, to stand it in a place the bots
	get stuck at; the same run again) */
	{
		char const *teleport = getenv("HALO_BOT_TELEPORT");
		long wanted_bot, wanted_tick;
		float tx, ty, tz;

		if (teleport && sscanf(teleport, "%ld %ld %f %f %f", &wanted_bot, &wanted_tick, &tx, &ty, &tz) == 5 &&
			tick->now == wanted_tick && (long)(bot - bots_globals.bots) + 1 == wanted_bot)
		{
			real_point3d point;

			point.x = tx;
			point.y = ty;
			point.z = tz;
			object_set_position(tick->player->unit_index, &point, NULL, NULL);
			platform_log("bots: teleport: bot %ld to (%.1f %.1f %.1f) at tick %ld", wanted_bot, tx, ty, tz, wanted_tick);
		}
	}
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
			ae_travel_pad_arrived(bot, travel, tick, index);
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


/* GOALS NO ROUTE REACHES (ae_bots_travel_rules.h): a goal on a piece the bot's own piece reaches by no route is swapped for
a roaming place its piece does reach. The bot's piece and its reach set, remade when it changes piece or a link is found */
static boolean ae_travel_bot_reach(struct bots_bot *bot, struct ae_travel_bot *travel, long *start_piece)
{
	real_point3d ground;
	struct object_datum *object = (struct object_datum *)object_try_and_get_and_verify_type(bot->unit_index, _object_mask_unit);
	long surface, index;

	/* (not in a vehicle: it drives where no piece goes) */
	if (!object || object->object.parent_object_index != NONE || !ae_bots_travel_links_on())
		return FALSE;
	surface = bots_unit_surface(bot->unit_index, &ground);
	*start_piece = surface != NONE ? ae_travel_piece(surface) : NONE;
	if (*start_piece == NONE)
		return FALSE;
	if (travel->reach_start != *start_piece || travel->reach_links != ae_travel.link_count || travel->reach_count <= 0)
	{
		long from[AE_BOTS_TRAVEL_MAXIMUM_LINKS], to[AE_BOTS_TRAVEL_MAXIMUM_LINKS];

		for (index = 0; index < ae_travel.link_count; index++)
		{
			from[index] = ae_travel_link_piece(index, FALSE);
			to[index] = ae_travel_link_piece(index, TRUE);
		}
		travel->reach_count = ae_bots_travel_reach_set((int)ae_travel.link_count, from, to, *start_piece, travel->reach,
			AE_BOTS_TRAVEL_REACH_PIECES);
		travel->reach_start = *start_piece;
		travel->reach_links = ae_travel.link_count;
	}
	return TRUE;
}

/* whether a goal on this surface is cut off from the bot's piece */
static boolean ae_travel_goal_cut_off(struct ae_travel_bot const *travel, long start_piece, long goal_surface)
{
	long goal_piece = goal_surface != NONE ? ae_travel_piece(goal_surface) : NONE;

	return ae_bots_travel_goal_cut_off(start_piece, goal_piece, travel->reach, travel->reach_count);
}

/* a stuck report on a goal cut off from the bot's piece: the goal's piece is banned for the bot (called by the stuck count) */
static void ae_travel_ban_stuck_goal(struct bots_bot *bot, long now)
{
	struct ae_travel_bot *travel = ae_travel_state(bot);
	long start_piece = NONE, goal_piece;

	if (!travel || !ae_travel.ready || bot->goal_kind == _bots_goal_none || bot->goal_surface_index == NONE ||
		!ae_travel_bot_reach(bot, travel, &start_piece) || !ae_travel_goal_cut_off(travel, start_piece, bot->goal_surface_index))
	{
		return;
	}
	goal_piece = ae_travel_piece(bot->goal_surface_index);
	if (!ae_bots_travel_banned(&travel->bans, start_piece, goal_piece, now))
	{
		ae_travel.goals_banned++;
		if (ae_travel.goals_banned <= 40)
		{
			platform_log("bots: travel: bot %ld stuck on goal (%.1f %.1f %.1f): piece %ld has no route from piece %ld, banned for a while",
				(long)(bot - bots_globals.bots), bot->goal_point.x, bot->goal_point.y, bot->goal_point.z, goal_piece, start_piece);
		}
	}
	ae_bots_travel_ban_add(&travel->bans, start_piece, goal_piece, now);
}

/* the bot's goals chosen (bots.c's bots_bot_update, instead of bots_choose_goal): not while it holds to a roaming place it
picked for a goal on a banned piece; then the goal chosen, swapped if its piece is banned */
/* TELEPORTERS IN A FIGHT (ae_bots_combat_rules.h): the best pad for a bot backing off from (or baiting) the enemy at
target, as its way to the pad's entry and the link; NONE for none */
static long ae_travel_fight_pad(struct bots_bot *bot, struct bots_tick *tick, real_point3d const *target, boolean bait,
	real *to_entry)
{
	long index, best = NONE;
	real best_score = 0.0f;
	real tx = target->x - tick->position.x, ty = target->y - tick->position.y;
	real to_enemy = (real)sqrt(tx * tx + ty * ty);

	if (!ae_travel.ready)
		return NONE;
	for (index = 0; index < ae_travel.link_count; index++)
	{
		struct ae_bots_travel_link const *link = &ae_travel.links[index];
		real_point3d const *entry = &ae_travel.entry_ground[index];
		real ex = entry->x - tick->position.x, ey = entry->y - tick->position.y;
		real distance = (real)sqrt(ex * ex + ey * ey);
		real towards = distance > 0.01f && to_enemy > 0.01f ? (ex * tx + ey * ty) / (distance * to_enemy) : 0.0f;
		real score;

		if (link->kind != AE_BOTS_TRAVEL_LINK_TELEPORTER || ae_travel.entry_surface[index] == NONE ||
			(real)fabs(entry->z - tick->position.z) > 2.5f)
		{
			continue;
		}
		score = ae_bots_combat_pad_score((float)distance, (float)towards, (float)bots_distance2d(&link->exit, target),
			(float)to_enemy, bait);
		if (score > best_score)
		{
			best_score = score;
			best = index;
			*to_entry = distance;
		}
	}
	return best;
}

/* (ae_bots_combat.inc, a bot backing off or baiting) the goal through the best pad; FALSE for none */
static boolean ae_bots_travel_fight_pad_goal(struct bots_bot *bot, struct bots_tick *tick, real_point3d const *target,
	boolean bait)
{
	struct ae_travel_bot *travel = ae_travel_state(bot);
	real to_entry = 0.0f;
	long link = travel ? ae_travel_fight_pad(bot, tick, target, bait, &to_entry) : NONE;

	if (link == NONE)
		return FALSE;
	travel->pad_link = (short)link;
	travel->pad_until = tick->now + 6 * TICKS_PER_SECOND;
	bot->hunting = FALSE;
	bots_set_goal(bot, _bots_goal_roam, &ae_travel.entry_ground[link], ae_travel.entry_surface[link]);
	if (ae_bots_events_on())
		ae_bots_event_line((long)(bot - bots_globals.bots), tick->now, bait ? "pad_bait" : "pad_evade", -1, to_entry, "");
	return TRUE;
}

/* (ae_bots_combat.inc) the way to the nearest pad a bot could bait through; a large number for none */
static real ae_bots_travel_bait_pad_distance(struct bots_bot *bot, struct bots_tick *tick, real_point3d const *target)
{
	real to_entry = REAL_MAX;

	return ae_travel_fight_pad(bot, tick, target, TRUE, &to_entry) != NONE ? to_entry : REAL_MAX;
}

/* come out at a pad's exit: one it meant to take to evade or bait waits off the exit, facing the pad, the level's
seconds (HEROIC and up): the roaming place 2.5 to 9 units from the exit nearest it, on its floor */
static void ae_travel_pad_arrived(struct bots_bot *bot, struct ae_travel_bot *travel, struct bots_tick *tick, long index)
{
	real seconds = ae_bots_combat_ambush_seconds(ae_bots_level_of_skill(ae_bots_movement_skill(bots_globals.skill)));
	real_point3d const *exit = &ae_travel.links[index].exit;
	long point, best = NONE;
	real best_distance = REAL_MAX;

	if (tick->now > travel->pad_until || travel->pad_until == 0)
		return;
	travel->pad_until = 0;
	if (seconds <= 0.0f)
		return;
	for (point = 0; point < bots_globals.roam_point_count; point++)
	{
		real distance = bots_distance2d(&bots_globals.roam_points[point], exit);

		if (distance >= 2.5f && distance <= 9.0f && bots_globals.roam_surfaces[point] != NONE &&
			(real)fabs(bots_globals.roam_points[point].z - exit->z) < 1.5f && distance < best_distance)
		{
			best = point;
			best_distance = distance;
		}
	}
	if (best == NONE)
		return;
	travel->ambush_point = bots_globals.roam_points[best];
	travel->ambush_surface = bots_globals.roam_surfaces[best];
	travel->ambush_yaw = (real)atan2(exit->y - travel->ambush_point.y, exit->x - travel->ambush_point.x);
	travel->ambush_until = tick->now + (long)(seconds * TICKS_PER_SECOND);
	bots_set_goal(bot, _bots_goal_roam, &travel->ambush_point, travel->ambush_surface);
	if (ae_bots_events_on())
		ae_bots_event_line((long)(bot - bots_globals.bots), tick->now, "pad_ambush", -1, best_distance, "");
}

/* (ae_bots_combat.inc) whether the bot is on its way to a pad it chose to evade or bait through */
static boolean ae_bots_travel_pad_going(struct bots_bot *bot, long now)
{
	struct ae_travel_bot const *travel = ae_travel_state(bot);

	return travel && travel->pad_until != 0 && now < travel->pad_until;
}

/* (ae_bots_look_about) the way a bot waiting off a pad's exit watches: the pad */
static boolean ae_bots_travel_ambush_yaw(long bot_index, real *yaw)
{
	struct ae_travel_bot const *travel = bot_index >= 0 && bot_index < BOTS_BRAINS ? &ae_travel_bots[bot_index] : NULL;

	if (!travel || game_time_get() >= travel->ambush_until)
		return FALSE;
	*yaw = travel->ambush_yaw;
	return TRUE;
}

static void ae_bots_travel_choose_goal(struct bots_bot *bot, real_point3d const *position, boolean fighting)
{
	struct ae_travel_bot *travel = ae_travel_state(bot);
	long now = game_time_get(), start_piece = NONE, tries, first, pass;

	/* (waiting off a pad's exit for a chaser: its spot, until the wait is over; a fight takes over as always) */
	if (travel && !fighting && now < travel->ambush_until)
	{
		bots_set_goal(bot, _bots_goal_roam, &travel->ambush_point, travel->ambush_surface);
		return;
	}
	if (travel && now < travel->hold_until && bot->goal_kind == _bots_goal_roam &&
		bots_distance3d(&bot->goal_point, &travel->hold_point) < 0.5f && bots_distance2d(&bot->goal_point, position) >= 1.5f)
	{
		return;
	}
	bots_choose_goal(bot, bot->player_index, position, fighting);
	if (!travel || !ae_travel.ready || bot->goal_kind == _bots_goal_none || bot->goal_surface_index == NONE ||
		bots_globals.roam_point_count <= 0 || !ae_travel_bot_reach(bot, travel, &start_piece) ||
		!ae_bots_travel_banned(&travel->bans, start_piece, ae_travel_piece(bot->goal_surface_index), now))
	{
		return;
	}
	ae_travel.goals_cut++;
	first = (long)(bots_random(bot) % (unsigned long)bots_globals.roam_point_count);
	/* (a place 6 units off or more first; failing that any 2 off: a bot on a platform with no way off, Boarding Action's, goes
	about on it) */
	for (pass = 0; pass < 2; pass++)
	{
		for (tries = 0; tries < bots_globals.roam_point_count; tries++)
		{
			long index = (first + tries) % bots_globals.roam_point_count;
			long piece = bots_globals.roam_surfaces[index] != NONE ? ae_travel_piece(bots_globals.roam_surfaces[index]) : NONE;

			if (bots_distance2d(&bots_globals.roam_points[index], position) < (pass ? 2.0f : 6.0f) || piece == NONE ||
				ae_bots_travel_banned(&travel->bans, start_piece, piece, now) ||
				ae_travel_goal_cut_off(travel, start_piece, bots_globals.roam_surfaces[index]))
			{
				continue;
			}
			bot->roam_index = (short)index;
			bot->hunting = FALSE;
			bots_set_goal(bot, _bots_goal_roam, &bots_globals.roam_points[index], bots_globals.roam_surfaces[index]);
			travel->hold_point = bot->goal_point;
			travel->hold_until = now + AE_BOTS_TRAVEL_GOAL_HOLD_TICKS;
			ae_travel.goals_swapped++;
			return;
		}
	}
	/* (none at all: it stays where it is, not walking at a goal it cannot reach) */
	{
		real_point3d ground;
		long surface = bots_unit_surface(bot->unit_index, &ground);

		if (surface != NONE)
		{
			bots_set_goal(bot, _bots_goal_roam, &ground, surface);
			ae_travel.goals_parked++;
		}
	}
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


/* JUMP LINKS FOUND (ae_bots_travel_rules.h): the corners of the bigger pieces' surfaces, a little in from the edge */
#define AE_TRAVEL_JUMP_CORNERS 8192
/* the nearest pairs of corners tried for a piece, and the jumps added to it at most, this far apart at the entry */
#define AE_TRAVEL_JUMP_TRIES 16
#define AE_TRAVEL_JUMPS_A_PIECE 4

static struct
{
	long count;
	real_point3d raw[AE_TRAVEL_JUMP_CORNERS];
	real_point3d point[AE_TRAVEL_JUMP_CORNERS];
	/* the ground well in from the edge, where a jump lands (the same piece's) */
	real_point3d landing[AE_TRAVEL_JUMP_CORNERS];
	long surface[AE_TRAVEL_JUMP_CORNERS];
	long piece[AE_TRAVEL_JUMP_CORNERS];
} ae_travel_jump_corners;

/* whether a biped jumping from a to b (ground points) has the air to do it: nothing across at the knees' reach over
the higher end's chest and head, room above both ends */
static boolean ae_travel_jump_clear(real_point3d const *a, real_point3d const *b)
{
	unsigned long flags = FLAG(_collision_test_structure_bit) | FLAG(_collision_test_front_facing_surfaces_bit) |
		FLAG(_collision_test_objects_bit) | FLAG(_collision_test_objects_scenery_bit) |
		FLAG(_collision_test_objects_machines_bit);
	static real const heights[] = { 0.6f, 1.0f };
	real top = MAX(a->z, b->z);
	long index, end;

	for (index = 0; index < 2; index++)
	{
		struct collision_result collision;
		real_point3d start;
		real_vector3d vector;

		start.x = a->x;
		start.y = a->y;
		start.z = top + heights[index];
		vector.i = b->x - a->x;
		vector.j = b->y - a->y;
		vector.k = 0.0f;
		if (collision_test_vector(flags, &start, &vector, NONE, &collision))
			return FALSE;
	}
	for (end = 0; end < 2; end++)
	{
		struct collision_result collision;
		real_point3d start = end ? *b : *a;
		real_vector3d vector;

		start.z += 0.05f;
		vector.i = vector.j = 0.0f;
		vector.k = 1.2f;
		if (collision_test_vector(flags | FLAG(_collision_test_back_facing_surfaces_bit), &start, &vector, NONE, &collision))
			return FALSE;
	}
	return TRUE;
}

static void ae_travel_jump_add(long from_corner, long to_corner)
{
	long index = ae_travel.link_count;
	struct ae_bots_travel_link *link = &ae_travel.links[index];

	link->kind = AE_BOTS_TRAVEL_LINK_JUMP;
	link->source = ae_travel_jump_corners.piece[from_corner];
	link->target = ae_travel_jump_corners.piece[to_corner];
	link->entry = ae_travel_jump_corners.point[from_corner];
	link->exit = ae_travel_jump_corners.landing[to_corner];
	ae_travel.entry_ground[index] = ae_travel_jump_corners.point[from_corner];
	ae_travel.entry_surface[index] = ae_travel_jump_corners.surface[from_corner];
	ae_travel.link_count++;
}

/* the pieces from which the links lead to the main piece (a link goes from its entry's piece to its exit's), by any number
of links: a bot on another piece has no way off its own */
static void ae_travel_escape_pieces(unsigned char *escapes, long main_piece)
{
	boolean changed = TRUE;

	escapes[main_piece] = TRUE;
	while (changed)
	{
		long index;

		changed = FALSE;
		for (index = 0; index < ae_travel.link_count; index++)
		{
			long from = ae_travel_link_piece(index, FALSE), to = ae_travel_link_piece(index, TRUE);

			if (from != NONE && to != NONE && escapes[to] && !escapes[from])
			{
				escapes[from] = TRUE;
				changed = TRUE;
			}
		}
	}
}

/* DROP LINKS (ae_bots_travel_rules.h): a piece no link leaves gets a jump off its edge onto the ground of a piece that does
(Digsite's Weld: a spawn on a platform 2.8 above the floor, whose bot stood at the ledge all game). The corners of the
piece's surfaces (the jump scan's) are the edges; the way out from a corner is away from where its surface's ground goes in.
A landing is probed where the fall carries a running bot, the drop measured there and again at the landing's own place. */
#define AE_TRAVEL_DROPS_A_PIECE 3
static long ae_travel_drops_scan(unsigned char *escapes, long main_piece)
{
	long round, added = 0;

	for (round = 0; round < 6; round++)
	{
		long corner, joined = 0;
		unsigned char *made;

		csmemset(escapes, 0, (size_t)MAX(ae_travel_pieces.count, 1));
		ae_travel_escape_pieces(escapes, main_piece);
		/* (a piece made an escape this round is a landing from the next round on, not within it) */
		made = (unsigned char *)malloc((size_t)MAX(ae_travel_pieces.count, 1));
		if (!made)
			return added;
		csmemset(made, 0, (size_t)MAX(ae_travel_pieces.count, 1));
		for (corner = 0; corner < ae_travel_jump_corners.count; corner++)
		{
			long piece = ae_travel_jump_corners.piece[corner], landing_surface, landing_piece, first;
			real_point3d const *raw = &ae_travel_jump_corners.raw[corner], *point = &ae_travel_jump_corners.point[corner];
			real_point3d probe, ground, first_ground = { 0.0f, 0.0f, 0.0f };
			real away = bots_distance2d(raw, point), drop, out;
			long attempt;

			if (escapes[piece] || made[piece] >= AE_TRAVEL_DROPS_A_PIECE || ae_travel.link_count + 1 > AE_BOTS_TRAVEL_MAXIMUM_LINKS)
				continue;
			if (away < 0.01f)
				continue;
			/* (outward: from the pulled in point to the edge's corner, on) */
			first = NONE;
			for (attempt = 0; attempt < 2; attempt++)
			{
				out = attempt ? ae_bots_travel_drop_out(raw->z - first_ground.z) : AE_BOTS_TRAVEL_DROP_OUT_BASE;
				probe.x = raw->x + (raw->x - point->x) / away * out;
				probe.y = raw->y + (raw->y - point->y) / away * out;
				probe.z = raw->z;
				landing_surface = bots_surface_below(&probe, &ground);
				if (landing_surface == NONE)
					break;
				drop = raw->z - ground.z;
				if (!ae_bots_travel_drop_ends(drop))
				{
					landing_surface = NONE;
					break;
				}
				if (!attempt)
				{
					first = landing_surface;
					first_ground = ground;
					continue;
				}
				/* (the landing's own place is lower or higher by a step at most than the first look found) */
				if (fabs(ground.z - first_ground.z) > 0.5f)
					landing_surface = NONE;
			}
			if (landing_surface == NONE || first == NONE)
				continue;
			landing_piece = ae_travel_piece(landing_surface);
			if (landing_piece == NONE || landing_piece == piece || !escapes[landing_piece])
				continue;
			if (!ae_travel_jump_clear(point, &ground))
				continue;
			{
				/* (apart from this piece's other drops: the entry 2.5 from each) */
				long other;
				boolean apart = TRUE;

				for (other = 0; other < ae_travel.link_count; other++)
				{
					if (ae_travel.links[other].kind == AE_BOTS_TRAVEL_LINK_JUMP && ae_travel.links[other].source == piece &&
						bots_distance3d(&ae_travel.links[other].entry, point) < AE_TRAVEL_WALK_SPACING)
					{
						apart = FALSE;
					}
				}
				if (!apart)
					continue;
			}
			{
				long index = ae_travel.link_count;
				struct ae_bots_travel_link *link = &ae_travel.links[index];

				link->kind = AE_BOTS_TRAVEL_LINK_JUMP;
				link->source = piece;
				link->target = landing_piece;
				link->entry = *point;
				link->exit = ground;
				ae_travel.entry_ground[index] = *point;
				ae_travel.entry_surface[index] = ae_travel_jump_corners.surface[corner];
				ae_travel.link_count++;
				platform_log("bots: travel: drop found from (%.1f %.1f %.1f) to (%.1f %.1f %.1f): %.1f down", point->x, point->y,
					point->z, ground.x, ground.y, ground.z, (double)(point->z - ground.z));
			}
			if (!made[piece])
				joined++;
			made[piece]++;
			added++;
		}
		free(made);
		if (!joined)
			break;
	}
	return added;
}

/* the pieces the links reach from the main piece (the one with the most surfaces), by any number of links */
static void ae_travel_reach_pieces(unsigned char *reached, long main_piece)
{
	boolean changed = TRUE;

	reached[main_piece] = TRUE;
	while (changed)
	{
		long index;

		changed = FALSE;
		for (index = 0; index < ae_travel.link_count; index++)
		{
			long from = ae_travel_link_piece(index, FALSE), to = ae_travel_link_piece(index, TRUE);

			if (from != NONE && to != NONE && reached[from] && !reached[to])
			{
				reached[to] = TRUE;
				changed = TRUE;
			}
		}
	}
}

/* a piece no route reaches gets a jump up from the nearest reached ground (and one back down), once the walk links are
found: the corners of the bigger pieces' surfaces, pairs across a hop's reach, the nearest few probed for air */
static void ae_travel_jumps_scan(void)
{
	struct collision_bsp const *bsp = TAG_BLOCK_GET_ELEMENT(&bots_paths.structure.collision_bsp, 0, struct collision_bsp);
	unsigned char *reached;
	long surface, main_piece = NONE, round, added = 0, unreached = 0, pieces_joined = 0;

	ae_travel_jump_corners.count = 0;
	for (surface = 0; surface < bsp->surfaces.count && surface < ae_travel_pieces.count; surface++)
	{
		real_point3d corners[BOTS_SURFACE_CORNERS], middle;
		long corner_count, corner, piece = ae_travel_piece(surface);
		real reach;

		if (piece == NONE || ae_travel_pieces.size[piece] < AE_TRAVEL_WALK_BIG ||
			!bots_paths_walkable_surface(bsp, surface, corners, &corner_count, &middle, &reach))
		{
			continue;
		}
		if (main_piece == NONE || ae_travel_pieces.size[piece] > ae_travel_pieces.size[main_piece])
			main_piece = piece;
		for (corner = 0; corner < corner_count && ae_travel_jump_corners.count < AE_TRAVEL_JUMP_CORNERS; corner++)
		{
			real_point3d point, ground;
			real away = bots_distance3d(&corners[corner], &middle), pull = away > 0.01f ? MIN(AE_BOTS_TRAVEL_JUMP_PULL / away, 0.5f) : 0.0f;
			long index = ae_travel_jump_corners.count;

			point.x = corners[corner].x + (middle.x - corners[corner].x) * pull;
			point.y = corners[corner].y + (middle.y - corners[corner].y) * pull;
			point.z = corners[corner].z + (middle.z - corners[corner].z) * pull;
			if (bots_surface_below(&point, &ground) != surface)
				continue;
			ae_travel_jump_corners.raw[index] = corners[corner];
			ae_travel_jump_corners.point[index] = ground;
			ae_travel_jump_corners.landing[index] = ground;
			{
				/* (the landing: AE_BOTS_TRAVEL_JUMP_LAND in, no further than half way to the middle, on the piece's ground) */
				real_point3d inside, inside_ground;
				real deep = away > 0.01f ? MIN(AE_BOTS_TRAVEL_JUMP_LAND / away, 0.5f) : 0.0f;
				long below;

				inside.x = corners[corner].x + (middle.x - corners[corner].x) * deep;
				inside.y = corners[corner].y + (middle.y - corners[corner].y) * deep;
				inside.z = corners[corner].z + (middle.z - corners[corner].z) * deep;
				below = bots_surface_below(&inside, &inside_ground);
				if (below != NONE && ae_travel_piece(below) == piece && fabs(inside_ground.z - ground.z) < 0.3f)
					ae_travel_jump_corners.landing[index] = inside_ground;
			}
			ae_travel_jump_corners.surface[index] = surface;
			ae_travel_jump_corners.piece[index] = piece;
			ae_travel_jump_corners.count++;
		}
	}
	if (main_piece == NONE)
		return;
	reached = (unsigned char *)malloc((size_t)MAX(ae_travel_pieces.count, 1));
	if (!reached)
		return;
	for (round = 0; round < 8; round++)
	{
		long target, joined = 0;

		csmemset(reached, 0, (size_t)MAX(ae_travel_pieces.count, 1));
		ae_travel_reach_pieces(reached, main_piece);
		for (target = 0; target < ae_travel_jump_corners.count; target++)
		{
			long piece = ae_travel_jump_corners.piece[target], tries[AE_TRAVEL_JUMP_TRIES], from[AE_TRAVEL_JUMP_TRIES], try_count = 0,
				other, slot, made = 0;
			real nearest[AE_TRAVEL_JUMP_TRIES];
			real_point3d first_entry = { 0.0f, 0.0f, 0.0f };

			/* (the first corner of an unreached piece does it for the piece; its others, and a piece tried, are skipped) */
			if (reached[piece] || ae_travel.link_count + 2 > AE_BOTS_TRAVEL_MAXIMUM_LINKS)
				continue;
			for (slot = 0; slot < ae_travel_jump_corners.count; slot++)
			{
				if (ae_travel_jump_corners.piece[slot] != piece)
					continue;
				for (other = 0; other < ae_travel_jump_corners.count; other++)
				{
					real across, rise;
					long place, worst;

					if (reached[ae_travel_jump_corners.piece[other]] != 1)
						continue;
					across = bots_distance2d(&ae_travel_jump_corners.raw[other], &ae_travel_jump_corners.raw[slot]);
					rise = ae_travel_jump_corners.raw[slot].z - ae_travel_jump_corners.raw[other].z;
					if (!ae_bots_travel_jump_ends(across, rise))
						continue;
					if (try_count < AE_TRAVEL_JUMP_TRIES)
						place = try_count++;
					else
					{
						for (worst = 0, place = 0; worst < try_count; worst++)
						{
							if (nearest[worst] > nearest[place])
								place = worst;
						}
						if (across >= nearest[place])
							continue;
					}
					tries[place] = slot;
					from[place] = other;
					nearest[place] = across;
				}
			}
			/* (the nearest first) */
			while (made < AE_TRAVEL_JUMPS_A_PIECE && try_count > 0)
			{
				long best = 0, index;

				for (index = 1; index < try_count; index++)
				{
					if (nearest[index] < nearest[best])
						best = index;
				}
				if (ae_travel_jump_clear(&ae_travel_jump_corners.point[from[best]], &ae_travel_jump_corners.landing[tries[best]]))
				{
					boolean apart = (boolean)(!made || bots_distance3d(&first_entry, &ae_travel_jump_corners.point[from[best]]) >=
						AE_TRAVEL_WALK_SPACING);

					if (apart && ae_travel.link_count + 2 <= AE_BOTS_TRAVEL_MAXIMUM_LINKS)
					{
						ae_travel_jump_add(from[best], tries[best]);
						ae_travel_jump_add(tries[best], from[best]);
						if (!made)
							first_entry = ae_travel_jump_corners.point[from[best]];
						platform_log("bots: travel: jump found from (%.1f %.1f %.1f) to (%.1f %.1f %.1f): %.1f across, %.2f up",
							ae_travel_jump_corners.point[from[best]].x, ae_travel_jump_corners.point[from[best]].y,
							ae_travel_jump_corners.point[from[best]].z, ae_travel_jump_corners.landing[tries[best]].x,
							ae_travel_jump_corners.landing[tries[best]].y, ae_travel_jump_corners.landing[tries[best]].z,
							(double)nearest[best], (double)(ae_travel_jump_corners.raw[tries[best]].z - ae_travel_jump_corners.raw[from[best]].z));
						made++;
						added += 2;
					}
				}
				tries[best] = tries[try_count - 1];
				from[best] = from[try_count - 1];
				nearest[best] = nearest[try_count - 1];
				try_count--;
			}
			/* (joined: a source for the next; else tried, not a source) */
			reached[piece] = (unsigned char)(made ? 1 : 2);
			if (made)
				joined++;
		}
		pieces_joined += joined;
		if (!joined)
			break;
	}
	{
		long piece;

		csmemset(reached, 0, (size_t)MAX(ae_travel_pieces.count, 1));
		ae_travel_reach_pieces(reached, main_piece);
		for (piece = 0; piece < ae_travel_pieces.count; piece++)
		{
			if (ae_travel_pieces.piece[piece] == piece && ae_travel_pieces.size[piece] >= AE_TRAVEL_WALK_BIG && !reached[piece])
				unreached++;
		}
	}
	{
		long drops = ae_travel_drops_scan(reached, main_piece);

		platform_log("bots: travel: %ld drop links found for pieces no link leaves", drops);
	}
	free(reached);
	platform_log("bots: travel: %ld jump links found for %ld pieces no walk or teleporter reached; %ld bigger pieces still unreached",
		added, pieces_joined, unreached);
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
		ae_travel_jumps_scan();
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
	char extra[640], pieces[80];
	long index;

	ae_bots_objlog_stuck_event(bot, (long)(bot - bots_globals.bots), tick);
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

		long own_piece = own_surface != NONE ? ae_travel_piece(own_surface) : NONE;
		long goal_piece = bot->goal_surface_index != NONE ? ae_travel_piece(bot->goal_surface_index) : NONE;

		snprintf(pieces, sizeof(pieces), " pieces=%ld/%ld surface=%ld open=%d sizes=%ld/%ld", own_surface != NONE ? own_piece : -2L,
			bot->goal_surface_index != NONE ? goal_piece : -2L, own_surface,
			(int)(own_surface != NONE && bots_paths.walkable && own_surface < bots_paths.structure.pathfinding_surfaces.count ?
				TEST_FLAG(bots_paths.walkable[own_surface], BOTS_PATHFINDING_WALKABLE_BIT) ? 1 : 0 : -1),
			own_piece != NONE ? ae_travel_pieces.size[own_piece] : -1L, goal_piece != NONE ? ae_travel_pieces.size[goal_piece] : -1L);
	}
	snprintf(extra, sizeof(extra), "goal=%d gx=%.1f gy=%.1f gz=%.1f path=%s%s%s step=%.1f,%.1f,%.1f dir=%.0f "
		"ahead=%.2f/%.2f/%.2f drop=%.2f x=%.1f y=%.1f z=%.1f", (int)bot->goal_kind, bot->goal_point.x, bot->goal_point.y,
		bot->goal_point.z, bot->path.valid ? "yes" : "no", bot->path_short ? "+short" : "",
		bot->climb_until_tick && tick->now < bot->climb_until_tick ? "+climb" : "", step_x, step_y, step_z,
		tick->move_direction / BOTS_DEGREES, ahead[0], ahead[1], ahead[2], drop, tick->position.x, tick->position.y,
		tick->position.z);
	strncat(extra, pieces, sizeof(extra) - strlen(extra) - 1);
	{
		/* (Frigid Keep 11-02: stuck with nothing ahead at the three heights) the same knee ray also against back faces
		and every kind of object (players, vehicles, equipment, controls): what it hit, how far; and the slope the bot
		stands on (the ground's normal, 1 for flat) */
		unsigned long all = FLAG(_collision_test_structure_bit) | FLAG(_collision_test_front_facing_surfaces_bit) |
			FLAG(_collision_test_back_facing_surfaces_bit) | FLAG(_collision_test_objects_bit) |
			(FLAG(_collision_test_objects_last_type_bit + 1) - FLAG(_collision_test_objects_first_type_bit));
		real_vector3d vector, down = { 0.0f, 0.0f, -2.0f };
		real wide = 9.0f, slope = 9.0f;
		int what = -1;
		char more[64];

		start = tick->position;
		start.z += 0.15f;
		vector.i = (real)cos(tick->move_direction) * 0.6f;
		vector.j = (real)sin(tick->move_direction) * 0.6f;
		vector.k = 0.0f;
		if (collision_test_vector(all, &start, &vector, tick->player->unit_index, &collision))
		{
			wide = collision.t * 0.6f;
			what = collision.type == _collision_result_structure ? -2 :
				(collision.object_index != NONE ? (int)object_get(collision.object_index)->object.type : -3);
		}
		start = tick->position;
		start.z += 0.4f;
		if (collision_test_vector(FLAG(_collision_test_structure_bit) | FLAG(_collision_test_front_facing_surfaces_bit),
			&start, &down, tick->player->unit_index, &collision))
		{
			slope = collision.plane.n.k;
		}
		snprintf(more, sizeof(more), " any=%.2f hit=%d slope=%.2f", wide, what, slope);
		strncat(extra, more, sizeof(extra) - strlen(extra) - 1);
		/* (the walk's own state: the throttle it asked, its speed, the bots within 1.6 wu (a crowd), crouched, the stuck count) */
		{
			real_vector3d const *velocity = &tick->object->object.translational_velocity;
			long near = 0, other;

			for (other = 0; other < BOTS_BRAINS; other++)
			{
				struct bots_bot const *mate = &bots_globals.bots[other];
				struct object_datum const *object = mate != bot && mate->player_index != NONE && mate->unit_index != NONE ?
					(struct object_datum const *)object_try_and_get_and_verify_type(mate->unit_index, _object_mask_unit) : NULL;

				if (object && bots_distance2d(&object->object.position, &tick->position) < 1.6f &&
					fabs(object->object.position.z - tick->position.z) < 1.5f)
					near++;
			}
			snprintf(more, sizeof(more), " th=%.2f sp=%.3f nb=%ld cr=%d", tick->throttle_scale,
				(real)sqrt(velocity->i * velocity->i + velocity->j * velocity->j), near,
				TEST_FLAG(tick->flags, _unit_control_crouch_modifier_bit) ? 1 : 0);
			strncat(extra, more, sizeof(extra) - strlen(extra) - 1);
		}
	}
	{
		/* (the objective log's words, appended: team, mode, job, goal, role, distances, fight, tether) */
		char objective[200];

		ae_bots_objlog_stuck_words(bot, (long)(bot - bots_globals.bots), tick, objective, sizeof(objective));
		strncat(extra, objective, sizeof(extra) - strlen(extra) - 1);
	}
	ae_bots_event_line((long)(bot - bots_globals.bots), tick->now, "stuck", -1, (real)bot->stuck_count, extra);
}

/* a fight's throttle at or under this is a slow walk by choice (ae_bots_fear.inc: SUPPRESS 0.35, HOLD 0.2) */
#define AE_TRAVEL_SLOW_BY_CHOICE 0.4f

/* (ae_bots_fear.inc, included after this file) */
static boolean ae_bots_fear_slowed(long bot_index, long now);
static void ae_bots_fear_free(long bot_index, long until_tick);

static struct ae_bots_travel_slow ae_travel_slow[BOTS_BRAINS];

boolean ae_bots_travel_may_stick(struct bots_bot *bot, struct bots_tick *tick)
{
	long bot_index = (long)(bot - bots_globals.bots);

	/* (the countdown holds every player where they stand) */
	if (game_engine_pregame_countdown_ticks_left() > 0)
		return FALSE;
	/* (a squad member the tether stands in its slot: stood still on purpose) */
	if (ae_bots_fireteam_holding(bot_index, tick->now))
		return FALSE;
	/* (a fight's slow walk by choice: a bot holding its ground (suppressing, a wide squad's hold: throttle 0.35 or less,
	crouched now and then) barely moves by design; Frigid Keep 11-02, 72 of 152 stuck reports were those, each a jump
	and a lost goal. By DISPLACEMENT, not by the throttle (the roles clamp it after this check, so the throttle seen
	here was never theirs): slow while the bot moves; pinned (still within a unit, at rest, five samples in a row)
	it is stuck as any: the roles stand off for two seconds (ae_bots_fear_free) so the recovery's step is not throttled) */
	if (tick->fighting && tick->moving && bot_index >= 0 && bot_index < BOTS_BRAINS &&
		(tick->throttle_scale <= AE_TRAVEL_SLOW_BY_CHOICE || ae_bots_fear_slowed(bot_index, tick->now)))
	{
		real_vector3d const *velocity = &tick->object->object.translational_velocity;

		if (!ae_bots_travel_slow_pinned(&ae_travel_slow[bot_index], tick->now, tick->position.x, tick->position.y,
			tick->position.z, (real)sqrt(velocity->i * velocity->i + velocity->j * velocity->j)))
		{
			return FALSE;
		}
		ae_bots_fear_free(bot_index, tick->now + 2 * TICKS_PER_SECOND);
		if (ae_bots_events_on())
			platform_log("bots: ev=slow_pinned bot=%ld x=%.1f y=%.1f z=%.1f", bot_index + 1, tick->position.x, tick->position.y,
				tick->position.z);
		return TRUE;
	}
	else if (bot_index >= 0 && bot_index < BOTS_BRAINS)
		ae_travel_slow[bot_index].still = 0;
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

/* (TRAPPED) the nearest walkable spot of a main piece within AE_BOTS_TRAVEL_TRAP_REACH of from: a surface's middle that stands
on that surface; FALSE for none */
static boolean ae_travel_trap_way_out(real_point3d const *from, real_point3d *out)
{
	struct collision_bsp const *bsp;
	real_point3d corners[BOTS_SURFACE_CORNERS];
	real best = AE_BOTS_TRAVEL_TRAP_REACH * AE_BOTS_TRAVEL_TRAP_REACH;
	long surface, count;
	boolean found = FALSE;

	if (!bots_paths.walkable || !bots_paths.scan_done)
		return FALSE;
	bsp = TAG_BLOCK_GET_ELEMENT(&bots_paths.structure.collision_bsp, 0, struct collision_bsp);
	count = MIN(bsp->surfaces.count, bots_paths.structure.pathfinding_surfaces.count);
	for (surface = 0; surface < count; surface++)
	{
		real_point3d middle, probe, ground;
		long corner, corner_count, piece;
		real dx, dy, dz, distance;

		if (!TEST_FLAG(bots_paths.walkable[surface], BOTS_PATHFINDING_WALKABLE_BIT))
			continue;
		piece = ae_travel_piece(surface);
		if (piece == NONE || ae_travel_pieces.size[piece] < AE_BOTS_TRAVEL_TRAP_PIECE_LEAST)
			continue;
		corner_count = bots_surface_corners(bsp, surface, corners);
		if (corner_count < 3)
			continue;
		middle.x = middle.y = middle.z = 0.0f;
		for (corner = 0; corner < corner_count; corner++)
		{
			middle.x += corners[corner].x / (real)corner_count;
			middle.y += corners[corner].y / (real)corner_count;
			middle.z += corners[corner].z / (real)corner_count;
		}
		dx = middle.x - from->x;
		dy = middle.y - from->y;
		dz = middle.z - from->z;
		distance = dx * dx + dy * dy + dz * dz;
		if (distance >= best)
			continue;
		probe = middle;
		probe.z += 0.5f;
		if (bots_surface_below(&probe, &ground) != surface)
			continue;
		best = distance;
		*out = ground;
		out->z += 0.2f;
		found = TRUE;
	}
	return found;
}

/* (TRAPPED) the stuck report off the paths: timed from the first; past AE_BOTS_TRAVEL_TRAP_TICKS with no route, moved out */
static void ae_travel_trap_check(struct bots_bot *bot, struct ae_travel_bot *travel, struct bots_tick *tick)
{
	real_point3d own, out;
	long surface = bots_unit_surface(tick->player->unit_index, &own);
	long piece = surface != NONE ? ae_travel_piece(surface) : NONE;
	boolean off = surface != NONE && bots_paths.walkable && surface < bots_paths.structure.pathfinding_surfaces.count &&
		(!TEST_FLAG(bots_paths.walkable[surface], BOTS_PATHFINDING_WALKABLE_BIT) || piece == NONE ||
			ae_travel_pieces.size[piece] < AE_BOTS_TRAVEL_TRAP_PIECE_LEAST);
	real moved;

	if (!off || tick->object->object.parent_object_index != NONE)
	{
		travel->trap_tick = 0;
		return;
	}
	moved = travel->trap_tick > 0 ? (real)bots_distance3d(&travel->trap_point, &tick->position) : 0.0f;
	if (travel->trap_tick <= 0 || travel->trap_tick > tick->now || moved >= AE_BOTS_TRAVEL_TRAP_MOVED)
	{
		travel->trap_tick = tick->now;
		travel->trap_point = tick->position;
		return;
	}
	if (!ae_bots_travel_trapped(off, bot->path.valid, moved, tick->now - travel->trap_tick) || !ae_travel_trap_way_out(&tick->position, &out))
		return;
	if (ae_bots_events_on())
	{
		char extra[96];

		snprintf(extra, sizeof(extra), "x=%.1f y=%.1f z=%.1f to=%.1f,%.1f,%.1f surface=%ld", tick->position.x, tick->position.y,
			tick->position.z, out.x, out.y, out.z, surface);
		ae_bots_event_line((long)(bot - bots_globals.bots), tick->now, "trap_out", -1, (real)bots_distance3d(&tick->position, &out),
			extra);
	}
	if (player_teleport(bot->player_index, NONE, &out))
	{
		bot->goal_kind = _bots_goal_none;
		bot->path.valid = FALSE;
		bot->stuck_count = 0;
	}
	travel->trap_tick = 0;
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
	{
		ae_travel_trap_check(bot, travel, tick);
		ae_travel_jump_out(bot, travel, tick, "stuck: jump");
	}
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

/* whether the bot is in the air on a link's jump (ae_bots_combat.inc: crouched at the top, fighting or not) */
static boolean ae_travel_jump_flying(struct bots_bot const *bot)
{
	struct ae_travel_bot const *travel = ae_travel_state((struct bots_bot *)bot);

	return (boolean)(travel && travel->via >= 0 && travel->phase == _ae_travel_phase_in_air &&
		ae_travel.links[travel->via].kind == AE_BOTS_TRAVEL_LINK_JUMP);
}

/* A LINK'S JUMP, DRIVEN (Wizard 11-08: bots fighting for the hill never took off: the fight's strafe and the edge guard
turned them from the edge, the button was pressed a unit before it, and the jump falls short of a rim 2.0 across and 0.45
up from there): the bot near a jump's entry runs straight along its line at full throttle, the button pressed at the
entry at its running speed, the way on to the landing in the air; whatever the fight asked of its legs is overruled from
the run-up's start to the landing (its aim and trigger are its own). Called last in the bot's tick (ae_bots_falls_guard) */
static void ae_travel_jump_drive(struct bots_bot *bot, struct bots_tick *tick)
{
	struct ae_travel_bot *travel = ae_travel_state(bot);
	struct ae_bots_travel_link const *link;
	real_vector3d const *velocity = &tick->object->object.translational_velocity;
	real dx, dy, length, ux, uy, along, lateral, speed, aim_x, aim_y;
	long now = tick->now;

	if (!travel || travel->via < 0 || travel->phase == _ae_travel_phase_to_entry)
		return;
	link = &ae_travel.links[travel->via];
	if (link->kind != AE_BOTS_TRAVEL_LINK_JUMP)
		return;
	tick->moving = TRUE;
	tick->throttle_scale = 1.0f;
	if (travel->phase == _ae_travel_phase_in_air)
	{
		tick->move_direction = (real)atan2(link->exit.y - tick->position.y, link->exit.x - tick->position.x);
		return;
	}
	dx = link->exit.x - link->entry.x;
	dy = link->exit.y - link->entry.y;
	length = (real)sqrt(dx * dx + dy * dy);
	if (length < 0.01f)
		return;
	ux = dx / length;
	uy = dy / length;
	along = (tick->position.x - link->entry.x) * ux + (tick->position.y - link->entry.y) * uy;
	lateral = -(tick->position.x - link->entry.x) * uy + (tick->position.y - link->entry.y) * ux;
	speed = (real)sqrt(velocity->i * velocity->i + velocity->j * velocity->j);
	if (along >= -0.1f && (fabs(lateral) <= AE_TRAVEL_JUMP_LINE || along > 0.6f) &&
		(speed >= AE_TRAVEL_JUMP_SPEED_MIN || now - travel->approach_tick > TICKS_PER_SECOND * 2))
	{
		tick->flags |= FLAG(_unit_control_jump_bit);
		travel->phase = _ae_travel_phase_in_air;
		travel->jump_tick = now;
		travel->air_top = tick->position.z;
		ae_travel.jumps++;
		if (ae_bots_trace_level() > 0)
		{
			platform_log("bots: travel trace jump press link %d at (%.2f %.2f %.2f) speed %.3f, %.2f along the entry, %.2f off the line",
				(int)travel->via, tick->position.x, tick->position.y, tick->position.z, speed, along, lateral);
		}
		tick->move_direction = (real)atan2(uy, ux);
		return;
	}
	/* (the line a step ahead of where the bot is along it, held to the entry: a run along it from wherever it comes) */
	{
		real ahead = MIN(along + 0.9f, 0.0f);

		aim_x = link->entry.x + ux * ahead;
		aim_y = link->entry.y + uy * ahead;
		if (fabs(lateral) > AE_TRAVEL_JUMP_LINE && along < -1.0f)
		{
			/* (off the line and not near the entry: back to the start of the run-up first) */
			aim_x = link->entry.x - ux * 1.5f;
			aim_y = link->entry.y - uy * 1.5f;
		}
	}
	tick->move_direction = (real)atan2(aim_y - tick->position.y, aim_x - tick->position.x);
}

/* (bots.c's bots_search, before its closed-start escape) a bot on a surface an object closed, with room above it, that it has
stood on for AE_BOTS_TRAVEL_REOPEN_TICKS, opens the surface again (ae_bots_travel_rules.h): the walks made over the closed
surfaces are made anew */
static struct
{
	long surface;
	long since;
} ae_travel_closed_stand[BOTS_BRAINS];

void ae_bots_travel_reopen(struct bots_bot *bot, long surface_index, real_point3d const *feet)
{
	unsigned long flags = FLAG(_collision_test_structure_bit) | FLAG(_collision_test_front_facing_surfaces_bit) |
		FLAG(_collision_test_back_facing_surfaces_bit);
	real_vector3d up = { 0.0f, 0.0f, AE_BOTS_TRAVEL_REOPEN_HEADROOM };
	struct collision_result collision;
	real_point3d start = *feet;
	byte const *map_walkable;
	long bot_index = (long)(bot - bots_globals.bots), now = game_time_get();

	if (!bots_paths.walkable || !bots_paths.source || !VALID_INDEX(surface_index, bots_paths.structure.pathfinding_surfaces.count) ||
		!VALID_INDEX(bot_index, BOTS_BRAINS))
	{
		return;
	}
	if (TEST_FLAG(bots_paths.walkable[surface_index], BOTS_PATHFINDING_WALKABLE_BIT))
	{
		ae_travel_closed_stand[bot_index].surface = NONE;
		return;
	}
	if (ae_travel_closed_stand[bot_index].surface != surface_index || now < ae_travel_closed_stand[bot_index].since)
	{
		ae_travel_closed_stand[bot_index].surface = surface_index;
		ae_travel_closed_stand[bot_index].since = now;
	}
	map_walkable = (byte const *)xbox_pointer(bots_paths.source->pathfinding_surfaces.address);
	start.z += 0.05f;
	if (!ae_bots_travel_reopen_wanted(TRUE, (boolean)TEST_FLAG(map_walkable[surface_index], BOTS_PATHFINDING_WALKABLE_BIT),
		(boolean)collision_test_vector(flags, &start, &up, NONE, &collision), now - ae_travel_closed_stand[bot_index].since))
	{
		return;
	}
	bots_paths.walkable[surface_index] |= (byte)FLAG(BOTS_PATHFINDING_WALKABLE_BIT);
	if (bots_paths.closed_count > 0)
		bots_paths.closed_count--;
	ae_travel_ways.closed = -1;
	ae_travel_closed_stand[bot_index].surface = NONE;
	platform_log("bots: surface %ld a bot has stood on for %ld s, closed by what is in the way, opened again", surface_index,
		(long)(AE_BOTS_TRAVEL_REOPEN_TICKS / TICKS_PER_SECOND));
}
