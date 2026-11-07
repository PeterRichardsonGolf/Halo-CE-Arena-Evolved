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
			/* (the attackers go home with the carrier: left to the flag's stand they count as holding a place and leave a vehicle) */
			utility[AE_BOTS_ESCORT] = s->defender ? 0.3f : s->wingman ? 0.9f : 0.45f;
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
			utility[AE_BOTS_ESCORT] = s->wingman ? 0.88f : 0.0f;
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
	case AE_BOTS_MODE_BOMB:
		if (s->carrying)
		{
			/* (to the enemy goal and in it until armed, whatever it meets or however hurt, as CTF's carrier) */
			if (s->enemy_goal >= 0.0f)
				utility[AE_BOTS_BOMB_CARRY] = 0.97f;
			break;
		}
		if (s->bomb_snd)
		{
			/* a planted bomb is the whole fight: one defender saves it while its teammates cover; the attackers
			guard it. Before the plant, one defender holds the site and the others meet the push. */
			if (s->bomb_planted_own >= 0.0f)
			{
				if (s->bomb_defuser)
					utility[AE_BOTS_BOMB_SAVE] = 0.98f;
				else
					utility[AE_BOTS_BOMB_DEFEND] = 0.88f;
				break;
			}
			if (s->bomb_planted_enemy >= 0.0f)
			{
				/* (the lurker does not guard the bomb: it hunts the defenders who come for it) */
				utility[AE_BOTS_BOMB_DEFEND] = s->snd_role == AE_BOTS_SND_ROLE_LURKER ? 0.0f : 0.9f;
				break;
			}
			if (s->bomb >= 0.0f && s->bomb_attacking)
				utility[AE_BOTS_BOMB_TAKE] = 0.6f + 0.3f * near(s->bomb, 10.0f) - 0.15f * hurt;
			if (s->enemy_carrier >= 0.0f)
			{
				utility[AE_BOTS_BALL_CHASE] = s->defender ? 0.45f + 0.35f * near(s->enemy_carrier, 6.0f) :
					0.6f + 0.2f * near(s->enemy_carrier, 10.0f);
			}
			if (!s->bomb_attacking && s->defender && s->own_goal >= 0.0f)
				utility[AE_BOTS_BOMB_DEFEND] = 0.9f;
			/* the forward defender holds its point down the lane, and leaves it for a carrier that comes near */
			else if (!s->bomb_attacking && s->snd_forward_point)
				utility[AE_BOTS_BOMB_DEFEND] = 0.7f;
			if (s->ally_carrier >= 0.0f && s->bomb_attacking)
				utility[AE_BOTS_ESCORT] = s->wingman ? 0.94f : 0.62f + 0.08f * near(s->ally_carrier, 10.0f);
			break;
		}
		/* the bomb planted in the own goal: onto it before it arms (it arms in a second or so: the near ones) */
		if (s->bomb_planted_own >= 0.0f)
			utility[AE_BOTS_BOMB_SAVE] = 0.7f + 0.28f * near(s->bomb_planted_own, 4.0f);
		if (s->bomb >= 0.0f)
		{
			utility[AE_BOTS_BOMB_TAKE] = s->defender ? 0.25f + 0.5f * near(s->bomb, 3.0f) :
				0.6f + 0.3f * near(s->bomb, 10.0f) - 0.15f * hurt;
		}
		/* the enemy carrier: hunted (its marker shows it to the whole team); a defender stays home unless it is near */
		if (s->enemy_carrier >= 0.0f)
		{
			utility[AE_BOTS_BALL_CHASE] = s->defender ? 0.45f + 0.35f * near(s->enemy_carrier, 6.0f) :
				0.6f + 0.2f * near(s->enemy_carrier, 10.0f);
		}
		if (s->own_goal >= 0.0f)
			utility[AE_BOTS_BOMB_DEFEND] = s->defender ? 0.64f : 0.15f;
		if (s->ally_carrier >= 0.0f)
			utility[AE_BOTS_ESCORT] = s->defender ? 0.3f : 0.55f + 0.1f * near(s->ally_carrier, 10.0f);
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

int ae_bots_ctf_defenders_with_human(int team_size)
{
	int defenders = ae_bots_ctf_defenders(team_size);

	return team_size <= 8 && defenders > 1 ? 1 : defenders;
}

int ae_bots_objective_runs_while_fighting(int behaviour, int bomb_snd)
{
	if (bomb_snd && (behaviour == AE_BOTS_ESCORT || behaviour == AE_BOTS_BOMB_DEFEND))
		return 1;
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
	case AE_BOTS_BOMB_TAKE:
	case AE_BOTS_BOMB_CARRY:
	case AE_BOTS_BOMB_SAVE:
		return 1;
	default:
		return 0;
	}
}

