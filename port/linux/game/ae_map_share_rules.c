/*
AE_MAP_SHARE_RULES.C

Arena Evolved: map sharing's safety rules (ae_map_share_rules.h). From
DamnationCE v0.3.41 (CC0), the address ranges as its address_local.
*/

#include "ae_map_share_rules.h"

#include <ctype.h>
#include <string.h>

int ae_map_share_address_local(
	unsigned long address)
{
	/* (an unsigned long of 64 bits: nothing above the 32 of an address) */
	if ((address >> 16) >> 16)
		return 0;
	return (address & 0xFF000000UL) == 0x0A000000UL || /* 10.0.0.0/8 */
		(address & 0xFFF00000UL) == 0xAC100000UL || /* 172.16.0.0/12 */
		(address & 0xFFFF0000UL) == 0xC0A80000UL || /* 192.168.0.0/16 */
		(address & 0xFFFF0000UL) == 0xA9FE0000UL || /* 169.254.0.0/16 */
		(address & 0xFF000000UL) == 0x7F000000UL; /* 127.0.0.0/8 */
}

int ae_map_share_path_in_folder(
	char const *path,
	char const *folder)
{
	size_t folder_length;
	size_t name_length;
	char const *name;
	size_t index;

	if (!path || !folder || !folder[0])
		return 0;
	folder_length = strlen(folder);
	if (strncmp(path, folder, folder_length) != 0)
		return 0;
	name = path + folder_length;
	name_length = strlen(name);
	/* (a name before .map) */
	if (name_length <= 4)
		return 0;
	for (index = 0; index < name_length; index++)
	{
		unsigned char c = (unsigned char)name[index];

		/* (no other folder: no separator, drive, or control character) */
		if (c == '\\' || c == '/' || c == ':' || c < 0x20)
			return 0;
	}
	/* (neither "." nor ".." nor anything that starts with them) */
	if (name[0] == '.' || strstr(name, ".."))
		return 0;
	return tolower((unsigned char)name[name_length - 4]) == '.' &&
		tolower((unsigned char)name[name_length - 3]) == 'm' &&
		tolower((unsigned char)name[name_length - 2]) == 'a' &&
		tolower((unsigned char)name[name_length - 1]) == 'p';
}

int ae_map_share_discard(
	char const *const *paths,
	int count,
	char const *folder,
	int (*delete_file)(char const *path, void *context),
	void *context)
{
	int removed = 0;
	int index;

	if (!paths || !delete_file)
		return 0;
	for (index = 0; index < count; index++)
	{
		if (!ae_map_share_path_in_folder(paths[index], folder))
			continue;
		if (delete_file(paths[index], context))
			removed++;
	}
	return removed;
}

/* ---------- sharing every map but the game's own */

static char const *const stock_names[] =
{
	/* multiplayer (custom_maps.c's stock list) */
	"beavercreek", "sidewinder", "damnation", "ratrace", "prisoner", "hangemhigh", "chillout", "carousel",
	"boardingaction", "bloodgulch", "wizard", "putput", "longest",
	/* the campaign */
	"a10", "a30", "a50", "b30", "b40", "c10", "c20", "c40", "d20", "d40",
	/* the menus */
	"ui",
	/* Custom Edition's resource maps */
	"bitmaps", "sounds", "loc",
	NULL
};

/* a name's length, at most limit (no end found) */
static size_t name_length(
	char const *name,
	size_t limit)
{
	size_t length = 0;

	while (length < limit && name[length])
		length++;
	return length;
}

static int same_name(
	char const *a,
	char const *b)
{
	for (; *a && *b; a++, b++)
	{
		if (tolower((unsigned char)*a) != tolower((unsigned char)*b))
			return 0;
	}
	return !*a && !*b;
}

char const *const *ae_map_share_stock_names(
	void)
{
	return stock_names;
}

int ae_map_share_stock_name(
	char const *name)
{
	int index;

	if (!name)
		return 0;
	for (index = 0; stock_names[index]; index++)
	{
		if (same_name(name, stock_names[index]))
			return 1;
	}
	return 0;
}

int ae_map_share_name_allowed(
	char const *name)
{
	size_t length;
	size_t index;

	if (!name)
		return 0;
	length = name_length(name, AE_MAP_SHARE_NAME_BYTES);
	if (!length || length >= AE_MAP_SHARE_NAME_BYTES)
		return 0;
	for (index = 0; index < length; index++)
	{
		unsigned char c = (unsigned char)name[index];

		if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-' ||
			c == '+' || c == '(' || c == ')' || c == '.' || c == ' '))
		{
			return 0;
		}
		if (c == '.' && name[index + 1] == '.')
			return 0;
	}
	return name[0] != '.' && name[0] != ' ' && name[length - 1] != '.' && name[length - 1] != ' ';
}

