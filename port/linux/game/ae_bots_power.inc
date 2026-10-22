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

#define AE_BOTS_POWER_ENTRIES 32

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

/* whether the entry the bot goes for is out of its reach: its weapon left be by bots.c, or the path to its spot short */
static boolean ae_bots_power_out_of_reach(struct bots_bot const *bot, short entry)
{
	struct item_timer const *timer = item_timers_get(entry);
	long item_index;

	if (!timer)
		return FALSE;
	item_index = ae_bots_power_item_at(timer);
	if (item_index != NONE && bots_item_unreachable(bot, item_index))
		return TRUE;
	if (!bot->path.valid || !bot->path_short || bots_distance2d(&bot->goal_point, &timer->position) >= 2.5f)
		return FALSE;
	/* (a short path that ends beside the spot, a jump below it: a stand, as Temple's camouflage's and overshield's,
	which bots.c's climb at the path's end (jumps towards the goal) takes; the stall rule catches a miss) */
	return (boolean)(bots_distance2d(&bot->path.endpoint.point, &timer->position) > 1.5f ||
		timer->position.z - bot->path.endpoint.point.z > 1.2f);
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
			item_index = ae_bots_power_item_at(timer);
			kind = ae_bots_power_entry_kind(timer, item_index);
			if (kind == NONE)
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
			char name[16];
			short letter;

			/* (the power list's label, in ASCII: platform_log has no wide strings) */
			for (letter = 0; letter < 15 && label[letter]; letter++)
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
		best_item = ae_bots_power_item_at(item_timers_get(best));
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
