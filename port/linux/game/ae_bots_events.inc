/*
AE_BOTS_EVENTS.INC

Included by ae_bots_lan.inc, so at the end of bots.c: the bots' fight log
(debug.bot_events = true; off by default; analysed by tools/ae_test/bot_report.py).
A bot's tick calls ae_bots_events_begin (before it knows whether it is alive)
and ae_bots_events_end (when its controls are built); object_cause_damage
calls ae_bots_damage_note. Every line has the prefix "bot event: " and
key=value words:

  bot event: t=<tick> bot=<1..> skill=<name> ev=<event> target=<player or -1> dist=<world units or -1> ...

  ev=pos x= y= z= fighting=0|1   once a second while alive (stuck spots, fight and alive time)
  ev=seen                        the tick it takes a target (a new one, or after losing it 2 s)
  ev=first_shot since_seen=<ticks>   its first shot at that target
  ev=shot weapon=<tag name>      a round leaves the magazine (energy weapons: a trigger press); seen one tick late
  ev=hit dmg=<vitality lost>     a player took damage with this bot as the cause (aggregated per damage call)
  ev=melee                       the blow
  ev=strafe dir=<-1|1>           the side it strafes to changed
  ev=crouch state=start|end
  ev=throw                       a grenade left its hand (its grenades went down; seen when they do)
  ev=jump                        it pressed jump while fighting
  ev=kill / ev=death             its kills or deaths counter went up; a death's line has its cause and how it came
                                 (ae_bots_falls.inc: cause= dmg= by= and, while it stood on the ground before,
                                 gx= gy= gz= drop= air= doing=)
*/

boolean config_boolean(char const *name);

#define AE_BOTS_EVENTS_PENDING 32

struct ae_bots_event_state
{
	boolean known;
	long seen_tick;
	boolean first_shot_done;
	long last_target;
	real last_dist;
	long weapon_index;
	short rounds;
	boolean rounds_known;
	boolean trigger;
	real strafe_sign;
	boolean crouching;
	long kills, deaths;
	long next_pos_tick;
	/* its grenades, and the unit they were counted on */
	long grenade_unit;
	short grenades;
};

struct ae_bots_event_pending
{
	long bot_index;
	long victim_object_index;
	long victim_player_index;
	real before;
};

static struct ae_bots_event_state ae_bots_event_states[BOTS_MAXIMUM];
static struct ae_bots_event_pending ae_bots_event_pending[AE_BOTS_EVENTS_PENDING];
static long ae_bots_event_pending_count;

static boolean ae_bots_events_on(void)
{
	static long on = -1;

	if (on < 0)
		on = config_boolean("debug.bot_events") ? 1 : 0;
	return on > 0;
}

static void ae_bots_event_line(long bot_index, long now, char const *event, long target, real dist, char const *extra)
{
	platform_log("bot event: t=%ld bot=%ld skill=%s ev=%s target=%ld dist=%.1f%s%s", now, bot_index + 1,
		ae_bots_skill_definition(bots_globals.skill)->name, event, target, dist, extra[0] ? " " : "", extra);
}

static long ae_bots_event_absolute(long player_index)
{
	return player_index == NONE ? -1L : (long)DATUM_INDEX_TO_ABSOLUTE_INDEX(player_index);
}

static real ae_bots_event_vitality(struct object_datum *object)
{
	return object->object.body_vitality + object->object.shield_vitality;
}

/* the damage the pending calls did, now that each is over */
static void ae_bots_events_resolve(long now)
{
	long index;

	for (index = 0; index < ae_bots_event_pending_count; index++)
	{
		struct ae_bots_event_pending *pending = &ae_bots_event_pending[index];
		struct bots_bot *bot = &bots_globals.bots[pending->bot_index];
		struct object_datum *victim = (struct object_datum *)object_try_and_get_and_verify_type(pending->victim_object_index, _object_mask_unit);
		real after = victim ? ae_bots_event_vitality(victim) : 0.0f;
		real lost = pending->before - after;

		if (lost > 0.0005f)
		{
			struct player_datum *player = player_try_and_get(bot->player_index);
			real dist = -1.0f;
			char extra[48];

			if (player && player->unit_index != NONE && victim)
			{
				struct object_datum *own = object_get(player->unit_index);

				dist = bots_distance3d(&own->object.position, &victim->object.position);
			}
			snprintf(extra, sizeof(extra), "dmg=%.3f", lost);
			ae_bots_event_line(pending->bot_index, now, "hit", ae_bots_event_absolute(pending->victim_player_index), dist, extra);
		}
	}
	ae_bots_event_pending_count = 0;
}

