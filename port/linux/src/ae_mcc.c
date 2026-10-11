/*
AE_MCC.C

Finding Halo: The Master Chief Collection's Custom Edition resource maps
(ae_mcc.h): Steam's libraries (libraryfolders.vdf), MCC's manifest
(appmanifest_976730.acf) and the three files' first words. Pure C and stdio,
no game or SDL: the unit tests (port/linux/tests/ae_mcc_test.c) build it alone
over fake Steam trees.

Steam's files are read as untrusted text: at most AE_MCC_MAXIMUM_VDF_SIZE
bytes, every token and block depth bounded, a broken file read as no
libraries. The detection only opens files to read them.
*/

#include "ae_mcc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------- constants */

enum
{
	/* a resource map's header: its type, the offsets of its paths and of
	its resources' table, the count of resources (ce_resources.c) */
	RESOURCE_HEADER_WORDS = 4,
	RESOURCE_ENTRY_SIZE = 12,
	RESOURCE_MAXIMUM_ELEMENTS = 0x10000,
	/* Steam's StateFlags, a bit field: fully installed, and the bits that
	say its files are missing or changing now (files missing, files corrupt,
	update running, paused or started, uninstalling, validating,
	downloading, staging, committing). An update only queued (2, with 4: 6)
	leaves the files whole */
	STEAM_STATE_FULLY_INSTALLED = 4,
	STEAM_STATE_FILES_CHANGING = 32 | 128 | 256 | 512 | 1024 | 2048 | 131072 | 1048576 | 2097152 | 4194304,
};

/* (Windows' folder names are case-blind, with either separator: ae_mcc_same_folder) */
#ifdef _WIN32
#define HOST_IS_WINDOWS 1
#else
#define HOST_IS_WINDOWS 0
#endif

static const char *const resource_names[] = { "bitmaps", "sounds", "loc" };

/* ---------- small helpers */

static void copy(char *to, size_t size, const char *from)
{
	if (size)
		snprintf(to, size, "%s", from ? from : "");
}

static int lower(int character)
{
	return character >= 'A' && character <= 'Z' ? character + ('a' - 'A') : character;
}

static int same_text(const char *a, const char *b)
{
	for (; *a && *b; a++, b++)
	{
		if (lower((unsigned char)*a) != lower((unsigned char)*b))
			return 0;
	}
	return *a == *b;
}

static void join(char *path, size_t size, const char *folder, const char *name)
{
	size_t length = strlen(folder);
	int separator = length && (folder[length - 1] == '/' || folder[length - 1] == '\\');

	snprintf(path, size, "%s%s%s", folder, separator ? "" : "/", name);
}

/* a whole small file, terminated (NULL if it cannot be read, is empty or is
over the limit) */
static char *file_read(const char *path, size_t limit, size_t *length)
{
	FILE *file = fopen(path, "rb");
	char *text;
	long size;

	*length = 0;
	if (!file)
		return NULL;
	if (fseek(file, 0, SEEK_END) != 0 || (size = ftell(file)) <= 0 || (unsigned long)size > limit ||
		fseek(file, 0, SEEK_SET) != 0)
	{
		fclose(file);
		return NULL;
	}
	text = malloc((size_t)size + 1);
	if (text && fread(text, 1, (size_t)size, file) != (size_t)size)
	{
		free(text);
		text = NULL;
	}
	fclose(file);
	if (text)
	{
		text[size] = 0;
		*length = (size_t)size;
	}
	return text;
}

static unsigned long little_endian(const unsigned char *bytes)
{
	return (unsigned long)bytes[0] | (unsigned long)bytes[1] << 8 | (unsigned long)bytes[2] << 16 |
		(unsigned long)bytes[3] << 24;
}

/* ---------- the resource maps */

const char *ae_mcc_resource_name(int type)
{
	return type >= 0 && type < (int)(sizeof(resource_names) / sizeof(resource_names[0])) ? resource_names[type] : NULL;
}

