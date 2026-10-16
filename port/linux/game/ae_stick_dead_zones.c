/*
AE_STICK_DEAD_ZONES.C

STICK DEADZONES (ae_stick_dead_zones.h).
*/

#include "cseries.h"
#include "math/integer_math.h"
#include "input/input.h"
#include "../src/port_config.h"

#include "ae_stick_dead_zones.h"

short ae_stick_dead_range(long percent)
{
	if (percent <= 0)
		return 0;
	if (percent > 90)
		percent = 90;
	return (short)((percent * SHORT_MAX + 50) / 100);
}

void ae_stick_dead_zones(union point2d *sticks, short left_x, short left_y, short right_x, short right_y)
{
	/* (read again only when a setting has changed: this runs for every pad each frame) */
	static unsigned long changes_seen = (unsigned long)-1;
	static short left = 0, right = 0;

	if (config_changes() != changes_seen)
	{
		changes_seen = config_changes();
		left = ae_stick_dead_range(config_integer("input.left_stick_deadzone"));
		right = ae_stick_dead_range(config_integer("input.right_stick_deadzone"));
	}
	if (left)
	{
		sticks[_gamepad_stick_left].x = fix_dead_zone(left_x, left);
		sticks[_gamepad_stick_left].y = fix_dead_zone(left_y, left);
	}
	if (right)
	{
		sticks[_gamepad_stick_right].x = fix_dead_zone(right_x, right);
		sticks[_gamepad_stick_right].y = fix_dead_zone(right_y, right);
	}
}
