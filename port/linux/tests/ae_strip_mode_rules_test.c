#include <stdio.h>
#include "ae_strip_mode_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

int main(void)
{
	/* slayer family */
	CHECK(ae_strip_mode_for_slayer(FALSE, FALSE, 0) == _ae_strip_slayer);
	CHECK(ae_strip_mode_for_slayer(FALSE, FALSE, 1) == _ae_strip_elimination);
	CHECK(ae_strip_mode_for_slayer(FALSE, TRUE, 0) == _ae_strip_phantoms);
	CHECK(ae_strip_mode_for_slayer(FALSE, TRUE, 3) == _ae_strip_phantoms);
	/* a round mode is the rounds strip whatever the lives or kill order say */
	CHECK(ae_strip_mode_for_slayer(TRUE, FALSE, 0) == _ae_strip_rounds);
	CHECK(ae_strip_mode_for_slayer(TRUE, FALSE, 1) == _ae_strip_rounds);
	CHECK(ae_strip_mode_for_slayer(TRUE, TRUE, 2) == _ae_strip_rounds);
	/* ball and race */
	CHECK(ae_strip_mode_for_ball(FALSE, FALSE) == _ae_strip_oddball);
	CHECK(ae_strip_mode_for_ball(TRUE, FALSE) == _ae_strip_juggernaut);
	CHECK(ae_strip_mode_for_ball(FALSE, TRUE) == _ae_strip_accumulation);
	CHECK(ae_strip_mode_for_race(FALSE) == _ae_strip_race);
	CHECK(ae_strip_mode_for_race(TRUE) == _ae_strip_rally);
	if (!failures)
		printf("ae_strip_mode_rules_test: ok\n");
	return failures != 0;
}
