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

#include "infection.h"

/* INFECTION ZOMBIES (OpenCE's Infection, infection.c; owner 10-24: "if the bot is the initial infected it can't kill any
other bots or players ... make it aggressive with the oddball"). Infection plays team Slayer's engine, so the
objective play (ae_bots_objective.inc) sees Slayer, and a zombie, holding the Oddball skull alone, fought as an armed
bot: bots.c kept the skull's distance (its name unknown to bots_weapon_range: 7 units, backing off under 4.2), the
confidence of a bot with an "empty" weapon and no shields sent it into RETREAT, its blow waited for bots.c's reach
(HEROIC and up: point blank, shields up), and its goals took it to weapons and vehicles it may not take. A zombie bot
now hunts: its goal is the enemy it last saw or heard (bots.c's target memory, never where they are now; reached with
no one there, the spot is spent), else roaming as bots.c's hunt; in a fight it runs at its target (along its path when
the target is on another level), with no retreat, halt, leash or trigger, and strikes within reach at every level
(ae_bots_combat_zombie_melee). A human bot fighting a zombie neither charges nor steps in for a blow, and backs off
from one closer than 4 units. Only while Infection plays (infection_unit_is_zombie asks infection_enabled first):
Oddball, Juggernaut, Bomb, Grifball and every other mode keep their play. */
static struct ae_bots_combat_bot *ae_bots_fight_state(struct bots_bot *bot, struct bots_tick *tick);

static boolean ae_bots_zombie(long unit_index)
{
	return unit_index != NONE && infection_unit_is_zombie(unit_index);
}

/* MELEE-ONLY BOMB (Grifball: game_engine_bomb.c's _bomb_melee_only_bit; owner 10-24, bots play Bomb): every player has
fists (or the map's sword) and the bomb, nothing that shoots, so a bot fights as a zombie does: at its target, its blow
within reach at every level, no trigger, no retreat or kept distance. Unlike a zombie it keeps its objective play
(ae_bots_objective.inc: the bomb, the goals), and its targets are not zombies to back off from. */
static boolean ae_bots_brawler(long unit_index)
{
#ifdef HALO_GAME_BROWSER
	if (unit_index != NONE && game_engine_bomb_melee_only())
		return TRUE;
#endif
	return ae_bots_zombie(unit_index);
}

/* the bot's target is a zombie (a human bot's fight) */
static boolean ae_bots_fight_target_zombie(struct bots_bot *bot)
{
	struct player_datum *target = bot->target_player_index != NONE ? player_try_and_get(bot->target_player_index) : NULL;

	return target && ae_bots_zombie(target->unit_index);
}

/* a zombie's goal (bots_choose_goal's, before the objectives, power items and teammates: ae_bots_objective_goal), TRUE
when the bot is one */
static boolean ae_bots_zombie_goal(struct bots_bot *bot, real_point3d const *position, boolean fighting)
{
	long now = game_time_get();

	if (!ae_bots_zombie(bot->unit_index))
		return FALSE;
	if (bot->target_player_index != NONE && bot->target_last_surface_index != NONE &&
		now - bot->target_seen_tick < TICKS_PER_SECOND * 8)
	{
		if (fighting || bots_distance3d(&bot->target_last_position, position) > 1.5f)
		{
			bots_set_goal(bot, _bots_goal_enemy, &bot->target_last_position, bot->target_last_surface_index);
			return TRUE;
		}
		bot->target_last_surface_index = NONE;
	}
	if (bot->goal_kind != _bots_goal_roam || bots_distance2d(&bot->goal_point, position) < 1.5f ||
		now - bot->goal_tick > TICKS_PER_SECOND * (bot->hunting ? 12 : 40))
	{
		bots_pick_roam_goal(bot, position);
	}
	return TRUE;
}

