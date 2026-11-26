/*
AE_REPLAY_RULES.C

A replay file's pure rules (ae_replay_rules.h).
*/

#include "ae_replay_rules.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned long get_long(const unsigned char *source)
{
	return (unsigned long)source[0] | (unsigned long)source[1] << 8 | (unsigned long)source[2] << 16 |
		(unsigned long)source[3] << 24;
}

static void put_long(unsigned char *destination, unsigned long value)
{
	destination[0] = (unsigned char)(value & 0xFF);
	destination[1] = (unsigned char)((value >> 8) & 0xFF);
	destination[2] = (unsigned char)((value >> 16) & 0xFF);
	destination[3] = (unsigned char)((value >> 24) & 0xFF);
}

static void say(char *why, size_t why_size, const char *text)
{
	if (why && why_size)
		snprintf(why, why_size, "%s", text);
}

/* a text field into size bytes: printable ASCII only (others '_') when ascii, else any byte from 0x20 up (UTF-8);
the rest zero */
static void field_put(unsigned char *destination, size_t size, const char *source, int ascii)
{
	size_t index = 0;

	memset(destination, 0, size);
	if (!source)
		return;
	for (; index + 1 < size && source[index]; index++)
	{
		unsigned char character = (unsigned char)source[index];
		int kept = ascii ? character >= 0x20 && character < 0x7F : character >= 0x20 && character != 0x7F;

		destination[index] = kept ? character : '_';
	}
}

/* a text field of size bytes out: its end within them, and only the bytes field_put writes; 0 if not */
static int field_take(char *destination, const unsigned char *source, size_t size, int ascii)
{
	size_t index;
	int ended = 0;

	for (index = 0; index < size; index++)
	{
		unsigned char character = source[index];

		if (!character)
			ended = 1;
		else if (ended || character < 0x20 || character == 0x7F || (ascii && character >= 0x7F))
			return 0;
		destination[index] = ended ? 0 : (char)character;
	}
	destination[size] = 0;
	return ended;
}

void ae_replay_header_write(unsigned char *bytes, const struct ae_replay_header *header)
{
	memset(bytes, 0, AE_REPLAY_HEADER_SIZE);
	memcpy(bytes, AE_REPLAY_MAGIC, 8);
	put_long(bytes + 8, header->format);
	put_long(bytes + 12, header->network_version);
	put_long(bytes + 16, header->build);
	put_long(bytes + 20, header->ticks_per_second);
	put_long(bytes + 24, header->start_tick);
	put_long(bytes + 28, AE_REPLAY_HEADER_SIZE);
	put_long(bytes + 32, (unsigned long)(header->time & 0xFFFFFFFFULL));
	put_long(bytes + 36, (unsigned long)(header->time >> 32));
	field_put(bytes + AE_REPLAY_MAP_OFFSET, AE_REPLAY_MAP_SIZE, header->map, 1);
	field_put(bytes + AE_REPLAY_VARIANT_OFFSET, AE_REPLAY_VARIANT_SIZE, header->variant, 1);
	field_put(bytes + AE_REPLAY_VERSION_OFFSET, AE_REPLAY_VERSION_SIZE, header->version, 1);
	put_long(bytes + AE_REPLAY_LENGTH_OFFSET, header->length_ticks);
	put_long(bytes + AE_REPLAY_FLAGS_OFFSET, header->flags);
	field_put(bytes + AE_REPLAY_SERVER_OFFSET, AE_REPLAY_SERVER_SIZE, header->server, 0);
	field_put(bytes + AE_REPLAY_PLAYER_OFFSET, AE_REPLAY_PLAYER_SIZE, header->player, 0);
}

int ae_replay_header_read(const unsigned char *bytes, size_t size, unsigned long network_version,
	struct ae_replay_header *header, char *why, size_t why_size)
{
	struct ae_replay_header read;

