/*
AE_MAP_LIBRARY_RULES_TEST.C

The installed maps library's rules (port/linux/game/ae_map_library_rules.c): map headers, sources and titles, the
search, filter and sort, every refusal of a delete or a move (links, folders linked out of the roots, a folder, a file
outside them, the sources that are not the game's to change, a map in use), the required files in fake folders, the
scan in slices over fake folders (pages, packs, skipped folders, files that are not maps), the DEV MODE command's words
and answer lines, and the scan's time over a folder of 500 maps (printed as "measured:").
*/

#define _DEFAULT_SOURCE
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

#include "../game/ae_map_library_rules.h"
#include "../src/ae_map_library_files.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static char base[512];

/* ---------- fake files */

static void header_make(unsigned char *header, char const *name, unsigned int version, int type)
{
	memset(header, 0, AE_MAP_HEADER_SIZE);
	memcpy(header, "daeh", 4);
	header[4] = (unsigned char)version;
	header[5] = (unsigned char)(version >> 8);
	snprintf((char *)header + 0x20, 32, "%s", name);
	header[0x60] = (unsigned char)type;
	memcpy(header + 0x7FC, "toof", 4);
}

static void path_join(char *out, size_t size, char const *a, char const *b)
{
	if ((size_t)snprintf(out, size, "%s/%s", a, b) >= size)
		out[0] = 0;
}

static void folder_make(char const *relative)
{
	char path[1024];

	path_join(path, sizeof(path), base, relative);
	mkdir(path, 0700);
}

/* a map file of a name at base/<relative>, padded to extra bytes past its header */
static void map_write(char const *relative, char const *name, unsigned int version, int type, long extra)
{
	unsigned char header[AE_MAP_HEADER_SIZE];
	char path[1024];
	FILE *file;

	path_join(path, sizeof(path), base, relative);
	header_make(header, name, version, type);
	file = fopen(path, "wb");
	if (!file)
	{
		printf("FAIL could not write %s\n", path);
		failures++;
		return;
	}
	fwrite(header, 1, sizeof(header), file);
	if (extra > 0)
	{
		fseek(file, extra - 1, SEEK_CUR);
		fputc(0, file);
	}
	fclose(file);
}

static void text_write(char const *relative, char const *text)
{
	char path[1024];
	FILE *file;

	path_join(path, sizeof(path), base, relative);
	file = fopen(path, "wb");
	if (file)
	{
		fputs(text, file);
		fclose(file);
	}
}

/* ---------- the scan's folders, POSIX */

static int clock_calls;

static void *posix_open(void *context, char const *path)
{
	(void)context;
	return opendir(path);
}

static int posix_next(void *context, void *folder, char *name, unsigned int size, int *is_folder)
{
	struct dirent *entry;

	(void)context;
	while ((entry = readdir((DIR *)folder)) != NULL)
	{
		if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, ".."))
			continue;
		if (strlen(entry->d_name) >= size)
			continue;
		strcpy(name, entry->d_name);
		*is_folder = entry->d_type == DT_DIR;
		return 1;
	}
	return 0;
}

static void posix_close(void *context, void *folder)
{
	(void)context;
	closedir((DIR *)folder);
}

static int posix_read(void *context, char const *path, unsigned char *header, unsigned int size,
	unsigned long long *file_size)
{
	struct stat status;
	FILE *file;
	size_t read;

	(void)context;
	file = fopen(path, "rb");
	if (!file)
		return -1;
	read = fread(header, 1, size, file);
	if (!fstat(fileno(file), &status))
		*file_size = (unsigned long long)status.st_size;
	fclose(file);
	return (int)read;
}

static unsigned long long posix_now(void *context)
{
	struct timeval now;

	(void)context;
	clock_calls++;
	gettimeofday(&now, NULL);
	return (unsigned long long)now.tv_sec * 1000000ull + (unsigned long long)now.tv_usec;
}

static struct ae_map_scan_io const posix_io = { NULL, posix_open, posix_next, posix_close, posix_read, posix_now };

/* the game's own folder calls (port/linux/src/ae_map_library_files.c), as its scan reads through them */
static unsigned long long files_entry_size;

static void *files_open(void *context, char const *path)
{
	(void)context;
	return ae_map_files_folder_open(path);
}

static int files_next(void *context, void *folder, char *name, unsigned int size, int *is_folder)
{
	(void)context;
	return ae_map_files_folder_next(folder, name, (int)size, is_folder, &files_entry_size);
}

static void files_close(void *context, void *folder)
{
	(void)context;
	ae_map_files_folder_close(folder);
}

static int files_read(void *context, char const *path, unsigned char *header, unsigned int size,
	unsigned long long *file_size)
{
	(void)context;
	*file_size = files_entry_size;
	return ae_map_files_read(path, header, (int)size);
}

static struct ae_map_scan_io const files_io = { NULL, files_open, files_next, files_close, files_read, posix_now };

static int posix_exists(void *context, char const *path)
{
	struct stat status;

	(void)context;
	return !stat(path, &status) && S_ISREG(status.st_mode);
}

/* ---------- the tests */

static struct ae_map_entry entries[AE_MAP_LIBRARY_MAXIMUM];
static struct ae_map_scan scan;

