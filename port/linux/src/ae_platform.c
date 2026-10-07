/*
AE_PLATFORM.C

What AE's menus read from the platform layer (ae_platform.h): the keys the
game's menu keys leave out or use otherwise (Q and E for tabs, Tab and
Shift+Tab as focus steps, Page Up and Page Down), mouse button 4's presses,
and the device the player last used. It reads the state sdl_platform.c
already keeps (platform_input_read, without taking the mouse's motion) and
xinput_sdl.c's platform_input_scheme. (Button 4's count is ae_back_presses.c,
pure, so the unit tests build it.)
*/

#include "platform.h"
#include "sdl_platform.h"
#include "ae_platform.h"

#if !defined(HALO_SERVER) && !defined(HALO_ANDROID)
#include <SDL3/SDL_keyboard.h>
#endif

#ifdef HALO_GAME_BROWSER
/* (xinput_sdl.c, with the game browser; the dedicated server has its own) */
int platform_input_scheme(void);
#endif

/* whether Num Lock is on. The dedicated server has no keyboard, and Android's guest has no SDL_GetModState (its SDL
is port/android/guest/runtime/guest_sdl.c's few functions): on there, so the keypad is digits */
static int num_lock(void)
{
#if defined(HALO_SERVER) || defined(HALO_ANDROID)
	return 1;
#else
	return (SDL_GetModState() & SDL_KMOD_NUM) != 0;
#endif
}

/* the numeric keypad's keys as AE's, with Num Lock off (SDL scancodes are where the keys are: KP_9 is the keypad's
Page Up whether or not Num Lock makes it a 9) */
static int keypad_keys(
	unsigned char const *keys,
	int numbers)
{
	int result = 0;

	if (numbers)
		return 0;
	if (keys[SDL_SCANCODE_KP_9])
		result |= AE_KEY_PAGE_UP;
	if (keys[SDL_SCANCODE_KP_3])
		result |= AE_KEY_PAGE_DOWN;
	if (keys[SDL_SCANCODE_KP_8])
		result |= AE_KEY_UP;
	if (keys[SDL_SCANCODE_KP_2])
		result |= AE_KEY_DOWN;
	if (keys[SDL_SCANCODE_KP_4])
		result |= AE_KEY_LEFT;
	if (keys[SDL_SCANCODE_KP_6])
		result |= AE_KEY_RIGHT;
	return result;
}

int ae_platform_keys(void)
{
	struct platform_input_state input;
	int keys = 0;

	platform_input_read(&input, FALSE);
	if (!input.focused)
		return 0;
	if (input.keys[SDL_SCANCODE_Q])
		keys |= AE_KEY_Q;
	if (input.keys[SDL_SCANCODE_E])
		keys |= AE_KEY_E;
	if (input.keys[SDL_SCANCODE_PAGEUP])
		keys |= AE_KEY_PAGE_UP;
	if (input.keys[SDL_SCANCODE_PAGEDOWN])
		keys |= AE_KEY_PAGE_DOWN;
	if (input.keys[SDL_SCANCODE_TAB])
		keys |= AE_KEY_TAB;
	if (input.keys[SDL_SCANCODE_LSHIFT] || input.keys[SDL_SCANCODE_RSHIFT])
		keys |= AE_KEY_SHIFT;
	return keys | keypad_keys(input.keys, num_lock());
}

int ae_platform_input_scheme(void)
{
#ifdef HALO_GAME_BROWSER
	return platform_input_scheme();
#else
	return 1;
#endif
}
