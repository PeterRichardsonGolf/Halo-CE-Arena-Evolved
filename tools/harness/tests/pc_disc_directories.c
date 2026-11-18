#define _GNU_SOURCE
#include "harness.h"
#include <sys/mman.h>
#include <unistd.h>
/* pc_disc.c's list_directory on crafted disc images: a root whose folder records all point back at the root itself
(the depth limit stops it), and a chain of folders whose 60 records each name the next one (within the depth limit:
60^7 reads), stop after a bounded number of folders and mark the disc damaged; a record that starts too close to the table's end to hold its name length is damaged, not read past
(the table sits right before an unreadable page here: a read past it faults) */

struct cabinet_import { int unused; };
#include "config.inc"

static unsigned char image[12 * 2048];
static unsigned long reads;

/* the table in the last bytes before an unreadable page */
static void *guarded_malloc(size_t size)
{
	long page = sysconf(_SC_PAGESIZE);
	size_t pages = (size + page - 1) / page + 1;
	unsigned char *base = mmap(NULL, pages * page, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

	CHECK(base != MAP_FAILED, "mmap");
	mprotect(base + (pages - 1) * page, page, PROT_NONE);
	return base + (pages - 1) * page - size;
}
static void guarded_free(void *pointer) { }
#define malloc guarded_malloc
#define free guarded_free

static int cabinet_read(struct cabinet_import *import, unsigned long long offset, void *buffer, unsigned long size)
{
	reads++;
	CHECK(reads < 100000, "the listing keeps reading folders: %lu reads", reads);
	if (offset + size > sizeof(image))
		return 0;
	memcpy(buffer, image + offset, size);
	return 1;
}
static int cabinet_map_name(const char *path, char *map, size_t size) { return 0; }
#include "under_test.inc"

/* a directory record at table + at: its length, extent sector and size, flags, name */
static unsigned int put_record(unsigned char *table, unsigned int at, unsigned int sector, unsigned int size,
	int directory, const char *name)
{
	unsigned int name_length = (unsigned int)strlen(name);
	unsigned int length = 33 + name_length + ((33 + name_length) & 1);

	if (!name_length)
	{
		name_length = 1;
		length = 34;
	}
	table[at] = (unsigned char)length;
	memcpy(table + at + 2, &sector, 4);
	memcpy(table + at + 10, &size, 4);
	table[at + 25] = directory ? 2 : 0;
	table[at + 32] = (unsigned char)name_length;
	memcpy(table + at + 33, name, strlen(name));
	return at + length;
}

int main(int argc, char **argv)
{
	char const *case_name = argv[1];
	struct disc_files *list = calloc(1, sizeof(*list));
	struct cabinet_import import;
	unsigned char *root = image + 2048;
	unsigned int at = 0;
	int index;

	CASE("self-loop")
	{
		at = put_record(root, at, 1, 2048, 1, "");
		at = put_record(root, at, 1, 2048, 1, "\001");
		for (index = 0; index < 60 && at + 40 < 2048; index++)
		{
			char name[8];
			snprintf(name, sizeof(name), "D%02d", index);
			at = put_record(root, at, 1, 2048, 1, name);
		}
		list_directory(&import, 1, 2048, "", 0, list);
		CHECK(list->damaged, "a disc whose folders loop is not marked damaged");
		CHECK(reads <= 2000, "%lu folder reads for a looping disc", reads);
		return 0;
	}
	CASE("wide-chain")
	{
		/* folders 1..8, each listing 60 folders that all are the next one (the last lists none): within the depth
		limit, so only a cap on the folders listed stops it short of 60^7 reads */
		int level;

		for (level = 1; level <= 8; level++)
		{
			unsigned char *table = image + level * 2048;
			unsigned int position = 0;

			position = put_record(table, position, level, 2048, 1, "");
			position = put_record(table, position, level, 2048, 1, "\001");
			for (index = 0; level < 8 && index < 60 && position + 40 < 2048; index++)
			{
				char name[8];
				snprintf(name, sizeof(name), "D%02d", index);
				position = put_record(table, position, level + 1, 2048, 1, name);
			}
		}
		list_directory(&import, 1, 2048, "", 0, list);
		CHECK(list->damaged, "a disc with 60^7 folders is not marked damaged");
		CHECK(reads <= 2000, "%lu folder reads", reads);
		return 0;
	}
	CASE("record-at-end")
	{
		/* a 40-byte table: one 34-byte record, then a record's length byte 6 bytes before the end */
		at = put_record(root, at, 1, 40, 1, "");
		root[at] = 34;
		list_directory(&import, 1, 40, "", 0, list);
		CHECK(list->damaged, "a record cut off at the table's end not marked damaged");
		return 0;
	}
	CASE("plain")
	{
		/* a root with ".", "..", one folder (sector 2) holding ".", ".." */
		unsigned char *folder = image + 2 * 2048;
		unsigned int folder_at = 0;

		at = put_record(root, at, 1, 2048, 1, "");
		at = put_record(root, at, 1, 2048, 1, "\001");
		at = put_record(root, at, 2, 2048, 1, "MAPS");
		folder_at = put_record(folder, folder_at, 2, 2048, 1, "");
		folder_at = put_record(folder, folder_at, 1, 2048, 1, "\001");
		list_directory(&import, 1, 2048, "", 0, list);
		CHECK(!list->damaged, "a plain disc marked damaged");
		CHECK(reads == 2, "%lu reads for two folders", reads);
		return 0;
	}
	fprintf(stderr, "unknown case %s\n", case_name);
	return 2;
}
