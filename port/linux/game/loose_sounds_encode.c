/*
LOOSE_SOUNDS_ENCODE.C

Arena Evolved: the encoder loose_sounds.c needs (loose_sounds_encode.h), a
sound permutation's samples brought to a rate and channel count this build
plays and encoded as Xbox ADPCM. It is OpenCE's, from its
custom_edition_sounds.c (the decoders, the conforming of frames and the
encoder, as they were at build-157), which ChupathingyCE and Arena Evolved
do not have: their Custom Edition maps' sounds are ce_resources.c's. Ogg
Vorbis is decoded by ChupathingyCE's ce_vorbis.c.

Xbox ADPCM is IMA ADPCM in blocks of 64 samples per channel: a 4-byte
header per channel (the predictor and step index the block starts from),
then 4-byte groups of eight nibbles, low nibble first, alternating between
the channels (port/linux/src/dsound_sdl.c decodes it).
*/

/* ---------- headers */

#include "cseries.h"
#include "errors.h"
#include "loose_sounds_encode.h"

#include <stdlib.h>
#include <string.h>

/* (ce_vorbis.c) */
int ce_vorbis_decode(const unsigned char *data, int size, int *channels, int *sample_rate, short **samples);
void ce_vorbis_free(short *samples);

/* ---------- constants */

#define SOUND_COMPRESSION_NONE 0
#define SOUND_COMPRESSION_XBOX_ADPCM 1
#define SOUND_COMPRESSION_OGG_VORBIS 3

#define ADPCM_BLOCK_SAMPLES 64
#define ADPCM_BLOCK_BYTES 36
#define ADPCM_STEP_INDEX_MAXIMUM 88

/* the decoded buffer grows by this much at a time */
#define DECODED_GROWTH 0x100000

/* an Ogg Vorbis stream is decoded into a buffer that grows by this many
frames; no packet decodes to more than 4096 frames */
#define VORBIS_GROWTH_FRAMES 4096
/* The most frames a permutation has at any stage (decoded, or brought to
the rate this build plays): over six minutes at 44 kHz, and 64 MB of
stereo samples. Every buffer of frames is sized from a count within it
(frames_allocate), so that count * channels * 2 cannot wrap the 32-bit
size_t of this build: a stream that said it was at 1 Hz once made a count
whose bytes wrapped to a few KB, which the resampler then wrote 4 GB into */
#define MAXIMUM_FRAMES 0x1000000L
/* the highest rate a stream is taken to be at (its own rate is read from
it, and the resampler scales by it) */
#define MAXIMUM_SAMPLE_RATE 192000

/* ---------- globals */

static int const adpcm_step_table[ADPCM_STEP_INDEX_MAXIMUM + 1] =
{
	7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45,
	50, 55, 60, 66, 73, 80, 88, 97, 107, 118, 130, 143, 157, 173, 190, 209, 230,
	253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658, 724, 796, 876, 963,
	1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327,
	3660, 4026, 4428, 4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442,
	11487, 12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794,
	32767,
};

static int const adpcm_index_table[8] = { -1, -1, -1, -1, 2, 4, 6, 8 };

/* ---------- private code */

/* ---------- private code */

struct adpcm_state
{
	int predictor;
	int step_index;
};

/* The nibble that brings `state` nearest `sample`, moving it as the decoder will. */
static byte adpcm_encode_sample(
	struct adpcm_state *state,
	int sample)
{
	int step = adpcm_step_table[state->step_index];
	int difference = sample - state->predictor;
	int change = step >> 3;
	byte nibble = 0;

	if (difference < 0)
	{
		nibble = 8;
		difference = -difference;
	}
	if (difference >= step)
	{
		nibble |= 4;
		difference -= step;
		change += step;
	}
	if (difference >= step >> 1)
	{
		nibble |= 2;
		difference -= step >> 1;
		change += step >> 1;
	}
	if (difference >= step >> 2)
	{
		nibble |= 1;
		change += step >> 2;
	}

	state->predictor += (nibble & 8) ? -change : change;
	state->predictor = PIN(state->predictor, -32768, 32767);
	state->step_index += adpcm_index_table[nibble & 7];
	state->step_index = PIN(state->step_index, 0, ADPCM_STEP_INDEX_MAXIMUM);

	return nibble;
}

/* Encodes `frame_count` frames of interleaved 16-bit samples as Xbox ADPCM into
`output`, which holds adpcm_encoded_bytes(frame_count, channel_count) bytes.
A last block short of 64 frames is filled with the final sample. */
static unsigned long adpcm_encoded_bytes(
	long frame_count,
	long channel_count)
{
	return (unsigned long)((frame_count + ADPCM_BLOCK_SAMPLES - 1) / ADPCM_BLOCK_SAMPLES) *
		ADPCM_BLOCK_BYTES * channel_count;
}

