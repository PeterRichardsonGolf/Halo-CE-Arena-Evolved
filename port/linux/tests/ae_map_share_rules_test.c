#define _DEFAULT_SOURCE
#define _XOPEN_SOURCE 700
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "ae_map_share_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static unsigned long ip(unsigned a, unsigned b, unsigned c, unsigned d)
{
	return (unsigned long)a << 24 | (unsigned long)b << 16 | (unsigned long)c << 8 | (unsigned long)d;
}

static int exists(char const *path)
{
	struct stat info;

	return stat(path, &info) == 0;
}

static void touch(char const *path)
{
	FILE *file = fopen(path, "wb");

	if (file)
	{
		fputs("map", file);
		fclose(file);
	}
}

static int delete_real(char const *path, void *context)
{
	int *calls = context;

	(*calls)++;
	return remove(path) == 0;
}

static int delete_logged(char const *path, void *context)
{
	(void)path;
	(*(int *)context)++;
	return 1;
}

int main(void)
{
	/* private ranges, their edges */
	CHECK(ae_map_share_address_local(ip(10, 0, 0, 1)));
	CHECK(ae_map_share_address_local(ip(10, 255, 255, 254)));
	CHECK(ae_map_share_address_local(ip(172, 16, 0, 1)));
	CHECK(ae_map_share_address_local(ip(172, 31, 255, 254)));
	CHECK(!ae_map_share_address_local(ip(172, 15, 255, 255)));
	CHECK(!ae_map_share_address_local(ip(172, 32, 0, 0)));
	CHECK(ae_map_share_address_local(ip(192, 168, 0, 1)));
	CHECK(ae_map_share_address_local(ip(192, 168, 255, 255)));
	CHECK(!ae_map_share_address_local(ip(192, 169, 0, 1)));
	CHECK(!ae_map_share_address_local(ip(192, 167, 255, 255)));
	/* loopback, link-local */
	CHECK(ae_map_share_address_local(ip(127, 0, 0, 1)));
	CHECK(ae_map_share_address_local(ip(127, 0, 0, 200)));
	CHECK(ae_map_share_address_local(ip(169, 254, 10, 20)));
	CHECK(!ae_map_share_address_local(ip(169, 253, 10, 20)));
	/* public: never a LAN source */
	CHECK(!ae_map_share_address_local(ip(8, 8, 8, 8)));
	CHECK(!ae_map_share_address_local(ip(1, 1, 1, 1)));
	CHECK(!ae_map_share_address_local(ip(11, 0, 0, 1)));
	CHECK(!ae_map_share_address_local(ip(9, 255, 255, 255)));
	CHECK(!ae_map_share_address_local(ip(203, 0, 113, 7)));
	/* internet play's tunnel range (100.64/10) is reached by key, never as a LAN address */
	CHECK(!ae_map_share_address_local(ip(100, 64, 0, 1)));
	/* malformed: none, broadcast, multicast, more than 32 bits */
	CHECK(!ae_map_share_address_local(0));
	CHECK(!ae_map_share_address_local(0xFFFFFFFFUL));
	CHECK(!ae_map_share_address_local(ip(224, 0, 0, 1)));
	CHECK(!ae_map_share_address_local(ip(0, 0, 0, 10)));
	if (sizeof(unsigned long) > 4)
	{
		unsigned long high = ((unsigned long)1 << 16) << 16;

		CHECK(!ae_map_share_address_local(high | ip(192, 168, 0, 1)));
		CHECK(!ae_map_share_address_local(high | ip(10, 0, 0, 1)));
	}

	/* the paths a failed download may remove: a plain <name>.map in the folder */
	CHECK(ae_map_share_path_in_folder("d:\\maps_ce\\hugeass_1a2b3c4d.map", "d:\\maps_ce\\"));
	CHECK(ae_map_share_path_in_folder("d:\\maps_ce\\bitmaps.map", "d:\\maps_ce\\"));
	CHECK(ae_map_share_path_in_folder("d:\\maps_ce\\X.MAP", "d:\\maps_ce\\"));
	CHECK(!ae_map_share_path_in_folder("d:\\maps_ce\\.map", "d:\\maps_ce\\"));
	CHECK(!ae_map_share_path_in_folder("d:\\maps_ce\\", "d:\\maps_ce\\"));
	CHECK(!ae_map_share_path_in_folder("d:\\maps_ce\\hugeass.txt", "d:\\maps_ce\\"));
	CHECK(!ae_map_share_path_in_folder("d:\\maps_ce\\hugeass.map.part", "d:\\maps_ce\\"));
	/* traversal and other folders */
	CHECK(!ae_map_share_path_in_folder("d:\\maps_ce\\..\\maps\\bloodgulch.map", "d:\\maps_ce\\"));
	CHECK(!ae_map_share_path_in_folder("d:\\maps_ce\\..map", "d:\\maps_ce\\"));
	CHECK(!ae_map_share_path_in_folder("d:\\maps_ce\\a..b.map", "d:\\maps_ce\\"));
	CHECK(!ae_map_share_path_in_folder("d:\\maps_ce\\sub\\x.map", "d:\\maps_ce\\"));
	CHECK(!ae_map_share_path_in_folder("d:\\maps_ce\\sub/x.map", "d:\\maps_ce\\"));
	CHECK(!ae_map_share_path_in_folder("d:\\maps_ce\\c:x.map", "d:\\maps_ce\\"));
	CHECK(!ae_map_share_path_in_folder("d:\\maps\\bloodgulch.map", "d:\\maps_ce\\"));
	CHECK(!ae_map_share_path_in_folder("d:\\maps_ce_other\\x.map", "d:\\maps_ce\\"));
	CHECK(!ae_map_share_path_in_folder("e:\\maps_ce\\x.map", "d:\\maps_ce\\"));
	CHECK(!ae_map_share_path_in_folder("d:\\maps_ce\\x\n.map", "d:\\maps_ce\\"));
	CHECK(!ae_map_share_path_in_folder(NULL, "d:\\maps_ce\\"));
	CHECK(!ae_map_share_path_in_folder("d:\\maps_ce\\x.map", NULL));
	CHECK(!ae_map_share_path_in_folder("d:\\maps_ce\\x.map", ""));

	/* discarding: only the listed paths in the folder reach the remover */
	{
		char const *paths[] = { "d:\\maps_ce\\bad_00000000.map", "d:\\maps_ce\\..\\maps\\a10.map",
			"d:\\maps\\bloodgulch.map", "d:\\maps_ce\\sounds.map" };
		int calls = 0;

		CHECK(ae_map_share_discard(paths, 4, "d:\\maps_ce\\", delete_logged, &calls) == 2);
		CHECK(calls == 2);
		calls = 0;
		CHECK(ae_map_share_discard(paths, 0, "d:\\maps_ce\\", delete_logged, &calls) == 0 && calls == 0);
		CHECK(ae_map_share_discard(NULL, 4, "d:\\maps_ce\\", delete_logged, &calls) == 0 && calls == 0);
	}

	/* on disk: the failed download's file goes; a good map beside it, and a file outside the folder named
	through "..", stay */
	{
		char const *scratch = getenv("AE_TEST_SCRATCH");
		char root[160];
		char folder[192], outside[192], bad[256], good[256], traversal[256];
		char const *paths[2];
		int calls = 0;

		snprintf(root, sizeof(root), "%s/ae_map_share_rules_XXXXXX", scratch && scratch[0] ? scratch : ".");
		if (!mkdtemp(root))
		{
			CHECK(!"mkdtemp");
			return 1;
		}
		snprintf(folder, sizeof(folder), "%s/maps_ce/", root);
		CHECK(mkdir(folder, 0700) == 0);
		snprintf(outside, sizeof(outside), "%s/keep.map", root);
		snprintf(bad, sizeof(bad), "%sbad_00000000.map", folder);
		snprintf(good, sizeof(good), "%sgood_11111111.map", folder);
		snprintf(traversal, sizeof(traversal), "%s../keep.map", folder);
		touch(outside);
		touch(bad);
		touch(good);
		paths[0] = bad;
		paths[1] = traversal;
		CHECK(ae_map_share_discard(paths, 2, folder, delete_real, &calls) == 1);
		CHECK(calls == 1);
		CHECK(!exists(bad));
		CHECK(exists(good));
		CHECK(exists(outside));
		remove(good);
		remove(outside);
		rmdir(folder);
		rmdir(root);
	}

	if (failures)
		return 1;
	printf("ae_map_share_rules_test: all passed\n");
	return 0;
}
