/* ae_bots_ut.inc: the UT SPIKE (owner 10-25: "spike test the next version of our bots"): three behaviours whose ideas
come from UT2004's bot scripts (read for ideas only),
written here from scratch for Halo's bots. HALO_BOTS_UT=0 turns all three off (A/B); on by default in the spike.

- DODGE (UT's projectile warning): a bot on foot sees a slow enemy shot (rocket, plasma, fuel rod; not bullets, not
  grenades: the falls guard has those) whose path passes near it, and steps off that path, across it, when the shot
  is far enough away for its level's reaction; better bots more often, and HEROIC up jump a rocket at their feet.
- CLEAR THE WAY (UT's clear path): a bot on foot steps aside from a moving vehicle whose way runs through it: a
  teammate's always (no more bots in front of the warthog), an enemy's as its level dodges.
- RUN-OVER CHARGE (UT's vehicle charge): a Ghost's driver fighting an enemy on foot ahead of it drives at a point
  past the enemy, not to it, so it keeps its speed and runs it over, then turns back on its route.
- WARTHOG GUNNER (owner 10-25: "less hesitant on the warthog turret ... acting like it can blow us up with it"):
  bots.c fires a vehicle's chain gun as a rifle: short bursts with pauses, only inside a body-width cone, which a
  turret swaying on a moving warthog rarely holds. The gunner now holds the trigger down while its target is in
  sight and its aim within a wider cone (a body and a half, a few degrees more for the sway).
- FLIP BACK (owner 10-25, Sidewinder: "flip and get back into the vehicles if they neutralize a threat that causes
  them to flip"): a bot thrown out of a flipped vehicle remembers it; once no enemy has been in its sight for 1.5 s it
  walks back, flips it (the action button: players.c offers the flip to one beside an upturned vehicle with no
  driver) and may board it again at once. Given up after 30 s, or when the vehicle is gone. Righted by anyone (owner:
  "bot Hall ran off when I flipped the vehicle"): it boards it again at once, any power item or goal aside: the gun if
  a teammate drives, else the wheel.
- SPARE TEAMMATES (owner 10-25, Sidewinder opening: two bots run over by their own teammates' vehicles in the first
  half minute, one after stepping aside twice): a driver whose way runs into a teammate on foot within 1.5 s swings
  its waypoint 5 units to the side away from it and eases off to half throttle.
- AROUND PARKED VEHICLES (owner 10-25, Sidewinder game 3: a group of blue bots stuck behind a stuck Ghost: four bots
  at (-13 -35) for the same goal, 'stuck' over and over): the walkable surfaces know nothing of vehicles, so a path
  can run through one standing still. A bot on foot whose way runs into a still vehicle within its size and a bit
  turns along its side, away from its middle, until past it.
- WAIT FOR RIDERS (owner 10-25, Sidewinder: "bots are taking off when others want to get into gun and seat"): a
  driver of a vehicle with room (its gunner seat empty, or a warthog's passenger seat) that is not fighting stops for a
  teammate on foot near it (within 3 units, or within 8 coming its way), up to 5 s at a time, then 6 s on before it
  waits again. HAILED (owner: "the old-school way of letting a driver know you want a ride was to send a round at
  them"): a teammate's shot passing within 2.5 units of the vehicle stops the driver for that teammate up to 8 s
  (fight or not), until it is aboard, gone further than 30 units, or dead.
- GIVE UP THE SEAT (owner 10-25: "bots would give up their seat or gun if you held action"): the game's own eviction
  works on campaign AI only (players.c: ai_try_vehicle_eviction needs an actor), so a teammate holding the action
  button half a second at a bot's seat (the game offers "evict" for it) now gets it: the bot gets out.
- TRACTION (owner 10-25: "the ice on Sidewinder is messing up bot driving"): no map data, the slide itself: when a
  vehicle's way over the ground strays from where it points (the sideways part of its speed over 35 %), the driver
  eases off the throttle (to 55 %, less the worse the slide) and steers half as hard until it
  grips again.
- WALL MARGIN (owner 10-25: "the bots are driving warthogs way close to walls"): the route's points come from the
  walkable surfaces, which run along walls. Six rays from the vehicle at hood height (35, 70 and 100 degrees each side,
  5 units): a wall nearer than 5 units pushes the waypoint away from it, up to 3.5 units, more the nearer it is.
- WARTHOG LAPS (owner 10-25, game 1: "less kamikaze, more making laps for the turret, smooth and evasive"): a
  warthog's driver fighting an enemy in reach circles it at a gunner's range, the waypoint well ahead on the circle
  (wide, smooth turns), the lap's way turned about now and then and when the enemy comes too near.

Called from AE code only: ae_bots_falls_guard (dodge, before the guard's edge checks, which may still hold a step),
ae_bots_vehicle_drive_tick (charge). */

#define AE_UT_DANGER_SHOT 1.6f		/* a shot passing this near (world units) is dodged */
#define AE_UT_DANGER_VEHICLE 2.2f	/* a vehicle's way this near */
#define AE_UT_SHOT_SPEED_MAX 1.6f	/* faster than this a tick (bullets, needles): not dodged */
#define AE_UT_LOOK_SECONDS 1.5f		/* a shot or a vehicle further off than this in time: not yet */
#define AE_UT_VEHICLE_SPEED_MIN 0.08f	/* a vehicle slower than this a tick is not coming */
#define AE_UT_CHARGE_RANGE 22.0f	/* a driver charges an enemy on foot this near */
#define AE_UT_CHARGE_PAST 7.0f		/* this far past it */
#define AE_UT_CHARGE_CONE (55.0f * BOTS_DEGREES)
#define AE_UT_LAP_REACH 40.0f		/* a warthog laps an enemy this near */
#define AE_UT_LAP_RADIUS 15.0f		/* at this range from it */
#define AE_UT_LAP_LEAD (50.0f * BOTS_DEGREES)	/* the waypoint this far on round the circle */

static void ae_bots_fear_slot(long bot_index);

