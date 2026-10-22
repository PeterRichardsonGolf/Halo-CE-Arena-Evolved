/*
AE_STICK_DEAD_ZONES.C

STICK DEADZONES (ae_stick_dead_zones.h).
*/

#include "cseries.h"
#include "math/integer_math.h"
#include "input/input.h"
#include "../src/port_config.h"
#include "../src/ae_config_players.h"

#include "ae_stick_dead_zones.h"

short ae_stick_dead_range(long percent)
{
	if (percent <= 0)
		return 0;
	if (percent > 90)
		percent = 90;
	return (short)((percent * SHORT_MAX + 50) / 100);
}

void ae_stick_dead_zones(short gamepad_index, union point2d *sticks, short left_x, short left_y, short right_x,
	short right_y)
{
	/* (read again only when a setting has changed, for each player in turn: this runs for every pad each frame; the
	pad's player is its controller's, 0.3.7's per-player settings) */
	static unsigned long changes_seen = (unsigned long)-1;
	static short left = 0, right = 0;
	int previous = ae_config_player_enter(gamepad_index);

	if (ae_config_player_changes() != changes_seen)
	{
		changes_seen = ae_config_player_changes();
		left = ae_stick_dead_range(config_integer("input.left_stick_deadzone"));
		right = ae_stick_dead_range(config_integer("input.right_stick_deadzone"));
	}
	ae_config_player_leave(previous);
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
