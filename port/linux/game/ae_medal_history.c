/*
AE_MEDAL_HISTORY.C

Pure records and questions for MEDALS (ae_medal_history.h). No engine includes.
*/

#include "ae_medal_history.h"

#include <math.h>
#include <string.h>

#define PI_F 3.14159265f
#define NO_WEAPON (-1L)

/* ---------- private code */

static struct ae_history_player *player_of(struct ae_history *history, int player)
{
	return player >= 0 && player < AE_MEDAL_RULES_PLAYERS ? &history->players[player] : NULL;
}

static struct ae_history_player const *player_const(struct ae_history const *history, int player)
{
	return player >= 0 && player < AE_MEDAL_RULES_PLAYERS ? &history->players[player] : NULL;
}

static int is_grenade(struct ae_history_hit const *hit)
{
	return hit->kind == AE_MEDAL_WEAPON_FRAG_GRENADE || hit->kind == AE_MEDAL_WEAPON_PLASMA_GRENADE ||
		hit->kind == AE_MEDAL_WEAPON_PLASMA_GRENADE_STUCK;
}

static float wrap_angle(float delta)
{
	while (delta > PI_F)
		delta -= 2.0f * PI_F;
	while (delta < -PI_F)
		delta += 2.0f * PI_F;
	return delta;
}

/* the i-th newest shot (0 the newest), NULL past the ring */
static struct ae_history_shot const *shot_back(struct ae_history_player const *player, int back)
{
	if (back < 0 || back >= player->shot_count)
		return NULL;
	return &player->shots[(player->shot_next - 1 - back + 2 * AE_HISTORY_SHOTS) % AE_HISTORY_SHOTS];
}

static struct ae_history_step const *step_back(struct ae_history_player const *player, int back)
{
	if (back < 0 || back >= player->step_count)
		return NULL;
	return &player->steps[(player->step_next - 1 - back + 2 * AE_HISTORY_SAMPLES) % AE_HISTORY_SAMPLES];
}

/* ---------- public code: records */

static void player_clear(struct ae_history_player *state)
{
	int index;

	memset(state, 0, sizeof(*state));
	for (index = 0; index < AE_HISTORY_PICKUPS; index++)
		state->pickups[index].weapon = NO_WEAPON;
}

void ae_history_reset(struct ae_history *history)
{
	int index;

	memset(history, 0, sizeof(*history));
	for (index = 0; index < AE_MEDAL_RULES_PLAYERS; index++)
		player_clear(&history->players[index]);
	for (index = 0; index < AE_HISTORY_SPAWNED; index++)
		history->spawned[index] = NO_WEAPON;
}

void ae_history_clear_player(struct ae_history *history, int player)
{
	struct ae_history_player *state = player_of(history, player);

	if (state)
		player_clear(state);
}

void ae_history_shot(struct ae_history *history, int player, long tick, long weapon, int role, int zoomed, int pellets)
{
	struct ae_history_player *state = player_of(history, player);
	struct ae_history_shot *shot;

	if (!state)
		return;
	shot = &state->shots[state->shot_next];
	shot->tick = tick;
	shot->weapon = weapon;
	shot->role = (short)role;
	shot->zoomed = (unsigned char)(zoomed != 0);
	shot->pellets = (unsigned char)(pellets < 0 ? 0 : pellets > 255 ? 255 : pellets);
	state->shot_next = (state->shot_next + 1) % AE_HISTORY_SHOTS;
	if (state->shot_count < AE_HISTORY_SHOTS)
		state->shot_count++;
}

void ae_history_hit(struct ae_history *history, int victim, struct ae_history_hit const *hit, float before_shield,
	float before_body)
{
	struct ae_history_player *state = player_of(history, victim);
	struct ae_history_life *life;

	if (!state || !hit)
		return;
	life = &state->life;
	if (life->count == 0 && !life->overflow)
	{
		life->start_shield = before_shield;
		life->start_body = before_body;
	}
	if (life->count >= AE_HISTORY_HITS)
	{
		life->overflow = 1;
		return;
	}
	life->hits[life->count++] = *hit;
}

