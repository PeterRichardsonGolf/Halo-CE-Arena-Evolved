/*
AE_MAP_SHARE_RULES.C

Arena Evolved: map sharing's safety rules (ae_map_share_rules.h). From
DamnationCE v0.3.41 (CC0), the address ranges as its address_local.
*/

#include "ae_map_share_rules.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

static int ci_equal(
	char const *a,
	char const *b)
{
	for (; *a && *b; a++, b++)
	{
		if (tolower((unsigned char)*a) != tolower((unsigned char)*b))
			return 1;
	}
	return *a != *b;
}

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

static int charge_find(
	struct ae_map_share_charges const *charges,
	char const *key)
{
	int index;

	for (index = 0; charges && key && key[0] && index < AE_MAP_SHARE_CHARGES; index++)
	{
		if (same_name(charges->items[index].key, key))
			return index;
	}
	return -1;
}

unsigned long long ae_map_share_charge_growth(
	struct ae_map_share_charges const *charges,
	char const *key,
	unsigned long long size)
{
	int index = charge_find(charges, key);

	if (index < 0)
		return size;
	return size > charges->items[index].size ? size - charges->items[index].size : 0;
}

void ae_map_share_charge_set(
	struct ae_map_share_charges *charges,
	char const *key,
	unsigned long long size)
{
	int index = charge_find(charges, key);

	if (!charges || !key || !key[0] || name_length(key, AE_MAP_SHARE_CHARGE_KEY_BYTES) >= AE_MAP_SHARE_CHARGE_KEY_BYTES)
		return;
	if (index < 0)
	{
		index = charges->next;
		charges->next = (charges->next + 1) % AE_MAP_SHARE_CHARGES;
		memset(&charges->items[index], 0, sizeof(charges->items[index]));
		strcpy(charges->items[index].key, key);
	}
	if (size > charges->items[index].size)
		charges->items[index].size = size;
}

int ae_map_share_policy_allows(
	int local,
	int setting,
	int internet_setting)
{
	return setting && (local || internet_setting);
}

void ae_map_share_display_name(
	char const *name,
	char *out,
	int size)
{
	char work[128];
	char const *slash;
	size_t length;
	int index;
	int word_start = 1;

	if (size <= 0)
		return;
	out[0] = 0;
	if (!name)
		return;
	slash = strrchr(name, '\\');
	if (strrchr(name, '/') > slash)
		slash = strrchr(name, '/');
	name = slash ? slash + 1 : name;
	length = strlen(name);
	if (length >= sizeof(work))
		length = sizeof(work) - 1;
	memcpy(work, name, length);
	work[length] = 0;
	/* (a Custom Edition level's "@ce") */
	{
		char *at = strchr(work, '@');

		if (at)
			*at = 0;
		length = strlen(work);
	}
	if (length > 4 && !ci_equal(work + length - 4, ".map"))
		work[length -= 4] = 0;
	/* ("_" and 8 hexadecimal digits: a second version's, its hash's first digits) */
	if (length > 9 && work[length - 9] == '_')
	{
		int hex = 1;

		for (index = 1; index <= 8; index++)
		{
			char c = work[length - 9 + index];

			if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')))
				hex = 0;
		}
		if (hex)
			work[length -= 9] = 0;
	}
	for (index = 0; work[index] && index < size - 1; index++)
	{
		char c = work[index] == '_' ? ' ' : work[index];

		if (c >= 'a' && c <= 'z' && word_start)
			c = (char)(c - 'a' + 'A');
		word_start = c == ' ';
		out[index] = c;
	}
	out[index] = 0;
}

void ae_map_share_megabytes_text(
	unsigned long long bytes,
	char *out,
	int size)
{
	if (bytes >= 10ULL * 1048576)
		snprintf(out, size, "%llu MB", (bytes + 524288) >> 20);
	else
		snprintf(out, size, "%llu.%llu MB", bytes / 1048576, (bytes % 1048576) * 10 / 1048576);
}

