/*
AE_BOTS_FALLS.INC

Included by ae_bots_lan.inc, so at the end of bots.c, after the fight's and the getting around's glue (it reads their
state): the bots' deaths by their own hand (ae_bots_falls_rules.h; the owner, 2026-10-20: "can you have a helper
work on tuning bots for suicide and fall deaths. i find that is what makes our bots still look like bots").

- THE CAUSE OF A DEATH (debug.bot_events): damage.c's AE hook tells ae_bots_events.inc of every damage to a unit;
  a bot's unit's last one is kept here (its effect, its owner, whether it killed at once), and its fight log's
  ev=death line gets cause=<word> (ae_bots_death_cause_name), the damage's tag (dmg=), its owner (by=), where the bot
  last stood on the ground (gx= gy= gz=), how far below that it died (drop=), how long it was in the air (air=, s),
  its own nearest live grenade then (nade=), the guard's probes of its way then (probe=) and what it was doing as it
  left the ground (doing=: fight, retreat, charge, halt, forward, back, strafe, walk:<goal>, unstick, climb, escape,
  link, jump, slip), so the falls can be traced to the behaviour that walked them off.
- THE GUARD (ae_bots_falls_guard, bots.c's AE hook after the unstick, the last word on where the bot walks this
  tick): its own live grenade within the danger is run from; a throw at a wall right ahead, into a wall well short
  of the target, or at a target within the blast is held, as is a rocket or a fuel rod shot within its blast (NORMAL
  and up); the ground ahead of where it walks is probed (ae_bots_falls_rules.h's THE EDGE: a ray down a step ahead
  and at the look ahead; a steep floor slid down; a big drop's landing probed beyond), and so is where its speed
  carries it; a lethal step is not taken: another way is (in a fight the strafe turns to the other side first; with
  a lethal carry, only a way that brakes it), else it stands; a walking bot held at an edge a second gives up its goal
  for a roaming place away from the edge; a jump with a lethal drop where it lands (NORMAL and up: about it) is not
  made. Not while it walks to a layout jump (the jump is the way); in the air too (a jump's air control drifted bots
  over the gaps between Fallout's roofs). halo.log's "bots: falls" line counts it all once a minute.

Box, 2026-10-20 (6 bots, 240 s games; each guard step was measured on the game before it): Fallout LEGENDARY pits 26
(the game without the guard) to 2-6; Boarding Action NORMAL 34 to 0, its bots fighting across the gap from the hull's
edge instead of walking off it.
*/

#include "objects/damage.h"
#include "items/projectiles.h"
#include "game/game_globals.h"
#include "ae_bots_falls_rules.h"
#include "ae_map_volumes.h"
#include "ae_round.h"

/* (ae_bots_fear.inc) a flank whose way goes over the edge ends, so the flank does not set the same goal again */
static void ae_bots_fear_flank_given_up(struct bots_bot *bot, long now);

struct ae_falls_bot
{
	/* its unit's last damage: the unit, the effect, its owner (the bot itself, no one), at once, riding, when */
	long damage_unit;
	long damage_definition;
	long damage_owner;
	boolean damage_self;
	boolean damage_none;
	boolean damage_instant;
	boolean damage_riding;
	long damage_tick;
	/* where it last stood on the ground, when, and what it was doing then */
	long ground_unit;
	long ground_tick;
	real_point3d ground_point;
	char ground_doing[64];
	/* where it was at its last tick alive */
	real_point3d last_point;
	/* THE GUARD: the ticks it has stood held at an edge while walking; the grenade it last ran from */
	long held_ticks;
	long held_tick;
	long fled_grenade;
	/* its own nearest live grenade's distance at its last tick (none: a large number), and when the guard last told
	the fight log it turned or held it */
	real own_grenade_distance;
	long edge_event_tick;
	/* someone else's nearest live grenade it noticed: the object, when, and whether its level judged it worth fleeing */
	long judged_grenade;
	long judged_tick;
	boolean judged_flee;
	/* (debug.bot_events) the guard's last probes of its way while on the ground: the death line's probe= */
	char probe[48];
	/* (0.9.4) the deadly volume its living unit is in (NONE: none), since when */
	long deadly_inside;
	long deadly_inside_since;
	long deadly_inside_unit;
};

/* DEADLY VOLUMES (0.9.4, ae_bots_falls_rules.h): the map's trigger volumes known to kill, from earlier games
(ae_bots_learnt.h) and this one */
/* (as many as the learnt file keeps: ae_bots_learnt_rules.h AE_LEARNT_DEADLY) */
#define AE_FALLS_DEADLY 16

static struct
{
	long count;
	short volumes[AE_FALLS_DEADLY];
	/* this game's acts of god in volumes not deadly yet: the volume, its kills counted, the last's tick */
	long candidate_count;
	short candidates[AE_FALLS_DEADLY];
	int candidate_kills[AE_FALLS_DEADLY];
	long candidate_tick[AE_FALLS_DEADLY];
	/* (0.9.4 review) the players killed in it, counted */
	long candidate_victims[AE_FALLS_DEADLY][AE_BOTS_FALLS_VOLUME_KILLS];
} ae_falls_deadly;

/* (debug.bot_events) the last probe's finding: W a wall (how far), N no floor, F a floor (the drop, its normal's
height), S a steep floor slid down (the same; ! when it slides off), L a big drop's landing with none beyond, R the same
round about it, K a floor down in a deadly volume (0.9.4) */
static char ae_falls_probe_note[24];
/* (0.9.4 perf audit) the probe's note is read only for debug.bot_events' lines: written only then, not on every probe
of every moving bot each tick */
#define AE_FALLS_PROBE_NOTE(...) do { if (ae_bots_events_on()) snprintf(ae_falls_probe_note, sizeof(ae_falls_probe_note), __VA_ARGS__); } while (0)

static struct ae_falls_bot ae_falls_bots[BOTS_BRAINS];

#define AE_FALLS_GRENADES 16

static struct
{
	/* the live grenades this tick: where, whose (player), the object */
	long grenades_tick;
	long grenade_count;
	real_point3d grenade_points[AE_FALLS_GRENADES];
	long grenade_owners[AE_FALLS_GRENADES];
	long grenade_objects[AE_FALLS_GRENADES];
	/* a projectile definition's grenade-ness, the last one asked (1 grenade, 0 not); a weapon's explosive-ness */
	long grenade_definition;
	boolean grenade_definition_is;
	long explosive_definition;
	boolean explosive_definition_is;
	/* halo.log's minute line */
	long last_report_tick;
	long steps_turned;
	long steps_held;
	long goals_given_up;
	long jumps_held;
	long flights;
	long throws_held;
	long shots_held;
	/* (0.9.4) a shot bot's step off a floor's drop turned or held, the drop not lethal itself */
	long levels_kept;
	real lethal;
} ae_falls;

static char const *ae_falls_goal_name(short kind)
{
	switch (kind)
	{
	case _bots_goal_enemy: return "enemy";
	case _bots_goal_roam: return "roam";
	case _bots_goal_item: return "item";
	default: return "none";
	}
}

/* (ae_bots_travel_prepare, after the learnt state is ready) the map's deadly volumes from earlier games */
static void ae_falls_deadly_load(void)
{
	struct scenario *scenario = global_scenario_get();
	long index, count = ae_bots_learnt_deadly_count();

	ae_falls_deadly.count = 0;
	ae_falls_deadly.candidate_count = 0;
	for (index = 0; index < BOTS_BRAINS; index++)
		ae_falls_bots[index].deadly_inside = NONE;
	for (index = 0; scenario && index < count && ae_falls_deadly.count < AE_FALLS_DEADLY; index++)
	{
		long volume = ae_bots_learnt_deadly_get(index);

		if (VALID_INDEX(volume, scenario->trigger_volumes.count))
			ae_falls_deadly.volumes[ae_falls_deadly.count++] = (short)volume;
	}
	if (ae_falls_deadly.count > 0)
		platform_log("bots: falls: %ld deadly trigger volumes from earlier games", ae_falls_deadly.count);
	/* (0.9.4 MAP VOLUMES, ae_map_volumes_rules.h: a volume the map names a kill zone, deadly from the start) */
	for (index = 0; scenario && index < scenario->trigger_volumes.count && ae_falls_deadly.count < AE_FALLS_DEADLY; index++)
	{
		long known;
		boolean already = FALSE;

		if (!ae_map_volumes_kills(index))
			continue;
		for (known = 0; known < ae_falls_deadly.count; known++)
			already = already || ae_falls_deadly.volumes[known] == (short)index;
		if (already)
			continue;
		ae_falls_deadly.volumes[ae_falls_deadly.count++] = (short)index;
		platform_log("bots: falls: trigger volume %ld is the map's kill zone: deadly", index);
	}
}