	memset(&read, 0, sizeof read);
	if (header)
		memset(header, 0, sizeof *header);
	if (!bytes || size < AE_REPLAY_HEADER_SIZE || memcmp(bytes, AE_REPLAY_MAGIC, 8))
	{
		say(why, why_size, "not a replay");
		return AE_REPLAY_HEADER_NOT_REPLAY;
	}
	read.format = get_long(bytes + 8);
	read.network_version = get_long(bytes + 12);
	read.build = get_long(bytes + 16);
	read.ticks_per_second = get_long(bytes + 20);
	read.start_tick = get_long(bytes + 24);
	read.header_size = get_long(bytes + 28);
	read.time = (unsigned long long)get_long(bytes + 32) | (unsigned long long)get_long(bytes + 36) << 32;
	read.length_ticks = get_long(bytes + AE_REPLAY_LENGTH_OFFSET);
	read.flags = get_long(bytes + AE_REPLAY_FLAGS_OFFSET);
	if (read.format != AE_REPLAY_FORMAT)
	{
		say(why, why_size, read.format > AE_REPLAY_FORMAT ? "made by a later build" : "made by an earlier build");
		return AE_REPLAY_HEADER_OTHER_FORMAT;
	}
	if (read.network_version != network_version)
	{
		say(why, why_size, "recorded by a build of another network version");
		return AE_REPLAY_HEADER_OTHER_NETWORK;
	}
	if (read.header_size != AE_REPLAY_HEADER_SIZE || read.ticks_per_second != AE_REPLAY_TICKS_PER_SECOND ||
		read.length_ticks > AE_REPLAY_LONGEST_TICKS || read.start_tick > AE_REPLAY_START_TICK_MOST ||
		(read.flags & ~(unsigned long)AE_REPLAY_FLAG_CLIENT) ||
		!field_take(read.map, bytes + AE_REPLAY_MAP_OFFSET, AE_REPLAY_MAP_SIZE, 1) || !read.map[0] ||
		!field_take(read.variant, bytes + AE_REPLAY_VARIANT_OFFSET, AE_REPLAY_VARIANT_SIZE, 1) ||
		!field_take(read.version, bytes + AE_REPLAY_VERSION_OFFSET, AE_REPLAY_VERSION_SIZE, 1) ||
		!field_take(read.server, bytes + AE_REPLAY_SERVER_OFFSET, AE_REPLAY_SERVER_SIZE, 0) ||
		!field_take(read.player, bytes + AE_REPLAY_PLAYER_OFFSET, AE_REPLAY_PLAYER_SIZE, 0))
	{
		say(why, why_size, "its header is damaged");
		return AE_REPLAY_HEADER_DAMAGED;
	}
	if (header)
		*header = read;
	say(why, why_size, "");
	return AE_REPLAY_HEADER_OK;
}

int ae_replay_record_take(const unsigned char *bytes, size_t size, unsigned long last_tick, unsigned long most_tick,
	struct ae_replay_record *record)
{
	struct ae_replay_record read;

	if (!bytes || size < AE_REPLAY_RECORD_HEADER_SIZE)
		return AE_REPLAY_RECORD_NEED_MORE;
	read.kind = bytes[0];
	read.size = (unsigned long)bytes[2] | (unsigned long)bytes[3] << 8;
	read.tick = get_long(bytes + 4);
	if (read.kind < AE_REPLAY_RECORD_DATAGRAM || read.kind >= AE_REPLAY_RECORD_KINDS || bytes[1] ||
		read.tick < last_tick || read.tick > most_tick)
	{
		return AE_REPLAY_RECORD_BAD;
	}
	if (read.size > size - AE_REPLAY_RECORD_HEADER_SIZE)
		return AE_REPLAY_RECORD_NEED_MORE;
	if (record)
		*record = read;
	return AE_REPLAY_RECORD_OK;
}

int ae_replay_stream_check(const unsigned char *bytes, size_t size, unsigned long start_tick, unsigned long game_size,
	unsigned long setup_size, unsigned long message_least, unsigned long datagram_most, unsigned long message_most,
	struct ae_replay_stream *stream, char *why, size_t why_size)
{
	struct ae_replay_stream read;
	struct ae_replay_record record;
	size_t at = 0;
	unsigned long most_tick = start_tick > 0xFFFFFFFFUL - AE_REPLAY_LONGEST_TICKS ? 0xFFFFFFFFUL :
		start_tick + AE_REPLAY_LONGEST_TICKS;
	unsigned long last_tick = start_tick;