static void headers(void)
{
	unsigned char header[AE_MAP_HEADER_SIZE];
	struct ae_map_header_info info;

	header_make(header, "bloodgulch", 5, 1);
	CHECK(ae_map_library_header_read(header, sizeof(header), &info));
	CHECK(info.version == 5 && info.type == 1 && !strcmp(info.name, "bloodgulch"));
	header_make(header, "deathstar", 609, 0);
	CHECK(ae_map_library_header_read(header, sizeof(header), &info) && info.version == 609 && info.type == 0);
	CHECK(ae_map_library_header_read(header, sizeof(header) - 1, &info) == 0);
	header_make(header, "x", 6, 1);
	CHECK(!ae_map_library_header_read(header, sizeof(header), &info));
	header_make(header, "x", 13, 1);
	CHECK(ae_map_library_header_read(header, sizeof(header), &info));
	header_make(header, "x", 5, 1);
	header[0x7FC] = 'x';
	CHECK(!ae_map_library_header_read(header, sizeof(header), &info));
	header_make(header, "", 5, 1);
	CHECK(!ae_map_library_header_read(header, sizeof(header), &info));
	header_make(header, "bad\x01name", 5, 1);
	CHECK(!ae_map_library_header_read(header, sizeof(header), &info));
	/* (a name with no zero in its 32 bytes) */
	header_make(header, "x", 5, 1);
	memset(header + 0x20, 'a', 32);
	CHECK(!ae_map_library_header_read(header, sizeof(header), &info));
	CHECK(!ae_map_library_header_read(NULL, sizeof(header), &info));

	CHECK(ae_map_library_serves(0) == AE_MAP_SERVES_CAMPAIGN);
	CHECK(ae_map_library_serves(1) == AE_MAP_SERVES_MULTIPLAYER);
	CHECK(ae_map_library_serves(2) == AE_MAP_SERVES_MENU);
	CHECK(ae_map_library_serves(7) == 0);
}

static void names_and_sources(void)
{
	struct ae_map_header_info info;
	char title[AE_MAP_LIBRARY_TITLE_SIZE];
	char tiny[5];

	CHECK(ae_map_library_stock_name("BloodGulch"));
	CHECK(ae_map_library_stock_name("a10"));
	CHECK(!ae_map_library_stock_name("bloodgulch2"));
	CHECK(!ae_map_library_stock_name(NULL));
	ae_map_library_title("carousel", title, sizeof(title));
	CHECK(!strcmp(title, "Derelict"));
	ae_map_library_title("my_cool_map", title, sizeof(title));
	CHECK(!strcmp(title, "My Cool Map"));
	ae_map_library_title("my_cool_map", tiny, sizeof(tiny));
	CHECK(!strcmp(tiny, "My C"));

	memset(&info, 0, sizeof(info));
	strcpy(info.name, "bloodgulch");
	info.version = 5;
	CHECK(ae_map_library_source_of(AE_MAP_SOURCE_STOCK, &info) == AE_MAP_SOURCE_STOCK);
	strcpy(info.name, "lavaflow");
	CHECK(ae_map_library_source_of(AE_MAP_SOURCE_STOCK, &info) == AE_MAP_SOURCE_XBOX_CUSTOM);
	info.version = 609;
	CHECK(ae_map_library_source_of(AE_MAP_SOURCE_STOCK, &info) == AE_MAP_SOURCE_CUSTOM_EDITION);
	CHECK(ae_map_library_source_of(AE_MAP_SOURCE_WORKSHOP, &info) == AE_MAP_SOURCE_WORKSHOP);
	/* (a stock name in a Custom Edition file is not the stock map) */
	strcpy(info.name, "bloodgulch");
	CHECK(ae_map_library_source_of(AE_MAP_SOURCE_STOCK, &info) == AE_MAP_SOURCE_CUSTOM_EDITION);

	CHECK(!strcmp(ae_map_library_source_name(AE_MAP_SOURCE_MCC), "mcc"));
	CHECK(!strcmp(ae_map_library_source_name(99), "?"));
	CHECK(ae_map_library_source_by_name("CE") == AE_MAP_SOURCE_CUSTOM_EDITION);
	CHECK(ae_map_library_source_by_name("packs") == AE_MAP_SOURCE_MAP_PACK);
	CHECK(ae_map_library_source_by_name("workshop") == AE_MAP_SOURCE_WORKSHOP);
	CHECK(ae_map_library_source_by_name("nope") == -1);
}

static struct ae_map_entry entry_make(char const *name, char const *pack, int source, int serves,
	unsigned long long size)
{
	struct ae_map_entry entry;

	memset(&entry, 0, sizeof(entry));
	snprintf(entry.name, sizeof(entry.name), "%s", name);
	ae_map_library_title(name, entry.title, sizeof(entry.title));
	snprintf(entry.pack, sizeof(entry.pack), "%s", pack);
	snprintf(entry.path, sizeof(entry.path), "/maps/%s.map", name);
	entry.source = source;
	entry.serves = serves;
	entry.size = size;
	entry.picture = -1;
	return entry;
}

