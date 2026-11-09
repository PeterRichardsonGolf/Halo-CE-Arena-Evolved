/*
AE_BOTS_POWER.INC

Included by ae_bots_lan.inc, so at the end of bots.c: the bots go for power items as a player who knows the timers
does (ae_bots_power_rules.h says how far and how early, by level). The items are item_timers.c's power entries (where
each spawns and when); bots_choose_goal asks first (its AE hook line, after the vehicles' and the objectives'):

- once a second a bot weighs every entry (its want, ae_bots_power_want) and goes for the most wanted: to the item
  itself when it lies there (a weapon as bots.c's own item goal, pressed for, so it is picked up even with two held),
  else to its spawn point to be there as it spawns;
- one bot of a team per entry (the others leave it to it);
- a bot that reaches an empty spot while the entry is not about to spawn takes it as taken, for its team, until the
  entry's next spawn;
- a bot never goes for a weapon it holds, nor for the overshield or the camouflage while it wears it;
- an entry out of its reach (bots.c found no whole path to it: a short one, or the weapon left be as unreachable) is
  left alone by that bot for 30 s, as bots.c leaves an item (the soak 10-20: on Beaver Creek, whose snipers and
  rockets lie on the arches, bots stood under them for minutes, searching a path each tick); so is one whose item is
  still there after 5 s at its spot (a bot stood under Temple's camouflage to the game's end). A short path that ends
  beside the spot, a jump below it (a stand: Temple's camouflage and overshield), is not out of reach: bots.c's climb
  at the path's end jumps on; at the spot, an item a step above the bot's feet starts that climb too. Three entries left out of its reach within 20 s leave them all alone 30 s: where the bot
  stands reaches none (Boarding Action 10-20, once the bots no longer walked off the ships).
*/

#include "ae_bots_power_rules.h"
#include "game/item_timers.h"

#define AE_BOTS_POWER_ENTRIES 256 /* (item_timers.c's MAXIMUM_ITEM_TIMERS: Ice Fields' flamethrowers are entries 39 to 95) */

static struct
{
	short entry;		/* the item_timers entry it goes for, or NONE */
	long check_tick;	/* when it weighs the entries again */
	long near_since;	/* since when it stands at the entry's spot with the item up (0: not) */
	/* the entries it left out of its reach lately: how many, the first's tick */
	short unreachable_run;
	long unreachable_run_tick;
} ae_bots_power_bots[BOTS_BRAINS];

/* when each team saw each entry's spot empty, until its next spawn (game ticks; 0 for not) */
static long ae_bots_power_taken_until[AE_BOTS_POWER_ENTRIES][2];

/* until when each bot leaves each entry alone, out of its reach (game ticks; 0 for not) */
static long ae_bots_power_unreachable_until[BOTS_BRAINS][AE_BOTS_POWER_ENTRIES];

static long ae_bots_power_bot_index(struct bots_bot const *bot)
{
	return (long)(bot - bots_globals.bots);
}

/* the item of the entry lying at its spawn point now (within 2 units, on the map, not held), or NONE */
static long ae_bots_power_item_at(struct item_timer const *timer)
{
	struct object_iterator iterator;
	short kind;

	object_iterator_new(&iterator, _object_mask_weapon | _object_mask_equipment, 0);
	while (object_iterator_next(&iterator))
	{
		struct object_datum const *object = object_get(iterator.index);

		if (object->object.parent_object_index != NONE ||
			!TEST_FLAG(object->object.flags, _object_connected_to_map_bit) ||
			bots_distance3d(&object->object.position, &timer->position) > 2.0f)
		{
			continue;
		}
		for (kind = 0; kind < NUMBER_OF_ITEM_TIMER_POWER_CLASSES; kind++)
		{
			if (TEST_FLAG(timer->classes, kind) && timer->class_definitions[kind] == object->definition_index)
				return iterator.index;
		}
	}
	return NONE;
}

/* (Codex review 2026-11-01: with 256 entries every bot scanned every weapon and equipment object for each entry once a
second, and for its chosen one every tick) an entry's item, found once a tick and shared by every bot */
static long ae_bots_power_item_tick[AE_BOTS_POWER_ENTRIES];
static long ae_bots_power_item_index[AE_BOTS_POWER_ENTRIES];

