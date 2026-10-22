/*
AE_BOTS_COMBAT.INC

Included by ae_bots_lan.inc, so at the end of bots.c, where its state and static functions are seen: the glue
between bots.c's fight, look and walk (their AE hook lines, declared in ae_bots_skill.inc) and the fight by level
of ae_bots_combat.c / ae_bots_combat_rules.c. What each does, per level (RECRUIT .. LEGENDARY):

- the reaction to a new target: the skill's time 0.85 .. 1.35 of it, another each sighting; the look about
  every 3 to 5 ticks (no metronome);
- HEADSHOTS (shooting NORMAL and up): at a target whose shields are down, with a weapon that kills with a shot to
  the head, the aim goes to its head;
- MELEE and GRENADES (the shooting skill): ae_bots_combat_rules.h's; a throw leads the target's path
  (NORMAL and up) by holding the aim ahead of it while the throw is in the hand;
- RETREAT (movement NORMAL and up): low confidence held a moment (and its shields low) sends the bot backing off
  along a path to a place away from its target, still shooting; it does not go back to where it saw the enemy
  while it retreats; CLOSING IN (HEROIC and up): a won fight close by is finished by running in for a blow;
  HALTS (NORMAL and up): it stops sidestepping at a fight's start and now and then; JUMPS in a fight
  (NORMAL and up) at the level's rhythm (bots.c's random jumps are off: ae_bots_tick_skill);
- SHARED SIGHTINGS (BOT SKILL HEROIC, teammates within 10 units; LEGENDARY, any teammate; team games): a bot that
  sees an enemy tells its teammates who have no target of their own, who then go to where it was seen (bots.c's
  "where an enemy was last seen"), as a person hears a callout; RECRUIT .. NORMAL know only what they see;
- PRO TRAITS (2026-10-20): PRECISION PACING and the DESCOPE REFLEX (ae_bots_fight_in_range), the GRENADE AT THE
  LAST-SEEN SPOT (ae_bots_fight_prenade), HIGH GROUND when roaming (ae_bots_roam_height / ae_bots_roam_higher), and
  RETREAT as shields pop (ae_bots_combat_retreat_step).
*/

static long ae_bots_fight_index(struct bots_bot *bot)
{
	return (long)(bot - bots_globals.bots);
}

static struct ae_bots_combat_bot *ae_bots_fight_state(struct bots_bot *bot, struct bots_tick *tick)
{
	return ae_bots_combat_bot_state(ae_bots_fight_index(bot), tick->player->unit_index);
}

static void ae_bots_fight_event(struct bots_bot *bot, struct bots_tick *tick, char const *event, real distance,
	char const *extra)
{
	if (!ae_bots_events_on())
		return;
	ae_bots_combat_event(ae_bots_fight_index(bot), tick->now, ae_bots_skill_definition(bots_globals.skill)->name,
		event, ae_bots_event_absolute(bot->target_player_index), distance, extra);
}

static long ae_bots_fight_reaction(struct bots_bot *bot, struct bots_tick *tick, long reaction_ticks)
{
	struct player_datum const *target = player_get(bot->target_player_index);

	(void)tick;
	/* (a camouflaged target: twice as long) */
	reaction_ticks = (long)((real)reaction_ticks * ae_bots_combat_camo_reaction_scale(
		TEST_FLAG(unit_get(target->unit_index)->unit.flags, _unit_active_camouflaged_bit)) + 0.5f);
	return ae_bots_combat_reaction_ticks(ae_bots_level_of_skill(ae_bots_shooting_skill(bots_globals.skill)), reaction_ticks,
		ae_bots_combat_hash01((unsigned long)bot->player_index, (unsigned long)bot->target_first_seen_tick));
}

static real ae_bots_fight_camo_reach(struct unit_datum const *unit)
{
	return ae_bots_combat_camo_reach_scale(TEST_FLAG(unit->unit.flags, _unit_active_camouflaged_bit));
}

static real ae_bots_fight_camo_error(struct player_datum const *target)
{
	return ae_bots_combat_camo_error_scale(TEST_FLAG(unit_get(target->unit_index)->unit.flags,
		_unit_active_camouflaged_bit));
}

