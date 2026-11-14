/*
AE_WEAPON_CACHE.C

The AE weapon cache in the engine (ae_weapon_cache.h; design notes: notes/plans/2026-11-05-weapon-cache-design.md, 6.1):
read once at start, held open for the resource reads, and let go at the end. This is the START half (T2-2): the open and
its refusals, the stamp (format version, tag schema hash, and in a release build the serial and SHA-256 of the cache this
build was made for), the optional parts, the identity the join tells, and the DIRECTORY's entries by id. The attach on a
map (T2-3) and the definition lookups (T2-4) come after it and use this file's cache.

THE CACHE NEVER HALF-LOADS. Any refusal leaves no cache: one log line says why, ae_weapon_cache_refusal() has the player-
facing words (the menus show them; they say to update Arena Evolved), and the game plays with stock weapons only.
A game with no packs folder, or with game.weapon_packs off, has no cache and says nothing is wrong.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cseries.h"
#include "cseries_windows.h"
#include "errors.h"

#include "ae_pack_format.h"
#include "ae_weapon_cache.h"
#include "ae_weapon_cache_lock.h"

/* port/linux/src/port_config.c */
int config_boolean(char const *name);

/* ---------- constants */

#define PACKS_FOLDER "d:\\packs"
#define CACHE_FILE PACKS_FOLDER "\\ae_content.aecache"
#define PART_FILE_FORMAT PACKS_FOLDER "\\ae_content.%s.aecache"

/* what a player is told (design 3.6) */
#define REFUSAL_TEXT "The AE weapons file is missing or belongs to another version of Arena Evolved. Update Arena Evolved " \
	"(or reinstall its zip) to play AE weapons."

/* ---------- globals */

static struct
{
	enum ae_weapon_cache_state state;
	struct ae_pack *pack;
	char refusal[256];
	struct ae_weapon_cache_entry entries[AE_CACHE_MAXIMUM_DIRECTORY];
	unsigned int entry_count;
	unsigned int parts_present;
} cache;

/* ---------- private code */

enum
{
	_path_missing,
	_path_file,
	_path_other
};

/* what is at the path, asked without opening it (as ae_weapon_pack.c's: a pipe would wait for a writer for ever) */
static short path_kind(
	char const *path)
{
	WIN32_FILE_ATTRIBUTE_DATA data;

	if (!GetFileAttributesExA(path, GetFileExInfoStandard, &data))
		return _path_missing;
	if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || (!data.nFileSizeHigh && data.nFileSizeLow < AE_CACHE_HEADER_SIZE))
		return _path_other;
	return _path_file;
}

static void refuse(
	char const *reason)
{
	cache.state = AE_WEAPON_CACHE_STALE;
	snprintf(cache.refusal, sizeof(cache.refusal), "%s", REFUSAL_TEXT);
	error(_error_silent, "weapon cache: refused: %s; the game plays with stock weapons only", reason);
}

static void entries_make(
	void)
{
	struct ae_cache_info const *info = cache.pack->cache;
	unsigned int index, ordinal = 0;

	cache.entry_count = 0;
	for (index = 0; index < info->directory_count && cache.entry_count < AE_CACHE_MAXIMUM_DIRECTORY; index++)
	{
		struct ae_cache_directory_record const *record = &info->directory[index];
		struct ae_weapon_cache_entry *entry = &cache.entries[cache.entry_count++];

		memset(entry, 0, sizeof(*entry));
		entry->id = record->id;
		entry->kind = record->kind;
		entry->flags = record->flags;
		entry->key = ae_pack_string(cache.pack, record->key);
		entry->display = ae_pack_string(cache.pack, record->display);
		entry->behaviour = record->behaviour;
		entry->stock_fallback = record->stock_fallback;
		entry->replaces_map_own = record->replaces_map_own;
		entry->legacy_slot = record->legacy_slot;
		entry->grenade_catalogue_id = record->grenade_catalogue_id;
		if (record->kind == AE_CACHE_KIND_WEAPON)
			entry->ordinal = ordinal++;
	}
}