int ae_map_share_name_hashed(
	char const *name)
{
	size_t length = name ? name_length(name, AE_MAP_SHARE_NAME_BYTES) : 0;
	size_t index;

	if (length < 10 || length >= AE_MAP_SHARE_NAME_BYTES || name[length - 9] != '_')
		return 0;
	for (index = length - 8; index < length; index++)
	{
		if (!((name[index] >= '0' && name[index] <= '9') || (name[index] >= 'a' && name[index] <= 'f')))
			return 0;
	}
	return 1;
}

int ae_map_share_size_allowed(
	int kind,
	unsigned long long size)
{
	switch (kind)
	{
	case AE_MAP_SHARE_KIND_XBOX:
	case AE_MAP_SHARE_KIND_MOD:
		return size > 0 && size <= AE_MAP_SHARE_XBOX_MAXIMUM_BYTES;
	case AE_MAP_SHARE_KIND_CE:
		return size > 0 && size <= AE_MAP_SHARE_CE_MAXIMUM_BYTES;
	case AE_MAP_SHARE_KIND_RESOURCE:
		return size > 0 && size <= AE_MAP_SHARE_RESOURCE_MAXIMUM_BYTES;
	}
	return 0;
}

int ae_map_share_entry_valid(
	struct ae_map_share_entry const *entry)
{
	if (!entry || !memchr(entry->name, 0, sizeof(entry->name)) || !ae_map_share_name_allowed(entry->name) ||
		!ae_map_share_size_allowed(entry->kind, entry->size))
	{
		return 0;
	}
	switch (entry->kind)
	{
	case AE_MAP_SHARE_KIND_XBOX:
	case AE_MAP_SHARE_KIND_MOD:
		return strlen(entry->name) <= AE_MAP_SHARE_XBOX_NAME_LENGTH && !ae_map_share_stock_name(entry->name);
	case AE_MAP_SHARE_KIND_CE:
		return ae_map_share_name_hashed(entry->name) && !ae_map_share_stock_name(entry->name);
	case AE_MAP_SHARE_KIND_RESOURCE:
		return same_name(entry->name, "bitmaps") || same_name(entry->name, "sounds") || same_name(entry->name, "loc");
	}
	return 0;
}

int ae_map_share_catalogue_begin(
	struct ae_map_share_catalogue *catalogue,
	int expected)
{
	memset(catalogue, 0, sizeof(*catalogue));
	if (expected < 0 || expected > AE_MAP_SHARE_CATALOGUE_MAXIMUM)
		return 0;
	catalogue->expected = expected;
	return 1;
}

int ae_map_share_catalogue_find(
	struct ae_map_share_catalogue const *catalogue,
	char const *name,
	int resource)
{
	int index;

	if (!name)
		return -1;
	for (index = 0; index < catalogue->count; index++)
	{
		if ((catalogue->entries[index].kind == AE_MAP_SHARE_KIND_RESOURCE) == (resource != 0) &&
			same_name(catalogue->entries[index].name, name))
		{
			return index;
		}
	}
	return -1;
}

int ae_map_share_catalogue_take(
	struct ae_map_share_catalogue *catalogue,
	struct ae_map_share_frame const *frame)
{
	int index;

	if (frame->type != AE_MAP_SHARE_FRAME_CATALOGUE || frame->count < 0 || frame->count > AE_MAP_SHARE_FRAME_ENTRIES ||
		frame->expected != catalogue->expected || frame->first != catalogue->received ||
		frame->count > catalogue->expected - catalogue->received)
	{
		return 0;
	}
	catalogue->received += frame->count;
	for (index = 0; index < frame->count; index++)
	{
		struct ae_map_share_entry const *entry = &frame->entries[index];

		if (!ae_map_share_entry_valid(entry) || catalogue->count >= AE_MAP_SHARE_CATALOGUE_MAXIMUM ||
			ae_map_share_catalogue_find(catalogue, entry->name, entry->kind == AE_MAP_SHARE_KIND_RESOURCE) >= 0)
		{
			continue;
		}
		catalogue->entries[catalogue->count++] = *entry;
	}
	return 1;
}

int ae_map_share_catalogue_complete(
	struct ae_map_share_catalogue const *catalogue)
{
	return catalogue->received == catalogue->expected;
}

static unsigned long wire_long(
	unsigned char const *bytes)
{
	return (unsigned long)bytes[0] | (unsigned long)bytes[1] << 8 | (unsigned long)bytes[2] << 16 |
		(unsigned long)bytes[3] << 24;
}