/* the deadly volume a point is in, NONE for none */
static long ae_falls_deadly_at(real_point3d const *point)
{
	long index;

	for (index = 0; index < ae_falls_deadly.count; index++)
	{
		if (scenario_trigger_volume_test_point(ae_falls_deadly.volumes[index], point))
			return ae_falls_deadly.volumes[index];
	}
	return NONE;
}

/* whether a drop from these feet onto that floor takes a unit's middle into a deadly volume (ae_bots_edge_volume_heights) */
static boolean ae_falls_drop_deadly(real feet_z, real_point3d const *floor)
{
	float heights[16];
	int count, index;

	if (ae_falls_deadly.count <= 0)
		return FALSE;
	count = ae_bots_edge_volume_heights((float)feet_z, (float)floor->z, heights, (int)NUMBEROF(heights));
	for (index = 0; index < count; index++)
	{
		real_point3d point = *floor;

		point.z = (real)heights[index];
		if (ae_falls_deadly_at(&point) != NONE)
			return TRUE;
	}
	return FALSE;
}

/* a trigger volume's size (its box's volume) */
static real ae_falls_volume_size(struct scenario_trigger_volume const *volume)
{
	if (volume->type == _scenario_trigger_volume_type_oriented)
		return (real)fabs(volume->extents.i * volume->extents.j * volume->extents.k);
	return (real)fabs((volume->bounds.x1 - volume->bounds.x0) * (volume->bounds.y1 - volume->bounds.y0) *
		(volume->bounds.z1 - volume->bounds.z0));
}

/* whether a living unit of a player's other than this one has its middle in the volume */
static boolean ae_falls_volume_others_alive(short volume, long object_index)
{
	long player_index;

	for (player_index = data_next_index(player_data, NONE); player_index != NONE;
		player_index = data_next_index(player_data, player_index))
	{
		struct player_datum *player = player_get(player_index);
		struct object_datum *unit = player->unit_index != NONE && player->unit_index != object_index ?
			object_try_and_get_and_verify_type(player->unit_index, _object_mask_unit) : NULL;

		if (unit && !TEST_FLAG(unit->object.damage_flags, _object_dead_bit) && unit->object.body_vitality > 0.0f &&
			scenario_trigger_volume_test_point(volume, &unit->object.bounding_sphere_center))
		{
			return TRUE;
		}
	}
	return FALSE;
}

/* a unit killed by the map (no owner, at once, not riding): the smallest trigger volume its middle is in that no other
living unit is in counts the kill; at AE_BOTS_FALLS_VOLUME_KILLS it is deadly (ae_bots_falls_volume_learn) */
static void ae_falls_deadly_learn(long object_index, long owner_player_index, unsigned long flags)
{
	struct scenario *scenario = global_scenario_get();
	struct object_datum *object = object_try_and_get_and_verify_type(object_index, _object_mask_unit);
	real_point3d const *middle;
	struct scenario_trigger_volume const *definition;
	short volume, best = NONE;
	real best_size = 0.0f;
	long index, now = game_time_get(), victim;
	int first;

	if (!scenario || !object || owner_player_index != NONE || !TEST_FLAG(flags, _damage_kill_instantly_bit) ||
		object->object.parent_object_index != NONE || ae_falls_deadly.count >= AE_FALLS_DEADLY)
	{
		return;
	}
	/* (0.9.4 review: a round's reset kills everyone where they stand) */
	if (ae_round_on() && ae_round_countdown_ticks_left() > 0)
		return;
	victim = ((struct unit_datum *)object)->unit.player_index;
	if (victim == NONE)
		return;
	middle = &object->object.bounding_sphere_center;
	/* (in a deadly one already: nothing to learn) */
	if (ae_falls_deadly_at(middle) != NONE)
		return;
	for (volume = 0; volume < scenario->trigger_volumes.count && volume < 0x7FFF; volume++)
	{
		real size;

		if (!scenario_trigger_volume_test_point(volume, middle) || ae_falls_volume_others_alive(volume, object_index))
			continue;
		size = ae_falls_volume_size(TAG_BLOCK_GET_ELEMENT(&scenario->trigger_volumes, volume,
			struct scenario_trigger_volume));
		if (best == NONE || size < best_size)
		{
			best = volume;
			best_size = size;
		}
	}
	if (best == NONE)
		return;
	for (index = 0; index < ae_falls_deadly.candidate_count && ae_falls_deadly.candidates[index] != best; index++)
		;
	first = index >= ae_falls_deadly.candidate_count;
	if (first)
	{
		if (ae_falls_deadly.candidate_count >= AE_FALLS_DEADLY)
			return;
		index = ae_falls_deadly.candidate_count++;
		ae_falls_deadly.candidates[index] = best;
		ae_falls_deadly.candidate_kills[index] = 0;
		ae_falls_deadly.candidate_tick[index] = now;
	}
	if (!ae_bots_falls_volume_kill_counts(first, now - ae_falls_deadly.candidate_tick[index]) ||
		!ae_bots_falls_volume_victim_new(ae_falls_deadly.candidate_victims[index], ae_falls_deadly.candidate_kills[index], victim))
	{
		return;
	}
	if (ae_falls_deadly.candidate_kills[index] < AE_BOTS_FALLS_VOLUME_KILLS)
		ae_falls_deadly.candidate_victims[index][ae_falls_deadly.candidate_kills[index]] = victim;
	ae_falls_deadly.candidate_kills[index]++;
	ae_falls_deadly.candidate_tick[index] = now;
	definition = TAG_BLOCK_GET_ELEMENT(&scenario->trigger_volumes, best, struct scenario_trigger_volume);
	if (!ae_bots_falls_volume_learn(TRUE, TRUE, FALSE, FALSE, ae_falls_deadly.candidate_kills[index]))
	{
		platform_log("bots: falls: trigger volume %d '%s': a unit killed by the map in it (%d of %d to be deadly)",
			(int)best, definition->name, ae_falls_deadly.candidate_kills[index], AE_BOTS_FALLS_VOLUME_KILLS);
		return;
	}
	ae_falls_deadly.candidates[index] = ae_falls_deadly.candidates[--ae_falls_deadly.candidate_count];
	ae_falls_deadly.candidate_kills[index] = ae_falls_deadly.candidate_kills[ae_falls_deadly.candidate_count];
	csmemcpy(ae_falls_deadly.candidate_victims[index], ae_falls_deadly.candidate_victims[ae_falls_deadly.candidate_count],
		sizeof(ae_falls_deadly.candidate_victims[index]));
	ae_falls_deadly.candidate_tick[index] = ae_falls_deadly.candidate_tick[ae_falls_deadly.candidate_count];
	ae_falls_deadly.volumes[ae_falls_deadly.count++] = best;
	ae_bots_learnt_deadly(best, TRUE);
	platform_log("bots: falls: trigger volume %d '%s' is deadly (units killed by the map in it, the last at %.1f %.1f %.1f): "
		"the bots keep out of it", (int)best, definition->name, middle->x, middle->y, middle->z);
}

/* (each guard tick) a living bot that stays in a deadly volume shows it is not one (ae_bots_falls_volume_cleared) */
static void ae_falls_deadly_watch(struct ae_falls_bot *falls, struct bots_tick *tick)
{
	long volume, index;

	if (ae_falls_deadly.count <= 0)
		return;
	volume = ae_falls_deadly_at(&tick->object->object.bounding_sphere_center);
	if (volume == NONE || TEST_FLAG(tick->object->object.damage_flags, _object_dead_bit) ||
		tick->object->object.body_vitality <= 0.0f)
	{
		falls->deadly_inside = NONE;
		return;
	}
	/* (a new unit, the bot respawned: counted from now) */
	if (falls->deadly_inside != volume || falls->deadly_inside_unit != tick->player->unit_index)
	{
		falls->deadly_inside = volume;
		falls->deadly_inside_since = tick->now;
		falls->deadly_inside_unit = tick->player->unit_index;
		return;
	}
	if (!ae_bots_falls_volume_cleared(tick->now - falls->deadly_inside_since))
		return;
	for (index = 0; index < ae_falls_deadly.count; index++)
	{
		if (ae_falls_deadly.volumes[index] == volume)
		{
			ae_falls_deadly.volumes[index] = ae_falls_deadly.volumes[--ae_falls_deadly.count];
			break;
		}
	}
	ae_bots_learnt_deadly(volume, FALSE);
	falls->deadly_inside = NONE;
	platform_log("bots: falls: trigger volume %ld: a bot alive in it %ld s: not deadly after all", volume,
		(long)(AE_BOTS_FALLS_VOLUME_ALIVE_TICKS / TICKS_PER_SECOND));
}