struct ae_ut_bot
{
	long until_tick;		/* stepping aside until */
	real way;			/* its step's direction */
	boolean jump;
	long judged_object;		/* the last threat judged, and the verdict */
	boolean judged_dodge;
	real lap_sign;			/* a warthog's lap: +1 anticlockwise, -1 clockwise */
	long lap_turn_tick;		/* when it turns its lap about next */
	long flip_vehicle;		/* the flipped vehicle it goes back to (NONE: none), until */
	long flip_until_tick;
	long flip_press_tick;
	long wait_since_tick;		/* waiting for a rider since (0: not), and not again before */
	long wait_again_tick;
	long hailed_until_tick;		/* a teammate shot at the vehicle: stopped for it until, its player */
	long hailer;
	long evict_ticks;		/* a teammate's action held at its seat, ticks */
	long evict_seen_tick;		/* the last tick a teammate pressed action at its seat */
	long unit_index;		/* the unit this state is for: another (a respawn, a new player in the slot): all reset */
	long wall_tick;			/* the wall margin's last rays, and the push they gave */
	real wall_push_x, wall_push_y;
};

static struct
{
	int on;				/* -1 not read yet */
	long last_report_tick;
	long shots, vehicles_team, vehicles_enemy, jumps, charges, charge_ticks, laps, lap_ticks, lap_turns, gunner_ticks, flips_wanted, flips_done, spared, rounded, rider_waits, hails, seats_given, slide_ticks;
	struct ae_ut_bot bots[BOTS_BRAINS];
} ae_ut = { -1 };

/* (review: every bot walked the whole object table two or three times a tick) the projectiles, vehicles and
bipeds, listed once a tick for every bot's checks */
#define AE_UT_SCAN_MAXIMUM 512
static struct
{
	long tick;
	long projectile_count, vehicle_count, biped_count;
	long projectiles[AE_UT_SCAN_MAXIMUM], vehicles[AE_UT_SCAN_MAXIMUM], bipeds[AE_UT_SCAN_MAXIMUM];
} ae_ut_scan = { -1 };

static void ae_bots_ut_scan(long now)
{
	struct object_iterator iterator;

	if (ae_ut_scan.tick == now)
		return;
	ae_ut_scan.tick = now;
	ae_ut_scan.projectile_count = ae_ut_scan.vehicle_count = ae_ut_scan.biped_count = 0;
	object_iterator_new(&iterator, _object_mask_projectile, 0);
	while (object_iterator_next(&iterator) && ae_ut_scan.projectile_count < AE_UT_SCAN_MAXIMUM)
		ae_ut_scan.projectiles[ae_ut_scan.projectile_count++] = iterator.index;
	object_iterator_new(&iterator, _object_mask_vehicle, 0);
	while (object_iterator_next(&iterator) && ae_ut_scan.vehicle_count < AE_UT_SCAN_MAXIMUM)
		ae_ut_scan.vehicles[ae_ut_scan.vehicle_count++] = iterator.index;
	object_iterator_new(&iterator, _object_mask_biped, 0);
	while (object_iterator_next(&iterator) && ae_ut_scan.biped_count < AE_UT_SCAN_MAXIMUM)
		ae_ut_scan.bipeds[ae_ut_scan.biped_count++] = iterator.index;
}

static boolean ae_bots_ut_on(void)
{
	if (ae_ut.on < 0)
	{
		char const *value = getenv("HALO_BOTS_UT");

		ae_ut.on = !(value && value[0] == '0');
		platform_log("bots: ut spike %s (HALO_BOTS_UT)", ae_ut.on ? "on" : "off");
	}
	return ae_ut.on != 0;
}

static void ae_bots_ut_report(long now)
{
	if (now - ae_ut.last_report_tick < 60 * TICKS_PER_SECOND && now >= ae_ut.last_report_tick)
		return;
	if (ae_ut.last_report_tick > 0 && now > ae_ut.last_report_tick)
	{
		platform_log("bots: ut tick %ld: shots dodged %ld (jumped %ld), stepped aside for teammates' vehicles %ld, enemies' "
			"%ld, charges %ld (%.1f s), warthog laps %ld (%.1f s, turned about %ld), gunner trigger held %.1f s, flipped vehicles gone back to %ld, flipped back %ld, teammates spared %ld, walked round still vehicles %ld, waits for riders %ld, hailed by a teammate's round %ld, seats given up %ld, sliding %.1f s", now, ae_ut.shots, ae_ut.jumps,
			ae_ut.vehicles_team, ae_ut.vehicles_enemy, ae_ut.charges, (real)ae_ut.charge_ticks / TICKS_PER_SECOND, ae_ut.laps,
			(real)ae_ut.lap_ticks / TICKS_PER_SECOND, ae_ut.lap_turns, (real)ae_ut.gunner_ticks / TICKS_PER_SECOND, ae_ut.flips_wanted, ae_ut.flips_done, ae_ut.spared, ae_ut.rounded, ae_ut.rider_waits, ae_ut.hails, ae_ut.seats_given,
			(real)ae_ut.slide_ticks / TICKS_PER_SECOND);
	}
	ae_ut.last_report_tick = now;
}

/* the level's least warning (seconds) and its chance to act on a threat it could */
static real ae_bots_ut_reaction(int level)
{
	static real const seconds[] = { 99.0f, 0.60f, 0.45f, 0.35f, 0.28f, 0.22f };

	return seconds[PIN(level, 0, 5)];
}

static real ae_bots_ut_chance(int level)
{
	static real const chance[] = { 0.0f, 0.25f, 0.45f, 0.65f, 0.80f, 0.90f };

	return chance[PIN(level, 0, 5)];
}

/* where a mover passes the bot: the ticks until its nearest and how near; FALSE when it goes away */
static boolean ae_bots_ut_pass(real_point3d const *from, real_vector3d const *velocity, real_point3d const *bot,
	real *ticks, real *miss)
{
	real dx = bot->x - from->x, dy = bot->y - from->y, dz = bot->z - from->z;
	real speed2 = velocity->i * velocity->i + velocity->j * velocity->j + velocity->k * velocity->k;
	real t, mx, my, mz;

	if (speed2 < 0.0001f)
		return FALSE;
	t = (dx * velocity->i + dy * velocity->j + dz * velocity->k) / speed2;
	if (t <= 0.0f)
		return FALSE;
	mx = dx - velocity->i * t;
	my = dy - velocity->j * t;
	mz = dz - velocity->k * t;
	*ticks = t;
	*miss = (real)sqrt(mx * mx + my * my + mz * mz);
	return TRUE;
}

