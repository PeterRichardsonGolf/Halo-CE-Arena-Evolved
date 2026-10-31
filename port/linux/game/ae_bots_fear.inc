/* ae_bots_fear.inc: the F.E.A.R. SPIKE (owner 10-25: "implement the F.E.A.R. stuff ... human-like flanking, suppressing";
ideas from Jeff Orkin's published GDC 2006 talk "Three States and a Plan", written here from scratch, no data from the
game). HALO_BOTS_FEAR=0 turns it off (A/B); on by default in the spike, NORMAL and up.

A SQUAD LAYER over the bots' own fight: twice a second, the bots of a team on foot that fight the same enemy (seen in
the last 2 s) are one squad; with two or more, each gets a ROLE, kept while it makes sense:
- SUPPRESS: the one nearest the enemy with it in sight holds its ground (crouched now and then) and, when the enemy
  ducks out of sight, keeps firing at where it was seen for up to 2.5 s: the enemy stays pinned.
- FLANK: the next one or two (left, then right) go round: a place 100 degrees round the enemy from the suppressor, at
  6-12 units, on the walkable surfaces (the nearest roaming place to it), along their path, still shooting when they
  can; there or after 10 s, they fight as usual and the role is given again.
- ADVANCE: any others fight as before.
- HOLD (a squad of 4 or more, ae_bots_combat_rules.h's SQUADS THAT SPLIT): any others past 8 units from the enemy fight
  from where they are, not closing in, so about a third goes round, left and right, while the rest keep it busy.
A PLAYER IN THE SQUAD (owner 10-26: "none of my teammate bots squad with me"): a person (not a bot) of the team on
foot within 15 units of the squad's bot, the enemy within 35 units of them, is the squad's suppressor: no bot holds
back, one or two bots flank round the enemy from where the person stands, the rest push with them (one bot is enough).
A bot going for a vehicle's seat, or in one, is no squad member (owner 10-26: with the squads the bots ignored the
vehicles: a flanker's goal took over the walk to the seat).
Barks (what each one does) go to the log as ev=fear_* lines (voice later: AE's voice packs).

Called from ae_bots_falls_guard (a bot on foot, after the UT spike's dodge; the guard's edge checks still hold a step). */

#define AE_FEAR_SQUAD_TICKS (TICKS_PER_SECOND / 2)
#define AE_FEAR_SUPPRESS_TICKS (TICKS_PER_SECOND * 5 / 2)

enum
{
	AE_FEAR_NONE = 0,
	AE_FEAR_SUPPRESS,
	AE_FEAR_FLANK,
	AE_FEAR_ADVANCE,
	AE_FEAR_HOLD
};

struct ae_fear_bot
{
	short role;
	short side;			/* a flank's side: +1 / -1 */
	long target;			/* the enemy its role is about */
	long role_tick;
	long flank_ticks;		/* how long its flank may last */
	long flank_unseen_ticks;	/* how long it may lose sight of the enemy going round */
	real_point3d flank_point;
	long flank_surface;
	boolean flank_valid;
	long burst_tick;
};

static struct
{
	int on;
	long squad_tick;
	long last_report_tick;
	long squads, person_squads, flanks, flanks_reached, flanks_no_place, holds, suppress_ticks, suppress_fire_ticks;
	struct ae_fear_bot bots[BOTS_BRAINS];
} ae_fear = { -1 };

static boolean ae_bots_fear_on(void)
{
	if (ae_fear.on < 0)
	{
		char const *value = getenv("HALO_BOTS_FEAR");

		ae_fear.on = !(value && value[0] == '0');
		platform_log("bots: fear spike %s (HALO_BOTS_FEAR)", ae_fear.on ? "on" : "off");
	}
	return ae_fear.on != 0;
}

static void ae_bots_fear_report(long now)
{
	if (now - ae_fear.last_report_tick < 60 * TICKS_PER_SECOND && now >= ae_fear.last_report_tick)
		return;
	if (ae_fear.last_report_tick > 0 && now > ae_fear.last_report_tick)
	{
		platform_log("bots: fear tick %ld: squads formed %ld (with a player %ld), flanks %ld (reached %ld, no place %ld), holds %ld, suppressing %.1f s (firing blind %.1f s)",
			now, ae_fear.squads, ae_fear.person_squads, ae_fear.flanks, ae_fear.flanks_reached, ae_fear.flanks_no_place, ae_fear.holds, (real)ae_fear.suppress_ticks / TICKS_PER_SECOND,
			(real)ae_fear.suppress_fire_ticks / TICKS_PER_SECOND);
	}
	ae_fear.last_report_tick = now;
}