/* (ae_bots_events.inc's damage note, debug.bot_events) a damage to a unit: a bot's living unit's is kept (not one
after its body is gone: the box's games of 10-20 had a dead bot's own firing effect, weapons\pistol\trigger, no
owner, dealt after its killer's damage in the same tick, logged as its death's cause) */
static void ae_bots_falls_damage(long owner_player_index, long object_index, long definition_index, unsigned long flags)
{
	struct unit_datum *unit = unit_try_and_get(object_index);
	long index;

	if (!unit || unit->unit.player_index == NONE || TEST_FLAG(unit->object.damage_flags, _object_dead_bit) ||
		unit->object.body_vitality <= 0.0f)
	{
		return;
	}
	/* (0.9.4: a player's unit killed by the map: the volume it is in) */
	ae_falls_deadly_learn(object_index, owner_player_index, flags);
	for (index = 0; index < BOTS_BRAINS; index++)
	{
		struct ae_falls_bot *falls = &ae_falls_bots[index];

		if (bots_globals.bots[index].player_index != unit->unit.player_index)
			continue;
		falls->damage_unit = object_index;
		falls->damage_definition = definition_index;
		falls->damage_owner = owner_player_index;
		falls->damage_self = owner_player_index == unit->unit.player_index;
		falls->damage_none = owner_player_index == NONE;
		falls->damage_instant = TEST_FLAG(flags, _damage_kill_instantly_bit);
		falls->damage_riding = unit->object.parent_object_index != NONE;
		falls->damage_tick = game_time_get();
		return;
	}
}

/* (FRESH SPAWN GRACE, ae_bots_combat.inc) whether the player's damage was the bot's last, within the ticks */
static boolean ae_bots_falls_hurt_by(long bot_index, long player_index, long ticks)
{
	struct ae_falls_bot const *falls;
	long now = game_time_get();

	if (bot_index < 0 || bot_index >= BOTS_BRAINS || player_index == NONE)
		return FALSE;
	falls = &ae_falls_bots[bot_index];
	return falls->damage_owner == player_index && falls->damage_tick <= now && now - falls->damage_tick < ticks;
}

/* what the bot does this tick, in the death line's words */
static void ae_falls_doing(struct bots_bot *bot, struct bots_tick *tick, char *out, size_t size)
{
	struct ae_travel_bot *travel = ae_travel_state(bot);
	size_t used = 0;

	out[0] = 0;
#define AE_FALLS_WORD(word) do { if (used < size) used += (size_t)snprintf(out + used, size - used, "%s%s", used ? "+" : "", (word)); } while (0)
	if (tick->fighting)
	{
		struct ae_bots_combat_bot *state = ae_bots_fight_state(bot, tick);

		AE_FALLS_WORD("fight");
		if (state->retreat.active)
			AE_FALLS_WORD("retreat");
		else if (state->charging)
			AE_FALLS_WORD("charge");
		else if (tick->now < state->halt_until)
			AE_FALLS_WORD("halt");
		if (tick->moving)
		{
			struct player_datum *target = player_try_and_get(bot->target_player_index);

			if (target && target->unit_index != NONE)
			{
				real_point3d point = ae_bots_vehicle_point(target->unit_index);
				real to_target = (real)atan2(point.y - tick->position.y, point.x - tick->position.x);
				real off = (real)fabs(bots_angle_difference(tick->move_direction, to_target));

				AE_FALLS_WORD(off > BOTS_PI * 0.66f ? "back" : off > BOTS_PI * 0.33f ? "strafe" : "forward");
			}
		}
	}
	else
	{
		char goal[16];

		snprintf(goal, sizeof(goal), "walk:%s", ae_falls_goal_name(bot->goal_kind));
		AE_FALLS_WORD(goal);
	}
	if (tick->now < bot->unstick_until_tick)
		AE_FALLS_WORD("unstick");
	if (bot->climb_until_tick && tick->now < bot->climb_until_tick)
		AE_FALLS_WORD("climb");
	if (tick->now < bot->escape_until_tick)
		AE_FALLS_WORD("escape");
	if (travel && travel->via >= 0 && travel->phase != _ae_travel_phase_to_entry)
		AE_FALLS_WORD("link");
	if (TEST_FLAG(tick->flags, _unit_control_jump_bit))
		AE_FALLS_WORD("jump");
	{
		struct biped_datum *biped = biped_try_and_get(tick->player->unit_index);

		if (biped && TEST_FLAG(biped->biped.flags, _biped_slipping_bit))
			AE_FALLS_WORD("slip");
	}
#undef AE_FALLS_WORD
}

/* (ae_bots_events_end, debug.bot_events) each living bot's tick, its controls built: where it stands on the ground,
and what it does there */
static void ae_bots_falls_tick_end(struct bots_bot *bot, long bot_index, struct bots_tick *tick)
{
	struct ae_falls_bot *falls;
	struct biped_datum *biped;

	if (bot_index < 0 || bot_index >= BOTS_BRAINS || !tick->object)
		return;
	falls = &ae_falls_bots[bot_index];
	falls->last_point = tick->position;
	biped = biped_try_and_get(tick->player->unit_index);
	if (tick->object->object.parent_object_index != NONE || !biped ||
		!TEST_FLAG(biped->biped.flags, _biped_airborne_bit))
	{
		falls->ground_unit = tick->player->unit_index;
		falls->ground_tick = tick->now;
		falls->ground_point = tick->position;
		ae_falls_doing(bot, tick, falls->ground_doing, sizeof(falls->ground_doing));
	}
}

/* (ae_bots_travel_tick, 0.9.4) what last hurt this unit of the bot's, as an ev=death line's cause: AE_BOTS_DEATH_OTHER when
the damage noted is another unit's */
static int ae_falls_unit_death_cause(long bot_index, long unit_index)
{
	struct ae_falls_bot const *falls;
	boolean known;
	char const *name = NULL;

	if (bot_index < 0 || bot_index >= BOTS_BRAINS)
		return AE_BOTS_DEATH_OTHER;
	falls = &ae_falls_bots[bot_index];
	known = unit_index != NONE && falls->damage_unit == unit_index;
	if (known && falls->damage_definition != NONE)
		name = tag_get_name(falls->damage_definition);
	return ae_bots_death_cause(known, name, known && falls->damage_self, known && falls->damage_none,
		known && falls->damage_instant, known && falls->damage_riding);
}

/* (ae_bots_events_begin) the words of a bot's ev=death line: its cause and how it came */
static void ae_bots_falls_death_words(struct bots_bot *bot, long bot_index, long now, char *out, size_t size)
{
	struct ae_falls_bot *falls;
	boolean known;
	char const *name = NULL;
	size_t used;
	char word[64];
	int cause;

	if (bot_index < 0 || bot_index >= BOTS_BRAINS)
	{
		out[0] = 0;
		return;
	}
	falls = &ae_falls_bots[bot_index];
	known = bot->unit_index != NONE && falls->damage_unit == bot->unit_index;
	if (known && falls->damage_definition != NONE)
		name = tag_get_name(falls->damage_definition);
	/* (the damage's tag in one word: its folders' spaces as underscores) */
	snprintf(word, sizeof(word), "%s", name ? name : "none");
	for (used = 0; word[used]; used++)
	{
		if (word[used] == ' ')
			word[used] = '_';
	}
	cause = ae_bots_death_cause(known, name, known && falls->damage_self, known && falls->damage_none,
		known && falls->damage_instant, known && falls->damage_riding);
	if (falls->ground_unit == bot->unit_index && bot->unit_index != NONE)
	{
		snprintf(out, size, "cause=%s dmg=%s by=%ld gx=%.1f gy=%.1f gz=%.1f drop=%.1f air=%.1f nade=%.1f probe=%s doing=%s",
			ae_bots_death_cause_name(cause), word,
			known ? ae_bots_event_absolute(falls->damage_owner) : -1L, falls->ground_point.x, falls->ground_point.y,
			falls->ground_point.z, falls->ground_point.z - falls->last_point.z,
			(real)(now - falls->ground_tick) / TICKS_PER_SECOND,
			falls->own_grenade_distance < 1000.0f ? falls->own_grenade_distance : -1.0f,
			falls->probe[0] ? falls->probe : "none", falls->ground_doing[0] ? falls->ground_doing : "none");
	}
	else
	{
		snprintf(out, size, "cause=%s dmg=%s by=%ld", ae_bots_death_cause_name(cause),
			word, known ? ae_bots_event_absolute(falls->damage_owner) : -1L);
	}
	/* (a death by a teammate's damage: tk=1, for counting betrayals in the logs) */
	if (known && game_engine_has_teams() && falls->damage_owner != NONE && falls->damage_owner != bot->player_index)
	{
		struct player_datum *owner = player_try_and_get(falls->damage_owner);

		if (owner && owner->team_index == player_get(bot->player_index)->team_index)
		{
			size_t length = strlen(out);

			snprintf(out + length, size - length, " tk=1");
		}
	}
	falls->damage_unit = NONE;
}

