/*
AE_PLATFORM.C

What AE's menus read from the platform layer (ae_platform.h): the keys the
game's menu keys leave out (Q and E for tabs, Page Up and Page Down), and the
device the player last used. New file, so no upstream code changes: it reads
the state sdl_platform.c already keeps (platform_input_read, without taking
the mouse's motion) and xinput_sdl.c's platform_input_scheme.
*/

#include "platform.h"
#include "sdl_platform.h"
#include "ae_platform.h"

#ifdef HALO_GAME_BROWSER
/* (xinput_sdl.c, with the game browser; the dedicated server has its own) */
int platform_input_scheme(void);
#endif

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
	return keys;
}

int ae_platform_input_scheme(void)
{
#ifdef HALO_GAME_BROWSER
	return platform_input_scheme();
#else
	return 1;
#endif
}