/* LONG RANGE: an error below the far target's floor is pushed out to it (0 to 30% past it, its side kept), so the aim
stays off by about the floor and a shot at a person of about that size hits about half the time */
static void ae_bots_fight_range_floor(struct bots_bot *bot, struct bots_tick *tick)
{
	real floor = ae_bots_combat_range_error_floor(ae_bots_level_of_skill(ae_bots_shooting_skill(bots_globals.skill)),
		tick->target_distance) * BOTS_DEGREES;
	real *errors[2];
	real scale[2] = { 1.0f, 0.6f };
	short index;

	if (floor <= 0.0f)
		return;
	errors[0] = &bot->aim_error_yaw;
	errors[1] = &bot->aim_error_pitch;
	for (index = 0; index < 2; index++)
	{
		real least = floor * scale[index];
		real random = bots_random_signed(bot);

		if ((real)fabs(*errors[index]) < least)
		{
			real sign = *errors[index] > 0.0f ? 1.0f : *errors[index] < 0.0f ? -1.0f : random < 0.0f ? -1.0f : 1.0f;

			*errors[index] = sign * least * (1.0f + 0.3f * (real)fabs(random));
		}
	}
}

/* VERTICAL VIEW (bots.c's target scoring, after its horizontal view): a target far above or below where the bot
looks is out of view, as for bots.c's horizontal view: unless within 3 units, it hurt the bot, or it is the one the
bot fights */
static boolean ae_bots_fight_in_vertical_view(struct bots_bot *bot, real_point3d const *eye, real_point3d const *point,
	real distance, boolean hurt, long player_index)
{
	real dx = point->x - eye->x, dy = point->y - eye->y;
	real elevation = (real)atan2(point->z - eye->z, (real)sqrt(dx * dx + dy * dy));

	if (distance <= 3.0f || hurt || player_index == bot->target_player_index)
		return TRUE;
	if (ae_bots_combat_in_vertical_view(ae_bots_level_of_skill(ae_bots_shooting_skill(bots_globals.skill)),
		elevation / BOTS_DEGREES, bot->pitch / BOTS_DEGREES))
	{
		return TRUE;
	}
	ae_bots_combat_count(AE_BOTS_COMBAT_OUT_OF_VERTICAL_VIEW);
	return FALSE;
}

/* LOOK DOWN (owner 10-20: "they don't look down when you are shooting below them"): bots_find_target casts its line of
sight to the enemy's middle. An enemy below a ledge (or above one) can show the bot its head alone, as the bot shows him
its own, and shoot it while the bot never takes it for a target: the head is tried when the middle is hidden, and the
bot then aims at it (ae_bots_fight_aim_point) for half a second from the last time only the head was seen. */
static long ae_bots_fight_head_only_tick[BOTS_BRAINS];

static boolean ae_bots_fight_head_in_sight(struct bots_bot *bot, long unit_index, real_point3d const *eye,
	long target_unit_index)
{
	long bot_index = (long)(bot - bots_globals.bots);
	real_point3d head;

	if (target_unit_index == NONE)
		return FALSE;
	unit_get_head_position(target_unit_index, &head);
	if (!bots_line_of_sight(unit_index, eye, &head, target_unit_index))
		return FALSE;
	if (bot_index >= 0 && bot_index < BOTS_BRAINS)
		ae_bots_fight_head_only_tick[bot_index] = game_time_get();
	ae_bots_combat_count(AE_BOTS_COMBAT_HEADS_SEEN);
	return TRUE;
}

/* whether only the target's head was seen in the last half second (ae_bots_fight_head_in_sight) */
static boolean ae_bots_fight_head_only(struct bots_bot *bot, long now)
{
	long bot_index = (long)(bot - bots_globals.bots);

	/* (a tick of an earlier game, after the clock's restart, is not now's) */
	return bot_index >= 0 && bot_index < BOTS_BRAINS && ae_bots_fight_head_only_tick[bot_index] &&
		now >= ae_bots_fight_head_only_tick[bot_index] &&
		now - ae_bots_fight_head_only_tick[bot_index] <= TICKS_PER_SECOND / 2;
}

/* a weapon's first trigger, NULL for none */
static struct weapon_trigger_definition *ae_bots_fight_trigger(long weapon_index)
{
	struct weapon_definition *definition = weapon_definition_get(weapon_get(weapon_index)->definition_index);

	if (definition->weapon.triggers.count <= 0)
		return NULL;
	return TAG_BLOCK_GET_ELEMENT(&definition->weapon.triggers, 0, struct weapon_trigger_definition);
}

