/*
AE_MCC_CRC_REUSE.C

The game's side of a validation's resource CRC kept for the load (ae_mcc_crc_reuse.h): one table, used on the game's
thread only (the validation's begin and end run there; its worker only reads), each stamp taken on the path the file
opens as. A kept CRC and a load that takes one are logged.
*/

#include "cseries.h"
#include "errors.h"

#include "ae_mcc_crc_reuse.h"
#include "ae_mcc_crc_reuse_rules.h"

void platform_translate_path(const char *xbox_path, char *host_path, unsigned long host_path_size);

static struct ae_mcc_crc_reuse reuse;

/* a resource file's stamp, on the path it opens as (mcc_resource_open's rule): an MCC install's or a Workshop item's
file by its host path as it is, anything else through platform_translate_path */
static int host_stamp(char const *path, struct ae_mcc_file_stamp *stamp)
{
	char host[1024];

	if (path[0] == '/' || path[0] == '\\' || (path[0] && path[1] == ':'))
		return ae_mcc_file_stamp(path, stamp);
	host[0] = 0;
	platform_translate_path(path, host, sizeof(host));
	return host[0] && ae_mcc_file_stamp(host, stamp);
}

void ae_mcc_crc_reuse_begin(void const *job, char const *name, uint32_t seed, char const paths[2][128])
{
	ae_mcc_crc_reuse_rule_begin(&reuse, job, name, seed, paths, host_stamp);
}

void ae_mcc_crc_reuse_end(void const *job, uint32_t crc)
{
	if (ae_mcc_crc_reuse_rule_end(&reuse, job, crc, host_stamp))
		error(_error_silent, "mcc resources: a validation's resource checksum kept for the load (%u kept so far)",
			reuse.kept);
}

void ae_mcc_crc_reuse_forget(void const *job)
{
	ae_mcc_crc_reuse_rule_forget(&reuse, job);
}

int ae_mcc_crc_reuse_take(char const *name, uint32_t seed, unsigned int mask, char const *bitmaps, char const *sounds,
	uint32_t *checksum)
{
	char const *paths[2];
	uint32_t crc;

	paths[0] = bitmaps;
	paths[1] = sounds;
	if (!ae_mcc_crc_reuse_rule_take(&reuse, name, seed, mask, paths, host_stamp, &crc))
		return 0;
	*checksum = crc ? crc : 1;
	error(_error_silent, "mcc resources: %s's resource checksum is its validation's (files unchanged since), not read again",
		name);
	return 1;
}
