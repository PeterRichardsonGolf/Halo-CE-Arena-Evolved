#ifndef AE_MCC_ADPCM_CHECK_H
#define AE_MCC_ADPCM_CHECK_H

/*
AE_MCC_ADPCM_CHECK.H

An MCC sound payload in Xbox ADPCM checked as mcc_pcm_open (mcc_audio.c) checks it before decoding, in the same order
and with the same failure texts, without decoding it: for a permutation kept as it is (its samples are never used).
1 and *frames; or 0 and *failure (*frames as mcc_pcm_open leaves it).
*/

#include <stdint.h>

int ae_mcc_adpcm_check(unsigned char const *bytes, uint32_t size, int channels, uint32_t frame_limit, uint32_t *frames,
	char const **failure);

#endif
