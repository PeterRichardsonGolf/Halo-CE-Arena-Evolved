/*
AE_WEAPON_PACK.C

port: weapon pack. The packs (ae_weapon_pack.h), read at start through the
format's reader (ae_pack_format.c), which checks the file's containers and
its SHA-256, then every resource read once (ae_pack_verify_resources), so a
pack whose resources lie is refused here and not on a map. What a pack's
tags hold is checked once they are placed on a map (tag_validate.c), not
here: nothing is placed yet.

Slot 1's pack is packs\ae_battle_rifle.aepack in the data root (d:\), or the
file debug.weapon_pack_file names (R31: an Xbox path under d:\, never a
host path: a test pack, read in its place; its hash travels like any
pack's). What is at the path is asked before it is opened: only a file at
least a header long is. A slot takes only its own pack:
another pack id in its file is refused.

The spike's DONOR code (a map's weapon taken out of another map file at
each load: game.weapon_pack) is not carried over: packs are built by
the host tool (tools/ae_pack_build) and only read here.
*/

#include <stdio.h>
#include <string.h>

#include "cseries.h"
#include "cseries_windows.h"
#include "errors.h"

#include "ae_pack_format.h"
#include "ae_weapon_pack.h"

/* ---------- constants */

#define PACKS_FOLDER "d:\\packs"

/* each slot's file in the packs folder, and the pack id it takes */
static struct
{
	char const *path;
	char const *pack_id;
} const slot_files[AE_WEAPON_PACK_SLOTS] =
{
	{ PACKS_FOLDER "\\ae_battle_rifle.aepack", "ae_battle_rifle" },
};

/* ---------- prototypes */

/* the platform layer's (port/linux/src) */
char const *config_string(char const *name);

/* ---------- globals */

static struct
{
	boolean initialized;
	struct ae_pack *packs[AE_WEAPON_PACK_SLOTS];
} weapon_pack_globals;

/* ---------- private code */

static struct ae_pack *slot_pack(
	short slot)
{
	if (slot < 1 || slot > AE_WEAPON_PACK_SLOTS)
		return NULL;
	return weapon_pack_globals.packs[slot - 1];
}

enum
{
	_path_missing,
	_path_pack_file,
	_path_other,
};

/* what is at the path, asked without opening it: opening a named pipe would wait for a writer for ever, and a
pack is a download's name away from being anything. A folder, or anything smaller than a pack's header (a pipe,
a socket and a device all report no size), is no pack file */
static short path_kind(
	char const *path)
{
	WIN32_FILE_ATTRIBUTE_DATA data;

	if (!GetFileAttributesExA(path, GetFileExInfoStandard, &data))
		return _path_missing;
	if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ||
		(!data.nFileSizeHigh && data.nFileSizeLow < AE_PACK_HEADER_SIZE))
	{
		return _path_other;
	}
	return _path_pack_file;
}

/* whether a path names another drive than d: (the data root). Another letter is a folder of the save root, which
asking about makes (the platform layer's path translation): a test pack is never looked for there */
static boolean path_leaves_data_root(
	char const *path)
{
	return path[0] && path[1] == ':' && path[0] != 'd' && path[0] != 'D';
}

/* "weapon pack: 'ae_battle_rifle' revision N (sha256 0123456789abcdef...), <own> own tags, <ext> external,
<res> resources (<MB>), weapons: battle_rifle" */
static void pack_log(
	struct ae_pack const *pack)
{
	char hash[17];
	char weapons[AE_PACK_MAXIMUM_WEAPONS * (AE_PACK_MAXIMUM_KEY + 2) + 1];
	unsigned int index, tenths;

	for (index = 0; index < 8; index++)
		snprintf(hash + index * 2, 3, "%02x", pack->header.sha256[index]);
	weapons[0] = 0;
	for (index = 0; index < pack->weapon_count; index++)
	{
		char const *key = ae_pack_string(pack, pack->weapons[index].key);
		size_t used = strlen(weapons);

		snprintf(weapons + used, sizeof(weapons) - used, "%s%s", index ? ", " : "", key ? key : "?");
	}
	/* (at most 48 MiB: times ten stays well inside 32 bits) */
	tenths = (pack->resources_size * 10u + 524288u) / 1048576u;
	error(_error_silent, "weapon pack: '%s' revision %u (sha256 %s...), %u own tags, %u external, %u resources "
		"(%u.%u MB), weapons: %s",
		pack->header.pack_id, pack->header.pack_revision, hash, pack->own_tag_count,
		pack->tag_count - pack->own_tag_count, pack->resource_count, tenths / 10, tenths % 10, weapons);
}

