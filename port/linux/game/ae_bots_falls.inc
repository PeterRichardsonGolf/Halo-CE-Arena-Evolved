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
	/* (debug.bot_events) the guard's last probes of its way while on the ground: the death line's probe= */
	char probe[48];
};

/* (debug.bot_events) the last probe's finding: W a wall (how far), N no floor, F a floor (the drop, its normal's
height), S a steep floor slid down (the same; ! when it slides off), L a big drop's landing with none beyond */
static char ae_falls_probe_note[24];

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

/* the drop that kills a player here (ae_bots_edge_lethal_distance): damage.c's game_globals_falling_damage (its size
asserted there, 0x98) has the distance of a landing's full damage at 0x0C and the maximum falling distance at 0x28 */
static real ae_falls_lethal_distance(long player_index)
{
	struct game_globals *globals = scenario_get_game_globals();
	real maximum = 0.0f, full = 0.0f;

	if (globals && globals->falling_damage.count > 0)
	{
		byte const *element = (byte const *)TAG_BLOCK_GET_ELEMENT(&globals->falling_damage, 0, byte);

		full = *(real const *)(element + 0x0C);
		maximum = *(real const *)(element + 0x28);
	}
	return ae_bots_edge_lethal_distance(game_engine_no_falling_damage(player_index), maximum, full);
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
		snprintf(ae_falls_probe_note, sizeof(ae_falls_probe_note), "W%.1f", collision.t * distance);
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
		snprintf(ae_falls_probe_note, sizeof(ae_falls_probe_note), "N");
		return (boolean)ae_bots_edge_lethal(FALSE, 0.0f, lethal);
	}
	if (collision.plane.n.k < AE_FALLS_STEEP)
	{
		boolean slid = ae_falls_slide_lethal(unit_index, position->z, &collision, lethal);

		snprintf(ae_falls_probe_note, sizeof(ae_falls_probe_note), "S%.1f/%.2f%s", position->z - collision.point.z,
			collision.plane.n.k, slid ? "!" : "");
		return slid;
	}
	snprintf(ae_falls_probe_note, sizeof(ae_falls_probe_note), "F%.1f/%.2f", position->z - collision.point.z,
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
				snprintf(ae_falls_probe_note, sizeof(ae_falls_probe_note), "D%.1f", position->z - collision.point.z);
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
				ae_bots_edge_lethal(TRUE, position->z - collision.point.z, lethal))
			{
				snprintf(ae_falls_probe_note, sizeof(ae_falls_probe_note), "L%.1f", position->z - landing.z);
				return TRUE;
			}
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

