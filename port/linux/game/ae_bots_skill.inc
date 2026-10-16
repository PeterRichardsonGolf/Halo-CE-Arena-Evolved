/*
AE_BOTS_SKILL.INC

Included in bots.c right after its skill table (one AE hook line), so that
Arena Evolved's RECRUIT level sits below EASY without a fifth row in the
table of OpenCE pull request #367: bots.skill = "recruit". For new players
and children (owner 2026-10-15: EASY "will clap my poor kids"): it reacts
late, sees less, starts far off its target and settles slowly, turns slowly,
fires short bursts with long pauses, throws no grenades, does not jump, takes
no lead, and roams the map more often than it heads for the nearest enemy.
Owner 2026-10-15 after play ("these bots will still destroy my kids"; "less
of strafe demons ... they can't track like me"): its aim never settles: it
wanders between 1.5 and 8 degrees off the target, so it fires and misses as
a new player does; and it sidesteps a third as hard, for longer each way,
so a new player's aim can stay on it.
*/

#define AE_BOTS_SKILL_RECRUIT NUMBER_OF_BOTS_SKILLS

static struct bots_skill_definition const ae_bots_recruit =
	{ "recruit", 1.4f, 16.0f, 80.0f, 22.0f, 2.5f, 5.0f, 2.6f, 0.0f, 0.0f, 0.0f, 4, 24 };

/* the skill's definition: the table's, or RECRUIT's */
static struct bots_skill_definition const *ae_bots_skill_definition(short skill)
{
	return skill == AE_BOTS_SKILL_RECRUIT ? &ae_bots_recruit : &bots_skills[skill];
}

/* the chance a roam goes towards an enemy (the place nearest a living one)
rather than anywhere: bots.c's 0.65, RECRUIT's 0.2 */
static real ae_bots_hunt_chance(short skill)
{
	return skill == AE_BOTS_SKILL_RECRUIT ? 0.2f : 0.65f;
}

/* RECRUIT's aim: after bots.c's settling, a wander that never settles: a
random walk kept between 1.5 and 8 degrees off (pitch: six tenths of that);
r1, r2: two random numbers in -1..1 */
static void ae_bots_aim_floor(short skill, real *yaw_error, real *pitch_error, real r1, real r2)
{
	real const low = 1.5f * BOTS_DEGREES, high = 8.0f * BOTS_DEGREES, step = 0.6f * BOTS_DEGREES;
	real *errors[2];
	real scale[2] = { 1.0f, 0.6f };
	real random[2];
	short index;

	if (skill != AE_BOTS_SKILL_RECRUIT)
		return;
	errors[0] = yaw_error;
	errors[1] = pitch_error;
	random[0] = r1;
	random[1] = r2;
	for (index = 0; index < 2; index++)
	{
		real value = *errors[index] + random[index] * step * scale[index];
		real magnitude = (real)fabs(value);
		real sign = value < 0.0f ? -1.0f : 1.0f;

		if (magnitude < low * scale[index])
			value += sign * 0.5f * step * scale[index];
		if (magnitude > high * scale[index])
			value = sign * high * scale[index];
		*errors[index] = value;
	}
}

/* how hard it sidesteps while fighting (bots.c: full, either way): RECRUIT
a third */
static real ae_bots_strafe(short skill)
{
	return skill == AE_BOTS_SKILL_RECRUIT ? 0.33f : 1.0f;
}

/* how long a sidestep lasts before it may turn, ticks (bots.c: a third of a
second to a second and a third): RECRUIT twice as long */
static long ae_bots_strafe_ticks(short skill, long ticks)
{
	return skill == AE_BOTS_SKILL_RECRUIT ? ticks * 2 : ticks;
}