static void search_and_sort(void)
{
	struct ae_map_filter filter;
	int indexes[16];
	int count = 0;
	int kept;

	entries[count++] = entry_make("bloodgulch", "", AE_MAP_SOURCE_STOCK, AE_MAP_SERVES_MULTIPLAYER, 30);
	entries[count++] = entry_make("a10", "", AE_MAP_SOURCE_STOCK, AE_MAP_SERVES_CAMPAIGN, 90);
	entries[count++] = entry_make("hugeass", "", AE_MAP_SOURCE_CUSTOM_EDITION, AE_MAP_SERVES_MULTIPLAYER, 120);
	entries[count++] = entry_make("lockout", "H2 Demakes", AE_MAP_SOURCE_MAP_PACK, AE_MAP_SERVES_MULTIPLAYER, 40);
	entries[count++] = entry_make("lockout", "", AE_MAP_SOURCE_DOWNLOADED, AE_MAP_SERVES_MULTIPLAYER, 41);
	entries[count++] = entry_make("Zanzibar", "", AE_MAP_SOURCE_WORKSHOP, AE_MAP_SERVES_MULTIPLAYER, 10);

	memset(&filter, 0, sizeof(filter));
	kept = ae_map_library_select(entries, count, &filter, AE_MAP_SORT_NAME, indexes, 16);
	CHECK(kept == 6);
	CHECK(indexes[0] == 1 && indexes[1] == 0 && indexes[2] == 2);
	/* (two lockouts: the pack's name breaks the tie, "" first) */
	CHECK(indexes[3] == 4 && indexes[4] == 3 && indexes[5] == 5);
	/* (NULL filter keeps all) */
	CHECK(ae_map_library_select(entries, count, NULL, AE_MAP_SORT_NAME, indexes, 16) == 6);
	/* (maximum honoured) */
	CHECK(ae_map_library_select(entries, count, &filter, AE_MAP_SORT_NAME, indexes, 2) == 2);

	kept = ae_map_library_select(entries, count, &filter, AE_MAP_SORT_SIZE_LARGEST, indexes, 16);
	CHECK(kept == 6 && indexes[0] == 2 && indexes[1] == 1 && indexes[5] == 5);
	kept = ae_map_library_select(entries, count, &filter, AE_MAP_SORT_SIZE_SMALLEST, indexes, 16);
	CHECK(kept == 6 && indexes[0] == 5 && indexes[5] == 2);
	kept = ae_map_library_select(entries, count, &filter, AE_MAP_SORT_SOURCE, indexes, 16);
	CHECK(kept == 6 && indexes[0] == 1 && indexes[1] == 0 && indexes[2] == 2 && indexes[5] == 5);
	kept = ae_map_library_select(entries, count, &filter, AE_MAP_SORT_TITLE, indexes, 16);
	/* (titles: Blood Gulch, Hugeass, Lockout x2, Pillar of Autumn, Zanzibar) */
	CHECK(kept == 6 && indexes[0] == 0 && indexes[1] == 2 && indexes[4] == 1 && indexes[5] == 5);

	strcpy(filter.text, "LOCK");
	CHECK(ae_map_library_select(entries, count, &filter, AE_MAP_SORT_NAME, indexes, 16) == 2);
	strcpy(filter.text, "lock demakes");
	CHECK(ae_map_library_select(entries, count, &filter, AE_MAP_SORT_NAME, indexes, 16) == 1 && indexes[0] == 3);
	strcpy(filter.text, "  blood   gulch ");
	CHECK(ae_map_library_select(entries, count, &filter, AE_MAP_SORT_NAME, indexes, 16) == 1 && indexes[0] == 0);
	strcpy(filter.text, "autumn");
	CHECK(ae_map_library_select(entries, count, &filter, AE_MAP_SORT_NAME, indexes, 16) == 1 && indexes[0] == 1);
	strcpy(filter.text, "nothing");
	CHECK(ae_map_library_select(entries, count, &filter, AE_MAP_SORT_NAME, indexes, 16) == 0);
	filter.text[0] = 0;
	filter.sources = AE_MAP_SOURCE_BIT(AE_MAP_SOURCE_STOCK) | AE_MAP_SOURCE_BIT(AE_MAP_SOURCE_WORKSHOP);
	CHECK(ae_map_library_select(entries, count, &filter, AE_MAP_SORT_NAME, indexes, 16) == 3);
	filter.serves = AE_MAP_SERVES_CAMPAIGN;
	CHECK(ae_map_library_select(entries, count, &filter, AE_MAP_SORT_NAME, indexes, 16) == 1 && indexes[0] == 1);

	/* (no pack named: the one lockout with no pack) */
	CHECK(ae_map_library_find(entries, count, "LOCKOUT", "") == 4);
	CHECK(ae_map_library_find(entries, count, "lockout", NULL) == 4);
	CHECK(ae_map_library_find(entries, count, "lockout", "h2 demakes") == 3);
	CHECK(ae_map_library_find(entries, count, "hugeass", NULL) == 2);
	CHECK(ae_map_library_find(entries, count, "gone", NULL) == -1);
	CHECK(ae_map_library_find(entries, count, "", NULL) == -1);
	/* (two with no pack: neither is picked) */
	entries[count++] = entry_make("lockout", "", AE_MAP_SOURCE_CUSTOM_EDITION, AE_MAP_SERVES_MULTIPLAYER, 42);
	CHECK(ae_map_library_find(entries, count, "lockout", "") == -2);
	CHECK(ae_map_library_find(entries, count, "lockout", "H2 Demakes") == 3);
}

