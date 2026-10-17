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
  "where an enemy was last seen"), as a person hears a callout; RECRUIT .. NORMAL know only what they see.
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
	(void)tick;
	return ae_bots_combat_reaction_ticks(reaction_ticks,
		ae_bots_combat_hash01((unsigned long)bot->player_index, (unsigned long)bot->target_first_seen_tick));
}

static long ae_bots_fight_look_jitter(struct bots_bot *bot, struct bots_tick *tick)
{
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
	for (index = 0; index < BOTS_MAXIMUM; index++)
	{
		struct bots_bot *other = &bots_globals.bots[index];
		struct player_datum *player;

		if (other == bot || other->player_index == NONE || other->target_player_index != NONE)
			continue;
		player = player_try_and_get(other->player_index);
		if (!player || player->unit_index == NONE || player->unit_index != other->unit_index ||
			player->team_index != tick->player->team_index ||
			bots_distance3d(&object_get(player->unit_index)->object.position, &tick->position) > reach)
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

	if (head)
	{
		unit_get_head_position(target_unit, aim_point);
		if (!state->head_aim)
			ae_bots_combat_count(AE_BOTS_COMBAT_HEAD_AIMS);
	}
	state->head_aim = head;
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
	real confidence = ae_bots_combat_confidence(own_shield, own_body, target_shield, target_body, tick->target_distance,
		bots_weapon_range(tick->weapon_index), tick->weapon_index != NONE ? bots_weapon_loaded(tick->weapon_index) : 1.0f,
		tick->position.z - target_point.z);
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