/* a shot the bot dodges: an enemy's slow projectile, not a grenade */
static boolean ae_bots_ut_shot(struct object_datum const *object, struct bots_tick *tick, long bot_player)
{
	real_vector3d const *v = &object->object.translational_velocity;
	real speed = (real)sqrt(v->i * v->i + v->j * v->j + v->k * v->k);

	if (object->object.owner_player_index == NONE || object->object.owner_player_index == bot_player)
		return FALSE;
	if (!game_team_is_enemy(tick->unit->object.owner_team_index, object->object.owner_team_index))
		return FALSE;
	if (speed > AE_UT_SHOT_SPEED_MAX || speed < 0.01f)
		return FALSE;
	return !ae_falls_grenade_definition(object->definition_index);
}

/* a step across the threat's way, to the side the bot already stands on */
static void ae_bots_ut_step(struct ae_ut_bot *ut, struct bots_tick *tick, real_point3d const *from,
	real_vector3d const *velocity, real ticks, boolean jump)
{
	real along = (real)atan2(velocity->j, velocity->i);
	real side = (velocity->i * (tick->position.y - from->y) - velocity->j * (tick->position.x - from->x)) >= 0.0f ? 1.0f : -1.0f;

	ut->way = along + side * BOTS_PI * 0.5f;
	ut->until_tick = tick->now + (long)ticks + TICKS_PER_SECOND / 5;
	ut->jump = jump;
}

/* (ae_bots_falls_guard, a bot on foot) DODGE and CLEAR THE WAY */
static void ae_bots_ut_dodge(struct bots_bot *bot, struct bots_tick *tick, int level)
{
	long bot_index = (long)(bot - bots_globals.bots);
	struct ae_ut_bot *ut;
	long scan;
	real best_ticks = REAL_MAX;
	long best = NONE;
	boolean best_vehicle = FALSE, best_team = FALSE;
	real_point3d best_from;
	real_vector3d best_velocity;

	if (!ae_bots_ut_on() || bot_index < 0 || bot_index >= BOTS_BRAINS || !tick->unit)
		return;
	ae_bots_ut_report(tick->now);
	ut = &ae_ut.bots[bot_index];
	/* (a new game: game time starts over) */
	if (ut->until_tick - tick->now > 3 * TICKS_PER_SECOND)
		ut->until_tick = 0;
	/* (once a tick the threats near it: shots, then vehicles) */
	ae_bots_ut_scan(game_time_get());
	for (scan = 0; scan < ae_ut_scan.projectile_count; scan++)
	{
		struct object_datum *object = (struct object_datum *)object_try_and_get_and_verify_type(ae_ut_scan.projectiles[scan], NONE);
		if (!object)
			continue;
		real ticks, miss;

		if (!ae_bots_ut_shot(object, tick, bot->player_index) ||
			!ae_bots_ut_pass(&object->object.position, &object->object.translational_velocity, &tick->position, &ticks, &miss))
		{
			continue;
		}
		if (miss < AE_UT_DANGER_SHOT && ticks < AE_UT_LOOK_SECONDS * TICKS_PER_SECOND && ticks < best_ticks)
		{
			best_ticks = ticks;
			best = ae_ut_scan.projectiles[scan];
			best_vehicle = FALSE;
			best_from = object->object.position;
			best_velocity = object->object.translational_velocity;
		}
	}
	ae_bots_ut_scan(game_time_get());
	for (scan = 0; scan < ae_ut_scan.vehicle_count; scan++)
	{
		struct object_datum *object = (struct object_datum *)object_try_and_get_and_verify_type(ae_ut_scan.vehicles[scan], NONE);
		if (!object)
			continue;
		struct unit_datum *vehicle = (struct unit_datum *)object;
		struct object_datum *driver;
		real_vector3d const *v = &object->object.translational_velocity;
		real ticks, miss;

		if (ae_ut_scan.vehicles[scan] == tick->object->object.parent_object_index || vehicle->unit.driver_object_index == NONE ||
			(real)sqrt(v->i * v->i + v->j * v->j) < AE_UT_VEHICLE_SPEED_MIN)
		{
			continue;
		}
		if (!ae_bots_ut_pass(&object->object.position, v, &tick->position, &ticks, &miss) ||
			miss > AE_UT_DANGER_VEHICLE || ticks > AE_UT_LOOK_SECONDS * TICKS_PER_SECOND || ticks >= best_ticks)
		{
			continue;
		}
		driver = (struct object_datum *)object_try_and_get_and_verify_type(vehicle->unit.driver_object_index, _object_mask_unit);
		best_ticks = ticks;
		best = ae_ut_scan.vehicles[scan];
		best_vehicle = TRUE;
		best_team = driver && !game_team_is_enemy(tick->unit->object.owner_team_index, driver->object.owner_team_index);
		best_from = object->object.position;
		best_velocity = *v;
	}
	if (best != NONE && tick->now >= ut->until_tick)
	{
		/* (judged once a threat: seen in time for its level, and acted on as often as its level does; a teammate's
		vehicle always, from EASY up) */
		if (ut->judged_object != best)
		{
			real seconds = best_ticks / TICKS_PER_SECOND;

			ut->judged_object = best;
			ut->judged_dodge = level > AE_BOTS_LEVEL_RECRUIT && seconds >= ae_bots_ut_reaction(level) &&
				((best_vehicle && best_team) || bots_random_real(bot) < ae_bots_ut_chance(level));
			if (ut->judged_dodge)
			{
				boolean jump = !best_vehicle && level >= AE_BOTS_LEVEL_HEROIC && seconds < 0.6f;

				ae_bots_ut_step(ut, tick, &best_from, &best_velocity, best_ticks, jump);
				if (best_vehicle)
				{
					if (best_team)
						ae_ut.vehicles_team++;
					else
						ae_ut.vehicles_enemy++;
				}
				else
				{
					ae_ut.shots++;
					ae_ut.jumps += jump ? 1 : 0;
				}
				if (ae_bots_events_on())
				{
					platform_log("bots: ev=ut_dodge bot=%ld kind=%s in=%.2f at=(%.1f %.1f %.1f)%s", bot_index + 1,
						best_vehicle ? (best_team ? "team_vehicle" : "enemy_vehicle") : "shot", seconds, tick->position.x,
						tick->position.y, tick->position.z, jump ? " jump" : "");
				}
			}
		}
	}
	/* (AROUND PARKED VEHICLES: a still vehicle in its way: along its side, away from its middle) */
	if (tick->moving && tick->now >= ut->until_tick)
	{
		real way_x = (real)cos(tick->move_direction), way_y = (real)sin(tick->move_direction);

		ae_bots_ut_scan(game_time_get());
		for (scan = 0; scan < ae_ut_scan.vehicle_count; scan++)
		{
			struct object_datum *object = (struct object_datum *)object_try_and_get_and_verify_type(ae_ut_scan.vehicles[scan], NONE);
			if (!object)
				continue;
			real_vector3d const *v = &object->object.translational_velocity;
			real dx = object->object.position.x - tick->position.x, dy = object->object.position.y - tick->position.y;
			real ahead = dx * way_x + dy * way_y, across = dx * way_y - dy * way_x;
			real radius = object->object.bounding_sphere_radius * 0.8f + 0.4f;

			/* (never round the vehicle it walks to board or flip: the Ghosts went unboarded, Sidewinder game 5) */
			if (ae_ut_scan.vehicles[scan] == tick->object->object.parent_object_index ||
				ae_ut_scan.vehicles[scan] == ae_bots_vehicle_bots[bot_index].target_vehicle_index || ae_ut_scan.vehicles[scan] == ut->flip_vehicle ||
				(real)sqrt(v->i * v->i + v->j * v->j) >= AE_UT_VEHICLE_SPEED_MIN ||
				fabs(object->object.position.z - tick->position.z) > 2.0f || ahead <= 0.0f || ahead > radius + 1.5f ||
				fabs(across) > radius)
			{
				continue;
			}
			/* (across > 0: the middle on its right: turn left; else right) */
			tick->move_direction += across > 0.0f ? BOTS_PI * 0.4f : -BOTS_PI * 0.4f;
			if (tick->now % TICKS_PER_SECOND == 0)
				ae_ut.rounded++;
			break;
		}
	}
	if (tick->now < ut->until_tick)
	{
		tick->move_direction = ut->way;
		tick->moving = TRUE;
		tick->throttle_scale = 1.0f;
		if (ut->jump)
		{
			tick->flags |= FLAG(_unit_control_jump_bit);
			ut->jump = FALSE;
		}
	}
}

