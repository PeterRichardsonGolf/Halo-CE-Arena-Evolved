/*
AE_BOTS_FALLS.INC

Included by ae_bots_lan.inc, so at the end of bots.c, after the fight's and the getting around's glue (it reads their
state): the bots' deaths by their own hand (ae_bots_falls_rules.h; the owner, 2026-10-20: "can you have a helper
work on tuning bots for suicide and fall deaths. i find that is what makes our bots still look like bots").

- THE CAUSE OF A DEATH (debug.bot_events): damage.c's AE hook tells ae_bots_events.inc of every damage to a unit;
  a bot's unit's last one is kept here (its effect, its owner, whether it killed at once), and its fight log's
  ev=death line gets cause=<word> (ae_bots_death_cause_name), the damage's tag (dmg=), its owner (by=), where the bot
  last stood on the ground (gx= gy= gz=), how far below that it died (drop=), how long it was in the air (air=, s)
  and what it was doing as it left the ground (doing=: fight, retreat, charge, halt, back, strafe, walk:<goal>,
  unstick, climb, escape, link, jump), so the falls can be traced to the behaviour that walked them off.
*/

#include "objects/damage.h"
#include "ae_bots_falls_rules.h"

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
	long jump_tick;
};

static struct ae_falls_bot ae_falls_bots[BOTS_MAXIMUM];

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

/* (ae_bots_events.inc's damage note, debug.bot_events) a damage to a unit: a bot's living unit's is kept */
static void ae_bots_falls_damage(long owner_player_index, long object_index, long definition_index, unsigned long flags)
{
	struct unit_datum *unit = unit_try_and_get(object_index);
	long index;

	if (!unit || unit->unit.player_index == NONE || TEST_FLAG(unit->object.damage_flags, _object_dead_bit))
		return;
	for (index = 0; index < BOTS_MAXIMUM; index++)
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
#undef AE_FALLS_WORD
}

/* (ae_bots_events_end, debug.bot_events) each living bot's tick, its controls built: where it stands on the ground,
and what it does there */
static void ae_bots_falls_tick_end(struct bots_bot *bot, long bot_index, struct bots_tick *tick)
{
	struct ae_falls_bot *falls;
	struct biped_datum *biped;

	if (bot_index < 0 || bot_index >= BOTS_MAXIMUM || !tick->object)
		return;
	falls = &ae_falls_bots[bot_index];
	falls->last_point = tick->position;
	if (TEST_FLAG(tick->flags, _unit_control_jump_bit))
		falls->jump_tick = tick->now;
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

/* (ae_bots_events_begin) the words of a bot's ev=death line: its cause and how it came */
static void ae_bots_falls_death_words(struct bots_bot *bot, long bot_index, long now, char *out, size_t size)
{
	struct ae_falls_bot *falls;
	boolean known;
	char const *name = NULL;
	size_t used;
	char word[64];
	int cause;

	if (bot_index < 0 || bot_index >= BOTS_MAXIMUM)
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
		snprintf(out, size, "cause=%s dmg=%s by=%ld gx=%.1f gy=%.1f gz=%.1f drop=%.1f air=%.1f doing=%s",
			ae_bots_death_cause_name(cause), word,
			known ? ae_bots_event_absolute(falls->damage_owner) : -1L, falls->ground_point.x, falls->ground_point.y,
			falls->ground_point.z, falls->ground_point.z - falls->last_point.z,
			(real)(now - falls->ground_tick) / TICKS_PER_SECOND, falls->ground_doing[0] ? falls->ground_doing : "none");
	}
	else
	{
		snprintf(out, size, "cause=%s dmg=%s by=%ld", ae_bots_death_cause_name(cause),
			word, known ? ae_bots_event_absolute(falls->damage_owner) : -1L);
	}
	falls->damage_unit = NONE;
}
