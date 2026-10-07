/* ae_mapinfo (port/linux/game/ae_mapinfo.c): maps the test writes itself (a Custom Edition map, its protected,
HaloMD, OpenSauce and MCC kinds, an Xbox map zlib-compressed), corrupt and cut ones (every length, pointers out of
range, counts past the data, random bytes), and the stock Xbox maps against the tools' readings
(port/linux/tests/data/ae_mapinfo_expected.txt) when $AE_MAPINFO_MAPS or $HALO_DATA_ROOT/maps has them. Exits 77
(a skip) when it found no real map, after checking everything else. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <zlib.h>
#include "ae_mapinfo.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

enum { HEADER = 0x800, TAGS = 0x4000, CE_SIZE = HEADER + TAGS };
#define CE_BASE 0x40440000UL
#define XBOX_BASE 0x803A6000UL

static void put32(unsigned char *p, unsigned long v) { p[0] = (unsigned char)v; p[1] = (unsigned char)(v >> 8); p[2] = (unsigned char)(v >> 16); p[3] = (unsigned char)(v >> 24); }
static void put16(unsigned char *p, int v) { p[0] = (unsigned char)v; p[1] = (unsigned char)(v >> 8); }
static void putf(unsigned char *p, float f) { unsigned int u; memcpy(&u, &f, 4); put32(p, u); }

/* a memory map for ae_mapinfo_parse */
struct memory { unsigned char const *bytes; unsigned long size; };
static int read_memory(void *context, unsigned long offset, void *buffer, unsigned long size)
{
	struct memory const *m = context;

	if (offset > m->size || size > m->size - offset)
		return 0;
	memcpy(buffer, m->bytes + offset, size);
	return 1;
}
static int parse(unsigned char const *bytes, unsigned long size, struct ae_mapinfo *info)
{
	struct memory m;

	m.bytes = bytes;
	m.size = size;
	return ae_mapinfo_parse(read_memory, &m, size, info);
}

/* the tag data of a small multiplayer scenario at base: 6 starting locations (5 for every game type, 1 slayer's),
flags (a hill, a ball spawn, one race checkpoint, a CTF flag of team 0 only, and, with full, team 1's flag and a
second checkpoint), 2 pieces of equipment, a vehicle, a BSP; group "scnr" or another (protected) */
static void scenario(unsigned char *t, unsigned long base, unsigned long tag_header_size, char const *group, int full)
{
	unsigned char *s = t + 0x200;
	int i, flag = 0;

	memset(t, 0, TAGS);
	put32(t + 0, base + tag_header_size);         /* tag instances */
	put32(t + 4, 0xE1740000UL);                   /* the scenario's tag: index 0 (with a salt) */
	put32(t + 0xC, 1);                            /* one tag */
	memcpy(t + tag_header_size - 4, "sgat", 4);
	t[tag_header_size + 0] = (unsigned char)group[3];
	t[tag_header_size + 1] = (unsigned char)group[2];
	t[tag_header_size + 2] = (unsigned char)group[1];
	t[tag_header_size + 3] = (unsigned char)group[0];
	put32(t + tag_header_size + 0x10, base + 0x100);
	put32(t + tag_header_size + 0x14, base + 0x200);
	strcpy((char *)t + 0x100, "levels\\test\\synth\\synth");
	put32(s + 0x240, 1); put32(s + 0x244, base + 0x3800);              /* vehicles */
	put32(s + 0x354, 6); put32(s + 0x358, base + 0x1000);              /* starting locations */
	put32(s + 0x378, full ? 6 : 4); put32(s + 0x37C, base + 0x2000);   /* netgame flags */
	put32(s + 0x384, 2); put32(s + 0x388, base + 0x3000);              /* netgame equipment */
	put32(s + 0x5A4, 1); put32(s + 0x5A8, base + 0x3900);              /* BSPs */
	for (i = 0; i < 6; i++)
	{
		unsigned char *start = t + 0x1000 + i * 0x34;

		putf(start, (float)(i * 10)); putf(start + 4, 0.0f);
		put16(start + 0x14, i < 5 ? 12 : 2);
	}
#define FLAG(type, team, x, y) do { unsigned char *f = t + 0x2000 + flag++ * 0x94; putf(f, x); putf(f + 4, y); \
	put16(f + 0x10, type); put16(f + 0x12, team); } while (0)
	FLAG(8, 0, 0.0f, 20.0f);   /* hill */
	FLAG(2, 0, 0.0f, -10.0f);  /* oddball */
	FLAG(3, 0, 5.0f, 5.0f);    /* race checkpoint 0 */
	FLAG(0, 0, 1.0f, 1.0f);    /* ctf flag, team 0 */
	if (full)
	{
		FLAG(0, 1, 2.0f, 2.0f);
		FLAG(3, 1, 3.0f, 3.0f);
	}
	for (i = 0; i < 2; i++)
	{
		unsigned char *e = t + 0x3000 + i * 0x90;

		put16(e + 4, 12);
		putf(e + 0x40, i ? 60.0f : 0.0f); putf(e + 0x44, 0.0f);
	}
}