static void changes_refused(void)
{
	char const *in_use[] = { "", "hugeass" };
	struct ae_map_entry entry;

	entry = entry_make("deathstar", "", AE_MAP_SOURCE_CUSTOM_EDITION, AE_MAP_SERVES_MULTIPLAYER, 1);
	CHECK(ae_map_library_may_change(&entry, in_use, 2) == AE_MAP_CHANGE_ALLOWED);
	entry.source = AE_MAP_SOURCE_DOWNLOADED;
	CHECK(ae_map_library_may_change(&entry, in_use, 2) == AE_MAP_CHANGE_ALLOWED);
	entry.source = AE_MAP_SOURCE_STOCK;
	CHECK(ae_map_library_may_change(&entry, in_use, 2) == AE_MAP_REFUSED_STOCK);
	entry.source = AE_MAP_SOURCE_XBOX_CUSTOM;
	CHECK(ae_map_library_may_change(&entry, in_use, 2) == AE_MAP_REFUSED_XBOX_CUSTOM);
	entry.source = AE_MAP_SOURCE_MAP_PACK;
	CHECK(ae_map_library_may_change(&entry, in_use, 2) == AE_MAP_REFUSED_MAP_PACK);
	entry.source = AE_MAP_SOURCE_MCC;
	CHECK(ae_map_library_may_change(&entry, in_use, 2) == AE_MAP_REFUSED_MCC);
	/* (a Workshop map is never changed in place, but its name is still told as in use: its item is removed whole) */
	CHECK(ae_map_library_name_in_use("HugeAss", in_use, 2));
	CHECK(!ae_map_library_name_in_use("deathstar", in_use, 2));
	CHECK(!ae_map_library_name_in_use("", in_use, 2));
	CHECK(!ae_map_library_name_in_use(NULL, in_use, 2));
	entry.source = AE_MAP_SOURCE_WORKSHOP;
	CHECK(ae_map_library_may_change(&entry, in_use, 2) == AE_MAP_REFUSED_WORKSHOP);
	entry.source = 42;
	CHECK(ae_map_library_may_change(&entry, in_use, 2) == AE_MAP_REFUSED_UNKNOWN_MAP);
	CHECK(ae_map_library_may_change(NULL, in_use, 2) == AE_MAP_REFUSED_UNKNOWN_MAP);
	/* (a Custom Edition file named as a stock map) */
	entry = entry_make("BloodGulch", "", AE_MAP_SOURCE_CUSTOM_EDITION, AE_MAP_SERVES_MULTIPLAYER, 1);
	CHECK(ae_map_library_may_change(&entry, in_use, 2) == AE_MAP_REFUSED_STOCK);
	/* (in use: the loaded map's or the lobby's, case aside) */
	entry = entry_make("HugeAss", "", AE_MAP_SOURCE_CUSTOM_EDITION, AE_MAP_SERVES_MULTIPLAYER, 1);
	CHECK(ae_map_library_may_change(&entry, in_use, 2) == AE_MAP_REFUSED_IN_USE);
	entry.source = AE_MAP_SOURCE_DOWNLOADED;
	CHECK(ae_map_library_may_change(&entry, in_use, 2) == AE_MAP_REFUSED_IN_USE);
	CHECK(ae_map_library_may_change(&entry, NULL, 0) == AE_MAP_CHANGE_ALLOWED);
	{
		int answer;

		for (answer = 0; answer < NUMBER_OF_AE_MAP_CHANGE_ANSWERS; answer++)
			CHECK(ae_map_library_refusal_text(answer) && strlen(ae_map_library_refusal_text(answer)) > 4);
		CHECK(!strcmp(ae_map_library_refusal_text(-1), "?"));
	}

	CHECK(ae_map_library_path_inside("/a/maps/x.map", "/a/maps"));
	CHECK(ae_map_library_path_inside("/a/maps/x.map", "/a/maps/"));
	CHECK(ae_map_library_path_inside("/a/maps", "/a/maps"));
	CHECK(!ae_map_library_path_inside("/a/maps2/x.map", "/a/maps"));
	CHECK(!ae_map_library_path_inside("/a/map", "/a/maps"));
	CHECK(!ae_map_library_path_inside("/a/maps/x.map", ""));
	CHECK(ae_map_library_path_inside("/x", "/"));

	CHECK(ae_map_library_folder_name_ok(""));
	CHECK(ae_map_library_folder_name_ok("H2 Demakes"));
	CHECK(!ae_map_library_folder_name_ok("ce"));
	CHECK(!ae_map_library_folder_name_ok("_shared"));
	CHECK(!ae_map_library_folder_name_ok(".."));
	CHECK(!ae_map_library_folder_name_ok("a/b"));
	CHECK(!ae_map_library_folder_name_ok("a\\b"));
	CHECK(!ae_map_library_folder_name_ok("a*"));
	CHECK(!ae_map_library_folder_name_ok("a?"));
	CHECK(!ae_map_library_folder_name_ok("end."));
	CHECK(!ae_map_library_folder_name_ok("end "));
	CHECK(!ae_map_library_folder_name_ok(NULL));

	CHECK(ae_map_library_move_answer("/r/x.map", "/r/x.map", 1) == AE_MAP_REFUSED_SAME_FOLDER);
	CHECK(ae_map_library_move_answer("/r/x.map", "/r/A/x.map", 1) == AE_MAP_REFUSED_TARGET_TAKEN);
	CHECK(ae_map_library_move_answer("/r/x.map", "/r/A/x.map", 0) == AE_MAP_CHANGE_ALLOWED);
	CHECK(ae_map_library_move_answer("", "/r/A/x.map", 0) == AE_MAP_REFUSED_UNKNOWN_MAP);
	CHECK(ae_map_library_move_root_source(609) == AE_MAP_SOURCE_CUSTOM_EDITION);
	CHECK(ae_map_library_move_root_source(7) == AE_MAP_SOURCE_CUSTOM_EDITION);
	CHECK(ae_map_library_move_root_source(5) == AE_MAP_SOURCE_STOCK);
	/* (an MCC-format map stays where Custom Edition maps are: the maps folder would take it for an Xbox map) */
	CHECK(ae_map_library_move_root_source(13) == AE_MAP_SOURCE_CUSTOM_EDITION);
	{
		char path[40];

		CHECK(ae_map_library_move_target("/r", "Mine", "x", path, sizeof(path)) && !strcmp(path, "/r/Mine/x.map"));
		CHECK(ae_map_library_move_target("/r/", "", "x", path, sizeof(path)) && !strcmp(path, "/r/x.map"));
		CHECK(!ae_map_library_move_target("/r", "a_very_long_folder_name_indeed_yes", "x", path, sizeof(path)));
		CHECK(!ae_map_library_move_target("", "", "x", path, sizeof(path)));
		/* (a game path's root: its own separator) */
		CHECK(ae_map_library_move_target("d:\\maps_ce\\", "Mine", "x", path, sizeof(path)) &&
			!strcmp(path, "d:\\maps_ce\\Mine\\x.map"));
	}
}

