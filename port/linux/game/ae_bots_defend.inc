/*
AE_BOTS_DEFEND.INC

Included by ae_bots_lan.inc before the fireteams (ae_bots_fireteam.inc, which calls it): a fireteam told DEFENDING HERE, and in a
BTB game a bot squad whose role is to defend, hold the point (ae_bots_defend_rules.h has the numbers and the reasons).

- STATE: per fireteam, set up when its order is new (the order's serial) or its point moved: the anchor and the area, the ways in
  (the open directions round the area, nearest the enemy first), the places (the area's walkable surfaces sampled as the cover
  scan does, at most a few ticks' rays a tick, the surfaces near the anchor only), the morale and the phase. Per bot: its place,
  the way it watches, how long it has been there.
- PLACES: a spaced place with walls toward its way in and the enemy's side, a line back onto the way, near the point; BTB leaves
  out places a vehicle drives onto. Nothing is written anywhere; a new map starts again.
- THE SWITCH: HALO_BOTS_DEFEND_HOLD=0 keeps today's behaviour exactly: no state, no scan, no log line (A/B games).
- Logs: "ae defend: ..." one line per event.
*/

#include "errors.h"
#include "ae_h2_king.h"
#include "ae_bots_defend_rules.h"

/* (ae_bots_fireteam.inc's, after this file) */
static void ae_bots_fireteam_note_hold(long bot_index, long now);
static boolean ae_bots_fireteam_objective_window(long fireteam);

#define AE_BOTS_DEFEND_PLACES 96
#define AE_BOTS_DEFEND_WAYS 4
/* the ways' look: this many directions round the area, out to the radius plus this */
#define AE_BOTS_DEFEND_DIRECTIONS 16
#define AE_BOTS_DEFEND_WAY_REACH 6.0f
/* the places' scan: the probes and the surfaces looked at a tick, all fireteams together */
#define AE_BOTS_DEFEND_RAYS_A_TICK 160
#define AE_BOTS_DEFEND_SURFACES_A_TICK 3000
/* a hill's zone is held by its middle: places this near it (the zone's real shape is the game's) */
#define AE_BOTS_DEFEND_ZONE_RADIUS 4.0f
/* at the place: this near across, this much up or down */
#define AE_BOTS_DEFEND_THERE 0.8f
/* a place not reached in this long is given up, and not picked again for the second */
#define AE_BOTS_DEFEND_REACH_TICKS (12 * TICKS_PER_SECOND)
/* the reach is timed once the bot is this near the area's edge */
#define AE_BOTS_DEFEND_NEAR_AREA 6.0f
/* a stuck bot gives its place up after this, and picks again no sooner */
#define AE_BOTS_DEFEND_STUCK_TICKS (5 * TICKS_PER_SECOND)
#define AE_BOTS_DEFEND_BAN_TICKS (10 * TICKS_PER_SECOND)
#define AE_BOTS_DEFEND_BANS 3
/* the best few places by the cheap score tried for a line back onto the way */
#define AE_BOTS_DEFEND_TRIED 4

struct ae_bots_defend_team
{
	boolean active;
	long epoch;
	/* the anchor's travel piece */
	long piece;
	long serial;
	short profile;
	/* the hold is a role's (the mode's default) and not the leader's own call */
	boolean role_hold;
	/* a hill's zone: places inside AE_BOTS_DEFEND_ZONE_RADIUS of the middle only */
	boolean zone;
	short phase;
	real morale;
	long phase_tick;
	long morale_tick;
	real_point3d anchor;
	real radius;
	/* the enemy's side seen from the anchor (radians), known or not */
	boolean enemy_known;
	real enemy_yaw;
	long way_count;
	real way_yaw[AE_BOTS_DEFEND_WAYS];
	real_point3d way_point[AE_BOTS_DEFEND_WAYS];
	/* the places: scanned from the surfaces near the anchor */
	boolean scan_done;
	long scan_surface;
	long scan_sample;
	long count;
	real_point3d point[AE_BOTS_DEFEND_PLACES];
	unsigned char walls[AE_BOTS_DEFEND_PLACES];
	unsigned char roof[AE_BOTS_DEFEND_PLACES];
	/* a hill's place found off the hill on arrival: no one is sent there again */
	unsigned char off_hill[AE_BOTS_DEFEND_PLACES];
	/* the fall-back spot (found when first needed) */
	boolean fallback_known;
	real_point3d fallback;
	/* the log */
	long start_tick;
	long fall_back_count;
	real morale_logged;
	long morale_log_tick;
};

struct ae_bots_defend_bot
{
	long player_index;
	long epoch;
	long fireteam;
	/* its place (an index of the team's places), NONE for none (open ground) */
	long place;
	real place_score;
	short way;
	boolean exposed;
	long chosen_tick;
	long arrived_tick;
	long banned[AE_BOTS_DEFEND_BANS];
	long banned_until[AE_BOTS_DEFEND_BANS];
	long banned_next;
	long pick_again_tick;
	long log_tick;
	/* why it picked again: the log's word */
	char const *why;
	/* the hold in a fight */
	long jink_until;
	real jink_direction;
	long watch_tick;
	short seen_by;
	short seen_by_near;
	boolean crouch_walk;
	long exposed_log_tick;
	/* the place (index + 1) its first peek was logged at, 0 for none: one log line a place */
	long peek_logged;
};

static struct ae_bots_defend_team ae_bots_defend_teams[AE_FIRETEAM_MAX];
static struct ae_bots_defend_bot ae_bots_defend_bots[BOTS_BRAINS];
/* the brains seen alive this round (a new game starts the list again): a member not yet spawned is not down */
static unsigned char ae_bots_defend_seen[BOTS_BRAINS];
/* the tick a seen member was first found gone (0: alive): it counts as down for AE_BOTS_DEFEND_DOWN_TICKS after, then it is out of
the round (no respawn: the hold's morale recovers) */
static long ae_bots_defend_gone[BOTS_BRAINS];
#define AE_BOTS_DEFEND_DOWN_TICKS (30 * TICKS_PER_SECOND)
#define AE_BOTS_DEFEND_OUT_TICKS (60 * TICKS_PER_SECOND)
/* teams' setups counted: a bot's state from an older setup is dropped */
static long ae_bots_defend_epochs;
/* some fireteam was in scope last tick */
static boolean ae_bots_defend_any_active;

/* the scan's probes used this tick (all fireteams) */
static long ae_bots_defend_rays_tick;
static long ae_bots_defend_rays_used;
static long ae_bots_defend_surfaces_used;

static boolean ae_bots_defend_on(void)
{
	static int on = -1;

	if (on < 0)
	{
		char const *text = getenv("HALO_BOTS_DEFEND_HOLD");

		on = text && text[0] == '0' ? 0 : 1;
	}
	return on > 0;
}

/* the profile of the game: its largest team, humans and bots together, those who quit not counted. Read once a game, when the
countdown has ended (before it, the count now); a seat swap or a bot leaving later never changes it */
static int ae_bots_defend_latched_profile = -1;