static void ae_bots_fear_bark(long bot_index, char const *what, long target)
{
	if (ae_bots_events_on())
		platform_log("bots: ev=fear_%s bot=%ld target %ld", what, bot_index + 1, target & 0xFFFF);
}

/* the enemy's unit position (FALSE: gone) */
static boolean ae_bots_fear_target_point(long player_index, real_point3d *point)
{
	struct player_datum *player = player_index != NONE ? player_try_and_get(player_index) : NULL;

	if (!player || player->unit_index == NONE)
		return FALSE;
	{
		struct object_datum *unit = (struct object_datum *)object_try_and_get_and_verify_type(player->unit_index, _object_mask_unit);

		if (!unit)
			return FALSE;
		*point = unit->object.position;
	}
	ae_bots_vehicle_world_point(player->unit_index, point);
	return TRUE;
}

/* LEVELS (owner 10-26: "HEROIC kind of feels like LEGENDARY used to"; rockets fired blind at him behind cover "crazy good
at NORMAL"): how many flank, how soon, and what a suppressor fires blind
- NORMAL: one flanker, once the enemy has been in sight 2 s; no blind fire.
- HEROIC: one flanker (two in a squad of four or more), after 1 s; blind fire with bullets only (no explosive).
- LEGENDARY, DEMON: two flankers at once; blind fire with any weapon. */
static long ae_bots_fear_max_flankers(int level, long squad_size)
{
	return ae_bots_combat_squad_flankers(level, squad_size);
}

static long ae_bots_fear_flank_delay_ticks(int level)
{
	return level >= AE_BOTS_LEVEL_LEGENDARY ? 0 : level == AE_BOTS_LEVEL_HEROIC ? TICKS_PER_SECOND : 2 * TICKS_PER_SECOND;
}

static boolean ae_bots_fear_blind_fire(int level, boolean explosive)
{
	return level >= AE_BOTS_LEVEL_LEGENDARY || (level == AE_BOTS_LEVEL_HEROIC && !explosive);
}

/* a roof within 8 units over a point */
static boolean ae_bots_fear_roofed(real_point3d const *point)
{
	struct collision_result collision;
	real_point3d start = *point;
	real_vector3d up = { 0.0f, 0.0f, 8.0f };

	start.z += 0.2f;
	return collision_test_vector(FLAG(_collision_test_structure_bit) | FLAG(_collision_test_front_facing_surfaces_bit),
		&start, &up, NONE, &collision) && collision.type == _collision_result_structure;
}

/* (the log) a flank's start, end or no place: its bot's own line */
static void ae_bots_fear_flank_log(struct ae_fear_bot const *fear, long now, char const *event, char const *extra)
{
	if (ae_bots_events_on())
		ae_bots_event_line((long)(fear - ae_fear.bots), now, event, ae_bots_event_absolute(fear->target), -1.0f, extra);
}

/* (the log) how a flank ended: why=reached|gone|target|unseen|time, the seconds it took, how far it still had to go */
static void ae_bots_fear_flank_end(struct bots_bot const *bot, struct ae_fear_bot const *fear, long now, char const *why)
{
	struct object_datum *unit = bot->unit_index != NONE ?
		(struct object_datum *)object_try_and_get_and_verify_type(bot->unit_index, _object_mask_unit) : NULL;
	char extra[80];

	snprintf(extra, sizeof(extra), "why=%s seconds=%.1f left=%.1f", why, (double)(now - fear->role_tick) / TICKS_PER_SECOND,
		unit && fear->flank_valid ? (double)bots_distance3d(&unit->object.position, &fear->flank_point) : -1.0);
	ae_bots_fear_flank_log(fear, now, "flank_end", extra);
}