static void adpcm_encode(
	short const *samples,
	long frame_count,
	long channel_count,
	byte *output)
{
	struct adpcm_state states[2] = { { 0, 0 }, { 0, 0 } };
	long block_count = (frame_count + ADPCM_BLOCK_SAMPLES - 1) / ADPCM_BLOCK_SAMPLES;
	long block_index;

	for (block_index = 0; block_index < block_count; block_index++)
	{
		byte *block = output + block_index * ADPCM_BLOCK_BYTES * channel_count;
		byte *nibbles = block + 4 * channel_count;
		long channel;

		for (channel = 0; channel < channel_count; channel++)
		{
			struct adpcm_state *state = &states[channel];
			byte *header = block + channel * 4;
			long group;

			header[0] = (byte)(state->predictor & 0xFF);
			header[1] = (byte)((state->predictor >> 8) & 0xFF);
			header[2] = (byte)state->step_index;
			header[3] = 0;
			for (group = 0; group < 8; group++)
			{
				byte *group_bytes = nibbles + (group * channel_count + channel) * 4;
				long byte_index;

				for (byte_index = 0; byte_index < 4; byte_index++)
				{
					long frame = block_index * ADPCM_BLOCK_SAMPLES + group * 8 + byte_index * 2;
					long first = MIN(frame, frame_count - 1);
					long second = MIN(frame + 1, frame_count - 1);
					byte low = adpcm_encode_sample(state, samples[first * channel_count + channel]);
					byte high = adpcm_encode_sample(state, samples[second * channel_count + channel]);

					group_bytes[byte_index] = (byte)(low | (high << 4));
				}
			}
		}
	}

	return;
}

/* 16-bit frames: their samples, channel count and rate */
struct frames
{
	short *samples;
	long count;
	long channels;
	long rate;
};

/* (none may have been made: the game's free, debug_free, does not take NULL) */
static void frames_free(
	struct frames *frames)
{
	if (frames->samples)
		free(frames->samples);
	frames->samples = NULL;
}

/* room for `count` frames of `channels` channels, or NULL when there are
none, too many (MAXIMUM_FRAMES), or no memory: the only way a buffer of
frames is made */
static short *frames_allocate(
	long count,
	long channels)
{
	if (count <= 0 || count > MAXIMUM_FRAMES || channels < 1 || channels > 2)
	{
		return NULL;
	}

	return malloc((size_t)count * (size_t)channels * sizeof(short));
}

/* the Ogg Vorbis stream `data`, at its own channel count and rate; FALSE
when it cannot be decoded (ChupathingyCE's decoder, ce_vorbis.c, which
refuses a stream of more channels than a sound has, or one longer than any
sound, as soon as that shows) */
static boolean vorbis_decode(
	byte const *data,
	long data_bytes,
	struct frames *frames)
{
	int channels = 0;
	int rate = 0;
	short *samples = NULL;
	int count = ce_vorbis_decode(data, (int)data_bytes, &channels, &rate, &samples);
	boolean decoded = FALSE;

	frames->samples = NULL;
	if (count > 0 && samples && channels >= 1 && channels <= 2 && rate > 0 && rate <= MAXIMUM_SAMPLE_RATE)
	{
		frames->count = count;
		frames->channels = channels;
		frames->rate = rate;
		/* (a buffer of the game's allocator, as the rest here are) */
		frames->samples = frames_allocate(frames->count, frames->channels);
		if (frames->samples)
		{
			memcpy(frames->samples, samples, (size_t)count * (size_t)channels * sizeof(short));
			decoded = TRUE;
		}
	}
	ce_vorbis_free(samples);

	return decoded;
}


/* uncompressed samples, 16-bit little-endian as Halo PC's maps keep them
(big-endian as its tag files do: loose_sounds.c) */
static boolean pcm_decode(
	byte const *data,
	long data_bytes,
	long channels,
	long rate,
	boolean big_endian,
	struct frames *frames)
{
	int low = big_endian ? 1 : 0;
	long index;

	frames->count = data_bytes / (2 * channels);
	frames->channels = channels;
	frames->rate = rate;
	frames->samples = frames_allocate(frames->count, channels);
	for (index = 0; frames->samples && index < frames->count * channels; index++)
	{
		frames->samples[index] = (short)(data[2 * index + low] | (data[2 * index + 1 - low] << 8));
	}

	return frames->samples != NULL;
}

/* Xbox ADPCM, as the mixer decodes it (port/linux/src/dsound_sdl.c) */
static int adpcm_decode_nibble(
	struct adpcm_state *state,
	int nibble)
{
	int step = adpcm_step_table[state->step_index];
	int difference = step >> 3;

	if (nibble & 1)
		difference += step >> 2;
	if (nibble & 2)
		difference += step >> 1;
	if (nibble & 4)
		difference += step;
	state->predictor += (nibble & 8) ? -difference : difference;
	state->predictor = PIN(state->predictor, -32768, 32767);
	state->step_index = PIN(state->step_index + adpcm_index_table[nibble & 7], 0, ADPCM_STEP_INDEX_MAXIMUM);

	return state->predictor;
}

