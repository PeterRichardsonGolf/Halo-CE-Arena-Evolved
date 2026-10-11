/* AE's MCC detection (port/linux/src/ae_mcc.c) over fake Steam trees made in $AE_TEST_SCRATCH: the research note's
cases (a) to (i) (notes/mcc-install-check-research-2026-10-10.md; (j), the player's own files winning, is the game's:
tools/test_ae_mcc.py), the VDF reader's escapes, and hostile files (strings and blocks left open, deep nesting, a huge
file, long tokens). Linux only: the trees are made with mkdir. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "ae_mcc.h"

/* (the trees' paths are short: GCC's guesses at their lengths are not) */
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic ignored "-Wformat-truncation"
#endif

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static char scratch[512];

/* mkdir -p */
static void make_folders(const char *path)
{
	char partial[AE_MCC_PATH_SIZE];
	size_t index;

	snprintf(partial, sizeof(partial), "%s", path);
	for (index = 1; partial[index]; index++)
	{
		if (partial[index] == '/')
		{
			partial[index] = 0;
			mkdir(partial, 0755);
			partial[index] = '/';
		}
	}
	mkdir(partial, 0755);
}

static void write_file(const char *path, const void *data, size_t size)
{
	char folder[AE_MCC_PATH_SIZE];
	char *slash;
	FILE *file;

	snprintf(folder, sizeof(folder), "%s", path);
	slash = strrchr(folder, '/');
	if (slash)
	{
		*slash = 0;
		make_folders(folder);
	}
	file = fopen(path, "wb");
	CHECK(file != NULL);
	if (!file)
		return;
	if (size)
		fwrite(data, 1, size, file);
	fclose(file);
}

static void write_text(const char *path, const char *text)
{
	write_file(path, text, strlen(text));
}

static void put_word(unsigned char *at, unsigned long value)
{
	at[0] = (unsigned char)value;
	at[1] = (unsigned char)(value >> 8);
	at[2] = (unsigned char)(value >> 16);
	at[3] = (unsigned char)(value >> 24);
}

/* a small resource map of a type: header, one path, a table of one resource */
static void write_resource(const char *path, int type, int truncated)
{
	unsigned char data[64];

	memset(data, 0, sizeof(data));
	put_word(data, (unsigned long)(type + 1));
	put_word(data + 4, 16);
	put_word(data + 8, 32);
	put_word(data + 12, 1);
	memcpy(data + 16, "a\\b", 3);
	/* (cut short: the table past the file's end) */
	write_file(path, data, truncated ? 40 : 44);
}

/* an MCC folder with its three files (bitmaps.map cut short if asked) */
static void make_install(const char *root, int truncated_bitmaps)
{
	int type;

	for (type = 0; ae_mcc_resource_name(type); type++)
	{
		char path[AE_MCC_PATH_SIZE];

		ae_mcc_resource_path(root, ae_mcc_resource_name(type), path, sizeof(path));
		write_resource(path, type, truncated_bitmaps && type == 0);
	}
}

static void manifest(const char *library, long state_flags, const char *install_dir)
{
	char path[AE_MCC_PATH_SIZE], text[2048];

	snprintf(path, sizeof(path), "%s/steamapps/appmanifest_976730.acf", library);
	snprintf(text, sizeof(text),
		"\"AppState\"\n{\n\t\"appid\"\t\t\"976730\"\n\t\"universe\"\t\t\"1\"\n\t\"name\"\t\t\"Halo: The Master Chief "
		"Collection\"\n\t\"StateFlags\"\t\t\"%ld\"\n\t\"installdir\"\t\t\"%s\"\n\t\"buildid\"\t\t\"123\"\n}\n",
		state_flags, install_dir);
	write_text(path, text);
}

/* a VDF string's escapes: \ and " */
static void escaped(const char *from, char *to, size_t size)
{
	size_t length = 0;

	for (; *from && length + 2 < size; from++)
	{
		if (*from == '\\' || *from == '"')
			to[length++] = '\\';
		to[length++] = *from;
	}
	to[length] = 0;
}

