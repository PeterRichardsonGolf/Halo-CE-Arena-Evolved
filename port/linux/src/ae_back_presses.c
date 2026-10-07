/*
AE_BACK_PRESSES.C

Mouse button 4's presses for AE's menus (ae_platform.h): counted by
sdl_platform.c's pointer branch (an AE hook line), only while the game's side
has armed the count, that is while an AE screen is open. Upstream's own menus
then never leave a count behind for the next AE screen, and with
display.arena_menus off the count is never armed. Pure C (no SDL), so the unit
tests build it (port/linux/tests/ae_input_rules_test.c). Both sides run on the
game's thread: sdl_platform.c's events are pumped from XInputGetState and
Present.
*/

#include "ae_platform.h"

static int back_presses_armed;
static int back_presses;

void ae_platform_count_back_press(void)
{
	if (back_presses_armed)
		back_presses++;
}

void ae_platform_arm_back_presses(int armed)
{
	back_presses_armed = armed != 0;
	if (!back_presses_armed)
		back_presses = 0;
}

int ae_platform_take_back_presses(void)
{
	int presses = back_presses;

	back_presses = 0;
	return presses;
}
