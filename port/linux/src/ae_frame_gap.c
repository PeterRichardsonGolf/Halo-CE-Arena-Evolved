/*
AE_FRAME_GAP.C

A debug line for the menus' pauses (owner 2026-11-01: "a slight delay/freeze when you select things on the ae menu
that used to not be there on linux and windows"): a frame shown 50 ms or more after the one before logs its gap and
the graphics driver's share of it, so a TEST RUN's log puts the pause beside the menu line it followed. A map's load
logs one too.
*/

#include "ae_platform.h"

void platform_log(char const *format, ...);

#define AE_FRAME_GAP_MS 50ull
/* (past this a pause is the game held elsewhere: a debugger, a sleep) */
#define AE_FRAME_GAP_IGNORED_MS 30000ull

void ae_frame_gap_note(unsigned long long now_ms, unsigned long long swap_ms)
{
	static unsigned long long last_ms;
	unsigned long long gap = last_ms && now_ms >= last_ms ? now_ms - last_ms : 0;

	last_ms = now_ms;
	if (gap >= AE_FRAME_GAP_MS && gap < AE_FRAME_GAP_IGNORED_MS)
		platform_log("display: a frame %llu ms after the last (%llu ms of it showing it)", gap, swap_ms);
}