/* libraryfolders.vdf: each library's path, and whether it lists MCC */
static void library_folders(const char *steam_root, const char *const *paths, const int *lists, int count)
{
	char path[AE_MCC_PATH_SIZE], text[8192], quoted[2048];
	int index;

	snprintf(text, sizeof(text), "\"libraryfolders\"\n{\n");
	for (index = 0; index < count; index++)
	{
		escaped(paths[index], quoted, sizeof(quoted));
		snprintf(text + strlen(text), sizeof(text) - strlen(text),
			"\t\"%d\"\n\t{\n\t\t\"path\"\t\t\"%s\"\n\t\t\"label\"\t\t\"\"\n\t\t\"contentid\"\t\t\"123\"\n"
			"\t\t\"totalsize\"\t\t\"0\"\n\t\t\"apps\"\n\t\t{\n\t\t\t\"228980\"\t\t\"1000\"\n%s\t\t}\n\t}\n",
			index, quoted, lists[index] ? "\t\t\t\"976730\"\t\t\"27372615038\"\n" : "");
	}
	strcat(text, "}\n");
	snprintf(path, sizeof(path), "%s/steamapps/libraryfolders.vdf", steam_root);
	write_text(path, text);
}

static int folder_exists(const char *path)
{
	struct stat information;

	return stat(path, &information) == 0 && S_ISDIR(information.st_mode);
}

static int detect_roots(const char *config_path, const char *environment_path, const char *const *roots, int count,
	struct ae_mcc_result *result)
{
	struct ae_mcc_inputs inputs;

	memset(&inputs, 0, sizeof(inputs));
	inputs.config_path = config_path;
	inputs.environment_path = environment_path;
	inputs.steam_roots = roots;
	inputs.steam_root_count = count;
	inputs.directory_exists = folder_exists;
	return ae_mcc_detect(&inputs, result);
}

static int detect_one(const char *steam_root, struct ae_mcc_result *result)
{
	const char *roots[1];

	roots[0] = steam_root;
	return detect_roots(NULL, NULL, roots, 1, result);
}

static void at(char *path, size_t size, const char *name)
{
	snprintf(path, size, "%s/%s", scratch, name);
}

/* ---- the VDF reader alone */

struct collected
{
	int pairs, blocks, deepest;
	char last_key[256], last_value[1024];
};

static int collect(void *opaque, const char *const *keys, int depth, const char *key, const char *value)
{
	struct collected *c = opaque;

	(void)keys;
	if (value)
	{
		c->pairs++;
		snprintf(c->last_key, sizeof(c->last_key), "%s", key);
		snprintf(c->last_value, sizeof(c->last_value), "%s", value);
	}
	else
	{
		c->blocks++;
	}
	if (depth > c->deepest)
		c->deepest = depth;
	return 1;
}

static int parse(const char *text, struct collected *c)
{
	memset(c, 0, sizeof(*c));
	return ae_vdf_parse(text, strlen(text), collect, c);
}

static void vdf_tests(void)
{
	struct collected c;
	char *big;
	size_t index;

	CHECK(parse("\"a\" { \"b\" \"c\" // comment \"x\" \"y\"\n \"d\" { \"e\" \"f\\\\g\\\"h\" } }", &c));
	CHECK(c.pairs == 2 && c.blocks == 2 && c.deepest == 2);
	CHECK(!strcmp(c.last_key, "e") && !strcmp(c.last_value, "f\\g\"h"));
	/* (bare tokens, as some Steam files have) */
	CHECK(parse("AppState { appid 976730 }", &c) && c.pairs == 1 && !strcmp(c.last_value, "976730"));
	/* (an empty text reads as nothing) */
	CHECK(parse("", &c) && c.pairs == 0);
	/* hostile: a string left open, a block left open, a stray brace, a key with no value */
	CHECK(!parse("\"a\" { \"b\" \"c", &c));
	CHECK(!parse("\"a\" { \"b\" \"c\"", &c));
	CHECK(!parse("}", &c));
	CHECK(!parse("\"a\" }", &c));
	CHECK(!parse("{", &c));
	/* (a control character in a bare token) */
	CHECK(!parse("a\001 b", &c));
	/* hostile: deep nesting (stops at AE_MCC_MAXIMUM_VDF_DEPTH, no stack used per level) */
	big = malloc(200000);
	CHECK(big != NULL);
	if (big)
	{
		for (index = 0; index < 10000; index++)
			memcpy(big + index * 6, "\"k\" { ", 6);
		big[10000 * 6] = 0;
		CHECK(!parse(big, &c));
		CHECK(c.deepest < AE_MCC_MAXIMUM_VDF_DEPTH);
		/* a token longer than a path */
		memset(big, 'x', 5000);
		big[0] = '"';
		big[4999] = '"';
		big[5000] = 0;
		strcat(big, " \"v\"");
		CHECK(!parse(big, &c));
		memset(big, 'x', 5000);
		big[5000] = 0;
		strcat(big, " v");
		CHECK(!parse(big, &c));
		free(big);
	}
	/* (a NUL inside: the text ends there, the open block is broken) */
	{
		static const char text[] = "\"a\" { \"b\" \"c\" \0 }";

		memset(&c, 0, sizeof(c));
		CHECK(!ae_vdf_parse(text, sizeof(text) - 1, collect, &c));
	}
}