int ae_mcc_resource_type(const char *name)
{
	int type;

	for (type = 0; name && ae_mcc_resource_name(type); type++)
	{
		if (same_text(name, ae_mcc_resource_name(type)))
			return type;
	}
	return -1;
}

void ae_mcc_resource_path(const char *root, const char *name, char *path, size_t size)
{
	char folder[AE_MCC_PATH_SIZE];

	join(folder, sizeof(folder), root, "halo1/maps/custom_edition");
	snprintf(path, size, "%s/%s.map", folder, name);
}

int ae_mcc_resource_file_valid(const char *path, int type, char *reason, size_t reason_size)
{
	unsigned char bytes[RESOURCE_HEADER_WORDS * 4];
	unsigned long header[RESOURCE_HEADER_WORDS];
	FILE *file = fopen(path, "rb");
	long size;
	int index;

	if (!file)
	{
		snprintf(reason, reason_size, "%.300s cannot be read", path);
		return 0;
	}
	/* (a file of 2 GB or more is none: ftell fails on it where long is 32 bits, and the engine refuses it) */
	if (fseek(file, 0, SEEK_END) != 0 || (size = ftell(file)) < 0 || fseek(file, 0, SEEK_SET) != 0)
	{
		fclose(file);
		snprintf(reason, reason_size, "%.300s: its size cannot be read", path);
		return 0;
	}
	if ((unsigned long)size < sizeof(bytes) || fread(bytes, 1, sizeof(bytes), file) != sizeof(bytes))
	{
		fclose(file);
		snprintf(reason, reason_size, "%.300s is empty or cut short (%ld bytes)", path, size);
		return 0;
	}
	fclose(file);
	for (index = 0; index < RESOURCE_HEADER_WORDS; index++)
		header[index] = little_endian(bytes + index * 4);
	/* (ce_resources.c's ce_resource_map_open's check, mirrored: its type, at most 0x10000 resources, its paths
	before its table, the table in the file, under 2 GB. That one is the engine's and the real gate when the file
	is opened; if its rules change, change these and ae_mcc_test.c's header_tests, which lists the verdicts) */
	if (header[0] != (unsigned long)(type + 1) || header[3] > RESOURCE_MAXIMUM_ELEMENTS || header[1] > header[2] ||
		header[2] > (unsigned long)size || header[3] * RESOURCE_ENTRY_SIZE > (unsigned long)size - header[2] ||
		(unsigned long)size > 0x7fffffffUL)
	{
		snprintf(reason, reason_size, "%.300s is not Custom Edition's %s resource map (or is cut short)", path,
			ae_mcc_resource_name(type));
		return 0;
	}
	return 1;
}

int ae_mcc_root_valid(const char *root, char *reason, size_t reason_size)
{
	int type;

	if (!root || !root[0])
	{
		copy(reason, reason_size, "no folder");
		return 0;
	}
	for (type = 0; ae_mcc_resource_name(type); type++)
	{
		char path[AE_MCC_PATH_SIZE];

		ae_mcc_resource_path(root, ae_mcc_resource_name(type), path, sizeof(path));
		if (!ae_mcc_resource_file_valid(path, type, reason, reason_size))
			return 0;
	}
	return 1;
}

/* ---------- VDF */

struct vdf_reader
{
	const char *text;
	size_t length;
	size_t at;
};

enum
{
	VDF_END,
	VDF_STRING,
	VDF_OPEN,
	VDF_CLOSE,
	VDF_ERROR,
};