/* GRENADES: away from its own live grenade within the danger (the nearest) */
static void ae_falls_flee(struct bots_bot *bot, struct ae_falls_bot *falls, struct bots_tick *tick, int level)
{
	long index, nearest = NONE;
	real best = REAL_MAX;

	ae_falls_find_grenades(tick->now);
	falls->own_grenade_distance = REAL_MAX;
	for (index = 0; index < ae_falls.grenade_count; index++)
	{
		real distance;

		if (ae_falls.grenade_owners[index] != bot->player_index)
			continue;
		distance = bots_distance3d(&ae_falls.grenade_points[index], &tick->position);
		falls->own_grenade_distance = MIN(falls->own_grenade_distance, distance);
		if (ae_bots_grenade_flee(level, TRUE, distance) && distance < best)
		{
			best = distance;
			nearest = index;
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
			ae_bots_event_line((long)(bot - bots_globals.bots), tick->now, "flee", -1, best, "");
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
			"own grenades run from %ld, throws held %ld, explosive shots held %ld (lethal drop %.1f)", now,
			ae_falls.steps_turned, ae_falls.steps_held, ae_falls.goals_given_up, ae_falls.jumps_held, ae_falls.flights,
			ae_falls.throws_held, ae_falls.shots_held, ae_falls.lethal);
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
static void ae_bots_ut_dodge(struct bots_bot *bot, struct bots_tick *tick, int level);
static void ae_bots_fear_tick(struct bots_bot *bot, struct bots_tick *tick, int level);

/* (bots.c's bots_bot_update, after the unstick) THE GUARD */
static void ae_bots_falls_guard(struct bots_bot *bot, struct bots_tick *tick)
{
	long bot_index = (long)(bot - bots_globals.bots);
	struct ae_falls_bot *falls;
	struct ae_travel_bot *travel;
	struct biped_datum *biped;
	int level = ae_bots_level_of_skill(ae_bots_movement_skill(bots_globals.skill));
	long unit_index = tick->player->unit_index;
	real lethal, lookahead;
	boolean carried = FALSE;
	real carry_way = 0.0f;

	if (bot_index < 0 || bot_index >= BOTS_BRAINS || !tick->object || tick->object->object.parent_object_index != NONE)
		return;
	falls = &ae_falls_bots[bot_index];
	ae_falls_report(tick->now);
	ae_falls_flee(bot, falls, tick, ae_bots_level_of_skill(ae_bots_shooting_skill(bots_globals.skill)));
	ae_falls_throw(bot, tick);
	ae_falls_explosive(bot, tick, ae_bots_level_of_skill(ae_bots_shooting_skill(bots_globals.skill)));
	/* (the UT spike: off the way of a slow shot or a vehicle; the edge checks below still hold a step) */
	ae_bots_ut_dodge(bot, tick, level);
	/* (the F.E.A.R. spike: the squad's roles, flank and suppress) */
	ae_bots_fear_tick(bot, tick, level);
	travel = ae_travel_state(bot);
	biped = biped_try_and_get(unit_index);
	/* (a layout jump the bot takes, walking to it: the jump is the way; a pad's last steps are guarded as any, as is
	a fight on the way: Boarding Action 10-20, a bot fighting at a pad's entry walked off the ship) */
	if (!biped || (travel && !tick->fighting && travel->via >= 0 && travel->phase != _ae_travel_phase_to_entry &&
		ae_travel.links[travel->via].kind == AE_BOTS_TRAVEL_LINK_JUMP))
	{
		return;
	}
	/* (RECRUIT and EASY, in a fight, look now and then: a half second at a time) */
	if (ae_bots_combat_hash01((unsigned long)bot->player_index ^ 0x6a09e667UL, (unsigned long)(tick->now / 15)) >=
		ae_bots_edge_attention(level, tick->fighting))
	{
		return;
	}
	lethal = ae_falls_lethal_distance(bot->player_index);
	ae_falls.lethal = lethal;
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

			/* (a fight: the strafe turns to the other side, tried first, when that way is clear; a way taken brakes
			what its speed carries it to) */
			if (tick->fighting && !ae_falls_way_lethal(unit_index, &tick->position, way + BOTS_PI, lookahead, lethal) &&
				!(carried && (real)cos(way + BOTS_PI - carry_way) > -0.3f))
			{
				bot->strafe_sign = -bot->strafe_sign;
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
				char extra[80];

				falls->edge_event_tick = tick->now;
				snprintf(extra, sizeof(extra), "what=%s fighting=%d air=%d x=%.1f y=%.1f z=%.1f", found ? "turned" : "held",
					tick->fighting ? 1 : 0, TEST_FLAG(biped->biped.flags, _biped_airborne_bit) ? 1 : 0, tick->position.x,
					tick->position.y, tick->position.z);
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
					ae_falls_away_goal(bot, tick, way);
					ae_falls.goals_given_up++;
				}
			}
		}
	}
	ae_falls_target_known = FALSE;
	/* (a jump: where it lands, along its way or its look; NORMAL and up, any way about it, as its momentum and its air
	control may take it any way: Boarding Action 10-20, bots jumping in a fight at the hull's edge fell) */
	if (TEST_FLAG(tick->flags, _unit_control_jump_bit))
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
