#include <math.h>
#include <stdio.h>
#include "ae_ragdoll_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

/* a standing body, a column of 5 parts along z at x = y = 0 (feet, knee, hip, chest, head), equal masses */
static float body[5][3] = { { 0.f, 0.f, 0.f }, { 0.f, 0.f, 0.3f }, { 0.f, 0.f, 0.5f }, { 0.f, 0.f, 0.65f }, { 0.f, 0.f, 0.75f } };
static float masses[5] = { 1.f, 1.f, 1.f, 1.f, 1.f };

static void zero(float (*v)[3], int count)
{
	int i;

	for (i = 0; i < count; i++)
		v[i][0] = v[i][1] = v[i][2] = 0.f;
}

static void category_tests(void)
{
	float v[6][3];
	float shot[3] = { -1.f, 0.f, 0.f }; /* the shooter at +x, the shot travelling to -x */
	float chest[3] = { 0.f, 0.f, 0.66f }, knee[3] = { 0.f, 0.f, 0.31f };
	float arm[6][3];
	float arm_masses[6] = { 1.f, 1.f, 1.f, 1.f, 1.f, 1.f };
	float side[3] = { 0.f, 0.25f, 0.6f };
	float push[3] = { -3.f, 0.f, 0.f };
	int i;

	/* the push for a CE damage category (wu/s before the body's 1.2): bullets 0.2, sniper 0.25, melee 1.0,
	plasma 0.125, needles 0.05, shotgun pellets 0.1, mounted guns 0.2; none for the rest (explosions keep their tags') */
	CHECK(ae_ragdoll_category_push(AE_DAMAGE_CATEGORY_BULLET) == 0.2f);
	CHECK(ae_ragdoll_category_push(AE_DAMAGE_CATEGORY_SNIPER) == 0.25f);
	CHECK(ae_ragdoll_category_push(AE_DAMAGE_CATEGORY_MELEE) == 1.f);
	CHECK(ae_ragdoll_category_push(AE_DAMAGE_CATEGORY_PLASMA) == 0.125f);
	CHECK(ae_ragdoll_category_push(AE_DAMAGE_CATEGORY_NEEDLE) == 0.05f);
	CHECK(ae_ragdoll_category_push(AE_DAMAGE_CATEGORY_SHOTGUN) == 0.1f);
	CHECK(ae_ragdoll_category_push(AE_DAMAGE_CATEGORY_MOUNTED_WEAPON) == 0.2f);
	CHECK(ae_ragdoll_category_push(AE_DAMAGE_CATEGORY_GRENADE) == 0.f);
	CHECK(ae_ragdoll_category_push(AE_DAMAGE_CATEGORY_HIGH_EXPLOSIVE) == 0.f);
	CHECK(ae_ragdoll_category_push(AE_DAMAGE_CATEGORY_FALLING) == 0.f);
	CHECK(ae_ragdoll_category_push(-1) == 0.f);
	CHECK(ae_ragdoll_category_push(99) == 0.f);

	/* the kick, a chest shot from the front: the head and chest go back (with the shot), the feet forward (x 0.65),
	every part lifted, nothing turned about the up axis (no y speed: no pirouette) */
	zero(v, 5);
	CHECK(ae_ragdoll_level_kick(body, masses, 5, chest, shot, 1.f, 0.05f, v));
	CHECK(v[4][0] < 0.f && v[3][0] < 0.f);
	CHECK(v[0][0] > 0.f);
	for (i = 0; i < 5; i++)
	{
		CHECK(v[i][1] == 0.f);
		CHECK(fabsf(v[i][2] - 0.05f) < 1e-6f);
	}
	/* the farthest part on the hit's side gets the whole strength: the head, -1 (the chest, the part hit, x 2) */
	CHECK(fabsf(v[4][0] - -1.f) < 1e-5f);
	/* (owner 11-11, round-1111: "if they get shot from the back they move forward but fall back ward") a body shot below
	the body's mass middle but above the legs (a heavy top, its inverse masses a third: the middle at 0.556, the hit on the part at 0.5) is no leg
	shot: the head goes with the shot, the feet against it, so the body falls the way it was shot */
	{
		float heavy[5] = { 1.f, 1.f, 1.f, 1.f / 3.f, 1.f / 3.f }, belly[3] = { 0.f, 0.f, 0.49f };

		zero(v, 5);
		CHECK(ae_ragdoll_level_kick(body, heavy, 5, belly, shot, 1.f, 0.f, v));
		CHECK(v[4][0] < 0.f && v[0][0] > 0.f);
	}
	/* a leg shot: the legs go with the shot, the head comes forward (swept off the feet) */
	zero(v, 5);
	CHECK(ae_ragdoll_level_kick(body, masses, 5, knee, shot, 1.f, 0.f, v));
	CHECK(v[0][0] < 0.f && v[4][0] > 0.f);
	/* an arm shot out at the side: still a turn about a level axis (an along to the arm would twist it about up) */
	for (i = 0; i < 5; i++)
	{
		arm[i][0] = body[i][0];
		arm[i][1] = body[i][1];
		arm[i][2] = body[i][2];
	}
	arm[5][0] = 0.f;
	arm[5][1] = 0.25f;
	arm[5][2] = 0.6f;
	zero(v, 6);
	CHECK(ae_ragdoll_level_kick(arm, arm_masses, 6, side, shot, 1.f, 0.f, v));
	for (i = 0; i < 6; i++)
		CHECK(v[i][1] == 0.f);
	CHECK(v[4][0] < 0.f);
	/* a shot straight down the body (no level part): no turn, the lift only */
	{
		float down[3] = { 0.f, 0.f, -1.f };

		zero(v, 5);
		CHECK(!ae_ragdoll_level_kick(body, masses, 5, chest, down, 1.f, 0.05f, v));
		for (i = 0; i < 5; i++)
			CHECK(v[i][0] == 0.f && v[i][1] == 0.f && fabsf(v[i][2] - 0.05f) < 1e-6f);
	}

	/* the big blow's tumble: end over end about up x push (a level axis): the head goes with the push, the feet
	against it, no y speed */
	zero(v, 5);
	ae_ragdoll_tumble(body, masses, 5, push, 0.1f, v);
	CHECK(v[4][0] < 0.f && v[0][0] > 0.f);
	for (i = 0; i < 5; i++)
		CHECK(fabsf(v[i][1]) < 1e-6f);
	/* a straight-up push has no level axis: nothing */
	{
		float up[3] = { 0.f, 0.f, 3.f };

		zero(v, 5);
		ae_ragdoll_tumble(body, masses, 5, up, 0.1f, v);
		for (i = 0; i < 5; i++)
			CHECK(v[i][0] == 0.f && v[i][1] == 0.f && v[i][2] == 0.f);
	}

	/* a kill to the legs (MODERN): the knees give out. The knee is a leg hit, the chest and the belly are not */
	{
		float belly[3] = { 0.f, 0.f, 0.49f };

		CHECK(ae_ragdoll_legs_give_out(body, masses, 5, knee));
		CHECK(!ae_ragdoll_legs_give_out(body, masses, 5, chest));
		CHECK(!ae_ragdoll_legs_give_out(body, masses, 5, belly));
	}
	/* the drop: the feet stay, each part above falls by its share of the height, the head the most; level speeds untouched */
	zero(v, 5);
	CHECK(ae_ragdoll_leg_collapse(body, 5, 0.1f, v));
	CHECK(v[0][2] == 0.f);
	CHECK(fabsf(v[4][2] - -0.1f) < 1e-6f);
	CHECK(v[2][2] < 0.f && v[2][2] > v[4][2] && v[1][2] > v[2][2]);
	for (i = 0; i < 5; i++)
		CHECK(v[i][0] == 0.f && v[i][1] == 0.f);
	/* a body with no height does not drop */
	{
		float flat[2][3] = { { 0.f, 0.f, 0.2f }, { 0.5f, 0.f, 0.2f } };
		float fv[2][3] = { { 0.f, 0.f, 0.f }, { 0.f, 0.f, 0.f } };

		CHECK(!ae_ragdoll_leg_collapse(flat, 2, 0.1f, fv));
	}
	/* the legs' kick is a little of the usual (a fifth) */
	CHECK(AE_RAGDOLL_LEG_KICK_SCALE > 0.f && AE_RAGDOLL_LEG_KICK_SCALE <= 0.25f);

	/* a melee kill's direction: level, from the attacker's middle (the epicenter) to the victim's, so a blow from
	behind sends the body forward (away from the attacker); none when they are on one spot */
	{
		float attacker[3] = { 1.f, 2.f, 0.6f }, victim[3] = { 1.f, 3.f, 0.7f }, d[3];

		CHECK(ae_ragdoll_melee_direction(attacker, victim, d));
		CHECK(fabsf(d[0]) < 1e-6f && fabsf(d[1] - 1.f) < 1e-6f && d[2] == 0.f);
		victim[0] = 4.f;
		victim[1] = 6.f;
		CHECK(ae_ragdoll_melee_direction(attacker, victim, d));
		CHECK(fabsf(d[0] - 0.6f) < 1e-5f && fabsf(d[1] - 0.8f) < 1e-5f);
		victim[0] = 1.f;
		victim[1] = 2.f;
		CHECK(!ae_ragdoll_melee_direction(attacker, victim, d));
	}
	/* the throw (wu/s): EXAGGERATED hardest, MIXED clear, MODERN through its category push (1.0 x 1.2), the original
	style's left */
	CHECK(ae_ragdoll_melee_throw(AE_RAGDOLL_STYLE_EXAGGERATED) >= 3.f * ae_ragdoll_melee_throw(AE_RAGDOLL_STYLE_MIXED));
	CHECK(ae_ragdoll_melee_throw(AE_RAGDOLL_STYLE_MIXED) > 1.2f);
	CHECK(ae_ragdoll_melee_throw(AE_RAGDOLL_STYLE_MODERN) == 0.f);
	CHECK(ae_ragdoll_melee_throw(AE_RAGDOLL_STYLE_SPAGHETTI_WESTERN) == 0.f);
	CHECK(ae_ragdoll_category_push(AE_DAMAGE_CATEGORY_MELEE) * 1.2f < 1.4f); /* under the tumble's threshold */
}

