#include <stdio.h>
#include <string.h>
#include "ae_hit_feedback_rules.h"
#include "ae_silent_rules.h"

static int failures;
static int checks;
#define CHECK(c) do { checks++; if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

/* the option lives beside SILENT's in one byte: neither touches the other's bits; every old gametype is CLASSIC */
static void test_byte(void)
{
	unsigned byte;

	CHECK(ae_hit_feedback_value(0) == AE_HIT_FEEDBACK_CLASSIC);
	for (byte = 0; byte < 256; byte++)
	{
		unsigned char on = ae_hit_feedback_byte_set((unsigned char)byte, AE_HIT_FEEDBACK_ARENA);
		unsigned char off = ae_hit_feedback_byte_set((unsigned char)byte, AE_HIT_FEEDBACK_CLASSIC);

		CHECK(ae_hit_feedback_value(on) == AE_HIT_FEEDBACK_ARENA);
		CHECK(ae_hit_feedback_value(off) == AE_HIT_FEEDBACK_CLASSIC && (off & 3) == 0 && (on & 3) == 1);
		/* (the two values kept for later looks play ARENA EVOLVED meanwhile) */
		CHECK(ae_hit_feedback_value((unsigned char)((byte & 0xFC) | 2)) == AE_HIT_FEEDBACK_ARENA);
		CHECK((on & 0xFC) == (byte & 0xFC) && (off & 0xFC) == (byte & 0xFC));
		CHECK(ae_silent_mode(on, AE_SILENT_ROLE_MOVEMENT) == ae_silent_mode((unsigned char)byte, AE_SILENT_ROLE_MOVEMENT));
		CHECK(ae_silent_mode(on, AE_SILENT_ROLE_WEAPON) == ae_silent_mode((unsigned char)byte, AE_SILENT_ROLE_WEAPON));
		CHECK(ae_hit_feedback_value(ae_silent_byte_set(on, AE_SILENT_ROLE_MOVEMENT, AE_SILENT_SILENT)) ==
			AE_HIT_FEEDBACK_ARENA);
	}
	/* (a value that is not one of the two is CLASSIC) */
	CHECK(ae_hit_feedback_value(ae_hit_feedback_byte_set(1, 7)) == AE_HIT_FEEDBACK_CLASSIC);
	CHECK(!strcmp(ae_hit_feedback_name(AE_HIT_FEEDBACK_ARENA), "arena evolved"));
	CHECK(!strcmp(ae_hit_feedback_name(AE_HIT_FEEDBACK_CLASSIC), "halo ce"));
}

/* the cases found in play: a 75 point shield, the magnum's 25 against it as the finisher */
static void test_pop_line(void)
{
	float const shield = 75.f, magnum = 25.f, rifle_round = 10.f;

	/* (one magnum hit: 50 left, no; two: exactly 25 left, the third only empties the shield: no kill, no pop...) */
	CHECK(!ae_hit_feedback_pop_due(shield - magnum, magnum));
	CHECK(!ae_hit_feedback_pop_due(shield - 2 * magnum, magnum));
	/* (...a hair under: yes) */
	CHECK(ae_hit_feedback_pop_due(shield - 2 * magnum - 0.001f, magnum));
	/* (the assault rifle: 5 rounds leave exactly 25, no; the 6th, 15: yes) */
	CHECK(!ae_hit_feedback_pop_due(shield - 5 * rifle_round, magnum));
	CHECK(ae_hit_feedback_pop_due(shield - 6 * rifle_round, magnum));
	/* (a shield already gone: yes; no finishing weapon on the map: never) */
	CHECK(ae_hit_feedback_pop_due(0.f, magnum));
	CHECK(!ae_hit_feedback_pop_due(0.f, 0.f));
	CHECK(!ae_hit_feedback_pop_due(10.f, -1.f));
}

static void test_shapes(void)
{
	float strength = 0.f, top = 0.f, front, tail, rgb[3], before = 2.f, height, flicker;
	int step, lit = 0;

	/* (the pop: whole body for the first quarter, then its top falls to the feet, never rising; over at 1) */
	CHECK(ae_hit_feedback_pop_shape(0.f, &strength, &top) && strength == 1.f && top == 1.f);
	CHECK(ae_hit_feedback_pop_shape(0.2f, &strength, &top) && top == 1.f);
	for (step = 0; step < 100; step++)
	{
		CHECK(ae_hit_feedback_pop_shape((float)step / 100.f, &strength, &top));
		CHECK(top <= before && top >= 0.f && top <= 1.f);
		before = top;
	}
	CHECK(top < 0.05f);
	CHECK(!ae_hit_feedback_pop_shape(1.f, &strength, &top) && !ae_hit_feedback_pop_shape(-0.1f, &strength, &top));
	CHECK(ae_hit_feedback_pop_shape(0.5f, NULL, NULL));
	/* (the band: its front is the shield's level, its tail under it and never under the feet) */
	before = -1.f;
	for (step = 0; step <= 100; step++)
	{
		ae_hit_feedback_band((float)step / 100.f, &front, &tail);
		CHECK(front >= before && tail <= front && tail >= 0.f && front <= 1.f);
		before = front;
		ae_hit_feedback_band_color((float)step / 100.f, rgb);
		CHECK(rgb[0] == 1.f && rgb[1] > 0.9f && rgb[1] <= 1.f && rgb[2] >= 0.5f && rgb[2] <= 1.f);
		CHECK(ae_hit_feedback_arms_swell((float)step / 100.f) >= 0.f && ae_hit_feedback_arms_swell((float)step / 100.f) <= 1.f);
		CHECK(ae_hit_feedback_hit_pulse((float)step / 100.f) <= (float)step / 100.f + 0.0001f);
	}
	ae_hit_feedback_band(2.f, &front, &tail);
	CHECK(front == 1.f);
	ae_hit_feedback_band(-1.f, &front, &tail);
	CHECK(front == 0.f && tail == 0.f);
	CHECK(ae_hit_feedback_arms_swell(0.55f) == 1.f && ae_hit_feedback_arms_swell(0.f) == 0.f);
	CHECK(ae_hit_feedback_hit_pulse(1.f) == 1.f && ae_hit_feedback_hit_pulse(0.f) == 0.f && ae_hit_feedback_hit_pulse(5.f) == 1.f);
	CHECK(ae_hit_feedback_full_flash(0.f) == 1.f && ae_hit_feedback_full_flash(0.3f) == 0.f &&
		ae_hit_feedback_full_flash(-0.1f) == 0.f && ae_hit_feedback_full_flash(0.15f) > 0.4f);
	/* (the flicker: about 3 bands a second, the same for the same unit and moment, heights and strengths in range) */
	for (step = 0; step < 14 * 100; step++)
	{
		float seconds = ((float)step + 0.5f) / 14.f;

		if (ae_hit_feedback_flicker(0x00E30012ul, seconds, &height, &flicker))
		{
			float height_again = -1.f;

			lit++;
			CHECK(height >= 0.f && height < 1.f && flicker > 0.3f && flicker <= 0.8f);
			CHECK(ae_hit_feedback_flicker(0x00E30012ul, seconds, &height_again, NULL) && height_again == height);
		}
	}
	CHECK(lit > 200 && lit < 420);
	CHECK(ae_hit_feedback_flicker(1, -5.f, NULL, NULL) == ae_hit_feedback_flicker(1, 0.f, NULL, NULL));
}

int main(void)
{
	test_byte();
	test_pop_line();
	test_shapes();
	printf("%d checks, %d failures\n", checks, failures);
	return failures != 0;
}