void ae_history_life_end(struct ae_history *history, int victim)
{
	struct ae_history_player *state = player_of(history, victim);

	if (state)
		memset(&state->life, 0, sizeof(state->life));
}

void ae_history_step(struct ae_history *history, int player, long tick, float yaw, float x, float y, float z, int airborne)
{
	struct ae_history_player *state = player_of(history, player);
	struct ae_history_step *step;

	if (!state)
		return;
	step = &state->steps[state->step_next];
	step->tick = tick;
	step->yaw = yaw;
	step->x = x;
	step->y = y;
	step->z = z;
	step->airborne = (unsigned char)(airborne != 0);
	state->step_next = (state->step_next + 1) % AE_HISTORY_SAMPLES;
	if (state->step_count < AE_HISTORY_SAMPLES)
		state->step_count++;
}

void ae_history_steps_clear(struct ae_history *history, int player)
{
	struct ae_history_player *state = player_of(history, player);

	if (state)
	{
		state->step_count = 0;
		state->step_next = 0;
	}
}

void ae_history_spawned(struct ae_history *history, long weapon)
{
	history->spawned[history->spawned_next] = weapon;
	history->spawned_next = (history->spawned_next + 1) % AE_HISTORY_SPAWNED;
}

void ae_history_picked(struct ae_history *history, int player, long weapon, long tick)
{
	struct ae_history_player *state = player_of(history, player);
	int index;

	if (weapon == NO_WEAPON)
		return;
	/* (whoever takes it, it is the spawner's no more) */
	for (index = 0; index < AE_HISTORY_SPAWNED; index++)
	{
		if (history->spawned[index] == weapon)
		{
			history->spawned[index] = NO_WEAPON;
			if (state)
			{
				state->pickups[state->pickup_next].weapon = weapon;
				state->pickups[state->pickup_next].tick = tick;
				state->pickup_next = (state->pickup_next + 1) % AE_HISTORY_PICKUPS;
			}
			return;
		}
	}
}

/* ---------- public code: questions */

float ae_history_taken_by(struct ae_history const *history, int victim, int attacker)
{
	struct ae_history_player const *state = player_const(history, victim);
	float taken = 0.0f;
	int index;

	/* (a log that lost blows says nothing) */
	if (!state || state->life.overflow)
		return 0.0f;
	for (index = 0; index < state->life.count; index++)
		if (state->life.hits[index].attacker == attacker)
			taken += state->life.hits[index].taken;
	return taken;
}

static int share_enough(float taken, int percent)
{
	return taken >= (float)percent / 100.0f;
}

int ae_history_no_scope(struct ae_history const *history, int killer, long tick)
{
	struct ae_history_player const *state = player_const(history, killer);
	int back;

	if (!state)
		return 0;
	for (back = 0; back < state->shot_count; back++)
	{
		struct ae_history_shot const *shot = shot_back(state, back);

		if (shot->role != AE_MEDAL_ROLE_SNIPER || shot->tick > tick)
			continue;
		return !shot->zoomed && tick - shot->tick <= AE_HISTORY_NO_SCOPE_TICKS;
	}
	return 0;
}

int ae_history_quick_draw(struct ae_history const *history, int killer, int victim, long tick, long killing_tag,
	int killer_role, int category)
{
	struct ae_history_player const *state = player_const(history, victim);
	long other = -1;
	int found = 0, index;

	if (!state || state->life.overflow || !player_const(history, killer))
		return 0;
	if ((killer_role != AE_MEDAL_ROLE_PISTOL && killer_role != AE_MEDAL_ROLE_PLASMA_PISTOL) ||
		category == AE_MEDAL_CATEGORY_MELEE)
	{
		return 0;
	}
	if (!share_enough(ae_history_taken_by(history, victim, killer), AE_HISTORY_KILLER_PERCENT))
		return 0;
	for (index = 0; index < state->life.count; index++)
	{
		struct ae_history_hit const *hit = &state->life.hits[index];

		if (hit->attacker == killer && hit->tag != killing_tag && hit->category != AE_MEDAL_CATEGORY_MELEE &&
			hit->tick <= tick && (!found || hit->tick > other))
		{
			other = hit->tick;
			found = 1;
		}
	}
	return found && tick - other <= AE_HISTORY_QUICK_DRAW_TICKS;
}