static void pack_refused(
	short slot,
	char const *path,
	char const *reason)
{
	error(_error_silent, "weapon pack: slot %d (%s) refused: %s; the game plays without it", slot, path, reason);
}

/* the slot's pack read, checked and logged, or refused and logged; a slot with no file in a game with no packs
folder logs nothing */
static struct ae_pack *slot_read(
	short slot,
	char const *path,
	boolean asked_for)
{
	struct ae_pack *pack = NULL;
	struct ae_pack_problem problem;
	char const *pack_id = slot_files[slot - 1].pack_id;

	/* (R32: no packs folder is a game without packs, and silent; a packs folder without the slot's file is a
	refusal. With a second slot, a folder holding only some slots' files would log one each start: decide then) */
	if (!asked_for && path_kind(PACKS_FOLDER) == _path_missing)
		return NULL;
	if (asked_for && path_leaves_data_root(path))
	{
		pack_refused(slot, path, "a test pack's path starts d:\\");
		return NULL;
	}
	switch (path_kind(path))
	{
	case _path_missing:
		pack_refused(slot, path, "there is no such file");
		return NULL;
	case _path_other:
		pack_refused(slot, path, "it is no pack file (a folder, or smaller than a pack's header)");
		return NULL;
	}
	if (!ae_pack_open(path, &pack, &problem))
	{
		pack_refused(slot, path, problem.text);
		return NULL;
	}
	if (strcmp(pack->header.pack_id, pack_id) != 0)
	{
		char reason[96];

		snprintf(reason, sizeof(reason), "it is pack '%s', the slot takes '%s'", pack->header.pack_id, pack_id);
		pack_refused(slot, path, reason);
		ae_pack_close(pack);
		return NULL;
	}
	if (!ae_pack_verify_resources(pack, &problem))
	{
		pack_refused(slot, path, problem.text);
		ae_pack_close(pack);
		return NULL;
	}
	pack_log(pack);
	return pack;
}

/* ---------- public code */

void ae_weapon_pack_initialize(
	void)
{
	char const *debug_file;
	short slot;

	if (weapon_pack_globals.initialized)
		return;
	weapon_pack_globals.initialized = TRUE;
	debug_file = config_string("debug.weapon_pack_file");
	for (slot = 1; slot <= AE_WEAPON_PACK_SLOTS; slot++)
	{
		boolean debug = slot == 1 && debug_file && debug_file[0];

		weapon_pack_globals.packs[slot - 1] = slot_read(slot, debug ? debug_file : slot_files[slot - 1].path, debug);
	}
}

void ae_weapon_pack_dispose(
	void)
{
	short slot;

	for (slot = 1; slot <= AE_WEAPON_PACK_SLOTS; slot++)
	{
		ae_pack_close(weapon_pack_globals.packs[slot - 1]);
		weapon_pack_globals.packs[slot - 1] = NULL;
	}
	weapon_pack_globals.initialized = FALSE;
}

boolean ae_weapon_pack_slot_loaded(
	short slot)
{
	return slot_pack(slot) != NULL;
}

boolean ae_weapon_pack_identity(
	short slot,
	unsigned int *revision,
	unsigned char *sha256)
{
	struct ae_pack const *pack = slot_pack(slot);

	if (!pack)
		return FALSE;
	if (revision)
		*revision = pack->header.pack_revision;
	if (sha256)
		memcpy(sha256, pack->header.sha256, AE_WEAPON_PACK_SHA256_SIZE);
	return TRUE;
}

short ae_weapon_pack_weapon_count(
	short slot)
{
	struct ae_pack const *pack = slot_pack(slot);

	return pack ? (short)pack->weapon_count : 0;
}

struct ae_pack_weapon_record const *ae_weapon_pack_weapon_entry(
	short slot,
	short weapon)
{
	struct ae_pack const *pack = slot_pack(slot);

	if (!pack || weapon < 0 || (unsigned int)weapon >= pack->weapon_count)
		return NULL;
	return &pack->weapons[weapon];
}

char const *ae_weapon_pack_string(
	short slot,
	unsigned int offset)
{
	struct ae_pack const *pack = slot_pack(slot);

	return pack ? ae_pack_string(pack, offset) : NULL;
}