/* the drop that kills a player here (ae_bots_edge_lethal_distance), with this much of its body left (0.9.4:
ae_bots_edge_hurt_lethal_distance): damage.c's game_globals_falling_damage (its size asserted there, 0x98) has the
distance a landing starts to hurt at at 0x08, of its full damage at 0x0C and the maximum falling distance at 0x28 */
static real ae_falls_globals_lethal(int no_falling_damage, real body);

static real ae_falls_lethal_distance(long player_index, real body)
{
	return ae_falls_globals_lethal(game_engine_no_falling_damage(player_index) ? 1 : 0, body);
}

/* (0.9.4, ae_bots_travel_gap_deadly) the map's lethal drop for a bot with all its body, falling damage on */
static real ae_falls_map_lethal(void)
{
	return ae_falls_globals_lethal(0, 1.0f);
}

static real ae_falls_globals_lethal(int no_falling_damage, real body)
{
	struct game_globals *globals = scenario_get_game_globals();
	real maximum = 0.0f, full = 0.0f, lower = 0.0f;

	if (globals && globals->falling_damage.count > 0)
	{
		byte const *element = (byte const *)TAG_BLOCK_GET_ELEMENT(&globals->falling_damage, 0, byte);

		lower = *(real const *)(element + 0x08);
		full = *(real const *)(element + 0x0C);
		maximum = *(real const *)(element + 0x28);
	}
	return ae_bots_edge_hurt_lethal_distance(ae_bots_edge_lethal_distance(no_falling_damage, maximum, full),
		no_falling_damage, lower, full, body);
}

/* the height of a step's wall test, and of a jump's: a wall lower than it does not stop the probe (Boarding Action
10-20: a lip the knee-high test took for a wall was walked over, and a bot fell beyond it; Fallout: bots jumped a
railing the step's test found, and fell) */
#define AE_FALLS_STEP_RISE 0.9f
#define AE_FALLS_JUMP_RISE 1.1f
/* a drop this big (to a ledge below, not lethal itself) has its landing's further side probed too, from the bot's
height (Boarding Action 10-20: bots dropped 1.4 to 8 units onto a narrow ledge or the hull's rim, kept going and fell) */
#define AE_FALLS_BIG_DROP 1.0f
#define AE_FALLS_LANDING 1.5f
/* a wall test that hits at once (nearer than this) says nothing */
#define AE_FALLS_WALL_NEAREST 0.2f
/* the ticks after which a walking bot's held ticks are forgotten */
#define AE_FALLS_HELD_FORGET (3 * TICKS_PER_SECOND)
/* a floor steeper than this (its normal's height; about 32 degrees) is slid down (Fallout 10-20: bots that stepped onto
a pitched roof slid off its edge, the floor under the probe being the roof itself) */
#define AE_FALLS_STEEP 0.85f

#define AE_FALLS_COLLISION_FLAGS (FLAG(_collision_test_structure_bit) | FLAG(_collision_test_front_facing_surfaces_bit) | \
	FLAG(_collision_test_objects_bit) | FLAG(_collision_test_objects_scenery_bit) | FLAG(_collision_test_objects_machines_bit))

/* where a biped on a steep floor slides to: down its slope (the way its normal leans), a unit at a time, while the floor
stays steep (8 units at most): TRUE when it slides off into a lethal drop, or past where any floor is found. Boarding
Action 10-20: the hulls' edges curve down, so the floor a step past the deck was the hull itself, a short drop; a bot
stepping onto it slid down the hull and off. */
static boolean ae_falls_slide_lethal(long unit_index, real feet_z, struct collision_result const *floor, real lethal)
{
	struct collision_result collision = *floor;
	short step;

	for (step = 0; step < 8 && collision.plane.n.k < AE_FALLS_STEEP; step++)
	{
		real lean = (real)sqrt(collision.plane.n.i * collision.plane.n.i + collision.plane.n.j * collision.plane.n.j);
		real_point3d from;
		real_vector3d down;

		if (lean < 0.01f)
			break;
		from.x = collision.point.x + collision.plane.n.i / lean;
		from.y = collision.point.y + collision.plane.n.j / lean;
		from.z = collision.point.z + 0.5f;
		down.i = 0.0f;
		down.j = 0.0f;
		down.k = -(lethal + 1.5f - (feet_z - from.z));
		if (down.k > -1.0f)
			return TRUE;
		if (!collision_test_vector(AE_FALLS_COLLISION_FLAGS, &from, &down, unit_index, &collision))
			return TRUE;
	}
	return (boolean)ae_bots_edge_lethal(TRUE, feet_z - collision.point.z, lethal);
}

/* the place the guarded bot walks to (its next path point, else its goal) while the guard probes: a small drop towards
it, not lethal itself, is let go by (ae_bots_edge_drop_allowed); none outside the guard's walking probes */
static boolean ae_falls_target_known;
static real_point3d ae_falls_target;

/* THE LANDING'S RING (ae_bots_edge_ring_lethal): each point round a big drop's landing, AE_BOTS_EDGE_RING_RADIUS off at
the landing's height and half a unit up, is lethal when the ray down from it finds no floor, or one a lethal drop under the
bot's feet; a point behind a wall from the landing (the ray across to it hits one) is not */
static boolean ae_falls_ring_lethal(long unit_index, real_point3d const *position, real_point3d const *landing,
	real direction, real lethal)
{
	real_point3d from = *landing, point;
	real_vector3d across, down;
	struct collision_result collision;
	int index, lethal_points = 0, forward_lethal = 0;

	from.z += 0.5f;
	down.i = 0.0f;
	down.j = 0.0f;
	down.k = -(lethal + 1.0f - (position->z - from.z));
	if (down.k > -0.5f)
		return TRUE;
	for (index = 0; index < AE_BOTS_EDGE_RING_POINTS; index++)
	{
		real way = direction + (real)index * BOTS_PI * 2.0f / (real)AE_BOTS_EDGE_RING_POINTS;

		across.i = (real)cos(way) * AE_BOTS_EDGE_RING_RADIUS;
		across.j = (real)sin(way) * AE_BOTS_EDGE_RING_RADIUS;
		across.k = 0.0f;
		if (collision_test_vector(AE_FALLS_COLLISION_FLAGS, &from, &across, unit_index, &collision))
			continue;
		point.x = from.x + across.i;
		point.y = from.y + across.j;
		point.z = from.z;
		if (!collision_test_vector(AE_FALLS_COLLISION_FLAGS, &point, &down, unit_index, &collision) ||
			ae_bots_edge_lethal(TRUE, position->z - collision.point.z, lethal) ||
			ae_falls_drop_deadly(position->z, &collision.point))
		{
			lethal_points++;
			if (ae_bots_edge_ring_forward(index))
				forward_lethal = 1;
		}
	}
	return (boolean)ae_bots_edge_ring_lethal(lethal_points, forward_lethal);
}

