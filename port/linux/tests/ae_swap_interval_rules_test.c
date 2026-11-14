#include <stdio.h>

#include "ae_swap_interval_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

int main(void)
{
	/* the Deck's 60 Hz with a limit of 60: every refresh */
	CHECK(ae_swap_interval_in_step(60.0f, 60, 1) == 1);
	CHECK(ae_swap_interval_in_step(60.0f, 60, 0) == 1);
	/* within a frame a second: a refresh rate of 59.94 */
	CHECK(ae_swap_interval_in_step(59.94f, 60, 0) == 1);
	CHECK(ae_swap_interval_in_step(60.0f, 62, 1) == 0);
	/* a whole fraction: 30 at 60 Hz, 45 at 90 Hz, only with the x11 driver */
	CHECK(ae_swap_interval_in_step(60.0f, 30, 1) == 2);
	CHECK(ae_swap_interval_in_step(60.0f, 30, 0) == 0);
	CHECK(ae_swap_interval_in_step(90.0f, 45, 1) == 2);
	CHECK(ae_swap_interval_in_step(90.0f, 30, 1) == 3);
	CHECK(ae_swap_interval_in_step(60.0f, 15, 1) == 4);
	CHECK(ae_swap_interval_in_step(60.0f, 12, 1) == 0);
	/* a limit that is no fraction, no limit, no refresh rate known */
	CHECK(ae_swap_interval_in_step(60.0f, 50, 1) == 0);
	CHECK(ae_swap_interval_in_step(60.0f, 0, 1) == 0);
	CHECK(ae_swap_interval_in_step(60.0f, -1, 1) == 0);
	CHECK(ae_swap_interval_in_step(0.0f, 60, 1) == 0);
	if (failures)
		return 1;
	printf("ae_swap_interval_rules: ok\n");
	return 0;
}
