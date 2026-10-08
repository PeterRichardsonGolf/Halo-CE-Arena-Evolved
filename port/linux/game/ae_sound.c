/* ae_sound.c: Arena Evolved menus, the frame's one sound (see ae_sound.h). No engine includes. */

#include "ae_sound.h"

/* a held key's repeats: the cursor at most this often */
#define CURSOR_REPEAT_MS 80

static int requested;
/* nonzero while every cursor asked this frame is a repeat's */
static int requested_repeat;
static int cursor_played;
static unsigned long cursor_played_at;

void ae_sound_reset(void)
{
	requested = AE_SOUND_NONE;
	requested_repeat = 0;
	cursor_played = 0;
	cursor_played_at = 0;
}

void ae_sound_request(int sound, int repeat)
{
	if (sound <= AE_SOUND_NONE || sound > AE_SOUND_FAILURE)
		return;
	if (sound > requested)
	{
		requested = sound;
		requested_repeat = repeat != 0;
	}
	else if (sound == requested && !repeat)
		requested_repeat = 0;
}

int ae_sound_take(unsigned long now_ms)
{
	int sound = requested;

	if (sound == AE_SOUND_CURSOR)
	{
		/* (offsets from the last: a wrapping clock is fine) */
		if (requested_repeat && cursor_played && now_ms - cursor_played_at < CURSOR_REPEAT_MS)
			sound = AE_SOUND_NONE;
		else
		{
			cursor_played = 1;
			cursor_played_at = now_ms;
		}
	}
	requested = AE_SOUND_NONE;
	requested_repeat = 0;
	return sound;
}