static long ae_bots_power_item_of(short entry)
{
	long now = game_time_get();

	if (entry < 0 || entry >= AE_BOTS_POWER_ENTRIES || !item_timers_get(entry))
		return NONE;
	/* (stored as tick + 1: zero is "never") */
	if (ae_bots_power_item_tick[entry] != now + 1)
	{
		ae_bots_power_item_tick[entry] = now + 1;
		ae_bots_power_item_index[entry] = ae_bots_power_item_at(item_timers_get(entry));
	}
	return ae_bots_power_item_index[entry];
}

/* the kind the bot would get from the entry (the spawned item's, else the first it can spawn), or NONE */
static short ae_bots_power_entry_kind(struct item_timer const *timer, long item_index)
{
	short kind;

	for (kind = 0; kind < NUMBER_OF_ITEM_TIMER_POWER_CLASSES; kind++)
	{
		if (!TEST_FLAG(timer->classes, kind))
			continue;
		if (item_index == NONE || timer->class_definitions[kind] == object_get(item_index)->definition_index)
			return kind;
	}
	return NONE;
}

/* whether the bot holds the kind already: the weapon, or the overshield's or the camouflage's effect */
static boolean ae_bots_power_holds(struct unit_datum *unit, struct item_timer const *timer, short kind)
{
	short slot;

	if (kind == AE_BOTS_POWER_OVERSHIELD)
		return (boolean)(unit->object.shield_vitality > 1.0f);
	if (kind == AE_BOTS_POWER_CAMO)
		return (boolean)TEST_FLAG(unit->unit.flags, _unit_active_camouflaged_bit);
	for (slot = 0; slot < MAXIMUM_WEAPONS_PER_UNIT; slot++)
	{
		long weapon = unit->unit.weapon_object_indices[slot];

		if (weapon != NONE && weapon_get(weapon)->definition_index == timer->class_definitions[kind])
			return TRUE;
	}
	return FALSE;
}

/* whether another bot of the team goes for the entry */
static boolean ae_bots_power_teammate_going(struct bots_bot const *bot, struct player_datum const *player, short entry)
{
	long index;

	for (index = 0; index < BOTS_BRAINS; index++)
	{
		struct bots_bot const *other = &bots_globals.bots[index];
		struct player_datum const *other_player;

		if (other == bot || other->player_index == NONE || ae_bots_power_bots[index].entry != entry ||
			(other_player = player_try_and_get(other->player_index)) == NULL)
		{
			continue;
		}
		if (other_player->team_index == player->team_index)
			return TRUE;
	}
	return FALSE;
}

/* whether a point (an item's, or an entry's spawn point) hangs out of a bot's reach above the ground below it (the
rule: ae_bots_travel_item_hung); a point with no ground below is not judged */
static boolean ae_bots_hung_at(real_point3d const *point)
{
	real_point3d ground;

	return (boolean)(bots_surface_below(point, &ground) != NONE && ae_bots_travel_item_hung(point->z - ground.z));
}

/* (bots_find_item) whether the item hangs out of reach: no goal, however near (Digsite's workshop maps hang theirs
to be shot down) */
static boolean ae_bots_item_hung(long item_index)
{
	return ae_bots_hung_at(&object_get(item_index)->object.position);
}

/* whether the entry the bot goes for is out of its reach: its weapon left be by bots.c, or the path to its spot short */
static boolean ae_bots_power_out_of_reach(struct bots_bot const *bot, short entry)
{
	struct item_timer const *timer = item_timers_get(entry);
	long item_index;

	if (!timer)
		return FALSE;
	item_index = ae_bots_power_item_of(entry);
	if (item_index != NONE && bots_item_unreachable(bot, item_index))
		return TRUE;
	if (!bot->path.valid || !bot->path_short || bots_distance2d(&bot->goal_point, &timer->position) >= 2.5f)
		return FALSE;
	/* (a short path that ends beside the spot, a jump below it: a stand, as Temple's camouflage's and overshield's,
	which bots.c's climb at the path's end (jumps towards the goal) takes; the stall rule catches a miss) */
	return (boolean)(bots_distance2d(&bot->path.endpoint.point, &timer->position) > 1.5f ||
		timer->position.z - bot->path.endpoint.point.z > 1.2f);
}

