#include <stdio.h>

#include "ae_plane_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

int main(void)
{
	float zero = 0.0f;
	float nan = zero / zero;
	float inf = 1.0f / zero;

	/* a valid normal: as it was */
	CHECK(ae_plane_unit_action(1.0f) == AE_PLANE_UNIT_OK);
	CHECK(ae_plane_unit_action(1.00005f) == AE_PLANE_UNIT_OK);
	CHECK(ae_plane_unit_action(0.99995f) == AE_PLANE_UNIT_OK);
	/* the planes of the map that met the assert (squared lengths 1.0019, 1.0018, 1.0014): rescaled */
	CHECK(ae_plane_unit_action(1.001921f) == AE_PLANE_UNIT_RESCALE);
	CHECK(ae_plane_unit_action(1.001829f) == AE_PLANE_UNIT_RESCALE);
	CHECK(ae_plane_unit_action(1.001356f) == AE_PLANE_UNIT_RESCALE);
	CHECK(ae_plane_unit_action(0.998f) == AE_PLANE_UNIT_RESCALE);
	CHECK(ae_plane_unit_action(1.5f) == AE_PLANE_UNIT_RESCALE);
	/* not a normal at all: left out */
	CHECK(ae_plane_unit_action(0.0f) == AE_PLANE_UNIT_UNUSABLE);
	CHECK(ae_plane_unit_action(-1.0f) == AE_PLANE_UNIT_UNUSABLE);
	CHECK(ae_plane_unit_action(100.0f) == AE_PLANE_UNIT_UNUSABLE);
	CHECK(ae_plane_unit_action(nan) == AE_PLANE_UNIT_UNUSABLE);
	CHECK(ae_plane_unit_action(inf) == AE_PLANE_UNIT_UNUSABLE);

	if (failures)
	{
		return 1;
	}
	printf("ae_plane_rules_test: ok\n");
	return 0;
}