/* whether a step this far that way is lethal: a wall before it (at rise above the feet: a step's, or a jump's, which
clears a railing) is not, as the bot cannot go there; the floor under it is found with a ray down past the lethal drop,
and a steep one is slid down (ae_falls_slide_lethal) */
static boolean ae_falls_step_lethal(long unit_index, real_point3d const *position, real direction, real distance,
	real rise, real lethal)
{
	struct collision_result collision;
	real_point3d start = *position, ahead;
	real_vector3d way, down;

	start.z += rise;
	way.i = (real)cos(direction) * distance;
	way.j = (real)sin(direction) * distance;
	way.k = 0.0f;
	/* (a wall the test starts against, under 0.2 units, says nothing: Fallout 10-20, bots against a sloped lip,
	the test hitting it at once, slid off beyond it) */
	if (collision_test_vector(AE_FALLS_COLLISION_FLAGS, &start, &way, unit_index, &collision) &&
		collision.t * distance >= AE_FALLS_WALL_NEAREST)
	{
		AE_FALLS_PROBE_NOTE( "W%.1f", collision.t * distance);
		return FALSE;
	}
	ahead.x = start.x + way.i;
	ahead.y = start.y + way.j;
	ahead.z = start.z;
	down.i = 0.0f;
	down.j = 0.0f;
	down.k = -(lethal + rise + 1.0f);
	if (!collision_test_vector(AE_FALLS_COLLISION_FLAGS, &ahead, &down, unit_index, &collision))
	{
		AE_FALLS_PROBE_NOTE( "N");
		return (boolean)ae_bots_edge_lethal(FALSE, 0.0f, lethal);
	}
	/* (0.9.4: a floor down in a deadly volume, K) */
	if (ae_falls_drop_deadly(position->z, &collision.point))
	{
		AE_FALLS_PROBE_NOTE( "K%.1f", position->z - collision.point.z);
		return TRUE;
	}
	if (collision.plane.n.k < AE_FALLS_STEEP)
	{
		boolean slid = ae_falls_slide_lethal(unit_index, position->z, &collision, lethal);

		AE_FALLS_PROBE_NOTE( "S%.1f/%.2f%s", position->z - collision.point.z,
			collision.plane.n.k, slid ? "!" : "");
		return slid;
	}
	AE_FALLS_PROBE_NOTE( "F%.1f/%.2f", position->z - collision.point.z,
		collision.plane.n.k);
	if (ae_bots_edge_lethal(TRUE, position->z - collision.point.z, lethal))
		return TRUE;
	/* (a big drop: beyond its landing, from the bot's height, at AE_FALLS_LANDING and twice that) */
	if (position->z - collision.point.z >= AE_FALLS_BIG_DROP)
	{
		real_point3d landing = collision.point;
		short far;

		/* (a small drop towards a lower place it walks to: the landing is not probed beyond) */
		if (ae_falls_target_known)
		{
			real tx = ae_falls_target.x - position->x, ty = ae_falls_target.y - position->y;
			real length = (real)sqrt(tx * tx + ty * ty);
			real cosine = length > 0.01f ? (tx * (real)cos(direction) + ty * (real)sin(direction)) / length : 1.0f;

			if (ae_bots_edge_drop_allowed(position->z - collision.point.z, position->z - ae_falls_target.z, cosine))
			{
				AE_FALLS_PROBE_NOTE( "D%.1f", position->z - collision.point.z);
				return FALSE;
			}
		}
		for (far = 1; far <= 2; far++)
		{
			real_point3d beyond;

			beyond.x = landing.x + (real)cos(direction) * AE_FALLS_LANDING * far;
			beyond.y = landing.y + (real)sin(direction) * AE_FALLS_LANDING * far;
			beyond.z = landing.z + 0.5f;
			down.k = -(lethal + 1.0f - (position->z - beyond.z));
			if (down.k > -0.5f)
				return TRUE;
			if (!collision_test_vector(AE_FALLS_COLLISION_FLAGS, &beyond, &down, unit_index, &collision) ||
				ae_bots_edge_lethal(TRUE, position->z - collision.point.z, lethal) ||
				ae_falls_drop_deadly(position->z, &collision.point))
			{
				AE_FALLS_PROBE_NOTE( "L%.1f", position->z - landing.z);
				return TRUE;
			}
		}
		/* (0.9.4: round about the landing too, ae_bots_edge_ring_lethal) */
		if (ae_falls_ring_lethal(unit_index, position, &landing, direction, lethal))
		{
			AE_FALLS_PROBE_NOTE( "R%.1f", position->z - landing.z);
			return TRUE;
		}
	}
	return FALSE;
}

/* a way lethal at half the look ahead or at all of it */
static boolean ae_falls_way_lethal(long unit_index, real_point3d const *position, real direction, real lookahead,
	real lethal)
{
	return ae_falls_step_lethal(unit_index, position, direction, lookahead * 0.5f, AE_FALLS_STEP_RISE, lethal) ||
		ae_falls_step_lethal(unit_index, position, direction, lookahead, AE_FALLS_STEP_RISE, lethal);
}

static boolean ae_falls_grenade_definition(long definition_index)
{
	if (definition_index != ae_falls.grenade_definition)
	{
		char const *name = tag_get_name(definition_index);

		ae_falls.grenade_definition = definition_index;
		ae_falls.grenade_definition_is = name && strstr(name, "grenade") != NULL;
	}
	return ae_falls.grenade_definition_is;
}

/* the live grenades, once a tick */
static void ae_falls_find_grenades(long now)
{
	struct object_iterator iterator;

	if (ae_falls.grenades_tick == now)
		return;
	ae_falls.grenades_tick = now;
	ae_falls.grenade_count = 0;
	object_iterator_new(&iterator, _object_mask_projectile, 0);
	while (object_iterator_next(&iterator) && ae_falls.grenade_count < AE_FALLS_GRENADES)
	{
		struct object_datum *object = object_get(iterator.index);

		if (object->object.owner_player_index == NONE || !ae_falls_grenade_definition(object->definition_index))
			continue;
		object_get_origin(iterator.index, &ae_falls.grenade_points[ae_falls.grenade_count]);
		ae_falls.grenade_owners[ae_falls.grenade_count] = object->object.owner_player_index;
		ae_falls.grenade_objects[ae_falls.grenade_count] = iterator.index;
		ae_falls.grenade_count++;
	}
}

/* GRENADES: away from its own live grenade within the danger (the nearest), and from someone else's as its movement
level judges it (ae_bots_grenade_flee_other: a roll a grenade at its dodge chance, after its dodge reaction) */
static void ae_falls_flee(struct bots_bot *bot, struct ae_falls_bot *falls, struct bots_tick *tick, int level,
	int movement_level)
{
	long index, nearest = NONE, other = NONE;
	real best = REAL_MAX, other_distance = REAL_MAX;

	ae_falls_find_grenades(tick->now);
	falls->own_grenade_distance = REAL_MAX;
	for (index = 0; index < ae_falls.grenade_count; index++)
	{
		real distance;

		distance = bots_distance3d(&ae_falls.grenade_points[index], &tick->position);
		if (ae_falls.grenade_owners[index] != bot->player_index)
		{
			if (distance < AE_BOTS_GRENADE_NOTICE && distance < other_distance)
			{
				other = index;
				other_distance = distance;
			}
			continue;
		}
		falls->own_grenade_distance = MIN(falls->own_grenade_distance, distance);
		if (ae_bots_grenade_flee(level, TRUE, distance) && distance < best)
		{
			best = distance;
			nearest = index;
		}
	}
	if (other != NONE)
	{
		if (falls->judged_grenade != ae_falls.grenade_objects[other])
		{
			falls->judged_grenade = ae_falls.grenade_objects[other];
			falls->judged_tick = tick->now;
			falls->judged_flee = (boolean)ae_bots_grenade_judge(movement_level, bots_random_real(bot));
		}
		if (ae_bots_grenade_flee_other(movement_level, (real)(tick->now - falls->judged_tick) / TICKS_PER_SECOND,
			falls->judged_flee, other_distance) && other_distance < best)
		{
			best = other_distance;
			nearest = other;
		}
	}
	if (nearest == NONE)
		return;
	tick->move_direction = (real)atan2(tick->position.y - ae_falls.grenade_points[nearest].y,
		tick->position.x - ae_falls.grenade_points[nearest].x);
	tick->moving = TRUE;
	tick->throttle_scale = 1.0f;
	if (falls->fled_grenade != ae_falls.grenade_objects[nearest])
	{
		falls->fled_grenade = ae_falls.grenade_objects[nearest];
		ae_falls.flights++;
		if (ae_bots_events_on())
			ae_bots_event_line((long)(bot - bots_globals.bots), tick->now, "flee", -1, best,
				ae_falls.grenade_owners[nearest] == bot->player_index ? "own=1" : "own=0");
	}
}

/* GRENADES: a throw this tick at a wall right ahead (along the aim), or at a target within the blast, is held */
static real ae_falls_aim_wall(struct bots_bot *bot, struct bots_tick *tick, real reach);

