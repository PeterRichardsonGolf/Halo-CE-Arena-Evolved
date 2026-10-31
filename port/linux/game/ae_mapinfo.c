/*
AE_MAPINFO.C

The AE menus' map-file reader (ae_mapinfo.h), a port of the tools' Python
reader (notes/map-metadata/mapmeta.py, REPORT.md "Layouts"), which it must
agree with. Layouts, from the game's own definitions:

- the cache file header (source/cache/cache_files.c:229-243): 'daeh',
  version 0x4, tag data offset 0x10 and size 0x14,
  name 0x20, build 0x40, scenario type 0x60 (0 solo, 1 multiplayer, 2 ui;
  scenario_definitions.h:22-24); OpenSauce's 'yelo' block at 0x70;
- the tag data: an Xbox map (version 5) is zlib after its 2048-byte header,
  its tag data at 0x803A6000 (physical_memory_map.c:48) behind a 0x24 tag
  header (cache_files.c:171); a Custom Edition (609) or HaloMD (7) map is
  plain, at 0x40440000 (cache_files.c:207) behind a 0x28 one (:211); its tag
  header: the tag instances' address, the scenario's tag index, a checksum,
  the tag count; a tag instance is 0x20 (:160): group, ..., name address
  0x10, data address 0x14;
- the scenario (scenario_definitions.h:244-319): vehicles 0x240, player
  starting locations 0x354 (0x34 each: position 0x0, team 0x10, game types
  0x14), netgame flags 0x378 (0x94 each: position 0x0, type 0x10, team 0x12;
  types :47), netgame equipment 0x384 (0x90 each: game types 0x4, position
  0x40), BSP references 0x5A4; a block is a count and an address;
- game types (game_engine.h:95): 1 ctf, 2 slayer, 3 oddball, 4 king, 5 race;
  12 all, 13 all but ctf, 14 all but ctf and race (game_engine.c:643):
  match_game_type (game_engine.c, "match_game_type"), with a game running.

Which game types a map supports is mapmeta.py's rule: the game type's own
objects are there (ctf: flags of teams 0 and 1; oddball: a ball spawn; king:
a hill; race: two race checkpoints) and it has at least 4 starting locations
for it. The engine's game_engine_verify_current_map (game_engine.c) asks more
(hills and ball spawns of teams 0 and 1, equipment for each game type), but
only logs what is missing and plays the map anyway: stock Damnation, whose
hills have no team 1, plays King. So its stricter checks would refuse maps
that play; they are not the rule here.

Every pointer and count is checked against what was read before it is used
(as ce_map_checks.c does): a map cut short, a pointer out of range or a
count past the tag data makes it unreadable, never a read past the buffer.
On valid maps this reader and mapmeta.py agree; on corrupt ones it is the
stricter: a negative or huge block count makes the map unreadable (mapmeta.py
takes it as empty), and NaN, infinite or far-off positions (beyond 10^6
units) are left out of the play area's spread.
*/

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef AE_MAPINFO_SYSTEM_ZLIB
/* (the unit test: the system's zlib) */
#include <zlib.h>
#else
/* the port's zlib (its names z_-prefixed: cache_files_decompress_windows.c) */
#include "../../port/third_party/zlib/zlib_prefixed.h"
#endif

#include "ae_mapinfo.h"

enum
{
	HEADER_SIZE = 0x800,
	XBOX_VERSION = 5,
	CE_VERSION = 609,
	HALOMD_VERSION = 7,
	MCC_VERSION = 13,
	XBOX_TAG_HEADER_SIZE = 0x24,
	CE_TAG_HEADER_SIZE = 0x28,
	TAG_INSTANCE_SIZE = 0x20,
	/* the engine's own limits: no map with more tag data than its tag cache holds can be played
	(CE_TAG_CACHE_SIZE, cache_files.c; the Xbox's TAG_CACHE_SIZE, physical_memory_map.h, is smaller), nor one larger
	than its largest cache file (SOLO_CACHE_FILE_MAXIMUM_SIZE, cache_files_windows.c) */
	MAXIMUM_TAG_DATA = 0x1700000,
	MAXIMUM_DECOMPRESSED = 0x11600000,
	MAXIMUM_TAGS = 65535,
	/* (blocks: more than any map's; Sanctuary's 256 starting locations are the most seen) */
	MAXIMUM_BLOCK_COUNT = 16384,
	CHUNK = 64 * 1024,

