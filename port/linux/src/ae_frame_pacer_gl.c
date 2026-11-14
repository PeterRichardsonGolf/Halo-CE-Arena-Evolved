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
swap returned at now_ns after swap_ns, real_vsync as above; whether BOOST is working */
void ae_frame_pacer_note_frame(unsigned long long now_ns, float gpu_ms);
void ae_frame_pacer_note_swap(unsigned long long now_ns, unsigned long long swap_ns, int real_vsync);
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

void ae_frame_pacer_swap_end(int real_vsync)
{
	Uint64 now = SDL_GetTicksNS();

	ae_frame_pacer_note_swap(now, now - swap_began_ns, real_vsync);
}

long ae_frame_pacer_frames_in_flight(long configured)
{
	return ae_frame_pacer_boosting() ? 1 : configured;
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