	memset(&read, 0, sizeof read);
	if (stream)
		memset(stream, 0, sizeof *stream);
	if (!bytes || size > AE_REPLAY_INFLATED_MOST)
	{
		say(why, why_size, !bytes ? "it has no records" : "it is too large to play");
		return 0;
	}
	for (;;)
	{
		int answer = ae_replay_record_take(bytes + at, size - at, last_tick, most_tick, &record);

		if (answer == AE_REPLAY_RECORD_NEED_MORE)
		{
			read.cut = at < size;
			break;
		}
		if (answer == AE_REPLAY_RECORD_BAD)
		{
			say(why, why_size, "a record of it is damaged");
			return 0;
		}
		if (read.records == 0 && (record.kind != AE_REPLAY_RECORD_GAME || record.size != game_size))
		{
			say(why, why_size, record.kind != AE_REPLAY_RECORD_GAME ? "it has no game to play" :
				"its game was recorded by another build");
			return 0;
		}
		if (read.records > 0 && record.kind == AE_REPLAY_RECORD_GAME)
		{
			say(why, why_size, "it has a second game");
			return 0;
		}
		if (record.kind == AE_REPLAY_RECORD_LEVEL_SETUP && (read.records != 1 || record.size != setup_size))
		{
			say(why, why_size, "its level setup is damaged");
			return 0;
		}
		if (record.kind <= AE_REPLAY_RECORD_GAME_MESSAGE && record.size < message_least)
		{
			say(why, why_size, "a record of it is damaged");
			return 0;
		}
		if ((record.kind == AE_REPLAY_RECORD_DATAGRAM && record.size > datagram_most) ||
			(record.kind > AE_REPLAY_RECORD_DATAGRAM && record.kind <= AE_REPLAY_RECORD_GAME_MESSAGE &&
			record.size > message_most))
		{
			say(why, why_size, "a record of it is larger than a game sends");
			return 0;
		}
		if (read.records == 0)
		{
			read.game_at = at + AE_REPLAY_RECORD_HEADER_SIZE;
			read.first_tick = record.tick;
		}
		read.kinds[record.kind]++;
		read.records++;
		last_tick = record.tick;
		at += AE_REPLAY_RECORD_HEADER_SIZE + record.size;
		if (record.kind == AE_REPLAY_RECORD_GAME || record.kind == AE_REPLAY_RECORD_LEVEL_SETUP)
			read.play_from = at;
	}
	if (!read.records)
	{
		say(why, why_size, "it has no game to play");
		return 0;
	}
	if (!read.kinds[AE_REPLAY_RECORD_DATAGRAM])
	{
		say(why, why_size, "it has nothing of the game to show");
		return 0;
	}
	read.last_tick = last_tick;
	read.end = at;
	if (stream)
		*stream = read;
	say(why, why_size, "");
	return 1;
}

int ae_replay_record_header_write(unsigned char *bytes, int kind, unsigned long size, unsigned long tick)
{
	if (kind < AE_REPLAY_RECORD_DATAGRAM || kind >= AE_REPLAY_RECORD_KINDS || size > AE_REPLAY_RECORD_MOST)
		return 0;
	bytes[0] = (unsigned char)kind;
	bytes[1] = 0;
	bytes[2] = (unsigned char)(size & 0xFF);
	bytes[3] = (unsigned char)((size >> 8) & 0xFF);
	put_long(bytes + 4, tick);
	return 1;
}