int ae_map_share_frame_decode(
	unsigned char const *bytes,
	long size,
	struct ae_map_share_frame *frame)
{
	int index;

	memset(frame, 0, sizeof(*frame));
	if (!bytes || size < AE_MAP_SHARE_WIRE_FRAME_BYTES)
		return 0;
	frame->type = bytes[0];
	frame->flags = bytes[1];
	frame->expected = bytes[2] | bytes[3] << 8;
	frame->first = bytes[4] | bytes[5] << 8;
	frame->count = bytes[6];
	if ((frame->type != AE_MAP_SHARE_FRAME_HELLO && frame->type != AE_MAP_SHARE_FRAME_CATALOGUE) ||
		frame->count > AE_MAP_SHARE_FRAME_ENTRIES || (frame->type == AE_MAP_SHARE_FRAME_HELLO && frame->count) ||
		frame->expected > AE_MAP_SHARE_CATALOGUE_MAXIMUM ||
		size != AE_MAP_SHARE_WIRE_FRAME_BYTES + (long)frame->count * AE_MAP_SHARE_WIRE_ENTRY_BYTES)
	{
		return 0;
	}
	for (index = 0; index < frame->count; index++)
	{
		unsigned char const *wire = bytes + AE_MAP_SHARE_WIRE_FRAME_BYTES + index * AE_MAP_SHARE_WIRE_ENTRY_BYTES;
		struct ae_map_share_entry *entry = &frame->entries[index];

		if (!memchr(wire, 0, AE_MAP_SHARE_NAME_BYTES))
			return 0;
		memcpy(entry->name, wire, AE_MAP_SHARE_NAME_BYTES);
		entry->kind = wire[AE_MAP_SHARE_NAME_BYTES];
		entry->size = wire_long(wire + AE_MAP_SHARE_NAME_BYTES + 4);
		memcpy(entry->hash, wire + AE_MAP_SHARE_NAME_BYTES + 8, AE_MAP_SHARE_HASH_BYTES);
	}
	return 1;
}

long ae_map_share_frame_encode(
	struct ae_map_share_frame const *frame,
	unsigned char *bytes,
	long size)
{
	long needed;
	int index;

	if (!frame || frame->count < 0 || frame->count > AE_MAP_SHARE_FRAME_ENTRIES || frame->expected < 0 ||
		frame->expected > AE_MAP_SHARE_CATALOGUE_MAXIMUM || frame->first < 0 ||
		frame->first > AE_MAP_SHARE_CATALOGUE_MAXIMUM)
	{
		return 0;
	}
	needed = AE_MAP_SHARE_WIRE_FRAME_BYTES + (long)frame->count * AE_MAP_SHARE_WIRE_ENTRY_BYTES;
	if (!bytes || size < needed)
		return 0;
	memset(bytes, 0, (size_t)needed);
	bytes[0] = (unsigned char)frame->type;
	bytes[1] = (unsigned char)frame->flags;
	bytes[2] = (unsigned char)frame->expected;
	bytes[3] = (unsigned char)(frame->expected >> 8);
	bytes[4] = (unsigned char)frame->first;
	bytes[5] = (unsigned char)(frame->first >> 8);
	bytes[6] = (unsigned char)frame->count;
	for (index = 0; index < frame->count; index++)
	{
		unsigned char *wire = bytes + AE_MAP_SHARE_WIRE_FRAME_BYTES + index * AE_MAP_SHARE_WIRE_ENTRY_BYTES;
		struct ae_map_share_entry const *entry = &frame->entries[index];

		strncpy((char *)wire, entry->name, AE_MAP_SHARE_NAME_BYTES - 1);
		wire[AE_MAP_SHARE_NAME_BYTES] = entry->kind;
		wire[AE_MAP_SHARE_NAME_BYTES + 4] = (unsigned char)entry->size;
		wire[AE_MAP_SHARE_NAME_BYTES + 5] = (unsigned char)(entry->size >> 8);
		wire[AE_MAP_SHARE_NAME_BYTES + 6] = (unsigned char)(entry->size >> 16);
		wire[AE_MAP_SHARE_NAME_BYTES + 7] = (unsigned char)(entry->size >> 24);
		memcpy(wire + AE_MAP_SHARE_NAME_BYTES + 8, entry->hash, AE_MAP_SHARE_HASH_BYTES);
	}
	return needed;
}

int ae_map_share_offer_matches(
	struct ae_map_share_entry const *entry,
	unsigned long size,
	unsigned char const *hash)
{
	return entry && hash && entry->size == size && !memcmp(entry->hash, hash, AE_MAP_SHARE_HASH_BYTES);
}

