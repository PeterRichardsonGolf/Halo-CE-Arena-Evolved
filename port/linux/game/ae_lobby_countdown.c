/* ae_lobby_countdown.c: CREATE GAME's start countdown, as a machine reads it (see ae_lobby_countdown.h). No engine
includes. */

#include <math.h>
#include <string.h>
#include "ae_lobby_countdown.h"

#define AE_BEEP_PI 3.14159265358979f

void ae_countdown_reset(struct ae_countdown *countdown)
{
	memset(countdown, 0, sizeof(*countdown));
	countdown->phase = AE_COUNTDOWN_IDLE;
}

void ae_countdown_begin(struct ae_countdown *countdown, long milliseconds, unsigned long now)
{
	ae_countdown_reset(countdown);
	countdown->phase = AE_COUNTDOWN_PENDING;
	countdown->seconds = (short)(milliseconds > 0 ? milliseconds / 1000 : 0);
	countdown->whole = (short)(countdown->seconds + 1);
	countdown->changed_at = now;
}

void ae_countdown_cancel(struct ae_countdown *countdown, unsigned long now)
{
	ae_countdown_reset(countdown);
	countdown->phase = AE_COUNTDOWN_CANCELLING;
	countdown->changed_at = now;
}

/* a number shown from now: its phase, and the event it is */
static int shown(struct ae_countdown *countdown, short seconds, unsigned long now)
{
	countdown->seconds = seconds;
	countdown->changed_at = now;
	countdown->phase = seconds > 0 ? AE_COUNTDOWN_COUNTING : AE_COUNTDOWN_GO;
	return seconds > 0 ? AE_COUNTDOWN_NUMBER : AE_COUNTDOWN_ZERO;
}

int ae_countdown_update(struct ae_countdown *countdown, short seconds, unsigned long now)
{
	int phase = countdown->phase;

	/* (the host's own cancel: its NONE, or the game already under way, ends it; a stale number until then is not a
	new countdown) */
	if (phase == AE_COUNTDOWN_CANCELLING)
	{
		if (seconds < 0 || now - countdown->changed_at >= AE_COUNTDOWN_PENDING_MS)
			ae_countdown_reset(countdown);
		return AE_COUNTDOWN_NOTHING;
	}
	if (seconds == AE_COUNTDOWN_SECONDS_STARTED)
	{
		if (phase == AE_COUNTDOWN_IDLE)
			return AE_COUNTDOWN_NOTHING;
		ae_countdown_reset(countdown);
		return AE_COUNTDOWN_STARTED;
	}
	if (seconds < 0)
	{
		/* (the host's start still waiting for the server's first number: given a moment, then it never began) */
		if (phase == AE_COUNTDOWN_IDLE ||
			(phase == AE_COUNTDOWN_PENDING && now - countdown->changed_at < AE_COUNTDOWN_PENDING_MS))
			return AE_COUNTDOWN_NOTHING;
		ae_countdown_reset(countdown);
		return AE_COUNTDOWN_CANCELLED;
	}
	/* (the first number: a joiner's, or the host's after its start; the bar's whole from it. A joiner that came in
	mid-count drains from its own first number) */
	if (phase == AE_COUNTDOWN_IDLE || phase == AE_COUNTDOWN_PENDING)
	{
		if (phase == AE_COUNTDOWN_IDLE || seconds + 1 > countdown->whole)
			countdown->whole = (short)(seconds + 1);
		return shown(countdown, seconds, now);
	}
	if (seconds != countdown->seconds)
	{
		/* (time added: a stock lobby's "slower" from another machine) */
		if (seconds + 1 > countdown->whole)
			countdown->whole = (short)(seconds + 1);
		return shown(countdown, seconds, now);
	}
	if (phase == AE_COUNTDOWN_GO && now - countdown->changed_at >= AE_COUNTDOWN_HOLD_MS)
	{
		countdown->phase = AE_COUNTDOWN_HELD;
		return AE_COUNTDOWN_HOLD;
	}
	return AE_COUNTDOWN_NOTHING;
}

int ae_countdown_running(struct ae_countdown const *countdown)
{
	return countdown->phase != AE_COUNTDOWN_IDLE && countdown->phase != AE_COUNTDOWN_CANCELLING;
}

float ae_countdown_left(struct ae_countdown const *countdown, unsigned long now)
{
	long left, floor_ms;

	if (countdown->whole <= 0 || (countdown->phase != AE_COUNTDOWN_PENDING &&
		countdown->phase != AE_COUNTDOWN_COUNTING && countdown->phase != AE_COUNTDOWN_GO))
		return 0.0f;
	/* (a number comes as its second starts: N shows from N.999 s down to N s) */
	floor_ms = (long)countdown->seconds * 1000L;
	left = floor_ms + 999L - (long)(now - countdown->changed_at);
	if (left < floor_ms)
		left = floor_ms;
	if (left > (long)countdown->whole * 1000L)
		left = (long)countdown->whole * 1000L;
	if (left < 0)
		left = 0;
	return (float)left / ((float)countdown->whole * 1000.0f);
}

int ae_countdown_beep(short seconds, int pregame_countdown)
{
	if (seconds < 0)
		return AE_BEEP_NONE;
	if (pregame_countdown)
		return seconds > 0 ? AE_BEEP_SOFT : AE_BEEP_NONE;
	return seconds > 0 ? AE_BEEP_TICK : AE_BEEP_GO;
}

unsigned long ae_countdown_tone(int beep, short *samples, unsigned long maximum, unsigned long rate)
{
	float hz, level;
	unsigned long ms, frames, fade, frame;

	switch (beep)
	{
	case AE_BEEP_TICK: hz = 880.0f; ms = 90; level = 0.42f; break;
	case AE_BEEP_SOFT: hz = 880.0f; ms = 60; level = 0.14f; break;
	case AE_BEEP_GO: hz = 1320.0f; ms = 220; level = 0.42f; break;
	default: return 0;
	}
	if (!samples || rate == 0)
		return 0;
	frames = rate * ms / 1000;
	if (frames > maximum)
		frames = maximum;
	fade = rate * 8 / 1000;
	for (frame = 0; frame < frames; frame++)
	{
		float gain = level, value;

		/* (fades in and out: no click at either end) */
		if (frame < fade)
			gain *= (float)frame / (float)fade;
		if (frames - 1 - frame < fade)
			gain *= (float)(frames - 1 - frame) / (float)fade;
		value = sinf(2.0f * AE_BEEP_PI * hz * (float)frame / (float)rate) * gain * 32767.0f;
		samples[frame] = (short)value;
	}
	return frames;
}