/* a flank place: round the enemy from the suppressor, the nearest roaming place to it; indoors only when the enemy is
indoors too (owner, Sidewinder: a group of blue bots made a drive for a tight interior) */
static boolean ae_bots_fear_flank_place(struct ae_fear_bot *fear, real_point3d const *enemy, real_point3d const *suppressor,
	real_point3d const *self, long squad_size)
{
	real base = (real)atan2(suppressor->y - enemy->y, suppressor->x - enemy->x);
	real angle = base + (real)fear->side * 100.0f * BOTS_DEGREES;
	real radius = PIN(bots_distance2d(self, enemy), 6.0f, 12.0f);
	real_point3d want;
	long index, best = NONE;
	/* (a wide squad on an open map: the roaming places lie far apart) */
	real best_distance = squad_size >= AE_BOTS_COMBAT_WIDE_SQUAD ? 14.0f : 7.0f;

	want.x = enemy->x + (real)cos(angle) * radius;
	want.y = enemy->y + (real)sin(angle) * radius;
	want.z = enemy->z;
	for (index = 0; index < bots_globals.roam_point_count; index++)
	{
		real distance = bots_distance3d(&bots_globals.roam_points[index], &want);

		if (distance < best_distance && fabs(bots_globals.roam_points[index].z - enemy->z) < 3.0f)
		{
			best_distance = distance;
			best = index;
		}
	}
	{
		/* (a wide squad also goes round by the ledges and tunnels) */
		long place = NONE;

		for (index = 0; squad_size >= AE_BOTS_COMBAT_WIDE_SQUAD && index < ae_squads.count; index++)
		{
			real distance = bots_distance3d(&ae_squads.points[index].point, &want);

			if (distance < best_distance && ae_squads.drops[index] < AE_SQUADS_DROPS_TO_LEAVE_OUT)
			{
				best_distance = distance;
				place = index;
			}
		}
		if (place != NONE && ae_bots_fear_roofed(&ae_squads.points[place].point) <= ae_bots_fear_roofed(enemy))
		{
			fear->flank_point = ae_squads.points[place].point;
			fear->flank_surface = ae_squads.points[place].surface;
			best = NONE;
		}
		else if (best == NONE || ae_bots_fear_roofed(&bots_globals.roam_points[best]) > ae_bots_fear_roofed(enemy))
			return FALSE;
		else
		{
			fear->flank_point = bots_globals.roam_points[best];
			fear->flank_surface = bots_globals.roam_surfaces[best];
		}
	}
	fear->flank_valid = TRUE;
	fear->flank_ticks = (long)(ae_bots_combat_flank_seconds(squad_size, bots_distance3d(self, &fear->flank_point)) * TICKS_PER_SECOND);
	fear->flank_unseen_ticks = (long)(ae_bots_combat_flank_unseen_seconds(squad_size) * TICKS_PER_SECOND);
	return TRUE;
}

/* a wide squad's flanker tries the other side too */
static boolean ae_bots_fear_flank_either_side(struct ae_fear_bot *fear, real_point3d const *enemy, real_point3d const *suppressor,
	real_point3d const *self, long squad_size)
{
	if (ae_bots_fear_flank_place(fear, enemy, suppressor, self, squad_size))
		return TRUE;
	if (squad_size >= AE_BOTS_COMBAT_WIDE_SQUAD)
	{
		fear->side = (short)-fear->side;
		if (ae_bots_fear_flank_place(fear, enemy, suppressor, self, squad_size))
			return TRUE;
	}
	ae_fear.flanks_no_place++;
	/* (the log: a held bot asks again and again; once in 5 s a bot) */
	{
		static long logged_tick[BOTS_BRAINS];
		long now = game_time_get(), bot_index = (long)(fear - ae_fear.bots);

		if (now < logged_tick[bot_index] || now - logged_tick[bot_index] >= 5 * TICKS_PER_SECOND)
		{
			char extra[80];

			logged_tick[bot_index] = now;
			snprintf(extra, sizeof(extra), "squad=%ld enemy_x=%.1f enemy_y=%.1f far=%.1f", squad_size, (double)enemy->x, (double)enemy->y,
				(double)bots_distance2d(self, enemy));
			ae_bots_fear_flank_log(fear, now, "flank_no_place", extra);
		}
	}
	return FALSE;
}