/* MOUNTING a stand (0.7.4; ae_bots_power_rules.h): a bot with a power entry whose item (or its spawn point, while it is
not up) lies a step above its feet goes to a staging point on the stand's nearest axis with ground at its own height,
runs at the item along the axis, and jumps once, in the lane and moving at it; one that comes down on the floor takes the
next axis. On the stand it walks on to the item. The bot's climb (bots.c, jumping every 20 ticks wherever it was, aimed
at the path's last step) is not used for these. */
struct ae_bots_power_mount_state
{
	short entry;
	long unit_index;
	short shift;		/* quarter turns of the axis, a miss each */
	long jump_tick;		/* when it jumped for the stand (0: not) */
	boolean counted;	/* that jump's end counted (on the stand, or a miss) */
	long engaged_tick;	/* the last tick it walked the bot to the staging point or along the lane */
	long seen_unstick;	/* the stuck turn (bots.c's unstick_until_tick) it turned the axis for */
	short still_ticks;	/* ticks it was told to walk to the staging point and did not move */
	real_point3d lock;	/* where the bot was when the axis was taken: it keeps to the axis wherever it walks, */
	boolean locked;		/* and a miss or a stuck turn takes the next (shift) */
};

static struct ae_bots_power_mount_state ae_bots_power_mounts[BOTS_BRAINS];

/* the place the axis is taken from: the bot's where it first looked, until the axis is turned */
static real_point3d const *ae_bots_power_mount_from(struct ae_bots_power_mount_state *mount, real_point3d const *position)
{
	if (!mount->locked)
	{
		mount->lock = *position;
		mount->locked = TRUE;
	}
	return &mount->lock;
}

/* (ae_bots_combat.inc) whether the bot is in the jump it made onto a stand: it crouches at the top of it in a fight too */
static boolean ae_bots_power_mount_in_flight(long bot_index, long now)
{
	return (boolean)(bot_index >= 0 && bot_index < BOTS_BRAINS && ae_bots_power_mounts[bot_index].jump_tick &&
		now - ae_bots_power_mounts[bot_index].jump_tick < 60);
}

/* (ae_bots_falls.inc) whether the bot pressed the jump for a stand this tick */
static boolean ae_bots_power_mount_jumped(long bot_index, long now)
{
	return (boolean)(bot_index >= 0 && bot_index < BOTS_BRAINS && ae_bots_power_mounts[bot_index].jump_tick == now);
}

/* whether the ground along the lane from the item steps up (0.3 or more in 0.25 units, or no ground at all): a stand's
face or its bevel, not a ramp's slope; sampled at 0.25 to 1.25 units out */
static boolean ae_bots_power_mount_is_stand(real_point3d const *target, real axis_x, real axis_y)
{
	real previous = target->z;
	short step;

	for (step = 1; step <= 5; step++)
	{
		real_point3d point, ground;

		point.x = target->x + axis_x * 0.25f * (real)step;
		point.y = target->y + axis_y * 0.25f * (real)step;
		point.z = target->z + 0.3f;
		ground = point;
		if (bots_surface_below(&point, &ground) == NONE)
			return TRUE;
		if (fabs(ground.z - previous) >= AE_BOTS_POWER_MOUNT_STEP)
			return TRUE;
		previous = ground.z;
	}
	return FALSE;
}

struct ae_bots_power_mount_axis
{
	short axis;
	real top;
	real turn;
	real x, y;
	real_point3d ground;
	long surface;
};

/* how far the stand's top runs from the item along an axis (0.2 steps up to 1.6 units): a ridge is long one way and
narrow the other, and a bot lands where there is room */
static float ae_bots_power_mount_top_run(real_point3d const *target, real axis_x, real axis_y)
{
	short step;

	for (step = 1; step <= 8; step++)
	{
		real_point3d point, ground;

		point.x = target->x + axis_x * 0.2f * (real)step;
		point.y = target->y + axis_y * 0.2f * (real)step;
		point.z = target->z + 0.3f;
		ground = point;
		if (bots_surface_below(&point, &ground) == NONE || ground.z < target->z - 0.2f)
			return 0.2f * (real)(step - 1);
	}
	return 1.6f;
}

