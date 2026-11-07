/*
AE_GAMETYPE_BLOCK_CACHE.C

The gametype files' blocks and variants kept in memory (ae_gametype_block_cache.h): a table of each by profile index,
each entry with the content generation and the list's change count it was read at.
*/

#include <string.h>

#include "ae_gametype_block_cache.h"

#define CACHE_ENTRIES 512
#define CACHE_BLOCK 512

static struct
{
	int used;
	long profile_index;
	long content_generation;
	unsigned long listing;
	unsigned long size;
	unsigned char data[CACHE_BLOCK];
} cache[NUMBER_OF_AE_GAMETYPE_CACHES][CACHE_ENTRIES];

static unsigned long listing_changes;

static unsigned int slot_of(long profile_index)
{
	return (unsigned int)(((unsigned long)profile_index * 2654435761ul) >> 7) % CACHE_ENTRIES;
}

void ae_saved_games_listing_changed(void)
{
	listing_changes++;
}

int ae_gametype_cache_get(int kind, long profile_index, long content_generation, void *data, unsigned long size)
{
	unsigned int slot = slot_of(profile_index);

	if (!data || kind < 0 || kind >= NUMBER_OF_AE_GAMETYPE_CACHES || size > CACHE_BLOCK ||
		!cache[kind][slot].used || cache[kind][slot].profile_index != profile_index ||
		cache[kind][slot].content_generation != content_generation || cache[kind][slot].listing != listing_changes ||
		cache[kind][slot].size != size)
	{
		return 0;
	}
	memcpy(data, cache[kind][slot].data, size);
	return 1;
}

void ae_gametype_cache_put(int kind, long profile_index, long content_generation, void const *data, unsigned long size)
{
	unsigned int slot = slot_of(profile_index);

	if (!data || kind < 0 || kind >= NUMBER_OF_AE_GAMETYPE_CACHES || size > CACHE_BLOCK)
		return;
	cache[kind][slot].used = 1;
	cache[kind][slot].profile_index = profile_index;
	cache[kind][slot].content_generation = content_generation;
	cache[kind][slot].listing = listing_changes;
	cache[kind][slot].size = size;
	memcpy(cache[kind][slot].data, data, size);
}