/* whether the bot fires now (bots.c's fire, each tick of a fight):
- WEAPON RANGE: a shotgun's or a flamethrower's target out of its reach is not fired at;
- DESCOPE REFLEX (NORMAL and up): a weapon true only zoomed (the sniper rifle) is zoomed at a target past 6 units; a
  hit knocks the zoom out, and the bot zooms again after its level's moment, not firing past 10 units before;
- SCOPED RIFLE (NORMAL and up): the pack's battle rifle and carbine are zoomed at a target past half their aim
  assist's reach, as the DESCOPE
  REFLEX zooms, but fired unzoomed while the bot waits to zoom again;
- PRECISION PACING (NORMAL and up): a precision weapon that blooms (the pistol; held in bursts, as its trigger is
  not latched) is not fired at range while its error is over the level's limit, for a few ticks at most. */
static boolean ae_bots_fight_in_range(struct bots_bot *bot, struct bots_tick *tick)
{
	struct ae_bots_combat_bot *state;
	struct weapon_trigger_definition *trigger;
	int level = ae_bots_level_of_skill(ae_bots_shooting_skill(bots_globals.skill));
	long definition_index;
	boolean sniper;

	if (tick->weapon_index == NONE)
		return TRUE;
	if (tick->target_distance > ae_bots_combat_max_fire_distance(tag_get_name(weapon_get(tick->weapon_index)->definition_index)))
		return FALSE;
	/* (a vehicle's gun: as before) */
	if (tick->object->object.parent_object_index != NONE || !(trigger = ae_bots_fight_trigger(tick->weapon_index)))
		return TRUE;
	state = ae_bots_fight_state(bot, tick);
	definition_index = weapon_get(tick->weapon_index)->definition_index;
	sniper = TEST_FLAG(trigger->flags, _weapon_trigger_use_error_when_unzoomed_bit) &&
		weapon_definition_get(definition_index)->weapon.zoom_level_count > 0;
	if (sniper || ae_bots_combat_scoped_rifle(definition_index))
	{
		boolean zooms = sniper ? ae_bots_combat_zooms(level, tick->target_distance) :
			ae_bots_combat_rifle_zooms(level, tick->target_distance,
				weapon_definition_get(definition_index)->weapon.aim_assist_parameters.autoaim_distance);

		/* (hurt while zoomed: CE's unit_unzoom took the zoom; the moment before it zooms again) */
		if (bot->hurt_tick == tick->now && tick->unit->unit.current_zoom_level == NONE &&
			tick->now >= state->rescope_until && zooms)
		{
			state->rescope_until = tick->now + ae_bots_combat_rescope_ticks(level, bots_random_real(bot));
			ae_bots_combat_count(AE_BOTS_COMBAT_RESCOPES);
		}
		if (tick->now < state->rescope_until)
		{
			/* (a scoped rifle fires unzoomed meanwhile) */
			if (sniper)
				return (boolean)!ae_bots_combat_descoped_holds_fire(tick->target_distance);
		}
		else if (zooms)
			tick->action->desired_zoom_level = 0;
		if (sniper)
			return TRUE;
	}
	/* (a precision weapon that blooms: paced at range; the wait counted from its first held tick) */
	if (ae_bots_combat_precision_weapon(definition_index) &&
		trigger->projectile_error_angle_upper_bound > trigger->projectile_error_angle_lower_bound + 0.001f &&
		ae_bots_combat_pace_holds(level, tick->target_distance, weapon_get(tick->weapon_index)->weapon.triggers[0].error,
			state->pace_since == NONE ? 0 : tick->now - state->pace_since))
	{
		if (state->pace_since == NONE)
		{
			state->pace_since = tick->now;
			ae_bots_combat_count(AE_BOTS_COMBAT_PACED);
		}
		return FALSE;
	}
	state->pace_since = NONE;
	return TRUE;
}

/* a unit's distance from a place: a rider's from its seat in the world (ae_bots_vehicle_point), not its seat offset */
static real ae_combat_unit_distance(long unit_index, real_point3d const *position)
{
	real_point3d point = ae_bots_vehicle_point(unit_index);

	return bots_distance3d(&point, position);
}

