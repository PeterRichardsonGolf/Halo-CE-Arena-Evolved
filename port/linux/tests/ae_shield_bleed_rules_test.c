#include <stdio.h>
#include "ae_shield_bleed_rules.h"
#include "ae_variant_ext_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)
#define NEAR(a, b) ((a) - (b) < 0.001f && (b) - (a) < 0.001f)

int main(void)
{
	struct ae_variant_ext ext;

	/* CLASSIC: Halo CE's leftover, the shield's multiplier in it */
	CHECK(NEAR(ae_shield_bleed_excess_rule(AE_EXT_SHIELD_BLEED_CLASSIC, 30.f, 40.f, 60.f, 0), 30.f));
	/* HALO 2: the leftover without the shield's multiplier: a weapon 1.5x on shields bleeds less, an explosion (0.5x) more */
	CHECK(NEAR(ae_shield_bleed_excess_rule(AE_EXT_SHIELD_BLEED_HALO2, 30.f, 40.f, 60.f, 0), 20.f));
	CHECK(NEAR(ae_shield_bleed_excess_rule(AE_EXT_SHIELD_BLEED_HALO2, 10.f, 80.f, 40.f, 0), 20.f));
	/* (a multiplier of 1: the same) */
	CHECK(NEAR(ae_shield_bleed_excess_rule(AE_EXT_SHIELD_BLEED_HALO2, 12.f, 50.f, 50.f, 0), 12.f));
	/* (no damage to divide by: the leftover as it is) */
	CHECK(NEAR(ae_shield_bleed_excess_rule(AE_EXT_SHIELD_BLEED_HALO2, 5.f, 0.f, 50.f, 0), 5.f));
	CHECK(NEAR(ae_shield_bleed_excess_rule(AE_EXT_SHIELD_BLEED_HALO2, 5.f, 50.f, 0.f, 0), 5.f));
	/* (an unknown value is CLASSIC) */
	CHECK(NEAR(ae_shield_bleed_excess_rule(7, 30.f, 40.f, 60.f, 0), 30.f));

	/* no spill: the breaking round carries nothing, in CLASSIC, HALO 2 and an unknown mode; spill as before without it */
	CHECK(NEAR(ae_shield_bleed_excess_rule(AE_EXT_SHIELD_BLEED_CLASSIC, 30.f, 40.f, 60.f, 1), 0.f));
	CHECK(NEAR(ae_shield_bleed_excess_rule(AE_EXT_SHIELD_BLEED_HALO2, 30.f, 40.f, 60.f, 1), 0.f));
	CHECK(NEAR(ae_shield_bleed_excess_rule(7, 30.f, 40.f, 60.f, 1), 0.f));
	CHECK(NEAR(ae_shield_bleed_excess_rule(AE_EXT_SHIELD_BLEED_HALO2, 30.f, 40.f, 60.f, 0), 20.f));

	/* any round that breaks the shield, not only from a full one: shield at 50 after a magnum shot (25), Evo rounds of
	18.9475 on it: 2 leave 12.105, the 3rd breaks it with 6.84 over and carries 0 into health in both settings (no spill),
	then the next head hit on the empty shield kills; a rifle that spills carries the 6.84 (CLASSIC) or 6.45 (HALO 2) */
	CHECK(NEAR(ae_shield_bleed_excess_rule(AE_EXT_SHIELD_BLEED_CLASSIC, 6.8425f, 17.875f, 18.9475f, 1), 0.f));
	CHECK(NEAR(ae_shield_bleed_excess_rule(AE_EXT_SHIELD_BLEED_HALO2, 6.8425f, 17.875f, 18.9475f, 1), 0.f));
	CHECK(NEAR(ae_shield_bleed_excess_rule(AE_EXT_SHIELD_BLEED_CLASSIC, 6.8425f, 17.875f, 18.9475f, 0), 6.8425f));
	CHECK(NEAR(ae_shield_bleed_excess_rule(AE_EXT_SHIELD_BLEED_HALO2, 6.8425f, 17.875f, 18.9475f, 0),
		6.8425f * 17.875f / 18.9475f));

	/* the block keeps the byte; zero is CLASSIC; out of range is CLASSIC again */
	ae_variant_ext_default(&ext);
	CHECK(ext.shield_bleed == AE_EXT_SHIELD_BLEED_CLASSIC);
	ext.shield_bleed = AE_EXT_SHIELD_BLEED_HALO2;
	CHECK(!ae_variant_ext_is_default(&ext) && ae_variant_ext_sanitize(&ext) && ext.shield_bleed == AE_EXT_SHIELD_BLEED_HALO2);
	ext.shield_bleed = AE_EXT_SHIELD_BLEED_COUNT;
	CHECK(!ae_variant_ext_sanitize(&ext) && ext.shield_bleed == AE_EXT_SHIELD_BLEED_CLASSIC);

	if (failures)
		return 1;
	printf("ae_shield_bleed_rules: ok\n");
	return 0;
}
