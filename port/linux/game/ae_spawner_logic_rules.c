/*
AE_SPAWNER_LOGIC_RULES.C

The state machine of one spawner (ae_spawner_logic_rules.h).
*/

#include "ae_spawner_logic_rules.h"

static long floored(long period_ticks)
{
	long floor_ticks = AE_SPAWNER_RESPAWN_FLOOR_SECONDS * AE_SPAWNER_TICKS_PER_SECOND;

	return period_ticks < floor_ticks ? floor_ticks : period_ticks;
}

void ae_spawner_logic_begin(struct ae_spawner_machine *s, int logic, long period_ticks, long delay_ticks)
{
	s->logic = (signed char)(logic >= 0 && logic < AE_SPAWNER_LOGIC_COUNT ? logic : AE_SPAWNER_STATIC);
	s->period = floored(period_ticks);
	s->delay = delay_ticks < 0 ? 0 : delay_ticks;
	s->rules = 0;
	ae_spawner_logic_reset(s);
}

void ae_spawner_logic_reset(struct ae_spawner_machine *s)
{
	s->state = AE_SPAWNER_WAITING;
	s->has_object = FALSE;
	s->taken = FALSE;
	/* (a static spot's clock is its beat: no spawn asked for) */
	s->next_at = s->logic == AE_SPAWNER_TRIGGER || s->logic == AE_SPAWNER_STATIC ? -1 : s->delay;
	s->made_at = -1;
	s->taken_at = -1;
	s->skipped_at = -1;
}

void ae_spawner_logic_static_taken(struct ae_spawner_machine *s, long now)
{
	if (s->logic == AE_SPAWNER_STATIC)
		s->taken_at = now;
}

int ae_spawner_out_of_play(int exists, int held, int empty, int reference)
{
	if (!exists) return 1;
	if (!empty) return 0;
	return reference || !held;
}

int ae_spawner_shown_red(int red, int power, int reference)
{
	return red && !(power && reference);
}

/* a static spawner's beat at `beat` is skipped: its weapon was taken less than the skip window before it */
static boolean static_skips(struct ae_spawner_machine const *s, long beat)
{
	return (s->rules & AE_SPAWNER_RULE_STATIC_SKIP) && s->taken_at >= 0 && beat >= s->taken_at &&
		beat - s->taken_at < AE_SPAWNER_STATIC_SKIP_TICKS;
}

void ae_spawner_logic_fire(struct ae_spawner_machine *s)
{
	if (s->logic != AE_SPAWNER_TRIGGER) return;
	s->next_at = 0;
	s->has_object = FALSE;
	s->taken = FALSE;
}

void ae_spawner_logic_made(struct ae_spawner_machine *s, long now, boolean object_made)
{
	s->made_at = now;
	if (s->logic == AE_SPAWNER_STATIC)
	{
		s->has_object = FALSE;
		s->state = AE_SPAWNER_WAITING;
		return;
	}
	s->taken = FALSE;
	if (object_made)
	{
		s->has_object = TRUE;
		s->state = AE_SPAWNER_READY;
		s->next_at = -1;
	}
	else
	{
		s->has_object = FALSE;
		s->state = AE_SPAWNER_WAITING;
		s->next_at = s->logic == AE_SPAWNER_TRIGGER ? -1 : now + s->period;
	}
}