static long ae_bots_fight_look_jitter(struct bots_bot *bot, struct bots_tick *tick)
{
	/* (HEARING, ae_bots_team.inc: every look, whatever the goal; bots_choose_goal is not reached while a vehicle,
	an objective or a power item has the bot) */
	ae_bots_team_hear(bot, bot->player_index, &tick->position, tick->now);
	return ae_bots_combat_look_jitter(ae_bots_combat_hash01((unsigned long)bot->player_index ^ 0x5bd1e995UL,
		(unsigned long)tick->now));
}

static void ae_bots_fight_share(struct bots_bot *bot, struct bots_tick *tick, long target)
{
	struct ae_bots_combat_bot *state;
	long index, told = 0;
	real reach = ae_bots_combat_share_distance(ae_bots_level_of_skill(bots_globals.skill));

	if (!game_engine_has_teams() || reach <= 0.0f || bot->target_last_surface_index == NONE)
		return;
	state = ae_bots_fight_state(bot, tick);
	if (tick->now - state->share_tick >= 0 && tick->now - state->share_tick < TICKS_PER_SECOND)
		return;
	state->share_tick = tick->now;
	for (index = 0; index < BOTS_BRAINS; index++)
	{
		struct bots_bot *other = &bots_globals.bots[index];
		struct player_datum *player;

		if (other == bot || other->player_index == NONE || other->target_player_index != NONE)
			continue;
		player = player_try_and_get(other->player_index);
		if (!player || player->unit_index == NONE || player->unit_index != other->unit_index ||
			player->team_index != tick->player->team_index ||
			ae_combat_unit_distance(player->unit_index, &tick->position) > reach)
		{
			continue;
		}
		/* (seen two seconds and a tick ago: not fought before it sees it, and then with a new aim's error;
		bots.c's goal takes it to where the enemy was) */
		other->target_player_index = target;
		other->target_seen_tick = tick->now - TICKS_PER_SECOND * 2 - 1;
		other->target_last_position = bot->target_last_position;
		other->target_last_surface_index = bot->target_last_surface_index;
		told++;
	}
	if (told)
	{
		char extra[32];

		ae_bots_combat_count(AE_BOTS_COMBAT_SHARES);
		snprintf(extra, sizeof(extra), "told=%ld", told);
		ae_bots_fight_event(bot, tick, "share", -1.0f, extra);
	}
}

static void ae_bots_fight_aim_point(struct bots_bot *bot, struct bots_tick *tick, real_point3d *aim_point)
{
	struct ae_bots_combat_bot *state = ae_bots_fight_state(bot, tick);
	long target_unit = player_get(bot->target_player_index)->unit_index;
	struct object_datum *target = object_get(target_unit);
	int level = ae_bots_level_of_skill(ae_bots_shooting_skill(bots_globals.skill));
	boolean head = tick->weapon_index != NONE && ae_bots_combat_aim_head(level, target->object.shield_vitality,
		ae_bots_combat_precision_weapon(weapon_get(tick->weapon_index)->definition_index));

	/* (LOOK DOWN: only the head over a ledge's lip: at the head, whatever the weapon) */
	if (!head && ae_bots_fight_head_only(bot, tick->now))
		head = TRUE;

	if (head)
	{
		unit_get_head_position(target_unit, aim_point);
		if (!state->head_aim)
			ae_bots_combat_count(AE_BOTS_COMBAT_HEAD_AIMS);
	}
	state->head_aim = head;
	/* (an explosive in hand, the rocket launcher or the fuel rod gun: at the feet, as players aim one, so that a near miss
	still catches the target in the blast (Octagon 10-20: aimed at the waist, 2% of the bots' rockets hit); the lead
	bots.c adds by the flight then carries the feet along) */
	if (!head && tick->weapon_index != NONE)
	{
		char const *name = tag_get_name(weapon_get(tick->weapon_index)->definition_index);

		if (name && (strstr(name, "rocket") || strstr(name, "plasma_cannon") || strstr(name, "fuel rod")))
			aim_point->z = MIN(aim_point->z, target->object.position.z + 0.1f);
	}
	/* (a grenade in the hand: the aim ahead of the target by its flight, from the throw's start) */
	if (tick->now < state->throw_until && state->throw_lead > 0.0f)
	{
		real_point3d const *eye = bots_tick_eye(tick);
		real ticks = 8.0f + bots_distance2d(eye, aim_point) / ae_bots_combat_grenade_speed(tick->player->unit_index);

		real dx, dy, lead;

		ticks = MIN(ticks, 60.0f) * state->throw_lead;
		dx = target->object.translational_velocity.i * ticks;
		dy = target->object.translational_velocity.j * ticks;
		/* (at most 3.5 units ahead: Infinite's ODST and Spartan throw that far in front of the feet) */
		lead = (real)sqrt(dx * dx + dy * dy);
		if (lead > 3.5f)
		{
			dx *= 3.5f / lead;
			dy *= 3.5f / lead;
		}
		aim_point->x += dx;
		aim_point->y += dy;
	}
}