static void header(unsigned char *h, unsigned long version, unsigned long length, unsigned long tags_size,
	char const *build)
{
	memset(h, 0, HEADER);
	memcpy(h, "daeh", 4);
	put32(h + 4, version);
	put32(h + 8, length);
	put32(h + 0x10, HEADER);
	put32(h + 0x14, tags_size);
	strcpy((char *)h + 0x20, "synth");
	strcpy((char *)h + 0x40, build);
	put16(h + 0x60, 1);
	memcpy(h + 0x7FC, "toof", 4);
}

static void ce_map(unsigned char *map, unsigned long version, char const *group, int full)
{
	header(map, version, CE_SIZE, TAGS, version == 7 ? "01.00.00.0564" : "01.00.00.0609");
	scenario(map + HEADER, CE_BASE, 0x28, group, full);
}

static int near(float a, float b) { return fabsf(a - b) <= 0.15f; }

static void check_synthetic(void)
{
	static unsigned char map[CE_SIZE], bad[CE_SIZE];
	struct ae_mapinfo info;
	unsigned long length;
	int i;

	/* a Custom Edition map: no CTF (team 0's flag only), no race (one checkpoint) */
	ce_map(map, 609, "scnr", 0);
	CHECK(parse(map, CE_SIZE, &info) == AE_MAPINFO_OK);
	CHECK(info.family == AE_MAP_CUSTOM_EDITION && !strcmp(info.name, "synth") && !strcmp(info.build, "01.00.00.0609"));
	CHECK(info.scenario_type == 1 && !info.protected_map && !info.open_sauce);
	CHECK(info.modes == (AE_MODE_SLAYER | AE_MODE_ODDBALL | AE_MODE_KING));
	CHECK(info.spawns[1] == 5 && info.spawns[2] == 6 && info.spawns[3] == 5 && info.spawns[4] == 5 && info.spawns[5] == 5);
	CHECK(info.hills == 1 && info.race_checkpoints == 1 && info.equipment == 2 && info.vehicles == 1 && info.bsps == 1);
	CHECK(near(info.play_extent, 67.08f));                 /* x 0..60, y -10..20 */
	/* both teams' flags and two checkpoints: every mode */
	ce_map(map, 609, "scnr", 1);
	CHECK(parse(map, CE_SIZE, &info) == AE_MAPINFO_OK && info.modes == 31 && info.race_checkpoints == 2);
	/* a protected map (its scenario's group relabelled) reads the same */
	ce_map(map, 609, "prot", 1);
	CHECK(parse(map, CE_SIZE, &info) == AE_MAPINFO_OK && info.protected_map && info.modes == 31);
	ce_map(map, 609, "devm", 0);
	CHECK(parse(map, CE_SIZE, &info) == AE_MAPINFO_OK && info.protected_map && info.modes == 14);
	/* HaloMD's (Halo PC retail's, 7) */
	ce_map(map, 7, "scnr", 0);
	CHECK(parse(map, CE_SIZE, &info) == AE_MAPINFO_OK && info.family == AE_MAP_HALOMD && info.modes == 14);
	/* OpenSauce's: listed, not refused */
	ce_map(map, 609, "scnr", 0);
	memcpy(map + 0x70, "oley", 4);
	CHECK(parse(map, CE_SIZE, &info) == AE_MAPINFO_OK && info.open_sauce && info.modes == 14);
	/* MCC's: named, nothing more */
	ce_map(map, 13, "scnr", 0);
	CHECK(parse(map, CE_SIZE, &info) == AE_MAPINFO_UNSUPPORTED && info.family == AE_MAP_MCC && info.modes == 0 &&
		!strcmp(info.name, "synth"));
	/* an unknown version, and not a cache file */
	ce_map(map, 6, "scnr", 0);
	CHECK(parse(map, CE_SIZE, &info) == AE_MAPINFO_UNREADABLE && info.family == AE_MAP_UNKNOWN);
	ce_map(map, 609, "scnr", 0);
	memcpy(map, "head", 4);
	CHECK(parse(map, CE_SIZE, &info) == AE_MAPINFO_UNREADABLE);

	/* cut at every length: unreadable (never a read past what is there), until it is whole */
	ce_map(map, 609, "scnr", 1);
	for (length = 0; length < CE_SIZE; length++)
	{
		if (parse(map, length, &info) != AE_MAPINFO_UNREADABLE)
		{
			CHECK(!"a cut map read");
			break;
		}
	}
	/* pointers and counts out of range */
#define CORRUPT(at, value) do { memcpy(bad, map, CE_SIZE); put32(bad + (at), (value)); \
	CHECK(parse(bad, CE_SIZE, &info) == AE_MAPINFO_UNREADABLE); } while (0)
	CORRUPT(0x10, CE_SIZE);                                 /* tag data past the end */
	CORRUPT(0x14, 0xFFFFFFF0UL);                            /* tag data size past the end */
	CORRUPT(0x14, 0x10);                                    /* tag data smaller than its header */
	CORRUPT(HEADER + 0, CE_BASE + TAGS);                    /* tag instances past the tag data */
	CORRUPT(HEADER + 0xC, 0);                               /* no tags */
	CORRUPT(HEADER + 0xC, 0x7FFFFFFF);                      /* too many tags */
	CORRUPT(HEADER + 4, 5);                                 /* scenario index past the tags */
	CORRUPT(HEADER + 0x28 + 0x14, CE_BASE + TAGS - 0x10);   /* scenario cut by the end */
	CORRUPT(HEADER + 0x28 + 0x14, 0x10);                    /* scenario below the tag data */
	CORRUPT(HEADER + 0x200 + 0x354, 0x7FFFFFFF);            /* starting locations: a huge count */
	CORRUPT(HEADER + 0x200 + 0x354, 0xFFFFFFFFUL);          /* ... a negative one */
	CORRUPT(HEADER + 0x200 + 0x358, CE_BASE + TAGS - 0x20); /* ... past the tag data */
	CORRUPT(HEADER + 0x200 + 0x37C, 0xFFFFFFFFUL);          /* flags: an address past everything */
	CORRUPT(HEADER + 0x200 + 0x384, 200);                   /* equipment: more than there is */
	CORRUPT(HEADER + 0x200 + 0x240, 0x80000000UL);          /* vehicles: a negative count */
	CORRUPT(HEADER + 0x200 + 0x5A4, 0x10000000UL);          /* BSPs: a huge count */
	/* random bytes anywhere: whatever the result, no read out of bounds (the sanitizers) */
	srand(1234);
	for (i = 0; i < 4000; i++)
	{
		int n, k;

		memcpy(bad, map, CE_SIZE);
		n = 1 + rand() % 8;
		for (k = 0; k < n; k++)
			bad[(i % 3 ? HEADER : 0) + rand() % (i % 3 ? TAGS : CE_SIZE)] = (unsigned char)rand();
		parse(bad, CE_SIZE, &info);
	}
}

