/*
AE_HIJACK_RULES.C

See ae_hijack_rules.h.
*/

#include "ae_hijack_rules.h"
#include "ae_variant_ext_rules.h"

int ae_hijack_allowed(int mode, int same_player, int enemies)
{
	if (!AE_HIJACK_SHIPPED || same_player)
		return 0;
	if (mode == AE_EXT_HIJACK_ENEMIES)
		return enemies != 0;
	return mode == AE_EXT_HIJACK_ANYONE;
}

int ae_hijack_speed_ok(float relative_speed)
{
	return relative_speed <= AE_HIJACK_SPEED_MOST;
}

int ae_hijack_mixed_riders(int mixed_mode, int taking_seat)
{
	return taking_seat != 0 || mixed_mode == AE_EXT_MIXED_VEHICLES_ON;
}

int ae_hijack_seat_rank(int seat_state, int hijack_seat, int available_state)
{
	return hijack_seat ? available_state : seat_state;
}