static boolean adpcm_decode(
	byte const *data,
	long data_bytes,
	long channels,
	long rate,
	struct frames *frames)
{
	long block_count = data_bytes / (ADPCM_BLOCK_BYTES * channels);
	long block_index;

	frames->count = block_count * ADPCM_BLOCK_SAMPLES;
	frames->channels = channels;
	frames->rate = rate;
	frames->samples = frames_allocate(frames->count, channels);
	for (block_index = 0; frames->samples && block_index < block_count; block_index++)
	{
		byte const *block = data + block_index * ADPCM_BLOCK_BYTES * channels;
		short *output = frames->samples + block_index * ADPCM_BLOCK_SAMPLES * channels;
		long channel;

		for (channel = 0; channel < channels; channel++)
		{
			byte const *header = block + channel * 4;
			struct adpcm_state state;
			long group;

			state.predictor = (short)(header[0] | (header[1] << 8));
			state.step_index = MIN(header[2], ADPCM_STEP_INDEX_MAXIMUM);
			for (group = 0; group < 8; group++)
			{
				byte const *group_bytes = block + 4 * channels + (group * channels + channel) * 4;
				long byte_index;

				for (byte_index = 0; byte_index < 4; byte_index++)
				{
					long frame = group * 8 + byte_index * 2;

					output[frame * channels + channel] = (short)adpcm_decode_nibble(&state, group_bytes[byte_index] & 0xF);
					output[(frame + 1) * channels + channel] = (short)adpcm_decode_nibble(&state, group_bytes[byte_index] >> 4);
				}
			}
		}
	}

	return frames->samples != NULL;
}

/* the samples `data` of `compression` at `channels` and `rate` (an Ogg
Vorbis stream's own); FALSE when they cannot be decoded */
static boolean samples_decode(
	byte const *data,
	long data_bytes,
	short compression,
	boolean big_endian,
	long channels,
	long rate,
	struct frames *frames)
{
	switch (compression)
	{
	case SOUND_COMPRESSION_OGG_VORBIS:
		return vorbis_decode(data, data_bytes, frames);
	case SOUND_COMPRESSION_NONE:
		return pcm_decode(data, data_bytes, channels, rate, big_endian, frames);
	case SOUND_COMPRESSION_XBOX_ADPCM:
		return adpcm_decode(data, data_bytes, channels, rate, frames);
	}

	return FALSE;
}

/* `frames` at `channels` and `rate` (a mono stream fills both channels, a
stereo one is averaged for mono, and another rate is resampled by the
nearest frame); FALSE when it comes to nothing */
static boolean frames_conform(
	struct frames *frames,
	long channels,
	long rate)
{
	double scaled_count = (double)frames->count * rate / frames->rate;
	long count;
	short *samples;
	long frame;

	if (frames->channels == channels && frames->rate == rate)
	{
		return frames->count > 0;
	}
	/* (the count at the new rate, refused before it is a long when it is
	more than any sound has: a stream's own rate can be anything) */
	if (frames->rate <= 0 || !(scaled_count >= 0.0) || scaled_count > (double)MAXIMUM_FRAMES)
	{
		frames_free(frames);
		return FALSE;
	}
	count = (long)scaled_count;
	samples = frames_allocate(count, channels);
	for (frame = 0; samples && frame < count; frame++)
	{
		long source = MIN((long)((double)frame * frames->rate / rate), frames->count - 1);
		short const *in = frames->samples + source * frames->channels;
		short *out = samples + frame * channels;

		if (channels == frames->channels)
			memcpy(out, in, channels * sizeof(*out));
		else if (channels == 2)
			out[0] = out[1] = in[0];
		else
			out[0] = (short)((in[0] + in[1]) / 2);
	}
	frames_free(frames);
	frames->samples = samples;
	frames->count = count;
	frames->channels = channels;
	frames->rate = rate;

	return samples != NULL;
}

/* ---------- public code */

byte *custom_edition_sounds_encode(
	byte const *data,
	long data_bytes,
	short compression,
	boolean big_endian,
	long channels,
	long rate,
	long encoded_rate,
	unsigned long *encoded_bytes)
{
	struct frames frames = { 0 };
	byte *encoded = NULL;

	if (data_bytes > 0 && channels >= 1 && channels <= 2 &&
		samples_decode(data, data_bytes, compression, big_endian, channels, rate, &frames) &&
		frames_conform(&frames, channels, encoded_rate))
	{
		*encoded_bytes = adpcm_encoded_bytes(frames.count, channels);
		encoded = malloc(*encoded_bytes);
		if (encoded)
		{
			adpcm_encode(frames.samples, frames.count, channels, encoded);
		}
	}
	frames_free(&frames);

	return encoded;
}
