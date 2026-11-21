/*
AE_MCC_CRC_REUSE.H

An MCC map's resource CRC kept from its package validation for its load (the rules: ae_mcc_crc_reuse_rules.h). The
validation (mcc_validation.inl) calls begin with the files it chose, end with the CRC it worked out, and forget when
it closes; mcc_resources_checksum asks take first and reads the files only when it says no. Paths are the game's, as
the resource files keep them; their stamps are taken on the host's paths they open as.
*/

#ifndef AE_MCC_CRC_REUSE_H
#define AE_MCC_CRC_REUSE_H

#include <stdint.h>

void ae_mcc_crc_reuse_begin(void const *job, char const *name, uint32_t seed, char const paths[2][128]);
void ae_mcc_crc_reuse_end(void const *job, uint32_t crc);
void ae_mcc_crc_reuse_forget(void const *job);
/* mask: 1 bitmaps.map, 2 sounds.map; the paths of the ones used (NULL for one not used): 1 and *checksum (never 0)
when the validation's CRC is for exactly these files, unchanged since */
int ae_mcc_crc_reuse_take(char const *name, uint32_t seed, unsigned int mask, char const *bitmaps, char const *sounds,
	uint32_t *checksum);

#endif