static void ae_falls_throw(struct bots_bot *bot, struct bots_tick *tick)
{
	real wall = REAL_MAX, target = REAL_MAX;
	real reach;

	if (!TEST_FLAG(tick->flags, _unit_control_throw_grenade_bit) ||
		TEST_FLAG(bot->previous_control_flags, _unit_control_throw_grenade_bit))
	{
		return;
	}
	if (tick->fighting)
	{
		struct player_datum *target_player = player_try_and_get(bot->target_player_index);

		target = tick->target_distance;
		/* (its feet hidden by a wall well short of it, a roof's parapet, where the grenade lands instead: Fallout
		10-20, frags thrown over a parapet at a target below came back) */
		if (target_player && target_player->unit_index != NONE)
		{
			struct collision_result collision;
			real_point3d const *eye = bots_tick_eye(tick);
			real_point3d feet = ae_bots_vehicle_point(target_player->unit_index);
			real_vector3d way;
			real length;

			feet.z += 0.2f;
			way.i = feet.x - eye->x;
			way.j = feet.y - eye->y;
			way.k = feet.z - eye->z;
			length = (real)sqrt(way.i * way.i + way.j * way.j + way.k * way.k);
			if (length > 0.01f && collision_test_vector(FLAG(_collision_test_structure_bit) |
				FLAG(_collision_test_front_facing_surfaces_bit), eye, &way, tick->player->unit_index, &collision) &&
				collision.t * length < length - AE_BOTS_GRENADE_SHORT_MARGIN)
			{
				wall = collision.t * length;
				target = REAL_MAX;
			}
		}
	}
	else
	{
		/* (a grenade at the last-seen spot: that spot's distance) */
		struct ae_bots_combat_bot *state = ae_bots_fight_state(bot, tick);

		if (tick->now <= state->prenade_until)
			target = bots_distance3d(bots_tick_eye(tick), &state->prenade_point);
	}
	/* (along the aim to the target, AE_BOTS_GRENADE_SHORT at most) */
	reach = MIN(MAX(target < 1000.0f ? target : AE_BOTS_GRENADE_SHORT, AE_BOTS_GRENADE_WALL), AE_BOTS_GRENADE_SHORT);
	wall = MIN(wall, ae_falls_aim_wall(bot, tick, reach));
	if (!ae_bots_grenade_throw_safe(wall, target))
	{
		tick->flags &= ~FLAG(_unit_control_throw_grenade_bit);
		ae_falls.throws_held++;
		if (ae_bots_events_on())
		{
			char extra[48];

			snprintf(extra, sizeof(extra), "wall=%.1f", wall < 1000.0f ? wall : -1.0f);
			ae_bots_event_line((long)(bot - bots_globals.bots), tick->now, "throw_held", -1,
				target < 1000.0f ? target : -1.0f, extra);
		}
	}
}

/* the first wall along the bot's aim within reach (a large number for none) */
static real ae_falls_aim_wall(struct bots_bot *bot, struct bots_tick *tick, real reach)
{
	struct collision_result collision;
	real_vector3d way;

	way.i = (real)cos(bot->yaw) * (real)cos(bot->pitch) * reach;
	way.j = (real)sin(bot->yaw) * (real)cos(bot->pitch) * reach;
	way.k = (real)sin(bot->pitch) * reach;
	if (collision_test_vector(AE_FALLS_COLLISION_FLAGS, bots_tick_eye(tick), &way, tick->player->unit_index, &collision))
		return collision.t * reach;
	return REAL_MAX;
}

/* whether a weapon (its object) is an explosive one: the rocket launcher, the fuel rod gun */
static boolean ae_falls_weapon_explosive(long weapon_index)
{
	char const *name = weapon_index != NONE ? tag_get_name(weapon_get(weapon_index)->definition_index) : NULL;

	return (boolean)(name && (strstr(name, "rocket") || strstr(name, "plasma_cannon") || strstr(name, "fuel rod")));
}

/* (owner 2026-10-20, Octagon 1v1s: bots with rockets hit 2-9% and held their fire for seconds, their target always
within the blast on a small map) in a fight, the explosive weapon in hand and the target nearer than the level fires
at (ae_bots_explosive_nearest): the other
weapon, when it has rounds (a player's switch); the other weapon in hand and the explosive one with rounds, the target
past twice the blast: back to it. NORMAL and up, as the held shot */
static void ae_falls_explosive_switch(struct bots_bot *bot, struct bots_tick *tick, int level)
{
	short slot;
	boolean explosive_in_hand;

	(void)bot;
	if (level < AE_BOTS_LEVEL_NORMAL || !tick->fighting || tick->weapon_index == NONE)
		return;
	explosive_in_hand = ae_falls_weapon_explosive(tick->weapon_index);
	if (explosive_in_hand ? tick->target_distance >= ae_bots_explosive_nearest(level) :
		tick->target_distance <= 2.0f * AE_BOTS_EXPLOSIVE_BLAST)
	{
		return;
	}
	for (slot = 0; slot < MAXIMUM_WEAPONS_PER_UNIT; slot++)
	{
		long other = tick->unit->unit.weapon_object_indices[slot];

		if (slot == tick->unit->unit.current_weapon_index || other == NONE || bots_weapon_empty(other))
			continue;
		if (ae_falls_weapon_explosive(other) != explosive_in_hand)
		{
			tick->action->desired_weapon_index = slot;
			return;
		}
	}
}

/* EXPLOSIVES: a rocket or a fuel rod not fired within its blast (NORMAL and up) */
static void ae_falls_explosive(struct bots_bot *bot, struct bots_tick *tick, int level)
{
	long definition;
	real wall;

	ae_falls_explosive_switch(bot, tick, level);
	if (!TEST_FLAG(tick->flags, _unit_control_weapon_primary_trigger_bit) || tick->weapon_index == NONE || !tick->fighting)
		return;
	definition = weapon_get(tick->weapon_index)->definition_index;
	if (definition != ae_falls.explosive_definition)
	{
		char const *name = tag_get_name(definition);

		ae_falls.explosive_definition = definition;
		ae_falls.explosive_definition_is = name && (strstr(name, "rocket") || strstr(name, "plasma_cannon") ||
			strstr(name, "fuel rod"));
	}
	if (!ae_falls.explosive_definition_is)
		return;
	wall = ae_falls_aim_wall(bot, tick, AE_BOTS_EXPLOSIVE_BLAST + 0.5f);
	if (!ae_bots_explosive_fire_safe(level, wall, tick->target_distance))
	{
		tick->flags &= ~FLAG(_unit_control_weapon_primary_trigger_bit);
		ae_falls.shots_held++;
	}
}

static void ae_falls_report(long now)
{
	if (now - ae_falls.last_report_tick < 60 * TICKS_PER_SECOND && now >= ae_falls.last_report_tick)
		return;
	if (ae_falls.last_report_tick > 0 && now > ae_falls.last_report_tick)
	{
		platform_log("bots: falls tick %ld: steps turned %ld, held %ld, goals given up at an edge %ld, jumps held %ld, "
			"own grenades run from %ld, throws held %ld, explosive shots held %ld, shot bots' levels kept %ld (lethal drop "
			"%.1f)", now, ae_falls.steps_turned, ae_falls.steps_held, ae_falls.goals_given_up, ae_falls.jumps_held,
			ae_falls.flights, ae_falls.throws_held, ae_falls.shots_held, ae_falls.levels_kept, ae_falls.lethal);
	}
	ae_falls.last_report_tick = now;
}

/* a walking bot that gave up its goal at an edge: a roaming place 5 to 30 units off on about its own level, away from
the edge (more than a quarter turn from the way it was held), the nearest to 12 units; FALSE for none (bots.c then
chooses, as ever). Boarding Action 10-20: a bot whose goal lay on the other ship, its path short, climbed at the hull's
edge, was held there, and chose a goal on the other ship again; before the guard it walked off. */
static boolean ae_falls_away_goal(struct bots_bot *bot, struct bots_tick *tick, real held_way)
{
	long index, best = NONE;
	real best_score = REAL_MAX;
	real hx = (real)cos(held_way), hy = (real)sin(held_way);

	for (index = 0; index < bots_globals.roam_point_count; index++)
	{
		real_point3d const *point = &bots_globals.roam_points[index];
		real px = point->x - tick->position.x, py = point->y - tick->position.y;
		real distance = (real)sqrt(px * px + py * py);

		if (distance < 5.0f || distance > 30.0f || bots_globals.roam_surfaces[index] == NONE ||
			(real)fabs(point->z - tick->position.z) > 2.5f || px * hx + py * hy > 0.0f)
		{
			continue;
		}
		if ((real)fabs(distance - 12.0f) < best_score)
		{
			best_score = (real)fabs(distance - 12.0f);
			best = index;
		}
	}
	if (best == NONE)
		return FALSE;
	bot->roam_index = (short)best;
	bot->hunting = FALSE;
	bots_set_goal(bot, _bots_goal_roam, &bots_globals.roam_points[best], bots_globals.roam_surfaces[best]);
	return TRUE;
}

/* (ae_bots_ut.inc, included after this file) */
static boolean ae_bots_ut_dodge(struct bots_bot *bot, struct bots_tick *tick, int level);
static void ae_bots_fear_tick(struct bots_bot *bot, struct bots_tick *tick, int level, boolean dodging);
/* (ae_bots_fireteam.inc, included before this file) */

