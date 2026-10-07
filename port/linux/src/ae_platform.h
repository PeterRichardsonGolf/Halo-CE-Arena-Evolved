/*
AE_PLATFORM.H

What AE's menus read from the platform layer themselves (ae_platform.c).
Plain types only: the game's units call it.
*/

#ifndef __AE_PLATFORM_H
#define __AE_PLATFORM_H

/* keys AE reads itself: the game's menu keys (xinput_sdl.c keyboard_gamepad, driving the first controller) have no
Q, Page Up or Page Down, and make E X and Tab Y, where AE's menus make E and Q tabs and Tab a focus step */
enum
{
	AE_KEY_Q = 1 << 0,
	AE_KEY_E = 1 << 1,
	AE_KEY_PAGE_UP = 1 << 2,
	AE_KEY_PAGE_DOWN = 1 << 3,
	AE_KEY_TAB = 1 << 4,
	AE_KEY_SHIFT = 1 << 5,
	/* the numeric keypad's arrows with Num Lock off (the game's menu keys take only the arrow keys) */
	AE_KEY_UP = 1 << 6,
	AE_KEY_DOWN = 1 << 7,
	AE_KEY_LEFT = 1 << 8,
	AE_KEY_RIGHT = 1 << 9
};

/* the AE_KEY_* held now (0 without a focused window). SDL reports the keys where they are, so the numeric keypad's
Page Up, Page Down and arrows are its 9, 3, 8, 2, 4 and 6: with Num Lock off they count as those keys (Page Up and
Page Down as AE_KEY_PAGE_UP / _DOWN, the arrows as AE_KEY_UP...); its Enter is the game's Enter already (A) */
int ae_platform_keys(void);
/* mouse button 4 (back) presses while the menus have the pointer (ae_back_presses.c): sdl_platform.c counts them
(an AE hook line, since its pointer branch keeps them from the controller), but only while armed: the game's side
arms the count while an AE screen is open (never with display.arena_menus off) and takes the presses (the count
starts over); disarming drops what was counted */
void ae_platform_count_back_press(void);
void ae_platform_arm_back_presses(int armed);
int ae_platform_take_back_presses(void);
/* the device the player last used: 0 the keyboard (or mouse), 1 an Xbox-like pad, 2 a PlayStation pad, 3 a
Nintendo pad (1 without the game browser, whose prompts it is) */
int ae_platform_input_scheme(void);

#endif
