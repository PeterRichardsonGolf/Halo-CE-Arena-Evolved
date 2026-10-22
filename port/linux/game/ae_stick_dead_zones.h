/*
AE_STICK_DEAD_ZONES.H

STICK DEADZONES (input.left_stick_deadzone, input.right_stick_deadzone;
Settings > Gamepads): each stick's dead zone as a percent of its throw, in
place of the game's own (input_xbox.c's GAMEPAD_STICK_DEAD_RANGE, 9000 of
32767 on each axis: about 27%). DEFAULT (0) leaves the game's: nothing is
done. The stick's values are worked out again from the raw ones with the
game's own fix_dead_zone, so the curve past the dead zone is the game's.
*/

#ifndef AE_STICK_DEAD_ZONES_H
#define AE_STICK_DEAD_ZONES_H

/* (input_xbox.c, after the game's dead zone: an AE hook line) a pad's sticks (left, right), given the raw axes */
void ae_stick_dead_zones(short gamepad_index, union point2d *sticks, short left_x, short left_y, short right_x,
	short right_y);
/* a setting's percent as the dead range on an axis (0 to 32767); 0 for DEFAULT, the game's (pure) */
short ae_stick_dead_range(long percent);

#endif