static void library_tests(void)
{
	struct ae_mcc_library libraries[4];
	long flags;
	char dir[256];
	int count;

	count = ae_mcc_parse_library_folders(
		"\"libraryfolders\" { \"0\" { \"path\" \"/a\" \"apps\" { \"2280\" \"1\" } } "
		"\"1\" { \"path\" \"D:\\\\Steam Library\" \"apps\" { \"976730\" \"5\" } } }", 1000, libraries, 4);
	CHECK(count == 2);
	CHECK(!strcmp(libraries[0].path, "/a") && !libraries[0].lists_mcc);
	CHECK(!strcmp(libraries[1].path, "D:\\Steam Library") && libraries[1].lists_mcc);
	/* the older format */
	count = ae_mcc_parse_library_folders(
		"\"LibraryFolders\" { \"TimeNextStatsReport\" \"1\" \"ContentStatsID\" \"2\" \"1\" \"E:\\\\Games\" }",
		1000, libraries, 4);
	CHECK(count == 1 && !strcmp(libraries[0].path, "E:\\Games") && !libraries[0].lists_mcc);
	/* at most the maximum */
	count = ae_mcc_parse_library_folders(
		"\"libraryfolders\" { \"0\" { \"path\" \"/a\" } \"1\" { \"path\" \"/b\" } \"2\" { \"path\" \"/c\" } }", 1000,
		libraries, 2);
	CHECK(count == 2);
	/* a broken file keeps what it read before the break */
	count = ae_mcc_parse_library_folders("\"libraryfolders\" { \"0\" { \"path\" \"/a\" } \"1\" { \"path\" \"/b",
		1000, libraries, 4);
	CHECK(count == 1 && !strcmp(libraries[0].path, "/a"));
	/* the manifest */
	CHECK(ae_mcc_parse_manifest("\"AppState\" { \"StateFlags\" \"4\" \"installdir\" \"Halo MCC\" }", 1000, &flags, dir,
		sizeof(dir)));
	CHECK(flags == 4 && !strcmp(dir, "Halo MCC"));
	/* (an installdir naming a path is not used) */
	CHECK(ae_mcc_parse_manifest("\"AppState\" { \"StateFlags\" \"1026\" \"installdir\" \"../../etc\" }", 1000, &flags,
		dir, sizeof(dir)));
	CHECK(flags == 1026 && !dir[0]);
	CHECK(!ae_mcc_parse_manifest("\"Other\" { }", 1000, &flags, dir, sizeof(dir)) && flags == -1);
	CHECK(!ae_mcc_parse_manifest("\"AppState\" { \"StateFlags\" \"4\"", 1000, &flags, dir, sizeof(dir)));
}