static int ae_bots_defend_profile_now(void)
{
	struct data_iterator iterator;
	struct player_datum *player;
	long counts[2] = { 0, 0 };
	int profile;

	if (ae_bots_defend_latched_profile >= 0)
		return ae_bots_defend_latched_profile;
	data_iterator_new(&iterator, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
	{
		if (player->quit_out_of_game || player->quit_out_of_game_time != NONE)
			continue;
		if (game_engine_has_teams() && VALID_INDEX(player->team_index, 2))
			counts[player->team_index]++;
	}
	profile = ae_defend_profile((int)MAX(counts[0], counts[1]));
	if (game_engine_pregame_countdown_ticks_left() <= 0 && game_time_get() > 0)
		ae_bots_defend_latched_profile = profile;
	return profile;
}

static char const *ae_bots_defend_profile_name(int profile)
{
	return profile == AE_DEFEND_BTB ? "btb" : "arena";
}

/* a new game: the states are the old game's */
static void ae_bots_defend_check_clock(void)
{
	static long last = 0;
	long now = game_time_get();

	if (now < last)
	{
		csmemset(ae_bots_defend_teams, 0, sizeof(ae_bots_defend_teams));
		csmemset(ae_bots_defend_bots, 0, sizeof(ae_bots_defend_bots));
		csmemset(ae_bots_defend_seen, 0, sizeof(ae_bots_defend_seen));
		csmemset(ae_bots_defend_gone, 0, sizeof(ae_bots_defend_gone));
		ae_bots_defend_latched_profile = -1;
	}
	last = now;
}

/* the bot's state, NULL for none */
static struct ae_bots_defend_bot *ae_bots_defend_bot_state(struct bots_bot *bot, long fireteam, struct ae_bots_defend_team const *team)
{
	long bot_index = (long)(bot - bots_globals.bots);
	struct ae_bots_defend_bot *state;

	if (bot_index < 0 || bot_index >= BOTS_BRAINS)
		return NULL;
	state = &ae_bots_defend_bots[bot_index];
	if (state->player_index != bot->player_index || state->fireteam != fireteam || state->epoch != team->epoch)
	{
		csmemset(state, 0, sizeof(*state));
		state->player_index = bot->player_index;
		state->fireteam = fireteam;
		state->epoch = team->epoch;
		state->place = NONE;
	}
	return state;
}

static boolean ae_bots_defend_unit_alive(struct bots_bot const *bot)
{
	struct player_datum const *player;

	if (bot->player_index == NONE)
		return FALSE;
	player = player_try_and_get(bot->player_index);
	return player && player->unit_index != NONE && object_try_and_get(player->unit_index);
}

/* the living members of the fireteam, and this brain's rank among them (0 first) */
static long ae_bots_defend_members(long fireteam, long brain, long *rank)
{
	long other, alive = 0;

	if (rank)
		*rank = 0;
	for (other = 0; other < BOTS_BRAINS; other++)
	{
		if (ae_fireteam_of_brain(other) != fireteam || !ae_bots_defend_unit_alive(&bots_globals.bots[other]))
			continue;
		if (rank && other < brain)
			(*rank)++;
		alive++;
	}
	return alive;
}

/* whether the fireteam is in scope of the hold: the switch, the profile (the game's now, or the one the state took at its
setup), its order (explicit: the leader's own) and its role */
static boolean ae_bots_defend_scope(long fireteam, short kind)
{
	struct ae_bots_defend_team const *team;
	int profile;
	boolean explicit;

	if (!ae_bots_defend_on() || fireteam < 0 || fireteam >= AE_FIRETEAM_MAX)
		return FALSE;
	team = &ae_bots_defend_teams[fireteam];
	profile = team->active ? team->profile : ae_bots_defend_profile_now();
	explicit = ae_fireteam_order_explicit_of(fireteam);
	/* (an ARENA game's roles are untouched: out before any other look) */
	if (!explicit && profile != AE_DEFEND_BTB)
		return FALSE;
	/* (TERRITORIES have their own hills: the role squads there are left as they were; a leader's own call still holds) */
	if (!explicit && ae_h2_territories_on())
		return FALSE;
	return ae_defend_in_scope(1, profile, explicit, kind, ae_fireteam_role_taken(fireteam)) != 0;
}

/* the enemy's side from a point: the way to the middle of the enemies alive; FALSE when there are none */
static boolean ae_bots_defend_enemy_side(short team_index, real_point3d const *from, real *yaw)
{
	struct data_iterator iterator;
	struct player_datum *player;
	real x = 0.0f, y = 0.0f;
	long count = 0;

	data_iterator_new(&iterator, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
	{
		real_point3d where;

		if (!game_engine_has_teams() || player->team_index == team_index || !ae_bots_team_alive(player) ||
			(ae_dev_cmd_unseen_player() != NONE && player_try_and_get(ae_dev_cmd_unseen_player()) == player))
		{
			continue;
		}
		where = ae_bots_vehicle_point(player->unit_index);
		x += where.x;
		y += where.y;
		count++;
	}
	if (count <= 0)
		return FALSE;
	*yaw = (real)atan2(y / (real)count - from->y, x / (real)count - from->x);
	return TRUE;
}

/* the team's own side: where its spawns are (the flag stand in a flag game) */
static boolean ae_bots_defend_base(short team_index, real_point3d *base)
{
	struct ae_bots_objective_snapshot const *snapshot = ae_bots_objective_snapshot();
	struct scenario *scenario = global_scenario_get();
	real x = 0.0f, y = 0.0f, z = 0.0f;
	long index, count = 0;

	if (snapshot && snapshot->mode == AE_BOTS_MODE_CTF && team_index >= 0 && team_index < 2 && snapshot->flags[team_index].exists)
	{
		*base = snapshot->flags[team_index].stand;
		return TRUE;
	}
	if (!scenario)
		return FALSE;
	for (index = 0; index < scenario->players.count; index++)
	{
		struct player_starting_location const *location =
			TAG_BLOCK_GET_ELEMENT(&scenario->players, index, struct player_starting_location);

		if (location->team_index != team_index)
			continue;
		x += location->position.x;
		y += location->position.y;
		z += location->position.z;
		count++;
	}
	if (count <= 0)
		return FALSE;
	base->x = x / (real)count;
	base->y = y / (real)count;
	base->z = z / (real)count;
	return TRUE;
}

/* the ways in: the open directions round the area (a walkable surface on the anchor's piece out past its edge), each run a way,
the one nearest the enemy's side first; four at most */
static void ae_bots_defend_find_ways(struct ae_bots_defend_team *team)
{
	boolean open[AE_BOTS_DEFEND_DIRECTIONS];
	real_point3d ground;
	long surface = bots_surface_below(&team->anchor, &ground), piece = surface != NONE ? ae_travel_piece(surface) : NONE;
	long direction, opens = 0, runs = 0, run_start[AE_BOTS_DEFEND_DIRECTIONS], run_length[AE_BOTS_DEFEND_DIRECTIONS], first = -1;
	real step = 2.0f * BOTS_PI / (real)AE_BOTS_DEFEND_DIRECTIONS;
	real yaws[AE_BOTS_DEFEND_WAYS * 2];
	long yaw_count = 0, index;

	team->way_count = 0;
	for (direction = 0; direction < AE_BOTS_DEFEND_DIRECTIONS; direction++)
	{
		real_point3d spot = team->anchor, floor;
		long under;

		spot.x += (real)cos(step * (real)direction) * (team->radius + AE_BOTS_DEFEND_WAY_REACH);
		spot.y += (real)sin(step * (real)direction) * (team->radius + AE_BOTS_DEFEND_WAY_REACH);
		under = bots_surface_below(&spot, &floor);
		open[direction] = under != NONE && fabs(floor.z - team->anchor.z) < AE_BOTS_COVER_HEIGHT &&
			(piece == NONE || ae_travel_piece(under) == piece);
		opens += open[direction];
	}
	/* (the runs of open directions, round the circle: each starts after a closed one) */
	for (direction = 0; direction < AE_BOTS_DEFEND_DIRECTIONS && opens < AE_BOTS_DEFEND_DIRECTIONS; direction++)
		if (!open[direction])
		{
			first = direction;
			break;
		}
	if (opens > 0 && opens < AE_BOTS_DEFEND_DIRECTIONS)
	{
		for (index = 1; index <= AE_BOTS_DEFEND_DIRECTIONS; index++)
		{
			long at = (first + index) % AE_BOTS_DEFEND_DIRECTIONS, before = (at + AE_BOTS_DEFEND_DIRECTIONS - 1) % AE_BOTS_DEFEND_DIRECTIONS;

			if (open[at] && !open[before])
			{
				run_start[runs] = at;
				run_length[runs] = 0;
				runs++;
			}
			if (open[at] && runs > 0)
				run_length[runs - 1]++;
		}
		for (index = 0; index < runs; index++)
		{
			real middle = step * ((real)run_start[index] + 0.5f * (real)(run_length[index] - 1));

			/* (a wide opening is two ways at its quarters) */
			if (run_length[index] > 8 && yaw_count + 2 <= (long)NUMBEROF(yaws))
			{
				yaws[yaw_count++] = step * ((real)run_start[index] + 0.25f * (real)(run_length[index] - 1));
				yaws[yaw_count++] = step * ((real)run_start[index] + 0.75f * (real)(run_length[index] - 1));
			}
			else if (yaw_count < (long)NUMBEROF(yaws))
				yaws[yaw_count++] = middle;
		}
	}
	else if (opens == AE_BOTS_DEFEND_DIRECTIONS)
	{
		/* (open all round: the four quarters) */
		for (index = 0; index < 4; index++)
			yaws[yaw_count++] = BOTS_PI * 0.5f * (real)index;
	}
	if (yaw_count <= 0)
	{
		/* (no way found: the enemy's side, or any) */
		yaws[yaw_count++] = team->enemy_known ? team->enemy_yaw : 0.0f;
	}
	/* (nearest the enemy's side first, four at most) */
	for (index = 0; index < yaw_count; index++)
	{
		long best = index, other;

		for (other = index + 1; other < yaw_count; other++)
		{
			real a = team->enemy_known ? (real)fabs(bots_angle_difference(yaws[other], team->enemy_yaw)) : (real)other;
			real b = team->enemy_known ? (real)fabs(bots_angle_difference(yaws[best], team->enemy_yaw)) : (real)best;

			if (a < b)
				best = other;
		}
		if (best != index)
		{
			real swap = yaws[index];

			yaws[index] = yaws[best];
			yaws[best] = swap;
		}
	}
	for (index = 0; index < yaw_count && team->way_count < AE_BOTS_DEFEND_WAYS; index++)
	{
		real_point3d spot = team->anchor;
		long under;

		spot.x += (real)cos(yaws[index]) * (team->radius + 3.0f);
		spot.y += (real)sin(yaws[index]) * (team->radius + 3.0f);
		team->way_yaw[team->way_count] = yaws[index];
		/* (the point a place looks at: the ground out there, else eye level at the anchor's) */
		under = bots_surface_below(&spot, &ground);
		team->way_point[team->way_count] = under != NONE ? ground : spot;
		team->way_count++;
	}
}

/* the fireteam's state, set up when its order is new or moved; NULL with no point to hold */
static struct ae_bots_defend_team *ae_bots_defend_team_for(long fireteam)
{
	struct ae_bots_defend_team *team = &ae_bots_defend_teams[fireteam];
	long serial = ae_fireteam_order_serial_of(fireteam);
	real_point3d point;

	if (!ae_fireteam_order_point_of(fireteam, &point))
	{
		team->active = FALSE;
		return NULL;
	}
	if (team->active && team->serial != serial)
	{
		/* (a role re-issued where it stood keeps its places, its phase and its morale; a leader's order given again, or a point
		that moved, starts again) */
		if (bots_distance3d(&team->anchor, &point) <= 2.0f && team->role_hold && !ae_fireteam_order_explicit_of(fireteam))
			team->serial = serial;
		else
		{
			if (bots_distance3d(&team->anchor, &point) > 2.0f)
				error(_error_silent, "ae defend: moved fireteam=%ld", fireteam);
			team->active = FALSE;
		}
	}
	if (!team->active)
	{
		struct ae_defend_numbers numbers;
		short team_index = ae_fireteam_team_of(fireteam);

		csmemset(team, 0, sizeof(*team));
		team->active = TRUE;
		team->epoch = ++ae_bots_defend_epochs;
		team->serial = serial;
		team->profile = (short)ae_bots_defend_profile_now();
		ae_defend_numbers_of(team->profile, &numbers);
		team->role_hold = !ae_fireteam_order_explicit_of(fireteam);
		team->zone = team->role_hold && ae_fireteam_role_taken(fireteam) == AE_ROLE_HILL;
		team->radius = team->zone ? AE_BOTS_DEFEND_ZONE_RADIUS : numbers.radius;
		team->anchor = point;
		{
			real_point3d floor;
			long under = bots_surface_below(&point, &floor);

			team->piece = under != NONE ? ae_travel_piece(under) : NONE;
		}
		team->phase = AE_DEFEND_HOLD;
		team->start_tick = game_time_get();
		team->morale_tick = team->start_tick;
		team->enemy_known = ae_bots_defend_enemy_side(team_index, &point, &team->enemy_yaw);
		if (!team->enemy_known)
		{
			real_point3d base;

			/* (none alive to look at: the enemy is on the side away from the team's own base) */
			if (ae_bots_defend_base(team_index, &base) && bots_distance2d(&base, &point) > 6.0f)
			{
				team->enemy_yaw = (real)atan2(point.y - base.y, point.x - base.x);
				team->enemy_known = TRUE;
			}
		}
		ae_bots_defend_find_ways(team);
		error(_error_silent, "ae defend: start fireteam=%ld profile=%s ways=%d radius=%.1f", fireteam,
			ae_bots_defend_profile_name(team->profile), (int)team->way_count, team->radius);
	}
	return team;
}

/* the places of the area, a few probes a tick: the walkable surfaces near the anchor sampled at their middles and halfway to
their corners, as the carriers' scan does */
static void ae_bots_defend_scan(struct ae_bots_defend_team *team)
{
	struct collision_bsp const *bsp;
	long now = game_time_get();

	if (team->scan_done || !bots_paths.walkable || !bots_paths.source || !bots_paths.scan_done)
		return;
	if (now != ae_bots_defend_rays_tick)
	{
		ae_bots_defend_rays_tick = now;
		ae_bots_defend_rays_used = 0;
		ae_bots_defend_surfaces_used = 0;
	}
	bsp = TAG_BLOCK_GET_ELEMENT(&bots_paths.structure.collision_bsp, 0, struct collision_bsp);
	while (ae_bots_defend_rays_used < AE_BOTS_DEFEND_RAYS_A_TICK && ae_bots_defend_surfaces_used < AE_BOTS_DEFEND_SURFACES_A_TICK)
	{
		real_point3d corners[BOTS_SURFACE_CORNERS], middle, point, ground;
		long corner_count, index, surface;
		real reach;
		unsigned walls;
		boolean roof, near = FALSE;

		if (team->scan_surface >= bsp->surfaces.count || team->count >= AE_BOTS_DEFEND_PLACES)
		{
			team->scan_done = TRUE;
			break;
		}
		ae_bots_defend_surfaces_used++;
		if (!bots_paths_walkable_surface(bsp, team->scan_surface, corners, &corner_count, &middle, &reach) ||
			team->scan_sample >= corner_count || (team->scan_sample >= 0 && reach < 1.0f) ||
			bots_distance2d(&middle, &team->anchor) > team->radius + reach + 0.5f ||
			fabs(middle.z - team->anchor.z) > AE_BOTS_COVER_HEIGHT + reach)
		{
			team->scan_surface++;
			team->scan_sample = -1;
			continue;
		}
		point = middle;
		if (team->scan_sample >= 0)
		{
			point.x = 0.5f * (middle.x + corners[team->scan_sample].x);
			point.y = 0.5f * (middle.y + corners[team->scan_sample].y);
			point.z = 0.5f * (middle.z + corners[team->scan_sample].z);
		}
		team->scan_sample++;
		if (bots_distance2d(&point, &team->anchor) > team->radius)
			continue;
		for (index = 0; index < team->count && !near; index++)
			near = bots_distance3d(&team->point[index], &point) < AE_BOTS_COVER_SPACING;
		if (near)
			continue;
		ae_bots_defend_rays_used++;
		surface = bots_surface_below(&point, &ground);
		if (surface == NONE || surface >= bots_paths.structure.pathfinding_surfaces.count ||
			!TEST_FLAG(bots_paths.walkable[surface], BOTS_PATHFINDING_WALKABLE_BIT) ||
			!ae_bots_cover_probe(&ground, &walls, &roof, &ae_bots_defend_rays_used) || (!walls && !roof) ||
			bots_distance2d(&ground, &team->anchor) > team->radius)
		{
			continue;
		}
		index = team->count++;
		team->point[index] = ground;
		team->walls[index] = (unsigned char)walls;
		team->roof[index] = (unsigned char)roof;
	}
	if (team->scan_done)
		error(_error_silent, "ae defend: places fireteam=%ld count=%ld", (long)(team - ae_bots_defend_teams), team->count);
}

static boolean ae_bots_defend_banned(struct ae_bots_defend_bot const *state, long place, long now)
{
	long index;

	for (index = 0; index < AE_BOTS_DEFEND_BANS; index++)
		if (state->banned[index] == place && now < state->banned_until[index])
			return TRUE;
	return FALSE;
}

static void ae_bots_defend_ban(struct ae_bots_defend_bot *state, long place, long now)
{
	if (place == NONE)
		return;
	state->banned[state->banned_next] = place;
	state->banned_until[state->banned_next] = now + AE_BOTS_DEFEND_BAN_TICKS;
	state->banned_next = (state->banned_next + 1) % AE_BOTS_DEFEND_BANS;
}

/* the nearest place held by another member of the fireteam to this point; negative when there is none */
static real ae_bots_defend_nearest_held(struct ae_bots_defend_team const *team, long fireteam, long bot_index,
	real_point3d const *point)
{
	long other;
	real nearest = -1.0f;

	for (other = 0; other < BOTS_BRAINS; other++)
	{
		struct ae_bots_defend_bot const *held = &ae_bots_defend_bots[other];
		real d;

		if (other == bot_index || held->fireteam != fireteam || held->player_index != bots_globals.bots[other].player_index ||
			held->place == NONE || held->place >= team->count || ae_fireteam_of_brain(other) != fireteam)
		{
			continue;
		}
		d = bots_distance3d(point, &team->point[held->place]);
		if (nearest < 0.0f || d < nearest)
			nearest = d;
	}
	return nearest;
}

/* the best place for the bot, NONE for none. beat: a place must score above this (the sticky score of the one it holds);
seer_yaw: the way an enemy that sees it is (radians), or NULL: walls toward that side count, not the ways in */
static long ae_bots_defend_pick(struct bots_bot *bot, struct ae_bots_defend_bot *state, struct ae_bots_defend_team *team,
	long fireteam, long bot_index, real beat, real const *seer_yaw, real *picked_score)
{
	struct ae_defend_numbers numbers;
	long candidates[AE_BOTS_DEFEND_TRIED];
	real scores[AE_BOTS_DEFEND_TRIED];
	long candidate_count = 0, index, best = NONE, now = game_time_get(), rank;
	real best_score = beat, yaws[2], edge;
	int yaw_count = 0;
	unsigned threat;
	struct player_datum const *player = player_try_and_get(bot->player_index);
	real_point3d eye;

	ae_defend_numbers_of(team->profile, &numbers);
	/* (a place at the area's edge leaves it when the defender steps out to peek: the places keep this far inside) */
	edge = team->zone ? 0.5f : 1.5f;
	ae_bots_defend_members(fireteam, bot_index, &rank);
	state->way = team->way_count > 0 ? (short)(rank % team->way_count) : 0;
	if (seer_yaw)
		yaws[yaw_count++] = *seer_yaw;
	else
	{
		if (team->way_count > 0)
			yaws[yaw_count++] = team->way_yaw[state->way];
		if (team->enemy_known)
			yaws[yaw_count++] = team->enemy_yaw;
	}
	threat = ae_defend_threat_sides(yaws, yaw_count);
	for (index = 0; index < team->count; index++)
	{
		real d = bots_distance2d(&team->point[index], &team->anchor);
		real nearest = ae_bots_defend_nearest_held(team, fireteam, bot_index, &team->point[index]);
		real score;
		long slot;

		if (team->off_hill[index] || ae_bots_defend_banned(state, index, now) || index == state->place || !ae_defend_spaced(nearest, numbers.spacing) ||
			ae_bots_travel_near_pad(&team->point[index], 1.0f))
		{
			continue;
		}
		/* (BTB: nowhere a vehicle drives onto or over: open on all sides but one) */
		if (numbers.vehicle_block && !team->roof[index])
		{
			unsigned bits = team->walls[index], count = 0;

			while (bits)
			{
				count += bits & 1u;
				bits >>= 1;
			}
			if (count < 2)
				continue;
		}
		/* (a line to break needs a wall on the shooter's side) */
		if (seer_yaw && !(team->walls[index] & threat))
			continue;
		score = ae_defend_place_score(team->walls[index], team->roof[index], threat, d, team->radius - edge, 0);
		if (score <= 0.0f)
			continue;
		score += bots_random_real(bot) * 0.3f;
		if (candidate_count == AE_BOTS_DEFEND_TRIED && score <= scores[candidate_count - 1])
			continue;
		slot = candidate_count < AE_BOTS_DEFEND_TRIED ? candidate_count++ : candidate_count - 1;
		while (slot > 0 && scores[slot - 1] < score)
		{
			scores[slot] = scores[slot - 1];
			candidates[slot] = candidates[slot - 1];
			slot--;
		}
		scores[slot] = score;
		candidates[slot] = index;
	}
	/* (the best few tried for a line back onto the way in) */
	if (player && player->unit_index != NONE)
	{
		for (index = 0; index < candidate_count; index++)
		{
			real d = bots_distance2d(&team->point[candidates[index]], &team->anchor), score;
			real_point3d target = team->way_count > 0 ? team->way_point[state->way] : team->anchor;
			boolean sees;

			eye = team->point[candidates[index]];
			eye.z += 0.65f;
			target.z += 0.65f;
			sees = bots_line_of_sight(player->unit_index, &eye, &target, NONE);
			score = ae_defend_place_score(team->walls[candidates[index]], team->roof[candidates[index]], threat, d, team->radius - edge,
				sees) + (scores[index] - ae_defend_place_score(team->walls[candidates[index]], team->roof[candidates[index]], threat, d,
				team->radius - edge, 0));
			if (score > best_score)
			{
				best_score = score;
				best = candidates[index];
			}
		}
	}
	*picked_score = best_score;
	return best;
}

static boolean ae_bots_defend_walkable(long surface)
{
	return surface != NONE && bots_paths.walkable && surface < bots_paths.structure.pathfinding_surfaces.count &&
		TEST_FLAG(bots_paths.walkable[surface], BOTS_PATHFINDING_WALKABLE_BIT);
}

/* the fall-back spot: toward the team's own base (its spawns; the flag stand in a flag game) a short way (ARENA) or a longer one
(BTB), else away from the enemy's side when the base is at the anchor; a covered place near it, else the ground there */
static void ae_bots_defend_find_fallback(struct ae_bots_defend_team *team, short team_index)
{
	real_point3d base, target, ground, best = team->anchor;
	real way, distance, reach = team->profile == AE_DEFEND_BTB ? 24.0f : 12.0f, best_score = -1.0f;
	long anchor_surface = bots_surface_below(&team->anchor, &ground), piece = anchor_surface != NONE ? ae_travel_piece(anchor_surface) : NONE;
	long attempt, index, surface = NONE;

	team->fallback_known = TRUE;
	team->fallback = team->anchor;
	if (ae_bots_defend_base(team_index, &base) && bots_distance2d(&base, &team->anchor) > 6.0f)
	{
		way = (real)atan2(base.y - team->anchor.y, base.x - team->anchor.x);
		distance = MIN(reach, bots_distance2d(&base, &team->anchor) - 3.0f);
		distance = MAX(distance, 6.0f);
	}
	else
	{
		way = (team->enemy_known ? team->enemy_yaw : 0.0f) + BOTS_PI;
		distance = reach;
	}
	for (attempt = 0; attempt < 3 && surface == NONE; attempt++)
	{
		real how_far = distance * (1.0f - 0.25f * (real)attempt);

		target = team->anchor;
		target.x += (real)cos(way) * how_far;
		target.y += (real)sin(way) * how_far;
		surface = bots_surface_below(&target, &ground);
		if (!ae_bots_defend_walkable(surface) || (piece != NONE && ae_travel_piece(surface) != piece))
			surface = NONE;
	}
	if (surface == NONE)
		return;
	best = ground;
	for (index = 0; index < 10; index++)
	{
		real angle = (real)index * 2.0f * BOTS_PI / 10.0f;
		real_point3d spot = ground, floor;
		unsigned walls;
		boolean roof;
		long rays = 0, under, cover;

		if (index > 0)
		{
			spot.x += (real)cos(angle) * 4.0f;
			spot.y += (real)sin(angle) * 4.0f;
		}
		under = bots_surface_below(&spot, &floor);
		if (!ae_bots_defend_walkable(under) || (piece != NONE && ae_travel_piece(under) != piece) ||
			!ae_bots_cover_probe(&floor, &walls, &roof, &rays))
			continue;
		cover = ae_bots_cover_score(walls, roof);
		if ((real)cover > best_score)
		{
			best_score = (real)cover;
			best = floor;
		}
	}
	team->fallback = best;
}

/* a defender's spot at the fall-back point: the members spread on a ring about it */
static void ae_bots_defend_fallback_spot(struct ae_bots_defend_team const *team, long fireteam, long bot_index, real_point3d *spot)
{
	long rank, living = ae_bots_defend_members(fireteam, bot_index, &rank);
	real angle = living > 0 ? (real)rank * 2.0f * BOTS_PI / (real)living : 0.0f;

	*spot = team->fallback;
	if (living > 1)
	{
		spot->x += (real)cos(angle) * 2.0f;
		spot->y += (real)sin(angle) * 2.0f;
	}
}

/* MORALE, once a second for an active fireteam: members down, the enemies near the anchor past its friends, a leader dead under
pressure raise it; time lowers it. Then the phase: hold, fall back, regroup, return (ae_defend_phase_step) */
static void ae_bots_defend_morale(struct ae_bots_defend_team *team, long fireteam, long now)
{
	struct ae_defend_numbers numbers;
	struct data_iterator iterator;
	struct player_datum *player;
	short own = ae_fireteam_team_of(fireteam);
	long brain, squad = 0, living = 0, down = 0, near_fallback = 0, at_fallback = 0, in_area = 0, allies = 0, enemies = 0;
	long leader = ae_fireteam_leader_player(fireteam), phase_age;
	boolean human_led = leader != NONE, leader_alive = FALSE, calm_log;
	int next;
	real seconds;

	if (now - team->morale_tick < TICKS_PER_SECOND)
		return;
	/* (before the game starts the squad is not yet on its feet: nobody is down) */
	if (game_engine_pregame_countdown_ticks_left() > 0)
	{
		team->morale_tick = now;
		return;
	}
	seconds = (real)MIN(now - team->morale_tick, 2 * TICKS_PER_SECOND) / (real)TICKS_PER_SECOND;
	team->morale_tick = now;
	ae_defend_numbers_of(team->profile, &numbers);
	if (human_led)
	{
		struct player_datum const *head = player_try_and_get(leader);

		leader_alive = head && head->unit_index != NONE && object_try_and_get(head->unit_index);
	}
	for (brain = 0; brain < BOTS_BRAINS; brain++)
	{
		struct player_datum const *member;
		real_point3d where;

		if (ae_fireteam_of_brain(brain) != fireteam || bots_globals.bots[brain].player_index == NONE)
			continue;
		member = player_try_and_get(bots_globals.bots[brain].player_index);
		if (!member)
			continue;
		squad++;
		if (member->unit_index == NONE || !object_try_and_get(member->unit_index))
		{
			down += ae_defend_member_down(ae_bots_defend_seen[brain] && ae_bots_defend_gone[brain] > 0 &&
				now - ae_bots_defend_gone[brain] <= AE_BOTS_DEFEND_DOWN_TICKS, 0);
			continue;
		}
		living++;
		where = object_get(member->unit_index)->object.position;
		if (bots_distance2d(&where, &team->anchor) <= team->radius + numbers.leash)
			in_area++;
		if (team->fallback_known)
		{
			real d = bots_distance2d(&where, &team->fallback);

			near_fallback += d <= AE_FIRETEAM_REFORM_RADIUS;
			at_fallback += d <= AE_FIRETEAM_REFORM_RADIUS * 2.0f;
		}
	}
	if (squad <= 0)
		return;
	data_iterator_new(&iterator, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
	{
		real_point3d where;

		if (!ae_bots_team_alive(player))
			continue;
		where = ae_bots_vehicle_point(player->unit_index);
		if (bots_distance2d(&where, &team->anchor) > team->radius + 14.0f || fabs(where.z - team->anchor.z) > 8.0f)
			continue;
		if (game_engine_has_teams() && player->team_index == own)
			allies++;
		else
			enemies++;
	}
	/* (a leader dead counts only under pressure: a fall, a stray grenade, a quiet death keeps the hold) */
	team->morale = ae_defend_morale_step(team->morale, (int)down, (int)(enemies > allies ? enemies - allies : 0),
		human_led && !leader_alive && (down > 0 || enemies > allies), seconds, numbers.morale_decay_seconds);
	calm_log = team->morale > 0.05f && now - team->morale_log_tick >= 10 * TICKS_PER_SECOND;
	if (fabs(team->morale - team->morale_logged) >= 0.15f || calm_log)
	{
		team->morale_logged = team->morale;
		team->morale_log_tick = now;
		error(_error_silent, "ae defend: morale fireteam=%ld value=%.2f", fireteam, (double)team->morale);
	}
	phase_age = now - team->phase_tick;
	/* (a phase that cannot finish (a member stuck, none alive) lets go after a while: no lock) */
	next = ae_defend_phase_step(team->phase, human_led, leader_alive, team->morale,
		(living > 0 && at_fallback * 2 >= living) || phase_age > 15 * TICKS_PER_SECOND,
		(living > 0 && near_fallback == living) || phase_age > 20 * TICKS_PER_SECOND,
		(int)living, (int)squad,
		(living > 0 && in_area * 2 >= living) || phase_age > 25 * TICKS_PER_SECOND);
	if (next == team->phase)
		return;
	if (next == AE_DEFEND_FALL_BACK)
	{
		ae_bots_defend_find_fallback(team, own);
		team->fall_back_count++;
		error(_error_silent, "ae defend: fall back fireteam=%ld human_led=%d", fireteam, human_led);
	}
	else if (next == AE_DEFEND_REGROUP)
		error(_error_silent, "ae defend: at fall-back spot fireteam=%ld", fireteam);
	else if (next == AE_DEFEND_RETURN)
	{
		error(_error_silent, "ae defend: regrouped fireteam=%ld", fireteam);
		error(_error_silent, "ae defend: return fireteam=%ld", fireteam);
	}
	team->phase = (short)next;
	team->phase_tick = now;
}

/* once a tick (the fireteams' roles update): every fireteam in scope has its state kept and its morale stepped; one out of scope
(its order gone, the feature off) has none */
static void ae_bots_defend_update(void)
{
	long fireteam, brain;

	if (!ae_bots_defend_on())
		return;
	ae_bots_defend_check_clock();
	/* (who is seen alive is kept while a hold is on, or a person leads a fireteam that may be given one: the order and what follows it
	can come in the same tick) */
	if (!ae_bots_defend_any_active)
		for (fireteam = 0; fireteam < ae_fireteam_slot_count() && fireteam < AE_FIRETEAM_MAX; fireteam++)
			if (ae_fireteam_team_of(fireteam) >= 0 && ae_fireteam_leader_player(fireteam) != NONE)
			{
				ae_bots_defend_any_active = TRUE;
				break;
			}
	if (ae_bots_defend_any_active)
	{
		long now = game_time_get();

		for (brain = 0; brain < BOTS_BRAINS; brain++)
		{
			if (ae_bots_defend_unit_alive(&bots_globals.bots[brain]))
			{
				ae_bots_defend_seen[brain] = 1;
				ae_bots_defend_gone[brain] = 0;
			}
			else if (ae_bots_defend_seen[brain])
			{
				if (!ae_bots_defend_gone[brain])
					ae_bots_defend_gone[brain] = now;
				else if (now - ae_bots_defend_gone[brain] > AE_BOTS_DEFEND_OUT_TICKS)
					ae_bots_defend_seen[brain] = 0, ae_bots_defend_gone[brain] = 0;
			}
		}
	}
	ae_bots_defend_any_active = FALSE;
	for (fireteam = 0; fireteam < ae_fireteam_slot_count() && fireteam < AE_FIRETEAM_MAX; fireteam++)
	{
		struct ae_bots_defend_team *team;

		if (ae_fireteam_team_of(fireteam) < 0 || ae_fireteam_order_of(fireteam) != _ae_order_defend ||
			!ae_bots_defend_scope(fireteam, _ae_order_defend))
		{
			ae_bots_defend_teams[fireteam].active = FALSE;
			continue;
		}
		team = ae_bots_defend_team_for(fireteam);
		if (team)
		{
			ae_bots_defend_any_active = TRUE;
			ae_bots_defend_morale(team, fireteam, game_time_get());
		}
	}
}

/* the goal for a defender of an in-scope fireteam at its place. 0: none here (the old goal code runs), 1: go to *goal, 2: stand */
static int ae_bots_defend_place_goal(struct bots_bot *bot, long bot_index, long fireteam, real_point3d const *position,
	real_point3d *goal, long *goal_surface)
{
	struct ae_bots_defend_team *team;
	struct ae_bots_defend_bot *state;
	long now = game_time_get();
	int level = ae_bots_level_of_skill(bots_globals.skill);

	ae_bots_defend_check_clock();
	team = ae_bots_defend_team_for(fireteam);
	if (!team)
		return 0;
	state = ae_bots_defend_bot_state(bot, fireteam, team);
	if (!state)
		return 0;
	/* (spawned again: it has not arrived anywhere yet) */
	if (!ae_bots_defend_unit_alive(bot))
		return 0;
	/* (fall back and regroup: the fall-back spot, spread on a ring; the places are for the hold and the return) */
	if (team->phase == AE_DEFEND_FALL_BACK || team->phase == AE_DEFEND_REGROUP)
	{
		real_point3d spot;

		if (!team->fallback_known)
			return 0;
		ae_bots_defend_fallback_spot(team, fireteam, bot_index, &spot);
		if (bots_distance2d(&spot, position) < 1.2f)
		{
			ae_bots_fireteam_note_hold(bot_index, now);
			return 2;
		}
		*goal = spot;
		*goal_surface = bots_surface_below(goal, goal);
		return *goal_surface == NONE ? 2 : 1;
	}
	ae_bots_defend_scan(team);
	if (!team->scan_done)
		return 0;
	if (state->place != NONE && state->place >= team->count)
		state->place = NONE;
	if (state->place != NONE)
	{
		real_point3d const *spot = &team->point[state->place];
		boolean there = bots_distance2d(spot, position) < AE_BOTS_DEFEND_THERE && fabs(spot->z - position->z) < 1.5f;

		if (there)
		{
			struct ae_bots_objective_snapshot const *snapshot = ae_bots_objective_snapshot();

			/* (a hill's place must be in the hill itself: the zone circle is a guess, the game's own test settles it on arrival) */
			if (team->zone && snapshot && snapshot->mode == AE_BOTS_MODE_KING && !ae_bots_objective_player_in_hill(bot->player_index))
			{
				error(_error_silent, "ae defend: off hill bot=%ld", bot_index + 1);
				state->why = "off hill";
				team->off_hill[state->place] = 1;
				ae_bots_defend_ban(state, state->place, now);
				state->place = NONE;
				state->arrived_tick = 0;
				state->pick_again_tick = now + TICKS_PER_SECOND;
				return 0;
			}
			if (!state->arrived_tick)
				state->arrived_tick = now;
			ae_bots_fireteam_note_hold(bot_index, now);
			return 2;
		}
		state->arrived_tick = 0;
		/* (a long walk from a spawn is no failure: the reach is timed from the bot's arrival near the area) */
		if (bots_distance2d(position, &team->anchor) > team->radius + AE_BOTS_DEFEND_NEAR_AREA)
			state->chosen_tick = MAX(state->chosen_tick, now - 1);
		/* (a bot stuck on the way is stuck wherever it is sent: a place is given up for it once it has had a few seconds to get
		free of the last one, and the next pick waits, so a stuck bot does not pick a new place every few ticks) */
		if (now - state->chosen_tick > AE_BOTS_DEFEND_REACH_TICKS ||
			(bot->stuck_count >= 2 && now - state->chosen_tick > AE_BOTS_DEFEND_STUCK_TICKS))
		{
			state->why = bot->stuck_count >= 2 ? "stuck" : "not reached";
			ae_bots_defend_ban(state, state->place, now);
			state->place = NONE;
			state->pick_again_tick = now + AE_BOTS_DEFEND_STUCK_TICKS;
		}
	}
	if (state->place == NONE && now >= state->pick_again_tick && team->scan_done)
	{
		real score = 0.0f;
		long place = ae_bots_defend_pick(bot, state, team, fireteam, bot_index, 0.0f, NULL, &score);

		state->pick_again_tick = now + (place == NONE ? TICKS_PER_SECOND : 0) + (long)(5 - level) * 3;
		if (place != NONE)
		{
			state->place = place;
			state->place_score = score;
			state->chosen_tick = now;
			state->arrived_tick = 0;
			error(_error_silent, "ae defend: place bot=%ld fireteam=%ld score=%.1f way=%d spaced=%.1f (%s)", bot_index + 1, fireteam,
				score, (int)state->way, (double)ae_bots_defend_nearest_held(team, fireteam, bot_index, &team->point[place]),
				state->why ? state->why : "first");
			state->why = NULL;
		}
		else if (now - state->log_tick >= 5 * TICKS_PER_SECOND)
		{
			state->log_tick = now;
			error(_error_silent, "ae defend: no place bot=%ld (%s)", bot_index + 1, team->scan_done ? "open" : "scanning");
		}
	}
	if (state->place == NONE)
		return 0;
	*goal = team->point[state->place];
	*goal_surface = bots_surface_below(goal, goal);
	return *goal_surface == NONE ? 0 : 1;
}

/* a role squad's hold gives way to the objective now: its own flag lying loose (to be returned), the enemy carrying it in sight
(to be killed), the planted bomb (to be defused). A leader's own order has its own window (ae_bots_fireteam_objective_window) */
static boolean ae_bots_defend_objective_break(struct bots_bot *bot, long fireteam, real_point3d const *position, char const **why)
{
	struct ae_bots_objective_snapshot const *snapshot = ae_bots_objective_snapshot();
	struct ae_bots_defend_team const *team;
	short own = ae_fireteam_team_of(fireteam);
	int dropped = 0, carried = 0, bomb = 0;

	if (fireteam < 0 || fireteam >= AE_FIRETEAM_MAX || !snapshot || own < 0 || own > 1)
		return FALSE;
	team = &ae_bots_defend_teams[fireteam];
	if (!team->active || !team->role_hold)
		return FALSE;
	if (snapshot->mode == AE_BOTS_MODE_CTF && snapshot->flags[own].exists)
	{
		struct ae_bots_objective_flag const *flag = &snapshot->flags[own];

		dropped = !flag->home && flag->carrier_player_index == NONE;
		if (flag->carrier_player_index != NONE)
		{
			struct player_datum const *carrier = player_try_and_get(flag->carrier_player_index);
			struct player_datum const *self = player_try_and_get(bot->player_index);

			if (carrier && self && self->unit_index != NONE && carrier->unit_index != NONE && carrier->team_index != own &&
				object_try_and_get(carrier->unit_index))
			{
				struct object_datum const *object = object_get(carrier->unit_index);

				if (bots_distance3d(position, &object->object.bounding_sphere_center) < AE_BOTS_COVER_SIGHT)
				{
					real_point3d eye;

					unit_get_head_position(self->unit_index, &eye);
					carried = bots_line_of_sight(self->unit_index, &eye, &object->object.bounding_sphere_center, carrier->unit_index);
				}
			}
		}
	}
	if (snapshot->mode == AE_BOTS_MODE_BOMB && ae_bomb_snd_on())
		bomb = snapshot->bomb_planted_goal != NONE && snapshot->bomb_attacking_team != own;
	if (!ae_defend_objective_break(1, dropped, carried, bomb))
		return FALSE;
	*why = dropped ? "own flag dropped" : carried ? "flag carrier in sight" : "bomb planted";
	return TRUE;
}

/* ---------- the hold in a fight (ae_bots_falls_guard) */

/* a bot's log lines about its hold: once a second at most */
static boolean ae_bots_defend_log_ready(struct ae_bots_defend_bot *state, long now)
{
	if (now - state->log_tick < TICKS_PER_SECOND && now >= state->log_tick)
		return FALSE;
	state->log_tick = now;
	return TRUE;
}

/* walk to the place the way the tether walks to its slot: the goal is the place, the path searched, no fight's chase */
static void ae_bots_defend_walk_to(struct bots_bot *bot, struct bots_tick *tick, real_point3d const *spot)
{
	real_point3d ground;
	long surface_index = bots_surface_below(spot, &ground);
	real direction;

	if (surface_index != NONE)
	{
		if (bot->goal_kind != _bots_goal_roam || bots_distance3d(&bot->goal_point, &ground) > 1.5f)
			bots_set_goal(bot, _bots_goal_roam, &ground, surface_index);
		if (!bot->path.valid && tick->now - bot->path_tick > TICKS_PER_SECOND / 3)
			bots_search(bot, tick->player->unit_index);
	}
	if (surface_index != NONE && bot->path.valid && bots_follow_path(bot, tick->player->unit_index, &tick->position, &direction))
		tick->move_direction = direction;
	else
		tick->move_direction = (real)atan2(spot->y - tick->position.y, spot->x - tick->position.x);
	tick->moving = TRUE;
	tick->throttle_scale = 1.0f;
}

/* turn the aim toward a yaw, a few degrees a tick (as the carriers' hold looks out) */
static void ae_bots_defend_look(struct bots_bot *bot, real yaw)
{
	real turn = 6.0f * BOTS_DEGREES, off = bots_angle_difference(yaw, bot->yaw);

	bot->yaw += PIN(off, -turn, turn);
}

/* in the open (no place, or none that covers it toward the enemy that sees it): never stand still. A direction inside the area,
changed every jink period; the odd crouch-walk and jump */
static void ae_bots_defend_jink(struct bots_bot *bot, struct bots_tick *tick, struct ae_bots_defend_bot *state,
	struct ae_bots_defend_team const *team, boolean target_known, real target_yaw)
{
	long now = tick->now;

	if (now >= state->jink_until)
	{
		real base, side = bots_random_real(bot) < 0.5f ? -1.0f : 1.0f;
		real_point3d next;
		int tries;

		/* (across the enemy's line when there is one: a target that has to follow it; else any way) */
		for (tries = 0; tries < 4; tries++)
		{
			base = target_known ? target_yaw + side * (BOTS_PI * 0.5f + bots_random_signed(bot) * 0.6f) :
				bots_random_real(bot) * 2.0f * BOTS_PI;
			next = tick->position;
			next.x += (real)cos(base) * 3.0f;
			next.y += (real)sin(base) * 3.0f;
			if (bots_distance2d(&next, &team->anchor) <= team->radius)
				break;
			side = -side;
			if (tries == 3)
				base = (real)atan2(team->anchor.y - tick->position.y, team->anchor.x - tick->position.x);
		}
		state->jink_direction = base;
		state->jink_until = now + ae_defend_jink_ticks(team->profile, bots_random_real(bot));
		/* (the odd jump, and a crouch-walk a quarter of the changes) */
		if (bots_random_real(bot) < 0.12f)
			tick->flags |= FLAG(_unit_control_jump_bit);
		state->crouch_walk = bots_random_real(bot) < 0.25f;
	}
	tick->moving = TRUE;
	tick->move_direction = state->jink_direction;
	tick->throttle_scale = state->crouch_walk ? 0.55f : 1.0f;
	if (state->crouch_walk)
		tick->flags |= FLAG(_unit_control_crouch_modifier_bit);
}

/* (ae_bots_falls_guard, before the dodge) the hold of a defender: TRUE when the bot is one (its fight's move is the hold's, the
squads' roles leave it alone). A defender fights from its place, steps out and back, never chases, keeps moving where nothing
covers it, and stays inside the area and its leash */
static boolean ae_bots_defend_hold(struct bots_bot *bot, struct bots_tick *tick)
{
	long bot_index = (long)(bot - bots_globals.bots), fireteam, now = tick->now;
	struct ae_bots_defend_team *team;
	struct ae_bots_defend_bot *state;
	struct ae_defend_numbers numbers;
	int level;
	real anchor_distance, target_yaw = 0.0f, target_distance = 0.0f;
	boolean target_known = FALSE, has_place, at_place = FALSE, covered = FALSE, shooting;
	char const *why = NULL;
	unsigned threat = 0u;

	if (!ae_bots_defend_on() || bot_index < 0 || bot_index >= BOTS_BRAINS || !tick->object ||
		tick->object->object.parent_object_index != NONE)
	{
		return FALSE;
	}
	fireteam = ae_fireteam_of_brain(bot_index);
	if (fireteam == NONE || ae_fireteam_order_of(fireteam) != _ae_order_defend || !ae_bots_defend_scope(fireteam, _ae_order_defend))
		return FALSE;
	if (ae_fireteam_order_explicit_of(fireteam) && ae_bots_fireteam_objective_window(fireteam))
		return FALSE;
	if (ae_bots_vehicle_carrying(bot, bot_index) || ae_bots_objective_player_carries(ae_bots_objective_snapshot(), bot->player_index))
		return FALSE;
	/* (a recovery from being stuck is the unstick's step, and a downed teammate's orb the revive's call: not overwritten) */
	if (tick->now < bot->unstick_until_tick || ae_bots_round_revive_pending(bot))
		return FALSE;
	ae_bots_defend_check_clock();
	team = ae_bots_defend_team_for(fireteam);
	if (!team)
		return FALSE;
	state = ae_bots_defend_bot_state(bot, fireteam, team);
	if (!state)
		return FALSE;
	if (ae_bots_defend_objective_break(bot, fireteam, &tick->position, &why))
	{
		if (ae_bots_defend_log_ready(state, now))
			error(_error_silent, "ae defend: objective break bot=%ld why=%s", bot_index + 1, why);
		return FALSE;
	}
	level = ae_bots_level_of_skill(bots_globals.skill);
	ae_defend_numbers_of(team->profile, &numbers);
	anchor_distance = bots_distance2d(&tick->position, &team->anchor);
	/* FALLING BACK AND REGROUPING: all together to the fall-back spot, shooting on the way (a walk with no fight is the goal's) */
	if (team->phase == AE_DEFEND_FALL_BACK || team->phase == AE_DEFEND_REGROUP)
	{
		real_point3d spot;

		if (team->fallback_known)
		{
			ae_bots_defend_fallback_spot(team, fireteam, bot_index, &spot);
			if (bots_distance2d(&spot, &tick->position) >= 1.2f)
			{
				if (tick->fighting)
					ae_bots_defend_walk_to(bot, tick, &spot);
			}
			else
			{
				tick->moving = FALSE;
				ae_bots_fireteam_note_hold(bot_index, now);
				if (level >= AE_BOTS_LEVEL_NORMAL)
					tick->flags |= FLAG(_unit_control_crouch_modifier_bit);
			}
		}
		return TRUE;
	}
	has_place = state->place != NONE && state->place < team->count;
	if (has_place)
	{
		real_point3d const *spot = &team->point[state->place];

		at_place = bots_distance2d(spot, &tick->position) < AE_BOTS_DEFEND_THERE && fabs(spot->z - tick->position.z) < 1.5f;
		if (at_place && !state->arrived_tick)
			state->arrived_tick = now;
		else if (!at_place && state->arrived_tick && bots_distance2d(spot, &tick->position) > 2.0f)
			state->arrived_tick = 0;
	}
	/* (the enemy it fights or last saw: three seconds of watching it, ten of knowing where) */
	if (bot->target_player_index != NONE && now - bot->target_seen_tick < 10 * TICKS_PER_SECOND)
	{
		target_known = TRUE;
		target_yaw = (real)atan2(bot->target_last_position.y - tick->position.y, bot->target_last_position.x - tick->position.x);
		target_distance = bots_distance3d(&tick->position, &bot->target_last_position);
		threat = ae_defend_threat_sides(&target_yaw, 1);
		covered = has_place && (team->walls[state->place] & threat) != 0u;
	}
	shooting = tick->fighting;
	/* (the exposure, looked at now and then: who sees it, hurt, open toward them) */
	if (now >= state->watch_tick && (shooting || target_known) && team->scan_done)
	{
		real_point3d eye;
		boolean hurt = now - bot->hurt_tick < TICKS_PER_SECOND && bot->hurt_tick > 0;
		real exposure;
		boolean was = state->exposed;

		unit_get_head_position(tick->player->unit_index, &eye);
		state->seen_by = ae_bots_cover_seen(tick->player, &eye, 8.0f, &state->seen_by_near);
		state->watch_tick = now + ae_bots_cover_watch_ticks(level);
		exposure = ae_defend_exposure(state->seen_by, state->seen_by_near, hurt, !covered);
		state->exposed = ae_defend_exposed_step(was, exposure, at_place ? now - state->arrived_tick : 0, level) != 0;
		if (state->exposed && !was)
		{
			long place = NONE;
			real score = 0.0f;

			/* (moved to a better place when one beats this one with the sticky bonus; else it stays and fights) */
			if (has_place)
			{
				ae_bots_defend_ban(state, state->place, now);
				place = ae_bots_defend_pick(bot, state, team, fireteam, bot_index,
					ae_defend_sticky(state->place_score, now - state->chosen_tick, level), target_known ? &target_yaw : NULL, &score);
				if (place != NONE)
				{
					state->place = place;
					state->place_score = score;
					state->chosen_tick = now;
					state->arrived_tick = 0;
					state->why = "exposed";
					has_place = TRUE;
					at_place = FALSE;
				}
			}
			if (now - state->exposed_log_tick >= TICKS_PER_SECOND || now < state->exposed_log_tick)
			{
				state->exposed_log_tick = now;
				error(_error_silent, "ae defend: exposed bot=%ld value=%.0f moved=%d", bot_index + 1, (double)exposure, place != NONE);
			}
		}
	}
	else if (!shooting && !target_known)
	{
		state->exposed = FALSE;
		state->seen_by = state->seen_by_near = 0;
	}
	/* IN THE OPEN: no place, or none that covers it toward the one who sees it */
	if (anchor_distance > team->radius + numbers.leash + AE_BOTS_DEFEND_NEAR_AREA || !team->scan_done)
	{
		/* (still on its way to the area, or the places not found yet: the walk to the point is the goal's) */
		return TRUE;
	}
	if (!has_place || (shooting && state->seen_by > 0 && !covered))
	{
		int action = AE_DEFEND_OPEN_JINK;

		if (state->seen_by > 0 && target_known && target_distance > 20.0f / numbers.sniper_weight && team->scan_done)
		{
			/* (shot from range: a place inside the area that breaks that line, toward the shooter) */
			real score = 0.0f;
			long place = NONE;

			/* (looked for twice a second at most, and a place must beat the held one with its sticky bonus) */
			if (now >= state->pick_again_tick)
			{
				state->pick_again_tick = now + TICKS_PER_SECOND / 2;
				place = ae_bots_defend_pick(bot, state, team, fireteam, bot_index,
					has_place ? ae_defend_sticky(state->place_score, now - state->chosen_tick, level) : 0.0f, &target_yaw, &score);
			}
			action = ae_defend_open_action(team->profile, TRUE, place != NONE);
			if (action == AE_DEFEND_OPEN_BREAK_LINE)
			{
				state->place = place;
				state->place_score = score;
				state->chosen_tick = now;
				state->arrived_tick = 0;
				state->why = "break line";
				has_place = TRUE;
				at_place = FALSE;
			}
		}
		if (ae_bots_defend_log_ready(state, now))
			error(_error_silent, "ae defend: open bot=%ld action=%s", bot_index + 1, action == AE_DEFEND_OPEN_JINK ? "jink" : "break line");
		if (action == AE_DEFEND_OPEN_JINK)
		{
			real_point3d floor;
			long under = bots_unit_surface(tick->player->unit_index, &floor);
			boolean on_floor = team->piece == NONE || (under != NONE && ae_travel_piece(under) == team->piece);

			if (!on_floor || anchor_distance > team->radius + numbers.leash || fabs(tick->position.z - team->anchor.z) > AE_BOTS_COVER_HEIGHT)
			{
				/* (past the leash, or on another floor or piece than the point: back by a path, not through the walls) */
				if (ae_bots_defend_log_ready(state, now))
					error(_error_silent, "ae defend: leash bot=%ld distance=%.1f", bot_index + 1, (double)anchor_distance);
				ae_bots_defend_walk_to(bot, tick, &team->anchor);
			}
			else
				ae_bots_defend_jink(bot, tick, state, team, target_known, target_yaw);
			if (target_known && now - bot->target_seen_tick < 3 * TICKS_PER_SECOND)
				ae_bots_defend_look(bot, target_yaw);
			return TRUE;
		}
	}
	/* AT ITS PLACE: it holds there, steps out along the open side and back, and watches */
	if (has_place && at_place)
	{
		real_point3d const *spot = &team->point[state->place];
		real from_spot = bots_distance2d(spot, &tick->position);
		int side = ae_bots_cover_open_side(team->walls[state->place]);
		long phase = (now + bot_index * 11) % 60;

		tick->moving = FALSE;
		ae_bots_fireteam_note_hold(bot_index, now);
		if (shooting && side >= 0)
		{
			real out = (real)side * 2.0f * BOTS_PI / (real)AE_BOTS_COVER_SIDES;

			/* (out for a moment, back in for longer: the peek) */
			if (phase < 18 && from_spot < 1.2f)
			{
				tick->moving = TRUE;
				tick->move_direction = out;
				tick->throttle_scale = 0.6f;
				if (state->peek_logged != state->place + 1)
				{
					state->peek_logged = state->place + 1;
					error(_error_silent, "ae defend: peek bot=%ld place=%ld side=%d", bot_index + 1, state->place, side);
				}
			}
			else if (phase >= 36 && from_spot > 0.25f)
			{
				tick->moving = TRUE;
				tick->move_direction = (real)atan2(spot->y - tick->position.y, spot->x - tick->position.x);
				tick->throttle_scale = 0.6f;
			}
		}
		if (level >= AE_BOTS_LEVEL_NORMAL && (!shooting || phase >= 18))
			tick->flags |= FLAG(_unit_control_crouch_modifier_bit);
		/* (the one it lost is watched for three seconds, then its way in) */
		if (!shooting)
		{
			if (target_known && now - bot->target_seen_tick < 3 * TICKS_PER_SECOND)
				ae_bots_defend_look(bot, target_yaw);
			else if (team->way_count > 0)
				ae_bots_defend_look(bot, team->way_yaw[state->way % team->way_count]);
		}
		return TRUE;
	}
	/* TO ITS PLACE: a fight does not chase, it walks to the place (a walk with no fight is the goal's) */
	if (has_place && shooting)
	{
		ae_bots_defend_walk_to(bot, tick, &team->point[state->place]);
	}
	return TRUE;
}
