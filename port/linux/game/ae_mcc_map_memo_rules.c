/*
AE_MCC_MAP_MEMO_RULES.C

Whether an MCC map is here, remembered a while (ae_mcc_map_memo_rules.h).
*/

#include <string.h>

#include "ae_mcc_map_memo_rules.h"

int ae_mcc_map_memo_rules_slot(char const *identity)
{
	unsigned long hash = 2166136261ul;

	for (; *identity; identity++)
		hash = ((hash ^ (unsigned char)*identity) * 16777619ul) & 0xFFFFFFFFul;
	return (int)(hash % AE_MCC_MAP_MEMO_ENTRIES);
}

static int keepable(char const *identity)
{
	return identity && identity[0] && strlen(identity) < AE_MCC_MAP_MEMO_IDENTITY;
}

/* the entry of the name, NULL for none */
static struct ae_mcc_map_memo_entry *kept(struct ae_mcc_map_memo *memo, char const *identity)
{
	int start = ae_mcc_map_memo_rules_slot(identity), place;

	for (place = 0; place < AE_MCC_MAP_MEMO_PLACES; place++)
	{
		struct ae_mcc_map_memo_entry *entry = &memo->entries[(start + place) % AE_MCC_MAP_MEMO_ENTRIES];

		if (entry->used && !strcmp(entry->identity, identity))
			return entry;
	}
	return NULL;
}

/* where the name's answer goes: its own entry, else the first free place of its 8, else the oldest of them */
static struct ae_mcc_map_memo_entry *place_for(struct ae_mcc_map_memo *memo, char const *identity, unsigned long now)
{
	struct ae_mcc_map_memo_entry *oldest = NULL;
	int start = ae_mcc_map_memo_rules_slot(identity), place;

	if ((oldest = kept(memo, identity)) != NULL)
		return oldest;
	for (place = 0; place < AE_MCC_MAP_MEMO_PLACES; place++)
	{
		struct ae_mcc_map_memo_entry *entry = &memo->entries[(start + place) % AE_MCC_MAP_MEMO_ENTRIES];

		if (!entry->used)
			return entry;
		if (!oldest || now - entry->time > now - oldest->time)
			oldest = entry;
	}
	return oldest;
}

/* one more older answer may be checked again in this frame */
static int frame_check_allowed(struct ae_mcc_map_memo *memo, unsigned long now, unsigned int frame)
{
	if (!memo->frame_known || memo->frame != frame || now - memo->frame_time >= AE_MCC_MAP_MEMO_FRAME_MILLISECONDS)
	{
		memo->frame_known = 1;
		memo->frame = frame;
		memo->frame_time = now;
		memo->frame_checks = 0;
	}
	if (memo->frame_checks >= AE_MCC_MAP_MEMO_CHECKS_A_FRAME)
		return 0;
	memo->frame_checks++;
	return 1;
}

int ae_mcc_map_memo_rules_find(struct ae_mcc_map_memo *memo, char const *identity, unsigned long generation,
	unsigned long now, unsigned int frame, int *present)
{
	struct ae_mcc_map_memo_entry const *entry;
	unsigned long age;

	if (!memo || !keepable(identity) || !present)
		return 0;
	entry = kept(memo, identity);
	if (!entry || entry->generation != generation)
		return 0;
	age = now - entry->time;
	if (age >= AE_MCC_MAP_MEMO_STALE_MILLISECONDS)
		return 0;
	if (age >= AE_MCC_MAP_MEMO_FRESH_MILLISECONDS && frame_check_allowed(memo, now, frame))
		return 0;
	*present = entry->present;
	return 1;
}

void ae_mcc_map_memo_rules_keep(struct ae_mcc_map_memo *memo, char const *identity, unsigned long generation,
	unsigned long now, int present)
{
	struct ae_mcc_map_memo_entry *entry;

	if (!memo || !keepable(identity))
		return;
	entry = place_for(memo, identity, now);
	strcpy(entry->identity, identity);
	entry->used = 1;
	entry->present = present ? 1 : 0;
	entry->generation = generation;
	entry->time = now;
}
