/*
AE_VIEW_MODEL.C

The first-person weapon's placement (ae_view_model.h). The table is the shipped defaults with the player's file
(view_models.txt in the config folder) over them, read at the first draw; DEV MODE's viewmodel command changes it live
and keeps it on request.
*/

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "cseries.h"
#include "interface/first_person_weapons.h"
#include "items/weapon_definitions.h"
#include "models/model_definitions.h"
#include "objects/objects.h"
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
static int probe_pending;

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

void ae_view_model_key(long definition_index, char *key, int size)
{
	key_of(definition_index, key, size);
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
	if (!ae_view_model_table_get(&table, key, &model) || (model.offset[0] == 0.0f && model.offset[1] == 0.0f &&
		model.offset[2] == 0.0f && model.scale == 1.0f))
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

void ae_view_model_probe_request(void)
{
	probe_pending = 1;
}

/* a world point in the camera's frame: forward, left, up from the camera's position */
static void camera_frame(union real_point3d const *point, union real_point3d const *camera_position,
	float const forward[3], float const left[3], float const up[3], float out[3])
{
	float d[3];
	int axis;

	for (axis = 0; axis < 3; axis++)
		d[axis] = point->n[axis] - camera_position->n[axis];
	out[0] = d[0] * forward[0] + d[1] * forward[1] + d[2] * forward[2];
	out[1] = d[0] * left[0] + d[1] * left[1] + d[2] * left[2];
	out[2] = d[0] * up[0] + d[1] * up[1] + d[2] * up[2];
}

/* the probe: the pivot and each marker of the first-person model, in the camera's frame (world units), one log line each,
the weapon's numbers as they are (the gun's own offset in, the arms and gun's placement not yet) */
static void probe(long weapon_index, char const *key, struct real_matrix4x3 const *pivot,
	union real_point3d const *camera_position, union real_vector3d const *camera_forward,
	union real_vector3d const *camera_up)
{
	struct weapon_datum *weapon = weapon_try_and_get(weapon_index);
	struct weapon_definition *definition;
	struct model *model;
	float forward[3], up[3], left[3], at[3], length, dot;
	long model_index;
	int axis, marker_index, logged = 0;

	if (!weapon)
		return;
	definition = weapon_definition_get(weapon->definition_index);
	model_index = definition->weapon.interface_definition.first_person_model.index;
	/* (the camera's frame as the placement makes it: up square to forward, left = up x forward) */
	length = sqrtf(camera_forward->i * camera_forward->i + camera_forward->j * camera_forward->j +
		camera_forward->k * camera_forward->k);
	for (axis = 0; axis < 3; axis++)
		forward[axis] = length > 0.0f ? camera_forward->n[axis] / length : 0.0f;
	dot = camera_up->n[0] * forward[0] + camera_up->n[1] * forward[1] + camera_up->n[2] * forward[2];
	for (axis = 0; axis < 3; axis++)
		up[axis] = camera_up->n[axis] - dot * forward[axis];
	length = sqrtf(up[0] * up[0] + up[1] * up[1] + up[2] * up[2]);
	for (axis = 0; axis < 3; axis++)
		up[axis] = length > 0.0f ? up[axis] / length : 0.0f;
	left[0] = up[1] * forward[2] - up[2] * forward[1];
	left[1] = up[2] * forward[0] - up[0] * forward[2];
	left[2] = up[0] * forward[1] - up[1] * forward[0];
	camera_frame(&pivot->position, camera_position, forward, left, up, at);
	platform_log("viewmodel probe: %s pivot %.5f %.5f %.5f", key, at[0], at[1], at[2]);
	if (model_index == NONE)
		return;
	model = model_definition_get(model_index);
	for (marker_index = 0; marker_index < model->markers.count; marker_index++)
	{
		struct model_marker const *marker = TAG_BLOCK_GET_ELEMENT(&model->markers, marker_index, struct model_marker);
		struct object_marker found;
		char name[sizeof marker->name + 1];

		memcpy(name, marker->name, sizeof marker->name);
		name[sizeof marker->name] = 0;
		if (!name[0] || first_person_weapon_get_marker_by_name(weapon_index, name, &found, 1) < 1)
			continue;
		camera_frame(&found.matrix.position, camera_position, forward, left, up, at);
		/* (a name with spaces kept whole: quoted) */
		platform_log("viewmodel probe: %s marker \"%s\" %.5f %.5f %.5f", key, name, at[0], at[1], at[2]);
		logged++;
	}
	platform_log("viewmodel probe: %s %d markers", key, logged);
}

void ae_view_model_place(long weapon_index, struct real_matrix4x3 *nodes, short count, short pivot_node,
	union real_point3d const *camera_position, union real_vector3d const *camera_forward,
	union real_vector3d const *camera_up)
{
	struct weapon_datum *weapon = weapon_try_and_get(weapon_index);
	struct ae_view_model model;
	struct ae_view_model_frame frame;
	char key[AE_VIEW_MODEL_KEY];
	float forward[3], up[3], pivot[3];
	short index;

	if (!weapon || count < 1)
		return;
	if (pivot_node < 0 || pivot_node >= count)
		pivot_node = 0;
	if (!loaded)
		load();
	key_of(weapon->definition_index, key, sizeof key);
	if (probe_pending)
	{
		probe_pending = 0;
		probe(weapon_index, key, &nodes[pivot_node], camera_position, camera_forward, camera_up);
	}
	ae_view_model_table_get(&table, key, &model);
	memcpy(forward, camera_forward->n, sizeof forward);
	memcpy(up, camera_up->n, sizeof up);
	memcpy(pivot, nodes[pivot_node].position.n, sizeof pivot);
	if (!ae_view_model_place_frame(&model, forward, up, pivot, &frame))
		return;
	for (index = 0; index < count; index++)
	{
		float position[3], axes[3][3];

		memcpy(position, nodes[index].position.n, sizeof position);
		memcpy(axes, nodes[index].n, sizeof axes);
		ae_view_model_place_node(&frame, position, axes, &nodes[index].scale);
		memcpy(nodes[index].position.n, position, sizeof position);
		memcpy(nodes[index].n, axes, sizeof axes);
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