static int name_character(char character)
{
	return (character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') ||
		(character >= '0' && character <= '9') || character == '-';
}

static int digits(const char *text, int count)
{
	int index;

	for (index = 0; index < count; index++)
	{
		if (text[index] < '0' || text[index] > '9')
			return 0;
	}
	return 1;
}

/* whether text is a stamp: "yyyymmdd-hhmmss" */
static int stamp_valid(const char *text)
{
	return text && strlen(text) >= 15 && digits(text, 8) && text[8] == '-' && digits(text + 9, 6);
}

int ae_replay_name_make(char *name, size_t size, const char *stamp, const char *map)
{
	char cleaned[AE_REPLAY_NAME_SIZE];
	size_t index, length = 0;
	int written;

	if (!name || !size)
		return 0;
	name[0] = 0;
	if (!stamp_valid(stamp) || stamp[15])
		return 0;
	for (index = 0; map && map[index] && length + 1 < sizeof cleaned; index++)
		cleaned[length++] = name_character(map[index]) ? map[index] : '_';
	cleaned[length] = 0;
	written = snprintf(name, size, "%s_%s%s", stamp, length ? cleaned : "map", AE_REPLAY_EXTENSION);
	if (written < 0 || (size_t)written >= size || (size_t)written >= AE_REPLAY_NAME_SIZE)
	{
		name[0] = 0;
		return 0;
	}
	return 1;
}

int ae_replay_name_is_safe(const char *name)
{
	size_t length = name ? strlen(name) : 0, extension = strlen(AE_REPLAY_EXTENSION);

	return length > extension && length < AE_REPLAY_NAME_SIZE && !strchr(name, '/') && !strchr(name, '\\') &&
		!strstr(name, "..") && !strcmp(name + length - extension, AE_REPLAY_EXTENSION);
}

int ae_replay_name_is_own(const char *name)
{
	size_t length, extension = strlen(AE_REPLAY_EXTENSION), index;

	if (!ae_replay_name_is_safe(name) || !stamp_valid(name) || name[15] != '_')
		return 0;
	length = strlen(name);
	if (length <= 16 + extension)
		return 0;
	for (index = 16; index < length - extension; index++)
	{
		if (!name_character(name[index]) && name[index] != '_')
			return 0;
	}
	return 1;
}

int ae_replay_keep_marker(const char *name, char *marker, size_t size)
{
	size_t length;

	if (!ae_replay_name_is_safe(name))
		return 0;
	length = strlen(name) + strlen(AE_REPLAY_KEEP_EXTENSION);
	if (length >= size)
		return 0;
	snprintf(marker, size, "%s%s", name, AE_REPLAY_KEEP_EXTENSION);
	return 1;
}

long ae_replay_kept(long value)
{
	return value < AE_REPLAY_KEPT_LEAST ? AE_REPLAY_KEPT_LEAST : value > AE_REPLAY_KEPT_MOST ? AE_REPLAY_KEPT_MOST : value;
}

long ae_replay_budget_mb(long value)
{
	return value < AE_REPLAY_BUDGET_LEAST_MB ? AE_REPLAY_BUDGET_LEAST_MB :
		value > AE_REPLAY_BUDGET_MOST_MB ? AE_REPLAY_BUDGET_MOST_MB : value;
}

static const struct ae_replay_file *sort_files;

static int compare_names(const void *first, const void *second)
{
	return strcmp(sort_files[*(const int *)first].name, sort_files[*(const int *)second].name);
}

int ae_replay_pick_removals(const struct ae_replay_file *files, int count, long kept, unsigned long long budget_bytes,
	unsigned long long incoming_bytes, int *picked, int most)
{
	int *own;
	int own_count = 0, index, picks = 0;
	unsigned long long total = 0;

	if (!files || count <= 0 || !picked || most <= 0)
		return 0;
	own = malloc(sizeof(*own) * (size_t)count);
	if (!own)
		return 0;
	for (index = 0; index < count; index++)
	{
		if (ae_replay_name_is_own(files[index].name))
		{
			own[own_count++] = index;
			total += files[index].bytes;
		}
	}
	sort_files = files;
	qsort(own, (size_t)own_count, sizeof(*own), compare_names);
	kept = ae_replay_kept(kept);
	/* (the oldest first, until the new one's room is made in both caps) */
	for (index = 0; index < own_count && picks < most; index++)
	{
		int over_count = (long)(own_count - index) > kept - 1;
		int over_budget = total + incoming_bytes > budget_bytes;

		if (!over_count && !over_budget)
			break;
		picked[picks++] = own[index];
		total -= files[own[index]].bytes;
	}
	free(own);
	return picks;
}

int ae_replay_disk_allows(unsigned long long free_bytes)
{
	return free_bytes >= (unsigned long long)AE_REPLAY_DISK_RESERVE_MB * 1024ULL * 1024ULL;
}

void ae_replay_cost_add(struct ae_replay_cost *cost, unsigned long us)
{
	cost->ticks++;
	cost->total_us += us;
	if (us > cost->most_us)
		cost->most_us = us;
}

unsigned long ae_replay_cost_mean(const struct ae_replay_cost *cost)
{
	return cost->ticks ? (unsigned long)(cost->total_us / cost->ticks) : 0;
}

void ae_replay_setup_cache_write(unsigned char *setup, unsigned long serial, const unsigned char *id)
{
	int at;

	setup[2] = 0;
	for (at = 4; at < 8 + AE_REPLAY_CACHE_ID_SIZE; at++)
		setup[at] = 0;
	if (!serial || !id)
		return;
	setup[2] = 1;
	for (at = 0; at < 4; at++)
		setup[4 + at] = (unsigned char)(serial >> (8 * at));
	memcpy(setup + 8, id, AE_REPLAY_CACHE_ID_SIZE);
}

int ae_replay_setup_cache_matches(const unsigned char *setup, unsigned long serial, const unsigned char *id, char *why,
	size_t why_size)
{
	unsigned long recorded = 0;
	int at;

	if (why_size)
		why[0] = 0;
	if (!setup[2])
		return 1;
	for (at = 0; at < 4; at++)
		recorded |= (unsigned long)setup[4 + at] << (8 * at);
	if (!serial || !id)
	{
		snprintf(why, why_size, "its game played with weapon set %lu, and this machine has no weapon set loaded", recorded);
		return 0;
	}
	if (recorded != (serial & 0xFFFFFFFFUL) || memcmp(setup + 8, id, AE_REPLAY_CACHE_ID_SIZE))
	{
		snprintf(why, why_size, "its game played with weapon set %lu, another than this machine's (%lu)", recorded,
			serial & 0xFFFFFFFFUL);
		return 0;
	}
	return 1;
}

int ae_replay_game_counts_check(long machine_count, long player_count, long maximum_players, long difficulty,
	long machines_most, long players_most, long difficulties, char *why, size_t why_size)
{
	if (machine_count < 0 || machine_count > machines_most || player_count < 0 || player_count > players_most ||
		maximum_players < 1 || maximum_players > players_most)
	{
		say(why, why_size, "its game's player or machine count is out of range");
		return 0;
	}
	if (difficulty < 0 || difficulty >= difficulties)
	{
		say(why, why_size, "its game's difficulty is out of range");
		return 0;
	}
	say(why, why_size, "");
	return 1;
}

int ae_replay_players_check(const long *machines, const long *controllers, int count, long machines_most,
	long locals_most, char *why, size_t why_size)
{
	unsigned char taken[256][8];
	int index;

	if (machines_most < 1 || machines_most > 256 || locals_most < 1 || locals_most > 8 || count < 0)
	{
		say(why, why_size, "its game's players cannot be checked");
		return 0;
	}
	memset(taken, 0, sizeof taken);
	for (index = 0; index < count; index++)
	{
		long machine = machines[index], controller = controllers[index];

		if (machine < 0 || machine >= machines_most || controller < 0 || controller >= locals_most)
			continue;
		if (taken[machine][controller]++)
		{
			say(why, why_size, "its game has two players of one machine's one controller");
			return 0;
		}
	}
	say(why, why_size, "");
	return 1;
}

int ae_replay_map_name_safe(const char *name, size_t size)
{
	static const char *const devices[] = { "con", "prn", "aux", "nul", "com", "lpt" };
	const char *leaf, *at, *end;
	size_t stem, length;
	int device;

	if (!name || !size || !(end = memchr(name, 0, size)))
		return 0;
	for (at = name; at < end; at++)
	{
		char c = *at;

		if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-' ||
			c == '.' || c == ' ' || c == '\\' || c == '@' || c == '[' || c == ']' || c == '(' || c == ')' || c == '+'))
			return 0;
	}
	if (strstr(name, ".."))
		return 0;
	leaf = strrchr(name, '\\');
	leaf = leaf ? leaf + 1 : name;
	length = strlen(leaf);
	if (!length || strspn(leaf, ". ") == length)
		return 0;
	stem = strcspn(leaf, ".@ ");
	for (device = 0; device < (int)(sizeof devices / sizeof devices[0]); device++)
	{
		int same = length >= 3, i;

		for (i = 0; same && i < 3; i++)
		{
			char c = leaf[i];

			if (c >= 'A' && c <= 'Z')
				c = (char)(c - 'A' + 'a');
			if (c != devices[device][i])
				same = 0;
		}
		if (same && ((device < 4 && stem == 3) || (device >= 4 && stem == 4 && leaf[3] >= '0' && leaf[3] <= '9')))
			return 0;
	}
	return 1;
}