/* a zombie's fight's move (ae_bots_fight_move's): at its target */
static void ae_bots_fight_zombie_move(struct bots_bot *bot, struct bots_tick *tick)
{
	struct ae_bots_combat_bot *state = ae_bots_fight_state(bot, tick);
	real_point3d target_point = ae_bots_vehicle_point(player_get(bot->target_player_index)->unit_index);
	real direction = (real)atan2(target_point.y - tick->position.y, target_point.x - tick->position.x);

	/* (a retreat begun before it turned is over: bots.c chooses its goal afresh) */
	if (state->retreat.active)
	{
		ae_bots_combat_retreat_reset(&state->retreat);
		bot->goal_kind = _bots_goal_none;
	}
	state->charging = FALSE;
	state->halt_until = 0;
	/* (a target on another level: the path to where it was seen; else straight at it, weaving a little until close) */
	if ((real)fabs(target_point.z - tick->position.z) > 1.2f && bot->target_last_surface_index != NONE)
	{
		real path_direction;

		bots_set_goal(bot, _bots_goal_enemy, &bot->target_last_position, bot->target_last_surface_index);
		if (bots_follow_path(bot, tick->player->unit_index, &tick->position, &path_direction))
			direction = path_direction;
	}
	else if (tick->target_distance > 3.0f)
		direction += bot->strafe_sign * 0.2f;
	tick->move_direction = direction;
	tick->moving = TRUE;
	tick->throttle_scale = 1.0f;
}

static long ae_bots_fight_index(struct bots_bot *bot)
{
	return (long)(bot - bots_globals.bots);
}

static struct ae_bots_combat_bot *ae_bots_fight_state(struct bots_bot *bot, struct bots_tick *tick)
{
	return ae_bots_combat_bot_state(ae_bots_fight_index(bot), tick->player->unit_index);
}

static void ae_bots_fight_event(struct bots_bot *bot, struct bots_tick *tick, char const *event, real distance,
	char const *extra);