/* the real paths: links, a folder linked out of the roots, a folder, a file outside */
static void paths_checked(void)
{
	char root[1024];
	char real_root[4096];
	char outside[1024];
	char path[1024];
	char target[1024];
	char real[1024];
	char const *roots[1];

	folder_make("maps_ce");
	folder_make("maps_ce/Mine");
	folder_make("elsewhere");
	map_write("maps_ce/deathstar.map", "deathstar", 609, 1, 0);
	map_write("maps_ce/Mine/hugeass.map", "hugeass", 609, 1, 0);
	map_write("elsewhere/secret.map", "secret", 609, 1, 0);
	path_join(root, sizeof(root), base, "maps_ce");
	CHECK(realpath(root, real_root) != NULL);
	path_join(outside, sizeof(outside), base, "elsewhere");
	roots[0] = real_root;

	path_join(path, sizeof(path), base, "maps_ce/deathstar.map");
	CHECK(ae_map_files_path_check(path, roots, 1, real, sizeof(real)) == AE_MAP_CHANGE_ALLOWED);
	CHECK(ae_map_library_path_inside(real, real_root));
	path_join(path, sizeof(path), base, "maps_ce/Mine/hugeass.map");
	CHECK(ae_map_files_path_check(path, roots, 1, real, sizeof(real)) == AE_MAP_CHANGE_ALLOWED);
	/* (too small for the real path) */
	CHECK(ae_map_files_path_check(path, roots, 1, real, 8) == AE_MAP_REFUSED_PATH_TOO_LONG);
	/* (a map file that is a link to one outside) */
	path_join(target, sizeof(target), outside, "secret.map");
	path_join(path, sizeof(path), base, "maps_ce/linked.map");
	CHECK(!symlink(target, path));
	CHECK(ae_map_files_path_check(path, roots, 1, real, sizeof(real)) == AE_MAP_REFUSED_LINK);
	/* (a folder in the root that is a link out of it) */
	path_join(path, sizeof(path), base, "maps_ce/Away");
	CHECK(!symlink(outside, path));
	path_join(path, sizeof(path), base, "maps_ce/Away/secret.map");
	CHECK(ae_map_files_path_check(path, roots, 1, real, sizeof(real)) == AE_MAP_REFUSED_OUTSIDE_ROOTS);
	/* (".." out of the root) */
	path_join(path, sizeof(path), base, "maps_ce/../elsewhere/secret.map");
	CHECK(ae_map_files_path_check(path, roots, 1, real, sizeof(real)) == AE_MAP_REFUSED_OUTSIDE_ROOTS);
	/* (a file outside, a folder, nothing there, the root itself) */
	path_join(path, sizeof(path), outside, "secret.map");
	CHECK(ae_map_files_path_check(path, roots, 1, real, sizeof(real)) == AE_MAP_REFUSED_OUTSIDE_ROOTS);
	path_join(path, sizeof(path), base, "maps_ce/Mine");
	CHECK(ae_map_files_path_check(path, roots, 1, real, sizeof(real)) == AE_MAP_REFUSED_NOT_A_FILE);
	path_join(path, sizeof(path), base, "maps_ce/gone.map");
	CHECK(ae_map_files_path_check(path, roots, 1, real, sizeof(real)) == AE_MAP_REFUSED_UNKNOWN_MAP);
	CHECK(ae_map_files_path_check("", roots, 1, real, sizeof(real)) == AE_MAP_REFUSED_UNKNOWN_MAP);
	/* (no roots: nothing is inside) */
	path_join(path, sizeof(path), base, "maps_ce/deathstar.map");
	CHECK(ae_map_files_path_check(path, roots, 0, real, sizeof(real)) == AE_MAP_REFUSED_OUTSIDE_ROOTS);
	/* (a root whose name is the start of another's: maps_ce2 is not inside maps_ce) */
	folder_make("maps_ce2");
	map_write("maps_ce2/other.map", "other", 609, 1, 0);
	path_join(path, sizeof(path), base, "maps_ce2/other.map");
	CHECK(ae_map_files_path_check(path, roots, 1, real, sizeof(real)) == AE_MAP_REFUSED_OUTSIDE_ROOTS);

	/* (the host calls a change makes: real paths, a rename that never replaces, a folder made, a delete) */
	{
		char from[1024];
		char to[1024];
		char folder[1024];
		unsigned long long size = 0;
		unsigned char bytes[8];

		CHECK(ae_map_files_real_path(root, real, sizeof(real)) && !strcmp(real, real_root));
		CHECK(!ae_map_files_real_path(root, real, 4));
		path_join(path, sizeof(path), base, "maps_ce/none");
		CHECK(!ae_map_files_real_path(path, real, sizeof(real)));
		path_join(from, sizeof(from), base, "maps_ce/deathstar.map");
		path_join(to, sizeof(to), base, "maps_ce/Mine/hugeass.map");
		CHECK(ae_map_files_is_file(from) && !ae_map_files_is_file(root) && ae_map_files_exists(root));
		path_join(path, sizeof(path), base, "maps_ce/linked.map");
		CHECK(ae_map_files_is_link(path) && !ae_map_files_is_link(from) && !ae_map_files_is_link(""));
		path_join(path, sizeof(path), base, "maps_ce/Away");
		CHECK(ae_map_files_is_link(path));
		CHECK(ae_map_files_size(from, &size) && size == AE_MAP_HEADER_SIZE && !ae_map_files_size(root, &size));
		CHECK(ae_map_files_read(from, bytes, sizeof(bytes)) == 8 && !memcmp(bytes, "daeh", 4));
		CHECK(ae_map_files_read(root, bytes, 0) == -1);
		/* (a name taken: refused, and the file there is the same one) */
		CHECK(!ae_map_files_rename_new(from, to) && ae_map_files_is_file(from));
		CHECK(ae_map_files_read(to, bytes, sizeof(bytes)) == 8);
		path_join(folder, sizeof(folder), base, "maps_ce/New Folder");
		CHECK(ae_map_files_make_folder(folder) && ae_map_files_make_folder(folder));
		CHECK(!ae_map_files_make_folder(from));
		path_join(to, sizeof(to), base, "maps_ce/New Folder/deathstar.map");
		CHECK(ae_map_files_rename_new(from, to) && !ae_map_files_exists(from) && ae_map_files_is_file(to));
		/* (a link's name taken counts too: a dangling link is never replaced) */
		path_join(path, sizeof(path), base, "maps_ce/dangling.map");
		CHECK(!symlink("/nonexistent/x.map", path));
		CHECK(!ae_map_files_rename_new(to, path) && ae_map_files_is_file(to));
		CHECK(ae_map_files_delete(to) && !ae_map_files_exists(to) && !ae_map_files_delete(to));
	}
}

