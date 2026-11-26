#include "harness.h"
/* port/linux/src/ae_capture_downmix.h: a recording takes stereo (capture.c); a 5.1 mix is folded to it in chunks
(center and back at -3 dB into each side, the LFE left out), a stereo mix passes as it is */

static float captured[4096];
static unsigned int captured_frames, calls;
static int wrong_format;
static void capture_audio(const float *samples, unsigned int frames, unsigned int channels, unsigned int rate)
{
	calls++;
	if (channels != 2 || rate != 48000 || captured_frames + frames > 2048)
	{
		wrong_format = 1;
		return;
	}
	memcpy(captured + captured_frames * 2, samples, frames * 2 * sizeof(float));
	captured_frames += frames;
}
#include "under_test.inc"

static float mix[600 * 6];

static int near(float a, float b) { return a - b < 1e-4f && b - a < 1e-4f; }

int main(int argc, char **argv)
{
	char const *case_name = argv[1];
	unsigned int frame;

	CASE("five-one")
	{
		for (frame = 0; frame < 600; frame++)
		{
			float *out = mix + frame * 6;
			out[0] = 0.1f; out[1] = frame == 599 ? 0.5f : -0.1f; out[2] = 0.2f; out[3] = 0.9f; out[4] = 0.3f; out[5] = frame == 599 ? 0.9f : 0.0f;
		}
		ae_capture_audio(mix, 600, 6, 48000);
		CHECK(!wrong_format && captured_frames == 600 && calls == 3, "%u frames in %u calls, format %s", captured_frames,
			calls, wrong_format ? "wrong" : "stereo");
		CHECK(near(captured[0], 0.1f + 0.70710678f * (0.2f + 0.3f)), "left %f", captured[0]);
		CHECK(near(captured[1], -0.1f + 0.70710678f * 0.2f), "right %f (the LFE in it?)", captured[1]);
		CHECK(near(captured[599 * 2 + 1], 1.0f), "a sum over 1 not clamped: %f", captured[599 * 2 + 1]);
		return 0;
	}
	CASE("stereo")
	{
		for (frame = 0; frame < 300 * 2; frame++)
			mix[frame] = frame % 2 ? -0.5f : 0.25f;
		ae_capture_audio(mix, 300, 2, 48000);
		CHECK(!wrong_format && calls == 1 && captured_frames == 300 && captured[0] == 0.25f && captured[599] == -0.5f,
			"the stereo mix changed");
		return 0;
	}
	fprintf(stderr, "unknown case %s\n", case_name);
	return 2;
}