/* the next token: a string (quoted or bare) into token, a brace, the end, or
an error (a string left open or too long, a stray character) */
static int vdf_token(struct vdf_reader *reader, char *token, size_t size)
{
	size_t length = 0;

	for (;;)
	{
		while (reader->at < reader->length && (reader->text[reader->at] == ' ' || reader->text[reader->at] == '\t' ||
			reader->text[reader->at] == '\r' || reader->text[reader->at] == '\n'))
		{
			reader->at++;
		}
		if (reader->at + 1 < reader->length && reader->text[reader->at] == '/' && reader->text[reader->at + 1] == '/')
		{
			while (reader->at < reader->length && reader->text[reader->at] != '\n')
				reader->at++;
			continue;
		}
		break;
	}
	if (reader->at >= reader->length || !reader->text[reader->at])
		return VDF_END;
	switch (reader->text[reader->at])
	{
	case '{':
		reader->at++;
		return VDF_OPEN;
	case '}':
		reader->at++;
		return VDF_CLOSE;
	case '"':
		reader->at++;
		for (;;)
		{
			char character;

			if (reader->at >= reader->length || !reader->text[reader->at])
				return VDF_ERROR;
			character = reader->text[reader->at++];
			if (character == '"')
				break;
			if (character == '\\' && reader->at < reader->length)
			{
				char escaped = reader->text[reader->at++];

				if (!escaped)
					return VDF_ERROR;
				character = escaped == 'n' ? '\n' : escaped == 't' ? '\t' : escaped;
			}
			if (length + 1 >= size)
				return VDF_ERROR;
			token[length++] = character;
		}
		token[length] = 0;
		return VDF_STRING;
	default:
		/* (a bare token: up to white space, a brace or a quote; #include and the like read as tokens, never acted on) */
		while (reader->at < reader->length && reader->text[reader->at] && !strchr(" \t\r\n{}\"", reader->text[reader->at]))
		{
			unsigned char character = (unsigned char)reader->text[reader->at];

			if (character < 0x20)
				return VDF_ERROR;
			if (length + 1 >= size)
				return VDF_ERROR;
			token[length++] = (char)character;
			reader->at++;
		}
		if (!length)
			return VDF_ERROR;
		token[length] = 0;
		return VDF_STRING;
	}
}

int ae_vdf_parse(const char *text, size_t length, ae_vdf_visit visit, void *context)
{
	struct vdf_reader reader = { text, length, 0 };
	/* (the blocks' keys, and the pair's key and value) */
	static const size_t token_size = AE_MCC_PATH_SIZE;
	char *keys_storage = malloc(AE_MCC_MAXIMUM_VDF_DEPTH * token_size);
	char *key = malloc(token_size), *value = malloc(token_size);
	const char *keys[AE_MCC_MAXIMUM_VDF_DEPTH];
	int depth = 0, result = 0;

	if (!text || !keys_storage || !key || !value)
		goto done;
	for (;;)
	{
		int kind = vdf_token(&reader, key, token_size);

		if (kind == VDF_END)
		{
			/* (a block left open: broken) */
			result = depth == 0;
			break;
		}
		if (kind == VDF_CLOSE)
		{
			if (!depth)
				break;
			depth--;
			continue;
		}
		if (kind != VDF_STRING)
			break;
		kind = vdf_token(&reader, value, token_size);
		if (kind == VDF_STRING)
		{
			if (visit && !visit(context, keys, depth, key, value))
			{
				result = 1;
				break;
			}
			continue;
		}
		if (kind != VDF_OPEN || depth >= AE_MCC_MAXIMUM_VDF_DEPTH)
			break;
		if (visit && !visit(context, keys, depth, key, NULL))
		{
			result = 1;
			break;
		}
		memcpy(keys_storage + depth * token_size, key, strlen(key) + 1);
		keys[depth] = keys_storage + depth * token_size;
		depth++;
	}
done:
	free(keys_storage);
	free(key);
	free(value);
	return result;
}

/* ---------- libraryfolders.vdf */

struct library_context
{
	struct ae_mcc_library *libraries;
	int maximum;
	int count;
	/* the library block being read (its key at depth 1), its index (-1: none yet) */
	char block[64];
	int current;
};

static int all_digits(const char *text)
{
	if (!*text)
		return 0;
	for (; *text; text++)
	{
		if (*text < '0' || *text > '9')
			return 0;
	}
	return 1;
}