/* (ae_bots_vehicle_ride, a gunner) WARTHOG GUNNER: the chain gun's trigger held while the target is in sight and the
aim near it */
static void ae_bots_ut_gunner(struct bots_bot *bot, struct bots_tick *tick, struct ae_bots_vehicle_bot *state)
{
	real dx, dy, dz, horizontal, distance, cone, yaw_off, pitch_off;
	real_point3d const *eye;

	if (!ae_bots_ut_on() || state->vehicle_class != AE_BOTS_VEHICLE_WARTHOG || !tick->fighting ||
		bot->target_player_index == NONE || tick->now - bot->target_seen_tick > 3 ||
		tick->now - bot->target_first_seen_tick < TICKS_PER_SECOND / 4)
	{
		return;
	}
	eye = bots_tick_eye(tick);
	dx = bot->target_last_position.x - eye->x;
	dy = bot->target_last_position.y - eye->y;
	dz = bot->target_last_position.z - eye->z;
	horizontal = (real)sqrt(dx * dx + dy * dy);
	distance = MAX(horizontal, 0.5f);
	cone = (real)atan(0.6f / distance) + 3.0f * BOTS_DEGREES;
	yaw_off = (real)fabs(bots_angle_difference((real)atan2(dy, dx), bot->yaw));
	pitch_off = (real)fabs((real)atan2(dz, horizontal) - bot->pitch);
	if (yaw_off < cone && pitch_off < cone * 1.5f)
	{
		tick->flags |= FLAG(_unit_control_weapon_primary_trigger_bit);
		ae_ut.gunner_ticks++;
	}
}

/* WARTHOG LAPS: the waypoint on a circle round the enemy, ahead in the lap's way */
static boolean ae_bots_ut_lap(struct bots_bot *bot, struct bots_tick *tick, struct ae_bots_vehicle_bot *state,
	struct object_datum *vehicle, real_point3d const *enemy, real_point3d *waypoint)
{
	long bot_index = (long)(bot - bots_globals.bots);
	struct ae_ut_bot *ut = &ae_ut.bots[bot_index];
	real dx = vehicle->object.position.x - enemy->x, dy = vehicle->object.position.y - enemy->y;
	real distance = (real)sqrt(dx * dx + dy * dy);
	real around, radius;

	if (distance > AE_UT_LAP_REACH || fabs(enemy->z - vehicle->object.position.z) > 6.0f)
		return FALSE;
	if (ut->lap_sign == 0.0f || ut->lap_turn_tick - tick->now > 30 * TICKS_PER_SECOND)
	{
		ut->lap_sign = (bot->player_index & 1) ? 1.0f : -1.0f;
		ut->lap_turn_tick = tick->now + 8 * TICKS_PER_SECOND;
	}
	/* (evasive: the lap turned about every 6 to 12 s, and at once when the enemy closes inside half the range) */
	if (tick->now >= ut->lap_turn_tick || (distance < AE_UT_LAP_RADIUS * 0.5f && tick->now + 4 * TICKS_PER_SECOND >= ut->lap_turn_tick))
	{
		ut->lap_sign = -ut->lap_sign;
		ut->lap_turn_tick = tick->now + 6 * TICKS_PER_SECOND + (long)(bots_random_real(bot) * 6 * TICKS_PER_SECOND);
		ae_ut.lap_turns++;
	}
	/* (far: in towards the circle; near: out to it; the point ahead round it keeps the turns wide) */
	radius = AE_UT_LAP_RADIUS;
	around = (real)atan2(dy, dx) + ut->lap_sign * AE_UT_LAP_LEAD;
	if (distance > radius * 1.6f)
		radius = distance * 0.75f;
	waypoint->x = enemy->x + (real)cos(around) * radius;
	waypoint->y = enemy->y + (real)sin(around) * radius;
	waypoint->z = vehicle->object.position.z;
	if (state->charge_tick != tick->now - 1)
	{
		ae_ut.laps++;
		if (ae_bots_events_on())
			platform_log("bots: ev=ut_lap bot=%ld at=(%.1f %.1f %.1f) enemy %.1f off, %s", bot_index + 1,
				vehicle->object.position.x, vehicle->object.position.y, vehicle->object.position.z, distance,
				ut->lap_sign > 0.0f ? "anticlockwise" : "clockwise");
	}
	state->charge_tick = tick->now;
	ae_ut.lap_ticks++;
	return TRUE;
}