static void mixed_tests(void)
{
	long index, full = 0, handoff = 0;
	int percent, pick;

	/* the kills whose canned animation looks wrong, and a kill nothing is known of, are always full */
	CHECK(ae_ragdoll_kill_kind(AE_DAMAGE_CATEGORY_BULLET, 1, 1) == AE_RAGDOLL_KILL_BLAST);
	CHECK(ae_ragdoll_kill_kind(AE_DAMAGE_CATEGORY_GRENADE, 0, 1) == AE_RAGDOLL_KILL_BLAST);
	CHECK(ae_ragdoll_kill_kind(AE_DAMAGE_CATEGORY_MELEE, 0, 1) == AE_RAGDOLL_KILL_MELEE);
	CHECK(ae_ragdoll_kill_kind(AE_DAMAGE_CATEGORY_VEHICLE, 0, 1) == AE_RAGDOLL_KILL_VEHICLE);
	CHECK(ae_ragdoll_kill_kind(AE_DAMAGE_CATEGORY_FALLING, 0, 1) == AE_RAGDOLL_KILL_FALL);
	CHECK(ae_ragdoll_kill_kind(AE_DAMAGE_CATEGORY_BULLET, 0, 0) == AE_RAGDOLL_KILL_UNKNOWN);
	CHECK(ae_ragdoll_kill_kind(AE_DAMAGE_CATEGORY_SNIPER, 0, 1) == AE_RAGDOLL_KILL_SHOT);
	for (index = 0; index < 5000; index++)
	{
		int kind;

		for (kind = 0; kind < AE_RAGDOLL_KILL_KINDS; kind++)
		{
			if (kind != AE_RAGDOLL_KILL_SHOT)
				CHECK(ae_ragdoll_mixed_pick(kind, index) == 0);
		}
		pick = ae_ragdoll_mixed_pick(AE_RAGDOLL_KILL_SHOT, index * 65536L + 12345L);
		if (pick == 0)
			full++;
		else
		{
			handoff++;
			CHECK(pick >= AE_RAGDOLL_HANDOFF_MINIMUM && pick <= AE_RAGDOLL_HANDOFF_MAXIMUM);
		}
	}
	/* about half each way, and the same index always gives the same pick */
	CHECK(full > 2300 && full < 2700 && handoff > 2300 && handoff < 2700);
	CHECK(ae_ragdoll_mixed_pick(AE_RAGDOLL_KILL_SHOT, 777) == ae_ragdoll_mixed_pick(AE_RAGDOLL_KILL_SHOT, 777));
	/* the share spans its range (both ends and the middle are reached) */
	{
		int low = 100, high = 0;

		for (index = 0; index < 5000; index++)
		{
			percent = ae_ragdoll_mixed_pick(AE_RAGDOLL_KILL_SHOT, index);
			if (percent && percent < low)
				low = percent;
			if (percent > high)
				high = percent;
		}
		CHECK(low <= 32 && high >= 58);
	}
	/* consecutive indices (a body's index counts up by one) do not all pick alike */
	full = 0;
	for (index = 1000; index < 1200; index++)
		full += ae_ragdoll_mixed_pick(AE_RAGDOLL_KILL_SHOT, index) == 0;
	CHECK(full > 70 && full < 130);
}