/* (ae_bots_travel.inc and ae_bots_falls.inc, included after) */
static void ae_bots_travel_tick_end(void);
static void ae_bots_falls_damage(long owner_player_index, long object_index, long definition_index, unsigned long flags);
static void ae_bots_falls_tick_end(struct bots_bot *bot, long bot_index, struct bots_tick *tick);
static void ae_bots_falls_death_words(struct bots_bot *bot, long bot_index, long now, char *out, size_t size);

/* object_cause_damage: a bot's damage to a player's unit, to be measured when it is done; a bot's unit's damage, the
cause of its death (ae_bots_falls.inc) */
void ae_bots_damage_note(long owner_player_index, long object_index, long definition_index, unsigned long flags)
{
	struct object_datum *victim;
	struct unit_datum *unit;
	long index;

	if (!ae_bots_events_on() || !bots_globals.map_ready)
		return;
	ae_bots_falls_damage(owner_player_index, object_index, definition_index, flags);
	if (owner_player_index == NONE)
		return;
	ae_bots_events_resolve(game_time_get());
	unit = unit_try_and_get(object_index);
	if (!unit || unit->unit.player_index == NONE || unit->unit.player_index == owner_player_index)
		return;
	for (index = 0; index < BOTS_MAXIMUM; index++)
	{
		if (bots_globals.bots[index].player_index == owner_player_index)
			break;
	}
	if (index >= BOTS_MAXIMUM || ae_bots_event_pending_count >= AE_BOTS_EVENTS_PENDING)
		return;
	victim = (struct object_datum *)unit;
	ae_bots_event_pending[ae_bots_event_pending_count].bot_index = index;
	ae_bots_event_pending[ae_bots_event_pending_count].victim_object_index = object_index;
	ae_bots_event_pending[ae_bots_event_pending_count].victim_player_index = unit->unit.player_index;
	ae_bots_event_pending[ae_bots_event_pending_count].before = ae_bots_event_vitality(victim);
	ae_bots_event_pending_count++;
}

void ae_bots_events_begin(struct bots_bot *bot, long bot_index, struct bots_tick *tick)
{
	struct ae_bots_event_state *state;
	long kills, deaths;

	if (!ae_bots_events_on() || bot_index < 0 || bot_index >= BOTS_MAXIMUM)
		return;
	state = &ae_bots_event_states[bot_index];
	ae_bots_events_resolve(tick->now);
	kills = (long)tick->player->statistics.kills[0];
	deaths = (long)tick->player->statistics.deaths;
	if (!state->known || kills < state->kills || deaths < state->deaths)
	{
		/* (first sight, or a new game's counters) */
		state->known = TRUE;
		state->weapon_index = NONE;
		state->rounds_known = FALSE;
		state->strafe_sign = 0.0f;
		state->crouching = FALSE;
		state->seen_tick = NONE;
		state->last_target = -1;
		state->last_dist = -1.0f;
		state->next_pos_tick = tick->now + bot_index;
	}
	else
	{
		for (; state->kills < kills; state->kills++)
			ae_bots_event_line(bot_index, tick->now, "kill", state->last_target, state->last_dist, "");
		for (; state->deaths < deaths; state->deaths++)
		{
			char words[320];

			ae_bots_falls_death_words(bot, bot_index, tick->now, words, sizeof(words));
			ae_bots_event_line(bot_index, tick->now, "death", state->last_target, state->last_dist, words);
			state->crouching = FALSE;
			state->rounds_known = FALSE;
		}
	}
	state->kills = kills;
	state->deaths = deaths;
}


