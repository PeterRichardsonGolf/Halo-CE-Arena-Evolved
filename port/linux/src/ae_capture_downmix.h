#ifndef AE_CAPTURE_DOWNMIX_H
#define AE_CAPTURE_DOWNMIX_H

/* what is heard, for a recording (capture.c takes stereo only): the mix as it is when it is stereo, and folded to
stereo when it is 5.1 (SDL's order: FL, FR, FC, LFE, BL, BR; the center and back at -3 dB into each side, the LFE
left out, as the game sends nothing to it), in chunks of a buffer on the stack */
#define AE_CAPTURE_DOWNMIX_CHUNK 256

static inline float ae_capture_downmix_clamp(float value)
{
	return value > 1.0f ? 1.0f : value < -1.0f ? -1.0f : value;
}

static inline void ae_capture_downmix_51(float const *input, unsigned int frames, float *stereo)
{
	unsigned int frame;

	for (frame = 0; frame < frames; frame++)
	{
		float const *in = input + (size_t)frame * 6;
		float center = 0.70710678f * in[2];

		stereo[frame * 2] = ae_capture_downmix_clamp(in[0] + center + 0.70710678f * in[4]);
		stereo[frame * 2 + 1] = ae_capture_downmix_clamp(in[1] + center + 0.70710678f * in[5]);
	}
}

static inline void ae_capture_audio(float const *samples, unsigned int frames, unsigned int channels, unsigned int rate)
{
	float stereo[AE_CAPTURE_DOWNMIX_CHUNK * 2];
	unsigned int done, count;

	if (channels != 6)
	{
		capture_audio(samples, frames, channels, rate);
		return;
	}
	for (done = 0; done < frames; done += count)
	{
		count = frames - done < AE_CAPTURE_DOWNMIX_CHUNK ? frames - done : AE_CAPTURE_DOWNMIX_CHUNK;
		ae_capture_downmix_51(samples + (size_t)done * 6, count, stereo);
		capture_audio(stereo, count, 2, rate);
	}
}

#endif