static void blast_cap_tests(void)
{
	float v[5][3], m[5] = { 1.f, 1.f, 1.f, 1.f, 0.f };
	int i;

	/* a body thrown at 0.4 a tick, two parts turning it: slowed together to a middle speed of 0.2 */
	zero(v, 5);
	for (i = 0; i < 4; i++)
		v[i][0] = 0.4f;
	v[2][1] = -0.1f;
	v[3][1] = 0.1f;
	v[4][0] = 5.f; /* (a fixed part, inverse mass 0, is not counted) */
	CHECK(fabsf(ae_ragdoll_blast_cap(AE_RAGDOLL_STYLE_MODERN, m, 5, v) - 0.5f) < 1e-4f);
	CHECK(fabsf(v[0][0] - 0.2f) < 1e-4f && fabsf(v[3][1] - 0.05f) < 1e-4f);
	/* already slow: left alone */
	zero(v, 5);
	v[0][2] = 0.1f;
	CHECK(ae_ragdoll_blast_cap(AE_RAGDOLL_STYLE_MIXED, m, 5, v) == 1.f && v[0][2] == 0.1f);
	/* EXAGGERATED throws it as far as the blast does */
	zero(v, 5);
	for (i = 0; i < 4; i++)
		v[i][2] = 0.45f;
	CHECK(ae_ragdoll_blast_cap(AE_RAGDOLL_STYLE_EXAGGERATED, m, 5, v) == 1.f && v[0][2] == 0.45f);
	/* no moving part */
	CHECK(ae_ragdoll_blast_cap(AE_RAGDOLL_STYLE_MODERN, m + 4, 1, v + 4) == 1.f);
}

int main(void)
{
	long units[3] = { 10, -1, -1 };
	long dead[3] = { -1, 22, -1 };

	/* a living player's unit */
	CHECK(ae_ragdoll_is_player_body(0, 10, units, dead, 3));
	/* a dead player's body: player_index cleared at death, the player's dead_unit_index names it (the old rule: AI) */
	CHECK(ae_ragdoll_is_player_body(-1, 22, units, dead, 3));
	/* the body whose unit_index the player still holds */
	CHECK(ae_ragdoll_is_player_body(-1, 10, units, dead, 3));
	/* an AI's body: no player names it */
	CHECK(!ae_ragdoll_is_player_body(-1, 33, units, dead, 3));
	/* no unit never matches the lists' empty entries */
	CHECK(!ae_ragdoll_is_player_body(-1, -1, units, dead, 3));
	/* no players */
	CHECK(!ae_ragdoll_is_player_body(-1, 22, units, dead, 0));
	category_tests();
	mixed_tests();
	blast_cap_tests();
	if (!failures)
		printf("ok\n");
	return failures ? 1 : 0;
}
