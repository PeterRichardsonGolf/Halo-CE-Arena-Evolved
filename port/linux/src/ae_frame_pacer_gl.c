/*
AE_FRAME_PACER_GL.C

LOW LATENCY in the platform (ae_frame_pacer_gl.h): the clock, the sleep, the swap's two calls.
*/

#include "ae_frame_pacer_gl.h"

#if defined(HALO_ANDROID) || defined(HALO_WEB) || defined(HALO_SERVER) /* (the dedicated server: no SDL, no swap: no pacing) */

void ae_frame_pacer_swap_begin(void)
{
}

void ae_frame_pacer_swap_end(int real_vsync)
{
	(void)real_vsync;
}

long ae_frame_pacer_frames_in_flight(long configured)
{
	return configured;
}

int ae_frame_pacer_platform(void)
{
	return 0;
}

/* (no pacing here, so the frames-in-flight limit never changes: no fence is ever past it) */
void ae_frame_pacer_fences_fit(struct __GLsync **fences, long count, long limit, long *next)
{
	(void)fences;
	(void)count;
	(void)limit;
	(void)next;
}

unsigned long long ae_frame_pacer_ticks_ns(void)
{
	return 0;
}

void ae_frame_pacer_sleep_until(unsigned long long until_ns, unsigned long long spin_ns)
{
	(void)until_ns;
	(void)spin_ns;
}

void ae_frame_pacer_restart_gpu_timing(void)
{
}

#else

#include "xgpu.h"

#include <SDL3/SDL.h>

/* (port/linux/game/ae_frame_pacer.c) a frame's work ended at now_ns with gpu_ms of GPU time (below 0: not known); a
swap returned at now_ns after swap_ns, real_vsync as above, the display mode refreshing every display_ms (0: not known);
whether BOOST is working */
void ae_frame_pacer_note_frame(unsigned long long now_ns, float gpu_ms);
void ae_frame_pacer_note_swap(unsigned long long now_ns, unsigned long long swap_ns, int real_vsync, float display_ms);
int ae_frame_pacer_boosting(void);
/* (ae_dynamic_resolution_gl.c) the GPU's time for a frame, ms, below 0 when it is not known */
float ae_dynamic_resolution_gpu_ms(void);
void ae_dynamic_resolution_gpu_restart(void);

static Uint64 swap_began_ns;

int ae_frame_pacer_platform(void)
{
	return 1;
}

unsigned long long ae_frame_pacer_ticks_ns(void)
{
	return SDL_GetTicksNS();
}

/* sleeps until until_ns, the last spin_ns of it spinning on the clock (a sleep wakes a little late) */
void ae_frame_pacer_sleep_until(unsigned long long until_ns, unsigned long long spin_ns)
{
	Uint64 now = SDL_GetTicksNS();

	if (now >= until_ns)
		return;
	if (until_ns - now > spin_ns)
		SDL_DelayPrecise(until_ns - spin_ns - now);
	while (SDL_GetTicksNS() < until_ns)
		;
}

void ae_frame_pacer_restart_gpu_timing(void)
{
	ae_dynamic_resolution_gpu_restart();
}

void ae_frame_pacer_swap_begin(void)
{
	swap_began_ns = SDL_GetTicksNS();
	ae_frame_pacer_note_frame(swap_began_ns, ae_dynamic_resolution_gpu_ms());
#ifndef HALO_GLES
	/* (BOOST: the driver gets the frame's commands now, not when it chooses) */
	if (ae_frame_pacer_boosting())
		glFlush();
#endif
}

/* the window's display mode's refresh interval in ms, 0 when it gives none (read a few times a second: the window can
move to another display) */
static float display_ms(Uint64 now)
{
	static Uint64 read_at;
	static float interval_ms;

	if (!read_at || now - read_at >= 250000000ull)
	{
		SDL_Window *window = SDL_GL_GetCurrentWindow();
		SDL_DisplayID display = window ? SDL_GetDisplayForWindow(window) : 0;
		const SDL_DisplayMode *mode = display ? SDL_GetCurrentDisplayMode(display) : NULL;

		/* (20 to 500 Hz: anything else is not a refresh rate to check a period by) */
		interval_ms = mode && mode->refresh_rate >= 20.0f && mode->refresh_rate <= 500.0f ? 1000.0f / mode->refresh_rate :
			0.0f;
		read_at = now ? now : 1;
	}
	return interval_ms;
}

void ae_frame_pacer_swap_end(int real_vsync)
{
	Uint64 now = SDL_GetTicksNS();

	ae_frame_pacer_note_swap(now, now - swap_began_ns, real_vsync, display_ms(now));
}

long config_integer(const char *name);
unsigned long config_changes(void);

/* (d3d8_gl.c's MAXIMUM_FRAMES_IN_FLIGHT: the fences it has room for) */
#define FRAMES_IN_FLIGHT_MOST 8

long ae_frame_pacer_frames_in_flight(long configured)
{
	/* display.frames_in_flight read again when a setting changes, not only at the start (configured is the start's),
	so a TEST RUN or the console can change it in a game; ae_frame_pacer_fences_fit drops the fences past a lower one */
	static unsigned long read_at = (unsigned long)-1;
	static long live;

	if (read_at != config_changes())
	{
		read_at = config_changes();
		live = config_integer("display.frames_in_flight");
		if (live < 0)
			live = 0;
		if (live > FRAMES_IN_FLIGHT_MOST)
			live = FRAMES_IN_FLIGHT_MOST;
	}
	(void)configured;
	return ae_frame_pacer_boosting() ? 1 : live;
}

void ae_frame_pacer_fences_fit(struct __GLsync **fences, long count, long limit, long *next)
{
	long index;

	/* (the fences past the limit, from before BOOST changed it, are dropped: nothing waits on them now) */
	for (index = limit < 0 ? 0 : limit; index < count; index++)
	{
		if (fences[index])
		{
			glDeleteSync(fences[index]);
			fences[index] = NULL;
		}
	}
	if (limit > 0 && *next >= limit)
		*next = 0;
}

#endif
