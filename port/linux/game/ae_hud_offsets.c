/*
AE_HUD_OFFSETS.C

The weapon HUD element mover (ae_hud_offsets.h; the table and the math: ae_hud_offsets_rules.c).
*/

#include <stdio.h>
#include <string.h>

#include "cseries.h"
#include "interface/hud_definitions.h"
#include "tag_files/tag_files.h"
#include "../src/port_config.h"
#include "../src/posix.h"

/* (the config folder is a host path: not the game's Xbox-path file calls) */
#undef fopen

#include "ae_hud_offsets.h"
#include "ae_view_model.h"

void platform_log(char const *format, ...);

#define FILE_NAME "hud_offsets.txt"
#define FILE_TEXT 8192
/* the most placements put back at the end of one element: an overlay's items */
#define UNDO_MOST 64

static struct ae_hud_table table;
static int loaded;

/* the weapon being drawn, none between draws */
static struct
{
	int active;
	long hud_index;
	short local_player_index;
	char key[AE_HUD_KEY];
} context;

/* the elements of the first local player's last weapon HUD draw */
static struct ae_hud_seen seen[AE_HUD_ELEMENTS];
static int seen_count;
static char seen_key[AE_HUD_KEY];

/* the element being drawn moved, and the placements to put back */
static struct
{
	int active;
	int overlay;
	struct ae_hud_element element;
	short corner;
} current;
static struct
{
	struct hud_placement_definition *where;
	struct hud_placement_definition saved;
} undo[UNDO_MOST];
static int undo_count;

static int file_path(char *path, int size)
{
	char folder[400];

	config_folder(folder, sizeof folder);
	return snprintf(path, (size_t)size, "%s/%s", folder, FILE_NAME) < size;
}

static void load(void)
{
	static char text[FILE_TEXT];
	char path[420];
	FILE *file;
	size_t length;
	int set = 0;

	loaded = 1;
	ae_hud_table_clear_all(&table);
	if (file_path(path, sizeof path) && (file = fopen(path, "rb")) != NULL)
	{
		length = fread(text, 1, sizeof text - 1, file);
		fclose(file);
		text[length] = 0;
		set = ae_hud_table_parse(&table, text);
	}
	file_path(path, sizeof path);
	platform_log("hud offsets: %d weapons moved (%d elements from %s)", table.count, set, path);
}

void ae_hud_offsets_weapon(long weapon_definition_index, long hud_index, short local_player_index)
{
	/* (the last draw's placements are all put back by now: end() did it) */
	if (weapon_definition_index == NONE)
	{
		context.active = 0;
		return;
	}
	if (!loaded)
		load();
	context.active = 1;
	context.hud_index = hud_index;
	context.local_player_index = local_player_index;
	ae_view_model_key(weapon_definition_index, context.key, sizeof context.key);
	if (local_player_index == 0)
	{
		seen_count = 0;
		snprintf(seen_key, sizeof seen_key, "%s", context.key);
	}
}

static void push(struct hud_placement_definition *placement)
{
	int index;

	if (undo_count >= UNDO_MOST)
		return;
	/* (a placement an element's items share is moved once) */
	for (index = 0; index < undo_count; index++)
		if (undo[index].where == placement)
			return;
	undo[undo_count].where = placement;
	undo[undo_count].saved = *placement;
	undo_count++;
	ae_hud_element_apply(&current.element, current.corner, &placement->offset.x, &placement->offset.y,
		&placement->scale.i, &placement->scale.j);
}

void ae_hud_offsets_begin(long hud_index, short kind, short element_index, struct hud_placement_definition *placement,
	struct tag_reference const *bitmap, short corner)
{
	char const *name;

	current.active = 0;
	if (!context.active || hud_index != context.hud_index)
		return;
	if (context.local_player_index == 0 && seen_count < AE_HUD_ELEMENTS)
	{
		struct ae_hud_seen *entry = &seen[seen_count++];

		entry->index = element_index;
		entry->kind = kind;
		name = bitmap && bitmap->index != NONE ? tag_name_strip_path(tag_get_name(bitmap->index)) : NULL;
		snprintf(entry->bitmap, sizeof entry->bitmap, "%s", name ? name : "");
	}
	if (!ae_hud_table_get(&table, context.key, element_index, &current.element))
		return;
	current.active = 1;
	current.overlay = kind == AE_HUD_OVERLAY;
	current.corner = corner;
	if (placement)
		push(placement);
}

void ae_hud_offsets_overlay_item(struct hud_placement_definition const *placement)
{
	if (current.active && current.overlay)
		push((struct hud_placement_definition *)placement);
}

void ae_hud_offsets_end(void)
{
	while (undo_count > 0)
	{
		undo_count--;
		*undo[undo_count].where = undo[undo_count].saved;
	}
	current.active = 0;
}

void ae_hud_offsets_seen_key(char *key, int size)
{
	snprintf(key, (size_t)size, "%s", seen_key);
}

int ae_hud_offsets_seen_count(void)
{
	return seen_count;
}

int ae_hud_offsets_seen_get(int i, struct ae_hud_seen *out)
{
	if (i < 0 || i >= seen_count)
		return 0;
	*out = seen[i];
	return 1;
}

char const *ae_hud_offsets_kind_name(short kind)
{
	static char const *const names[] = { "static", "meter", "number", "overlay" };

	return kind >= 0 && kind < 4 ? names[kind] : "?";
}

int ae_hud_offsets_get(char const *key, int index, struct ae_hud_element *out)
{
	if (!loaded)
		load();
	return ae_hud_table_get(&table, key, index, out);
}

int ae_hud_offsets_set(char const *key, struct ae_hud_element const *element)
{
	if (!loaded)
		load();
	return ae_hud_table_set(&table, key, element);
}

int ae_hud_offsets_clear(char const *key, int index)
{
	if (!loaded)
		load();
	return ae_hud_table_clear(&table, key, index);
}

int ae_hud_offsets_save(void)
{
	static char text[FILE_TEXT];
	char path[420], new_path[430];
	FILE *file;
	int written;

	if (!loaded)
		load();
	if (!file_path(path, sizeof path) || !ae_hud_table_format(&table, text, sizeof text))
		return 0;
	snprintf(new_path, sizeof new_path, "%s.new", path);
	if (!(file = fopen(new_path, "wb")))
		return 0;
	written = fwrite(text, 1, strlen(text), file) == strlen(text);
	written = fclose(file) == 0 && written;
	return written && posix_replace_file(new_path, path) == 0;
}
