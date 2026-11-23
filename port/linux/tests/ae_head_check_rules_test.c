#include <stdio.h>

#include "ae_head_check_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

int main(void)
{
	/* a target the host had at (10, 0, 0) when the shooter saw it, its head 0.6 above its origin now */
	struct ae_head_check_point seen = { 10.f, 0.f, 0.f }, offset = { 0.f, 0.f, 0.6f };
	struct ae_head_check_point on_head = { 10.f, 0.05f, 0.62f }, edge = { 10.f, 0.f, 0.71f };
	struct ae_head_check_point chest = { 10.f, 0.f, 0.4f }, beside = { 10.f, 0.2f, 0.6f };
	struct ae_head_check_point not_a_number;
	int head[4], type[4];

	/* on the head, at the reach's edge, off it below and beside */
	CHECK(ae_head_check_on_head(&on_head, &seen, &offset));
	CHECK(ae_head_check_on_head(&edge, &seen, &offset));
	CHECK(!ae_head_check_on_head(&chest, &seen, &offset));
	CHECK(!ae_head_check_on_head(&beside, &seen, &offset));
	/* a point that is not a number is not on the head */
	not_a_number.x = 0.f / 0.f;
	not_a_number.y = 0.f;
	not_a_number.z = 0.6f;
	CHECK(!ae_head_check_on_head(&not_a_number, &seen, &offset));
	/* a span for the crouch the host cannot know: a standing target now (the head may have been 0.27 lower), a
	crouched one (0.27 higher); beside the head stays off it, and the span is not a number-proof hole */
	{
		struct ae_head_check_point was_crouched = { 10.f, 0.02f, 0.36f }, was_standing = { 10.f, 0.f, 0.86f };
		struct ae_head_check_point far_below = { 10.f, 0.f, 0.15f };

		CHECK(!ae_head_check_on_head(&was_crouched, &seen, &offset));
		CHECK(ae_head_check_on_head_span(&was_crouched, &seen, &offset, 0.27f, 0.f));
		CHECK(!ae_head_check_on_head_span(&was_standing, &seen, &offset, 0.27f, 0.f));
		CHECK(ae_head_check_on_head_span(&was_standing, &seen, &offset, 0.f, 0.27f));
		CHECK(!ae_head_check_on_head_span(&far_below, &seen, &offset, 0.27f, 0.f));
		CHECK(!ae_head_check_on_head_span(&beside, &seen, &offset, 0.27f, 0.27f));
		CHECK(ae_head_check_on_head_span(&on_head, &seen, &offset, -1.f, 0.f / 0.f));
		CHECK(!ae_head_check_on_head_span(&not_a_number, &seen, &offset, 0.27f, 0.27f));
	}

	/* materials: 0 head (flesh), 1 body (flesh), 2 armour (metal), 3 head (metal) */
	head[0] = 1, type[0] = 5;
	head[1] = 0, type[1] = 5;
	head[2] = 0, type[2] = 9;
	head[3] = 1, type[3] = 9;
	/* the body material of the head's type first */
	CHECK(ae_head_check_body_material(4, head, type, 0, -1) == 1);
	CHECK(ae_head_check_body_material(4, head, type, 3, 1) == 2);
	/* none of its type: the indirect material, if not a head one, else the first that is not */
	type[2] = 7;
	CHECK(ae_head_check_body_material(4, head, type, 3, 1) == 1);
	CHECK(ae_head_check_body_material(4, head, type, 3, 0) == 1);
	CHECK(ae_head_check_body_material(4, head, type, 3, 2) == 2);
	/* every material a head one: none */
	head[1] = 1, head[2] = 1;
	CHECK(ae_head_check_body_material(4, head, type, 0, -1) == -1);
	/* a head material out of range still finds a body one */
	head[2] = 0;
	CHECK(ae_head_check_body_material(4, head, type, 9, -1) == 2);
	CHECK(ae_head_check_body_material(0, head, type, 0, -1) == -1);

	if (failures)
		return 1;
	printf("ae_head_check_rules: ok\n");
	return 0;
}
