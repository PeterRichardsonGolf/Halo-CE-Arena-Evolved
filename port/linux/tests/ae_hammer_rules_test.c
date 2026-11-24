#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include "ae_hammer_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static int near(float a, float b)
{
	return fabsf(a - b) < 1e-4f;
}

static float length(float const v[3])
{
	return sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
}

int main(void)
{
	float const blast[3] = { 0.f, 0.f, 0.f };
	float velocity[3];

	/* the blast is the gravity hammer pack's damage tag, whatever the case; nothing else is */
	CHECK(ae_hammer_rules_blast("ae\\packs\\gravity_hammer\\hammer\\effects\\push"));
	CHECK(ae_hammer_rules_blast("AE\\Packs\\Gravity_Hammer\\Hammer\\Effects\\Push"));
	CHECK(!ae_hammer_rules_blast("ae\\packs\\gravity_hammer\\hammer\\effects\\melee"));
	CHECK(!ae_hammer_rules_blast("ae\\packs\\gravity_hammer\\hammer\\effects\\push2"));
	CHECK(!ae_hammer_rules_blast("grifball\\hammer\\effects\\push"));
	CHECK(!ae_hammer_rules_blast("weapons\\frag grenade\\explosion"));
	CHECK(!ae_hammer_rules_blast(NULL));

	/* the swing is the gravity hammer pack's weapon, whatever the case; its blow's rays are 2.5 (3.125 x the 0.8 ray):
	with the origin 0.163 ahead, the front of a body (0.2 before its centre) at 2.8 is reached, one at 3 is not */
	CHECK(ae_hammer_rules_weapon("ae\\packs\\gravity_hammer\\hammer\\gravity_hammer"));
	CHECK(ae_hammer_rules_weapon("AE\\PACKS\\GRAVITY_HAMMER\\HAMMER\\GRAVITY_HAMMER"));
	CHECK(!ae_hammer_rules_weapon("ae\\packs\\gravity_hammer\\hammer\\effects\\push"));
	CHECK(!ae_hammer_rules_weapon("weapons\\rocket launcher\\rocket launcher"));
	CHECK(!ae_hammer_rules_weapon(NULL));
	CHECK(!ae_hammer_rules_blast("ae\\packs\\gravity_hammer\\hammer\\gravity_hammer"));
	CHECK(near(ae_hammer_rules_swing_reach_scale() * 0.8f, 2.5f));
	CHECK(0.163f + ae_hammer_rules_swing_reach_scale() * 0.8f > 2.8f - 0.2f);
	CHECK(0.163f + ae_hammer_rules_swing_reach_scale() * 0.8f < 3.f - 0.2f - 0.1f);

	/* the reticle's red test: the swing's own rays cut 0.15 short, 2.35 of their 2.5 */
	CHECK(near(ae_hammer_rules_red_reach_share() * ae_hammer_rules_swing_reach_scale() * 0.8f, 2.35f));
	CHECK(ae_hammer_rules_red_reach_share() < 1.f && ae_hammer_rules_red_reach_share() > 0.9f);

	/* one swing, one cooldown: at 0.7 swings a second (30 ticks a second, a cycle of 42.9 ticks) the melee button is
	refused for 42 ticks after a trigger swing, let at 43; a hammer never swung, or a trigger of no rate, never refuses */
	CHECK(ae_hammer_rules_melee_blocked(0, 0.7f, 30.f));
	CHECK(ae_hammer_rules_melee_blocked(12, 0.7f, 30.f));
	CHECK(ae_hammer_rules_melee_blocked(42, 0.7f, 30.f));
	CHECK(!ae_hammer_rules_melee_blocked(43, 0.7f, 30.f));
	CHECK(!ae_hammer_rules_melee_blocked(-1, 0.7f, 30.f));
	CHECK(!ae_hammer_rules_melee_blocked(5, 0.f, 30.f));

	/* one unit in front, at the blast's height: pushed away, 3.5 x the object's scale a second,
	not raised but by a twentieth of its radius */
	{
		float const center[3] = { 1.f, 0.f, 0.f };

		CHECK(ae_hammer_rules_push(blast, center, 0.2f, 1.f, 0, velocity));
		CHECK(near(length(velocity), 3.5f));
		CHECK(velocity[0] > 3.49f);
		CHECK(near(velocity[2] / velocity[0], 0.05f * 0.2f / 1.f));
		CHECK(near(velocity[1], 0.f));
		CHECK(ae_hammer_rules_push(blast, center, 0.2f, 2.f, 0, velocity));
		CHECK(near(length(velocity), 7.f));
	}
	/* a body takes 4.2 a second whatever its own scale (the stock cyborg's is 0.7) */
	{
		float const center[3] = { 0.f, -1.f, 0.f };

		CHECK(ae_hammer_rules_push(blast, center, 0.42f, 0.7f, 1, velocity));
		CHECK(near(length(velocity), 4.2f));
		CHECK(velocity[1] < -4.19f);
		CHECK(ae_hammer_rules_push(blast, center, 0.42f, 0.f, 1, velocity));
		CHECK(near(length(velocity), 4.2f));
	}
	/* the same push near and far: distance does not scale it */
	{
		float const close[3] = { 0.3f, 0.f, 0.f };
		float const far[3] = { 1.6f, 0.f, 0.f };
		float other[3];

		CHECK(ae_hammer_rules_push(blast, close, 0.2f, 1.f, 1, velocity));
		CHECK(ae_hammer_rules_push(blast, far, 0.2f, 1.f, 1, other));
		CHECK(near(length(velocity), length(other)));
	}
	/* out to 1.5 from the body's surface (its centre less its radius), no farther */
	{
		float const edge[3] = { 1.7f, 0.f, 0.f };
		float const past[3] = { 1.71f, 0.f, 0.f };
		float const two[3] = { 2.f, 0.f, 0.f };

		CHECK(ae_hammer_rules_push(blast, edge, 0.2f, 1.f, 1, velocity));
		CHECK(!ae_hammer_rules_push(blast, past, 0.2f, 1.f, 1, velocity));
		CHECK(near(length(velocity), 0.f));
		CHECK(!ae_hammer_rules_push(blast, two, 0.2f, 1.f, 1, velocity));
	}
	/* an object whose push is 0.05 or less is not pushed (a body's is never that low) */
	{
		float const center[3] = { 1.f, 0.f, 0.f };

		CHECK(ae_hammer_rules_push(blast, center, 0.2f, 0.3f / 3.5f, 0, velocity));
		CHECK(!ae_hammer_rules_push(blast, center, 0.2f, 0.05f / 3.5f, 0, velocity));
		CHECK(ae_hammer_rules_push(blast, center, 0.2f, 0.06f / 3.5f, 0, velocity));
		CHECK(!ae_hammer_rules_push(blast, center, 0.2f, 0.f, 0, velocity));
	}
	/* above the blast: straight up; at the blast itself: up */
	{
		float const above[3] = { 0.f, 0.f, 0.8f };

		CHECK(ae_hammer_rules_push(blast, above, 0.2f, 1.f, 1, velocity));
		CHECK(near(velocity[2], 4.2f));
		CHECK(ae_hammer_rules_push(blast, blast, 0.f, 1.f, 1, velocity));
		CHECK(near(velocity[0], 0.f) && near(velocity[1], 0.f) && near(velocity[2], 4.2f));
	}
	if (failures)
		return 1;
	printf("ae_hammer_rules_test: all passed\n");
	return 0;
}
