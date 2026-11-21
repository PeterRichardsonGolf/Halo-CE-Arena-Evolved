#include <stddef.h>
#include <stdio.h>

#include "ae_melee_rules.h"
#include "ae_variant_ext_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)
#define NEAR(a, b) ((a) - (b) < 0.0001f && (b) - (a) < 0.0001f)

int main(void)
{
	struct ae_variant_ext ext;

	/* the blow's points */
	CHECK(NEAR(ae_melee_blow_points(AE_EXT_MELEE_AE, 71.f, 75.f, 1.f), 100.f));
	CHECK(NEAR(ae_melee_blow_points(AE_EXT_MELEE_AE, 120.f, 75.f, 1.f), 120.f));
	CHECK(NEAR(ae_melee_blow_points(AE_EXT_MELEE_CLASSIC, 71.f, 75.f, 1.f), 71.f));
	/* the floor follows the gametype's damage multiplier: 75 * 4/3 * scale (the weapon's own blow is the least) */
	CHECK(NEAR(ae_melee_blow_points(AE_EXT_MELEE_AE, 10.f, 75.f, 0.5f), 50.f));
	CHECK(NEAR(ae_melee_blow_points(AE_EXT_MELEE_AE, 10.f, 75.f, 1.f), 100.f));
	CHECK(NEAR(ae_melee_blow_points(AE_EXT_MELEE_AE, 10.f, 75.f, 2.f), 200.f));
	CHECK(NEAR(ae_melee_blow_points(AE_EXT_MELEE_AE, 60.f, 75.f, 0.5f), 60.f));
	/* the rule off: the blow comes back as it came whatever the scale */
	CHECK(ae_melee_blow_points(AE_EXT_MELEE_CLASSIC, 71.f, 75.f, 0.5f) == 71.f);
	CHECK(ae_melee_blow_points(AE_EXT_MELEE_CLASSIC, 10.f, 75.f, 2.f) == 10.f);
	CHECK(!ae_melee_rule_on(AE_EXT_MELEE_CLASSIC) && ae_melee_rule_on(AE_EXT_MELEE_AE) && !ae_melee_rule_on(7));
	CHECK(NEAR(ae_melee_reach_forward(), 0.163f));

	/* the byte */
	CHECK(offsetof(struct ae_variant_ext, melee_rule) == 36);
	ae_variant_ext_default(&ext);
	CHECK(ext.melee_rule == AE_EXT_MELEE_CLASSIC);
	ext.melee_rule = 7;
	CHECK(!ae_variant_ext_sanitize(&ext));
	CHECK(ext.melee_rule == AE_EXT_MELEE_CLASSIC);

	/* the shield and the body: a blow of 100 */
	CHECK(NEAR(ae_melee_body_after(100.f, 75.f), 25.f));
	CHECK(75.f - ae_melee_body_after(100.f, 75.f) == 50.f);
	CHECK(NEAR(ae_melee_body_after(100.f, 25.f), 75.f));
	CHECK(NEAR(ae_melee_body_after(100.f, 26.f), 74.f));
	CHECK(ae_melee_body_after(100.f, 25.f) >= 75.f && ae_melee_body_after(100.f, 26.f) < 75.f);
	CHECK(NEAR(ae_melee_body_after(10.f, 75.f), 0.f));

	/* the bleed: a covered melee blow converts the second game's way whatever the row says */
	CHECK(ae_melee_bleed_mode(AE_EXT_MELEE_AE, 1, AE_EXT_SHIELD_BLEED_CLASSIC) == AE_EXT_SHIELD_BLEED_HALO2);
	CHECK(ae_melee_bleed_mode(AE_EXT_MELEE_AE, 0, AE_EXT_SHIELD_BLEED_CLASSIC) == AE_EXT_SHIELD_BLEED_CLASSIC);
	CHECK(ae_melee_bleed_mode(AE_EXT_MELEE_CLASSIC, 1, AE_EXT_SHIELD_BLEED_CLASSIC) == AE_EXT_SHIELD_BLEED_CLASSIC);
	CHECK(ae_melee_bleed_mode(AE_EXT_MELEE_CLASSIC, 1, AE_EXT_SHIELD_BLEED_HALO2) == AE_EXT_SHIELD_BLEED_HALO2);

	/* carry-over units: a blow raised from 70 (raw 50 x1.4) to 100 breaks a shield with 25 left; the 75 left over
	converts by the UNRAISED ratio (raw/70) to 75/1.4 raw, and the body's 1.4 lands it as 75 body points */
	{
		float raw = 50.f, unraised = 70.f, shield_left = 25.f, excess = 100.f - shield_left;
		float carried = ae_melee_bleed_excess(excess, raw, unraised);

		CHECK(NEAR(carried * 1.4f, 75.f));
		/* the raised ratio (raw/100) would land only 52.5 */
		CHECK(!NEAR(excess * raw / 100.f * 1.4f, 75.f));
		/* an unraised blow's number is the same as the second game's own rule */
		CHECK(NEAR(ae_melee_bleed_excess(10.f, 50.f, 70.f), 10.f * 50.f / 70.f));
		CHECK(NEAR(ae_melee_bleed_excess(10.f, 0.f, 70.f), 10.f));
	}

	/* a leftover that reaches the victim's full health kills (it is a hair over), one under does not */
	CHECK(ae_melee_kill_points(75.f, 75.f) > 75.f);
	CHECK(ae_melee_kill_points(74.9999f, 75.f) > 75.f);
	CHECK(ae_melee_kill_points(75.0001f, 75.f) > 75.f);
	CHECK(NEAR(ae_melee_kill_points(74.f, 75.f), 74.f));
	CHECK(NEAR(ae_melee_kill_points(120.f, 75.f), 120.f));
	CHECK(NEAR(ae_melee_kill_points(0.f, 75.f), 0.f));

	/* the body side: an uncounted blow that reaches the body is raised; one the shield counted, or an over-charged
	shield's (the shield hook did not run, the body gets 0), is not */
	CHECK(ae_melee_body_raise(0, 70.f));
	CHECK(!ae_melee_body_raise(1, 70.f));
	CHECK(!ae_melee_body_raise(0, 0.f));
	CHECK(!ae_melee_body_raise(1, 0.f));

	/* a melee from behind kills only under AE, a melee, on an enemy, not seated, unsuspecting */
	CHECK(ae_melee_kills_from_behind(AE_EXT_MELEE_AE, 1, 1, 0, 1));
	CHECK(!ae_melee_kills_from_behind(AE_EXT_MELEE_CLASSIC, 1, 1, 0, 1));
	CHECK(!ae_melee_kills_from_behind(AE_EXT_MELEE_AE, 0, 1, 0, 1));
	CHECK(!ae_melee_kills_from_behind(AE_EXT_MELEE_AE, 1, 0, 0, 1));
	CHECK(!ae_melee_kills_from_behind(AE_EXT_MELEE_AE, 1, 1, 1, 1));
	CHECK(!ae_melee_kills_from_behind(AE_EXT_MELEE_AE, 1, 1, 0, 0));

	/* the pop's finishing points: AE none (the pop plays at the real break), CLASSIC the finisher's */
	CHECK(NEAR(ae_melee_pop_points(AE_EXT_MELEE_AE, 25.f), 25.f));
	CHECK(NEAR(ae_melee_pop_points(AE_EXT_MELEE_CLASSIC, 25.f), 25.f));
	CHECK(NEAR(ae_melee_pop_points(AE_EXT_MELEE_CLASSIC, 25.02f), 25.02f));

	/* the ray's origin: the unit's position plus the reach along the aim's horizontal, the head's height as posed */
	{
		float o[3];

		ae_melee_origin_from(0.f, 0.f, 0.62f, 1.f, 0.f, o);
		CHECK(NEAR(o[0], 0.163f) && NEAR(o[1], 0.f) && NEAR(o[2], 0.62f));
		ae_melee_origin_from(10.f, 20.f, 0.62f, 3.f, 4.f, o);
		CHECK(NEAR(o[0], 10.0978f) && NEAR(o[1], 20.1304f) && NEAR(o[2], 0.62f));
		ae_melee_origin_from(10.f, 20.f, 0.5f, 0.f, 0.f, o);
		CHECK(NEAR(o[0], 10.f) && NEAR(o[1], 20.f) && NEAR(o[2], 0.5f));
	}

	/* a back smack's cone: straight behind and 45 degrees off count; the side (90) and 70 degrees off do not;
	the look's length does not matter; a zero vector never counts */
	CHECK(ae_melee_from_behind(1.f, 0.f, 1.f, 0.f));
	CHECK(ae_melee_from_behind(1.f, 1.f, 2.f, 0.f));
	CHECK(!ae_melee_from_behind(0.f, 1.f, 1.f, 0.f));
	CHECK(!ae_melee_from_behind(0.342f, 0.940f, 1.f, 0.f));
	CHECK(!ae_melee_from_behind(-1.f, 0.f, 1.f, 0.f));
	CHECK(!ae_melee_from_behind(0.f, 0.f, 1.f, 0.f));
	CHECK(!ae_melee_from_behind(1.f, 0.f, 0.f, 0.f));

	if (failures)
		return 1;
	printf("ae_melee_rules_test: ok\n");
	return 0;
}