/* the staging point for a stand: of the eight axes (45 degrees apart) those whose staging point has the ground of a
stand's foot, the one with the most top to land on first (the bot's own side first among the like), and from the second
on after a miss (shift); its ground and surface, or FALSE for none */
static boolean ae_bots_power_mount_stage(real_point3d const *target, real_point3d const *from, short shift, float *x, float *y,
	real_point3d *ground, long *surface_index)
{
	struct ae_bots_power_mount_axis found[8];
	short count = 0, axis, a, b;
	real bearing = (real)atan2(from->y - target->y, from->x - target->x);

	for (axis = 0; axis < 8; axis++)
	{
		real angle = (real)axis * (real)(3.14159265358979 / 4.0);
		real axis_x = (real)cos(angle), axis_y = (real)sin(angle), turn;
		real_point3d point, floor_point;
		long surface;
		real rise;

		point.x = target->x + axis_x * AE_BOTS_POWER_MOUNT_STAGING;
		point.y = target->y + axis_y * AE_BOTS_POWER_MOUNT_STAGING;
		point.z = target->z + 0.3f;
		floor_point = point;
		surface = bots_surface_below(&point, &floor_point);
		rise = target->z - floor_point.z;
		if (surface == NONE || rise < AE_BOTS_POWER_MOUNT_RISE_MIN || rise > AE_BOTS_POWER_MOUNT_RISE_MAX)
			continue;
		turn = (real)fabs(angle - bearing);
		while (turn > 3.14159265358979f)
			turn = (real)fabs(turn - 2.0f * 3.14159265358979f);
		found[count].axis = axis;
		found[count].top = ae_bots_power_mount_top_run(target, axis_x, axis_y);
		found[count].turn = turn;
		found[count].x = point.x;
		found[count].y = point.y;
		found[count].ground = floor_point;
		found[count].surface = surface;
		count++;
	}
	if (count <= 0)
		return FALSE;
	/* (a longer top first; within 0.3 of each other the nearer to the bot's own side) */
	for (a = 1; a < count; a++)
	{
		for (b = a; b > 0 && (found[b].top > found[b - 1].top + 0.3f ||
			(found[b].top > found[b - 1].top - 0.3f && found[b].turn < found[b - 1].turn)); b--)
		{
			struct ae_bots_power_mount_axis swap = found[b];

			found[b] = found[b - 1];
			found[b - 1] = swap;
		}
	}
	a = (short)(((shift % count) + count) % count);
	*x = found[a].x;
	*y = found[a].y;
	*ground = found[a].ground;
	*surface_index = found[a].surface;
	return TRUE;
}