static void small_tests(void)
{
	char roots[8][AE_MCC_PATH_SIZE];
	int count;

	CHECK(ae_mcc_use_from_text("yes") == AE_MCC_USE_YES && ae_mcc_use_from_text("NO") == AE_MCC_USE_NO);
	CHECK(ae_mcc_use_from_text("ask") == AE_MCC_USE_ASK && ae_mcc_use_from_text("maybe") == AE_MCC_USE_ASK &&
		ae_mcc_use_from_text(NULL) == AE_MCC_USE_ASK);
	CHECK(ae_mcc_path_storable("/a b/c") && !ae_mcc_path_storable("") && !ae_mcc_path_storable("a\nb"));
	CHECK(ae_mcc_resource_type("bitmaps") == 0 && ae_mcc_resource_type("SOUNDS") == 1 &&
		ae_mcc_resource_type("loc") == 2 && ae_mcc_resource_type("ui") == -1 && ae_mcc_resource_type(NULL) == -1);
	count = ae_mcc_linux_steam_roots("/x/data", "/users/p", roots, 8);
	CHECK(count == 6);
	CHECK(!strcmp(roots[0], "/x/data/Steam") && !strcmp(roots[1], "/users/p/.local/share/Steam"));
	CHECK(!strcmp(roots[2], "/users/p/.steam/steam") && !strcmp(roots[3], "/users/p/.steam/root"));
	CHECK(!strcmp(roots[4], "/users/p/.var/app/com.valvesoftware.Steam/.local/share/Steam"));
	CHECK(!strcmp(roots[5], "/users/p/snap/steam/common/.local/share/Steam"));
	/* (XDG_DATA_HOME at its default is looked in once; relative values are not used) */
	CHECK(ae_mcc_linux_steam_roots("/users/p/.local/share", "/users/p", roots, 8) == 5);
	CHECK(ae_mcc_linux_steam_roots("relative", NULL, roots, 8) == 0);
}

