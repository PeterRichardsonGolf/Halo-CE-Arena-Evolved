/*
AE_AUDIO_CAPTURE.C

Arena Evolved: the audio probe's recording of the mix (port/linux/game/
audio_probe.h, ChupathingyCE 0.8.0d) on Arena Evolved's mixer.

ChupathingyCE records its own mixer's output (its dsound_sdl.c: 7.1, binaural
rendering, Doppler). Arena Evolved keeps OpenCE's mixer (5.1, stereo voices
positioned in the world, the callouts' UI voice), which has no recording: the
probe's scripted runs and debug.audio_capture_seconds log their sound events
and say that nothing is recorded.
*/

#include "platform.h"

int audio_capture_start(char const *path_prefix);
unsigned long audio_capture_frames(void);
unsigned long audio_capture_rate(void);
void audio_capture_update(void);
void audio_capture_stop(void);
char const *audio_capture_platform(void);

int audio_capture_start(char const *path_prefix)
{
	platform_log("audio probe: %s: this build's mixer (OpenCE's) does not record its output",
		path_prefix ? path_prefix : "");
	return 0;
}

unsigned long audio_capture_frames(void)
{
	return 0;
}

unsigned long audio_capture_rate(void)
{
	return 48000;
}

void audio_capture_update(void)
{
}

void audio_capture_stop(void)
{
}

char const *audio_capture_platform(void)
{
	return "pc";
}
