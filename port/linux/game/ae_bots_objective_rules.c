/*
AE_BOTS_OBJECTIVE_RULES.C

The bots' objective choice (ae_bots_objective_rules.h): pure numbers, the
curves tuned by box games (the objective report's log numbers).
*/

#include "ae_bots_objective_rules.h"

/* how much a goal's nearness counts: 1 at it, a half at `scale` away, 0 for a goal not there */
static float near(float distance, float scale)
{
	if (distance < 0.0f)
		return 0.0f;
	return 1.0f / (1.0f + distance / scale);
}

static float clamp01(float value)
{
	return value < 0.0f ? 0.0f : value > 1.0f ? 1.0f : value;
}

void ae_bots_objective_utilities(struct ae_bots_situation const *s, float utility[AE_BOTS_BEHAVIOURS])
{
	float hurt = 1.0f - clamp01(s->health);
	int index;

	for (index = 0; index < AE_BOTS_BEHAVIOURS; index++)
		utility[index] = 0.0f;
	/* a fight in sight: the nearer, the more it matters (0.85 at arm's length, 0.65 at 8 units, 0.56 at 20); none
	in sight: bots.c's hunting and roaming, whatever the mode has nothing better */
	utility[AE_BOTS_FIGHT] = s->fighting ? 0.45f + 0.4f * near(s->enemy_distance < 0.0f ? 20.0f : s->enemy_distance, 8.0f) :
		0.1f;

	switch (s->mode)
	{
	case AE_BOTS_MODE_CTF:
		if (s->carrying)
		{
			/* (home, whatever it meets or however hurt) */
			utility[AE_BOTS_CTF_CARRY] = 0.97f;
			break;
		}
		if (s->own_flag_away >= 0.0f)
		{
			/* the own flag dropped: touch it; carried: after its carrier */
			utility[AE_BOTS_CTF_RETURN] = (s->own_flag_carried ? 0.62f : 0.72f) + 0.2f * near(s->own_flag_away, 15.0f) +
				(s->defender ? 0.08f : 0.0f);
		}
		if (s->enemy_flag >= 0.0f)
		{
			utility[AE_BOTS_CTF_TAKE] = s->defender ? 0.25f + 0.5f * near(s->enemy_flag, 3.0f) :
				0.6f + 0.3f * near(s->enemy_flag, 10.0f) - 0.15f * hurt;
		}
		if (s->own_stand >= 0.0f)
			utility[AE_BOTS_CTF_DEFEND] = s->defender ? 0.64f : 0.15f;
		if (s->ally_carrier >= 0.0f)
			utility[AE_BOTS_ESCORT] = s->defender ? 0.3f : 0.55f + 0.1f * near(s->ally_carrier, 10.0f);
		break;
	case AE_BOTS_MODE_ODDBALL:
		if (s->carrying)
		{
			utility[AE_BOTS_BALL_HOLD] = 0.95f;
			break;
		}
		if (s->ball >= 0.0f)
			utility[AE_BOTS_BALL_TAKE] = 0.7f + 0.2f * near(s->ball, 10.0f) - 0.1f * hurt;
		if (s->enemy_carrier >= 0.0f)
			utility[AE_BOTS_BALL_CHASE] = 0.6f + 0.15f * near(s->enemy_carrier, 10.0f);
		if (s->ally_carrier >= 0.0f)
			utility[AE_BOTS_ESCORT] = 0.45f;
		break;
	case AE_BOTS_MODE_TAG:
		/* (the one who is "it" scores while it lives: away from the others; they go for it) */
		if (s->carrying)
		{
			utility[AE_BOTS_BALL_HOLD] = 0.9f;
			break;
		}
		if (s->enemy_carrier >= 0.0f)
			utility[AE_BOTS_BALL_CHASE] = 0.68f + 0.15f * near(s->enemy_carrier, 10.0f);
		if (s->ally_carrier >= 0.0f)
			utility[AE_BOTS_ESCORT] = 0.45f;
		break;
	case AE_BOTS_MODE_JUGGERNAUT:
		/* (the juggernaut scores by kills: it fights and hunts as bots.c does, and breaks away badly hurt) */
		if (s->carrying)
		{
			if (s->health < 0.4f)
				utility[AE_BOTS_JUGG_FLEE] = 0.75f + 0.2f * (0.4f - clamp01(s->health)) / 0.4f;
			break;
		}
		if (s->enemy_carrier >= 0.0f)
			utility[AE_BOTS_JUGG_HUNT] = 0.62f + 0.15f * near(s->enemy_carrier, 10.0f);
		if (s->ally_carrier >= 0.0f)
			utility[AE_BOTS_ESCORT] = 0.4f;
		break;
	case AE_BOTS_MODE_KING:
		if (s->in_hill)
			utility[AE_BOTS_HILL_HOLD] = 0.9f;
		else if (s->hill >= 0.0f)
			utility[AE_BOTS_HILL_GO] = 0.66f + 0.2f * near(s->hill, 10.0f) - 0.1f * hurt;
		break;
	case AE_BOTS_MODE_RACE:
		if (s->race_flag >= 0.0f)
			utility[AE_BOTS_RACE_NEXT] = 0.92f;
		break;
	default:
		break;
	}
	for (index = 0; index < AE_BOTS_BEHAVIOURS; index++)
		utility[index] = clamp01(utility[index]);
}

int ae_bots_objective_pick(float const utility[AE_BOTS_BEHAVIOURS], int current, long held_ticks,
	long hysteresis_ticks, float bonus)
{
	int best = AE_BOTS_FIGHT;
	float best_value = -1.0f;
	int index;

	if (current < 0 || current >= AE_BOTS_BEHAVIOURS || utility[current] <= 0.0f)
		current = -1;
	for (index = 0; index < AE_BOTS_BEHAVIOURS; index++)
	{
		float value = utility[index];

		if (index == current && held_ticks < hysteresis_ticks)
			value += bonus;
		/* (a tie: the current one, else the first) */
		if (value > best_value || (value == best_value && index == current))
		{
			best_value = value;
			best = index;
		}
	}
	return best;
}

int ae_bots_ctf_defenders(int team_size)
{
	return team_size < 3 ? 0 : team_size < 6 ? 1 : 2;
}

int ae_bots_objective_runs_while_fighting(int behaviour)
{
	switch (behaviour)
	{
	case AE_BOTS_CTF_TAKE:
	case AE_BOTS_CTF_CARRY:
	case AE_BOTS_BALL_TAKE:
	case AE_BOTS_BALL_HOLD:
	case AE_BOTS_HILL_GO:
	case AE_BOTS_HILL_HOLD:
	case AE_BOTS_JUGG_FLEE:
	case AE_BOTS_RACE_NEXT:
		return 1;
	default:
		return 0;
	}
}

char const *ae_bots_behaviour_name(int behaviour)
{
	static char const *const names[AE_BOTS_BEHAVIOURS] =
	{
		"fight", "take", "carry", "return", "defend", "escort", "ball", "hold", "chase", "hill", "king", "hunt", "flee",
		"race",
	};

	return behaviour >= 0 && behaviour < AE_BOTS_BEHAVIOURS ? names[behaviour] : "?";
}

char const *ae_bots_mode_name(int mode)
{
	static char const *const names[AE_BOTS_MODES] =
	{
		"slayer", "ctf", "oddball", "reverse tag", "juggernaut", "king", "race",
	};

	return mode >= 0 && mode < AE_BOTS_MODES ? names[mode] : "?";
}