static void required_files(void)
{
	struct ae_map_required_file files[AE_MAP_REQUIRED_MAXIMUM];
	char first[1024];
	char second[1024];
	char const *folders[2];
	char line[512];
	int count;

	folder_make("ce_one");
	folder_make("ce_two");
	text_write("ce_two/bitmaps.map", "x");
	text_write("ce_one/loc.map", "x");
	folder_make("ce_one/sounds.map");
	path_join(first, sizeof(first), base, "ce_one");
	path_join(second, sizeof(second), base, "ce_two");
	folders[0] = first;
	folders[1] = second;

	count = ae_map_library_required(AE_MAP_REQUIRED_CUSTOM_EDITION, folders, 2, posix_exists, NULL, files,
		AE_MAP_REQUIRED_MAXIMUM);
	CHECK(count == 3);
	CHECK(!strcmp(files[0].file, "bitmaps.map") && files[0].found && !strcmp(files[0].where, second));
	/* (a folder named sounds.map is not the file: missing, and where it must go is the first folder) */
	CHECK(!strcmp(files[1].file, "sounds.map") && !files[1].found && !strcmp(files[1].where, first));
	CHECK(!strcmp(files[2].file, "loc.map") && files[2].found && !strcmp(files[2].where, first));
	CHECK(files[1].what && files[1].unlocks && strstr(files[1].unlocks, "sounds"));
	ae_map_library_missing_line(AE_MAP_REQUIRED_CUSTOM_EDITION, files, count, line, sizeof(line));
	{
		char expected[1200];

		snprintf(expected, sizeof(expected), "required ce: ok bitmaps.map, loc.map; MISSING sounds.map (put in %s)",
			first);
		CHECK(!strcmp(line, expected));
	}
	/* (too small to say anything: an empty line, never one left unended) */
	memset(line, 'x', sizeof(line));
	ae_map_library_missing_line(AE_MAP_REQUIRED_CUSTOM_EDITION, files, count, line, 4);
	CHECK(line[0] == 0);
	memset(line, 'x', sizeof(line));
	ae_map_library_list_line(entries, 0, NULL, 0, line, 4);
	CHECK(line[0] == 0);
	ae_map_library_missing_line(AE_MAP_REQUIRED_CUSTOM_EDITION, files, count, line, 24);
	CHECK(strlen(line) < 24 && strstr(line, "...") != NULL);

	count = ae_map_library_required(AE_MAP_REQUIRED_MCC, folders, 1, posix_exists, NULL, files, 2);
	CHECK(count == 2 && !files[0].found && files[1].found == 0);
	CHECK(ae_map_library_required(AE_MAP_REQUIRED_MCC, NULL, 0, posix_exists, NULL, files, 8) == 3 && !files[0].found &&
		!files[0].where[0]);
	CHECK(ae_map_library_required(9, folders, 2, posix_exists, NULL, files, 8) == 0);
	ae_map_library_missing_line(AE_MAP_REQUIRED_MCC, files, 0, line, sizeof(line));
	CHECK(!strcmp(line, "required mcc: no folder to look in"));
	/* (none found: the missing ones alone) */
	count = ae_map_library_required(AE_MAP_REQUIRED_MCC, folders, 1, posix_exists, NULL, files, 8);
	ae_map_library_missing_line(AE_MAP_REQUIRED_MCC, files, count, line, sizeof(line));
	CHECK(!strncmp(line, "required mcc: MISSING singleplayer/bitmaps.map, multiplayer/bitmaps.map, multiplayer/sounds.map (put in ",
		104));
}

static int entry_of(int count, char const *name, char const *pack)
{
	return ae_map_library_find(entries, count, name, pack);
}