void ae_map_share_amount_text(
	unsigned long long done,
	unsigned long long total,
	char *out,
	int size)
{
	if (total >= 10ULL * 1048576)
		snprintf(out, size, "%llu of %llu MB", (done + 524288) >> 20, (total + 524288) >> 20);
	else
		snprintf(out, size, "%llu.%llu of %llu.%llu MB", done / 1048576, (done % 1048576) * 10 / 1048576,
			total / 1048576, (total % 1048576) * 10 / 1048576);
}

int ae_map_share_why(
	char const *reason)
{
	if (!reason)
		return AE_MAP_SHARE_WHY_OTHER;
	if (strstr(reason, "DOWNLOAD MAPS is OFF"))
		return AE_MAP_SHARE_WHY_SETTING_OFF;
	if (strstr(reason, "INTERNET DOWNLOADS is OFF"))
		return AE_MAP_SHARE_WHY_INTERNET_OFF;
	if (strstr(reason, "was stopped"))
		return AE_MAP_SHARE_WHY_STOPPED;
	if (strstr(reason, "does not share"))
		return AE_MAP_SHARE_WHY_NOT_SHARED;
	if (strstr(reason, "you take from one host"))
		return AE_MAP_SHARE_WHY_OVER_LIMIT;
	if (strstr(reason, "cannot share") || strstr(reason, "does not have this map") ||
		strstr(reason, "its list of maps does not have") || strstr(reason, "kind that is not downloaded") ||
		strstr(reason, "larger than the game takes"))
	{
		return AE_MAP_SHARE_WHY_CANNOT_SEND;
	}
	if (strstr(reason, "did not send its list") || strstr(reason, "did not answer"))
		return AE_MAP_SHARE_WHY_NO_ANSWER;
	if (strstr(reason, "offer was not of this map") || strstr(reason, "not the map of its list"))
		return AE_MAP_SHARE_WHY_WRONG_FILE;
	if (strstr(reason, "enough memory"))
		return AE_MAP_SHARE_WHY_NO_MEMORY;
	if (strstr(reason, "a file of its name is there"))
		return AE_MAP_SHARE_WHY_NAME_TAKEN;
	if (strstr(reason, "could not be written") || strstr(reason, "free disk space"))
		return AE_MAP_SHARE_WHY_DISK_FULL;
	if (strstr(reason, "cannot be played"))
		return AE_MAP_SHARE_WHY_WONT_LOAD;
	if (strstr(reason, "no player sent"))
		return AE_MAP_SHARE_WHY_STALLED;
	return AE_MAP_SHARE_WHY_OTHER;
}

char const *ae_map_share_why_tag(
	int why)
{
	switch (why)
	{
	case AE_MAP_SHARE_WHY_NOT_SHARED: return "not shared";
	case AE_MAP_SHARE_WHY_CANNOT_SEND: return "host can't send";
	case AE_MAP_SHARE_WHY_WRONG_FILE: return "wrong file";
	case AE_MAP_SHARE_WHY_NO_MEMORY: return "out of memory";
	case AE_MAP_SHARE_WHY_DISK_FULL: return "disk full";
	case AE_MAP_SHARE_WHY_NAME_TAKEN: return "name taken";
	case AE_MAP_SHARE_WHY_WONT_LOAD: return "won't load";
	case AE_MAP_SHARE_WHY_NO_ANSWER: return "no answer";
	case AE_MAP_SHARE_WHY_STALLED: return "stalled";
	case AE_MAP_SHARE_WHY_OVER_LIMIT: return "over your limit";
	case AE_MAP_SHARE_WHY_SETTING_OFF: return "setting off";
	case AE_MAP_SHARE_WHY_INTERNET_OFF: return "setting off";
	}
	return "failed";
}