static int library_add(struct library_context *context, const char *path)
{
	int index;

	/* (room left for steamapps/common/<installdir>/halo1/... below it) */
	if (strlen(path) >= AE_MCC_PATH_SIZE - 400)
		return -1;
	for (index = 0; index < context->count; index++)
	{
		if (ae_mcc_same_folder(context->libraries[index].path, path, HOST_IS_WINDOWS))
			return index;
	}
	if (context->count >= context->maximum)
		return -1;
	copy(context->libraries[context->count].path, sizeof(context->libraries[context->count].path), path);
	context->libraries[context->count].lists_mcc = 0;
	return context->count++;
}

static int library_visit(void *opaque, const char *const *keys, int depth, const char *key, const char *value)
{
	struct library_context *context = opaque;

	/* ("libraryfolders" { "0" { "path" "..." "apps" { "976730" "<size>" } } }, or the older
	"LibraryFolders" { "1" "<path>" }) */
	if (depth == 0)
		return 1;
	if (!same_text(keys[0], "libraryfolders"))
		return 1;
	if (depth == 1)
	{
		if (!value && all_digits(key))
		{
			copy(context->block, sizeof(context->block), key);
			context->current = -1;
		}
		else if (value && all_digits(key) && value[0])
		{
			library_add(context, value);
		}
		return 1;
	}
	if (!all_digits(keys[1]) || strcmp(keys[1], context->block))
		return 1;
	if (depth == 2 && value && same_text(key, "path") && value[0])
	{
		int index = library_add(context, value);

		/* (apps listed before the path, as no Steam writes them: kept for this block) */
		if (index >= 0 && context->current == -2)
			context->libraries[index].lists_mcc = 1;
		context->current = index;
	}
	else if (depth == 3 && same_text(keys[2], "apps") && value && !strcmp(key, AE_MCC_STEAM_APP))
	{
		if (context->current >= 0)
			context->libraries[context->current].lists_mcc = 1;
		else
			context->current = -2;
	}
	return 1;
}

int ae_mcc_parse_library_folders(const char *text, size_t length, struct ae_mcc_library *libraries, int maximum)
{
	struct library_context context;

	memset(&context, 0, sizeof(context));
	context.libraries = libraries;
	context.maximum = maximum;
	context.current = -1;
	/* (a broken file still gives the libraries read before the break) */
	ae_vdf_parse(text, length, library_visit, &context);
	return context.count;
}

/* ---------- appmanifest_976730.acf */

struct manifest_context
{
	long state_flags;
	char *install_dir;
	size_t size;
	int app_state;
};

static int manifest_visit(void *opaque, const char *const *keys, int depth, const char *key, const char *value)
{
	struct manifest_context *context = opaque;

	if (depth == 0 && !value && same_text(key, "AppState"))
		context->app_state = 1;
	if (depth != 1 || !value || !same_text(keys[0], "AppState"))
		return 1;
	if (same_text(key, "StateFlags"))
	{
		char *end;
		long flags = strtol(value, &end, 10);

		context->state_flags = *end || end == value ? -1 : flags;
	}
	else if (same_text(key, "installdir"))
	{
		/* (a folder's name under steamapps/common: never a path, nor . or ..) */
		if (value[0] && !strpbrk(value, "/\\:") && strcmp(value, ".") && strcmp(value, ".."))
			copy(context->install_dir, context->size, value);
	}
	return 1;
}

int ae_mcc_parse_manifest(const char *text, size_t length, long *state_flags, char *install_dir, size_t size)
{
	struct manifest_context context = { -1, install_dir, size, 0 };

	copy(install_dir, size, "");
	if (!ae_vdf_parse(text, length, manifest_visit, &context) || !context.app_state)
	{
		*state_flags = -1;
		copy(install_dir, size, "");
		return 0;
	}
	*state_flags = context.state_flags;
	return 1;
}

/* ---------- the Steam roots */