long ae_map_share_cap_megabytes(
	long setting)
{
	return setting < 0 ? 0 : setting > AE_MAP_SHARE_CAP_MAXIMUM_MEGABYTES ? AE_MAP_SHARE_CAP_MAXIMUM_MEGABYTES : setting;
}

int ae_map_share_cap_allows(
	unsigned long long taken,
	unsigned long long size,
	long cap_megabytes)
{
	unsigned long long cap = (unsigned long long)ae_map_share_cap_megabytes(cap_megabytes) << 20;

	return size <= cap && taken <= cap - size;
}

int ae_map_share_disk_allows(
	unsigned long long free_bytes,
	unsigned long long size)
{
	return free_bytes >= AE_MAP_SHARE_DISK_MARGIN_BYTES && size <= free_bytes - AE_MAP_SHARE_DISK_MARGIN_BYTES;
}

int ae_map_share_policy_allows(
	int local,
	int setting,
	int internet_setting)
{
	return setting && (local || internet_setting);
}

int ae_map_share_consent_step(
	int state,
	int event)
{
	switch (event)
	{
	case AE_MAP_SHARE_CONSENT_EVENT_HOST:
		return AE_MAP_SHARE_CONSENT_UNASKED;
	case AE_MAP_SHARE_CONSENT_EVENT_NEED:
		return state == AE_MAP_SHARE_CONSENT_UNASKED ? AE_MAP_SHARE_CONSENT_ASKING : state;
	case AE_MAP_SHARE_CONSENT_EVENT_YES:
		return state == AE_MAP_SHARE_CONSENT_ASKING ? AE_MAP_SHARE_CONSENT_YES : state;
	case AE_MAP_SHARE_CONSENT_EVENT_NO:
		return state == AE_MAP_SHARE_CONSENT_ASKING ? AE_MAP_SHARE_CONSENT_NO : state;
	}
	return state;
}

int ae_map_share_consent_writes(
	int state)
{
	return state == AE_MAP_SHARE_CONSENT_YES;
}

int ae_map_share_rate_allow(
	struct ae_map_share_rate *rate,
	unsigned long now,
	int burst,
	unsigned long refill)
{
	if (!rate || burst < 1 || !refill)
		return 0;
	if (!rate->started)
	{
		rate->started = 1;
		rate->tokens = burst;
		rate->at = now;
	}
	else
	{
		/* (a clock of 32 bits that wraps: the difference still counts) */
		unsigned long passed = (now - rate->at) & 0xFFFFFFFFUL;
		unsigned long earned = passed / refill;

		if (earned)
		{
			rate->tokens = earned >= (unsigned long)(burst - rate->tokens) ? burst : rate->tokens + (int)earned;
			rate->at = (rate->at + earned * refill) & 0xFFFFFFFFUL;
		}
	}
	if (rate->tokens < 1)
		return 0;
	rate->tokens--;
	return 1;
}

int ae_map_share_plan(
	struct ae_map_share_catalogue const *catalogue,
	int (*lacks)(struct ae_map_share_entry const *entry, void *context),
	void *context,
	long cap_megabytes,
	unsigned long long taken,
	short *indices,
	int maximum,
	unsigned long long *bytes,
	int *over)
{
	unsigned long long total = 0;
	int resources_counted = 0;
	int count = 0;
	int skipped = 0;
	int index;

	for (index = 0; catalogue && lacks && index < catalogue->count; index++)
	{
		struct ae_map_share_entry const *entry = &catalogue->entries[index];
		unsigned long long extra = 0;

		if (entry->kind == AE_MAP_SHARE_KIND_RESOURCE || !lacks(entry, context))
			continue;
		/* (the first Custom Edition map brings the resource maps this machine lacks) */
		if (entry->kind == AE_MAP_SHARE_KIND_CE && !resources_counted)
		{
			int other;

			for (other = 0; other < catalogue->count; other++)
			{
				if (catalogue->entries[other].kind == AE_MAP_SHARE_KIND_RESOURCE &&
					lacks(&catalogue->entries[other], context))
				{
					extra += catalogue->entries[other].size;
				}
			}
		}
		if (count >= maximum || !ae_map_share_cap_allows(taken + total, entry->size + extra, cap_megabytes))
		{
			skipped++;
			continue;
		}
		if (entry->kind == AE_MAP_SHARE_KIND_CE)
			resources_counted = 1;
		total += entry->size + extra;
		if (indices)
			indices[count] = (short)index;
		count++;
	}
	if (bytes)
		*bytes = total;
	if (over)
		*over = skipped;
	return count;
}
