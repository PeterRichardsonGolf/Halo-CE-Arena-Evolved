#include <stdio.h>

#include "ae_scope_mask_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

int main(void)
{
	/* the packs' masks: 2048 x 512 (Evo, DMR, SMG, magnum, HJ) and 1024 x 512 (CMT's), a zoom item, whole, own size */
	CHECK(ae_scope_mask_fills_view(1, 1, 0.0f, 0.0f, 2048, 512));
	CHECK(ae_scope_mask_fills_view(1, 1, 1.0f, 1.0f, 1024, 512));
	/* a scope's marks (square, scaled to half) and a reticle: as they were */
	CHECK(!ae_scope_mask_fills_view(1, 1, 0.5f, 0.5f, 1024, 1024));
	CHECK(!ae_scope_mask_fills_view(1, 1, 0.0f, 0.0f, 1024, 1024));
	CHECK(!ae_scope_mask_fills_view(1, 1, 0.0f, 0.0f, 64, 64));
	/* a wide picture scaled by its item: its own placement, as it was */
	CHECK(!ae_scope_mask_fills_view(1, 1, 0.5f, 0.0f, 2048, 512));
	CHECK(!ae_scope_mask_fills_view(1, 1, 0.0f, 2.0f, 2048, 512));
	/* not the zoom state, or a sprite of a sheet: as it was */
	CHECK(!ae_scope_mask_fills_view(0, 1, 0.0f, 0.0f, 2048, 512));
	CHECK(!ae_scope_mask_fills_view(1, 0, 0.0f, 0.0f, 2048, 512));
	/* no picture */
	CHECK(!ae_scope_mask_fills_view(1, 1, 0.0f, 0.0f, 0, 0));
	{
		float x0, x1, y0, y1;

		/* a picture in 0-1 (a 2D texture): five times its whole about its middle */
		ae_scope_mask_clip(0, 2048, 512, &x0, &x1, &y0, &y1);
		CHECK(x0 == -2.0f && x1 == 3.0f && y0 == -2.0f && y1 == 3.0f);
		CHECK(x1 - x0 == 5.0f && (x0 + x1) * 0.5f == 0.5f);
		/* an interface bitmap, in texels: the same about its middle */
		ae_scope_mask_clip(1, 2048, 512, &x0, &x1, &y0, &y1);
		CHECK(x0 == -4096.0f && x1 == 6144.0f && y0 == -1024.0f && y1 == 1536.0f);
		CHECK((x0 + x1) * 0.5f == 1024.0f && (y0 + y1) * 0.5f == 256.0f);
	}
	if (failures)
		return 1;
	printf("ae_scope_mask_rules_test: ok\n");
	return 0;
}
