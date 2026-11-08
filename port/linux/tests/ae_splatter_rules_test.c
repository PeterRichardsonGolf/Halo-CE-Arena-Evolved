#include <stdio.h>
#include "ae_splatter_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

int main(void)
{
	unsigned int byte;

	/* bit 3 of pad5 only: COMBAT FEEDBACK (0-1), JUGGERNAUT (2) and SILENT (4-7) kept */
	for (byte = 0; byte < 256; byte++)
	{
		unsigned char on = ae_splatter_byte_set((unsigned char)byte, 1), off = ae_splatter_byte_set((unsigned char)byte, 0);

		CHECK(ae_splatter_fast_only(on) && !ae_splatter_fast_only(off));
		CHECK((on & ~AE_SPLATTER_BYTE_BIT) == (byte & ~AE_SPLATTER_BYTE_BIT));
		CHECK((off & ~AE_SPLATTER_BYTE_BIT) == (byte & ~AE_SPLATTER_BYTE_BIT));
	}
	CHECK(!ae_splatter_fast_only(0));
	/* HALO CE and enemies: all of it, at any speed */
	CHECK(ae_splatter_scale(0, 1, 0.07f) == 1.0f && ae_splatter_scale(1, 0, 0.07f) == 1.0f);
	/* FAST ONLY, a teammate: nothing at a nudge, half between, all of it from a hard ram */
	CHECK(ae_splatter_scale(1, 1, 0.05f) == 0.0f && ae_splatter_scale(1, 1, 0.10f) == 0.0f);
	CHECK(ae_splatter_scale(1, 1, 0.15f) > 0.49f && ae_splatter_scale(1, 1, 0.15f) < 0.51f);
	CHECK(ae_splatter_scale(1, 1, 0.20f) == 1.0f && ae_splatter_scale(1, 1, 0.5f) == 1.0f);
	if (!failures)
		printf("ae_splatter_rules_test: all passed\n");
	return failures != 0;
}