int ae_mcc_linux_steam_roots(const char *xdg_data_home, const char *home, char (*roots)[AE_MCC_PATH_SIZE],
	int maximum)
{
	static const char *const below_home[] =
	{
		".local/share/Steam",
		".steam/steam",
		".steam/root",
		/* (Flatpak's, Snap's) */
		".var/app/com.valvesoftware.Steam/.local/share/Steam",
		"snap/steam/common/.local/share/Steam",
	};
	int count = 0;
	size_t index;

	if (xdg_data_home && xdg_data_home[0] == '/' && count < maximum)
		join(roots[count++], AE_MCC_PATH_SIZE, xdg_data_home, "Steam");
	for (index = 0; home && home[0] == '/' && index < sizeof(below_home) / sizeof(below_home[0]); index++)
	{
		char path[AE_MCC_PATH_SIZE];
		int seen, other;

		join(path, sizeof(path), home, below_home[index]);
		for (seen = 0, other = 0; other < count; other++)
			seen |= !strcmp(roots[other], path);
		if (!seen && count < maximum)
			copy(roots[count++], AE_MCC_PATH_SIZE, path);
	}
	return count;
}

/* ---------- the detection */

int ae_mcc_state_flags_installed(long state_flags)
{
	return state_flags >= 0 && (state_flags & STEAM_STATE_FULLY_INSTALLED) &&
		!(state_flags & STEAM_STATE_FILES_CHANGING);
}

int ae_mcc_same_folder(const char *a, const char *b, int windows)
{
	if (!windows)
		return !strcmp(a, b);
	for (;; a++, b++)
	{
		int x = *a == '/' ? '\\' : lower((unsigned char)*a);
		int y = *b == '/' ? '\\' : lower((unsigned char)*b);

		/* (a trailing separator on either is no difference) */
		if (!x || !y)
			return (!x && (!y || (y == '\\' && !b[1]))) || (!y && x == '\\' && !a[1]);
		if (x != y)
			return 0;
	}
}

int ae_mcc_virtual_path_type(const char *xbox_path)
{
	static const char prefix[] = "mcc:";
	char name[16];
	const char *rest;
	size_t length, index;

	if (!xbox_path)
		return AE_MCC_VIRTUAL_NOT;
	for (index = 0; prefix[index]; index++)
	{
		if (lower((unsigned char)xbox_path[index]) != prefix[index])
			return AE_MCC_VIRTUAL_NOT;
	}
	if (xbox_path[index] != '\\' && xbox_path[index] != '/')
		return AE_MCC_VIRTUAL_NOT;
	rest = xbox_path + index + 1;
	length = strlen(rest);
	/* (only "<name>.map" of the three names: no folder, no .., nothing else of MCC's is ever named) */
	if (length < 5 || length - 4 >= sizeof(name) || !same_text(rest + length - 4, ".map"))
		return AE_MCC_VIRTUAL_INVALID;
	memcpy(name, rest, length - 4);
	name[length - 4] = 0;
	index = (size_t)ae_mcc_resource_type(name);
	return (int)index >= 0 ? (int)index : AE_MCC_VIRTUAL_INVALID;
}

int ae_mcc_use_from_text(const char *text)
{
	if (text && same_text(text, "yes"))
		return AE_MCC_USE_YES;
	if (text && same_text(text, "no"))
		return AE_MCC_USE_NO;
	return AE_MCC_USE_ASK;
}

int ae_mcc_path_storable(const char *path)
{
	const unsigned char *character;

	if (!path || !path[0] || strlen(path) >= 256)
		return 0;
	for (character = (const unsigned char *)path; *character; character++)
	{
		if (*character < 0x20 || *character == 0x7f)
			return 0;
	}
	return 1;
}

static void result_set(struct ae_mcc_result *result, int state, int source, const char *root, const char *reason)
{
	result->state = state;
	result->source = source;
	copy(result->root, sizeof(result->root), root);
	copy(result->reason, sizeof(result->reason), reason);
}

