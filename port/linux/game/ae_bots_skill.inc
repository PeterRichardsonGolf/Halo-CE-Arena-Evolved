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
of strafe demons ... they can't track like me"; then "less strafing on recruit"): its aim never settles: it
wanders between 1.5 and 8 degrees off the target, so it fires and misses as
a new player does; and it sidesteps barely (0.15 as hard), three times as
long each way, so a young player's aim can stay on it. RECRUIT and EASY also aim where
their target was a moment ago (the TRACKING DELAY below).
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
0.15 (0.33 was still too hard to track, owner 10-16) */
static real ae_bots_strafe(short skill)
{
	return skill == AE_BOTS_SKILL_RECRUIT ? 0.15f : 1.0f;
}

/* how long a sidestep lasts before it may turn, ticks (bots.c: a third of a
second to a second and a third): RECRUIT three times as long */
static long ae_bots_strafe_ticks(short skill, long ticks)
{
	return skill == AE_BOTS_SKILL_RECRUIT ? ticks * 3 : ticks;
}

/* TRACKING DELAY (owner 2026-10-16, from Infinite's bots as researched: "the over-correction window players win
strafe fights with"): RECRUIT and EASY aim at where the target was a moment ago, not where it is, so a target that
strafes or turns back is missed for that moment, as by a person. Each target's centre is kept for the last
AE_BOTS_TRACK_TICKS ticks, by its player's absolute index. */
#define AE_BOTS_TRACK_TICKS 16
#define AE_BOTS_TRACK_PLAYERS 256

static struct { long tick; real_point3d centre; } ae_bots_track[AE_BOTS_TRACK_PLAYERS][AE_BOTS_TRACK_TICKS];

/* the skill's delay, ticks: RECRUIT 0.3 s, EASY 0.2 s, none above */
static long ae_bots_track_delay_ticks(short skill)
{
	return skill == AE_BOTS_SKILL_RECRUIT ? 9 : skill == _bots_skill_easy ? 6 : 0;
}

/* aim_point (the target's centre now) becomes its centre the skill's delay ago, when that tick was kept */
static void ae_bots_aim_delay(short skill, long target_player_index, real_point3d *aim_point, long now)
{
	long delay = ae_bots_track_delay_ticks(skill);
	long player = target_player_index == NONE ? -1 : DATUM_INDEX_TO_ABSOLUTE_INDEX(target_player_index);
	long slot;

	if (player < 0 || player >= AE_BOTS_TRACK_PLAYERS || now < 0)
		return;
	slot = now % AE_BOTS_TRACK_TICKS;
	ae_bots_track[player][slot].tick = now;
	ae_bots_track[player][slot].centre = *aim_point;
	if (delay <= 0 || delay >= AE_BOTS_TRACK_TICKS)
		return;
	slot = (now - delay) % AE_BOTS_TRACK_TICKS;
	if (now - delay >= 0 && ae_bots_track[player][slot].tick == now - delay)
		*aim_point = ae_bots_track[player][slot].centre;
}

/* MELEE (owner 2026-10-16, Infinite's lower levels rarely melee): bots.c strikes within 1.1 units once a second at
every skill; RECRUIT only at point blank (0.8) and every 3 s, EASY every 2 s */
static real ae_bots_melee_reach(short skill)
{
	return skill == AE_BOTS_SKILL_RECRUIT ? 0.8f : 1.1f;
}

static long ae_bots_melee_ticks(short skill)
{
	return skill == AE_BOTS_SKILL_RECRUIT ? TICKS_PER_SECOND * 3 : skill == _bots_skill_easy ? TICKS_PER_SECOND * 2 :
		TICKS_PER_SECOND;
}
