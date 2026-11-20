/*
AE_MCC_SHA256_BLOCKS.H

Whole SHA-256 blocks with the CPU's SHA instructions (x86's SHA extensions), for mcc_sha256.c: an MCC map's content
fingerprint (mcc_resource_check.inl) reads the map and its resource maps, a gigabyte for a large Workshop package, and
the instructions work it out several times faster than the portable rounds, to the same bytes.
*/

#ifndef AE_MCC_SHA256_BLOCKS_H
#define AE_MCC_SHA256_BLOCKS_H

#include <stdint.h>

/* the blocks (64 bytes each) of data run through state: how many were done (all of them), or 0 when this CPU or build
has no SHA instructions, and the caller runs them itself */
int ae_mcc_sha256_blocks(uint32_t state[8], unsigned char const *data, int blocks);

/* whether ae_mcc_sha256_blocks uses the CPU's instructions here (1) or does nothing (0) */
int ae_mcc_sha256_hardware(void);

/* the blocks the CPU's instructions have done since the start (a count that only grows; threads add to it unlocked,
so it is a close count, not an exact one) */
unsigned long ae_mcc_sha256_hardware_blocks(void);

#endif