int main(void)
{
	struct ae_mcc_result result;
	char steam[AE_MCC_PATH_SIZE], install[AE_MCC_PATH_SIZE], library[AE_MCC_PATH_SIZE], path[AE_MCC_PATH_SIZE];
	const char *scratch_env = getenv("AE_TEST_SCRATCH");
	char reason[512];

	snprintf(scratch, sizeof(scratch), "%s/ae_mcc", scratch_env && *scratch_env ? scratch_env : "/tmp");
	make_folders(scratch);
	vdf_tests();
	library_tests();
	small_tests();

	/* (a) a normal install: the Steam root's own library */
	at(steam, sizeof(steam), "a/Steam");
	{
		const char *paths[] = { steam };
		const int lists[] = { 1 };

		library_folders(steam, paths, lists, 1);
	}
	manifest(steam, 4, AE_MCC_DEFAULT_INSTALL_DIR);
	snprintf(install, sizeof(install), "%s/steamapps/common/%s", steam, AE_MCC_DEFAULT_INSTALL_DIR);
	make_install(install, 0);
	CHECK(detect_one(steam, &result) == AE_MCC_FOUND);
	CHECK(!strcmp(result.root, install) && result.source == AE_MCC_SOURCE_STEAM);
	CHECK(ae_mcc_root_valid(install, reason, sizeof(reason)));

	/* (b) a second library holds it (the root's lists other games) */
	at(steam, sizeof(steam), "b/Steam");
	at(library, sizeof(library), "b/SteamLibrary");
	{
		const char *paths[] = { steam, library };
		const int lists[] = { 0, 1 };

		library_folders(steam, paths, lists, 2);
	}
	manifest(library, 4, "Halo MCC Elsewhere");
	snprintf(install, sizeof(install), "%s/steamapps/common/Halo MCC Elsewhere", library);
	make_install(install, 0);
	CHECK(detect_one(steam, &result) == AE_MCC_FOUND && !strcmp(result.root, install));

	/* (c) listed in libraryfolders.vdf, but no manifest and no files (the owner's laptop) */
	at(steam, sizeof(steam), "c/Steam");
	{
		const char *paths[] = { steam };
		const int lists[] = { 1 };

		library_folders(steam, paths, lists, 1);
	}
	CHECK(detect_one(steam, &result) == AE_MCC_INCOMPLETE);
	CHECK(strstr(result.reason, "lists MCC") != NULL);

	/* (d) a manifest of an update under way (StateFlags 6), the files there */
	at(steam, sizeof(steam), "d/Steam");
	{
		const char *paths[] = { steam };
		const int lists[] = { 1 };

		library_folders(steam, paths, lists, 1);
	}
	manifest(steam, 6, AE_MCC_DEFAULT_INSTALL_DIR);
	snprintf(install, sizeof(install), "%s/steamapps/common/%s", steam, AE_MCC_DEFAULT_INSTALL_DIR);
	make_install(install, 0);
	CHECK(detect_one(steam, &result) == AE_MCC_INCOMPLETE && strstr(result.reason, "StateFlags 6"));

	/* (e) an empty shell: Engine/ and MCC/Binaries/Win64 only (listed: incomplete; unlisted, its folder seen:
	incomplete; with nothing to tell folders: not found) */
	at(steam, sizeof(steam), "e/Steam");
	snprintf(install, sizeof(install), "%s/steamapps/common/%s", steam, AE_MCC_DEFAULT_INSTALL_DIR);
	snprintf(path, sizeof(path), "%s/Engine/Binaries/Win64", install);
	make_folders(path);
	snprintf(path, sizeof(path), "%s/MCC/Binaries/Win64", install);
	make_folders(path);
	{
		const char *paths[] = { steam };
		const int lists[] = { 0 };

		library_folders(steam, paths, lists, 1);
	}
	CHECK(detect_one(steam, &result) == AE_MCC_INCOMPLETE && strstr(result.reason, "has a folder of"));
	{
		struct ae_mcc_inputs inputs;
		const char *roots[] = { steam };

		memset(&inputs, 0, sizeof(inputs));
		inputs.steam_roots = roots;
		inputs.steam_root_count = 1;
		CHECK(ae_mcc_detect(&inputs, &result) == AE_MCC_NOT_FOUND);
	}

	/* (f) Flatpak's Steam, found through the Linux roots */
	{
		char home[AE_MCC_PATH_SIZE], roots[8][AE_MCC_PATH_SIZE];
		const char *root_pointers[8];
		int count, index;

		at(home, sizeof(home), "f/home");
		snprintf(steam, sizeof(steam), "%s/.var/app/com.valvesoftware.Steam/.local/share/Steam", home);
		{
			const char *paths[] = { steam };
			const int lists[] = { 1 };

			library_folders(steam, paths, lists, 1);
		}
		manifest(steam, 4, AE_MCC_DEFAULT_INSTALL_DIR);
		snprintf(install, sizeof(install), "%s/steamapps/common/%s", steam, AE_MCC_DEFAULT_INSTALL_DIR);
		make_install(install, 0);
		count = ae_mcc_linux_steam_roots(NULL, home, roots, 8);
		for (index = 0; index < count; index++)
			root_pointers[index] = roots[index];
		CHECK(detect_roots(NULL, NULL, root_pointers, count, &result) == AE_MCC_FOUND && !strcmp(result.root, install));
	}

	/* (g) a library path with spaces, quotes and a backslash (escaped in the file) */
	at(steam, sizeof(steam), "g/Steam");
	at(library, sizeof(library), "g/Games \"Lib\" back\\slash");
	{
		const char *paths[] = { steam, library };
		const int lists[] = { 0, 1 };

		library_folders(steam, paths, lists, 2);
	}
	manifest(library, 4, AE_MCC_DEFAULT_INSTALL_DIR);
	snprintf(install, sizeof(install), "%s/steamapps/common/%s", library, AE_MCC_DEFAULT_INSTALL_DIR);
	make_install(install, 0);
	CHECK(detect_one(steam, &result) == AE_MCC_FOUND && !strcmp(result.root, install));

	/* (h) a bitmaps.map cut short (its table past its end), and an empty one */
	at(steam, sizeof(steam), "h/Steam");
	{
		const char *paths[] = { steam };
		const int lists[] = { 1 };

		library_folders(steam, paths, lists, 1);
	}
	manifest(steam, 4, AE_MCC_DEFAULT_INSTALL_DIR);
	snprintf(install, sizeof(install), "%s/steamapps/common/%s", steam, AE_MCC_DEFAULT_INSTALL_DIR);
	make_install(install, 1);
	CHECK(detect_one(steam, &result) == AE_MCC_INCOMPLETE && strstr(result.reason, "bitmaps"));
	ae_mcc_resource_path(install, "bitmaps", path, sizeof(path));
	write_file(path, "", 0);
	CHECK(!ae_mcc_resource_file_valid(path, 0, reason, sizeof(reason)) && strstr(reason, "empty"));
	/* (a sounds.map where bitmaps.map goes: not of its type) */
	ae_mcc_resource_path(install, "sounds", path, sizeof(path));
	CHECK(ae_mcc_resource_file_valid(path, 1, reason, sizeof(reason)));
	CHECK(!ae_mcc_resource_file_valid(path, 0, reason, sizeof(reason)));

	/* (i) a moved library: game.mcc_path's folder gone, Steam's libraries have it now */
	at(steam, sizeof(steam), "a/Steam");
	{
		const char *roots[] = { steam };
		char gone[AE_MCC_PATH_SIZE];

		at(gone, sizeof(gone), "i/old/Halo The Master Chief Collection");
		snprintf(install, sizeof(install), "%s/steamapps/common/%s", steam, AE_MCC_DEFAULT_INSTALL_DIR);
		CHECK(detect_roots(gone, NULL, roots, 1, &result) == AE_MCC_FOUND && !strcmp(result.root, install) &&
			result.source == AE_MCC_SOURCE_STEAM);
		/* (and game.mcc_path first, the environment's next, when they hold the files) */
		at(path, sizeof(path), "b/SteamLibrary/steamapps/common/Halo MCC Elsewhere");
		CHECK(detect_roots(path, NULL, roots, 1, &result) == AE_MCC_FOUND && result.source == AE_MCC_SOURCE_CONFIG &&
			!strcmp(result.root, path));
		CHECK(detect_roots(gone, path, roots, 1, &result) == AE_MCC_FOUND &&
			result.source == AE_MCC_SOURCE_ENVIRONMENT);
		CHECK(detect_roots(gone, NULL, NULL, 0, &result) == AE_MCC_NOT_FOUND && strstr(result.reason, "no longer"));
	}

	/* hostile files: a huge libraryfolders.vdf (over the limit: not read, the root still looked at), a deeply
	nested one, one with a string left open */
	at(steam, sizeof(steam), "hostile/Steam");
	{
		size_t size = AE_MCC_MAXIMUM_VDF_SIZE + 10, index;
		char *text = malloc(size);

		CHECK(text != NULL);
		if (text)
		{
			memset(text, ' ', size);
			snprintf(path, sizeof(path), "%s/steamapps/libraryfolders.vdf", steam);
			write_file(path, text, size);
			CHECK(detect_one(steam, &result) == AE_MCC_NOT_FOUND);
			for (index = 0; index < 5000; index++)
				memcpy(text + index * 6, "\"0\" { ", 6);
			text[5000 * 6] = 0;
			write_text(path, text);
			CHECK(detect_one(steam, &result) == AE_MCC_NOT_FOUND);
			write_text(path, "\"libraryfolders\" { \"0\" { \"path\" \"/nowhere\" \"apps\" { \"976730");
			CHECK(detect_one(steam, &result) == AE_MCC_NOT_FOUND);
			free(text);
		}
		/* (a manifest that is garbage: the default folder, no files: incomplete, never a crash) */
		snprintf(path, sizeof(path), "%s/steamapps/appmanifest_976730.acf", steam);
		write_text(path, "\"AppState\" { \"StateFlags\" \"4\" \"installdir\" \"");
		CHECK(detect_one(steam, &result) == AE_MCC_INCOMPLETE);
	}
	/* a Steam root that is not there */
	at(steam, sizeof(steam), "nothing/Steam");
	CHECK(detect_one(steam, &result) == AE_MCC_NOT_FOUND && !result.root[0]);

	if (failures)
	{
		printf("%d failures\n", failures);
		return 1;
	}
	printf("ae_mcc: all passed\n");
	return 0;
}
