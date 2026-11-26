#include "harness.h"
/* ae_announcer.c's line queue across an MCC announcer change: mcc_close frees the clips the queue points at, and the
loader thread then fills them again (samples before rate). The queue is emptied with the clips, and a clip with no
rate yet is not played (its length divides by the rate) */

#include "config.inc"
static int tones, bank_frees;
static unsigned long clock_ms = 10000;
static void ae_mcc_bank_free(short *samples) { bank_frees++; }
static void mcc_events(void) { }
static unsigned long system_milliseconds(void) { return clock_ms; }
static void platform_ui_tone_start(short *samples, unsigned long frames, unsigned long channels, unsigned long rate) { tones++; }
#include "under_test.inc"

static short samples[64];

int main(int argc, char **argv)
{
	char const *case_name = argv[1];

	memset(&announcer, 0, sizeof(announcer));
	CASE("close-empties-queue")
	{
		announcer.mcc_medals[3].samples = samples;
		announcer.mcc_medals[3].rate = 48000;
		announcer.mcc_medals[3].frames = 4800;
		announcer.queue[0] = &announcer.mcc_medals[3];
		announcer.queued = 1;
		mcc_close();
		CHECK(bank_frees == 1, "the clip not freed");
		CHECK(announcer.queued == 0, "%d lines still queued at clips just freed", announcer.queued);
		/* the loader has written the clip's samples, not yet its rate */
		announcer.mcc_medals[3].samples = samples;
		ae_announcer_update();
		CHECK(tones == 0, "a freed clip played");
		return 0;
	}
	CASE("no-rate")
	{
		announcer.mcc_sounds[2].samples = samples;
		announcer.mcc_sounds[2].frames = 4800;
		announcer.queue[0] = &announcer.mcc_sounds[2];
		announcer.queued = 1;
		ae_announcer_update();
		CHECK(tones == 0 && announcer.queued == 0, "a clip with no rate played");
		return 0;
	}
	CASE("plays")
	{
		announcer.mcc_sounds[2].samples = samples;
		announcer.mcc_sounds[2].frames = 4800;
		announcer.mcc_sounds[2].rate = 48000;
		announcer.queue[0] = &announcer.mcc_sounds[2];
		announcer.queued = 1;
		ae_announcer_update();
		CHECK(tones == 1 && announcer.free_at == clock_ms + 100 + 120, "a ready clip: tones %d, free at %lu", tones,
			announcer.free_at);
		return 0;
	}
	fprintf(stderr, "unknown case %s\n", case_name);
	return 2;
}
