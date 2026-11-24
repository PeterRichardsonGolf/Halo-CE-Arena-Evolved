/*
AE_MCC_CRC_REUSE_RULES.C

The rules of a validation's resource CRC kept for the load (ae_mcc_crc_reuse_rules.h).
*/

#include <string.h>

#include "ae_mcc_crc_reuse_rules.h"

static int same_stamp(struct ae_mcc_file_stamp const *a, struct ae_mcc_file_stamp const *b)
{
	return !memcmp(a, b, sizeof(*a));
}

/* every used file's stamp taken again and compared with the entry's: 1 all the same */
static int stamps_hold(struct ae_mcc_crc_reuse_entry const *entry, ae_mcc_crc_stamp_call stamp)
{
	int type;

	for (type = 0; type < 2; type++)
	{
		struct ae_mcc_file_stamp now;

		if (!entry->paths[type][0])
			continue;
		if (!stamp(entry->paths[type], &now) || !same_stamp(&now, &entry->stamps[type]))
			return 0;
	}
	return 1;
}

static struct ae_mcc_crc_reuse_entry *pending_of(struct ae_mcc_crc_reuse *reuse, void const *job)
{
	int index;

	for (index = 0; index < AE_MCC_CRC_REUSE_PENDING; index++)
	{
		if (reuse->pending[index].used && reuse->pending[index].job == job)
			return &reuse->pending[index];
	}
	return NULL;
}

void ae_mcc_crc_reuse_rule_forget(struct ae_mcc_crc_reuse *reuse, void const *job)
{
	struct ae_mcc_crc_reuse_entry *entry = reuse && job ? pending_of(reuse, job) : NULL;

	if (entry)
		memset(entry, 0, sizeof(*entry));
}

void ae_mcc_crc_reuse_rule_begin(struct ae_mcc_crc_reuse *reuse, void const *job, char const *name, uint32_t seed,
	char const paths[2][128], ae_mcc_crc_stamp_call stamp)
{
	struct ae_mcc_crc_reuse_entry entry, *slot;
	int type, index, any = 0;

	if (!reuse || !job || !name || !paths || !stamp)
		return;
	ae_mcc_crc_reuse_rule_forget(reuse, job);
	memset(&entry, 0, sizeof(entry));
	if (strlen(name) >= sizeof(entry.name))
		return;
	memcpy(entry.name, name, strlen(name) + 1);
	entry.job = job;
	entry.seed = seed;
	for (type = 0; type < 2; type++)
	{
		size_t length = 0;

		while (length < 128 && paths[type][length])
			length++;

		if (!length)
			continue;
		if (length >= sizeof(entry.paths[type]))
			return;
		memcpy(entry.paths[type], paths[type], length);
		entry.paths[type][length] = 0;
		if (!stamp(entry.paths[type], &entry.stamps[type]))
			return;
		any = 1;
	}
	/* (a map with no resources of its own to read: its load reads nothing either, nothing to keep) */
	if (!any)
		return;
	slot = NULL;
	for (index = 0; index < AE_MCC_CRC_REUSE_PENDING && !slot; index++)
	{
		if (!reuse->pending[index].used)
			slot = &reuse->pending[index];
	}
	/* (more validations at once than slots: the oldest-placed slot is reused, its validation keeps nothing) */
	if (!slot)
		slot = &reuse->pending[reuse->next++ % AE_MCC_CRC_REUSE_PENDING];
	*slot = entry;
	slot->used = 1;
}

int ae_mcc_crc_reuse_rule_end(struct ae_mcc_crc_reuse *reuse, void const *job, uint32_t crc, ae_mcc_crc_stamp_call stamp)
{
	struct ae_mcc_crc_reuse_entry *entry = reuse && job && stamp ? pending_of(reuse, job) : NULL;
	struct ae_mcc_crc_reuse_entry kept;
	int index, slot = -1;

	if (!entry)
		return 0;
	kept = *entry;
	memset(entry, 0, sizeof(*entry));
	/* (a file written while the validation read it: its CRC is of no one state of the file) */
	if (!crc || !stamps_hold(&kept, stamp))
		return 0;
	kept.crc = crc;
	kept.job = NULL;
	for (index = 0; index < AE_MCC_CRC_REUSE_RESULTS && slot < 0; index++)
	{
		if (reuse->results[index].used && !strcmp(reuse->results[index].name, kept.name))
			slot = index;
	}
	for (index = 0; index < AE_MCC_CRC_REUSE_RESULTS && slot < 0; index++)
	{
		if (!reuse->results[index].used)
			slot = index;
	}
	if (slot < 0)
		slot = (int)(reuse->next++ % AE_MCC_CRC_REUSE_RESULTS);
	reuse->results[slot] = kept;
	reuse->kept++;
	return 1;
}

int ae_mcc_crc_reuse_rule_take(struct ae_mcc_crc_reuse *reuse, char const *name, uint32_t seed, unsigned int mask,
	char const *const paths[2], ae_mcc_crc_stamp_call stamp, uint32_t *crc)
{
	int index, type;

	if (!reuse || !name || !paths || !stamp || !crc || !mask || (mask & ~3u))
		return 0;
	for (index = 0; index < AE_MCC_CRC_REUSE_RESULTS; index++)
	{
		struct ae_mcc_crc_reuse_entry *entry = &reuse->results[index];
		int fits = 1;

		if (!entry->used || strcmp(entry->name, name))
			continue;
		if (entry->seed != seed)
			fits = 0;
		for (type = 0; type < 2 && fits; type++)
		{
			int uses = (mask & (1u << type)) != 0;

			if (uses != (entry->paths[type][0] != 0))
				fits = 0;
			else if (uses && (!paths[type] || strcmp(paths[type], entry->paths[type])))
				fits = 0;
		}
		if (fits && stamps_hold(entry, stamp))
		{
			*crc = entry->crc;
			reuse->taken++;
			return 1;
		}
		/* (a kept CRC that no longer fits: dropped, the next validation keeps a new one) */
		memset(entry, 0, sizeof(*entry));
		break;
	}
	reuse->refused++;
	return 0;
}
