/*
AE_MCC_ADPCM_CHECK.C

mcc_pcm_open's checks of an Xbox ADPCM payload (ae_mcc_adpcm_check.h): whole 36-byte blocks per channel, a frame count
within the limit, and each block's header per channel (step index at most 88, reserved byte zero), block by block.
*/

#include "ae_mcc_adpcm_check.h"

int ae_mcc_adpcm_check(unsigned char const *bytes, uint32_t size, int channels, uint32_t frame_limit, uint32_t *frames,
	char const **failure)
{
	uint32_t block;

	if (size % (uint32_t)(36 * channels))
	{
		*failure = "Xbox ADPCM byte count is not a whole 36-byte block per channel";
		return 0;
	}
	*frames = size / (uint32_t)(36 * channels) * 64;
	if (!*frames || *frames > frame_limit)
	{
		*failure = "decoded frame count is zero or exceeds 16777216 frames";
		return 0;
	}
	for (block = 0; block < *frames / 64; block++)
	{
		unsigned char const *header = bytes + block * 36 * (uint32_t)channels;
		int channel;

		for (channel = 0; channel < channels; channel++)
		{
			if (header[channel * 4 + 2] > 88 || header[channel * 4 + 3])
			{
				*failure = "invalid Xbox ADPCM block header (step index exceeds 88 or reserved byte is nonzero)";
				return 0;
			}
		}
	}
	return 1;
}