	SCENARIO_SIZE = 0x5B0,
	SCENARIO_VEHICLES = 0x240,
	SCENARIO_STARTS = 0x354,
	SCENARIO_FLAGS = 0x378,
	SCENARIO_EQUIPMENT = 0x384,
	SCENARIO_BSPS = 0x5A4,
	START_SIZE = 0x34,
	FLAG_SIZE = 0x94,
	EQUIPMENT_SIZE = 0x90,

	FLAG_CTF = 0,
	FLAG_ODDBALL = 2,
	FLAG_RACE = 3,
	FLAG_HILL = 8,

	GAME_TYPE_ALL = 12,
	GAME_TYPE_ALL_BUT_CTF = 13,
	GAME_TYPE_ALL_BUT_CTF_RACE = 14
};

#define XBOX_TAG_BASE 0x803A6000UL
#define CE_TAG_BASE 0x40440000UL
#define XBOX_STOCK_BUILD "01.10.12.2276"

/* the tag data, as addresses base .. base + size */
struct tag_data
{
	unsigned char *bytes;
	unsigned long base, size;
};

static unsigned long u32(unsigned char const *p)
{
	return (unsigned long)p[0] | (unsigned long)p[1] << 8 | (unsigned long)p[2] << 16 | (unsigned long)p[3] << 24;
}

static long s32(unsigned char const *p)
{
	unsigned long value = u32(p);

	return value & 0x80000000UL ? -(long)(0xFFFFFFFFUL - value) - 1 : (long)value;
}

static int s16(unsigned char const *p)
{
	int value = p[0] | p[1] << 8;

	return value & 0x8000 ? value - 0x10000 : value;
}

static float f32(unsigned char const *p)
{
	unsigned long bits = u32(p);
	float value;
	unsigned int word = (unsigned int)bits;

	memcpy(&value, &word, sizeof(value));
	return value;
}

/* n bytes at an address of the tag data, or NULL if any of them is outside */
static unsigned char const *at(struct tag_data const *tags, unsigned long address, unsigned long n)
{
	unsigned long offset;

	if (address < tags->base)
		return NULL;
	offset = address - tags->base;
	if (offset > tags->size || n > tags->size - offset)
		return NULL;
	return tags->bytes + offset;
}

/* a block of count elements of size each: its first, or NULL when it is out of range (count 0: not NULL) */
static unsigned char const *block(struct tag_data const *tags, unsigned char const *field, unsigned long size,
	long *count)
{
	static unsigned char const empty = 0;
	unsigned char const *first;

	*count = s32(field);
	if (*count < 0 || *count > MAXIMUM_BLOCK_COUNT)
		return NULL;
	if (*count == 0)
		return &empty;
	first = at(tags, u32(field + 4), (unsigned long)*count * size);
	return first;
}

static void copy_text(char *out, size_t size, unsigned char const *in, size_t length)
{
	size_t index;

	for (index = 0; index + 1 < size && index < length && in[index]; index++)
		out[index] = (char)in[index];
	out[index] = 0;
}

/* game_engine.c's match_game_type, with a game running */
static int match_game_type(int game_type, unsigned char const *game_types)
{
	int index, result = 0;

	for (index = 0; index < 4; index++)
	{
		int entry = s16(game_types + index * 2);

		result |= entry == game_type;
		if (entry == GAME_TYPE_ALL)
			result = 1;
		else if (entry == GAME_TYPE_ALL_BUT_CTF)
			result |= game_type != 1;
		else if (entry == GAME_TYPE_ALL_BUT_CTF_RACE)
			result |= game_type != 1 && game_type != 5;
	}
	return result;
}

static int compare_shorts(void const *a, void const *b)
{
	return *(short const *)a - *(short const *)b;
}