static boolean ae_bots_power_goal(struct bots_bot *bot, long player_index, real_point3d const *position,
	boolean fighting)
{
	long index = ae_bots_power_bot_index(bot);
	long now = game_time_get();
	struct player_datum *player = player_get(player_index);
	struct unit_datum *unit = unit_get(player->unit_index);
	short team = (short)(player->team_index == 1 ? 1 : 0);
	short count = item_timers_count();
	short entry, best = NONE;
	long best_item = NONE;
	real best_want = 0.0f;
	int level = ae_bots_level_of_skill(bots_globals.skill);

	if (index < 0 || index >= BOTS_BRAINS || count <= 0)
		return FALSE;
	if (count > AE_BOTS_POWER_ENTRIES)
		count = AE_BOTS_POWER_ENTRIES;
	/* (the entry it goes for, out of its reach: left alone 30 s, and the entries weighed again now) */
	if (ae_bots_power_bots[index].entry != NONE && ae_bots_power_bots[index].entry < AE_BOTS_POWER_ENTRIES &&
		ae_bots_power_out_of_reach(bot, ae_bots_power_bots[index].entry))
	{
		ae_bots_power_unreachable_until[index][ae_bots_power_bots[index].entry] = now + TICKS_PER_SECOND * 30;
		if (ae_bots_events_on())
			platform_log("bots: bot %ld leaves power entry %d (out of reach)", (long)(index + 1),
				(int)ae_bots_power_bots[index].entry);
		/* (three left within 20 s: where it stands reaches none of them, so none for 30 s; bots.c's roaming and
		hunting move it on. Boarding Action 10-20: a bot on one ship went for the other ship's items one after the
		other, each out of reach, and stood climbing at the hull a minute or more; it used to walk off and respawn) */
		if (now - ae_bots_power_bots[index].unreachable_run_tick > TICKS_PER_SECOND * 20 ||
			now < ae_bots_power_bots[index].unreachable_run_tick)
		{
			ae_bots_power_bots[index].unreachable_run = 0;
			ae_bots_power_bots[index].unreachable_run_tick = now;
		}
		if (++ae_bots_power_bots[index].unreachable_run >= 3)
		{
			for (entry = 0; entry < AE_BOTS_POWER_ENTRIES; entry++)
				ae_bots_power_unreachable_until[index][entry] = now + TICKS_PER_SECOND * 30;
			ae_bots_power_bots[index].unreachable_run = 0;
			if (ae_bots_events_on())
				platform_log("bots: bot %ld leaves the power entries a while (none in reach)", (long)(index + 1));
		}
		ae_bots_power_bots[index].entry = NONE;
		ae_bots_power_bots[index].check_tick = now;
		if (bot->goal_kind == _bots_goal_item || bot->goal_kind == _bots_goal_roam)
			bot->goal_kind = _bots_goal_none;
	}
	/* (the entry it goes for: kept between the once-a-second weighings while it is still wanted) */
	if (now < ae_bots_power_bots[index].check_tick && ae_bots_power_bots[index].entry == NONE)
		return FALSE;
	if (now >= ae_bots_power_bots[index].check_tick)
	{
		short previous = ae_bots_power_bots[index].entry;

		ae_bots_power_bots[index].check_tick = now + TICKS_PER_SECOND;
		ae_bots_power_bots[index].entry = NONE;
		for (entry = 0; entry < count; entry++)
		{
			struct item_timer const *timer = item_timers_get(entry);
			struct ae_bots_power_view view;
			long item_index;
			short kind;
			real want;

			if (!timer || timer->timer_class >= NUMBER_OF_ITEM_TIMER_POWER_CLASSES || timer->period_ticks <= 0 ||
				now < ae_bots_power_unreachable_until[index][entry])
			{
				continue;
			}
			item_index = ae_bots_power_item_of(entry);
			kind = ae_bots_power_entry_kind(timer, item_index);
			if (kind == NONE)
				continue;
			/* (an item hung in the air, or a spawn point there: shot down by a player, no goal for a bot) */
			if (ae_bots_hung_at(item_index != NONE ? &object_get(item_index)->object.position : &timer->position))
				continue;
			view.level = level;
			view.kind = kind;
			view.distance = (float)bots_distance3d(position, &timer->position);
			view.seconds_left = (float)item_timer_ticks_left(timer) / (float)TICKS_PER_SECOND;
			view.up = item_index != NONE;
			view.known_taken = !view.up && now < ae_bots_power_taken_until[entry][team];
			view.fighting = fighting;
			view.holds_it = ae_bots_power_holds(unit, timer, kind);
			view.teammate_going = ae_bots_power_teammate_going(bot, player, entry);
			/* (a wingman leaves its player only for an item within 15 units: ae_bots_team.inc) */
			if (view.distance > 15.0f && ae_bots_team_wingman_of(bot) != NONE)
				continue;
			want = ae_bots_power_want(&view);
			if (want > best_want)
			{
				best_want = want;
				best = entry;
				best_item = item_index;
			}
		}
		if (best != NONE && best != previous && ae_bots_events_on())
		{
			wchar_t const *label = item_timers_get(best)->label;
			char name[24];
			short letter;

			/* (the power list's label, in ASCII: platform_log has no wide strings) */
			for (letter = 0; letter < 23 && label[letter]; letter++)
				name[letter] = label[letter] < 128 ? (char)label[letter] : '?';
			name[letter] = 0;
			platform_log("bots: bot %ld goes for %s (%s, %.0f m)", (long)(index + 1), name,
				best_item != NONE ? "up" : "spawning", best_want > 0.0f ? (double)bots_distance3d(position,
				&item_timers_get(best)->position) * 3.048 : 0.0);
		}
		ae_bots_power_bots[index].entry = best;
	}
	else
	{
		best = ae_bots_power_bots[index].entry;
		best_item = ae_bots_power_item_of(best);
	}
	if (best == NONE)
		return FALSE;
	{
		struct item_timer const *timer = item_timers_get(best);
		real_point3d ground;
		long surface_index;

		/* (at an empty spot that is not about to spawn: taken, for the team, until its next spawn) */
		/* (at the spot with the item up, 5 s and not got: out of its reach, 30 s) */
		if (best_item != NONE && bots_distance2d(position, &timer->position) < 1.5f)
		{
			/* (an item on a stand, a step above the bot's feet: Temple's camouflage and overshield, 0.75 units up. The
			path's last step is on the stand's top, which the bot walked into without jumping: bots.c's climb, as at a
			short path's end, takes it on, walking at the goal and jumping) */
			if (object_get(best_item)->object.position.z - position->z > 0.3f &&
				bots_distance2d(position, &object_get(best_item)->object.position) > 0.3f && !bot->climb_until_tick)
			{
				bot->climb_start_tick = now;
				bot->climb_until_tick = now + TICKS_PER_SECOND * 3;
				bot->climb_best_distance = bots_distance3d(&bot->goal_point, position);
			}
			if (!ae_bots_power_bots[index].near_since)
				ae_bots_power_bots[index].near_since = now;
			else if (now - ae_bots_power_bots[index].near_since > TICKS_PER_SECOND * 5 && best < AE_BOTS_POWER_ENTRIES)
			{
				ae_bots_power_unreachable_until[index][best] = now + TICKS_PER_SECOND * 30;
				if (ae_bots_events_on())
					platform_log("bots: bot %ld leaves power entry %d (not got at its spot)", (long)(index + 1), (int)best);
				ae_bots_power_bots[index].entry = NONE;
				ae_bots_power_bots[index].near_since = 0;
				bot->goal_kind = _bots_goal_none;
				return FALSE;
			}
		}
		else
			ae_bots_power_bots[index].near_since = 0;
		if (best_item == NONE && bots_distance2d(position, &timer->position) < 1.5f &&
			item_timer_ticks_left(timer) > (long)((ae_bots_power_lead_seconds(level) + 2.0f) * TICKS_PER_SECOND))
		{
			if (best < AE_BOTS_POWER_ENTRIES)
				ae_bots_power_taken_until[best][team] = now + item_timer_ticks_left(timer);
			ae_bots_power_bots[index].entry = NONE;
			return FALSE;
		}
		surface_index = bots_surface_below(best_item != NONE ? &object_get(best_item)->object.position : &timer->position,
			&ground);
		if (surface_index == NONE)
		{
			ae_bots_power_bots[index].entry = NONE;
			return FALSE;
		}
		/* (a weapon lying there: bots.c's item goal, which presses for it; else to the spot) */
		if (best_item != NONE && object_get(best_item)->object.type == _object_type_weapon)
		{
			bots_set_goal(bot, _bots_goal_item, &ground, surface_index);
			bot->item_index = best_item;
		}
		else
			bots_set_goal(bot, _bots_goal_roam, &ground, surface_index);
	}
	return TRUE;
}