int ae_history_reversal(struct ae_history const *history, int killer, int victim)
{
	struct ae_history_player const *on_victim = player_const(history, victim);
	struct ae_history_player const *on_killer = player_const(history, killer);
	long killer_first = 0, victim_first = 0;
	int have_killer = 0, have_victim = 0, index;
	float victim_done = 0.0f;

	if (!on_victim || !on_killer || killer == victim || on_victim->life.overflow || on_killer->life.overflow)
		return 0;
	if (!share_enough(ae_history_taken_by(history, victim, killer), AE_HISTORY_KILLER_PERCENT))
		return 0;
	for (index = 0; index < on_victim->life.count; index++)
	{
		struct ae_history_hit const *hit = &on_victim->life.hits[index];

		if (hit->attacker == killer && (!have_killer || hit->tick < killer_first))
		{
			killer_first = hit->tick;
			have_killer = 1;
		}
	}
	for (index = 0; index < on_killer->life.count; index++)
	{
		struct ae_history_hit const *hit = &on_killer->life.hits[index];

		if (hit->attacker == victim && (!have_victim || hit->tick < victim_first))
		{
			victim_first = hit->tick;
			have_victim = 1;
		}
	}
	if (!have_killer || !have_victim)
		return 0;
	/* the victim struck first, and not long before */
	if (victim_first > killer_first || killer_first - victim_first > AE_HISTORY_REVERSAL_TICKS)
		return 0;
	for (index = 0; index < on_killer->life.count; index++)
	{
		struct ae_history_hit const *hit = &on_killer->life.hits[index];

		if (hit->attacker == victim && hit->tick < killer_first)
			victim_done += hit->taken;
	}
	return share_enough(victim_done, AE_HISTORY_REVERSAL_PERCENT);
}

int ae_history_nade_shot(struct ae_history const *history, int killer, int victim, int headshot)
{
	struct ae_history_player const *state = player_const(history, victim);
	long grenade_last = 0, gun_first = 0, gun_tag = 0;
	int grenades = 0, guns = 0, index;

	if (!state || state->life.overflow || !player_const(history, killer) || !headshot)
		return 0;
	if (!share_enough(ae_history_taken_by(history, victim, killer), AE_HISTORY_KILLER_PERCENT))
		return 0;
	for (index = 0; index < state->life.count; index++)
	{
		struct ae_history_hit const *hit = &state->life.hits[index];

		if (hit->attacker != killer)
			continue;
		if (hit->category == AE_MEDAL_CATEGORY_MELEE)
			return 0;
		if (is_grenade(hit))
		{
			if (!grenades || hit->tick > grenade_last)
				grenade_last = hit->tick;
			grenades++;
		}
		else
		{
			if (guns && hit->tag != gun_tag)
				return 0;
			if (!guns || hit->tick < gun_first)
				gun_first = hit->tick;
			gun_tag = hit->tag;
			guns++;
		}
	}
	/* the grenade's damage ends before the gun's begins, within 2 s */
	return grenades > 0 && guns > 0 && grenade_last < gun_first && gun_first - grenade_last <= AE_HISTORY_NADE_GAP_TICKS;
}

int ae_history_turned(struct ae_history *history, int player, long tick)
{
	struct ae_history_player *state = player_of(history, player);
	float total = 0.0f;
	int back, have_previous = 0;
	float previous = 0.0f;
	long previous_tick = 0;

	if (!state)
		return 0;
	/* oldest first, inside the window */
	for (back = state->step_count - 1; back >= 0; back--)
	{
		struct ae_history_step const *step = step_back(state, back);

		if (step->tick < tick - AE_HISTORY_TURN_TICKS || step->tick > tick)
			continue;
		/* (a gap of more than a tick or two: the player was gone, and came back facing anywhere) */
		if (have_previous && step->tick - previous_tick <= 2)
			total += wrap_angle(step->yaw - previous);
		have_previous = 1;
		previous = step->yaw;
		previous_tick = step->tick;
	}
	if (fabsf(total) >= AE_HISTORY_TURN_DEGREES * PI_F / 180.0f)
	{
		/* (it counts once: the next turn starts from nothing) */
		state->step_count = 0;
		state->step_next = 0;
		return 1;
	}
	return 0;
}