/* (ae_bots_vehicle_drive_tick) RUN-OVER CHARGE: the waypoint past an enemy on foot ahead; TRUE when it charges */
static boolean ae_bots_ut_charge(struct bots_bot *bot, struct bots_tick *tick, struct ae_bots_vehicle_bot *state,
	struct object_datum *vehicle, real heading, real_point3d *waypoint)
{
	struct player_datum *target;
	struct object_datum *target_object;
	real dx, dy, distance, bearing;

	if (!ae_bots_ut_on() || !tick->fighting || bot->target_player_index == NONE ||
		state->vehicle_class == AE_BOTS_VEHICLE_TURRET || state->vehicle_class == AE_BOTS_VEHICLE_BANSHEE)
	{
		return FALSE;
	}
	target = player_get(bot->target_player_index);
	if (!target || target->unit_index == NONE)
		return FALSE;
	target_object = (struct object_datum *)object_try_and_get_and_verify_type(target->unit_index, _object_mask_unit);
	if (!target_object)
		return FALSE;
	/* (a warthog laps for its gunner, never rams) */
	if (state->vehicle_class == AE_BOTS_VEHICLE_WARTHOG || state->vehicle_class == AE_BOTS_VEHICLE_ROCKET_WARTHOG)
	{
		real_point3d enemy = target_object->object.position;

		ae_bots_vehicle_world_point(target->unit_index, &enemy);
		return ae_bots_ut_lap(bot, tick, state, vehicle, &enemy, waypoint);
	}
	/* (on foot only: a vehicle is not run over) */
	if (target_object->object.parent_object_index != NONE)
		return FALSE;
	dx = target_object->object.position.x - vehicle->object.position.x;
	dy = target_object->object.position.y - vehicle->object.position.y;
	distance = (real)sqrt(dx * dx + dy * dy);
	if (distance > AE_UT_CHARGE_RANGE || distance < 0.5f ||
		fabs(target_object->object.position.z - vehicle->object.position.z) > 3.0f)
	{
		return FALSE;
	}
	bearing = bots_angle_difference((real)atan2(dy, dx), heading);
	if (fabs(bearing) > AE_UT_CHARGE_CONE)
		return FALSE;
	waypoint->x = target_object->object.position.x + dx / distance * AE_UT_CHARGE_PAST;
	waypoint->y = target_object->object.position.y + dy / distance * AE_UT_CHARGE_PAST;
	waypoint->z = target_object->object.position.z;
	if (state->charge_tick != tick->now - 1)
	{
		ae_ut.charges++;
		if (ae_bots_events_on())
			platform_log("bots: ev=ut_charge bot=%ld at=(%.1f %.1f %.1f) target %.1f off", (long)(bot - bots_globals.bots) + 1,
				vehicle->object.position.x, vehicle->object.position.y, vehicle->object.position.z, distance);
	}
	state->charge_tick = tick->now;
	ae_ut.charge_ticks++;
	return TRUE;
}

/* FLIP BACK: (ae_bots_vehicle_move, out of a vehicle) remember an upturned one */
static void ae_bots_ut_flipped(struct bots_bot *bot, long vehicle_index, long now)
{
	long bot_index = (long)(bot - bots_globals.bots);

	if (!ae_bots_ut_on() || bot_index < 0 || bot_index >= BOTS_BRAINS)
		return;
	ae_ut.bots[bot_index].flip_vehicle = vehicle_index;
	ae_ut.bots[bot_index].flip_until_tick = now + 30 * TICKS_PER_SECOND;
	ae_ut.flips_wanted++;
}

/* the flipped vehicle it goes back to now (NONE: none): still upturned, alive, near, and no enemy seen a while */
static long ae_bots_ut_flip_target(struct bots_bot *bot, long now, real_point3d const *position)
{
	long bot_index = (long)(bot - bots_globals.bots);
	struct ae_ut_bot *ut;
	struct object_datum *vehicle;

	if (!ae_bots_ut_on() || bot_index < 0 || bot_index >= BOTS_BRAINS)
		return NONE;
	ut = &ae_ut.bots[bot_index];
	if (ut->flip_vehicle == NONE || ut->flip_until_tick - now > 60 * TICKS_PER_SECOND)
	{
		ut->flip_vehicle = NONE;
		return NONE;
	}
	vehicle = ae_bots_vehicle_get(ut->flip_vehicle);
	if (now > ut->flip_until_tick || !vehicle || !ae_bots_vehicle_alive(vehicle) ||
		bots_distance3d(&vehicle->object.position, position) > 30.0f)
	{
		ut->flip_vehicle = NONE;
		return NONE;
	}
	/* (righted: nothing to flip; kept for the boarding (ae_bots_ut_flip_move)) */
	if (ae_bots_vehicle_upright(vehicle))
		return NONE;
	if (bot->target_player_index != NONE && now - bot->target_seen_tick < TICKS_PER_SECOND * 3 / 2)
		return NONE;
	return ut->flip_vehicle;
}

/* (ae_bots_vehicle_goal) the ground beside it as the goal; TRUE when going back */
static boolean ae_bots_ut_flip_goal(struct bots_bot *bot, real_point3d const *position)
{
	long vehicle_index = ae_bots_ut_flip_target(bot, game_time_get(), position);
	struct object_datum *vehicle;
	real_point3d ground;
	long surface_index;

	if (vehicle_index == NONE || !(vehicle = ae_bots_vehicle_get(vehicle_index)))
		return FALSE;
	surface_index = bots_surface_below(&vehicle->object.position, &ground);
	if (surface_index == NONE)
		ground = vehicle->object.position;
	bots_set_goal(bot, _bots_goal_roam, &ground, surface_index);
	return TRUE;
}

/* (ae_bots_vehicle_move, on foot) the last steps and the flip; TRUE while it handles the tick. Righted: the seat look
right away, the vehicle no longer one it just left */
static boolean ae_bots_ut_flip_move(struct bots_bot *bot, long bot_index, struct bots_tick *tick,
	struct ae_bots_vehicle_bot *state)
{
	struct ae_ut_bot *ut = &ae_ut.bots[bot_index];
	long vehicle_index;
	struct object_datum *vehicle;
	real distance;

