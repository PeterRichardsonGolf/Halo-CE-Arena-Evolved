#include <stdio.h>

#include "ae_hit_cover_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)
#define NEAR(a, b) ((a) - (b) < 0.0001f && (b) - (a) < 0.0001f)

int main(void)
{
	struct ae_hit_cover_point head = { 0.f, 0.f, 0.6f }, body = { 0.f, 0.f, 0.f }, centre = { 4.f, 0.f, 0.f };
	struct ae_hit_cover_point epicenter = { 0.f, 0.f, 0.f }, close = { 3.9f, 0.f, 0.f };
	struct ae_hit_cover_line lines[AE_HIT_COVER_MAXIMUM_LINES];
	int blocked[2];

	/* an explosion: from the epicenter 0.1 toward the centre, to the centre; and to 0.5 above it */
	CHECK(ae_hit_cover_lines(0, &head, &epicenter, &centre, lines) == 2);
	CHECK(NEAR(lines[0].from.x, 0.1f) && NEAR(lines[0].from.y, 0.f) && NEAR(lines[0].from.z, 0.f));
	CHECK(NEAR(lines[0].to.x, 4.f) && NEAR(lines[0].to.z, 0.f));
	CHECK(NEAR(lines[1].to.x, 4.f) && NEAR(lines[1].to.z, 0.5f));
	/* (the second line's start moves toward its own end: up a little too) */
	CHECK(lines[1].from.x > 0.09f && lines[1].from.x < 0.1f && lines[1].from.z > 0.f);

	/* an epicenter at the target: the start no further than half the way */
	CHECK(ae_hit_cover_lines(0, &head, &close, &centre, lines) == 2);
	CHECK(NEAR(lines[0].from.x, 3.95f));
	/* (on the centre itself: the start stays, the length is 0) */
	CHECK(ae_hit_cover_lines(0, &head, &centre, &centre, lines) == 2);
	CHECK(NEAR(lines[0].from.x, 4.f));

	/* a melee blow: from the head and from the body, both to the centre, unmoved */
	CHECK(ae_hit_cover_lines(1, &head, &body, &centre, lines) == 2);
	CHECK(NEAR(lines[0].from.z, 0.6f) && NEAR(lines[0].to.x, 4.f));
	CHECK(NEAR(lines[1].from.z, 0.f) && NEAR(lines[1].to.x, 4.f) && NEAR(lines[1].to.z, 0.f));

	/* the rule: refused only when every line is blocked, and the setting on */
	blocked[0] = 1;
	blocked[1] = 1;
	CHECK(ae_hit_cover_refused(1, 2, blocked));
	CHECK(!ae_hit_cover_refused(0, 2, blocked));
	blocked[1] = 0;
	CHECK(!ae_hit_cover_refused(1, 2, blocked));
	blocked[0] = 0;
	blocked[1] = 1;
	CHECK(!ae_hit_cover_refused(1, 2, blocked));
	CHECK(!ae_hit_cover_refused(1, 0, blocked));

	if (failures)
		return 1;
	printf("ae_hit_cover_rules: ok\n");
	return 0;
}