/* a person (not a bot) of the team on foot near a bot fighting an enemy (within 15 units of it, the enemy within 35 of
them): its index in player_data, its position */
static boolean ae_bots_fear_person_near(struct object_datum *unit, real_point3d const *enemy, real_point3d *where)
{
	struct data_iterator iterator;
	struct player_datum *player;

	data_iterator_new(&iterator, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
	{
		struct object_datum *person;
		long index;
		boolean bot = FALSE;

		if (player->unit_index == NONE)
			continue;
		for (index = 0; index < BOTS_BRAINS && !bot; index++)
			bot = bots_globals.bots[index].player_index == iterator.datum_index;
		person = (struct object_datum *)object_try_and_get_and_verify_type(player->unit_index, _object_mask_unit);
		if (bot || !person || person->object.parent_object_index != NONE ||
			game_team_is_enemy(unit->object.owner_team_index, person->object.owner_team_index) ||
			bots_distance3d(&person->object.position, &unit->object.position) > 15.0f ||
			bots_distance3d(&person->object.position, enemy) > 35.0f)
		{
			continue;
		}
		*where = person->object.position;
		return TRUE;
	}
	return FALSE;
}

/* the squads: twice a second, the roles */
static void ae_bots_fear_squads(long now, int level)
{
	static struct object_datum *units[BOTS_BRAINS];
	long index, other;

	if (now - ae_fear.squad_tick < AE_FEAR_SQUAD_TICKS && now >= ae_fear.squad_tick)
		return;
	ae_fear.squad_tick = now;
	for (index = 0; index < BOTS_BRAINS; index++)
	{
		struct bots_bot *bot = &bots_globals.bots[index];
		struct ae_fear_bot *fear = &ae_fear.bots[index];

		/* (a role ends when its enemy is no longer fought, or a flank's time is up) */
		if (fear->role != AE_FEAR_NONE &&
			(bot->unit_index == NONE || bot->target_player_index != fear->target ||
				now - bot->target_seen_tick > (fear->role == AE_FEAR_FLANK ? fear->flank_unseen_ticks : 2 * TICKS_PER_SECOND) ||
				(fear->role == AE_FEAR_FLANK && now - fear->role_tick > fear->flank_ticks) || fear->role_tick > now))
		{
			if (fear->role == AE_FEAR_FLANK)
			{
				ae_bots_fear_flank_end(bot, fear, now, bot->unit_index == NONE ? "gone" : bot->target_player_index != fear->target ? "target" :
					now - bot->target_seen_tick > fear->flank_unseen_ticks ? "unseen" : "time");
			}
			fear->role = AE_FEAR_NONE;
			fear->flank_valid = FALSE;
		}
	}
	if (level < AE_BOTS_LEVEL_NORMAL)
		return;
	/* (each bot's unit, checked: a bot later in the update order may still name a unit that died this tick (a
	review: object_get on it asserts)) */
	for (index = 0; index < BOTS_BRAINS; index++)
	{
		long unit_index = bots_globals.bots[index].unit_index;

		units[index] = unit_index != NONE ?
			(struct object_datum *)object_try_and_get_and_verify_type(unit_index, _object_mask_unit) : NULL;
	}
	for (index = 0; index < BOTS_BRAINS; index++)
	{
		struct bots_bot *lead = &bots_globals.bots[index];
		long members[BOTS_BRAINS], count = 0, suppressor = NONE, flankers = 0;
		real_point3d enemy, person_point;
		real nearest = REAL_MAX;
		short side = 1;

		if (!units[index] || lead->target_player_index == NONE || now - lead->target_seen_tick > 2 * TICKS_PER_SECOND ||
			ae_bots_vehicle_bots[index].target_vehicle_index != NONE ||
			units[index]->object.parent_object_index != NONE || ae_bots_team_wingman_of(&bots_globals.bots[index]) != NONE ||
			(ae_fear.bots[index].role != AE_FEAR_NONE && ae_fear.bots[index].role != AE_FEAR_HOLD) ||
			!ae_bots_fear_target_point(lead->target_player_index, &enemy))
		{
			continue;
		}
		/* (its squad: the teammates on foot fighting the same enemy) */
		for (other = 0; other < BOTS_BRAINS; other++)
		{
			struct bots_bot *bot = &bots_globals.bots[other];

			if (!units[other] || bot->target_player_index != lead->target_player_index ||
				ae_bots_vehicle_bots[other].target_vehicle_index != NONE || ae_bots_vehicle_bots[other].vehicle_index != NONE ||
				now - bot->target_seen_tick > 2 * TICKS_PER_SECOND || units[other]->object.parent_object_index != NONE ||
				game_team_is_enemy(units[other]->object.owner_team_index, units[index]->object.owner_team_index))
			{
				continue;
			}
			members[count++] = other;
		}
		/* (a person of the team near: the squad's suppressor; every bot flanks or pushes with them) */
		if (ae_bots_fear_person_near(units[index], &enemy, &person_point))
		{
			ae_fear.squads++;
			ae_fear.person_squads++;
			for (other = 0; other < count; other++)
				flankers += ae_fear.bots[members[other]].role == AE_FEAR_FLANK;
			for (other = 0; other < count; other++)
			{
				long member = members[other];
				struct ae_fear_bot *fear = &ae_fear.bots[member];

				if (fear->role == AE_FEAR_SUPPRESS)
					fear->role = AE_FEAR_NONE;
				if (fear->role != AE_FEAR_NONE)
					continue;
				fear->target = lead->target_player_index;
				fear->role_tick = now;
				fear->side = side;
				if (flankers < ae_bots_fear_max_flankers(level, count + 1) &&
					now - bots_globals.bots[member].target_first_seen_tick >= ae_bots_fear_flank_delay_ticks(level) &&
					ae_bots_fear_flank_either_side(fear, &enemy, &person_point,
					&units[member]->object.position, count + 1))
				{
					fear->role = AE_FEAR_FLANK;
					flankers++;
					side = (short)-side;
					ae_fear.flanks++;
					{
						char flank_extra[64];

						snprintf(flank_extra, sizeof(flank_extra), "seconds=%.1f x=%.1f y=%.1f z=%.1f", (double)fear->flank_ticks / TICKS_PER_SECOND,
							(double)fear->flank_point.x, (double)fear->flank_point.y, (double)fear->flank_point.z);
						ae_bots_fear_flank_log(fear, now, "flank_start", flank_extra);
					}
					ae_bots_fear_bark(member, fear->side > 0 ? "flank_left_with_player" : "flank_right_with_player", fear->target);
					continue;
				}
				fear->role = AE_FEAR_ADVANCE;
			}
			continue;
		}
		if (count < 2)
			continue;
		/* (the suppressor: a role already given, else the nearest with the enemy in sight) */
		for (other = 0; other < count; other++)
		{
			struct ae_fear_bot *fear = &ae_fear.bots[members[other]];
			struct bots_bot *bot = &bots_globals.bots[members[other]];
			real distance = bots_distance3d(&units[members[other]]->object.position, &enemy);

			if (fear->role == AE_FEAR_SUPPRESS)
			{
				suppressor = members[other];
				break;
			}
			if (now - bot->target_seen_tick <= TICKS_PER_SECOND / 2 && distance < nearest)
			{
				nearest = distance;
				suppressor = members[other];
			}
			if (fear->role == AE_FEAR_FLANK)
			{
				flankers++;
				side = (short)-fear->side;
			}
		}
		if (suppressor == NONE)
			continue;
		ae_fear.squads++;
		if (ae_fear.bots[suppressor].role != AE_FEAR_SUPPRESS)
		{
			ae_fear.bots[suppressor].role = AE_FEAR_SUPPRESS;
			ae_fear.bots[suppressor].target = lead->target_player_index;
			ae_fear.bots[suppressor].role_tick = now;
			ae_bots_fear_bark(suppressor, "suppress", lead->target_player_index);
		}
		/* (one or two flankers, left then right; the rest advance) */
		for (other = 0; other < count; other++)
		{
			long member = members[other];
			struct ae_fear_bot *fear = &ae_fear.bots[member];

			/* (one holding asks again: too new to the fight, or no place, last time) */
			if (member == suppressor || (fear->role != AE_FEAR_NONE && fear->role != AE_FEAR_HOLD))
				continue;
			fear->target = lead->target_player_index;
			fear->role_tick = now;
			if (flankers < ae_bots_fear_max_flankers(level, count) &&
				now - bots_globals.bots[member].target_first_seen_tick >= ae_bots_fear_flank_delay_ticks(level))
			{
				fear->side = side;
				if (ae_bots_fear_flank_either_side(fear, &enemy, &units[suppressor]->object.position,
					&units[member]->object.position, count))
				{
					fear->role = AE_FEAR_FLANK;
					flankers++;
					side = (short)-side;
					ae_fear.flanks++;
					{
						char flank_extra[64];

						snprintf(flank_extra, sizeof(flank_extra), "seconds=%.1f x=%.1f y=%.1f z=%.1f", (double)fear->flank_ticks / TICKS_PER_SECOND,
							(double)fear->flank_point.x, (double)fear->flank_point.y, (double)fear->flank_point.z);
						ae_bots_fear_flank_log(fear, now, "flank_start", flank_extra);
					}
					ae_bots_fear_bark(member, fear->side > 0 ? "flank_left" : "flank_right", fear->target);
					continue;
				}
			}
			fear->role = count >= AE_BOTS_COMBAT_WIDE_SQUAD ? AE_FEAR_HOLD : AE_FEAR_ADVANCE;
		}
	}
}

/* (ae_bots_falls_guard, a bot on foot) the role's moves */
static void ae_bots_fear_tick(struct bots_bot *bot, struct bots_tick *tick, int level)
{
	long bot_index = (long)(bot - bots_globals.bots);
	struct ae_fear_bot *fear;

	if (!ae_bots_fear_on() || bot_index < 0 || bot_index >= BOTS_BRAINS)
		return;
	ae_bots_fear_report(tick->now);
	ae_bots_fear_squads(tick->now, level);
	fear = &ae_fear.bots[bot_index];
	/* (going for a seat: the seat first) */
	if (fear->role != AE_FEAR_NONE && ae_bots_vehicle_bots[bot_index].target_vehicle_index != NONE)
	{
		fear->role = AE_FEAR_NONE;
		fear->flank_valid = FALSE;
		return;
	}
	if (fear->role == AE_FEAR_FLANK && fear->flank_valid)
	{
		real direction;

		if (bots_distance3d(&tick->position, &fear->flank_point) < 2.0f)
		{
			ae_fear.flanks_reached++;
			ae_bots_fear_flank_end(bot, fear, tick->now, "reached");
			ae_bots_fear_bark(bot_index, "flanked", fear->target);
			fear->role = AE_FEAR_ADVANCE;
			fear->flank_valid = FALSE;
			return;
		}
		bots_set_goal(bot, _bots_goal_roam, &fear->flank_point, fear->flank_surface);
		if (!bot->path.valid && tick->now - bot->path_tick > TICKS_PER_SECOND / 3)
			bots_search(bot, tick->player->unit_index);
		if (bots_follow_path(bot, tick->player->unit_index, &tick->position, &direction))
		{
			tick->move_direction = direction;
			tick->moving = TRUE;
			tick->throttle_scale = 1.0f;
		}
		return;
	}
	/* (a wide squad's others: strafing as they like, no walking in on the enemy) */
	if (fear->role == AE_FEAR_HOLD)
	{
		if (tick->moving && tick->target_distance > 8.0f)
		{
			real to_enemy = (real)atan2(bot->target_last_position.y - tick->position.y, bot->target_last_position.x - tick->position.x);

			if (fabs(bots_angle_difference(tick->move_direction, to_enemy)) < 70.0f * BOTS_DEGREES)
			{
				tick->throttle_scale = MIN(tick->throttle_scale, 0.2f);
				ae_fear.holds++;
			}
		}
		return;
	}
	if (fear->role == AE_FEAR_SUPPRESS)
	{
		long unseen = tick->now - bot->target_seen_tick;

		ae_fear.suppress_ticks++;
		/* (holding its ground: no closing in; crouched a moment now and then) */
		if (tick->moving && tick->throttle_scale > 0.0f)
			tick->throttle_scale = MIN(tick->throttle_scale, 0.35f);
		if ((tick->now / (TICKS_PER_SECOND * 2)) % 3 == 0)
			tick->flags |= FLAG(_unit_control_crouch_modifier_bit);
		/* (the enemy out of sight: fire at where it was, bursts, up to 2.5 s) */
		if (unseen > TICKS_PER_SECOND / 2 && unseen < AE_FEAR_SUPPRESS_TICKS && tick->weapon_index != NONE &&
			ae_bots_fear_blind_fire(level, ae_falls_weapon_explosive(tick->weapon_index)))
		{
			real_point3d const *eye = bots_tick_eye(tick);
			real dx = bot->target_last_position.x - eye->x, dy = bot->target_last_position.y - eye->y;
			real dz = bot->target_last_position.z + 0.3f - eye->z;
			real desired_yaw = (real)atan2(dy, dx), desired_pitch = (real)atan2(dz, (real)sqrt(dx * dx + dy * dy));
			real turn = 8.0f * BOTS_DEGREES;

			bot->yaw += PIN(bots_angle_difference(desired_yaw, bot->yaw), -turn, turn);
			bot->pitch += PIN(desired_pitch - bot->pitch, -turn, turn);
			if ((tick->now - fear->burst_tick) % 20 < 12 &&
				fabs(bots_angle_difference(desired_yaw, bot->yaw)) < 6.0f * BOTS_DEGREES)
			{
				tick->flags |= FLAG(_unit_control_weapon_primary_trigger_bit);
				ae_fear.suppress_fire_ticks++;
			}
		}
		else
			fear->burst_tick = tick->now;
	}
}

/* (ae_bots_ut_slot) a new unit in the slot: no role */
static void ae_bots_fear_slot(long bot_index)
{
	if (bot_index >= 0 && bot_index < BOTS_BRAINS)
		csmemset(&ae_fear.bots[bot_index], 0, sizeof(ae_fear.bots[bot_index]));
}