int ae_spawner_logic_step(struct ae_spawner_machine *s, long now, struct ae_spawner_sight const *sight)
{
	if (s->state == AE_SPAWNER_DORMANT) return AE_SPAWNER_DO_NOTHING;
	if (s->logic == AE_SPAWNER_STATIC)
	{
		s->state = AE_SPAWNER_WAITING;
		/* (a spawn asked for once: a round's reset) */
		if (s->next_at >= 0 && now >= s->next_at)
		{
			s->next_at = -1;
			return AE_SPAWNER_DO_SPAWN;
		}
		if (now >= s->delay && (now - s->delay) % s->period == 0)
		{
			if (static_skips(s, now))
			{
				s->skipped_at = now;
				return AE_SPAWNER_DO_LOG_SKIPPED;
			}
			return AE_SPAWNER_DO_SPAWN;
		}
		return AE_SPAWNER_DO_NOTHING;
	}
	if (!s->has_object)
	{
		if (s->next_at >= 0 && now >= s->next_at)
		{
			s->next_at = -1;
			return AE_SPAWNER_DO_SPAWN;
		}
		s->state = AE_SPAWNER_WAITING;
		return AE_SPAWNER_DO_NOTHING;
	}
	if (!s->taken)
	{
		if (!sight->exists)
		{
			/* gone from its spot without a pickup: purged, destroyed, its ammo walked off (a trigger's next: its beat) */
			s->has_object = FALSE;
			s->next_at = s->logic == AE_SPAWNER_TRIGGER ? -1 : now + s->period;
			s->state = AE_SPAWNER_WAITING;
			return AE_SPAWNER_DO_LOG_LOST;
		}
		if (sight->held)
		{
			s->taken = TRUE;
			if (s->logic == AE_SPAWNER_EXPIRED)
			{
				s->state = AE_SPAWNER_RED;
				return AE_SPAWNER_DO_LOG_TAKEN;
			}
			/* pickup, or a trigger spawner's weapon: the next one in a period (a trigger's on its beat) */
			s->has_object = FALSE;
			s->next_at = s->logic == AE_SPAWNER_TRIGGER ? -1 : now + s->period;
			s->state = AE_SPAWNER_WAITING;
			return AE_SPAWNER_DO_LOG_TAKEN;
		}
		s->state = AE_SPAWNER_READY;
		return AE_SPAWNER_DO_KEEP;
	}
	/* expired: taken, red while the weapon is in play */
	if (sight->out_of_play || !sight->exists)
	{
		s->has_object = FALSE;
		s->taken = FALSE;
		s->next_at = now + s->period;
		s->state = AE_SPAWNER_WAITING;
		return AE_SPAWNER_DO_LOG_EXPIRED;
	}
	s->state = AE_SPAWNER_RED;
	return AE_SPAWNER_DO_NOTHING;
}

void ae_spawner_logic_retime(struct ae_spawner_machine *s, int logic, long period_ticks, long delay_ticks, long now)
{
	long old_period = s->period;

	s->logic = (signed char)(logic >= 0 && logic < AE_SPAWNER_LOGIC_COUNT ? logic : AE_SPAWNER_STATIC);
	s->period = floored(period_ticks);
	s->delay = delay_ticks < 0 ? 0 : delay_ticks;
	if (s->logic == AE_SPAWNER_STATIC)
	{
		/* the clock: a phase so the next beat is at most a period away, counting from now */
		s->has_object = FALSE;
		s->taken = FALSE;
		s->state = AE_SPAWNER_WAITING;
		s->next_at = -1;
		if (now >= s->delay)
			s->delay = now + (s->delay - now) % s->period;
		return;
	}
	if (s->logic == AE_SPAWNER_TRIGGER)
	{
		if (!s->has_object) s->next_at = -1;
		return;
	}
	if (!s->has_object)
	{
		/* a running count keeps what is left, capped to the new period; a count that was never started starts now */
		long left = s->next_at < 0 ? 0 : s->next_at - now;

		if (left < 0) left = 0;
		if (left > s->period) left = s->period;
		s->next_at = now + left;
	}
	else if (s->taken && s->logic == AE_SPAWNER_PICKUP)
	{
		/* was RED under expired: under pickup the count runs from now */
		s->has_object = FALSE;
		s->taken = FALSE;
		s->next_at = now + s->period;
		s->state = AE_SPAWNER_WAITING;
	}
	(void)old_period;
}

long ae_spawner_logic_ticks_left(struct ae_spawner_machine const *s, long now)
{
	long left;

	if (s->logic == AE_SPAWNER_STATIC)
	{
		if (now < s->delay) left = s->delay - now;
		else left = s->period - (now - s->delay) % s->period;
		/* (a beat that will be skipped: the one after it) */
		if (static_skips(s, now + left))
			left += s->period;
	}
	else if (s->state == AE_SPAWNER_WAITING && !s->has_object && s->next_at > now)
		left = s->next_at - now;
	else
		left = s->period;
	if (left > s->period) left = s->period;
	if (left < 1) left = 1;
	return left;
}

boolean ae_spawner_logic_on_spot(struct ae_spawner_machine const *s, long now)
{
	if (s->logic == AE_SPAWNER_STATIC)
	{
		long beat;

		if (now < s->delay)
			return FALSE;
		beat = now - (now - s->delay) % s->period;
		return now - beat < AE_SPAWNER_STATIC_ON_SPOT_TICKS && beat != s->skipped_at;
	}
	return s->state == AE_SPAWNER_READY;
}

void ae_spawner_logic_restart(struct ae_spawner_machine *s, long now)
{
	ae_spawner_logic_reset(s);
	/* (a static spot spawns now, as Halo CE's item loop does on a round's reset, and keeps its beat) */
	if (s->logic == AE_SPAWNER_STATIC)
		s->next_at = now;
	else if (s->logic != AE_SPAWNER_TRIGGER)
		s->next_at = now + s->delay;
}