static void scanned_in_slices(void)
{
	struct ae_map_root roots[4];
	int steps = 0;
	int index;

	folder_make("maps");
	folder_make("maps/Comp");
	folder_make("maps/ce");
	folder_make("maps/_shared");
	folder_make("packs");
	folder_make("packs/H2 Demakes");
	folder_make("downloads");
	map_write("maps/bloodgulch.map", "bloodgulch", 5, 1, 4096);
	map_write("maps/a10.map", "a10", 5, 0, 0);
	map_write("maps/ui.map", "ui", 5, 2, 0);
	map_write("maps/lavaflow.map", "lavaflow", 5, 1, 0);
	map_write("maps/Comp/prisoner_pro.map", "prisoner_pro", 5, 1, 0);
	/* (skipped folders: ce is a root of its own, _shared is MCC's) */
	map_write("maps/ce/notme.map", "notme", 609, 1, 0);
	map_write("maps/_shared/notme2.map", "notme2", 5, 1, 0);
	text_write("maps/readme.map", "not a map");
	text_write("maps/notes.txt", "x");
	map_write("packs/H2 Demakes/lockout.map", "lockout", 5, 1, 0);
	map_write("downloads/coldsnap.map", "coldsnap", 609, 1, 0);

	memset(roots, 0, sizeof(roots));
	path_join(roots[0].path, sizeof(roots[0].path), base, "maps");
	roots[0].source = AE_MAP_SOURCE_STOCK;
	roots[0].folders = 1;
	path_join(roots[1].path, sizeof(roots[1].path), base, "packs/");
	roots[1].source = AE_MAP_SOURCE_MAP_PACK;
	roots[1].folders = 1;
	roots[1].pack_folders = 1;
	path_join(roots[2].path, sizeof(roots[2].path), base, "downloads");
	roots[2].source = AE_MAP_SOURCE_DOWNLOADED;
	path_join(roots[3].path, sizeof(roots[3].path), base, "missing_folder");
	roots[3].source = AE_MAP_SOURCE_CUSTOM_EDITION;

	ae_map_scan_begin(&scan, roots, 4, entries, AE_MAP_LIBRARY_MAXIMUM);
	/* (a slice reads one header at most) */
	while (ae_map_scan_step(&scan, &posix_io, 1, 1000000ull) && steps < 100)
		steps++;
	CHECK(scan.done);
	CHECK(scan.count == 7);
	CHECK(scan.not_maps == 1);
	CHECK(scan.unopened == 1);
	CHECK(scan.steps >= 8);
	index = entry_of(scan.count, "bloodgulch", "");
	CHECK(index >= 0 && entries[index].source == AE_MAP_SOURCE_STOCK &&
		entries[index].serves == AE_MAP_SERVES_MULTIPLAYER && entries[index].size == 2048 + 4096 &&
		!strcmp(entries[index].title, "Blood Gulch") && entries[index].root == 0 && entries[index].picture == -1);
	index = entry_of(scan.count, "a10", "");
	CHECK(index >= 0 && entries[index].serves == AE_MAP_SERVES_CAMPAIGN && entries[index].source == AE_MAP_SOURCE_STOCK);
	index = entry_of(scan.count, "ui", "");
	CHECK(index >= 0 && entries[index].serves == AE_MAP_SERVES_MENU);
	index = entry_of(scan.count, "lavaflow", "");
	CHECK(index >= 0 && entries[index].source == AE_MAP_SOURCE_XBOX_CUSTOM);
	index = entry_of(scan.count, "prisoner_pro", "");
	CHECK(index >= 0 && !strcmp(entries[index].folder, "Comp") && entries[index].source == AE_MAP_SOURCE_XBOX_CUSTOM);
	index = entry_of(scan.count, "lockout", "H2 Demakes");
	CHECK(index >= 0 && entries[index].source == AE_MAP_SOURCE_MAP_PACK && !entries[index].folder[0]);
	index = entry_of(scan.count, "coldsnap", "");
	CHECK(index >= 0 && entries[index].source == AE_MAP_SOURCE_DOWNLOADED && entries[index].version == 609);
	CHECK(entry_of(scan.count, "notme", "") == -1 && entry_of(scan.count, "notme2", "") == -1);

	/* (the same folders through the game's own folder calls: the same maps, sizes and all) */
	{
		static struct ae_map_entry again[AE_MAP_LIBRARY_MAXIMUM];
		static struct ae_map_scan second;
		int found;

		ae_map_scan_begin(&second, roots, 4, again, AE_MAP_LIBRARY_MAXIMUM);
		while (ae_map_scan_step(&second, &files_io, 1, 1000000ull))
		{
		}
		CHECK(second.count == scan.count && second.not_maps == 1 && second.unopened == 1);
		for (index = 0; index < scan.count; index++)
		{
			found = ae_map_library_find(again, second.count, entries[index].name, entries[index].pack);
			CHECK(found >= 0 && again[found].size == entries[index].size && again[found].source == entries[index].source &&
				!strcmp(again[found].folder, entries[index].folder) && !strcmp(again[found].path, entries[index].path));
		}
		/* (two folders open at most: a third is refused, and one closed frees its place) */
		{
			void *one = ae_map_files_folder_open(roots[0].path);
			void *two = ae_map_files_folder_open(roots[0].path);

			CHECK(one && two && !ae_map_files_folder_open(roots[0].path));
			ae_map_files_folder_close(two);
			two = ae_map_files_folder_open(roots[0].path);
			CHECK(two != NULL);
			ae_map_files_folder_close(one);
			ae_map_files_folder_close(two);
			CHECK(!ae_map_files_folder_open(roots[3].path));
		}
	}
	/* (a full library: the rest counted, not kept) */
	ae_map_scan_begin(&scan, roots, 4, entries, 3);
	while (ae_map_scan_step(&scan, &posix_io, 64, 1000000ull))
	{
	}
	CHECK(scan.count == 3 && scan.dropped == 4);
	/* (stopped half way: the open folder closed) */
	ae_map_scan_begin(&scan, roots, 4, entries, AE_MAP_LIBRARY_MAXIMUM);
	CHECK(ae_map_scan_step(&scan, &posix_io, 1, 1000000ull));
	ae_map_scan_end(&scan, &posix_io);
	CHECK(scan.done && !scan.folder && !ae_map_scan_step(&scan, &posix_io, 1, 1000000ull));
	/* (no roots: done at once) */
	ae_map_scan_begin(&scan, roots, 0, entries, AE_MAP_LIBRARY_MAXIMUM);
	CHECK(scan.done && !ae_map_scan_step(&scan, &posix_io, 1, 1000000ull));
}

/* 500 maps in one folder: the slices' longest time and the whole scan's, printed */
static void five_hundred_maps(void)
{
	struct ae_map_root root;
	unsigned long long one_go;
	int index;

	folder_make("big");
	for (index = 0; index < 500; index++)
	{
		char relative[64];
		char name[32];

		snprintf(name, sizeof(name), "map_%03d", index);
		snprintf(relative, sizeof(relative), "big/%s.map", name);
		map_write(relative, name, 609, 1, 0);
	}
	memset(&root, 0, sizeof(root));
	path_join(root.path, sizeof(root.path), base, "big");
	root.source = AE_MAP_SOURCE_CUSTOM_EDITION;

	/* (the menus' slice: 16 headers or 2 ms, whichever comes first) */
	ae_map_scan_begin(&scan, &root, 1, entries, AE_MAP_LIBRARY_MAXIMUM);
	while (ae_map_scan_step(&scan, &posix_io, 16, 2000ull))
	{
	}
	CHECK(scan.count == 500);
	CHECK(scan.steps >= 500 / 16);
	printf("measured: 500 maps in %d slices of 16 headers / 2 ms: longest slice %llu us, all %llu us\n", scan.steps,
		scan.longest_step_us, scan.total_us);
	/* (one go, as a scan with no slices would stall a frame) */
	ae_map_scan_begin(&scan, &root, 1, entries, AE_MAP_LIBRARY_MAXIMUM);
	ae_map_scan_step(&scan, &posix_io, 1 << 30, ~0ull);
	one_go = scan.longest_step_us;
	CHECK(scan.count == 500 && scan.steps == 1);
	printf("measured: 500 maps in one go: %llu us\n", one_go);
	ae_map_scan_begin(&scan, &root, 1, entries, AE_MAP_LIBRARY_MAXIMUM);
	ae_map_scan_step(&scan, &files_io, 1 << 30, ~0ull);
	CHECK(scan.count == 500);
	printf("measured: 500 maps in one go through the game's folder calls: %llu us\n", scan.longest_step_us);
	{
		struct ae_map_filter filter;
		static int indexes[AE_MAP_LIBRARY_MAXIMUM];
		unsigned long long start = posix_now(NULL);
		int kept;

		/* (sizes in a shuffled order, so the sort has work to do) */
		for (index = 0; index < scan.count; index++)
			entries[index].size = (unsigned long long)((index * 7919) % 500);
		memset(&filter, 0, sizeof(filter));
		strcpy(filter.text, "map_4");
		kept = ae_map_library_select(entries, scan.count, &filter, AE_MAP_SORT_SIZE_LARGEST, indexes,
			AE_MAP_LIBRARY_MAXIMUM);
		CHECK(kept == 100);
		filter.text[0] = 0;
		kept = ae_map_library_select(entries, scan.count, &filter, AE_MAP_SORT_TITLE, indexes, AE_MAP_LIBRARY_MAXIMUM);
		CHECK(kept == 500);
		kept = ae_map_library_select(entries, scan.count, &filter, AE_MAP_SORT_SIZE_SMALLEST, indexes,
			AE_MAP_LIBRARY_MAXIMUM);
		CHECK(kept == 500);
		for (index = 1; index < kept; index++)
			CHECK(entries[indexes[index - 1]].size <= entries[indexes[index]].size);
		printf("measured: three searches and sorts of 500 maps: %llu us\n", posix_now(NULL) - start);
	}
}