/* each optional part: present and the header's size and SHA-256, else a soft failure (a part the header does not call
OPTIONAL is a refusal of the whole cache) */
static int parts_check(
	void)
{
	struct ae_cache_header const *header = &cache.pack->cache->header;
	unsigned int index;

	cache.parts_present = 0;
	for (index = 0; index < header->part_count; index++)
	{
		struct ae_cache_part const *part = &header->parts[index];
		char path[128];
		struct ae_pack *opened = NULL;
		struct ae_pack_problem problem;
		int good;

		snprintf(path, sizeof(path), PART_FILE_FORMAT, part->name);
		good = path_kind(path) == _path_file && ae_cache_open(path, &opened, &problem) &&
			(opened->cache->header.flags & AE_CACHE_FLAG_PART) && opened->file_size == part->size[0] &&
			!memcmp(opened->header.sha256, part->sha256, AE_CACHE_SHA256_SIZE);
		ae_pack_close(opened);
		if (good)
		{
			cache.parts_present |= 1u << index;
			continue;
		}
		if (!(part->flags & AE_CACHE_PART_OPTIONAL))
		{
			refuse("a required part of the cache is missing or does not match");
			return 0;
		}
		error(_error_silent, "weapon cache: optional part '%s' is missing or does not match: its entries are unavailable",
			part->name);
	}
	return 1;
}

/* "weapon cache: 'ae_weapons' serial N id <16 hex> (sha256 <16 hex>...), T tags, R resources (X.Y MB), W weapons, G
grenade types, C configs" */
static void log_loaded(
	void)
{
	struct ae_cache_info const *info = cache.pack->cache;
	char hash[17];
	unsigned int index, tenths, grenades = 0;

	for (index = 0; index < AE_CACHE_ID_SIZE; index++)
		snprintf(hash + index * 2, 3, "%02x", cache.pack->header.sha256[index]);
	for (index = 0; index < info->directory_count; index++)
		if (info->directory[index].kind == AE_CACHE_KIND_GRENADE)
			grenades++;
	tenths = (cache.pack->resources_size / 1024u * 10u + 5120u) / 10240u;
	error(_error_silent, "weapon cache: serial %u id %s (%s), %u tags, %u resources (%u.%u MB), %u weapons, %u grenade "
		"types, %u configs, schema %08x%08x", info->header.cache_serial, hash,
		cache.state == AE_WEAPON_CACHE_DEV ? "dev cache, not this build's lock" : "this build's lock",
		cache.pack->tag_count, cache.pack->resource_count, tenths / 10, tenths % 10, info->weapon_entries, grenades,
		info->config_count, info->header.schema_hash[1], info->header.schema_hash[0]);
}

/* HALO_SCHEMA_TRACE=1: every schema group's own hash in the log, to compare with ae_cache_check --schema-groups */
static void schema_trace(
	unsigned int group_tag,
	char const *name,
	unsigned int low,
	unsigned int high)
{
	error(_error_silent, "schema group %c%c%c%c %s %08x%08x", (int)(group_tag >> 24) & 0xFF, (int)(group_tag >> 16) & 0xFF,
		(int)(group_tag >> 8) & 0xFF, (int)group_tag & 0xFF, name, high, low);
}

/* ---------- public code */