void ae_map_share_why_line(
	int why,
	long limit_megabytes,
	char *out,
	int size)
{
	switch (why)
	{
	case AE_MAP_SHARE_WHY_NOT_SHARED: snprintf(out, size, "Ask the host to share maps."); break;
	case AE_MAP_SHARE_WHY_CANNOT_SEND: snprintf(out, size, "Only the host can fix this."); break;
	case AE_MAP_SHARE_WHY_WRONG_FILE: snprintf(out, size, "It was thrown away. Join again."); break;
	case AE_MAP_SHARE_WHY_NO_MEMORY: snprintf(out, size, "Close programs, then join again."); break;
	case AE_MAP_SHARE_WHY_DISK_FULL: snprintf(out, size, "Free up space, then join again."); break;
	case AE_MAP_SHARE_WHY_NAME_TAKEN: snprintf(out, size, "Move your copy out of maps_ce."); break;
	case AE_MAP_SHARE_WHY_WONT_LOAD: snprintf(out, size, "This map does not work here."); break;
	case AE_MAP_SHARE_WHY_NO_ANSWER: snprintf(out, size, "Join again in a minute."); break;
	case AE_MAP_SHARE_WHY_STALLED: snprintf(out, size, "Check your internet, join again."); break;
	case AE_MAP_SHARE_WHY_OVER_LIMIT: snprintf(out, size, "Over your %ld MB host limit.", limit_megabytes); break;
	case AE_MAP_SHARE_WHY_SETTING_OFF: snprintf(out, size, "Turn on DOWNLOAD MAPS."); break;
	case AE_MAP_SHARE_WHY_INTERNET_OFF: snprintf(out, size, "Turn on INTERNET DOWNLOADS."); break;
	default: snprintf(out, size, "Join again."); break;
	}
}

void ae_map_share_why_dialog(
	int why,
	long limit_megabytes,
	char const *reason,
	char *out,
	int size)
{
	switch (why)
	{
	case AE_MAP_SHARE_WHY_NOT_SHARED:
		snprintf(out, size, "The host does not share its maps. Ask the host to turn on SHARE MAPS, then join again.");
		break;
	case AE_MAP_SHARE_WHY_CANNOT_SEND:
		snprintf(out, size, "The host cannot send this map. Only the host can fix this.");
		break;
	case AE_MAP_SHARE_WHY_WRONG_FILE:
		snprintf(out, size, "The host sent a different file than it listed, so it was thrown away. Join again.");
		break;
	case AE_MAP_SHARE_WHY_NO_MEMORY:
		snprintf(out, size, "Your computer ran out of memory. Close other programs, then join again.");
		break;
	case AE_MAP_SHARE_WHY_DISK_FULL:
		snprintf(out, size, "It could not be saved: is your disk full? Free up space, then join again.");
		break;
	case AE_MAP_SHARE_WHY_NAME_TAKEN:
		snprintf(out, size, "You have a different map with this name in maps_ce. Move it out, then join again.");
		break;
	case AE_MAP_SHARE_WHY_WONT_LOAD:
		snprintf(out, size, "The map arrived but does not work in this game. Tell the host.");
		break;
	case AE_MAP_SHARE_WHY_NO_ANSWER:
		snprintf(out, size, "The host did not answer. Join again in a minute.");
		break;
	case AE_MAP_SHARE_WHY_STALLED:
		snprintf(out, size, "Nothing arrived for a minute. Check your connection, then join again.");
		break;
	case AE_MAP_SHARE_WHY_OVER_LIMIT:
		snprintf(out, size, "This host's maps are over the %ld MB you take from one host.", limit_megabytes);
		break;
	case AE_MAP_SHARE_WHY_SETTING_OFF:
		snprintf(out, size, "DOWNLOAD MAPS is OFF. Turn it on in Network Setup, Map Sharing, then join again.");
		break;
	case AE_MAP_SHARE_WHY_INTERNET_OFF:
		snprintf(out, size, "This is an internet game and INTERNET DOWNLOADS is OFF. Turn it on in Map Sharing.");
		break;
	default:
		snprintf(out, size, "%s", reason ? reason : "The map is not downloaded.");
		break;
	}
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