void ae_bots_events_end(struct bots_bot *bot, long bot_index, struct bots_tick *tick)
{
	struct ae_bots_event_state *state;
	long now = tick->now;
	boolean shot = FALSE;
	boolean trigger = TEST_FLAG(tick->flags, _unit_control_weapon_primary_trigger_bit);
	boolean crouching = TEST_FLAG(tick->flags, _unit_control_crouch_modifier_bit);
	long target;
	real dist;
	char extra[160];

	ae_bots_travel_tick_end();
	if (!ae_bots_events_on() || bot_index < 0 || bot_index >= BOTS_MAXIMUM)
		return;
	ae_bots_falls_tick_end(bot, bot_index, tick);
	state = &ae_bots_event_states[bot_index];
	if (bot->target_player_index != NONE)
	{
		struct player_datum *target_player = player_try_and_get(bot->target_player_index);

		state->last_target = target_player ? ae_bots_event_absolute(bot->target_player_index) : -1;
	}
	target = state->last_target;
	if (tick->fighting)
		state->last_dist = tick->target_distance;
	dist = state->last_dist;

	if (tick->fighting && bot->target_first_seen_tick == now && state->seen_tick != now)
	{
		state->seen_tick = now;
		state->first_shot_done = FALSE;
		ae_bots_event_line(bot_index, now, "seen", target, dist, "");
	}

	/* a round gone from the magazine (the weapon unchanged); an energy weapon: a press */
	if (tick->weapon_index != NONE)
	{
		struct weapon_datum *weapon = weapon_get(tick->weapon_index);
		short rounds = weapon->weapon.magazines[0].rounds_loaded;
		boolean has_rounds = weapon->weapon.magazines[0].rounds_total > 0 || rounds > 0;

		if (state->weapon_index == tick->weapon_index && state->rounds_known)
		{
			if (has_rounds)
				shot = rounds < state->rounds;
			else
				shot = trigger && !state->trigger && tick->fighting;
		}
		state->weapon_index = tick->weapon_index;
		state->rounds = rounds;
		state->rounds_known = TRUE;
		if (shot)
		{
			snprintf(extra, sizeof(extra), "weapon=%s", tag_get_name(weapon->definition_index));
			if (!state->first_shot_done && state->seen_tick != NONE)
			{
				char first[48];

				state->first_shot_done = TRUE;
				snprintf(first, sizeof(first), "since_seen=%ld", now - state->seen_tick);
				ae_bots_event_line(bot_index, now, "first_shot", target, dist, first);
			}
			ae_bots_event_line(bot_index, now, "shot", target, dist, extra);
		}
	}
	else
		state->weapon_index = NONE;
	state->trigger = trigger;

	if (TEST_FLAG(tick->flags, _unit_control_use_equipment_bit) && tick->fighting)
		ae_bots_event_line(bot_index, now, "melee", target, dist, "");
	if (tick->fighting && bot->strafe_sign != state->strafe_sign)
	{
		if (state->strafe_sign != 0.0f)
		{
			snprintf(extra, sizeof(extra), "dir=%d", bot->strafe_sign < 0.0f ? -1 : 1);
			ae_bots_event_line(bot_index, now, "strafe", target, dist, extra);
		}
		state->strafe_sign = bot->strafe_sign;
	}
	{
		short grenades = (short)(tick->unit ? tick->unit->unit.grenade_counts[0] + tick->unit->unit.grenade_counts[1] : 0);
		long unit = tick->unit ? tick->player->unit_index : NONE;

		if (unit != NONE && unit == state->grenade_unit && grenades < state->grenades)
			ae_bots_event_line(bot_index, now, "throw", target, dist, "");
		state->grenade_unit = unit;
		state->grenades = grenades;
	}
	if (tick->fighting && TEST_FLAG(tick->flags, _unit_control_jump_bit))
		ae_bots_event_line(bot_index, now, "jump", target, dist, "");
	if (crouching != state->crouching)
	{
		state->crouching = crouching;
		ae_bots_event_line(bot_index, now, "crouch", target, dist, crouching ? "state=start" : "state=end");
	}
	if (now >= state->next_pos_tick)
	{
		state->next_pos_tick = now + TICKS_PER_SECOND;
		snprintf(extra, sizeof(extra), "x=%.1f y=%.1f z=%.1f fighting=%d", tick->position.x, tick->position.y,
			tick->position.z, tick->fighting ? 1 : 0);
		ae_bots_event_line(bot_index, now, "pos", target, dist, extra);
	}
}
