#include <math.h>
#include <stdio.h>
#include "ae_party_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

int main(void)
{
	/* b_tap_is_back: released within 250 ms */
	CHECK(ae_b_gesture_step(100, 1, 1) == AE_B_NONE);
	CHECK(ae_b_gesture_step(100, 0, 1) == AE_B_BACK);
	CHECK(ae_b_gesture_step(250, 0, 1) == AE_B_BACK);
	CHECK(ae_b_gesture_step(250, 0, 0) == AE_B_BACK);
	/* b_hold_arms_after_250: held past 250 ms on a card that can go */
	CHECK(ae_b_gesture_step(251, 1, 1) == AE_B_HOLDING);
	CHECK(ae_b_gesture_step(1049, 1, 1) == AE_B_HOLDING);
	/* b_hold_removes_at_1050 */
	CHECK(ae_b_gesture_step(1050, 1, 1) == AE_B_REMOVE);
	CHECK(ae_b_gesture_step(1400, 1, 1) == AE_B_REMOVE);
	/* b_release_while_holding_cancels: no Back */
	CHECK(ae_b_gesture_step(600, 0, 1) == AE_B_NONE);
	/* b_hold_player1_does_nothing: no REMOVE to arm there, so a hold arms nothing and its release is Back (a slow
	tap stays a tap) */
	CHECK(ae_b_gesture_step(600, 1, 0) == AE_B_NONE);
	CHECK(ae_b_gesture_step(2000, 1, 0) == AE_B_NONE);
	CHECK(ae_b_gesture_step(600, 0, 0) == AE_B_BACK);
	/* b_fill_linear: 0 before 250 ms, 1 at 1050 ms */
	CHECK(ae_b_hold_fill(0) == 0.0f);
	CHECK(ae_b_hold_fill(250) == 0.0f);
	CHECK(fabsf(ae_b_hold_fill(650) - 0.5f) < 1e-6f);
	CHECK(ae_b_hold_fill(1050) == 1.0f);
	CHECK(ae_b_hold_fill(5000) == 1.0f);
	if (failures)
		printf("%d failures\n", failures);
	else
		printf("ok\n");
	return failures ? 1 : 0;
}
