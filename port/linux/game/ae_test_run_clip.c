/* ae_test_run_clip.c: Arena Evolved TEST RUN's recording decisions (see ae_test_run_clip.h). */

#include <string.h>

#include "ae_test_run_clip.h"

void ae_test_run_clip_clear(struct ae_test_run_clip *clip)
{
	unsigned next = clip->next_serial;

	memset(clip, 0, sizeof(*clip));
	clip->next_serial = next; /* (never reused, even after a clear) */
	clip->test = clip->want_test = clip->ended_test = -1;
}

void ae_test_run_clip_want(struct ae_test_run_clip *clip, short test)
{
	if (clip->active && clip->test == test && !clip->stop_wanted)
		return;
	clip->want = 1;
	clip->want_test = test;
}

int ae_test_run_clip_keep(struct ae_test_run_clip *clip, short test, unsigned serial)
{
	int unstarted = 0;

	if (clip->active && clip->test == test && clip->serial == serial)
	{
		clip->keep = 1;
		clip->stop_wanted = 1;
		clip->delete_after = 0;
	}
	if (clip->want && clip->want_test == test)
	{
		clip->want = 0;
		unstarted = 1;
	}
	return unstarted;
}

void ae_test_run_clip_mark_kept(struct ae_test_run_clip *clip, short test, unsigned serial)
{
	if (clip->active && clip->test == test && clip->serial == serial)
		clip->keep = 1;
}

void ae_test_run_clip_drop(struct ae_test_run_clip *clip, short test, unsigned serial)
{
	if (clip->active && clip->test == test && clip->serial == serial)
	{
		clip->stop_wanted = 1;
		clip->delete_after = !clip->keep;
	}
	if (clip->want && clip->want_test == test)
		clip->want = 0;
}

void ae_test_run_clip_leave(struct ae_test_run_clip *clip, short test)
{
	if (clip->active && clip->test != test)
	{
		clip->stop_wanted = 1;
		clip->delete_after = !clip->keep;
	}
	if (clip->want && clip->want_test != test)
		clip->want = 0;
}

int ae_test_run_clip_service(struct ae_test_run_clip *clip, int state, unsigned long now)
{
	int actions = 0;

	if (clip->active)
	{
		if (state == 1)
			clip->seen = 1;
		if (clip->stop_wanted && !clip->stop_sent && state == 1)
		{
			clip->stop_sent = 1;
			actions |= AE_TEST_RUN_CLIP_STOP;
		}
		if ((clip->stop_sent && state == 0) || (!clip->seen && now - clip->asked_at > AE_TEST_RUN_CLIP_START_MS))
		{
			if (clip->delete_after || !clip->seen)
				actions |= AE_TEST_RUN_CLIP_DELETE;
			if (!clip->seen)
				actions |= AE_TEST_RUN_CLIP_START_FAILED;
			clip->ended_test = clip->test;
			clip->active = clip->keep = clip->delete_after = clip->stop_wanted = clip->stop_sent = clip->seen = 0;
		}
	}
	if (clip->want && !clip->active)
	{
		if (state == -1)
		{
			clip->want = 0;
			actions |= AE_TEST_RUN_CLIP_NO_FFMPEG;
		}
		else if (state == 0)
		{
			clip->active = 1;
			clip->test = clip->want_test;
			clip->serial = ++clip->next_serial;
			clip->keep = clip->delete_after = clip->stop_wanted = clip->stop_sent = clip->seen = 0;
			clip->asked_at = now ? now : 1;
			clip->want = 0;
			actions |= AE_TEST_RUN_CLIP_START;
		}
	}
	return actions;
}
