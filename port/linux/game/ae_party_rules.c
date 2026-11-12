/*
AE_PARTY_RULES.C

See ae_party_rules.h.
*/

#include "ae_party_rules.h"

int ae_b_gesture_step(unsigned long down_ms, int still_down, int can_remove)
{
	if (down_ms <= AE_B_TAP_MS)
		return still_down ? AE_B_NONE : AE_B_BACK;
	/* (nothing to remove here: no hold to arm, the release is Back) */
	if (!can_remove)
		return still_down ? AE_B_NONE : AE_B_BACK;
	if (!still_down)
		return AE_B_NONE;
	return down_ms >= AE_B_TAP_MS + AE_B_HOLD_MS ? AE_B_REMOVE : AE_B_HOLDING;
}

float ae_b_hold_fill(unsigned long down_ms)
{
	if (down_ms <= AE_B_TAP_MS)
		return 0.0f;
	if (down_ms >= AE_B_TAP_MS + AE_B_HOLD_MS)
		return 1.0f;
	return (float)(down_ms - AE_B_TAP_MS) / (float)AE_B_HOLD_MS;
}