/* the netgame flags of a type: how many distinct teams, and whether teams 0 and 1 are among them */
static int flag_teams(unsigned char const *flags, long count, int type, short *scratch, int *teams_0_and_1)
{
	long index, used = 0, distinct = 0;
	int zero = 0, one = 0;

	for (index = 0; index < count; index++)
	{
		unsigned char const *flag = flags + index * FLAG_SIZE;

		if (s16(flag + 0x10) == type)
		{
			short team = (short)s16(flag + 0x12);

			scratch[used++] = team;
			zero |= team == 0;
			one |= team == 1;
		}
	}
	qsort(scratch, (size_t)used, sizeof(short), compare_shorts);
	for (index = 0; index < used; index++)
	{
		if (!index || scratch[index] != scratch[index - 1])
			distinct++;
	}
	*teams_0_and_1 = zero && one;
	return (int)distinct;
}

/* the scenario's facts, from the tag data */
static int read_scenario(struct tag_data const *tags, unsigned long tag_header_size, struct ae_mapinfo *info)
{
	unsigned char const *header = at(tags, tags->base, tag_header_size);
	unsigned char const *instance, *scenario, *starts, *flags, *equipment, *field;
	long tag_count, scenario_index, start_count, flag_count, equipment_count, count, index;
	short *scratch;
	int game_type, both, ctf_both, ball_spawns, hills, race_checkpoints;
	double minimum_x = 0.0, maximum_x = 0.0, minimum_y = 0.0, maximum_y = 0.0;
	int points = 0;
	char group[5];

	if (!header)
		return AE_MAPINFO_UNREADABLE;
	tag_count = s32(header + 0xC);
	scenario_index = s32(header + 4) & 0xFFFF;
	if (tag_count <= 0 || tag_count > MAXIMUM_TAGS || scenario_index >= tag_count)
		return AE_MAPINFO_UNREADABLE;
	instance = at(tags, u32(header) + (unsigned long)scenario_index * TAG_INSTANCE_SIZE, TAG_INSTANCE_SIZE);
	if (!instance)
		return AE_MAPINFO_UNREADABLE;
	/* (a protected Custom Edition map relabels its scenario's group: ce_repairs.c's repair) */
	group[0] = (char)instance[3];
	group[1] = (char)instance[2];
	group[2] = (char)instance[1];
	group[3] = (char)instance[0];
	group[4] = 0;
	info->protected_map = strcmp(group, "scnr") != 0;
	scenario = at(tags, u32(instance + 0x14), SCENARIO_SIZE);
	if (!scenario)
		return AE_MAPINFO_UNREADABLE;

	starts = block(tags, scenario + SCENARIO_STARTS, START_SIZE, &start_count);
	flags = block(tags, scenario + SCENARIO_FLAGS, FLAG_SIZE, &flag_count);
	equipment = block(tags, scenario + SCENARIO_EQUIPMENT, EQUIPMENT_SIZE, &equipment_count);
	if (!starts || !flags || !equipment)
		return AE_MAPINFO_UNREADABLE;
	/* (vehicles and BSPs: their counts) */
	field = scenario + SCENARIO_VEHICLES;
	count = s32(field);
	if (count < 0 || count > MAXIMUM_BLOCK_COUNT)
		return AE_MAPINFO_UNREADABLE;
	info->vehicles = (int)count;
	field = scenario + SCENARIO_BSPS;
	count = s32(field);
	if (count < 0 || count > MAXIMUM_BLOCK_COUNT)
		return AE_MAPINFO_UNREADABLE;
	info->bsps = (int)count;
	info->equipment = (int)equipment_count;

	/* starting locations by game type */
	for (game_type = 1; game_type <= 5; game_type++)
	{
		info->spawns[game_type] = 0;
		for (index = 0; index < start_count; index++)
		{
			if (match_game_type(game_type, starts + index * START_SIZE + 0x14))
				info->spawns[game_type]++;
		}
	}

	/* the game types' objects */
	scratch = malloc((size_t)(flag_count ? flag_count : 1) * sizeof(short));
	if (!scratch)
		return AE_MAPINFO_UNREADABLE;
	flag_teams(flags, flag_count, FLAG_CTF, scratch, &ctf_both);
	ball_spawns = flag_teams(flags, flag_count, FLAG_ODDBALL, scratch, &both);
	hills = flag_teams(flags, flag_count, FLAG_HILL, scratch, &both);
	race_checkpoints = flag_teams(flags, flag_count, FLAG_RACE, scratch, &both);
	free(scratch);
	info->hills = hills;
	info->race_checkpoints = race_checkpoints;
	info->modes = 0;
	if (ctf_both && info->spawns[1] >= 4)
		info->modes |= AE_MODE_CTF;
	if (info->spawns[2] >= 4)
		info->modes |= AE_MODE_SLAYER;
	if (ball_spawns > 0 && info->spawns[3] >= 4)
		info->modes |= AE_MODE_ODDBALL;
	if (hills > 0 && info->spawns[4] >= 4)
		info->modes |= AE_MODE_KING;
	if (race_checkpoints >= 2 && info->spawns[5] >= 4)
		info->modes |= AE_MODE_RACE;

	/* the play area: the spread of starting locations, equipment and flags, across (x, y) */
	for (index = 0; index < start_count + equipment_count + flag_count; index++)
	{
		unsigned char const *position = index < start_count ? starts + index * START_SIZE :
			index < start_count + equipment_count ? equipment + (index - start_count) * EQUIPMENT_SIZE + 0x40 :
			flags + (index - start_count - equipment_count) * FLAG_SIZE;
		double x = f32(position), y = f32(position + 4);

		/* (a corrupt position counts for nothing) */
		if (!(x == x) || !(y == y) || fabs(x) > 1.0e6 || fabs(y) > 1.0e6)
			continue;
		if (!points || x < minimum_x) minimum_x = x;
		if (!points || x > maximum_x) maximum_x = x;
		if (!points || y < minimum_y) minimum_y = y;
		if (!points || y > maximum_y) maximum_y = y;
		points++;
	}
	info->play_extent = points ? (float)sqrt((maximum_x - minimum_x) * (maximum_x - minimum_x) +
		(maximum_y - minimum_y) * (maximum_y - minimum_y)) : 0.0f;
	return AE_MAPINFO_OK;
}

