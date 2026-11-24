/*
AE_MCC_ADPCM_CHECK_TEST.C

The check of a kept Xbox ADPCM payload (port/linux/game/ae_mcc_adpcm_check.c) against mcc_pcm_open's own checks of it:
the frame count of good mono and stereo payloads; a size off the block grid; none and too many frames; a step index
past 88 or a nonzero reserved byte in the first, a middle and the last block, on either channel; each with
mcc_pcm_open's failure text and the frame count it leaves.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../game/ae_mcc_adpcm_check.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

#define SIZE_TEXT "Xbox ADPCM byte count is not a whole 36-byte block per channel"
#define FRAMES_TEXT "decoded frame count is zero or exceeds 16777216 frames"
#define HEADER_TEXT "invalid Xbox ADPCM block header (step index exceeds 88 or reserved byte is nonzero)"

static unsigned char *payload(int channels, uint32_t blocks)
{
	uint32_t size = blocks * 36 * (uint32_t)channels, index;
	unsigned char *bytes = (unsigned char *)malloc(size ? size : 1);

	for (index = 0; index < size; index++)
		bytes[index] = (unsigned char)(index * 7u + 3u);
	for (index = 0; index < blocks; index++)
	{
		int channel;

		for (channel = 0; channel < channels; channel++)
		{
			bytes[index * 36 * (uint32_t)channels + (uint32_t)channel * 4 + 2] = (unsigned char)((index + (uint32_t)channel) % 89);
			bytes[index * 36 * (uint32_t)channels + (uint32_t)channel * 4 + 3] = 0;
		}
	}
	return bytes;
}

static void expect(unsigned char const *bytes, uint32_t size, int channels, uint32_t limit, int ok, uint32_t frames,
	char const *text)
{
	uint32_t got_frames = 0;
	char const *got_text = NULL;

	CHECK(ae_mcc_adpcm_check(bytes, size, channels, limit, &got_frames, &got_text) == ok);
	CHECK(got_frames == frames);
	CHECK(ok ? got_text == NULL : got_text && !strcmp(got_text, text));
}

int main(void)
{
	int channels;

	for (channels = 1; channels <= 2; channels++)
	{
		uint32_t blocks = 9, stride = 36 * (uint32_t)channels, position[3] = {0, 4, 8};
		unsigned char *bytes = payload(channels, blocks);
		int which, channel;

		expect(bytes, blocks * stride, channels, 0x1000000u, 1, blocks * 64, NULL);
		expect(bytes, blocks * stride - 1, channels, 0x1000000u, 0, 0, SIZE_TEXT);
		expect(bytes, 0, channels, 0x1000000u, 0, 0, FRAMES_TEXT);
		expect(bytes, blocks * stride, channels, blocks * 64 - 1, 0, blocks * 64, FRAMES_TEXT);
		expect(bytes, blocks * stride, channels, blocks * 64, 1, blocks * 64, NULL);
		for (which = 0; which < 3; which++)
		{
			for (channel = 0; channel < channels; channel++)
			{
				unsigned char *header = bytes + position[which] * stride + (uint32_t)channel * 4;
				unsigned char step = header[2];

				header[2] = 89;
				expect(bytes, blocks * stride, channels, 0x1000000u, 0, blocks * 64, HEADER_TEXT);
				header[2] = 88;
				expect(bytes, blocks * stride, channels, 0x1000000u, 1, blocks * 64, NULL);
				header[2] = step;
				header[3] = 1;
				expect(bytes, blocks * stride, channels, 0x1000000u, 0, blocks * 64, HEADER_TEXT);
				header[3] = 0;
			}
		}
		/* (only headers are checked: the sample nibbles are any bytes) */
		bytes[stride - 1] ^= 0xFF;
		expect(bytes, blocks * stride, channels, 0x1000000u, 1, blocks * 64, NULL);
		free(bytes);
	}
	if (failures)
		return 1;
	printf("ae_mcc_adpcm_check: all checks passed\n");
	return 0;
}