int ae_bots_carrier_choice(struct ae_bots_carrier_situation const *s)
{
	float reach;

	if (s->dropped)
	{
		float settle = s->skill_level >= 3 ? 1.0f : 1.5f;

		return s->enemy_engaging || s->clear_seconds < settle ? AE_BOTS_CARRIER_WAIT : AE_BOTS_CARRIER_RECOVER;
	}
	if (!s->carrying || !s->gun_loaded || s->drop_cooldown_seconds > 0.0f ||
		(s->mode != AE_BOTS_MODE_ODDBALL && !(s->mode == AE_BOTS_MODE_BOMB && s->bomb_snd)))
	{
		return AE_BOTS_CARRIER_KEEP;
	}
	/* RECRUIT never gives up scoring time. EASY does it only after being hit at close quarters; the higher levels
	read a threatening enemy's line of sight out to progressively more of their gun's useful range. */
	if (s->skill_level <= 0 || !s->enemy_engaging)
		return AE_BOTS_CARRIER_KEEP;
	reach = s->gun_range * (s->skill_level == 1 ? 0.45f : s->skill_level == 2 ? 0.75f :
		s->skill_level == 3 ? 1.0f : s->skill_level == 4 ? 1.25f : 1.5f);
	if (s->enemy_distance < 0.0f || s->enemy_distance > reach || (s->skill_level == 1 && !s->under_fire))
		return AE_BOTS_CARRIER_KEEP;
	/* Finish a short plant, and do not throw away the last attacker's one way to win. */
	if (s->mode == AE_BOTS_MODE_BOMB && (s->last_alive || (s->goal_distance >= 0.0f && s->goal_distance <= 4.0f)))
		return AE_BOTS_CARRIER_KEEP;
	/* A nearby teammate can shoot for it (SEARCH AND DESTROY: one at the carrier's shoulder; farther, the carrier lays
	the bomb down and fights, then goes back for it). An Oddball holder already established in cover keeps scoring
	with an escort; otherwise the enemy must be nearer than the safety or objective the carrier is trying to reach. */
	if (s->teammate_distance >= 0.0f && s->teammate_distance <= (s->mode == AE_BOTS_MODE_BOMB ? 4.0f : 8.0f))
		return AE_BOTS_CARRIER_KEEP;
	if (s->mode == AE_BOTS_MODE_ODDBALL && s->in_cover && s->teammate_distance >= 0.0f)
		return AE_BOTS_CARRIER_KEEP;
	if (s->goal_distance >= 0.0f && s->enemy_distance >= s->goal_distance)
		return AE_BOTS_CARRIER_KEEP;
	if (s->cover_distance >= 0.0f && s->enemy_distance >= s->cover_distance)
		return AE_BOTS_CARRIER_KEEP;
	return AE_BOTS_CARRIER_DROP;
}

int ae_bots_wingman_pick(struct ae_bots_wingman_candidate const *candidates, int count, int current,
	float switch_margin)
{
	int index, nearest = -1;

	for (index = 0; index < count; index++)
	{
		if (!candidates[index].eligible)
			continue;
		if (nearest < 0 || candidates[index].distance < candidates[nearest].distance)
			nearest = index;
	}
	if (nearest < 0)
		return -1;
	if (current >= 0 && current < count && candidates[current].eligible &&
		candidates[current].distance <= candidates[nearest].distance + switch_margin)
	{
		return current;
	}
	return nearest;
}

int ae_bots_recovery_choice(struct ae_bots_recovery_situation const *s)
{
	if (!s->dropper_alive)
		return s->team_game && s->near_teammate ? AE_BOTS_RECOVERY_HANDOFF : AE_BOTS_RECOVERY_ABANDON_DEAD;
	if (s->remembered_free)
	{
		if (!s->goal_valid)
			return AE_BOTS_RECOVERY_ABANDON_NO_GOAL;
		return s->enemy_engaging || s->clear_seconds < 1.0f ? AE_BOTS_RECOVERY_WAIT : AE_BOTS_RECOVERY_GO;
	}
	if (s->replacement_free)
		return AE_BOTS_RECOVERY_RESET;
	if (s->objective_carried)
		return AE_BOTS_RECOVERY_ABANDON_TAKEN;
	return s->missing_seconds < 2.0f ? AE_BOTS_RECOVERY_WAIT : AE_BOTS_RECOVERY_ABANDON_GONE;
}