/* an Xbox map's tag data: the zlib stream after the header inflated in chunks, kept from offset to its end */
static int inflate_tag_data(ae_mapinfo_reader read, void *context, unsigned long file_size, unsigned long offset,
	struct tag_data *tags)
{
	z_stream stream;
	unsigned char *input, *output;
	unsigned long read_at = HEADER_SIZE, produced_at = HEADER_SIZE, end = offset + tags->size;
	int status = Z_OK, result = AE_MAPINFO_UNREADABLE;

	input = malloc(CHUNK);
	output = malloc(CHUNK);
	if (!input || !output)
	{
		free(input);
		free(output);
		return AE_MAPINFO_UNREADABLE;
	}
	memset(&stream, 0, sizeof(stream));
	if (inflateInit(&stream) != Z_OK)
	{
		free(input);
		free(output);
		return AE_MAPINFO_UNREADABLE;
	}
	while (produced_at < end && status != Z_STREAM_END)
	{
		unsigned long produced;

		if (stream.avail_in == 0)
		{
			unsigned long size = file_size - read_at < CHUNK ? file_size - read_at : CHUNK;

			if (read_at >= file_size || !read(context, read_at, input, size))
				break;
			read_at += size;
			stream.next_in = input;
			stream.avail_in = (unsigned int)size;
		}
		stream.next_out = output;
		stream.avail_out = CHUNK;
		status = inflate(&stream, Z_NO_FLUSH);
		if (status != Z_OK && status != Z_STREAM_END)
			break;
		produced = CHUNK - stream.avail_out;
		/* (the part of this chunk inside the tag data) */
		if (produced_at + produced > offset && produced_at < end)
		{
			unsigned long from = produced_at < offset ? offset - produced_at : 0;
			unsigned long to = produced_at + produced < end ? produced : end - produced_at;

			memcpy(tags->bytes + (produced_at + from - offset), output + from, to - from);
		}
		produced_at += produced;
		if (produced_at > MAXIMUM_DECOMPRESSED)
			break;
	}
	if (produced_at >= end)
		result = AE_MAPINFO_OK;
	inflateEnd(&stream);
	free(input);
	free(output);
	return result;
}