static boolean ae_bots_fight_melee(struct bots_bot *bot, struct bots_tick *tick)
{
	struct ae_bots_combat_bot *state = ae_bots_fight_state(bot, tick);
	long target_unit = player_get(bot->target_player_index)->unit_index;
	int level = ae_bots_level_of_skill(ae_bots_shooting_skill(bots_globals.skill));
	int choice = ae_bots_combat_melee(level, tick->target_distance, tick->now >= bot->melee_tick,
		ae_bots_combat_back_turned(target_unit, &tick->position), object_get(target_unit)->object.shield_vitality,
		state->charging, bots_random_real(bot));

	/* (within reach, it waits its level's moment first; out of reach, it waits afresh next time) */
	if (choice == 0)
	{
		state->melee_ready_tick = NONE;
		return FALSE;
	}
	if (state->melee_ready_tick == NONE)
		state->melee_ready_tick = tick->now + ae_bots_combat_melee_delay(level, bots_random_real(bot));
	if (tick->now < state->melee_ready_tick)
		return FALSE;
	state->melee_ready_tick = NONE;
	if (choice == 2)
	{
		/* (it chose not to: asked again in a second) */
		bot->melee_tick = tick->now + TICKS_PER_SECOND;
		ae_bots_combat_count(AE_BOTS_COMBAT_MELEES_DECLINED);
		return FALSE;
	}
	if (choice == 1)
		ae_bots_combat_count(AE_BOTS_COMBAT_MELEES);
	return choice == 1;
}

static long ae_bots_fight_melee_ticks(struct bots_bot *bot, struct bots_tick *tick)
{
	(void)tick;
	return ae_bots_combat_melee_cooldown(ae_bots_level_of_skill(ae_bots_shooting_skill(bots_globals.skill)),
		bots_random_real(bot));
}

static void ae_bots_fight_grenade(struct bots_bot *bot, struct bots_tick *tick, long reaction_ticks)
{
	short shooting = ae_bots_shooting_skill(bots_globals.skill);
	int level = ae_bots_level_of_skill(shooting);
	struct ae_bots_combat_bot *state;
	long now = tick->now;
	boolean has = tick->unit->unit.grenade_counts[0] > 0 || tick->unit->unit.grenade_counts[1] > 0;
	char extra[48];

	if (now < bot->grenade_tick || !has)
		return;
	if (!ae_bots_combat_grenade(level, tick->target_distance, has, now - bot->target_first_seen_tick >= reaction_ticks,
		now - bot->target_first_seen_tick < TICKS_PER_SECOND * 3 / 2,
		ae_bots_combat_about_to_die(tick->object->object.shield_vitality, tick->object->object.body_vitality),
		ae_bots_skill_definition(shooting)->grenade_chance, bots_random_real(bot)))
	{
		return;
	}
	tick->flags |= FLAG(_unit_control_throw_grenade_bit);
	if (tick->unit->unit.grenade_counts[(long)tick->unit->unit.current_grenade_index] <= 0)
		tick->action->desired_grenade_index = tick->unit->unit.grenade_counts[0] > 0 ? 0 : 1;
	bot->grenade_tick = now + ae_bots_combat_grenade_cooldown(level, bots_random_real(bot));
	state = ae_bots_fight_state(bot, tick);
	state->throw_until = now + TICKS_PER_SECOND / 2;
	state->throw_lead = ae_bots_combat_grenade_lead(level);
	ae_bots_combat_count(AE_BOTS_COMBAT_GRENADES);
	snprintf(extra, sizeof(extra), "lead=%.1f", state->throw_lead);
	ae_bots_fight_event(bot, tick, "grenade", tick->target_distance, extra);
}