static void commands(void)
{
	struct ae_map_command command;
	int indexes[8];
	char line[96];
	int count = 0;

	CHECK(ae_map_library_command_parse("list", &command) && command.verb == AE_MAP_COMMAND_LIST &&
		!command.text[0] && command.source == -1);
	CHECK(ae_map_library_command_parse("list  blood gulch ", &command) && !strcmp(command.text, "blood gulch"));
	CHECK(ae_map_library_command_parse("list ce: lock", &command) &&
		command.source == AE_MAP_SOURCE_CUSTOM_EDITION && !strcmp(command.text, "lock"));
	CHECK(ae_map_library_command_parse("LIST workshop:", &command) && command.source == AE_MAP_SOURCE_WORKSHOP &&
		!command.text[0]);
	CHECK(!ae_map_library_command_parse("list nope: x", &command));
	CHECK(ae_map_library_command_parse("missing", &command) && command.verb == AE_MAP_COMMAND_MISSING);
	CHECK(!ae_map_library_command_parse("missing x", &command));
	CHECK(ae_map_library_command_parse("scan", &command) && command.verb == AE_MAP_COMMAND_SCAN);
	CHECK(ae_map_library_command_parse("delete hugeass", &command) && command.verb == AE_MAP_COMMAND_DELETE &&
		!strcmp(command.text, "hugeass") && !command.pack[0]);
	CHECK(ae_map_library_command_parse("delete lockout@H2Demakes", &command) && !strcmp(command.text, "lockout") &&
		!strcmp(command.pack, "H2Demakes"));
	CHECK(!ae_map_library_command_parse("delete", &command));
	CHECK(!ae_map_library_command_parse("delete a b", &command));
	CHECK(!ae_map_library_command_parse("delete @pack", &command));
	CHECK(!ae_map_library_command_parse("delete map@", &command));
	CHECK(ae_map_library_command_parse("move hugeass H2 Demakes ", &command) && command.verb == AE_MAP_COMMAND_MOVE &&
		!strcmp(command.text, "hugeass") && !strcmp(command.folder, "H2 Demakes"));
	CHECK(ae_map_library_command_parse("move hugeass .", &command) && !command.folder[0]);
	CHECK(ae_map_library_command_parse("move hugeass \"H2 Demakes\"", &command) && !strcmp(command.folder, "H2 Demakes"));
	CHECK(!ae_map_library_command_parse("move hugeass", &command));
	CHECK(!ae_map_library_command_parse("", &command));
	CHECK(!ae_map_library_command_parse("frob", &command));
	CHECK(!ae_map_library_command_parse(NULL, &command));
	{
		char huge[600];

		memset(huge, 'a', sizeof(huge) - 1);
		huge[sizeof(huge) - 1] = 0;
		CHECK(!ae_map_library_command_parse(huge, &command));
		memcpy(huge, "list ", 5);
		CHECK(!ae_map_library_command_parse(huge, &command));
		memcpy(huge, "delete ", 7);
		CHECK(!ae_map_library_command_parse(huge, &command));
	}

	entries[count++] = entry_make("bloodgulch", "", AE_MAP_SOURCE_STOCK, AE_MAP_SERVES_MULTIPLAYER, 30);
	entries[count++] = entry_make("lockout", "H2 Demakes", AE_MAP_SOURCE_MAP_PACK, AE_MAP_SERVES_MULTIPLAYER, 40);
	entries[count++] = entry_make("hugeass", "", AE_MAP_SOURCE_CUSTOM_EDITION, AE_MAP_SERVES_MULTIPLAYER, 120);
	indexes[0] = 0;
	indexes[1] = 2;
	indexes[2] = 1;
	ae_map_library_list_line(entries, count, indexes, 3, line, sizeof(line));
	CHECK(!strcmp(line, "maps: 3 of 3 (stock 1, ce 1, pack 1): bloodgulch, hugeass, lockout@H2 Demakes"));
	ae_map_library_list_line(entries, count, indexes, 0, line, sizeof(line));
	CHECK(!strcmp(line, "maps: 0 of 3 ()"));
	ae_map_library_list_line(entries, count, indexes, 3, line, 48);
	CHECK(strlen(line) < 48 && !strcmp(line + strlen(line) - 3, "..."));
}

int main(void)
{
	char pattern[] = "/tmp/ae_map_library_test_XXXXXX";
	char command[600];

	if (!mkdtemp(pattern))
	{
		printf("FAIL mkdtemp: %s\n", strerror(errno));
		return 1;
	}
	snprintf(base, sizeof(base), "%s", pattern);
	headers();
	names_and_sources();
	search_and_sort();
	changes_refused();
	paths_checked();
	required_files();
	scanned_in_slices();
	five_hundred_maps();
	commands();
	snprintf(command, sizeof(command), "rm -rf '%s'", base);
	if (system(command))
		printf("note: %s left\n", base);
	if (failures)
	{
		printf("%d failures\n", failures);
		return 1;
	}
	printf("ae_map_library_rules_test: ok\n");
	return 0;
}