	/* (righted while it was out, by it or anyone, near: back in at once, a seat it can take) */
	if (ae_bots_ut_on() && ut->flip_vehicle != NONE && tick->now <= ut->flip_until_tick &&
		ut->flip_until_tick - tick->now <= 60 * TICKS_PER_SECOND && state->target_vehicle_index == NONE &&
		(vehicle = ae_bots_vehicle_get(ut->flip_vehicle)) != NULL && ae_bots_vehicle_alive(vehicle) &&
		ae_bots_vehicle_upright(vehicle) && bots_distance3d(&vehicle->object.position, &tick->position) < 15.0f)
	{
		short seat_count = ae_bots_vehicle_seat_count(ut->flip_vehicle), seat_index, best_index = NONE, best_role = AE_BOTS_SEAT_NONE;
		boolean driven = ((struct unit_datum *)vehicle)->unit.driver_object_index != NONE;

		for (seat_index = 0; seat_index < seat_count; seat_index++)
		{
			long occupant = NONE;
			short role = (short)ae_bots_vehicle_seat_role(ut->flip_vehicle, seat_index);

			if (role == AE_BOTS_SEAT_NONE || !unit_can_enter_seat(tick->player->unit_index, ut->flip_vehicle, seat_index, &occupant))
				continue;
			/* (the gun when a teammate drives, the wheel when no one does, else any seat) */
			if (best_index == NONE || (driven && role == AE_BOTS_SEAT_GUNNER) || (!driven && role == AE_BOTS_SEAT_DRIVER))
			{
				best_index = seat_index;
				best_role = role;
			}
		}
		if (best_index != NONE)
		{
			state->target_vehicle_index = ut->flip_vehicle;
			state->target_seat_index = best_index;
			state->target_seat = best_role;
			state->target_tick = tick->now;
			state->near_tick = NONE;
			state->left_until_tick = tick->now;
			bot->path.valid = FALSE;
			bot->goal_kind = _bots_goal_none;
			ae_ut.flips_done++;
			if (ae_bots_events_on())
				platform_log("bots: ev=ut_reboard bot=%ld seat %s", bot_index + 1, ae_bots_vehicle_seat_name(best_role));
		}
		ut->flip_vehicle = NONE;
		ut->flip_press_tick = 0;
		return FALSE;
	}
	vehicle_index = ae_bots_ut_flip_target(bot, tick->now, &tick->position);

	if (vehicle_index == NONE)
	{
		return FALSE;
	}
	vehicle = ae_bots_vehicle_get(vehicle_index);
	distance = bots_distance2d(&vehicle->object.position, &tick->position);
	if (distance > 3.0f)
		return FALSE;
	if (tick->player->action_result == _player_action_result_flip_vehicle && tick->player->action_object_index == vehicle_index)
	{
		tick->moving = FALSE;
		if (ut->flip_press_tick != tick->now - 1)
		{
			tick->flags |= FLAG(_unit_control_action_bit);
			ut->flip_press_tick = tick->now;
			if (ae_bots_events_on())
				platform_log("bots: ev=ut_flip bot=%ld vehicle %ld", bot_index + 1, vehicle_index);
		}
		return TRUE;
	}
	tick->move_direction = (real)atan2(vehicle->object.position.y - tick->position.y, vehicle->object.position.x - tick->position.x);
	tick->moving = TRUE;
	tick->throttle_scale = 0.6f;
	return TRUE;
}

/* (ae_bots_vehicle_drive_tick) SPARE TEAMMATES: the waypoint swung aside from a teammate on foot in the vehicle's way;
TRUE when it eases off */
static boolean ae_bots_ut_spare(struct bots_bot *bot, struct bots_tick *tick, struct object_datum *vehicle,
	real_point3d *waypoint)
{
	long scan;
	real_vector3d const *v = &vehicle->object.translational_velocity;
	real best_ticks = REAL_MAX, side = 0.0f;
	real_point3d const *best = NULL;

	if (!ae_bots_ut_on() || (real)sqrt(v->i * v->i + v->j * v->j) < AE_UT_VEHICLE_SPEED_MIN)
		return FALSE;
	ae_bots_ut_scan(game_time_get());
	for (scan = 0; scan < ae_ut_scan.biped_count; scan++)
	{
		struct object_datum *object = (struct object_datum *)object_try_and_get_and_verify_type(ae_ut_scan.bipeds[scan], NONE);
		if (!object)
			continue;
		real ticks, miss;

		if (ae_ut_scan.bipeds[scan] == bot->unit_index || object->object.parent_object_index != NONE ||
			game_team_is_enemy(tick->unit->object.owner_team_index, object->object.owner_team_index))
		{
			continue;
		}
		if (!ae_bots_ut_pass(&vehicle->object.position, v, &object->object.position, &ticks, &miss) ||
			miss > AE_UT_DANGER_VEHICLE || ticks > AE_UT_LOOK_SECONDS * TICKS_PER_SECOND || ticks >= best_ticks)
		{
			continue;
		}
		best_ticks = ticks;
		best = &object->object.position;
		/* (the side of the vehicle's way the teammate stands on: swing to the other) */
		side = (v->i * (object->object.position.y - vehicle->object.position.y) -
			v->j * (object->object.position.x - vehicle->object.position.x)) >= 0.0f ? -1.0f : 1.0f;
	}
	if (!best)
		return FALSE;
	{
		real along = (real)atan2(v->j, v->i), away = along + side * BOTS_PI * 0.5f;

		waypoint->x = best->x + (real)cos(along) * 3.0f + (real)cos(away) * 5.0f;
		waypoint->y = best->y + (real)sin(along) * 3.0f + (real)sin(away) * 5.0f;
	}
	if (tick->now % TICKS_PER_SECOND == 0)
		ae_ut.spared++;
	/* (the horn spike: a honk at the teammate in the way, at most one each 3 s) */
	if (tick->now % (3 * TICKS_PER_SECOND) == 0)
		ae_horn_honk(tick->unit->object.parent_object_index);
	return TRUE;
}

/* (ae_bots_vehicle_drive_tick) WAIT FOR RIDERS: TRUE while the driver stops for a teammate coming to a seat */
static boolean ae_bots_ut_wait_riders(struct bots_bot *bot, struct bots_tick *tick, struct ae_bots_vehicle_bot *state,
	struct object_datum *vehicle)
{
	long bot_index = (long)(bot - bots_globals.bots);
	struct ae_ut_bot *ut;
	struct unit_datum *vehicle_unit = (struct unit_datum *)vehicle;
	long scan;
	boolean room, coming = FALSE;