/* GRENADE AT THE LAST-SEEN SPOT (LEGENDARY and up; bots.c's walk, with no target in sight): once a lost contact, a
throw at where the target was last seen, 0.6 to 3 s after, the spot 5 to 20 units off and in its line of sight; the
aim held on it for the throw (half a second). Not while it retreats (the spot is forgotten then). */
static void ae_bots_fight_prenade(struct bots_bot *bot, struct bots_tick *tick)
{
	struct ae_bots_combat_bot *state = ae_bots_fight_state(bot, tick);
	int level = ae_bots_level_of_skill(ae_bots_shooting_skill(bots_globals.skill));
	boolean has = tick->unit->unit.grenade_counts[0] > 0 || tick->unit->unit.grenade_counts[1] > 0;

	if (tick->now < state->prenade_until)
	{
		real_point3d const *eye = bots_tick_eye(tick);
		real dx = state->prenade_point.x - eye->x, dy = state->prenade_point.y - eye->y;
		real dz = state->prenade_point.z - eye->z;
		real turn = ae_bots_skill_definition(ae_bots_shooting_skill(bots_globals.skill))->turn_degrees_per_tick * BOTS_DEGREES;
		real yaw_off = bots_angle_difference((real)atan2(dy, dx), bot->yaw);
		real pitch_off = (real)atan2(dz, (real)sqrt(dx * dx + dy * dy)) - bot->pitch;

		bot->yaw += PIN(yaw_off, -turn, turn);
		bot->pitch += PIN(pitch_off, -turn, turn);
		/* (the throw once the aim is on the spot) */
		if (state->prenade_until - tick->now > 1 && (real)fabs(yaw_off) < 0.1f && (real)fabs(pitch_off) < 0.1f)
		{
			tick->flags |= FLAG(_unit_control_throw_grenade_bit);
			if (tick->unit->unit.grenade_counts[(long)tick->unit->unit.current_grenade_index] <= 0)
				tick->action->desired_grenade_index = tick->unit->unit.grenade_counts[0] > 0 ? 0 : 1;
			state->prenade_until = tick->now + 1;
		}
		return;
	}
	if (level < AE_BOTS_LEVEL_LEGENDARY || bot->target_player_index == NONE || bot->target_last_surface_index == NONE ||
		state->prenade_seen_tick == bot->target_seen_tick || !has || tick->now < bot->grenade_tick ||
		tick->object->object.parent_object_index != NONE)
	{
		return;
	}
	{
		real seconds = (real)(tick->now - bot->target_seen_tick) / TICKS_PER_SECOND;
		real_point3d point = bot->target_last_position;
		struct player_datum *target = player_try_and_get(bot->target_player_index);
		real distance;

		if (seconds < 0.6f)
			return;
		state->prenade_seen_tick = bot->target_seen_tick;
		/* (about the target's middle where it stood) */
		point.z += 0.3f;
		distance = bots_distance3d(bots_tick_eye(tick), &point);
		if (!ae_bots_combat_prenade(level, seconds, distance, has, bots_random_real(bot)) ||
			!bots_line_of_sight(tick->player->unit_index, bots_tick_eye(tick), &point,
				target && target->unit_index != NONE ? target->unit_index : NONE))
		{
			return;
		}
		state->prenade_point = point;
		state->prenade_until = tick->now + TICKS_PER_SECOND / 2;
		bot->grenade_tick = tick->now + ae_bots_combat_grenade_cooldown(level, bots_random_real(bot));
		ae_bots_combat_count(AE_BOTS_COMBAT_PRENADES);
		ae_bots_fight_event(bot, tick, "prenade", distance, "");
	}
}

/* HIGH GROUND (LEGENDARY and up; bots.c's roaming place): a hunting bot's credit for a place above the enemy it
hunts (taken off the place's distance to it) */
static real ae_bots_roam_height(struct bots_bot *bot, real_point3d const *point, real_point3d const *enemy)
{
	(void)bot;
	return ae_bots_combat_high_ground(ae_bots_level_of_skill(bots_globals.skill), point->z - enemy->z);
}