/* CHASE HOP (0.9.4, ae_bots_combat_rules.h): in a fight, a target standing a hop above and near is run at, the jump
pressed near it; the ground under the way there must be no lethal drop. Its legs overrule the fight's strafe, as a
link's jump's do; TRUE while it drives (the edge guard leaves it be: it goes up) */
static boolean ae_bots_fight_chase_hop(struct bots_bot *bot, struct bots_tick *tick, int level, struct biped_datum *biped)
{
	struct ae_bots_combat_bot *state;
	struct player_datum *target_player;
	struct biped_datum *target_biped;
	real_point3d target_point, middle, ground;
	long bot_index = (long)(bot - bots_globals.bots);
	int hop;

	if (!tick->fighting || bot->target_player_index == NONE || TEST_FLAG(biped->biped.flags, _biped_airborne_bit) ||
		tick->object->object.parent_object_index != NONE)
	{
		return FALSE;
	}
	target_player = player_try_and_get(bot->target_player_index);
	if (!target_player || target_player->unit_index == NONE)
		return FALSE;
	target_biped = biped_try_and_get(target_player->unit_index);
	if (!target_biped || target_biped->object.parent_object_index != NONE)
		return FALSE;
	state = ae_bots_fight_state(bot, tick);
	target_point = target_biped->object.position;
	hop = ae_bots_combat_chase_hop(level, (float)(target_point.z - tick->position.z),
		(float)bots_distance2d(&target_point, &tick->position), state->retreat.active ? 1 : 0,
		ae_bots_vehicle_carrying(bot, bot_index) || ae_bots_objective_player_carries(ae_bots_objective_snapshot(),
			bot->player_index) ? 1 : 0, TEST_FLAG(target_biped->biped.flags, _biped_airborne_bit) ? 1 : 0);
	if (!hop)
		return FALSE;
	/* (the way there: ground under its middle, not far under the bot) */
	middle.x = 0.5f * (tick->position.x + target_point.x);
	middle.y = 0.5f * (tick->position.y + target_point.y);
	middle.z = tick->position.z + 0.2f;
	if (bots_surface_below(&middle, &ground) == NONE || tick->position.z - ground.z > 1.5f ||
		ae_falls_drop_deadly(tick->position.z, &ground))
	{
		return FALSE;
	}
	tick->moving = TRUE;
	tick->throttle_scale = 1.0f;
	tick->move_direction = (real)atan2(target_point.y - tick->position.y, target_point.x - tick->position.x);
	if (hop == 2 && (tick->now - state->chase_hop_tick >= AE_BOTS_CHASE_HOP_GAP_TICKS || tick->now < state->chase_hop_tick))
	{
		state->chase_hop_tick = tick->now;
		tick->flags |= FLAG(_unit_control_jump_bit);
		if (ae_bots_events_on())
		{
			char extra[64];

			snprintf(extra, sizeof(extra), "kind=chase rise=%.1f x=%.1f y=%.1f z=%.1f", target_point.z - tick->position.z,
				tick->position.x, tick->position.y, tick->position.z);
			ae_bots_event_line(bot_index, tick->now, "jump", -1, -1.0f, extra);
		}
	}
	return TRUE;
}