int ae_history_ninja(struct ae_history const *history, int killer, long tick, float victim_x, float victim_y, float victim_z,
	float victim_forward_x, float victim_forward_y)
{
	struct ae_history_player const *state = player_const(history, killer);
	int back = 0, near_enough = 0;
	long landing;
	struct ae_history_step const *step;
	float side_squared = AE_HISTORY_NINJA_SIDE * AE_HISTORY_NINJA_SIDE;

	if (!state)
		return 0;
	/* the samples up to the kill (newest first) */
	while ((step = step_back(state, back)) != NULL && step->tick > tick)
		back++;
	if (!step || step->airborne)
		return 0;
	/* back on the ground: where it began (a gap in the samples, a death between, ends the reading) */
	landing = step->tick;
	while ((step = step_back(state, back + 1)) != NULL && !step->airborne)
	{
		if (landing - step->tick > 2)
			return 0;
		back++;
		landing = step->tick;
	}
	if (step && landing - step->tick > 2)
		return 0;
	if (!step || tick - landing > AE_HISTORY_NINJA_LANDED_TICKS)
		return 0;
	/* the leap, newest to oldest: over the victim at some sample */
	back++;
	{
		long newer = landing;

		while ((step = step_back(state, back)) != NULL && step->airborne)
		{
			float height = step->z - victim_z;
			float dx = step->x - victim_x, dy = step->y - victim_y;

			if (newer - step->tick > 2)
				return 0;
			if (height > 0.0f && height < AE_HISTORY_NINJA_HEIGHT && dx * dx + dy * dy < side_squared)
				near_enough = 1;
			newer = step->tick;
			back++;
		}
		if (step && newer - step->tick > 2)
			return 0;
	}
	/* the sample before the leap: in front of the victim, about their height */
	if (!step || !near_enough)
		return 0;
	{
		float dx = step->x - victim_x, dy = step->y - victim_y;

		return dx * victim_forward_x + dy * victim_forward_y > 0.0f && fabsf(step->z - victim_z) < AE_HISTORY_NINJA_HEIGHT;
	}
}

int ae_history_off_the_rack(struct ae_history const *history, int killer, long weapon, long tick)
{
	struct ae_history_player const *state = player_const(history, killer);
	int index;

	if (!state)
		return 0;
	for (index = 0; index < AE_HISTORY_PICKUPS; index++)
	{
		struct ae_history_pickup const *pickup = &state->pickups[index];

		if (weapon != NO_WEAPON && pickup->weapon == weapon && tick >= pickup->tick &&
			tick - pickup->tick <= AE_HISTORY_OFF_THE_RACK_TICKS)
		{
			return 1;
		}
	}
	return 0;
}

/* ---------- PERFECT */

int ae_perfect_required_hits(struct ae_perfect_damage const *damage)
{
	float shield_damage, normalized, inverse, actual, vitality = 1.0f;
	int round;

	if (!damage || !isfinite(damage->damage) || !isfinite(damage->shield_max) || !isfinite(damage->shield_multiplier) ||
		!isfinite(damage->shield_modifier) || !isfinite(damage->leak) || !isfinite(damage->body_multiplier))
	{
		return 0;
	}
	if (damage->damage <= 0.0f || damage->shield_max <= 0.0f || damage->body_multiplier <= 0.0f ||
		damage->shield_multiplier <= 0.0f || damage->shield_modifier <= 0.0f)
	{
		return 0;
	}
	/* (the game's own steps, in single floats: object_damage_shield) */
	shield_damage = (1.0f - damage->leak) * damage->damage;
	if (damage->damage - shield_damage > 0.0f)
		return 0;
	actual = damage->shield_multiplier * shield_damage;
	actual *= damage->shield_modifier;
	inverse = 1.0f / damage->shield_max;
	normalized = actual * inverse;
	for (round = 1; round <= 64; round++)
	{
		if (vitality > 0.0f)
		{
			if (normalized > vitality)
			{
				float excess = actual - damage->shield_max * vitality;

				if (excess > 0.0f)
					return round;
				vitality = 0.0f;
			}
			else
				vitality -= normalized;
		}
		else
			return round;
	}
	return 0;
}