/* (a map's start) every bot's entry and every team's taken spots forgotten */
static void ae_bots_power_reset(void)
{
	long index;

	for (index = 0; index < BOTS_BRAINS; index++)
	{
		ae_bots_power_bots[index].entry = NONE;
		ae_bots_power_bots[index].check_tick = 0;
		ae_bots_power_bots[index].near_since = 0;
	}
	csmemset(ae_bots_power_taken_until, 0, sizeof(ae_bots_power_taken_until));
	csmemset(ae_bots_power_unreachable_until, 0, sizeof(ae_bots_power_unreachable_until));
}

static void ae_bots_power_mount(struct bots_bot *bot, struct bots_tick *tick)
{
	long index = ae_bots_power_bot_index(bot);
	struct ae_bots_power_mount_state *mount;
	struct item_timer const *timer;
	struct biped_datum *biped;
	real_point3d target;
	long item_index, entry;
	real across, rise;
	boolean airborne;

	if (index < 0 || index >= BOTS_BRAINS || !tick->object || tick->object->object.parent_object_index != NONE)
		return;
	mount = &ae_bots_power_mounts[index];
	entry = ae_bots_power_bots[index].entry;
	if (entry == NONE || entry >= AE_BOTS_POWER_ENTRIES || !(timer = item_timers_get((short)entry)))
	{
		mount->entry = NONE;
		return;
	}
	if (mount->entry != entry || mount->unit_index != tick->player->unit_index)
	{
		mount->entry = (short)entry;
		mount->unit_index = tick->player->unit_index;
		mount->shift = 0;
		mount->jump_tick = 0;
		mount->counted = TRUE;
		mount->engaged_tick = 0;
		mount->seen_unstick = 0;
		mount->still_ticks = 0;
		mount->locked = FALSE;
	}
	/* (a stuck turn while it walked to the staging point or along the lane: something in that way, the next axis) */
	if (tick->now < bot->unstick_until_tick && bot->unstick_until_tick != mount->seen_unstick)
	{
		mount->seen_unstick = bot->unstick_until_tick;
		if (mount->engaged_tick && tick->now - mount->engaged_tick <= TICKS_PER_SECOND &&
			(!mount->jump_tick || tick->now - mount->jump_tick > 2 * TICKS_PER_SECOND))
		{
			mount->shift = (short)((mount->shift + 1) % 8);
			if (ae_bots_events_on())
				ae_bots_event_line(index, tick->now, "mount_stuck", -1, 0.0f, "");
		}
	}
	item_index = ae_bots_power_item_of((short)entry);
	target = item_index != NONE ? object_get(item_index)->object.position : timer->position;
	across = bots_distance2d(&tick->position, &target);
	rise = target.z - tick->position.z;
	biped = biped_try_and_get(tick->player->unit_index);
	airborne = (boolean)(biped && TEST_FLAG(biped->biped.flags, _biped_airborne_bit));
	/* (a jump pressed: in the air within a tick or two, over the stand some 30 ticks later; at the item meanwhile) */
	if (mount->jump_tick && tick->now - mount->jump_tick < 8)
	{
		tick->flags &= ~FLAG(_unit_control_jump_bit);
		tick->move_direction = (real)atan2(target.y - tick->position.y, target.x - tick->position.x);
		tick->moving = TRUE;
		return;
	}
	/* (in the air after it: at the item still, fight or not) */
	if (airborne && mount->jump_tick && tick->now - mount->jump_tick < 60)
	{
		tick->flags &= ~FLAG(_unit_control_jump_bit);
		tick->move_direction = (real)atan2(target.y - tick->position.y, target.x - tick->position.x);
		tick->moving = TRUE;
		return;
	}
	/* (on the floor again with it not made: a miss, the next axis; on the stand: counted) */
	if (!airborne && mount->jump_tick && !mount->counted)
	{
		mount->counted = TRUE;
		if (fabs(rise) <= 0.35f && across < 1.0f)
		{
			if (ae_bots_events_on())
				ae_bots_event_line(index, tick->now, "mount_on", -1, across, "");
		}
		else
		{
			mount->shift = (short)((mount->shift + 1) % 8);
			if (ae_bots_events_on())
				ae_bots_event_line(index, tick->now, "mount_miss", -1, across, "");
		}
	}
	/* (dev events: where a bot within reach of a stand is once a second, and what it holds the mount back for) */
	if (ae_bots_events_on() && across < 6.0f && rise > -0.5f && rise < 1.3f && tick->now % 30 == 0)
	{
		char extra[256];

		snprintf(extra, sizeof(extra), "entry=%ld item=%ld tx=%.2f ty=%.2f tz=%.2f x=%.2f y=%.2f z=%.2f rise=%.2f fight=%d unstick=%d air=%d shift=%d",
			entry, item_index, (double)target.x, (double)target.y, (double)target.z, (double)tick->position.x, (double)tick->position.y, (double)tick->position.z, (double)rise,
			(int)tick->fighting, (int)(tick->now < bot->unstick_until_tick), (int)airborne, (int)mount->shift);
		ae_bots_event_line(index, tick->now, "mount_near", -1, across, extra);
	}
	/* (debug, HALO_BOT_MOUNT=0: the mount left out, the trace above kept, for a run to compare with) */
	{
		static int mount_off = -1;

		if (mount_off < 0)
		{
			char const *value = getenv("HALO_BOT_MOUNT");

			mount_off = value && value[0] == '0';
		}
		if (mount_off)
			return;
	}
	/* (not in a stuck bot's turn) */
	if (tick->now < bot->unstick_until_tick || tick->now < bot->escape_until_tick)
		return;
	/* (on the stand: on to the item, in a fight too: the jump was made for it, and a bot that stands on the altar to shoot
	leaves with it still lying there) */
	/* (not with the item gone, the stand only its spawn spot: two rivals who both wait there walked into the same point,
	0.4 apart, and the stuck check was told every tick that they were fine: the camo and overshield stands of Chill Out held
	bots 1000 ticks in a fight neither won. A bot in a fight with nothing lying there fights as anywhere) */
	if (!airborne && fabs(rise) <= 0.35f && across < 1.2f && !(tick->fighting && item_index == NONE))
	{
		/* (it waits there for the spawn: standing still on the altar is not being stuck) */
		bot->stuck_position = tick->position;
		bot->stuck_tick = tick->now;
		if (across > 0.12f)
		{
			tick->move_direction = (real)atan2(target.y - tick->position.y, target.x - tick->position.x);
			tick->moving = TRUE;
			tick->throttle_scale = 0.6f;
		}
		tick->flags &= ~FLAG(_unit_control_jump_bit);
		bot->climb_until_tick = 0;
		return;
	}
	/* (the rest only out of a fight) */
	if (tick->fighting)
		return;
	if (airborne || !ae_bots_power_mount_applies((float)rise, (float)across))
		return;
	{
		float point_x, point_y, axis_x, axis_y;
		real_point3d stage_ground;
		long stage_surface;

		/* (the nearest axis, from the miss-turned one on, whose staging point has the ground of a stand's foot) */
		{
			real_point3d const *from = ae_bots_power_mount_from(mount, &tick->position);

			if (!ae_bots_power_mount_stage(&target, from, mount->shift, &point_x, &point_y, &stage_ground, &stage_surface))
				ae_bots_power_mount_point((float)target.x, (float)target.y, (float)from->x, (float)from->y, mount->shift,
					&point_x, &point_y);
		}
		stage_ground.x = point_x;
		stage_ground.y = point_y;
		axis_x = (point_x - (float)target.x) / AE_BOTS_POWER_MOUNT_STAGING;
		axis_y = (point_y - (float)target.y) / AE_BOTS_POWER_MOUNT_STAGING;
		/* (a stand has a face: along the lane the ground steps up near the item. A ramp or stairs to it are walked up) */
		if (ae_bots_events_on() && tick->now % TICKS_PER_SECOND == 0)
		{
			char extra[160];

			snprintf(extra, sizeof(extra), "stand=%d lane=%d stage=%.2f,%.2f speed=%.3f",
				(int)ae_bots_power_mount_is_stand(&target, axis_x, axis_y),
				(int)ae_bots_power_mount_in_lane((float)(tick->position.x - target.x), (float)(tick->position.y - target.y),
					axis_x, axis_y), (double)point_x, (double)point_y,
				(double)sqrt(tick->object->object.translational_velocity.i * tick->object->object.translational_velocity.i +
					tick->object->object.translational_velocity.j * tick->object->object.translational_velocity.j));
			ae_bots_event_line(index, tick->now, "mount_go", -1, across, extra);
		}
		if (!ae_bots_power_mount_is_stand(&target, axis_x, axis_y))
			return;
		tick->moving = TRUE;
		tick->throttle_scale = 1.0f;
		tick->flags &= ~FLAG(_unit_control_jump_bit);
		bot->climb_until_tick = 0;
		mount->engaged_tick = tick->now;
		if (ae_bots_power_mount_in_lane((float)(tick->position.x - target.x), (float)(tick->position.y - target.y), axis_x,
				axis_y))
		{
			/* (in the lane: at the item, the button 1.5 to 1.8 from it with the run made; nearer than that it is too late, or against the
			stand's base, which stops it 1.0 out, and goes back to run at it again) */
			tick->move_direction = (real)atan2(target.y - tick->position.y, target.x - tick->position.x);
			if (across < AE_BOTS_POWER_MOUNT_JUMP_NEAREST)
				tick->move_direction = (real)atan2(point_y - tick->position.y, point_x - tick->position.x);
			if (ae_bots_power_mount_jump((float)across, (float)tick->object->object.translational_velocity.i,
					(float)tick->object->object.translational_velocity.j, (float)(target.x - tick->position.x),
					(float)(target.y - tick->position.y)))
			{
				tick->flags |= FLAG(_unit_control_jump_bit);
				mount->jump_tick = tick->now;
				mount->counted = FALSE;
				if (ae_bots_events_on())
					ae_bots_event_line(index, tick->now, "mount_jump", -1, across, "");
			}
		}
		else
		{
			tick->move_direction = (real)atan2(point_y - tick->position.y, point_x - tick->position.x);
			/* (told to walk to the staging point and held: the edge of a drop the bots' fall guard stops at, or a wall; the
			next axis after a third of a second, not after bots.c's stuck check's second) */
			if (bots_distance2d(&tick->position, &stage_ground) > 0.4f &&
				tick->object->object.translational_velocity.i * tick->object->object.translational_velocity.i +
				tick->object->object.translational_velocity.j * tick->object->object.translational_velocity.j < 0.01f * 0.01f)
			{
				if (++mount->still_ticks >= 10)
				{
					mount->still_ticks = 0;
					mount->shift = (short)((mount->shift + 1) % 8);
					if (ae_bots_events_on())
						ae_bots_event_line(index, tick->now, "mount_held", -1, across, "");
				}
			}
			else
				mount->still_ticks = 0;
		}
	}
}