/* one library: its manifest, else its default folder (when it lists MCC, or
the folder is there). The state; result filled for FOUND and INCOMPLETE */
static int library_scan(const struct ae_mcc_library *library, int (*directory_exists)(const char *path),
	struct ae_mcc_result *result)
{
	char steamapps[AE_MCC_PATH_SIZE], manifest_path[AE_MCC_PATH_SIZE], common[AE_MCC_PATH_SIZE];
	char install_dir[256], root[AE_MCC_PATH_SIZE], why[384], reason[600];
	char *text;
	size_t length;
	long state_flags = -1;

	join(steamapps, sizeof(steamapps), library->path, "steamapps");
	join(manifest_path, sizeof(manifest_path), steamapps, "appmanifest_" AE_MCC_STEAM_APP ".acf");
	join(common, sizeof(common), steamapps, "common");
	copy(install_dir, sizeof(install_dir), AE_MCC_DEFAULT_INSTALL_DIR);
	text = file_read(manifest_path, AE_MCC_MAXIMUM_VDF_SIZE, &length);
	if (text)
	{
		char named[256];

		if (ae_mcc_parse_manifest(text, length, &state_flags, named, sizeof(named)) && named[0])
			copy(install_dir, sizeof(install_dir), named);
		free(text);
		join(root, sizeof(root), common, install_dir);
		if (!ae_mcc_state_flags_installed(state_flags))
		{
			snprintf(reason, sizeof(reason), "Steam's manifest %.400s says MCC is not fully installed (StateFlags %ld): "
				"being updated or downloaded", manifest_path, state_flags);
			result_set(result, AE_MCC_INCOMPLETE, AE_MCC_SOURCE_STEAM, root, reason);
			return AE_MCC_INCOMPLETE;
		}
	}
	else
	{
		join(root, sizeof(root), common, install_dir);
		/* (no manifest: only a library that lists MCC, or has its folder, has it at all) */
		if (!library->lists_mcc && !(directory_exists && directory_exists(root)))
			return AE_MCC_NOT_FOUND;
	}
	if (ae_mcc_root_valid(root, why, sizeof(why)))
	{
		snprintf(reason, sizeof(reason), "Steam library %.500s", library->path);
		result_set(result, AE_MCC_FOUND, AE_MCC_SOURCE_STEAM, root, reason);
		return AE_MCC_FOUND;
	}
	snprintf(reason, sizeof(reason), "Steam %s MCC in %.200s, but its Custom Edition files are not whole: %.300s",
		text ? "has a manifest of" : library->lists_mcc ? "lists" : "has a folder of", library->path, why);
	result_set(result, AE_MCC_INCOMPLETE, AE_MCC_SOURCE_STEAM, root, reason);
	return AE_MCC_INCOMPLETE;
}

int ae_mcc_scan_steam_root(const char *steam_root, int (*directory_exists)(const char *path),
	struct ae_mcc_result *result)
{
	struct ae_mcc_library libraries[AE_MCC_MAXIMUM_LIBRARIES];
	struct ae_mcc_result incomplete;
	char path[AE_MCC_PATH_SIZE], steamapps[AE_MCC_PATH_SIZE];
	char *text;
	size_t length;
	int count = 0, pass, index, state = AE_MCC_NOT_FOUND;

