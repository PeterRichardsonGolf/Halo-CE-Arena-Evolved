#include <stdio.h>

#include "ae_melee_lunge_rules.h"
#include "ae_variant_ext_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)
#define NEAR(a, b) ((a) - (b) < 0.0001f && (b) - (a) < 0.0001f)

int main(void)
{
	struct ae_melee_lunge_params ae, strong, none;
	struct ae_variant_ext ext;
	float gap, moved;
	int tick;

	/* the presets: OFF and unknown values are Halo CE's melee, no params */
	CHECK(!ae_melee_lunge_params_get(AE_EXT_MELEE_LUNGE_OFF, &none));
	CHECK(!ae_melee_lunge_params_get(AE_EXT_MELEE_LUNGE_COUNT, &none));
	CHECK(!ae_melee_lunge_params_get(200, &none));
	CHECK(ae_melee_lunge_params_get(AE_EXT_MELEE_LUNGE_AE, &ae));
	CHECK(NEAR(ae.reach, 1.1f) && NEAR(ae.speed, 1.5f) && NEAR(ae.turn_degrees, 1.5f) && NEAR(ae.minimum, 0.8f));
	CHECK(ae.cooldown_ticks == 30 && NEAR(ae.ease, 0.375f) && NEAR(ae.cone_degrees, 15.0f));
	CHECK(ae_melee_lunge_params_get(AE_EXT_MELEE_LUNGE_STRONG, &strong));
	CHECK(NEAR(strong.reach, 1.05f) && NEAR(strong.speed, 4.0f) && NEAR(strong.turn_degrees, 4.0f));
	CHECK(strong.cooldown_ticks == 30 && NEAR(strong.minimum, 0.8f));
	CHECK(strong.reach < ae.reach && strong.speed > ae.speed);

	/* the names */
	CHECK(ae_melee_lunge_name(AE_EXT_MELEE_LUNGE_AE)[0] == 'a');
	CHECK(ae_melee_lunge_name(AE_EXT_MELEE_LUNGE_STRONG)[0] == 's');
	CHECK(ae_melee_lunge_name(AE_EXT_MELEE_LUNGE_OFF)[0] == 'o' && ae_melee_lunge_name(9)[0] == 'o');

	/* the window: inside 0.8 (and at it) Halo CE's melee; past it a lunge, once a second */
	CHECK(ae_melee_lunge_verdict(&ae, 0.5f, 100, 0) == _ae_melee_lunge_plain);
	CHECK(ae_melee_lunge_verdict(&ae, 0.8f, 100, 0) == _ae_melee_lunge_plain);
	CHECK(ae_melee_lunge_verdict(&ae, 0.81f, 100, 0) == _ae_melee_lunge_go);
	CHECK(ae_melee_lunge_verdict(&ae, 1.1f, 100, 100) == _ae_melee_lunge_go);
	CHECK(ae_melee_lunge_verdict(&ae, 1.1f, 99, 100) == _ae_melee_lunge_too_soon);
	/* (a close swing is plain even when the last lunge was lately: it never waits) */
	CHECK(ae_melee_lunge_verdict(&ae, 0.4f, 99, 100) == _ae_melee_lunge_plain);

	/* a step: the ease's share of the gap, no more than the speed gives, never negative */
	CHECK(NEAR(ae_melee_lunge_step(&ae, 1.1f, 0.6f), 0.05f));        /* 1.5 / 30 */
	CHECK(NEAR(ae_melee_lunge_step(&strong, 1.0f, 0.6f), 0.4f < 4.0f / 30.0f ? 0.4f : 4.0f / 30.0f));
	CHECK(NEAR(ae_melee_lunge_step(&ae, 0.65f, 0.6f), 0.05f));       /* the gap left to the arrival is 0.05 */
	CHECK(NEAR(ae_melee_lunge_step(&ae, 0.62f, 0.6f), 0.02f));
	CHECK(NEAR(ae_melee_lunge_step(&ae, 0.5f, 0.6f), 0.0f));         /* past the arrival: no step */
	{
		struct ae_melee_lunge_params slow = strong;

		slow.speed = 90.0f;                                          /* fast: the ease binds, 0.375 of the gap */
		CHECK(NEAR(ae_melee_lunge_step(&slow, 1.0f, 0.0f), 0.375f));
	}
	/* a lunge from the reach closes toward 0.6 and never past it */
	gap = 1.1f;
	moved = 0.0f;
	for (tick = 0; tick < 60; tick++)
	{
		float step = ae_melee_lunge_step(&ae, gap, 0.6f);

		gap -= step;
		moved += step;
	}
	CHECK(gap >= 0.6f - 0.0001f && moved <= 0.5001f);

	/* the block keeps the byte; zero is OFF; out of range is OFF again; the other byte likewise */
	ae_variant_ext_default(&ext);
	CHECK(ext.melee_lunge == AE_EXT_MELEE_LUNGE_OFF && ext.shootable_grenades == AE_EXT_SHOOTABLE_GRENADES_OFF);
	ext.melee_lunge = AE_EXT_MELEE_LUNGE_STRONG;
	CHECK(!ae_variant_ext_is_default(&ext) && ae_variant_ext_sanitize(&ext) && ext.melee_lunge == AE_EXT_MELEE_LUNGE_STRONG);
	ext.melee_lunge = AE_EXT_MELEE_LUNGE_COUNT;
	CHECK(!ae_variant_ext_sanitize(&ext) && ext.melee_lunge == AE_EXT_MELEE_LUNGE_OFF);

	if (failures)
		return 1;
	printf("ae_melee_lunge_rules: ok\n");
	return 0;
}
