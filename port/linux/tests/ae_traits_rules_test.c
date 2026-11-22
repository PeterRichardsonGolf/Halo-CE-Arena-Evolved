#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ae_traits_rules.h"
#include "ae_variant_ext_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static int near(float a, float b)
{
	return fabsf(a - b) < 0.0005f;
}

int main(void)
{
	static const int heights[16] = { 50, 60, 70, 80, 90, 100, 110, 120, 130, 140, 150, 160, 170, 180, 190, 200 };
	/* (the JUMP HEIGHT row's stored speeds, as ae_glue_setup_options.c lists them) */
	static const unsigned char speeds[16] = { 71, 77, 84, 89, 95, 0, 105, 110, 114, 118, 122, 126, 130, 134, 138, 141 };
	struct ae_variant_ext ext, wire;
	int index;

	/* the table: every trait has a rule, in its own storage */
	CHECK(ae_trait_rule(AE_TRAIT_COUNT) == NULL && ae_trait_rule(-1) == NULL);
	CHECK(ae_trait_rule(AE_TRAIT_JUMP_HEIGHT)->storage == AE_TRAIT_IN_OPTIONS &&
		ae_trait_rule(AE_TRAIT_JUMP_HEIGHT)->offset == 0x27);
	CHECK(ae_trait_rule(AE_TRAIT_GRAVITY)->storage == AE_TRAIT_IN_EXT &&
		ae_trait_rule(AE_TRAIT_GRAVITY)->offset == offsetof(struct ae_variant_ext, gravity_percent));
	CHECK(!strcmp(ae_trait_rule(AE_TRAIT_SPEED)->name, "PLAYER SPEED"));

	/* zero, and a byte out of range, is the map's own */
	for (index = 0; index < AE_TRAIT_COUNT; index++)
	{
		CHECK(ae_trait_percent(index, 0) == 100 && near(ae_trait_multiplier(index, 0), 1.0f));
		CHECK(ae_trait_percent(index, 255) == 100 && near(ae_trait_multiplier(index, 255), 1.0f));
		CHECK(ae_trait_stored(index, 100) == 0);
	}
	CHECK(ae_trait_percent(AE_TRAIT_GRAVITY, 9) == 100 && ae_trait_percent(AE_TRAIT_GRAVITY, 10) == 10);

	/* JUMP HEIGHT: the row's heights are the squares of the stored speeds, within a percent */
	for (index = 0; index < 16; index++)
	{
		CHECK(ae_trait_stored(AE_TRAIT_JUMP_HEIGHT, heights[index]) == speeds[index]);
		CHECK(abs(ae_trait_percent(AE_TRAIT_JUMP_HEIGHT, speeds[index]) - heights[index]) <= 1);
	}
	/* a jump speed of 120% (the seeds' JUMP before the heights) rises 144%; the speed is the multiplier */
	CHECK(ae_trait_percent(AE_TRAIT_JUMP_HEIGHT, 120) == 144 && near(ae_trait_multiplier(AE_TRAIT_JUMP_HEIGHT, 120), 1.2f));
	CHECK(ae_trait_stored(AE_TRAIT_JUMP_HEIGHT, 10) == 50 && ae_trait_stored(AE_TRAIT_JUMP_HEIGHT, 900) == 200);

	/* GRAVITY and PLAYER SPEED: the percent itself */
	CHECK(ae_trait_percent(AE_TRAIT_GRAVITY, 50) == 50 && near(ae_trait_multiplier(AE_TRAIT_GRAVITY, 50), 0.5f));
	CHECK(ae_trait_percent(AE_TRAIT_SPEED, 150) == 150 && near(ae_trait_multiplier(AE_TRAIT_SPEED, 150), 1.5f));
	CHECK(ae_trait_stored(AE_TRAIT_SPEED, 300) == 250 && ae_trait_stored(AE_TRAIT_GRAVITY, 5) == 10);

	/* the rise: Halo CE's jump (0.07 a tick) under its gravity rises 0.687; half the gravity doubles it, as does a
	speed of 141%; the reach is the same ratio */
	CHECK(fabsf(ae_traits_jump_rise(0.07f, 1.0f, 0.0035651792f, 1.0f) - 0.687f) < 0.001f);
	CHECK(near(ae_traits_jump_rise(0.07f, 1.0f, 0.0035651792f, 0.5f) / ae_traits_jump_rise(0.07f, 1.0f,
		0.0035651792f, 1.0f), 2.0f));
	CHECK(near(ae_traits_jump_reach(1.0f, 0.5f), 2.0f) && near(ae_traits_jump_reach(1.2f, 1.0f), 1.44f));
	CHECK(ae_traits_jump_rise(0.0f, 1.0f, 0.0035f, 1.0f) == 0.0f && near(ae_traits_jump_reach(0.0f, 1.0f), 1.0f));

	/* the builds that play GRAVITY and PLAYER SPEED: this one's own, 1.0.0-rc5 and later */
	CHECK(ae_traits_build_has_them("Arena Evolved 1.0.0-rc5", "Arena Evolved 1.0.0-rc5"));
	CHECK(ae_traits_build_has_them("Arena Evolved 1.0.0-rc4-dev", "Arena Evolved 1.0.0-rc4-dev"));
	CHECK(!ae_traits_build_has_them("Arena Evolved 1.0.0-rc4", "Arena Evolved 1.0.0-rc5"));
	CHECK(ae_traits_build_has_them("Arena Evolved 1.0.0-rc5-dev", "x") && ae_traits_build_has_them("Arena Evolved 1.0.0-rc12", NULL));
	CHECK(ae_traits_build_has_them("Arena Evolved 1.0.0", NULL) && ae_traits_build_has_them("Arena Evolved 1.0.1-beta", NULL));
	CHECK(ae_traits_build_has_them("Arena Evolved 1.1.0", NULL) && ae_traits_build_has_them("Arena Evolved 2.0.0", NULL));
	CHECK(!ae_traits_build_has_them("Arena Evolved 1.0.0-beta", NULL) && !ae_traits_build_has_them("Arena Evolved 0.9.9", NULL));
	CHECK(!ae_traits_build_has_them("Arena Evolved 0.7.3-beta", NULL) && !ae_traits_build_has_them("OpenCE 1.0.0", NULL));
	CHECK(!ae_traits_build_has_them("Arena Evolved 1.0", NULL) && !ae_traits_build_has_them(NULL, "Arena Evolved 1.0.0"));

	/* the AE block's bytes: 40 and 41, zero the map's own, so a gametype that never set them needs no block */
	CHECK(offsetof(struct ae_variant_ext, gravity_percent) == 40 && offsetof(struct ae_variant_ext, speed_percent) == 41);
	CHECK(sizeof(struct ae_variant_ext) == AE_VARIANT_EXT_SIZE);
	ae_variant_ext_default(&ext);
	CHECK(ext.gravity_percent == 0 && ext.speed_percent == 0 && ae_variant_ext_is_default(&ext));

	/* a host's travel in the block to a guest and survive its sanitising */
	ext.gravity_percent = 50;
	ext.speed_percent = 200;
	memcpy(&wire, &ext, sizeof(wire));
	CHECK(ae_variant_ext_sanitize(&wire) && wire.gravity_percent == 50 && wire.speed_percent == 200);

	/* out of range falls back to the map's own */
	wire.gravity_percent = 5;
	wire.speed_percent = 251;
	CHECK(!ae_variant_ext_sanitize(&wire) && wire.gravity_percent == 0 && wire.speed_percent == 0);

	if (failures)
		return 1;
	printf("ae_traits_rules_test: ok\n");
	return 0;
}