	memset(&incomplete, 0, sizeof(incomplete));
	join(steamapps, sizeof(steamapps), steam_root, "steamapps");
	join(path, sizeof(path), steamapps, "libraryfolders.vdf");
	text = file_read(path, AE_MCC_MAXIMUM_VDF_SIZE, &length);
	if (!text)
	{
		/* (older Steam kept it in config/) */
		join(path, sizeof(path), steam_root, "config/libraryfolders.vdf");
		text = file_read(path, AE_MCC_MAXIMUM_VDF_SIZE, &length);
	}
	if (text)
	{
		count = ae_mcc_parse_library_folders(text, length, libraries, AE_MCC_MAXIMUM_LIBRARIES - 1);
		free(text);
	}
	/* (the root is a library too, listed or not) */
	for (index = 0; index < count && !ae_mcc_same_folder(libraries[index].path, steam_root, HOST_IS_WINDOWS); index++)
		;
	if (index == count)
	{
		copy(libraries[count].path, sizeof(libraries[count].path), steam_root);
		libraries[count++].lists_mcc = 0;
	}
	/* (the libraries listing MCC first; then the others, whose manifest or folder may still say) */
	for (pass = 0; pass < 2; pass++)
	{
		for (index = 0; index < count; index++)
		{
			struct ae_mcc_result found;
			int library_state;

			if (libraries[index].lists_mcc != (pass == 0))
				continue;
			memset(&found, 0, sizeof(found));
			library_state = library_scan(&libraries[index], directory_exists, &found);
			if (library_state == AE_MCC_FOUND)
			{
				*result = found;
				return AE_MCC_FOUND;
			}
			if (library_state == AE_MCC_INCOMPLETE && state == AE_MCC_NOT_FOUND)
			{
				incomplete = found;
				state = AE_MCC_INCOMPLETE;
			}
		}
	}
	if (state == AE_MCC_INCOMPLETE)
		*result = incomplete;
	return state;
}

int ae_mcc_detect(const struct ae_mcc_inputs *inputs, struct ae_mcc_result *result)
{
	struct ae_mcc_result incomplete;
	char why[384], reason[700], not_found[700] = "";
	int index, state = AE_MCC_NOT_FOUND;

	memset(result, 0, sizeof(*result));
	memset(&incomplete, 0, sizeof(incomplete));
	if (inputs->config_path && inputs->config_path[0])
	{
		if (ae_mcc_root_valid(inputs->config_path, why, sizeof(why)))
		{
			result_set(result, AE_MCC_FOUND, AE_MCC_SOURCE_CONFIG, inputs->config_path, "game.mcc_path");
			return AE_MCC_FOUND;
		}
		snprintf(not_found, sizeof(not_found), "game.mcc_path %.250s no longer holds MCC's files (%.80s); ",
			inputs->config_path, why);
	}
	if (inputs->environment_path && inputs->environment_path[0])
	{
		if (ae_mcc_root_valid(inputs->environment_path, why, sizeof(why)))
		{
			result_set(result, AE_MCC_FOUND, AE_MCC_SOURCE_ENVIRONMENT, inputs->environment_path, "HALO_MCC_PATH");
			return AE_MCC_FOUND;
		}
		snprintf(not_found + strlen(not_found), sizeof(not_found) - strlen(not_found),
			"HALO_MCC_PATH %.250s has not MCC's files (%.80s); ", inputs->environment_path, why);
	}
	for (index = 0; index < inputs->steam_root_count; index++)
	{
		struct ae_mcc_result found;
		int root_state;

		if (!inputs->steam_roots[index] || !inputs->steam_roots[index][0])
			continue;
		memset(&found, 0, sizeof(found));
		root_state = ae_mcc_scan_steam_root(inputs->steam_roots[index], inputs->directory_exists, &found);
		if (root_state == AE_MCC_FOUND)
		{
			*result = found;
			return AE_MCC_FOUND;
		}
		if (root_state == AE_MCC_INCOMPLETE && state == AE_MCC_NOT_FOUND)
		{
			incomplete = found;
			state = AE_MCC_INCOMPLETE;
		}
	}
	if (state == AE_MCC_INCOMPLETE)
	{
		*result = incomplete;
		return AE_MCC_INCOMPLETE;
	}
	snprintf(reason, sizeof(reason), "%.640sno Steam library has MCC (%d Steam folders looked in)", not_found,
		inputs->steam_root_count);
	result_set(result, AE_MCC_NOT_FOUND, AE_MCC_SOURCE_NONE, "", reason);
	return AE_MCC_NOT_FOUND;
}
