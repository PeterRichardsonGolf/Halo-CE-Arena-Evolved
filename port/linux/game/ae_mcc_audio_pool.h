#ifndef AE_MCC_AUDIO_POOL_H
#define AE_MCC_AUDIO_POOL_H

/*
AE_MCC_AUDIO_POOL.H

An MCC load's Ogg Vorbis sounds decoded and encoded to Xbox ADPCM on worker threads (ae_mcc_convert_pool.h), for
mcc_audio.c, which includes ae_mcc_audio_pool.inl at its end: the workers run that unit's own decoder and encoder
(mcc_pcm_open, mcc_audio_store) on copies of the payloads, so each result is the bytes the game's thread would make.

debug.mcc_convert_threads: 0 (the default) converts on the loading thread alone, as without this unit; N workers;
-1 as many as the processor leaves (up to 4, two processors kept for the game; none on one or two).

The loading thread walks the sounds ahead of mcc_audio_prepare (the same tags, pitch ranges and permutations, in
the same order), reads each Vorbis payload and submits it. When mcc_audio_prepare reaches that permutation,
mcc_pcm_open takes the worker's decoded samples (the payload compared byte for byte first) and mcc_audio_store the
worker's ADPCM, both in order; anything else (another payload, a worker's failure, no worker) is converted on the
loading thread as before, so its messages are the same. A worker that has not finished in 30 seconds fails the load.
mcc_audio_prepare's end and mcc_audio_dispose drain the workers.
*/

#include <stdint.h>

struct mcc_runtime;
struct mcc_pcm;

void ae_mcc_audio_pool_begin(struct mcc_runtime *runtime);
/* mcc_pcm_open's start: 1 or 0, the worker's result (0 only for a worker past its deadline); -1, decode here */
int ae_mcc_audio_pcm_take(unsigned char const *bytes, uint32_t size, int compression, int channels, uint32_t rate,
	struct mcc_pcm *pcm);
/* mcc_audio_store's, its storage reserved: 1 when the worker's ADPCM for this pcm was copied to out */
int ae_mcc_audio_adpcm_take(struct mcc_pcm const *pcm, int channels, uint32_t rate, unsigned char *out,
	uint32_t size);
/* mcc_audio_prepare's success: the drain, its line, and with debug.mcc_convert_digest the converted sounds' CRC */
void ae_mcc_audio_pool_done(struct mcc_runtime *runtime);
/* the drain alone (mcc_audio_dispose: a failed or unloaded map) */
void ae_mcc_audio_pool_end(void);

#endif