int ae_history_perfect(struct ae_history const *history, struct ae_perfect const *perfect, int *hits_out, int *shots_out)
{
	struct ae_history_player const *killer, *victim;
	struct ae_history_hit const *killer_hits[AE_HISTORY_HITS];
	struct ae_history_shot const *shots[AE_HISTORY_SHOTS];
	unsigned char matched[AE_HISTORY_SHOTS];
	float chip = 0.0f;
	long tag = 0, first_tick = 0, limit;
	int hits = 0, shot_count = 0, index, back, last_matched = -1, result = 0;

	if (hits_out)
		*hits_out = 0;
	if (shots_out)
		*shots_out = 0;
	if (!perfect || perfect->required <= 0 || perfect->fastest_ticks < 0 || !perfect->multiplier_ok ||
		!perfect->head_kill || perfect->sniper || perfect->killer == perfect->victim)
	{
		return 0;
	}
	killer = player_const(history, perfect->killer);
	victim = player_const(history, perfect->victim);
	if (!killer || !victim || victim->life.count <= 0 || victim->life.overflow)
		return 0;
	/* the victim began at exactly full shield and body (a charged shield is more than full) */
	if (victim->life.start_shield != 1.0f || victim->life.start_body != 1.0f)
		return 0;
	for (index = 0; index < victim->life.count; index++)
	{
		struct ae_history_hit const *hit = &victim->life.hits[index];

		if (hit->attacker != perfect->killer)
		{
			chip += hit->taken;
			continue;
		}
		if (hit->category == AE_MEDAL_CATEGORY_SNIPER || (hits > 0 && hit->tag != tag))
			return 0;
		if (hits == 0)
		{
			tag = hit->tag;
			first_tick = hit->tick;
		}
		killer_hits[hits++] = hit;
	}
	if (hits_out)
		*hits_out = hits;
	/* other hands' damage under a tenth of the whole is forgiven */
	if (chip >= (float)AE_HISTORY_PERFECT_CHIP_PERCENT / 100.0f)
		return 0;
	if (hits != perfect->required)
		return 0;
	/* the last blow of the life is the killer's, to the head */
	if (victim->life.hits[victim->life.count - 1].attacker != perfect->killer ||
		!victim->life.hits[victim->life.count - 1].head)
	{
		return 0;
	}
	/* in time: the fastest time, half again and half a second */
	limit = (perfect->fastest_ticks * 3 + 1) / 2 + AE_HISTORY_PERFECT_TIME_EXTRA_TICKS;
	if (perfect->tick - first_tick > limit)
		return 0;
	/* the rounds from a little before the first damage, oldest first */
	for (back = killer->shot_count - 1; back >= 0; back--)
	{
		struct ae_history_shot const *shot = shot_back(killer, back);

		if (shot->tick < first_tick - AE_HISTORY_PERFECT_BEFORE_TICKS || shot->tick > perfect->tick)
			continue;
		if (shot->pellets != 1 || shot->weapon != perfect->weapon)
			return 0;
		shots[shot_count++] = shot;
	}
	if (shots_out)
		*shots_out = shot_count;
	/* each hit came from a round fired a little before it (the earliest left); the rounds before the last that hit and
	not matched were misses. Rounds after it were fired after the killing one */
	memset(matched, 0, sizeof(matched));
	for (index = 0; index < hits; index++)
	{
		int found = -1;

		for (back = 0; back < shot_count; back++)
		{
			if (!matched[back] && shots[back]->tick <= killer_hits[index]->tick &&
				killer_hits[index]->tick - shots[back]->tick <= perfect->flight_ticks)
			{
				found = back;
				break;
			}
		}
		if (found < 0)
			return 0;
		matched[found] = 1;
		if (found > last_matched)
			last_matched = found;
	}
	result = 1;
	for (back = 0; back < last_matched; back++)
		if (!matched[back])
			result = 0;
	return result;
}
