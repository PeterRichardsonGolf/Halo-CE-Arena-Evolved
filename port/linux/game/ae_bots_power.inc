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
#include "items/equipment_definitions.h"

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

static boolean ae_bots_hung_at(real_point3d const *point);
static void ae_bots_power_shoot_reset(void);

static long ae_bots_power_bot_index(struct bots_bot const *bot)
{
	return (long)(bot - bots_globals.bots);
}

/* the item of the entry lying at its spawn point now (within 2 units, on the map, not held), or NONE; for a spot
hung in the air, also one lying anywhere below it (within 2 units across): shot down, it fell there */
static long ae_bots_power_item_at(struct item_timer const *timer)
{
	struct object_iterator iterator;
	short kind;
	int spot_hung = -1;	/* (whether the spot hangs in the air: judged once, when an item lies below it) */

	object_iterator_new(&iterator, _object_mask_weapon | _object_mask_equipment, 0);
	while (object_iterator_next(&iterator))
	{
		struct object_datum const *object = object_get(iterator.index);

		if (object->object.parent_object_index != NONE ||
			!TEST_FLAG(object->object.flags, _object_connected_to_map_bit))
		{
			continue;
		}
		if (bots_distance3d(&object->object.position, &timer->position) > 2.0f)
		{
			if (bots_distance2d(&object->object.position, &timer->position) > 2.0f ||
				object->object.position.z > timer->position.z)
			{
				continue;
			}
			if (spot_hung < 0)
				spot_hung = ae_bots_hung_at(&timer->position) ? 1 : 0;
			if (!spot_hung)
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
	ae_bots_power_shoot_reset();
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

/* SHOOTING DOWN (0.9.0, ae_bots_power_rules.h): a bot that sees no enemy shoots down an item hung in the air in its
reach and sight that it would go for: a power item, a weapon bots_find_item would fetch, a health pack while it is
hurt (and the gametype's health has packs). It stands, turns its aim onto the item at its turning speed, and once it
has reacted fires one tap with its aim on it, then half a second to see the item fall. A fallen item is no longer
hung, and the bots' own goals take it (ae_bots_power_goal finds a power item below its spot; bots_find_item, the
packs' detour). Not in a vehicle, not without a weapon, not while it aims a grenade, not with a teammate in the line,
not at an item steeper above than a bot looks, nor at a spectator's placement (a "flycam" item: Digsite's Prisoner
hangs a camouflage 32 units up for it); 4 s at one item, then that item is left alone 20 s. */
#define AE_BOTS_POWER_SHOOT_LEFT 4
/* (bots.c's bots_find_item looks for a weapon within 20 units, out of a fight) */
#define AE_BOTS_POWER_SHOOT_WEAPON_REACH 20.0f

static struct
{
	long item;		/* the hung item it shoots at, or NONE */
	long since;		/* when it started on it */
	long check_tick;	/* when it looks for one again */
	long shot_tick;		/* when it may fire next */
	long unit_index;	/* the body it started with (a new life starts over) */
	/* the items it left after 4 s at them, and until when (game ticks) */
	long left_items[AE_BOTS_POWER_SHOOT_LEFT];
	long left_until[AE_BOTS_POWER_SHOOT_LEFT];
	short left_next;
} ae_bots_power_shots[BOTS_BRAINS];

/* (a map's start) no bot shooting, no item left alone */
static void ae_bots_power_shoot_reset(void)
{
	long index;
	short slot;

	for (index = 0; index < BOTS_BRAINS; index++)
	{
		ae_bots_power_shots[index].item = NONE;
		ae_bots_power_shots[index].since = 0;
		ae_bots_power_shots[index].check_tick = 0;
		ae_bots_power_shots[index].shot_tick = 0;
		ae_bots_power_shots[index].unit_index = NONE;
		for (slot = 0; slot < AE_BOTS_POWER_SHOOT_LEFT; slot++)
		{
			ae_bots_power_shots[index].left_items[slot] = NONE;
			ae_bots_power_shots[index].left_until[slot] = 0;
		}
		ae_bots_power_shots[index].left_next = 0;
	}
}

/* how many seconds since the bot last saw an enemy or took a hit (its target's sight tick, kept after the target
dies or is dropped) */
static real ae_bots_power_calm_seconds(struct bots_bot const *bot, long now)
{
	long last = bot->hurt_tick > bot->target_seen_tick ? bot->hurt_tick : bot->target_seen_tick;

	return (real)(now - last) / (real)TICKS_PER_SECOND;
}

/* whether the bot left the item alone lately (4 s at it) */
static boolean ae_bots_power_shoot_left(long index, long item_index, long now)
{
	short slot;

	for (slot = 0; slot < AE_BOTS_POWER_SHOOT_LEFT; slot++)
	{
		if (ae_bots_power_shots[index].left_items[slot] == item_index && now < ae_bots_power_shots[index].left_until[slot])
			return TRUE;
	}
	return FALSE;
}

/* (ae_bots_objective.inc) how far a bot in an objective role fetches a weapon (0: none, -1: no role) */
static real ae_bots_objective_weapon_reach(long index, long player_index);

/* an item lying on the map at rest (a shot one falling is not), not held: the point to shoot at on it and its distance
from the bot's eyes. The cheap checks, before any ray */
static boolean ae_bots_power_hung_candidate(struct bots_tick *tick, long item_index, real_point3d *point, real *distance)
{
	struct object_datum *object = item_index == NONE ? NULL :
		(struct object_datum *)object_try_and_get_and_verify_type(item_index, _object_mask_weapon | _object_mask_equipment);

	if (!object || object->object.parent_object_index != NONE ||
		!TEST_FLAG(object->object.flags, _object_connected_to_map_bit) ||
		!TEST_FLAG(object->object.flags, _object_at_rest_bit) ||
		TEST_FLAG(item_get(item_index)->item.flags, _item_attached_to_unit_bit))
	{
		return FALSE;
	}
	*point = object->object.bounding_sphere_center;
	*distance = (real)bots_distance3d(bots_tick_eye(tick), point);
	return TRUE;
}

/* whether the candidate hangs in the air and the bot can shoot it: no floor below within reach (ae_bots_item_hung),
nor a crate, scenery, a machine or a vehicle it rests on; not a spectator's placement; no steeper above or below than
a bot looks (bots.c holds its pitch within 1.4 radians); in its sight. The rays, last */
static boolean ae_bots_power_hung_seen(struct bots_tick *tick, long item_index, real_point3d const *point)
{
	struct object_datum *object = object_get(item_index);
	char const *name = tag_get_name(object->definition_index);
	real_point3d const *eye;
	struct collision_result collision;
	real_vector3d down;
	real dx, dy;

	if (name && strstr(name, "flycam"))
		return FALSE;
	eye = bots_tick_eye(tick);
	dx = point->x - eye->x;
	dy = point->y - eye->y;
	if ((real)fabs((real)atan2(point->z - eye->z, (real)sqrt(dx * dx + dy * dy))) > 1.35f)
		return FALSE;
	if (!ae_bots_item_hung(item_index))
		return FALSE;
	down.i = 0.0f;
	down.j = 0.0f;
	down.k = -(AE_BOTS_TRAVEL_HOP_RISE + AE_BOTS_TRAVEL_GRAB_REACH);
	if (collision_test_vector(FLAG(_collision_test_front_facing_surfaces_bit) | FLAG(_collision_test_objects_bit) |
		FLAG(_collision_test_objects_scenery_bit) | FLAG(_collision_test_objects_machines_bit) |
		FLAG(_collision_test_objects_vehicles_bit), &object->object.position, &down, item_index, &collision))
	{
		return FALSE;
	}
	return bots_line_of_sight(tick->player->unit_index, eye, point, NONE);
}

/* whether the item is a power entry's: 1 of a kind the bot does not hold (any power weapon, however it weighs against
its own; the power goal takes it once fallen), 0 of one it holds, -1 not a power entry's */
static int ae_bots_power_entry_item_wanted(struct bots_tick *tick, long item_index)
{
	short count = item_timers_count();
	short entry;

	if (count > AE_BOTS_POWER_ENTRIES)
		count = AE_BOTS_POWER_ENTRIES;
	for (entry = 0; entry < count; entry++)
	{
		struct item_timer const *timer = item_timers_get(entry);
		short kind;

		if (!timer || timer->timer_class >= NUMBER_OF_ITEM_TIMER_POWER_CLASSES ||
			ae_bots_power_item_of(entry) != item_index)
		{
			continue;
		}
		kind = ae_bots_power_entry_kind(timer, item_index);
		return kind != NONE && !ae_bots_power_holds(tick->unit, timer, kind) ? 1 : 0;
	}
	return -1;
}

/* whether the bot would go for the item, were it on the ground: a power entry's of a kind it does not hold; a weapon
as bots_find_item weighs one (better than the one it holds, or its ammunition while low), within 20 units across of
the bot (where it falls; 10 in an objective role, as ae_bots_objective.inc fetches one); a powerup it does not wear; a
health pack while hurt. A carrier, a holder or a racer shoots none (it keeps to its goal) */
static boolean ae_bots_power_hung_wanted(struct bots_bot *bot, struct bots_tick *tick, long item_index)
{
	struct object_datum *object = object_get(item_index);
	real role_reach = ae_bots_objective_weapon_reach(ae_bots_power_bot_index(bot), bot->player_index);
	int power;

	if (role_reach == 0.0f)
		return FALSE;
	power = ae_bots_power_entry_item_wanted(tick, item_index);
	if (power >= 0)
		return (boolean)power;
	if (object_get_type(item_index) == _object_type_weapon)
	{
		long current = bots_current_weapon(tick->unit);
		short current_value = current != NONE ? bots_weapon_value(weapon_get(current)->definition_index) : 0;
		short value = bots_weapon_value(object->definition_index);
		real reach = role_reach > 0.0f ? role_reach : AE_BOTS_POWER_SHOOT_WEAPON_REACH;
		short slot;

		if (bots_distance2d(&tick->position, &object->object.position) > reach)
			return FALSE;
		if (ae_bots_item_refills(tick->unit, item_index))
			return TRUE;
		for (slot = 0; slot < MAXIMUM_WEAPONS_PER_UNIT; slot++)
		{
			long other = tick->unit->unit.weapon_object_indices[slot];

			if (other != NONE && weapon_get(other)->definition_index == object->definition_index)
				return FALSE;
		}
		return (boolean)(value > current_value ||
			(value == current_value && ae_bots_item_reaches_further(item_index, current)));
	}
	{
		struct equipment_definition *definition = equipment_definition_get(object->definition_index);
		int style;

		if (!definition)
			return FALSE;
		switch (definition->equipment.powerup_type)
		{
		case _equipment_powerup_health:
			style = ae_bots_travel_health_style();
			return (boolean)(style != AE_BOTS_TRAVEL_HEALTH_FULL &&
				ae_bots_travel_pack_need(style, tick->object->object.body_vitality) > 0.0f);
		case _equipment_powerup_overshield:
			return (boolean)(tick->object->object.shield_vitality <= 1.0f);
		case _equipment_powerup_active_camouflage:
			return (boolean)!TEST_FLAG(tick->unit->unit.flags, _unit_active_camouflaged_bit);
		case _equipment_powerup_double_speed:
		case _equipment_powerup_full_spectrum_vision:
			return TRUE;
		default:
			return FALSE;
		}
	}
}

/* (bots.c, a bot that is not fighting, before its walk) whether the bot shoots at a hung item this tick; TRUE takes
the tick from its walk */
static boolean ae_bots_power_shoot(struct bots_bot *bot, struct bots_tick *tick)
{
	long index = ae_bots_power_bot_index(bot);
	long now = tick->now;
	int level = ae_bots_level_of_skill(bots_globals.skill);
	long item;
	real calm, range, distance = 0.0f, dx, dy, dz, horizontal, turn, yaw_off, pitch_off, cone;
	real_point3d point;
	real_point3d const *eye;

	if (index < 0 || index >= BOTS_BRAINS)
		return FALSE;
	item = ae_bots_power_shots[index].item;
	if (ae_bots_power_shots[index].unit_index != tick->player->unit_index)
	{
		ae_bots_power_shots[index].unit_index = tick->player->unit_index;
		item = NONE;
	}
	/* (not in a vehicle, not without a weapon, nor while it aims a grenade at where an enemy went: ae_bots_combat.inc) */
	if (tick->weapon_index == NONE || tick->object->object.parent_object_index != NONE ||
		now < ae_bots_fight_state(bot, tick)->prenade_until)
	{
		ae_bots_power_shots[index].item = NONE;
		return FALSE;
	}
	calm = ae_bots_power_calm_seconds(bot, now);
	range = bots_weapon_range(tick->weapon_index);
	/* (the item it shoots at: still hung, in sight and wanted, else left; left after 4 s at it, for 20 s) */
	if (item != NONE)
	{
		real trying = (real)(now - ae_bots_power_shots[index].since) / (real)TICKS_PER_SECOND;

		if (!ae_bots_power_hung_candidate(tick, item, &point, &distance) ||
			!ae_bots_power_shoot_wanted(level, distance, range, calm, trying) ||
			!ae_bots_power_hung_wanted(bot, tick, item) || !ae_bots_power_hung_seen(tick, item, &point))
		{
			if (trying >= AE_BOTS_POWER_SHOOT_GIVE_UP_SECONDS)
			{
				short slot = ae_bots_power_shots[index].left_next;

				ae_bots_power_shots[index].left_items[slot] = item;
				ae_bots_power_shots[index].left_until[slot] =
					now + (long)(AE_BOTS_POWER_SHOOT_REST_SECONDS * TICKS_PER_SECOND);
				ae_bots_power_shots[index].left_next = (short)((slot + 1) % AE_BOTS_POWER_SHOOT_LEFT);
				if (ae_bots_events_on())
					ae_bots_event_line(index, now, "hung_left", item, trying, "");
			}
			item = NONE;
			ae_bots_power_shots[index].check_tick = now + TICKS_PER_SECOND;
		}
	}
	/* (once a second, each bot on its own tick of the second, calm: the nearest hung item it sees and wants; the cheap
	checks first, the rays last) */
	if (item == NONE && now >= ae_bots_power_shots[index].check_tick && (now + index) % TICKS_PER_SECOND == 0 &&
		calm >= AE_BOTS_POWER_SHOOT_CALM_SECONDS)
	{
		struct object_iterator iterator;
		real nearest = 0.0f;

		ae_bots_power_shots[index].check_tick = now + TICKS_PER_SECOND;
		object_iterator_new(&iterator, _object_mask_weapon | _object_mask_equipment, 0);
		while (object_iterator_next(&iterator))
		{
			real_point3d at;
			real away;

			if (ae_bots_power_shoot_left(index, iterator.index, now) ||
				!ae_bots_power_hung_candidate(tick, iterator.index, &at, &away) ||
				!ae_bots_power_shoot_wanted(level, away, range, calm, 0.0f) ||
				(item != NONE && away >= nearest) ||
				!ae_bots_power_hung_wanted(bot, tick, iterator.index) ||
				!ae_bots_power_hung_seen(tick, iterator.index, &at))
			{
				continue;
			}
			item = iterator.index;
			nearest = away;
			point = at;
			distance = away;
		}
		if (item != NONE)
		{
			ae_bots_power_shots[index].since = now;
			ae_bots_power_shots[index].shot_tick = now + (long)(tick->skill->reaction_seconds * TICKS_PER_SECOND);
			if (ae_bots_events_on())
				ae_bots_event_line(index, now, "hung_shoot", item, distance, "");
		}
	}
	ae_bots_power_shots[index].item = item;
	if (item == NONE)
		return FALSE;
	/* (standing, the aim onto the item at its turning speed; a tap with the aim on it, a quarter of its size) */
	eye = bots_tick_eye(tick);
	dx = point.x - eye->x;
	dy = point.y - eye->y;
	dz = point.z - eye->z;
	horizontal = (real)sqrt(dx * dx + dy * dy);
	turn = tick->skill->turn_degrees_per_tick * BOTS_DEGREES;
	yaw_off = bots_angle_difference((real)atan2(dy, dx), bot->yaw);
	pitch_off = (real)atan2(dz, horizontal) - bot->pitch;
	bot->yaw += PIN(yaw_off, -turn, turn);
	bot->pitch += PIN(pitch_off, -turn, turn);
	tick->moving = FALSE;
	yaw_off = (real)fabs(bots_angle_difference((real)atan2(dy, dx), bot->yaw));
	pitch_off = (real)fabs((real)atan2(dz, horizontal) - bot->pitch);
	cone = (real)atan(MAX(object_get(item)->object.bounding_sphere_radius, 0.1f) * 0.25f / MAX(distance, 0.5f));
	if (now >= ae_bots_power_shots[index].shot_tick && yaw_off < cone && pitch_off < cone &&
		!ae_bots_fight_teammate_near(bot, tick, &point, FALSE))
	{
		tick->flags |= FLAG(_unit_control_weapon_primary_trigger_bit);
		ae_bots_power_shots[index].shot_tick = now + bots_weapon_tap_ticks(tick->weapon_index) + TICKS_PER_SECOND / 2;
	}
	return TRUE;
}