void ae_weapon_cache_open(
	void)
{
	static unsigned char const lock_sha256[AE_CACHE_SHA256_SIZE] = AE_WEAPON_CACHE_LOCK_SHA256;
	struct ae_pack_problem problem;
	unsigned int low, high;
	short kind;

	ae_weapon_cache_close();
	cache.state = AE_WEAPON_CACHE_ABSENT;
	cache.refusal[0] = 0;
	/* (Settings > Game Options, WEAPON PACKS: OFF: none is read) */
	if (!config_boolean("game.weapon_packs"))
		return;
	/* (no packs folder is a game without packs, and silent) */
	if (path_kind(PACKS_FOLDER) == _path_missing)
		return;
	kind = path_kind(CACHE_FILE);
	if (kind != _path_file)
	{
		cache.state = AE_WEAPON_CACHE_MISSING;
		snprintf(cache.refusal, sizeof(cache.refusal), "%s", REFUSAL_TEXT);
		error(_error_silent, "weapon cache: %s is %s; the game plays with stock weapons only", CACHE_FILE,
			kind == _path_missing ? "not there" : "no cache file (a folder, or smaller than a header)");
		return;
	}
	if (!ae_cache_open(CACHE_FILE, &cache.pack, &problem) || !ae_pack_verify_resources(cache.pack, &problem))
	{
		ae_pack_close(cache.pack);
		cache.pack = NULL;
		refuse(problem.text);
		return;
	}
	if (cache.pack->cache->header.flags & AE_CACHE_FLAG_PART)
	{
		refuse("it is an optional part, not the main cache");
		goto failed;
	}
	tag_schema_hash(&low, &high);
	if (getenv("HALO_SCHEMA_TRACE"))
		tag_schema_group_hashes(schema_trace);
	if (cache.pack->cache->header.schema_hash[0] != low || cache.pack->cache->header.schema_hash[1] != high)
	{
		char reason[128];

		snprintf(reason, sizeof(reason), "it was made for tag schema %08x%08x, this build's is %08x%08x",
			cache.pack->cache->header.schema_hash[1], cache.pack->cache->header.schema_hash[0], high, low);
		refuse(reason);
		goto failed;
	}
	cache.state = AE_WEAPON_CACHE_LOADED;
	if (AE_WEAPON_CACHE_LOCK_PRESENT)
	{
		if (cache.pack->cache->header.cache_serial != AE_WEAPON_CACHE_LOCK_SERIAL ||
			memcmp(cache.pack->header.sha256, lock_sha256, AE_CACHE_SHA256_SIZE) != 0)
		{
			char reason[160];

			snprintf(reason, sizeof(reason), "it is not the cache this build was made for (its serial %u, id %02x%02x%02x%02x; the build's serial %u)",
				cache.pack->cache->header.cache_serial, cache.pack->header.sha256[0], cache.pack->header.sha256[1],
				cache.pack->header.sha256[2], cache.pack->header.sha256[3], (unsigned int)AE_WEAPON_CACHE_LOCK_SERIAL);
			refuse(reason);
			goto failed;
		}
	}
	else
	{
		cache.state = AE_WEAPON_CACHE_DEV;
	}
	if (!parts_check())
		goto failed;
	entries_make();
	log_loaded();
	return;

failed:
	ae_pack_close(cache.pack);
	cache.pack = NULL;
	cache.entry_count = 0;
}

void ae_weapon_cache_close(
	void)
{
	ae_pack_close(cache.pack);
	memset(&cache, 0, sizeof(cache));
}

struct ae_pack *ae_weapon_cache_pack(
	void)
{
	return cache.pack;
}

enum ae_weapon_cache_state ae_weapon_cache_state(
	void)
{
	return cache.state;
}

char const *ae_weapon_cache_refusal(
	void)
{
	return cache.refusal[0] ? cache.refusal : NULL;
}

unsigned int ae_weapon_cache_serial(
	void)
{
	return cache.pack ? cache.pack->cache->header.cache_serial : 0;
}

unsigned char const *ae_weapon_cache_id(
	void)
{
	return cache.pack ? cache.pack->header.sha256 : NULL;
}

unsigned int ae_weapon_cache_schema_hash(
	unsigned int *high)
{
	if (high)
		*high = cache.pack ? cache.pack->cache->header.schema_hash[1] : 0;
	return cache.pack ? cache.pack->cache->header.schema_hash[0] : 0;
}

struct ae_weapon_cache_entry const *ae_weapon_cache_entry(
	unsigned int id)
{
	unsigned int low = 0, high = cache.entry_count;

	/* (the DIRECTORY is sorted by id: the reader checked) */
	while (low < high)
	{
		unsigned int middle = low + (high - low) / 2;

		if (cache.entries[middle].id == id)
			return &cache.entries[middle];
		if (cache.entries[middle].id < id)
			low = middle + 1;
		else
			high = middle;
	}
	return NULL;
}

struct ae_weapon_cache_entry const *ae_weapon_cache_grenade(
	unsigned int catalogue_id)
{
	return ae_weapon_cache_entry(AE_CACHE_ID_GRENADE_FIRST + catalogue_id);
}
