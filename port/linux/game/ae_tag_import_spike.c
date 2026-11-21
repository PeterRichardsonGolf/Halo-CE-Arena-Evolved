/*
AE_TAG_IMPORT_SPIKE.C

(spike, DEV only) Runs the tag importer on a level as it loads. It is also where the
campaign randomizer's copies are made, read and let go (randomizer.c).

HALO_SPIKE_IMPORT lists roots as pack,map,group,name separated by ';'
(group: the four letters of a tag group; name: a tag path), for example
ae_spike,a10,bipd,characters\elite\elite. Each root is copied into the
level with every tag it names, from tag_pack/<pack>.pack. Without the
variable nothing here does anything: the importer is never begun.
*/

#include "cseries.h"
#include "tag_files/tag_groups.h"
#include "tag_import.h"
#include "ae_tag_loaded.h"
#include "ae_tag_import_spike.h"
#include "ae_tag_inject.h"
#include "randomizer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void platform_log(char const *format, ...);

enum
{
	MAXIMUM_SPIKE_ROOTS = 8,
	MAXIMUM_SPIKE_TEXT = 1024,
};

struct spike_root
{
	char const *pack;
	char const *map;
	long group;
	char const *name;
};

static char spike_text[MAXIMUM_SPIKE_TEXT];
static struct spike_root spike_roots[MAXIMUM_SPIKE_ROOTS];
static int spike_root_count;
static boolean spike_read_done;
static boolean spike_imported;

/* the variable's roots, read once: its text is cut in place into the roots' parts */
static void spike_roots_read(void)
{
	char const *value;
	char *cursor;

	if (spike_read_done)
		return;
	spike_read_done = TRUE;
	value = getenv("HALO_SPIKE_IMPORT");
	if (!value || !value[0] || strlen(value) >= sizeof(spike_text))
		return;
	strcpy(spike_text, value);
	cursor = spike_text;
	while (cursor && *cursor && spike_root_count < MAXIMUM_SPIKE_ROOTS)
	{
		char *next = strchr(cursor, ';');
		char *parts[4];
		int part;

		if (next)
			*next++ = 0;
		for (part = 0; part < 4; part++)
		{
			parts[part] = cursor;
			if (part < 3)
			{
				cursor = cursor ? strchr(cursor, ',') : NULL;
				if (cursor)
					*cursor++ = 0;
			}
		}
		if (parts[0] && parts[1] && parts[2] && parts[3] && strlen(parts[2]) == 4)
		{
			struct spike_root *root = &spike_roots[spike_root_count++];

			root->pack = parts[0];
			root->map = parts[1];
			root->group = (long)(((unsigned long)(unsigned char)parts[2][0] << 24) |
				((unsigned long)(unsigned char)parts[2][1] << 16) |
				((unsigned long)(unsigned char)parts[2][2] << 8) |
				(unsigned long)(unsigned char)parts[2][3]);
			root->name = parts[3];
		}
		else
		{
			platform_log("spike import: a root is not pack,map,group,name: left out");
		}
		cursor = next;
	}
}

void ae_tag_import_spike_tags_loaded(
	char const *map_name)
{
	int index;
	long count = 0;
	unsigned long hash = 0;

	/* (the campaign randomizer's copies, when game.randomizer is on: randomizer.c; then the tag injector's) */
	randomizer_tags_loaded(map_name);
	ae_tag_inject_tags_loaded(map_name);
	spike_roots_read();
	/* (the main menu's map takes none) */
	if (!spike_root_count || !map_name || !strcmp(map_name, "ui"))
		return;
	tag_import_tag_table_fingerprint(&count, &hash);
	platform_log("spike import: %s before: %ld tags, fingerprint %08lx", map_name ? map_name : "?", count,
		hash);
	if (!tag_import_begin())
	{
		platform_log("spike import: no room to begin");
		return;
	}
	for (index = 0; index < spike_root_count; index++)
	{
		struct tag_import_root root;
		int found;

		root.group = spike_roots[index].group;
		root.name = spike_roots[index].name;
		found = tag_import_roots(spike_roots[index].pack, spike_roots[index].map, &root, 1);
		platform_log("spike import: root %s of %s in %s: %s", root.name, spike_roots[index].map,
			spike_roots[index].pack, found ? "found" : "NOT found");
	}
	tag_import_run();
	tag_import_end();
	tag_import_log("spike");
	spike_imported = TRUE;
	ae_tag_table_changed();
	tag_import_tag_table_fingerprint(&count, &hash);
	platform_log("spike import: %s after: %ld tags, fingerprint %08lx", map_name ? map_name : "?", count,
		hash);
	for (index = 0; index < spike_root_count; index++)
	{
		long tag_index = tag_loaded((unsigned long)spike_roots[index].group, spike_roots[index].name);

		platform_log("spike import: %s is tag %08lx", spike_roots[index].name, (unsigned long)tag_index);
	}
}

void ae_tag_import_spike_tags_unloaded(void)
{
	ae_tag_inject_tags_unloaded();
	randomizer_tags_unloaded();
	if (!spike_imported)
		return;
	spike_imported = FALSE;
	tag_import_level_unloaded();
	ae_tag_table_changed();
}

boolean ae_tag_import_spike_read(
	long tag_index,
	long offset,
	long size,
	void *buffer,
	boolean *completion_flag)
{
	if (!ae_tag_inject_read(tag_index, offset, size, buffer) &&
		((!spike_imported && !randomizer_imported()) || !tag_import_donor_read(tag_index, offset, size, buffer)))
	{
		return FALSE;
	}
	if (completion_flag)
		*completion_flag = TRUE;

	return TRUE;
}