/* an Xbox map: the same scenario at the Xbox base, zlib-compressed after the header */
static void check_xbox(void)
{
	static unsigned char tags[TAGS], map[HEADER + TAGS + 4096];
	struct ae_mapinfo info;
	uLongf packed = TAGS + 4096 - 16;
	unsigned long length;

	scenario(tags, XBOX_BASE, 0x24, "scnr", 1);
	CHECK(compress2(map + HEADER, &packed, tags, TAGS, 6) == Z_OK);
	header(map, 5, HEADER + TAGS, TAGS, "01.10.12.2276");
	CHECK(parse(map, HEADER + packed, &info) == AE_MAPINFO_OK);
	CHECK(info.family == AE_MAP_XBOX && info.modes == 31 && info.spawns[2] == 6 && info.equipment == 2);
	CHECK(near(info.play_extent, 67.08f));
	header(map, 5, HEADER + TAGS, TAGS, "01.10.12.2300");       /* a community build (Halo 1: NHE's) */
	CHECK(parse(map, HEADER + packed, &info) == AE_MAPINFO_OK && info.family == AE_MAP_XBOX_COMMUNITY);
	/* the stream cut short, or corrupt */
	/* (up to its last bytes: the stream's checksum, after the data, isn't needed to read it) */
	for (length = 0; length + 16 < HEADER + packed; length += 7)
	{
		if (parse(map, length, &info) != AE_MAPINFO_UNREADABLE)
		{
			CHECK(!"a cut Xbox map read");
			break;
		}
	}
	memset(map + HEADER + 2, 0xFF, 64);
	CHECK(parse(map, HEADER + packed, &info) == AE_MAPINFO_UNREADABLE);
}

