/*
AE_SWAP_INTERVAL.C

A frame limit in step with the display under gamescope (ae_swap_interval.h).
*/

#include "ae_swap_interval.h"

#if defined(HALO_ANDROID) || defined(HALO_WEB) || defined(_WIN32) || defined(__APPLE__)

int ae_swap_interval_wanted(int vsync, int focused)
{
	return vsync && focused ? 1 : 0;
}

int ae_swap_interval_refused(int interval)
{
	(void)interval;
	return 0;
}

int ae_swap_interval_in_use(void)
{
	return 0;
}

#else

#include <SDL3/SDL.h>
#include <stdlib.h>
#include <string.h>


/* (port/linux/game/ae_swap_interval_rules.c) */
int ae_swap_interval_in_step(float refresh, long maximum, int x11);

long config_integer(const char *name);
unsigned long config_changes(void);
void platform_log(const char *format, ...);

/* the in step interval last wanted (0 none), the one the driver refused (0 none), and the refresh rate known */
static int in_step;
static int refused;
static float refresh_rate;
static Uint64 refresh_read_at;
static int refresh_known;

/* GAMESCOPE_WAYLAND_DISPLAY, which gamescope sets for the programs it runs */
static int under_gamescope(void)
{
	static int known = -1;

	if (known < 0)
	{
		const char *display = getenv("GAMESCOPE_WAYLAND_DISPLAY");

		known = display && display[0] ? 1 : 0;
	}
	return known;
}

static float refresh_rate_now(void)
{
	Uint64 now = SDL_GetTicks();

	if (!refresh_known || now - refresh_read_at >= 1000)
	{
		SDL_Window *window = SDL_GL_GetCurrentWindow();
		SDL_DisplayID display = window ? SDL_GetDisplayForWindow(window) : 0;
		const SDL_DisplayMode *mode = display ? SDL_GetCurrentDisplayMode(display) : NULL;

		refresh_rate = mode && mode->refresh_rate > 0.0f ? mode->refresh_rate : 0.0f;
		refresh_read_at = now;
		refresh_known = 1;
	}
	return refresh_rate;
}

/* display.max_fps, read again when a setting changes (this runs every frame) */
static long max_fps_now(void)
{
	static unsigned long read_at = (unsigned long)-1;
	static long maximum;

	if (read_at != config_changes())
	{
		read_at = config_changes();
		maximum = config_integer("display.max_fps");
	}
	return maximum;
}

int ae_swap_interval_wanted(int vsync, int focused)
{
	static int x11 = -1;
	static int logged;
	int interval;

	in_step = 0;
	if (!focused)
		return 0;
	if (vsync)
		return 1;
	if (!under_gamescope())
		return 0;
	if (x11 < 0)
	{
		const char *driver = SDL_GetCurrentVideoDriver();

		x11 = driver && !strcmp(driver, "x11") ? 1 : 0;
	}
	interval = ae_swap_interval_in_step(refresh_rate_now(), max_fps_now(), x11);
	if (interval && interval == refused)
		return 0;
	if (interval != refused)
		refused = 0;
	if (interval && interval != logged)
	{
		long maximum = max_fps_now();

		platform_log("frame rate: display.max_fps %ld shown in step with the display's %.2f Hz under gamescope, "
			"each frame for %d refresh%s", maximum, (double)refresh_rate_now(), interval, interval == 1 ? "" : "es");
	}
	logged = interval;
	in_step = interval;
	return interval;
}

int ae_swap_interval_refused(int interval)
{
	if (!in_step || interval != in_step)
		return 0;
	platform_log("frame rate: a swap interval of %d refused (%s): the clock paces the frames", interval,
		SDL_GetError());
	refused = interval;
	in_step = 0;
	return 1;
}

int ae_swap_interval_in_use(void)
{
	return in_step;
}

#endif