/* a roaming bot's place (bots.c's random one, index): LEGENDARY and up weigh two more random places, not within 6
units, and take the one with the most credit for its height over the bot */
static short ae_bots_roam_higher(struct bots_bot *bot, real_point3d const *position, short index)
{
	int level = ae_bots_level_of_skill(bots_globals.skill);
	short best = index, tries;
	real best_credit = ae_bots_combat_high_ground(level, bots_globals.roam_points[index].z - position->z);

	if (level < AE_BOTS_LEVEL_LEGENDARY)
		return index;
	for (tries = 0; tries < 2; tries++)
	{
		short other = (short)(bots_random(bot) % (unsigned long)bots_globals.roam_point_count);
		real credit;

		if (bots_distance2d(&bots_globals.roam_points[other], position) < 6.0f)
			continue;
		credit = ae_bots_combat_high_ground(level, bots_globals.roam_points[other].z - position->z);
		if (credit > best_credit)
		{
			best_credit = credit;
			best = other;
		}
	}
	if (best != index)
		ae_bots_combat_count(AE_BOTS_COMBAT_HIGH_GROUND);
	return best;
}

/* a place to back off to: a roaming place 5 to 35 units off, not towards the target, the furthest from the target
for the least way; FALSE for none */
static boolean ae_bots_fight_retreat_goal(struct bots_bot *bot, struct bots_tick *tick, real_point3d const *target)
{
	long index, best = NONE;
	real best_score = -REAL_MAX;
	real tx = target->x - tick->position.x, ty = target->y - tick->position.y;
	real target_distance = (real)sqrt(tx * tx + ty * ty);

	for (index = 0; index < bots_globals.roam_point_count; index++)
	{
		real_point3d const *point = &bots_globals.roam_points[index];
		real px = point->x - tick->position.x, py = point->y - tick->position.y;
		real distance = (real)sqrt(px * px + py * py);
		real score;

		if (distance < 5.0f || distance > 35.0f || bots_globals.roam_surfaces[index] == NONE)
			continue;
		if (target_distance > 0.01f && (px * tx + py * ty) > 0.3f * distance * target_distance)
			continue;
		score = bots_distance2d(point, target) - 0.6f * distance;
		if (score > best_score)
		{
			best_score = score;
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

static void ae_bots_fight_move(struct bots_bot *bot, struct bots_tick *tick)
{
	struct ae_bots_combat_bot *state = ae_bots_fight_state(bot, tick);
	short movement = ae_bots_movement_skill(bots_globals.skill);
	int level = ae_bots_level_of_skill(movement);
	struct object_datum *target = object_get(player_get(bot->target_player_index)->unit_index);
	real own_shield = tick->object->object.shield_vitality, own_body = tick->object->object.body_vitality;
	real target_shield = target->object.shield_vitality, target_body = target->object.body_vitality;
	/* (where it is in the world, a rider too: ae_bots_vehicle.c) */
	real_point3d target_point = ae_bots_vehicle_point(player_get(bot->target_player_index)->unit_index);
	real to_target = (real)atan2(target_point.y - tick->position.y, target_point.x - tick->position.x);
	/* (OUTNUMBERED, ae_bots_team.inc: the enemies near past the teammates near take from it) */
	real confidence = ae_bots_combat_confidence(own_shield, own_body, target_shield, target_body, tick->target_distance,
		bots_weapon_range(tick->weapon_index), tick->weapon_index != NONE ? bots_weapon_loaded(tick->weapon_index) : 1.0f,
		tick->position.z - target_point.z) - ae_bots_team_outnumbered(bot, tick, level);
	int started, ended;
	boolean new_fight = tick->now - state->last_fight_tick > TICKS_PER_SECOND * 2 || tick->now < state->last_fight_tick;
	boolean retreating = ae_bots_combat_retreat_step(&state->retreat, level, confidence, own_shield, tick->now,
		ae_bots_combat_hash01((unsigned long)bot->player_index, (unsigned long)state->retreat.next_allowed), &started, &ended);
	char extra[48];

	/* HALTS (NORMAL and up): at the fight's start, and now and then at a new strafe: it stops sidestepping, still
	keeping its weapon's distance */
	if (new_fight)
	{
		state->halt_until = bot->target_first_seen_tick + ae_bots_combat_initial_halt_ticks(level,
			ae_bots_combat_hash01((unsigned long)bot->player_index ^ 0x27d4eb2dUL, (unsigned long)bot->target_first_seen_tick));
		state->next_vertical_tick = tick->now + ae_bots_combat_vertical_delay(level, bots_random_real(bot));
		state->last_strafe_until = bot->strafe_until_tick;
	}
	else if (bot->strafe_until_tick != state->last_strafe_until)
	{
		long halt = ae_bots_combat_halt_ticks(level, bots_random_real(bot), bots_random_real(bot));

		state->last_strafe_until = bot->strafe_until_tick;
		if (halt > 0)
		{
			state->halt_until = tick->now + halt;
			ae_bots_combat_count(AE_BOTS_COMBAT_HALTS);
		}
	}
	state->last_fight_tick = tick->now;
	if (tick->now < state->halt_until)
	{
		real range = bots_weapon_range(tick->weapon_index);

		if (tick->target_distance > range * 1.3f)
			tick->move_direction = to_target;
		else if (tick->target_distance < range * 0.6f)
			tick->move_direction = to_target + BOTS_PI;
		else
			tick->moving = FALSE;
	}

	if (started)
	{
		boolean pathed = ae_bots_fight_retreat_goal(bot, tick, &target_point);

		ae_bots_combat_count(AE_BOTS_COMBAT_RETREATS);
		snprintf(extra, sizeof(extra), "state=start conf=%.2f path=%d", confidence, pathed ? 1 : 0);
		ae_bots_fight_event(bot, tick, "retreat", tick->target_distance, extra);
	}
	if (ended)
	{
		/* (bots.c chooses its goal afresh) */
		bot->goal_kind = _bots_goal_none;
		ae_bots_fight_event(bot, tick, "retreat", tick->target_distance, "state=end");
	}
	if (retreating)
	{
		real direction;

		/* (not back to where it saw the enemy once out of its sight, nor off for a weapon) */
		bot->target_last_surface_index = NONE;
		bot->item_check_tick = MAX(bot->item_check_tick, tick->now + TICKS_PER_SECOND);
		if (bot->goal_kind == _bots_goal_roam &&
			bots_follow_path(bot, tick->player->unit_index, &tick->position, &direction))
		{
			tick->move_direction = direction;
		}
		else
			tick->move_direction = to_target + BOTS_PI + bot->strafe_sign * 0.5f;
		tick->moving = TRUE;
		tick->throttle_scale = 1.0f;
		state->charging = FALSE;
	}
	else
	{
		boolean charge = ae_bots_combat_charge(level, confidence, target_shield, target_body, tick->target_distance);

		if (charge)
		{
			tick->move_direction = to_target + bot->strafe_sign * 0.25f;
			tick->moving = TRUE;
			tick->throttle_scale = 1.0f;
			if (!state->charging)
			{
				ae_bots_combat_count(AE_BOTS_COMBAT_CHARGES);
				ae_bots_fight_event(bot, tick, "charge", tick->target_distance, "");
			}
		}
		state->charging = charge;
		/* STEPPING IN FOR A BLOW: a near target (its shields low below HEROIC) is closed on, not backed off from,
		so the blow's reach is met (ae_bots_fight_melee strikes) */
		if (!charge && ae_bots_combat_step_in(level, tick->target_distance, target_shield))
		{
			tick->move_direction = to_target + bot->strafe_sign * 0.3f;
			tick->moving = TRUE;
			tick->throttle_scale = 1.0f;
		}
	}
	/* WINGMAN's leash (ae_bots_team.inc): far from its player, back towards them while it shoots, unless retreating */
	if (!retreating)
	{
		real direction;

		if (ae_bots_team_leash(bot, tick, &direction))
		{
			tick->move_direction = direction + bot->strafe_sign * 0.4f;
			tick->moving = TRUE;
			tick->throttle_scale = 1.0f;
			state->charging = FALSE;
		}
	}
	/* JUMPS in a fight (NORMAL and up), at the level's rhythm */
	if (state->next_vertical_tick > 0 && tick->now >= state->next_vertical_tick)
	{
		long delay = ae_bots_combat_vertical_delay(level, bots_random_real(bot));

		state->next_vertical_tick = delay > 0 ? tick->now + delay : 0;
		if (delay > 0)
		{
			tick->flags |= FLAG(_unit_control_jump_bit);
			ae_bots_combat_count(AE_BOTS_COMBAT_JUMPS);
		}
	}
	ae_bots_combat_report(tick->now, ae_bots_skill_definition(ae_bots_shooting_skill(bots_globals.skill))->name,
		ae_bots_skill_definition(movement)->name);
}