/* ae_mapinfo_read: a file, an empty file, a missing one */
static void check_files(void)
{
	static unsigned char map[CE_SIZE];
	char const *scratch = getenv("AE_TEST_SCRATCH");
	char path[512];
	struct ae_mapinfo info;
	FILE *file;

	snprintf(path, sizeof(path), "%s/synth.map", scratch ? scratch : ".");
	ce_map(map, 609, "scnr", 1);
	file = fopen(path, "wb");
	CHECK(file != NULL);
	if (!file)
		return;
	fwrite(map, 1, CE_SIZE, file);
	fclose(file);
	CHECK(ae_mapinfo_read(path, &info) == 0 && info.modes == 31);
	file = fopen(path, "wb");
	fclose(file);
	CHECK(ae_mapinfo_read(path, &info) != 0);
	remove(path);
	CHECK(ae_mapinfo_read(path, &info) != 0);
}

/* the stock maps, against the tools' readings; the number of maps read */
static int check_real_maps(void)
{
	char const *maps = getenv("AE_MAPINFO_MAPS");
	char const *root = getenv("HALO_DATA_ROOT");
	char directory[512], line[512], path[1024];
	FILE *fixture = fopen("port/linux/tests/data/ae_mapinfo_expected.txt", "r");
	int found = 0;
	clock_t started = clock();

	if (maps)
		snprintf(directory, sizeof(directory), "%s", maps);
	else if (root)
		snprintf(directory, sizeof(directory), "%s/maps", root);
	else
		directory[0] = 0;
	CHECK(fixture != NULL);
	if (!fixture)
		return 0;
	while (fgets(line, sizeof(line), fixture))
	{
		char file[128], modes[8];
		int family, spawns[5], hills, race, equipment, vehicles, bsps, protected_map, mode_bits = 0, i;
		float extent;
		struct ae_mapinfo info;
		FILE *probe;

		if (line[0] == '#' || sscanf(line, "%127s %d %7s %d %d %d %d %d %d %d %d %d %d %f %d", file, &family, modes,
			&spawns[0], &spawns[1], &spawns[2], &spawns[3], &spawns[4], &hills, &race, &equipment, &vehicles, &bsps,
			&extent, &protected_map) != 15)
		{
			continue;
		}
		if (!directory[0])
			continue;
		snprintf(path, sizeof(path), "%s/%s", directory, file);
		probe = fopen(path, "rb");
		if (!probe)
			continue;
		fclose(probe);
		for (i = 0; i < 5; i++)
			mode_bits |= modes[i] != '-' ? 1 << i : 0;
		found++;
		if (ae_mapinfo_read(path, &info) != 0)
		{
			printf("FAIL %s: unreadable\n", file);
			failures++;
			continue;
		}
		if (info.family != family || info.modes != mode_bits || info.spawns[1] != spawns[0] ||
			info.spawns[2] != spawns[1] || info.spawns[3] != spawns[2] || info.spawns[4] != spawns[3] ||
			info.spawns[5] != spawns[4] || info.hills != hills || info.race_checkpoints != race ||
			info.equipment != equipment || info.vehicles != vehicles || info.bsps != bsps ||
			fabsf(info.play_extent - extent) > 0.1f || info.protected_map != protected_map || info.scenario_type != 1)
		{
			printf("FAIL %s: family %d modes %d spawns %d %d %d %d %d hills %d race %d equipment %d vehicles %d bsps %d "
				"extent %.2f protected %d type %d\n", file, info.family, info.modes, info.spawns[1], info.spawns[2],
				info.spawns[3], info.spawns[4], info.spawns[5], info.hills, info.race_checkpoints, info.equipment,
				info.vehicles, info.bsps, info.play_extent, info.protected_map, info.scenario_type);
			failures++;
		}
		/* its first 4096 bytes: unreadable */
		{
			static unsigned char start[4096];
			FILE *whole = fopen(path, "rb");
			size_t got = whole ? fread(start, 1, sizeof(start), whole) : 0;

			if (whole)
				fclose(whole);
			CHECK(got == sizeof(start) && parse(start, sizeof(start), &info) == AE_MAPINFO_UNREADABLE);
		}
	}
	fclose(fixture);
	if (found)
		printf("ae_mapinfo: %d stock maps in %.2f s\n", found, (double)(clock() - started) / CLOCKS_PER_SEC);
	return found;
}

int main(void)
{
	int real;

	check_synthetic();
	check_xbox();
	check_files();
	real = check_real_maps();
	if (failures)
		return 1;
	if (!real)
	{
		printf("ae_mapinfo: ok (no stock maps found: set AE_MAPINFO_MAPS or HALO_DATA_ROOT to check them)\n");
		return 77;
	}
	printf("ae_mapinfo: ok\n");
	return 0;
}