/* A teammate between this bot and a shot, or near a grenade's expected landing place. Players in vehicles use the
object's world centre, as the target does. */
static boolean ae_bots_fight_teammate_near(struct bots_bot *bot, struct bots_tick *tick,
	real_point3d const *point, boolean grenade)
{
	struct data_iterator iterator;
	struct player_datum *teammate;
	real_point3d const *eye = bots_tick_eye(tick);

	/* (HALO_BOTS_FRIENDLY_FIRE=1: no holds, for the before and after of a test) */
	if (!game_engine_has_teams() || getenv("HALO_BOTS_FRIENDLY_FIRE"))
		return FALSE;
	data_iterator_new(&iterator, player_data);
	while ((teammate = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
	{
		struct object_datum *object;
		real_point3d teammate_point;

		if (iterator.datum_index == bot->player_index || teammate->team_index != tick->player->team_index ||
			teammate->unit_index == NONE)
		{
			continue;
		}
		object = object_get(teammate->unit_index);
		if (TEST_FLAG(object->object.damage_flags, _object_dead_bit))
			continue;
		teammate_point = object->object.bounding_sphere_center;
		if (grenade ? ae_bots_combat_grenade_near_teammate(point->x, point->y, point->z,
			teammate_point.x, teammate_point.y, teammate_point.z) :
			ae_bots_combat_teammate_blocks(eye->x, eye->y, eye->z, teammate_point.x, teammate_point.y,
				teammate_point.z, point->x, point->y, point->z))
		{
			return TRUE;
		}
	}
	return FALSE;
}

static boolean ae_bots_fight_friendly_fire_clear(struct bots_bot *bot, struct bots_tick *tick)
{
	struct ae_bots_combat_bot *state = ae_bots_fight_state(bot, tick);
	struct player_datum *target = player_try_and_get(bot->target_player_index);
	boolean blocked = target && target->unit_index != NONE &&
		ae_bots_fight_teammate_near(bot, tick, &object_get(target->unit_index)->object.bounding_sphere_center, FALSE);

	if (blocked && !state->friendly_fire_held)
	{
		state->friendly_fire_held = TRUE;
		ae_bots_combat_count(AE_BOTS_COMBAT_FRIENDLY_FIRE_HELD);
		ae_bots_fight_event(bot, tick, "hold_fire", tick->target_distance, "reason=teammate");
	}
	else if (!blocked)
		state->friendly_fire_held = FALSE;
	return (boolean)!blocked;
}

static boolean ae_bots_fight_friendly_grenade_clear(struct bots_bot *bot, struct bots_tick *tick,
	real_point3d const *landing)
{
	struct ae_bots_combat_bot *state = ae_bots_fight_state(bot, tick);
	boolean blocked = ae_bots_fight_teammate_near(bot, tick, landing, TRUE);

	if (blocked && !state->friendly_grenade_held)
	{
		state->friendly_grenade_held = TRUE;
		ae_bots_combat_count(AE_BOTS_COMBAT_FRIENDLY_GRENADES_HELD);
		ae_bots_fight_event(bot, tick, "hold_grenade", bots_distance3d(&tick->position, landing), "reason=teammate");
	}
	else if (!blocked)
		state->friendly_grenade_held = FALSE;
	return (boolean)!blocked;
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

/* LONG RANGE (and, for NORMAL, CLOSE RANGE: ae_bots_combat_near_error_floor): an error below the far target's floor is
pushed out to it (0 to 30% past it, its side kept), so the aim stays off by about the floor and a shot at a person of about that size hits about half the time */
static void ae_bots_fight_range_floor(struct bots_bot *bot, struct bots_tick *tick)
{
	int level = ae_bots_level_of_skill(ae_bots_shooting_skill(bots_globals.skill));
	real floor = MAX(ae_bots_combat_range_error_floor(level, tick->target_distance),
		ae_bots_combat_near_error_floor(level, tick->target_distance)) * BOTS_DEGREES;
	real *errors[2];
	real scale[2] = { 1.0f, 0.6f };
	real chance = ae_bots_combat_long_range_hit_chance(level, tick->target_distance);
	short index;

	/* FAR SHOTS: a new roll about five times a second; on target within the body, else its side kept just off it, still
	inside the fire cone, so it keeps firing */
	if (chance >= 0.0f)
	{
		struct ae_bots_combat_bot *state = ae_bots_fight_state(bot, tick);
		real body = (real)atan(0.35f / tick->target_distance);

		if (tick->now >= state->far_shot_until)
		{
			state->far_shot_until = tick->now + TICKS_PER_SECOND / 5;
			state->far_shot_on_target = bots_random_real(bot) < chance;
		}
		if (state->far_shot_on_target)
		{
			bot->aim_error_yaw = PIN(bot->aim_error_yaw, -body * 0.4f, body * 0.4f);
			bot->aim_error_pitch = PIN(bot->aim_error_pitch, -body * 0.5f, body * 0.5f);
		}
		else if ((real)fabs(bot->aim_error_yaw) < body * 1.3f)
		{
			bot->aim_error_yaw = (bot->aim_error_yaw < 0.0f ? -1.0f : 1.0f) * body *
				(1.3f + 0.2f * (real)fabs(bots_random_signed(bot)));
		}
		return;
	}
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

	if (!ae_bots_fight_friendly_fire_clear(bot, tick))
		return FALSE;
	/* (a zombie's skull, a melee-only Bomb game's fists: its blow alone, no trigger) */
	if (ae_bots_brawler(tick->player->unit_index))
		return FALSE;
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

	ae_bots_vehicle_threat_note(bot, tick, target);
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
		/* (at most 3.5 units ahead: the higher levels throw that far in front of the feet) */
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
	boolean zombie = ae_bots_brawler(tick->player->unit_index);
	int choice = zombie ? ae_bots_combat_zombie_melee(level, tick->target_distance, tick->now >= bot->melee_tick,
		bots_random_real(bot)) : ae_bots_combat_melee(level, tick->target_distance, tick->now >= bot->melee_tick,
		ae_bots_combat_back_turned(target_unit, &tick->position), object_get(target_unit)->object.shield_vitality,
		state->charging, bots_random_real(bot));

	/* (within reach, it waits its level's moment first; out of reach, it waits afresh next time) */
	if (choice == 0)
	{
		state->melee_ready_tick = NONE;
		return FALSE;
	}
	if (state->melee_ready_tick == NONE)
		state->melee_ready_tick = tick->now + (zombie ? ae_bots_combat_zombie_melee_delay(level, bots_random_real(bot)) :
			ae_bots_combat_melee_delay(level, bots_random_real(bot)));
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
	/* (no blow with a teammate in its arc: a pistol-whip killed one beside its target) */
	if (choice == 1 && ae_bots_fight_teammate_near(bot, tick, &object_get(target_unit)->object.bounding_sphere_center, FALSE))
	{
		bot->melee_tick = tick->now + TICKS_PER_SECOND / 3;
		return FALSE;
	}
	if (choice == 1)
		ae_bots_combat_count(AE_BOTS_COMBAT_MELEES);
	return choice == 1;
}

static long ae_bots_fight_melee_ticks(struct bots_bot *bot, struct bots_tick *tick)
{
	int level = ae_bots_level_of_skill(ae_bots_shooting_skill(bots_globals.skill));

	if (ae_bots_brawler(tick->player->unit_index))
		return ae_bots_combat_zombie_melee_cooldown(level, bots_random_real(bot));
	return ae_bots_combat_melee_cooldown(level, bots_random_real(bot));
}

static void ae_bots_fight_grenade(struct bots_bot *bot, struct bots_tick *tick, long reaction_ticks)
{
	short shooting = ae_bots_shooting_skill(bots_globals.skill);
	int level = ae_bots_level_of_skill(shooting);
	struct ae_bots_combat_bot *state;
	long now = tick->now;
	boolean has = tick->unit->unit.grenade_counts[0] > 0 || tick->unit->unit.grenade_counts[1] > 0;
	real_point3d landing;
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
	landing = object_get(player_get(bot->target_player_index)->unit_index)->object.bounding_sphere_center;
	{
		struct object_datum *target = object_get(player_get(bot->target_player_index)->unit_index);
		real speed = ae_bots_combat_grenade_speed(tick->player->unit_index);
		real flight = speed > 0.01f ? MIN(tick->target_distance / speed, 60.0f) : 0.0f;
		real lead = ae_bots_combat_grenade_lead(level);

		landing.x += target->object.translational_velocity.i * flight * lead;
		landing.y += target->object.translational_velocity.j * flight * lead;
		landing.z += target->object.translational_velocity.k * flight * lead;
	}
	if (!ae_bots_fight_friendly_grenade_clear(bot, tick, &landing))
	{
		bot->grenade_tick = now + TICKS_PER_SECOND;
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
			if (!ae_bots_fight_friendly_grenade_clear(bot, tick, &state->prenade_point))
			{
				state->prenade_until = tick->now + 1;
				return;
			}
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
		if (!ae_bots_fight_friendly_grenade_clear(bot, tick, &point))
		{
			bot->grenade_tick = tick->now + TICKS_PER_SECOND;
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

static void ae_bots_fight_move_armed(struct bots_bot *bot, struct bots_tick *tick)
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
		tick->position.z - target_point.z) - ae_bots_team_outnumbered(bot, tick, level) + ae_bots_revenge_tilt(bot, level);
	int started, ended;
	/* (INFECTION: a zombie's target is neither charged nor stepped in on, and is backed off from) */
	boolean zombie_target = ae_bots_fight_target_zombie(bot);
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
		boolean charge = !zombie_target &&
			ae_bots_combat_charge(level, confidence, target_shield, target_body, tick->target_distance);

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
		if (!charge && !zombie_target && ae_bots_combat_step_in(level, tick->target_distance, target_shield))
		{
			tick->move_direction = to_target + bot->strafe_sign * 0.3f;
			tick->moving = TRUE;
			tick->throttle_scale = 1.0f;
		}
		if (zombie_target && tick->target_distance < 4.0f)
		{
			tick->move_direction = to_target + BOTS_PI + bot->strafe_sign * 0.5f;
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
	/* JUMPS WITH PURPOSE (spike 10-26, owner: "gandhi hopping with purpose or just randomly?"): a jump to spoil the
	enemy's aim, when it is hurt just now, or (HEROIC and up) when its enemy aims right at it (closer still when
	zoomed); the level's chance, then a moment before the next */
	if (level >= AE_BOTS_LEVEL_NORMAL)
	{
		static long purpose_until[BOTS_BRAINS];
		long bot_index = (long)(bot - bots_globals.bots);
		struct player_datum *enemy = bot->target_player_index != NONE ? player_try_and_get(bot->target_player_index) : NULL;
		struct unit_datum *enemy_unit = enemy && enemy->unit_index != NONE ?
			(struct unit_datum *)object_try_and_get_and_verify_type(enemy->unit_index, _object_mask_unit) : NULL;
		boolean hurt = bot->hurt_tick > 0 && tick->now - bot->hurt_tick <= 2;
		boolean aimed = FALSE;
		real chance = level >= AE_BOTS_LEVEL_LEGENDARY ? 0.8f : level == AE_BOTS_LEVEL_HEROIC ? 0.6f : 0.35f;
		long rest = level >= AE_BOTS_LEVEL_LEGENDARY ? TICKS_PER_SECOND * 4 / 5 : level == AE_BOTS_LEVEL_HEROIC ?
			TICKS_PER_SECOND * 6 / 5 : TICKS_PER_SECOND * 2;

		if (enemy_unit && level >= AE_BOTS_LEVEL_HEROIC)
		{
			real_vector3d to_bot;
			real length, cosine, limit;

			vector_from_points3d(&enemy_unit->object.position, &tick->position, &to_bot);
			length = magnitude3d(&to_bot);
			cosine = length > 0.01f ? dot_product3d(&to_bot, &enemy_unit->unit.aiming_vector) / length : 0.0f;
			/* (within 8 degrees of its aim; zoomed, 3) */
			limit = enemy_unit->unit.current_zoom_level != NONE ? 0.9986f : 0.9903f;
			aimed = cosine > limit;
		}
		if (bot_index >= 0 && bot_index < BOTS_BRAINS && tick->now >= purpose_until[bot_index] && (hurt || aimed))
		{
			purpose_until[bot_index] = tick->now + rest;
			if (bots_random_real(bot) < chance)
			{
				tick->flags |= FLAG(_unit_control_jump_bit);
				ae_bots_combat_count(AE_BOTS_COMBAT_JUMPS);
				/* (the rhythm's next jump put off: a purposeful one just now) */
				if (state->next_vertical_tick > 0)
					state->next_vertical_tick = tick->now + ae_bots_combat_vertical_delay(level, bots_random_real(bot));
				if (ae_bots_events_on())
					platform_log("bots: ev=jump_purpose bot=%ld why=%s", bot_index + 1, hurt ? "hurt" : "aimed_at");
			}
		}
	}
	/* JUMPS in a fight (NORMAL and up), at the level's rhythm (now and then too, so they are not read; half as often
	since the purposeful ones) */
	if (state->next_vertical_tick > 0 && tick->now >= state->next_vertical_tick)
	{
		long delay = 2 * ae_bots_combat_vertical_delay(level, bots_random_real(bot));

		state->next_vertical_tick = delay > 0 ? tick->now + delay : 0;
		if (delay > 0)
		{
			tick->flags |= FLAG(_unit_control_jump_bit);
			ae_bots_combat_count(AE_BOTS_COMBAT_JUMPS);
		}
	}
}

static void ae_bots_fight_move(struct bots_bot *bot, struct bots_tick *tick)
{
	if (ae_bots_brawler(tick->player->unit_index))
		ae_bots_fight_zombie_move(bot, tick);
	else
		ae_bots_fight_move_armed(bot, tick);
	ae_bots_combat_report(tick->now, ae_bots_skill_definition(ae_bots_shooting_skill(bots_globals.skill))->name,
		ae_bots_skill_definition(ae_bots_movement_skill(bots_globals.skill))->name);
}

/* CROUCH JUMP (owner 2026-10-28): the top of a jump a bot makes on its way (a travel link's ledge, a climb, a jump out
of a stuck spot), crouched to the landing; RECRUIT never. Not in a fight (owner 2026-10-29, "crouch jumping on
everything": every dodge was one): EASY and up alike. Kept by the bot's player index. */
static struct { long jump_tick; boolean crouching; } ae_bots_jumping[AE_BOTS_TRACK_PLAYERS];

static void ae_bots_crouch_jump_tick(struct bots_bot *bot, struct bots_tick *tick)
{
	short skill = ae_bots_movement_skill(bots_globals.skill);
	long player = bot->player_index == NONE ? -1 : DATUM_INDEX_TO_ABSOLUTE_INDEX(bot->player_index);
	struct biped_datum *biped;
	boolean airborne;

	if (player < 0 || player >= AE_BOTS_TRACK_PLAYERS || skill == AE_BOTS_SKILL_RECRUIT)
		return;
	if (tick->now < ae_bots_jumping[player].jump_tick)
		ae_bots_jumping[player].jump_tick = NONE;
	if (TEST_FLAG(tick->flags, _unit_control_jump_bit))
	{
		ae_bots_jumping[player].jump_tick = tick->now;
		ae_bots_jumping[player].crouching = FALSE;
		return;
	}
	biped = biped_try_and_get(tick->player->unit_index);
	airborne = biped && TEST_FLAG(biped->biped.flags, _biped_airborne_bit);
	if (!airborne)
	{
		ae_bots_jumping[player].crouching = FALSE;
		return;
	}
	if (!ae_bots_jumping[player].crouching &&
		ae_bots_combat_crouch_jump(TRUE, tick->now - ae_bots_jumping[player].jump_tick,
			tick->object->object.translational_velocity.k) &&
		!tick->fighting)
	{
		ae_bots_jumping[player].crouching = TRUE;
	}
	if (ae_bots_jumping[player].crouching)
		tick->flags |= FLAG(_unit_control_crouch_modifier_bit);
}

/* BOT TELLS (owner 2026-10-28, "scanning and defending look robotic"): a bot standing at its post turned at a steady two
degrees a tick, round and round. It now looks to one angle at a time: eased into the turn, held 0.5 to 1.8 s (now and then
2.5 to 4.5 s, as a person holds an angle), and the next angle nearer the way the objective has it watch (a defender, the
enemy's way) than behind it; the pitch drifts a few degrees. A scan that was not made for a moment starts again from
where the bot faces. */
static struct
{
	long last_tick, next_tick;
	real target_yaw, centre_yaw, target_pitch;
} ae_bots_scans[BOTS_BRAINS];

static void ae_bots_look_about(struct bots_bot *bot, struct bots_tick *tick)
{
	long index = (long)(bot - bots_globals.bots);
	real centre, off, step;
	boolean watching;

	if (index < 0 || index >= BOTS_BRAINS)
	{
		bot->yaw += 2.0f * BOTS_DEGREES;
		return;
	}
	watching = ae_bots_objective_hold_yaw(index, &centre);
	if (tick->now - ae_bots_scans[index].last_tick > 10 || tick->now < ae_bots_scans[index].last_tick)
	{
		ae_bots_scans[index].centre_yaw = bot->yaw;
		ae_bots_scans[index].target_yaw = bot->yaw;
		ae_bots_scans[index].target_pitch = 0.0f;
		ae_bots_scans[index].next_tick = tick->now + 6 + (long)(bots_random(bot) % 14);
	}
	ae_bots_scans[index].last_tick = tick->now;
	if (watching)
		ae_bots_scans[index].centre_yaw = centre;
	if (tick->now >= ae_bots_scans[index].next_tick)
	{
		real spread = watching ? 80.0f : 150.0f;
		real pick = bots_random_signed(bot);
		real target = ae_bots_scans[index].centre_yaw + pick * spread * BOTS_DEGREES;

		/* (a new angle is a real turn, 25 degrees or more: the other side if it came out near the old one) */
		if ((real)fabs(bots_angle_difference(target, bot->yaw)) < 25.0f * BOTS_DEGREES)
			target = ae_bots_scans[index].centre_yaw - pick * spread * BOTS_DEGREES;
		ae_bots_scans[index].target_yaw = target;
		ae_bots_scans[index].target_pitch = bots_random_signed(bot) * 5.0f * BOTS_DEGREES;
		ae_bots_scans[index].next_tick = tick->now + (bots_random_real(bot) < 0.2f ?
			TICKS_PER_SECOND * 5 / 2 + (long)(bots_random(bot) % (TICKS_PER_SECOND * 2)) :
			TICKS_PER_SECOND / 2 + (long)(bots_random(bot) % (TICKS_PER_SECOND * 4 / 3)));
	}
	off = bots_angle_difference(ae_bots_scans[index].target_yaw, bot->yaw);
	step = off * 0.14f;
	if (fabs(off) > 0.5f * BOTS_DEGREES && fabs(step) < 0.4f * BOTS_DEGREES)
		step = off > 0.0f ? 0.4f * BOTS_DEGREES : -0.4f * BOTS_DEGREES;
	bot->yaw += PIN(step, -7.0f * BOTS_DEGREES, 7.0f * BOTS_DEGREES);
	bot->pitch += PIN(ae_bots_scans[index].target_pitch - bot->pitch, -0.01f, 0.01f);
}

/* RANGE SWAP (0.7.0): a bot fighting with a close-range weapon (a shotgun's, a flamethrower's: its range 3 units or
less) whose enemy is more than twice that and at least 6 units away takes its other weapon, when that one has rounds
and reaches further; not more often than every 2 seconds, so a fight at the edge does not switch back and forth (the
swap back is the empty weapon's or the player's own pick, as before) */
/* AMMUNITION (owner 2026-10-30: the assault rifle is not as good; bots spent the 60 rounds of their rifle, then kept to the assault rifle
to the end of the life): a weapon of a kind it holds, with rounds in it, lying near while its own has 15 rounds or fewer: its goal,
walking over it fills the magazines */
#define AE_BOTS_REFILL_ROUNDS 15
#define AE_BOTS_GOOD_WEAPON_VALUE 7

static boolean ae_bots_item_refills(struct unit_datum *unit, long item_index)
{
	struct weapon_datum *item = weapon_get(item_index);
	short slot;

	if (bots_weapon_value(item->definition_index) < AE_BOTS_GOOD_WEAPON_VALUE ||
		weapon_definition_get(item->definition_index)->weapon.magazines.count <= 0 ||
		item->weapon.magazines[0].rounds_loaded + item->weapon.magazines[0].rounds_total <= 0)
	{
		return FALSE;
	}
	for (slot = 0; slot < MAXIMUM_WEAPONS_PER_UNIT; slot++)
	{
		long other = unit->unit.weapon_object_indices[slot];

		if (other != NONE && weapon_get(other)->definition_index == item->definition_index)
			return weapon_get(other)->weapon.magazines[0].rounds_loaded + weapon_get(other)->weapon.magazines[0].rounds_total <= AE_BOTS_REFILL_ROUNDS;
	}
	return FALSE;
}

/* (AMMUNITION) on foot with a lesser weapon in hand and a good one held with rounds: back to it, as soon as it has rounds again */
static boolean ae_bots_good_weapon_back(struct bots_bot *bot, struct bots_tick *tick, long *last)
{
	short slot;

	if (tick->weapon_index == NONE || tick->object->object.parent_object_index != NONE || tick->now - *last < 2 * TICKS_PER_SECOND ||
		bots_weapon_value(weapon_get(tick->weapon_index)->definition_index) >= AE_BOTS_GOOD_WEAPON_VALUE)
	{
		return FALSE;
	}
	for (slot = 0; slot < MAXIMUM_WEAPONS_PER_UNIT; slot++)
	{
		long other = tick->unit->unit.weapon_object_indices[slot];

		if (slot != tick->unit->unit.current_weapon_index && other != NONE && !bots_weapon_empty(other) && bots_weapon_range(other) > 3.0f &&
			bots_weapon_value(weapon_get(other)->definition_index) >= AE_BOTS_GOOD_WEAPON_VALUE)
		{
			tick->action->desired_weapon_index = slot;
			*last = tick->now;
			platform_log("bots: ev=good_weapon_back player=%ld slot=%d", (long)DATUM_INDEX_TO_ABSOLUTE_INDEX(bot->player_index), (int)slot);
			return TRUE;
		}
	}
	return FALSE;
}

static void ae_bots_range_swap(struct bots_bot *bot, struct bots_tick *tick)
{
	static long swapped_at[64];
	long *last = &swapped_at[DATUM_INDEX_TO_ABSOLUTE_INDEX(bot->player_index) & 63];
	real range, best_range;
	short slot, best = NONE;

	if (ae_bots_good_weapon_back(bot, tick, last))
		return;
	if (!tick->fighting || tick->weapon_index == NONE || bots_weapon_empty(tick->weapon_index))
		return;
	range = bots_weapon_range(tick->weapon_index);
	if (range > 3.0f || tick->target_distance < MAX(6.0f, range * 2.0f) || tick->now - *last < 2 * TICKS_PER_SECOND)
		return;
	best_range = range;
	for (slot = 0; slot < MAXIMUM_WEAPONS_PER_UNIT; slot++)
	{
		long other = tick->unit->unit.weapon_object_indices[slot];

		if (slot != tick->unit->unit.current_weapon_index && other != NONE && !bots_weapon_empty(other) &&
			bots_weapon_range(other) > best_range)
		{
			best = slot;
			best_range = bots_weapon_range(other);
		}
	}
	if (best == NONE)
		return;
	tick->action->desired_weapon_index = best;
	*last = tick->now;
	platform_log("bots: ev=range_swap player=%ld dist=%.1f range=%.1f slot=%d",
		(long)DATUM_INDEX_TO_ABSOLUTE_INDEX(bot->player_index), (double)tick->target_distance, (double)range, (int)best);
}
