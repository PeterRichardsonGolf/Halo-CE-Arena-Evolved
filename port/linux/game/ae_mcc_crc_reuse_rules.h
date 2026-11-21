/*
AE_MCC_CRC_REUSE_RULES.H

An MCC map's resource CRC kept from its package validation for its load. The validation (mcc_validation.inl) reads the
map's bitmaps.map and sounds.map whole for the map's SHA-256 and the resources' CRC; the load (mcc_cache.c) read them
whole again for the same CRC (mcc_resources_checksum: the same seed, the map header's checksum, and the same framing).
The validation's CRC is kept with each resource file's stamp (ae_mcc_file_stamp.h), taken before the validation read
the file and again after; the load takes it only for the same map, the same header checksum, the same resource files
by path, and every one of those files' stamps unchanged since. Anything else, or no stamp (macOS), and the load reads
the files as before. Pure rules: the stamps come through a call the caller gives.
*/

#ifndef AE_MCC_CRC_REUSE_RULES_H
#define AE_MCC_CRC_REUSE_RULES_H

#include <stdint.h>

#include "../src/ae_mcc_file_stamp.h"

enum
{
	AE_MCC_CRC_REUSE_NAME_SIZE = 64,
	AE_MCC_CRC_REUSE_PATH_SIZE = 128,
	/* validations at once (the lobby's, an admission's, a staged download's) and results kept */
	AE_MCC_CRC_REUSE_PENDING = 4,
	AE_MCC_CRC_REUSE_RESULTS = 8,
};

typedef int (*ae_mcc_crc_stamp_call)(char const *path, struct ae_mcc_file_stamp *stamp);

struct ae_mcc_crc_reuse_entry
{
	void const *job;
	int used;
	char name[AE_MCC_CRC_REUSE_NAME_SIZE];
	uint32_t seed, crc;
	/* bitmaps.map's (0) and sounds.map's (1): an empty path is a resource the map does not use */
	char paths[2][AE_MCC_CRC_REUSE_PATH_SIZE];
	struct ae_mcc_file_stamp stamps[2];
};

struct ae_mcc_crc_reuse
{
	struct ae_mcc_crc_reuse_entry pending[AE_MCC_CRC_REUSE_PENDING];
	struct ae_mcc_crc_reuse_entry results[AE_MCC_CRC_REUSE_RESULTS];
	unsigned int next;
	/* what happened: results kept, loads that took one, loads that read the files again */
	unsigned int kept, taken, refused;
};

/* a validation's files chosen, before it reads them: the map's name, its header checksum, the resource paths (empty
for one not used); the stamps taken now. A path too long, or a file with no stamp, keeps nothing for this job */
void ae_mcc_crc_reuse_rule_begin(struct ae_mcc_crc_reuse *reuse, void const *job, char const *name, uint32_t seed,
	char const paths[2][128], ae_mcc_crc_stamp_call stamp);
/* the validation done, its CRC: kept when every stamp is what it was before the reading (1 kept) */
int ae_mcc_crc_reuse_rule_end(struct ae_mcc_crc_reuse *reuse, void const *job, uint32_t crc, ae_mcc_crc_stamp_call stamp);
/* a validation closed or cancelled: what it began is dropped */
void ae_mcc_crc_reuse_rule_forget(struct ae_mcc_crc_reuse *reuse, void const *job);
/* the load's: the map's name, its header checksum, the resources it uses (mask: 1 bitmaps, 2 sounds) and their paths;
1 and *crc when a kept CRC is for exactly that and no file's stamp changed, else 0 (the load reads them) */
int ae_mcc_crc_reuse_rule_take(struct ae_mcc_crc_reuse *reuse, char const *name, uint32_t seed, unsigned int mask,
	char const *const paths[2], ae_mcc_crc_stamp_call stamp, uint32_t *crc);

#endif