	if (!ae_bots_ut_on() || bot_index < 0 || bot_index >= BOTS_BRAINS)
		return FALSE;
	ut = &ae_ut.bots[bot_index];
	if (ut->wait_again_tick - tick->now > 60 * TICKS_PER_SECOND)
		ut->wait_again_tick = 0;
	if (ut->hailed_until_tick - tick->now > 60 * TICKS_PER_SECOND)
		ut->hailed_until_tick = 0;
	/* (HAILED: a teammate's round at the vehicle) */
	ae_bots_ut_scan(game_time_get());
	for (scan = 0; scan < ae_ut_scan.projectile_count; scan++)
	{
		struct object_datum *object = (struct object_datum *)object_try_and_get_and_verify_type(ae_ut_scan.projectiles[scan], NONE);
		if (!object)
			continue;
		real ticks, miss;

		if (object->object.owner_player_index == NONE || object->object.owner_player_index == bot->player_index ||
			game_team_is_enemy(tick->unit->object.owner_team_index, object->object.owner_team_index) ||
			!ae_bots_ut_pass(&object->object.position, &object->object.translational_velocity, &vehicle->object.position,
				&ticks, &miss) || miss > 2.5f || ticks > 4.0f)
		{
			continue;
		}
		if (tick->now >= ut->hailed_until_tick || ut->hailer != object->object.owner_player_index)
		{
			ae_ut.hails++;
			if (ae_bots_events_on())
				platform_log("bots: ev=ut_hailed bot=%ld by player %ld", bot_index + 1, object->object.owner_player_index & 0xFFFF);
		}
		ut->hailer = object->object.owner_player_index;
		ut->hailed_until_tick = tick->now + 8 * TICKS_PER_SECOND;
		break;
	}
	if (tick->now < ut->hailed_until_tick)
	{
		struct player_datum *hailer = player_try_and_get(ut->hailer);
		struct object_datum *hailer_unit = hailer && hailer->unit_index != NONE ?
			(struct object_datum *)object_try_and_get_and_verify_type(hailer->unit_index, _object_mask_unit) : NULL;

		if (hailer_unit && hailer_unit->object.parent_object_index == NONE &&
			bots_distance3d(&hailer_unit->object.position, &vehicle->object.position) < 30.0f)
		{
			return TRUE;
		}
		ut->hailed_until_tick = 0;
	}
	/* (room: fewer riders than seats (review: a full Warthog stopped for riders who could not get in)) */
	{
		long riders = 0;

		ae_bots_ut_scan(tick->now);
		for (scan = 0; scan < ae_ut_scan.biped_count; scan++)
		{
			struct object_datum *rider = (struct object_datum *)object_try_and_get_and_verify_type(ae_ut_scan.bipeds[scan], NONE);

			riders += rider && rider->object.parent_object_index == state->vehicle_index;
		}
		room = riders < ae_bots_vehicle_seat_count(state->vehicle_index);
	}
	(void)vehicle_unit;
	if (!room || state->vehicle_class == AE_BOTS_VEHICLE_GHOST || tick->now < ut->wait_again_tick ||
		(bot->target_player_index != NONE && tick->now - bot->target_seen_tick < TICKS_PER_SECOND))
	{
		ut->wait_since_tick = 0;
		return FALSE;
	}
	ae_bots_ut_scan(game_time_get());
	for (scan = 0; scan < ae_ut_scan.biped_count && !coming; scan++)
	{
		struct object_datum *object = (struct object_datum *)object_try_and_get_and_verify_type(ae_ut_scan.bipeds[scan], NONE);
		if (!object)
			continue;
		real dx = vehicle->object.position.x - object->object.position.x;
		real dy = vehicle->object.position.y - object->object.position.y;
		real distance = (real)sqrt(dx * dx + dy * dy);
		real toward;

		if (ae_ut_scan.bipeds[scan] == bot->unit_index || object->object.parent_object_index != NONE ||
			game_team_is_enemy(tick->unit->object.owner_team_index, object->object.owner_team_index) ||
			fabs(object->object.position.z - vehicle->object.position.z) > 3.0f || distance > 8.0f)
		{
			continue;
		}
		toward = distance > 0.01f ? (object->object.translational_velocity.i * dx + object->object.translational_velocity.j * dy) /
			distance : 0.0f;
		coming = distance < 3.0f || toward > 0.04f;
	}
	if (!coming)
	{
		ut->wait_since_tick = 0;
		return FALSE;
	}
	if (ut->wait_since_tick == 0)
	{
		ut->wait_since_tick = tick->now;
		ae_ut.rider_waits++;
	}
	if (tick->now - ut->wait_since_tick > 5 * TICKS_PER_SECOND)
	{
		ut->wait_since_tick = 0;
		ut->wait_again_tick = tick->now + 6 * TICKS_PER_SECOND;
		return FALSE;
	}
	return TRUE;
}