void ae_bots_snd_roles(int attacking, int size, unsigned long alive_mask, long round, int roles[])
{
	static int const attack[8] =
	{
		AE_BOTS_SND_ROLE_ENTRY, AE_BOTS_SND_ROLE_SUPPORT, AE_BOTS_SND_ROLE_LURKER, AE_BOTS_SND_ROLE_ENTRY,
		AE_BOTS_SND_ROLE_SUPPORT, AE_BOTS_SND_ROLE_ENTRY, AE_BOTS_SND_ROLE_LURKER, AE_BOTS_SND_ROLE_ENTRY,
	};
	static int const defend[8] =
	{
		AE_BOTS_SND_ROLE_ANCHOR, AE_BOTS_SND_ROLE_HOLDER, AE_BOTS_SND_ROLE_FORWARD, AE_BOTS_SND_ROLE_HOLDER,
		AE_BOTS_SND_ROLE_ANCHOR, AE_BOTS_SND_ROLE_FORWARD, AE_BOTS_SND_ROLE_HOLDER, AE_BOTS_SND_ROLE_HOLDER,
	};
	int const *table = attacking ? attack : defend;
	int slot, living = 0, anchor = 0, support = 0, first_other = -1, spare = -1;

	if (size > 8)
		size = 8;
	/* the table is dealt as if everyone lived, turned by the round: a death changes only that bot's role */
	for (slot = 0; slot < size; slot++)
	{
		int dealt = table[(slot + (int)(round < 0 ? 0 : round % size)) % size];

		roles[slot] = (alive_mask >> slot) & 1 ? dealt : AE_BOTS_SND_ROLE_NONE;
		living += roles[slot] != AE_BOTS_SND_ROLE_NONE;
	}
	for (slot = 0; slot < size; slot++)
	{
		if (roles[slot] == AE_BOTS_SND_ROLE_NONE)
			continue;
		/* few left: a forward player or a lurker falls in with the others */
		if (!attacking && roles[slot] == AE_BOTS_SND_ROLE_FORWARD && living < 3)
			roles[slot] = AE_BOTS_SND_ROLE_HOLDER;
		if (attacking && roles[slot] == AE_BOTS_SND_ROLE_LURKER && living < 3)
			roles[slot] = AE_BOTS_SND_ROLE_ENTRY;
	}
	for (slot = 0; slot < size; slot++)
	{
		anchor += roles[slot] == AE_BOTS_SND_ROLE_ANCHOR;
		support += roles[slot] == AE_BOTS_SND_ROLE_SUPPORT;
	}
	/* promotion: the first living holder (or forward player) anchors, the first living lurker (or entry) supports */
	if (!attacking && !anchor)
	{
		for (slot = 0; slot < size; slot++)
		{
			if (roles[slot] == AE_BOTS_SND_ROLE_HOLDER)
			{
				roles[slot] = AE_BOTS_SND_ROLE_ANCHOR;
				return;
			}
			if (roles[slot] == AE_BOTS_SND_ROLE_FORWARD && spare < 0)
				spare = slot;
		}
		if (spare >= 0)
			roles[spare] = AE_BOTS_SND_ROLE_ANCHOR;
	}
	if (attacking && !support && living >= 2)
	{
		for (slot = 0; slot < size; slot++)
		{
			if (roles[slot] == AE_BOTS_SND_ROLE_LURKER)
			{
				roles[slot] = AE_BOTS_SND_ROLE_SUPPORT;
				return;
			}
			if (roles[slot] == AE_BOTS_SND_ROLE_ENTRY)
				first_other = slot;
		}
		if (first_other >= 0)
			roles[first_other] = AE_BOTS_SND_ROLE_SUPPORT;
	}
}

char const *ae_bots_snd_role_name(int role)
{
	static char const *const names[AE_BOTS_SND_ROLES] =
	{
		"none", "carrier", "entry", "support", "lurker", "anchor", "holder", "forward",
	};

	return role >= 0 && role < AE_BOTS_SND_ROLES ? names[role] : "?";
}

float ae_bots_lane_score(float start_x, float start_y, float target_x, float target_y,
	float point_x, float point_y, int lane, float nearest_taken_squared)
{
	float tx = target_x - start_x, ty = target_y - start_y;
	float px = point_x - start_x, py = point_y - start_y;
	float length_squared = tx * tx + ty * ty;
	float along, cross, side, lateral_squared;

	if (length_squared < 64.0f || nearest_taken_squared < 9.0f)
		return -1.0f;
	along = (px * tx + py * ty) / length_squared;
	if (along < 0.08f || along > 0.72f)
		return -1.0f;
	cross = (tx * py - ty * px);
	lateral_squared = cross * cross / length_squared;
	if (lateral_squared < 2.25f || lateral_squared > 144.0f)
		return -1.0f;
	side = cross < 0.0f ? -1.0f : 1.0f;
	return along * 10.0f + (lane == 0 || side == (float)lane ? 4.0f : 0.0f) +
		(lateral_squared < 36.0f ? lateral_squared : 36.0f) / 18.0f;
}

char const *ae_bots_behaviour_name(int behaviour)
{
	static char const *const names[AE_BOTS_BEHAVIOURS] =
	{
		"fight", "take", "carry", "return", "defend", "escort", "ball", "hold", "chase", "hill", "king", "hunt", "flee",
		"race", "bomb", "plant", "guard", "save",
	};

	return behaviour >= 0 && behaviour < AE_BOTS_BEHAVIOURS ? names[behaviour] : "?";
}

char const *ae_bots_mode_name(int mode)
{
	static char const *const names[AE_BOTS_MODES] =
	{
		"slayer", "ctf", "oddball", "reverse tag", "juggernaut", "king", "race", "bomb",
	};

	return mode >= 0 && mode < AE_BOTS_MODES ? names[mode] : "?";
}
