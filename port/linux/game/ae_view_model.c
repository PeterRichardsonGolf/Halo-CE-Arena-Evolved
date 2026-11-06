/*
AE_VIEW_MODEL.C

The first-person weapon's placement (ae_view_model.h). The table is the shipped defaults with the player's file
(view_models.txt in the config folder) over them, read at the first draw; DEV MODE's viewmodel command changes it live
and keeps it on request.
*/

#include <stdio.h>
#include <string.h>

#include "cseries.h"
#include "items/weapon_definitions.h"
#include "items/weapons.h"
#include "tag_files/tag_files.h"
#include "units/units.h"
#include "game/players.h"
#include "../src/port_config.h"
#include "../src/posix.h"

/* (the config folder is a host path: not the game's Xbox-path file calls) */
#undef fopen

#include "ae_view_model.h"
#include "ae_weapon_pack.h"
#include "ae_weapon_slots.h"

void platform_log(char const *format, ...);

#define FILE_NAME "view_models.txt"
#define FILE_TEXT 4096

static struct ae_view_model_table table;
static int loaded;

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
	ae_view_model_table_defaults(&table);
	if (file_path(path, sizeof path) && (file = fopen(path, "rb")) != NULL)
	{
		length = fread(text, 1, sizeof text - 1, file);
		fclose(file);
		text[length] = 0;
		set = ae_view_model_table_parse(&table, text);
	}
	file_path(path, sizeof path);
	platform_log("view models: %d weapons placed (%d from %s)", table.count, set, path);
}

/* a weapon's key: a pack weapon's name, else its tag's last name */
static void key_of(long definition_index, char *key, int size)
{
	short slot = ae_weapon_pack_definition_slot(definition_index);
	char const *name = slot > 0 ? ae_weapon_slots_name(slot) : tag_name_strip_path(tag_get_name(definition_index));

	/* (a tag with no name keys nothing) */
	snprintf(key, (size_t)size, "%s", name ? name : "");
}

void ae_view_model_apply(long weapon_index, struct real_matrix4x3 *nodes, short count)
{
	struct weapon_datum *weapon = weapon_try_and_get(weapon_index);
	struct ae_view_model model;
	char key[AE_VIEW_MODEL_KEY];
	float axes[3][3], position[3];
	short index;

	if (!weapon || count < 1)
		return;
	if (!loaded)
		load();
	key_of(weapon->definition_index, key, sizeof key);
	if (!ae_view_model_table_get(&table, key, &model) || ae_view_model_is_identity(&model))
		return;
	/* (the root's frame, kept: the root moves with the rest) */
	memcpy(axes, nodes[0].n, sizeof axes);
	memcpy(position, &nodes[0].position, sizeof position);
	{
		float root_scale = nodes[0].scale;

		for (index = 0; index < count; index++)
		{
			float at[3];

			memcpy(at, &nodes[index].position, sizeof at);
			ae_view_model_node(&model, position, (float const (*)[3])axes, root_scale, at, &nodes[index].scale);
			memcpy(&nodes[index].position, at, sizeof at);
		}
	}
}

int ae_view_model_held(long player_index, char *key, int key_size, struct ae_view_model *out)
{
	struct player_datum *player;
	struct unit_datum *unit;
	struct weapon_datum *weapon;
	long weapon_index;

	if (player_index == NONE || (player = player_get(player_index))->unit_index == NONE)
		return 0;
	unit = unit_get(player->unit_index);
	if (unit->unit.current_weapon_index == NONE)
		return 0;
	weapon_index = unit->unit.weapon_object_indices[unit->unit.current_weapon_index];
	if (weapon_index == NONE || !(weapon = weapon_try_and_get(weapon_index)))
		return 0;
	if (!loaded)
		load();
	key_of(weapon->definition_index, key, key_size);
	ae_view_model_table_get(&table, key, out);
	return 1;
}

int ae_view_model_set(char const *key, struct ae_view_model const *model)
{
	if (!loaded)
		load();
	return ae_view_model_table_set(&table, key, model);
}

/* the weapon's shipped numbers back (or none, when the shipped table has none) */
void ae_view_model_reset(char const *key)
{
	struct ae_view_model_table shipped;
	struct ae_view_model model;

	if (!loaded)
		load();
	ae_view_model_table_defaults(&shipped);
	if (ae_view_model_table_get(&shipped, key, &model))
		ae_view_model_table_set(&table, key, &model);
	else
		ae_view_model_table_clear(&table, key);
}

/* the table as the player's file: only the weapons that differ from the shipped table are kept */
int ae_view_model_save(void)
{
	static char text[FILE_TEXT];
	struct ae_view_model_table shipped, own;
	char path[420], new_path[430];
	FILE *file;
	int index, written;

	if (!loaded)
		load();
	ae_view_model_table_defaults(&shipped);
	memset(&own, 0, sizeof own);
	for (index = 0; index < table.count; index++)
	{
		struct ae_view_model_entry const *entry = &table.entry[index];
		struct ae_view_model base;

		if (!ae_view_model_table_get(&shipped, entry->key, &base) || memcmp(&base, &entry->value, sizeof base))
			ae_view_model_table_set(&own, entry->key, &entry->value);
	}
	if (!file_path(path, sizeof path) || !ae_view_model_table_format(&own, text, sizeof text))
		return 0;
	snprintf(new_path, sizeof new_path, "%s.new", path);
	if (!(file = fopen(new_path, "wb")))
		return 0;
	written = fwrite(text, 1, strlen(text), file) == strlen(text);
	written = fclose(file) == 0 && written;
	return written && posix_replace_file(new_path, path) == 0;
}
