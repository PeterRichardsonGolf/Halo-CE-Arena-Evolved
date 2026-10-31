/*
AE_MAP_SHARE_KEPT.C (test)

Arena Evolved: map sharing's catalogue of hashed files (port/linux/game/map_share_files.c, map_share_file_hash)
replaces its files in turn once it is full. A file in use (ae_map_share_file_kept: the game's own, or one being sent
to a machine) keeps its place: the turn passes over it. test_ae_map_share_kept.py takes the real function
(under_test.inc) and the catalogue's size (config.inc).
*/

#include "harness.h"
#include "config.inc"

#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define MAP_SHARE_PATH_SIZE 256
#define MAP_SHARE_HASH_BYTES 32
#define MAXIMUM_FILE_BYTES 0x7FFFFFFF
#define csmemset(destination, value, size) memset(destination, value, size)
#define csstrncpy(destination, source, size) strncpy(destination, source, size)
#define csstrcasecmp(first, second) strcasecmp(first, second)

enum
{
	_hash_job_none,
	_hash_job_file,
};

struct map_share_file
{
	boolean used;
	short kind;
	char path[MAP_SHARE_PATH_SIZE];
	boolean ready;
	boolean failed;
	unsigned long size;
	unsigned long modified;
	long piece_count;
	byte *piece_hashes;
	byte hash[MAP_SHARE_HASH_BYTES];
};

static struct map_share_files_globals
{
	struct map_share_file files[MAXIMUM_SHARE_FILES];
	short next_file;
	short job_type;
	long job_owner;
} map_share_files_globals;

/* every file is there, 1000 bytes long */
static boolean file_stamp(char const *path, unsigned long *size, unsigned long *modified)
{
	(void)path;
	*size = 1000;
	*modified = 1;
	return TRUE;
}

/* the paths that keep their places */
static char const *kept[4];

static boolean ae_map_share_file_kept(char const *path)
{
	int index;

	for (index = 0; index < 4; index++)
	{
		if (kept[index] && !strcasecmp(kept[index], path))
			return TRUE;
	}
	return FALSE;
}

#include "under_test.inc"

static short place_of(char const *path)
{
	short index;

	for (index = 0; index < MAXIMUM_SHARE_FILES; index++)
	{
		if (map_share_files_globals.files[index].used && !strcmp(map_share_files_globals.files[index].path, path))
			return index;
	}
	return NONE;
}

static void fill(int first, int count)
{
	char path[64];
	int index;

	for (index = first; index < first + count; index++)
	{
		snprintf(path, sizeof(path), "d:\\maps\\map%02d.map", index);
		map_share_file_hash(path, 0);
	}
}

int main(int argc, char **argv)
{
	char const *case_name = argc > 1 ? argv[1] : "";

	/* a guest cycles through three times the catalogue's size of other files: the kept files keep their places */
	CASE("kept-files-keep-their-places")
	{
		short played;
		short flight;

		memset(&map_share_files_globals, 0, sizeof(map_share_files_globals));
		played = map_share_file_hash("d:\\maps\\played.map", 0);
		fill(0, 5);
		flight = map_share_file_hash("d:\\maps\\flight.map", 0);
		kept[0] = "d:\\maps\\played.map";
		kept[1] = "D:\\MAPS\\FLIGHT.MAP";
		fill(5, MAXIMUM_SHARE_FILES * 3);
		CHECK(place_of("d:\\maps\\played.map") == played, "the game's own file lost its place to a file asked for later");
		CHECK(place_of("d:\\maps\\flight.map") == flight, "a file being sent lost its place");
		CHECK(place_of("d:\\maps\\map00.map") == NONE, "a file not in use kept its place for ever: no file is replaced");
		return 0;
	}
	/* one no longer kept is replaced when its turn comes */
	CASE("let-go-is-replaced")
	{
		memset(&map_share_files_globals, 0, sizeof(map_share_files_globals));
		map_share_file_hash("d:\\maps\\played.map", 0);
		kept[0] = "d:\\maps\\played.map";
		fill(0, MAXIMUM_SHARE_FILES * 2);
		CHECK(place_of("d:\\maps\\played.map") != NONE, "the kept file was replaced");
		kept[0] = NULL;
		fill(100, MAXIMUM_SHARE_FILES);
		CHECK(place_of("d:\\maps\\played.map") == NONE, "a file let go of was never replaced");
		return 0;
	}
	/* the file being hashed is passed over too, also when the turn comes to it from a kept file */
	CASE("hashing-file-passed-over")
	{
		short owner;

		memset(&map_share_files_globals, 0, sizeof(map_share_files_globals));
		/* (the place before it is a kept file's: the turn that passes over that one comes to this one) */
		kept[0] = "d:\\maps\\map02.map";
		kept[1] = NULL;
		fill(0, MAXIMUM_SHARE_FILES);
		owner = place_of("d:\\maps\\map03.map");
		map_share_files_globals.job_type = _hash_job_file;
		map_share_files_globals.job_owner = owner;
		fill(100, MAXIMUM_SHARE_FILES * 2);
		CHECK(place_of("d:\\maps\\map03.map") == owner, "the file being hashed was replaced");
		return 0;
	}
	fprintf(stderr, "unknown case %s\n", case_name);
	return 2;
}