/* (ae_bots_vehicle_ride) GIVE UP THE SEAT: TRUE when a teammate has held the action button at its seat half a second */
static boolean ae_bots_ut_give_seat(struct bots_bot *bot, struct bots_tick *tick, long vehicle_index, short seat_index)
{
	long bot_index = (long)(bot - bots_globals.bots);
	struct ae_ut_bot *ut;
	struct data_iterator iterator;
	struct player_datum *other;
	boolean held = FALSE;

	if (!ae_bots_ut_on() || bot_index < 0 || bot_index >= BOTS_BRAINS)
		return FALSE;
	ut = &ae_ut.bots[bot_index];
	/* (the game offers EVICT only from a campaign actor's seat (unit_find_nearby_seat: the occupant's actor_index), never
	from a bot's: so a person on foot by the vehicle, holding the action button, the bot sitting nearest them) */
	data_iterator_new(&iterator, player_data);
	while ((other = (struct player_datum *)data_iterator_next(&iterator)) != NULL && !held)
	{
		struct unit_datum *unit;
		struct data_iterator riders;
		struct player_datum *rider;
		real mine;
		boolean nearest = TRUE;

		if (other == tick->player || other->unit_index == NONE ||
			(other->local_player_index == NONE && bots_machine_is_bot(other->network_player_data.machine_index)))
		{
			continue;
		}
		unit = (struct unit_datum *)object_try_and_get_and_verify_type(other->unit_index, _object_mask_unit);
		if (!unit || unit->object.parent_object_index != NONE || !TEST_FLAG(unit->unit.control_flags, _unit_control_action_bit) ||
			game_team_is_enemy(tick->unit->object.owner_team_index, unit->object.owner_team_index))
		{
			continue;
		}
		/* (a rider's world place: ae_bots_vehicle_point; a seated unit's own centre is not one) */
		{
			real_point3d bot_point = ae_bots_vehicle_point(bot->unit_index);

			mine = bots_distance3d(&unit->object.bounding_sphere_center, &bot_point);
		}
		if (ae_bots_events_on() && mine <= 8.0f && tick->now % 15 == 0)
			platform_log("bots: ev=seat_hold bot=%ld seat %d distance %.1f", bot_index + 1, (int)seat_index, mine);
		if (mine > 3.0f)
			continue;
		/* (the rider nearest the person gets out: by the gun, the gunner; by the driver's door, the driver) */
		data_iterator_new(&riders, player_data);
		while ((rider = (struct player_datum *)data_iterator_next(&riders)) != NULL && nearest)
		{
			struct object_datum *seated;

			if (rider == tick->player || rider == other || rider->unit_index == NONE)
				continue;
			seated = object_try_and_get_and_verify_type(rider->unit_index, _object_mask_unit);
			if (seated && seated->object.parent_object_index == vehicle_index)
			{
				real_point3d rider_point = ae_bots_vehicle_point(rider->unit_index);

				if (bots_distance3d(&unit->object.bounding_sphere_center, &rider_point) < mine)
					nearest = FALSE;
			}
		}
		held = nearest;
	}
	/* (the action bit is set only for moments of a hold (owner's TEST RUN 10-26: one tick seen): a press is
	remembered 20 ticks, so a press or a hold at the seat gives it up after half a second) */
	if (held)
		ut->evict_seen_tick = tick->now;
	held = ut->evict_seen_tick > 0 && tick->now - ut->evict_seen_tick <= 20;
	ut->evict_ticks = held ? ut->evict_ticks + 1 : 0;
	if (ut->evict_ticks < TICKS_PER_SECOND / 2)
		return FALSE;
	ut->evict_ticks = 0;
	ut->evict_seen_tick = 0;
	ae_ut.seats_given++;
	if (ae_bots_events_on())
		platform_log("bots: ev=ut_give_seat bot=%ld seat %d", bot_index + 1, (int)seat_index);
	return TRUE;
}

/* (ae_bots_vehicle_drive_tick) TRACTION: the throttle and the steering eased in a slide */
static void ae_bots_ut_traction(struct object_datum *vehicle, real heading, real *throttle, real *steer)
{
	real_vector3d const *v = &vehicle->object.translational_velocity;
	real speed = (real)sqrt(v->i * v->i + v->j * v->j);
	real sideways, slip;

	if (!ae_bots_ut_on() || speed < 0.1f)
		return;
	sideways = (real)fabs(-v->i * (real)sin(heading) + v->j * (real)cos(heading));
	slip = sideways / speed;
	if (slip < 0.35f)
		return;
	ae_ut.slide_ticks++;
	*throttle = PIN(*throttle, -0.55f * (1.0f - (slip - 0.35f)), 0.55f * (1.0f - (slip - 0.35f)));
	*steer *= 0.5f;
}

/* (ae_bots_vehicle_drive_tick) WALL MARGIN: the waypoint pushed off walls beside and ahead of the vehicle */
static void ae_bots_ut_wall_margin(struct bots_bot *bot, struct object_datum *vehicle, long vehicle_index, real heading,
	real_point3d *waypoint)
{
	long bot_index = (long)(bot - bots_globals.bots);
	struct ae_ut_bot *ut = bot_index >= 0 && bot_index < BOTS_BRAINS ? &ae_ut.bots[bot_index] : NULL;
	long now = game_time_get();
	static real const angles[6] = { 35.0f, -35.0f, 70.0f, -70.0f, 100.0f, -100.0f };
	unsigned long flags = FLAG(_collision_test_structure_bit) | FLAG(_collision_test_front_facing_surfaces_bit) |
		FLAG(_collision_test_objects_bit) | FLAG(_collision_test_objects_scenery_bit);
	struct collision_result collision;
	real_point3d start = vehicle->object.position;
	real push_x = 0.0f, push_y = 0.0f, push;
	long index;

	if (!ae_bots_ut_on() || !ut)
		return;
	/* (the rays every third tick, the push kept between (review: rays per driver per tick)) */
	if (now - ut->wall_tick < 3 && now >= ut->wall_tick)
	{
		waypoint->x += ut->wall_push_x;
		waypoint->y += ut->wall_push_y;
		return;
	}
	ut->wall_tick = now;
	ut->wall_push_x = ut->wall_push_y = 0.0f;
	start.z += 0.6f;
	for (index = 0; index < 6; index++)
	{
		real angle = heading + angles[index] * BOTS_DEGREES;
		real_vector3d vector;

		vector.i = (real)cos(angle) * 5.0f;
		vector.j = (real)sin(angle) * 5.0f;
		vector.k = 0.0f;
		if (collision_test_vector(flags, &start, &vector, vehicle_index, &collision))
		{
			real near = 1.0f - collision.t;

			push_x -= (real)cos(angle) * near;
			push_y -= (real)sin(angle) * near;
		}
	}
	push = (real)sqrt(push_x * push_x + push_y * push_y);
	if (push < 0.05f)
		return;
	if (push > 1.0f)
	{
		push_x /= push;
		push_y /= push;
	}
	ut->wall_push_x = push_x * 3.5f;
	ut->wall_push_y = push_y * 3.5f;
	waypoint->x += ut->wall_push_x;
	waypoint->y += ut->wall_push_y;
}

/* (ae_bots_vehicle_move, every bot every tick) a slot's state is its unit's: a new unit (respawn, another player in the
slot) starts clean (review: a new player could inherit a 30 s flip job) */
static void ae_bots_ut_slot(struct bots_bot *bot, long bot_index)
{
	struct ae_ut_bot *ut;

	if (bot_index < 0 || bot_index >= BOTS_BRAINS)
		return;
	ut = &ae_ut.bots[bot_index];
	if (ut->unit_index == bot->unit_index)
		return;
	csmemset(ut, 0, sizeof(*ut));
	ut->unit_index = bot->unit_index;
	ut->judged_object = NONE;
	ut->flip_vehicle = NONE;
	ut->hailer = NONE;
	ae_bots_fear_slot(bot_index);
}
