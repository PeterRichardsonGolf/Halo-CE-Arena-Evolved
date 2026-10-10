/*
AE_WEAPON_PACK.C

port: weapon pack. The packs (ae_weapon_pack.h), read at start through the
format's reader (ae_pack_format.c), which checks the file's containers and
its SHA-256, then every resource read once (ae_pack_verify_resources), so a
pack whose resources lie is refused here and not on a map. What a pack's
tags hold is checked once they are placed on a map (tag_validate.c), not
here: nothing is placed yet.

Slot 1's pack is packs\ae_battle_rifle.aepack in the data root (d:\), or the
file debug.weapon_pack_file names (an Xbox path: a test pack, read in its
place; its hash travels like any pack's). A slot takes only its own pack:
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

static boolean path_exists(
	char const *path)
{
	return GetFileAttributesA(path) != (DWORD)-1;
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

	if (!asked_for && !path_exists(PACKS_FOLDER))
		return NULL;
	if (!path_exists(path))
	{
		pack_refused(slot, path, "there is no such file");
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
