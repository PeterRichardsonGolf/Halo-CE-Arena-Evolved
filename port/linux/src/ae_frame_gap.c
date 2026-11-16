/*
AE_FRAME_GAP.C

A debug line for the menus' pauses (owner 2026-11-01: "a slight delay/freeze when you select things on the ae menu
that used to not be there on linux and windows"): a frame shown 50 ms or more after the one before logs its gap and
the graphics driver's share of it, so a TEST RUN's log puts the pause beside the menu line it followed. A map's load
logs one too.

(0.9.3) debug.frame_summary: every 10 s of frames, one line: how many a second, the middle and the longest gap, how
many over 25 ms, and display.frames_in_flight (a TEST RUN's A/B of a setting reads its cost from the log).
*/

#include "ae_platform.h"

void platform_log(char const *format, ...);
int config_boolean(const char *name);
long config_integer(const char *name);
unsigned long config_changes(void);

#define AE_FRAME_GAP_MS 50ull
/* (past this a pause is the game held elsewhere: a debugger, a sleep) */
#define AE_FRAME_GAP_IGNORED_MS 30000ull

#define SUMMARY_MS 10000ull
#define SUMMARY_SLOW_MS 25ull
/* (the gaps kept for the middle: 10 s at up to 400 frames a second) */
#define SUMMARY_GAPS 4096

static struct
{
	unsigned long read_at;
	int read, on;
	unsigned long long start_ms;
	unsigned short gaps[SUMMARY_GAPS];
	unsigned long count, slow;
	unsigned long long longest;
} summary;

/* the middle gap (counting sort over 0..255 ms and more) */
static unsigned long long summary_middle(void)
{
	unsigned long counts[257] = { 0 }, seen = 0, kept = summary.count < SUMMARY_GAPS ? summary.count : SUMMARY_GAPS;
	unsigned long index;

	for (index = 0; index < kept; index++)
		counts[summary.gaps[index] > 256 ? 256 : summary.gaps[index]]++;
	for (index = 0; index < 257; index++)
	{
		seen += counts[index];
		if (seen * 2 >= kept)
			return index;
	}
	return 0;
}

static void summary_note(unsigned long long now_ms, unsigned long long gap)
{
	if (!summary.read || summary.read_at != config_changes())
	{
		summary.read = 1;
		summary.read_at = config_changes();
		summary.on = config_boolean("debug.frame_summary");
		if (!summary.on)
			summary.start_ms = 0;
	}
	if (!summary.on)
		return;
	if (!summary.start_ms || !gap || gap >= AE_FRAME_GAP_IGNORED_MS)
	{
		/* (the first frame, or a pause: a summary starts here) */
		summary.start_ms = now_ms;
		summary.count = summary.slow = 0;
		summary.longest = 0;
		return;
	}
	if (summary.count < SUMMARY_GAPS)
		summary.gaps[summary.count] = (unsigned short)(gap > 65535ull ? 65535ull : gap);
	summary.count++;
	if (gap > SUMMARY_SLOW_MS)
		summary.slow++;
	if (gap > summary.longest)
		summary.longest = gap;
	if (now_ms - summary.start_ms >= SUMMARY_MS)
	{
		unsigned long long span = now_ms - summary.start_ms;

		platform_log("frames: %.1f a second over %.1f s, middle %llu ms, longest %llu ms, %lu over %llu ms "
			"(display.frames_in_flight %ld)", (double)summary.count * 1000.0 / (double)span, (double)span / 1000.0,
			summary_middle(), summary.longest, summary.slow, SUMMARY_SLOW_MS,
			config_integer("display.frames_in_flight"));
		summary.start_ms = now_ms;
		summary.count = summary.slow = 0;
		summary.longest = 0;
	}
}

void ae_frame_gap_note(unsigned long long now_ms, unsigned long long swap_ms)
{
	static unsigned long long last_ms;
	unsigned long long gap = last_ms && now_ms >= last_ms ? now_ms - last_ms : 0;

	last_ms = now_ms;
	summary_note(now_ms, gap);
	if (gap >= AE_FRAME_GAP_MS && gap < AE_FRAME_GAP_IGNORED_MS)
		platform_log("display: a frame %llu ms after the last (%llu ms of it showing it)", gap, swap_ms);
}
