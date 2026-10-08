/*
AE_PLATFORM.C

What AE's menus read from the platform layer (ae_platform.h): the keys the
game's menu keys leave out or use otherwise (Q and E for tabs, Tab and
Shift+Tab as focus steps, Page Up and Page Down), Tab's presses (counted by
an SDL event watch, so a tap within one frame counts), mouse button 4's
presses, and the device the player last used. It reads the state sdl_platform.c
already keeps (platform_input_read, without taking the mouse's motion) and
xinput_sdl.c's platform_input_scheme. (Button 4's count is ae_back_presses.c,
pure, so the unit tests build it.)
*/

#include "platform.h"
#include "sdl_platform.h"
#include "ae_platform.h"

#if !defined(HALO_SERVER) && !defined(HALO_ANDROID)
#include <SDL3/SDL_atomic.h>
#include <SDL3/SDL_events.h>
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

#if !defined(HALO_SERVER) && !defined(HALO_ANDROID)
/* (atomics: SDL may call a watch from the thread that queues the event; the game's thread pumps them here) */
static SDL_AtomicInt tab_armed, tab_forward, tab_backward, tab_ignored;

static bool SDLCALL tab_watch(
	void *userdata,
	SDL_Event *event)
{
	(void)userdata;
	/* (Alt is either Alt key: right Alt+Tab switches windows on most desktops too; an AltGr that SDL reports as
	right Alt (some layouts) is taken as Alt, AltGr+Tab then stepping nothing, which costs nothing real) */
	if (event->type == SDL_EVENT_KEY_DOWN && event->key.scancode == SDL_SCANCODE_TAB && SDL_GetAtomicInt(&tab_armed))
	{
		switch (ae_platform_tab_press_kind(event->key.repeat, (event->key.mod & SDL_KMOD_SHIFT) != 0,
			(event->key.mod & SDL_KMOD_ALT) != 0, (event->key.mod & SDL_KMOD_GUI) != 0))
		{
		case AE_TAB_PRESS_FORWARD: SDL_AddAtomicInt(&tab_forward, 1); break;
		case AE_TAB_PRESS_BACKWARD: SDL_AddAtomicInt(&tab_backward, 1); break;
		case AE_TAB_PRESS_IGNORED: SDL_AddAtomicInt(&tab_ignored, 1); break;
		}
	}
	/* (a watch's answer is ignored: the event is queued as ever) */
	return true;
}

void ae_platform_arm_tab_presses(
	int armed)
{
	static int watching;

	if (armed && !watching)
		watching = SDL_AddEventWatch(tab_watch, NULL) ? 1 : 0;
	SDL_SetAtomicInt(&tab_armed, armed != 0);
	if (!armed)
	{
		SDL_SetAtomicInt(&tab_forward, 0);
		SDL_SetAtomicInt(&tab_backward, 0);
		SDL_SetAtomicInt(&tab_ignored, 0);
	}
}

void ae_platform_take_tab_presses(
	int *forward,
	int *backward,
	int *ignored)
{
	*forward = SDL_SetAtomicInt(&tab_forward, 0);
	*backward = SDL_SetAtomicInt(&tab_backward, 0);
	*ignored = SDL_SetAtomicInt(&tab_ignored, 0);
}
#else
void ae_platform_arm_tab_presses(
	int armed)
{
	(void)armed;
}

void ae_platform_take_tab_presses(
	int *forward,
	int *backward,
	int *ignored)
{
	*forward = *backward = *ignored = 0;
}
#endif

int ae_platform_input_scheme(void)
{
#ifdef HALO_GAME_BROWSER
	return platform_input_scheme();
#else
	return 1;
#endif
}