/* (bots.c's bots_bot_update, after the unstick) THE GUARD */
static void ae_bots_falls_guard(struct bots_bot *bot, struct bots_tick *tick)
{
	long bot_index = (long)(bot - bots_globals.bots);
	struct ae_falls_bot *falls;
	struct ae_travel_bot *travel;
	struct biped_datum *biped;
	int level = ae_bots_level_of_skill(ae_bots_movement_skill(bots_globals.skill));
	long unit_index = tick->player->unit_index;
	real lethal, keep_lethal, lookahead;
	boolean carried = FALSE, level_kept = FALSE;
	real carry_way = 0.0f;

	if (bot_index < 0 || bot_index >= BOTS_BRAINS || !tick->object || tick->object->object.parent_object_index != NONE)
		return;
	falls = &ae_falls_bots[bot_index];
	ae_falls_report(tick->now);
	ae_falls_deadly_watch(falls, tick);
	ae_falls_flee(bot, falls, tick, ae_bots_level_of_skill(ae_bots_shooting_skill(bots_globals.skill)), level);
	ae_falls_throw(bot, tick);
	ae_falls_explosive(bot, tick, ae_bots_level_of_skill(ae_bots_shooting_skill(bots_globals.skill)));
	/* (a power item on a stand: the run at it and the jump, ae_bots_power.inc) */
	ae_bots_power_mount(bot, tick);
	/* (the UT spike: off the way of a slow shot or a vehicle; the edge checks below still hold a step. MOVEMENT PRIORITY:
	a dodge in hand is the last word over the squad's roles, which neither turn nor throttle it
	(ae_bots_tactical_move_allowed); the edge guard below, the lethal drop's veto, comes after both) */
	{
		/* (a squad a person leads: its members' walk is the tether's, fight or not (ae_bots_fireteam_tether): the roles never
		take one out of the radius; a dodge, still the last word, and the edge guard come after) */
		boolean tethered = ae_bots_fireteam_tether(bot, tick);
		boolean dodging = ae_bots_ut_dodge(bot, tick, level);

		/* (the F.E.A.R. spike: the squad's roles, flank and suppress) */
		ae_bots_fear_tick(bot, tick, level, dodging || tethered || ae_bots_round_revive_pending(bot));
	}
	travel = ae_travel_state(bot);
	biped = biped_try_and_get(unit_index);
	ae_travel_jump_drive(bot, tick);
	/* (a pad's last steps are guarded as any, as is a fight on the way: Boarding Action 10-20, a bot fighting at a pad's
	entry walked off the ship) */
	/* (a layout or found jump at its entry or in the air, fighting or not: the way to the edge is the way; Wizard 11-08,
	bots fighting for the hill were turned back at the platform's edge and never jumped) */
	/* (no place walked to known yet: the last bot's is not this one's) */
	ae_falls_target_known = FALSE;
	/* (0.9.4: a bot on a ladder climbs it; no edge to guard there) */
	if (ae_travel_ladder_drive(bot, tick))
		return;
	if (biped && ae_bots_fight_chase_hop(bot, tick, level, biped))
		return;
	if (!biped)
		return;
	if (travel && travel->via >= 0 && travel->phase != _ae_travel_phase_to_entry &&
		ae_travel.links[travel->via].kind == AE_BOTS_TRAVEL_LINK_JUMP)
	{
		/* (0.9.4, ae_bots_edge_link_step_checked: at the entry, on the ground, a step not along the jump's way, as to the
		start of its run-up over a ledge behind, is held when it is lethal) */
		struct ae_bots_travel_link const *link = &ae_travel.links[travel->via];

		if (travel->phase == _ae_travel_phase_at_entry && tick->moving && tick->throttle_scale > 0.0f &&
			!TEST_FLAG(tick->flags, _unit_control_jump_bit) && !TEST_FLAG(biped->biped.flags, _biped_airborne_bit) &&
			ae_bots_edge_link_step_checked((real)cos(tick->move_direction -
				(real)atan2(link->exit.y - link->entry.y, link->exit.x - link->entry.x))))
		{
			lethal = ae_falls_lethal_distance(bot->player_index, tick->object->object.body_vitality);
			if (ae_falls_way_lethal(unit_index, &tick->position, tick->move_direction, ae_bots_edge_lookahead(level),
				lethal))
			{
				tick->moving = FALSE;
				ae_falls.steps_held++;
				if (ae_bots_events_on() && tick->now - falls->edge_event_tick >= TICKS_PER_SECOND)
				{
					char extra[80];

					falls->edge_event_tick = tick->now;
					snprintf(extra, sizeof(extra), "what=held link=%d x=%.1f y=%.1f z=%.1f", (int)travel->via,
						tick->position.x, tick->position.y, tick->position.z);
					ae_bots_event_line(bot_index, tick->now, "edge", -1, -1.0f, extra);
				}
			}
		}
		return;
	}
	/* (RECRUIT and EASY, in a fight, look now and then: a half second at a time) */
	if (ae_bots_combat_hash01((unsigned long)bot->player_index ^ 0x6a09e667UL, (unsigned long)(tick->now / 15)) >=
		ae_bots_edge_attention(level, tick->fighting))
	{
		return;
	}
	lethal = ae_falls_lethal_distance(bot->player_index, tick->object->object.body_vitality);
	ae_falls.lethal = lethal;
	/* (0.9.4, ae_bots_edge_keep_level_lethal: no bot takes a floor's drop, its landing's stun as good as a pit) */
	keep_lethal = ae_bots_edge_keep_level_lethal(lethal,
		ae_bots_vehicle_carrying(bot, bot_index) || ae_bots_objective_player_carries(ae_bots_objective_snapshot(),
		bot->player_index));
	level_kept = keep_lethal < lethal;
	lethal = keep_lethal;
	/* (walking, not fighting: the place it walks to, for a small drop towards it) */
	ae_falls_target_known = FALSE;
	if (!tick->fighting && bot->goal_kind != _bots_goal_none)
	{
		ae_falls_target_known = TRUE;
		ae_falls_target = bot->goal_point;
		if (bot->path.valid && bot->path.step_index < bot->path.step_count)
			ae_falls_target = bot->path.steps[(long)bot->path.step_index].point;
	}
	lookahead = ae_bots_edge_lookahead(level);
	/* MOMENTUM: where its speed carries it, half a second on past the look ahead (Boarding Action 10-20: a bot turned
	back at the brink by the guard slid over it) */
	{
		real_vector3d const *velocity = &tick->object->object.translational_velocity;
		real speed = (real)sqrt(velocity->i * velocity->i + velocity->j * velocity->j);

		if (speed > 0.02f)
		{
			carry_way = (real)atan2(velocity->j, velocity->i);
			carried = ae_falls_way_lethal(unit_index, &tick->position, carry_way, lookahead + speed * TICKS_PER_SECOND * 0.5f,
				lethal);
		}
	}
	/* (standing still, yet carried towards a lethal drop: against its speed) */
	if (carried && !tick->moving)
	{
		tick->move_direction = carry_way + BOTS_PI;
		tick->moving = TRUE;
		tick->throttle_scale = 1.0f;
		ae_falls.steps_turned++;
	}
	else if (tick->moving && tick->throttle_scale > 0.0f)
	{
		/* (debug.bot_events: the probes of its way, near and far, as it chose it) */
		if (ae_bots_events_on() && !TEST_FLAG(biped->biped.flags, _biped_airborne_bit))
		{
			char near[24];

			ae_falls_step_lethal(unit_index, &tick->position, tick->move_direction, lookahead * 0.5f, AE_FALLS_STEP_RISE,
				lethal);
			snprintf(near, sizeof(near), "%s", ae_falls_probe_note);
			ae_falls_step_lethal(unit_index, &tick->position, tick->move_direction, lookahead, AE_FALLS_STEP_RISE, lethal);
			snprintf(falls->probe, sizeof(falls->probe), "%s,%s", near, ae_falls_probe_note);
		}
		if (ae_falls_way_lethal(unit_index, &tick->position, tick->move_direction, lookahead, lethal) ||
			(carried && (real)cos(tick->move_direction - carry_way) > -0.3f))
		{
			real way = tick->move_direction;
			boolean found = FALSE;
			int attempt;
			/* (a shot bot's floor drop, not lethal at the map's lethal drop: its level kept) */
			boolean kept = level_kept && !carried &&
				!ae_falls_way_lethal(unit_index, &tick->position, way, lookahead, ae_falls.lethal);

			if (kept)
				ae_falls.levels_kept++;

			/* (a fight: the strafe turns to the other side, tried first, when that way is clear; a way taken brakes
			what its speed carries it to) */
			if (tick->fighting && ae_bots_fight_strafe_may_flip(bot, tick) &&
				!ae_falls_way_lethal(unit_index, &tick->position, way + BOTS_PI, lookahead, lethal) &&
				!(carried && (real)cos(way + BOTS_PI - carry_way) > -0.3f))
			{
				bot->strafe_sign = -bot->strafe_sign;
				ae_bots_fight_strafe_flipped(bot, tick, "edge");
				tick->move_direction = way + BOTS_PI;
				found = TRUE;
			}
			for (attempt = 0; !found && attempt < AE_BOTS_EDGE_TURNS; attempt++)
			{
				real other = way + ae_bots_edge_turn(attempt, bot->strafe_sign < 0.0f ? -1 : 1, !tick->fighting);

				if (!ae_falls_way_lethal(unit_index, &tick->position, other, lookahead, lethal) &&
					!(carried && (real)cos(other - carry_way) > -0.3f))
				{
					tick->move_direction = other;
					found = TRUE;
				}
			}
			/* (none: against its speed, to brake) */
			if (!found && carried)
			{
				tick->move_direction = carry_way + BOTS_PI;
				found = TRUE;
			}
			if (found)
				ae_falls.steps_turned++;
			else
			{
				tick->moving = FALSE;
				ae_falls.steps_held++;
			}
			/* (the fight log: once a second at most) */
			if (ae_bots_events_on() && tick->now - falls->edge_event_tick >= TICKS_PER_SECOND)
			{
				char extra[112];

				falls->edge_event_tick = tick->now;
				snprintf(extra, sizeof(extra), "what=%s%s fighting=%d air=%d x=%.1f y=%.1f z=%.1f", found ? "turned" : "held",
					kept ? " kept=level" : "", tick->fighting ? 1 : 0,
					TEST_FLAG(biped->biped.flags, _biped_airborne_bit) ? 1 : 0, tick->position.x, tick->position.y,
					tick->position.z);
				ae_bots_event_line(bot_index, tick->now, "edge", -1, -1.0f, extra);
			}
			/* (walking: its path or its climb goes over the edge; held a second in all (the ticks it was turned away
			between them counted out after 3 s), the goal is given up; a power item's for all of them, 30 s: Boarding
			Action 10-20, bots on one ship went for the power items of the other one after the other, each out of
			reach, and stood at the hull's edge a minute or more, where they used to walk off) */
			if (!tick->fighting)
			{
				long power = ae_bots_power_bots[bot_index].entry;

				bot->climb_until_tick = 0;
				if (tick->now - falls->held_tick > AE_FALLS_HELD_FORGET)
					falls->held_ticks = 0;
				falls->held_tick = tick->now;
				if (++falls->held_ticks >= AE_BOTS_EDGE_GIVE_UP_TICKS)
				{
					short entry;

					falls->held_ticks = 0;
					/* (the fight log: which goal, where, so a spot that gives up goals over and over can be found) */
					if (ae_bots_events_on())
					{
						char extra[160];

						snprintf(extra, sizeof(extra), "what=give_up goal=%d power=%d x=%.1f y=%.1f z=%.1f gx=%.1f gy=%.1f gz=%.1f",
							(int)bot->goal_kind, power != NONE ? 1 : 0, tick->position.x, tick->position.y, tick->position.z,
							bot->goal_point.x, bot->goal_point.y, bot->goal_point.z);
						ae_bots_event_line(bot_index, tick->now, "edge", -1, -1.0f, extra);
					}
					if (bot->goal_kind == _bots_goal_item)
						bots_item_set_unreachable(bot, bot->item_index);
					if (power != NONE)
					{
						for (entry = 0; entry < AE_BOTS_POWER_ENTRIES; entry++)
							ae_bots_power_unreachable_until[bot_index][entry] = tick->now + TICKS_PER_SECOND * 30;
						ae_bots_power_bots[bot_index].entry = NONE;
					}
					bot->goal_kind = _bots_goal_none;
					bot->path.valid = FALSE;
					ae_bots_fear_flank_given_up(bot, tick->now);
					ae_falls_away_goal(bot, tick, way);
					ae_falls.goals_given_up++;
				}
			}
		}
	}
	ae_falls_target_known = FALSE;
	/* (a jump: where it lands, along its way or its look; NORMAL and up, any way about it, as its momentum and its air
	control may take it any way: Boarding Action 10-20, bots jumping in a fight at the hull's edge fell) */
	/* (not the jump onto a stand for a power item, which ae_bots_power_mount made 1.5 to 1.8 from it: a stand's other side
	is its floor, and the 8 turns of the test found a drop somewhere about, Temple: 24 jumps held in 10 minutes) */
	/* (nor a link's jump, pressed at its entry whether the bot fights or not: its landing is known ground, and the held jump
	left bots fighting for the hill at Wizard's platform edges, the rest of the way blocked: 12 held presses in 100 s, no
	hill entered) */
	if (TEST_FLAG(tick->flags, _unit_control_jump_bit) && !ae_bots_power_mount_jumped(bot_index, tick->now) &&
		!ae_travel_jump_flying(bot))
	{
		real way = tick->moving ? tick->move_direction : bot->yaw;
		boolean held = ae_falls_step_lethal(unit_index, &tick->position, way, AE_BOTS_EDGE_JUMP_REACH, AE_FALLS_JUMP_RISE,
			lethal);
		int eighth;

		for (eighth = 1; !held && level >= AE_BOTS_LEVEL_NORMAL && eighth < 8; eighth++)
		{
			held = ae_falls_step_lethal(unit_index, &tick->position, way + (real)eighth * BOTS_PI * 0.25f,
				AE_BOTS_EDGE_JUMP_REACH, AE_FALLS_JUMP_RISE, lethal);
		}
		if (held)
		{
			tick->flags &= ~FLAG(_unit_control_jump_bit);
			ae_falls.jumps_held++;
		}
	}
}