int ae_mapinfo_parse(ae_mapinfo_reader read, void *context, unsigned long file_size, struct ae_mapinfo *info)
{
	unsigned char header[HEADER_SIZE];
	unsigned long version, decompressed, offset, size, tag_header_size;
	struct tag_data tags;
	int result;

	memset(info, 0, sizeof(*info));
	if (!read || file_size < HEADER_SIZE || !read(context, 0, header, HEADER_SIZE) || memcmp(header, "daeh", 4))
		return AE_MAPINFO_UNREADABLE;
	version = u32(header + 4);
	decompressed = u32(header + 8);
	offset = u32(header + 0x10);
	size = u32(header + 0x14);
	copy_text(info->name, sizeof(info->name), header + 0x20, 0x20);
	copy_text(info->build, sizeof(info->build), header + 0x40, 0x20);
	info->scenario_type = s16(header + 0x60);
	info->open_sauce = !memcmp(header + 0x70, "oley", 4);
	switch (version)
	{
	case XBOX_VERSION:
		info->family = strcmp(info->build, XBOX_STOCK_BUILD) ? AE_MAP_XBOX_COMMUNITY : AE_MAP_XBOX;
		tags.base = XBOX_TAG_BASE;
		tag_header_size = XBOX_TAG_HEADER_SIZE;
		break;
	case CE_VERSION:
		info->family = AE_MAP_CUSTOM_EDITION;
		tags.base = CE_TAG_BASE;
		tag_header_size = CE_TAG_HEADER_SIZE;
		break;
	case HALOMD_VERSION:
		info->family = AE_MAP_HALOMD;
		tags.base = CE_TAG_BASE;
		tag_header_size = CE_TAG_HEADER_SIZE;
		break;
	case MCC_VERSION:
		/* (named, not read: the port can't play it) */
		info->family = AE_MAP_MCC;
		return AE_MAPINFO_UNSUPPORTED;
	default:
		info->family = AE_MAP_UNKNOWN;
		return AE_MAPINFO_UNREADABLE;
	}
	if (size < tag_header_size || size > MAXIMUM_TAG_DATA || offset < HEADER_SIZE)
		return AE_MAPINFO_UNREADABLE;
	/* (an Xbox map's tag data ends its inflated stream: the header's decompressed length is at least its end
	(rounded up to a whole 2048 bytes on Halo 1: NHE's maps: cache_files_decompress_windows.c) and within the engine's
	limit, checked before anything is allocated; a Custom Edition map's is in the file) */
	if (version == XBOX_VERSION ? decompressed > MAXIMUM_DECOMPRESSED || offset > decompressed ||
		size > decompressed - offset : offset > file_size || size > file_size - offset)
	{
		return AE_MAPINFO_UNREADABLE;
	}
	tags.size = size;
	tags.bytes = malloc(size);
	if (!tags.bytes)
		return AE_MAPINFO_UNREADABLE;
	if (version == XBOX_VERSION)
		result = inflate_tag_data(read, context, file_size, offset, &tags);
	else
		result = read(context, offset, tags.bytes, size) ? AE_MAPINFO_OK : AE_MAPINFO_UNREADABLE;
	if (result == AE_MAPINFO_OK)
		result = read_scenario(&tags, tag_header_size, info);
	free(tags.bytes);
	return result;
}

/* ---------- stdio */

static int read_file(void *context, unsigned long offset, void *buffer, unsigned long size)
{
	FILE *file = context;

	return fseek(file, (long)offset, SEEK_SET) == 0 && fread(buffer, 1, size, file) == size;
}

int ae_mapinfo_read(const char *path, struct ae_mapinfo *info)
{
	FILE *file = fopen(path, "rb");
	long size;
	int result;

	if (!file)
	{
		memset(info, 0, sizeof(*info));
		return AE_MAPINFO_UNREADABLE;
	}
	if (fseek(file, 0, SEEK_END) != 0 || (size = ftell(file)) < 0)
	{
		fclose(file);
		memset(info, 0, sizeof(*info));
		return AE_MAPINFO_UNREADABLE;
	}
	result = ae_mapinfo_parse(read_file, file, (unsigned long)size, info);
	fclose(file);
	return result;
}
