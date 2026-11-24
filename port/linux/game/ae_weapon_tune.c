/*
AE_WEAPON_TUNE.C

DEV MODE's tune command (ae_weapon_tune.h; the keys and limits: ae_weapon_tune_rules.c). Key scheme: Halo Content Evolved
(Watanoge/halo-content-evolved, AGPL-3.0-or-later).
*/

#include <stdio.h>
#include <string.h>

#include "cseries.h"
#include "game/game.h"
#include "game/game_globals.h"
#include "game/players.h"
#include "items/projectile_definitions.h"
#include "items/weapon_definitions.h"
#include "items/weapons.h"
#include "objects/damage_effect_definitions.h"
#include "tag_files/tag_groups.h"
#include "tag_files/tag_files.h"
#include "units/units.h"
#include "../src/port_config.h"
#include "../src/posix.h"

/* (the config folder is a host path: not the game's Xbox-path file calls) */
#undef fopen

#include "ae_view_model.h"
#include "ae_weapon_tune.h"
#include "ae_weapon_tune_rules.h"

void platform_log(char const *format, ...);

#define CHANGES_MOST 64
#define FILE_TEXT 8192

/* where a key's number lives in the loaded tags: one or two reals (a bounds pair, or a speed's initial and final), or a
whole number of 16 bits; the factor from the tag editor's unit to the memory's (a speed is kept for a tick, an angle in
radians) */
struct place
{
	real *real_at[2];
	short *short_at;
	int pair;
	double factor;
	long tag_index;
	enum ae_tune_tag_kind kind;
};

/* a number changed, with what the memory held before the first change */
struct change
{
	long definition_index;
	struct ae_tune_key key;
	long tag_index;
	/* the memory the number lives in: two keys of two weapons can be one (a shared projectile or damage effect) */
	unsigned long where;
	boolean is_short;
};

static struct change changes[CHANGES_MOST];
static int change_count;
/* the numbers each location held before its first change (one canonical original a location) */
static struct ae_tune_origins origins;

int ae_weapon_tune_count(void)
{
	return change_count;
}

void ae_weapon_tune_new_game(void)
{
	change_count = 0;
	memset(&origins, 0, sizeof origins);
}

/* the definition of the weapon the player holds, NONE for none */
static long held_definition(long player_index)
{
	struct player_datum *player;
	struct unit_datum *unit;
	struct weapon_datum *weapon;
	long weapon_index;

	if (player_index == NONE || (player = player_get(player_index))->unit_index == NONE)
		return NONE;
	unit = unit_get(player->unit_index);
	if (unit->unit.current_weapon_index == NONE)
		return NONE;
	weapon_index = unit->unit.weapon_object_indices[unit->unit.current_weapon_index];
	if (weapon_index == NONE || !(weapon = weapon_try_and_get(weapon_index)))
		return NONE;
	return weapon->definition_index;
}

/* the place of a key in the weapon's tags; 1, or 0 with the reason */
static int place_of(long definition_index, struct ae_tune_key key, struct place *place, char const **why)
{
	struct weapon_definition *weapon = weapon_definition_get(definition_index);
	struct weapon_trigger_definition *trigger = NULL;
	struct weapon_magazine_definition *magazine = NULL;
	long projectile_index = NONE;

	memset(place, 0, sizeof *place);
	place->factor = 1.0;
	place->kind = ae_tune_key_tag_kind(key);
	place->tag_index = definition_index;
	if (key.field == AE_TUNE_MAGAZINE_SIZE || key.field == AE_TUNE_MAGAZINE_TOTAL || key.field == AE_TUNE_MAGAZINE_RELOAD)
	{
		if (key.index >= weapon->weapon.magazines.count)
		{
			*why = "the weapon has no such magazine";
			return 0;
		}
		magazine = TAG_BLOCK_GET_ELEMENT(&weapon->weapon.magazines, key.index, struct weapon_magazine_definition);
	}
	else if (key.field != AE_TUNE_MELEE_DAMAGE && key.field != AE_TUNE_ZOOM && key.field != AE_TUNE_AUTOAIM)
	{
		if (key.index >= weapon->weapon.triggers.count)
		{
			*why = "the weapon has no such trigger";
			return 0;
		}
		trigger = TAG_BLOCK_GET_ELEMENT(&weapon->weapon.triggers, key.index, struct weapon_trigger_definition);
		projectile_index = trigger->projectile.index;
	}
	switch (key.field)
	{
	case AE_TUNE_TRIGGER_RATE:
		place->real_at[0] = &trigger->initial_rate_of_fire;
		place->real_at[1] = &trigger->final_rate_of_fire;
		place->pair = 1;
		break;
	case AE_TUNE_TRIGGER_SPREAD:
		place->real_at[0] = &trigger->projectile_error_angle_lower_bound;
		place->real_at[1] = &trigger->projectile_error_angle_upper_bound;
		place->pair = 1;
		place->factor = DEGREES_TO_RADIANS(1.0);
		break;
	case AE_TUNE_TRIGGER_PROJECTILES:
		place->short_at = &trigger->projectiles_per_shot;
		break;
	case AE_TUNE_TRIGGER_ROUNDS:
		place->short_at = &trigger->rounds_per_shot;
		break;
	case AE_TUNE_MAGAZINE_SIZE:
		place->short_at = &magazine->rounds_loaded_maximum;
		break;
	case AE_TUNE_MAGAZINE_TOTAL:
		place->short_at = &magazine->rounds_total_maximum;
		break;
	case AE_TUNE_MAGAZINE_RELOAD:
		place->real_at[0] = &magazine->reload_time;
		break;
	case AE_TUNE_PROJECTILE_SPEED:
	case AE_TUNE_IMPACT_DAMAGE:
	{
		struct projectile_definition *projectile;

		if (projectile_index == NONE)
		{
			*why = "the trigger fires no projectile";
			return 0;
		}
		projectile = projectile_definition_get(projectile_index);
		if (key.field == AE_TUNE_PROJECTILE_SPEED)
		{
			place->real_at[0] = &projectile->projectile.initial_velocity;
			place->real_at[1] = &projectile->projectile.final_velocity;
			place->factor = 1.0 / TICKS_PER_SECOND;
			place->tag_index = projectile_index;
		}
		else
		{
			struct damage_effect_definition *damage;

			if (projectile->projectile.impact_damage.index == NONE)
			{
				*why = "the projectile has no impact damage";
				return 0;
			}
			damage = damage_effect_definition_get(projectile->projectile.impact_damage.index);
			place->real_at[0] = &damage->damage.damage_lower_bound;
			place->real_at[1] = &damage->damage.damage_upper_bound;
			place->pair = 1;
			place->tag_index = projectile->projectile.impact_damage.index;
		}
		break;
	}
	case AE_TUNE_MELEE_DAMAGE:
	{
		struct damage_effect_definition *damage;

		if (weapon->weapon.melee_attack_damage.index == NONE)
		{
			*why = "the weapon has no melee damage";
			return 0;
		}
		damage = damage_effect_definition_get(weapon->weapon.melee_attack_damage.index);
		place->real_at[0] = &damage->damage.damage_lower_bound;
		place->real_at[1] = &damage->damage.damage_upper_bound;
		place->pair = 1;
		place->tag_index = weapon->weapon.melee_attack_damage.index;
		break;
	}
	case AE_TUNE_ZOOM:
		place->real_at[0] = &weapon->weapon.zoom_magnification_minimum;
		place->real_at[1] = &weapon->weapon.zoom_magnification_maximum;
		place->pair = 1;
		break;
	case AE_TUNE_AUTOAIM:
		place->real_at[0] = &weapon->weapon.aim_assist_parameters.autoaim_angle;
		place->factor = DEGREES_TO_RADIANS(1.0);
		break;
	default:
		*why = "no such key";
		return 0;
	}
	return 1;
}

/* the number in the tag editor's unit (the second end the same as the first where the key has one) */
static void value_get(struct place const *place, double *low, double *high)
{
	if (place->short_at)
		*low = *high = *place->short_at;
	else
	{
		*low = *place->real_at[0] / place->factor;
		*high = place->real_at[1] ? *place->real_at[1] / place->factor : *low;
	}
}

static void value_put(struct place const *place, struct ae_tune_value value)
{
	if (place->short_at)
		*place->short_at = (short)value.low;
	else
	{
		*place->real_at[0] = (real)(value.low * place->factor);
		if (place->real_at[1])
			*place->real_at[1] = (real)((place->pair ? value.high : value.low) * place->factor);
	}
}

/* a place's memory, as a number (the first of its fields) */
static unsigned long place_where(struct place const *place)
{
	return place->short_at ? (unsigned long)(size_t)place->short_at : (unsigned long)(size_t)place->real_at[0];
}

static struct change *change_find(long definition_index, struct ae_tune_key key)
{
	int i;

	for (i = 0; i < change_count; i++)
		if (changes[i].definition_index == definition_index && changes[i].key.field == key.field &&
			changes[i].key.index == key.index)
			return &changes[i];
	return NULL;
}

/* the keys a weapon may have: the ones with a number (a trigger, a magazine), and those a weapon has once */
static int keys_of_field(int field)
{
	return field == AE_TUNE_MELEE_DAMAGE || field == AE_TUNE_ZOOM || field == AE_TUNE_AUTOAIM ? 1 : 2;
}

/* (a line added when it fits: the text stays whole lines) */
static void line_add(char *out, int size, int *used, char const *line)
{
	int length = (int)strlen(line);

	if (*used + length < size)
	{
		memcpy(out + *used, line, (size_t)length + 1);
		*used += length;
	}
}

int ae_weapon_tune_list(long player_index, char *out, int size)
{
	long definition_index = held_definition(player_index);
	char key[AE_VIEW_MODEL_KEY], name[48], line[160];
	char const *tag_name;
	int field, index, used = 0;

	if (size <= 0)
		return 0;
	out[0] = 0;
	if (definition_index == NONE)
		return 0;
	ae_view_model_key(definition_index, key, sizeof key);
	tag_name = tag_get_name(definition_index);
	snprintf(line, sizeof line, "%s (%s)\n", key, tag_name ? tag_name : "?");
	line_add(out, size, &used, line);
	for (field = 0; field < AE_TUNE_FIELD_COUNT; field++)
		for (index = 0; index < keys_of_field(field); index++)
		{
			struct ae_tune_key tune_key;
			struct place place;
			char const *unused;
			double low, high;

			tune_key.field = (enum ae_tune_field)field;
			tune_key.index = (short)index;
			if (!place_of(definition_index, tune_key, &place, &unused))
				continue;
			value_get(&place, &low, &high);
			ae_tune_key_name(tune_key, name, sizeof name);
			if (ae_tune_key_is_bounds(tune_key))
				snprintf(line, sizeof line, "%s%s %g..%g\n", change_find(definition_index, tune_key) ? "*" : "", name,
					low, high);
			else
				snprintf(line, sizeof line, "%s%s %g\n", change_find(definition_index, tune_key) ? "*" : "", name, low);
			line_add(out, size, &used, line);
		}
	return 1;
}

int ae_weapon_tune_set(long player_index, char const *key_text, char const *value_text, char *answer, int size)
{
	long definition_index = held_definition(player_index);
	struct ae_tune_key key;
	struct ae_tune_value value;
	struct place place;
	struct change *change;
	char const *why = "";
	char name[48];
	double before_low, before_high;

	if (definition_index == NONE)
	{
		snprintf(answer, (size_t)size, "no weapon in hand");
		return 0;
	}
	if (!ae_tune_key_parse(key_text, &key))
	{
		snprintf(answer, (size_t)size, "no such key: %s", key_text);
		return 0;
	}
	if (!ae_tune_value_parse(key, value_text, &value))
	{
		snprintf(answer, (size_t)size, "%s takes %s", key_text, ae_tune_key_is_bounds(key) ? "a number or a..b" : "a number");
		return 0;
	}
	if (!ae_tune_value_allowed(key, value.low) || !ae_tune_value_allowed(key, value.high))
	{
		snprintf(answer, (size_t)size, "%s cannot hold %s", key_text, value_text);
		return 0;
	}
	if (!place_of(definition_index, key, &place, &why))
	{
		snprintf(answer, (size_t)size, "%s: %s", key_text, why);
		return 0;
	}
	if (!(change = change_find(definition_index, key)))
	{
		if (change_count >= CHANGES_MOST)
		{
			snprintf(answer, (size_t)size, "too many numbers changed (tune reset)");
			return 0;
		}
		{
			double first[2];
			int kept;

			/* (the memory's two numbers: a shared one may have been changed already; the first original is kept) */
			kept = ae_tune_origin_keep(&origins, place_where(&place),
				place.short_at ? (double)*place.short_at : (double)*place.real_at[0],
				place.real_at[1] ? (double)*place.real_at[1] : 0.0, &first[0], &first[1]);
			if (!kept)
			{
				snprintf(answer, (size_t)size, "too many numbers changed (tune reset)");
				return 0;
			}
		}
		change = &changes[change_count++];
		change->definition_index = definition_index;
		change->key = key;
		change->tag_index = place.tag_index;
		change->where = place_where(&place);
		change->is_short = place.short_at != NULL;
	}
	value_get(&place, &before_low, &before_high);
	value_put(&place, value);
	ae_tune_key_name(key, name, sizeof name);
	if (ae_tune_key_is_bounds(key))
		snprintf(answer, (size_t)size, "%s %g..%g -> %g..%g", name, before_low, before_high, value.low, value.high);
	else
		snprintf(answer, (size_t)size, "%s %g -> %g", name, before_low, value.low);
	platform_log("dev tune: %s", answer);
	return 1;
}

/* a change's place put back as the map loaded it (the location's canonical original), and every change of that location
(another weapon's key on a shared projectile or damage effect) forgotten with it */
static void change_undo(int index)
{
	struct place place;
	char const *why;
	double a = 0.0, b = 0.0;
	unsigned long where = changes[index].where;
	int i, kept = 0;

	if (place_of(changes[index].definition_index, changes[index].key, &place, &why) && place_where(&place) == where &&
		ae_tune_origin_keep(&origins, where, 0.0, 0.0, &a, &b))
	{
		if (place.short_at)
			*place.short_at = (short)a;
		if (place.real_at[0])
			*place.real_at[0] = (real)a;
		if (place.real_at[1])
			*place.real_at[1] = (real)b;
	}
	ae_tune_origin_drop(&origins, where);
	for (i = 0; i < change_count; i++)
		if (changes[i].where != where)
			changes[kept++] = changes[i];
	change_count = kept;
}

int ae_weapon_tune_reset(long player_index)
{
	long definition_index = held_definition(player_index);
	int i, dropped = 0;

	if (definition_index == NONE)
		return 0;
	for (i = 0; i < change_count;)
	{
		if (changes[i].definition_index == definition_index)
		{
			change_undo(i);
			dropped++;
			i = 0;
		}
		else
			i++;
	}
	return dropped;
}

int ae_weapon_tune_reset_all(void)
{
	int dropped = change_count;

	while (change_count > 0)
		change_undo(0);
	return dropped;
}

/* a tag's name as the recipes write it: forward slashes, the group's extension */
static void tag_path(long tag_index, enum ae_tune_tag_kind kind, char *out, int size)
{
	static char const *const extensions[] = { "weapon", "projectile", "damage_effect", "damage_effect" };
	char const *name = tag_get_name(tag_index);
	int i;

	snprintf(out, (size_t)size, "%s.%s", name ? name : "?", extensions[kind]);
	for (i = 0; out[i]; i++)
		if (out[i] == '\\')
			out[i] = '/';
}

static void json_value(char *out, int size, struct ae_tune_key key, double low, double high)
{
	if (ae_tune_key_is_bounds(key))
		snprintf(out, (size_t)size, "[%.9g, %.9g]", low, high);
	else
		snprintf(out, (size_t)size, "%.9g", low);
}

int ae_weapon_tune_save(long player_index, char *path_out, int size)
{
	static char text[FILE_TEXT];
	long definition_index = held_definition(player_index);
	char key[AE_VIEW_MODEL_KEY], folder[400], directory[420], path[460], new_path[470], weapon_tag[256];
	char key_json[3 * AE_VIEW_MODEL_KEY], tag_json[800], safe[AE_TUNE_NAME_MOST + 1];
	int i, used, edits = 0;
	FILE *file;
	int written;

	if (definition_index == NONE)
	{
		snprintf(path_out, (size_t)size, "no weapon in hand");
		return 0;
	}
	ae_view_model_key(definition_index, key, sizeof key);
	tag_path(definition_index, AE_TUNE_IN_WEAPON, weapon_tag, sizeof weapon_tag);
	/* (a key and tag names come from the map: escaped in the file, and the file's own name is made of safe letters) */
	if (!ae_tune_json_escape(key, key_json, sizeof key_json) || !ae_tune_json_escape(weapon_tag, tag_json, sizeof tag_json))
	{
		snprintf(path_out, (size_t)size, "the weapon's names are too long for a tune file");
		return 0;
	}
	used = snprintf(text, sizeof text, "{\"format\": 1, \"weapon_key\": \"%s\", \"weapon_tag\": \"%s\", \"edits\": [",
		key_json, tag_json);
	for (i = 0; i < change_count && used < (int)sizeof text - 1; i++)
	{
		struct place place;
		char const *why, *kind_names[] = { "weapon", "projectile", "impact_effect", "melee_effect" };
		char field_path[96], second[96], tag[256], tag_escaped[800], value[96], name[48];
		double low, high;

		if (changes[i].definition_index != definition_index || !place_of(definition_index, changes[i].key, &place, &why))
			continue;
		value_get(&place, &low, &high);
		ae_tune_invader_path(changes[i].key, field_path, sizeof field_path);
		tag_path(place.tag_index, place.kind, tag, sizeof tag);
		if (!ae_tune_json_escape(tag, tag_escaped, sizeof tag_escaped))
			continue;
		json_value(value, sizeof value, changes[i].key, low, high);
		ae_tune_key_name(changes[i].key, name, sizeof name);
		used += snprintf(text + used, sizeof text - (size_t)used,
			"%s\n {\"key\": \"%s\", \"tag_kind\": \"%s\", \"tag\": \"%s\", \"set\": [\"%s\", %s]}", edits ? "," : "", name,
			kind_names[place.kind], tag_escaped, field_path, value);
		/* (a speed's final velocity goes with its initial one: its own edit) */
		if (ae_tune_invader_path_second(changes[i].key, second, sizeof second) && used < (int)sizeof text - 1)
			used += snprintf(text + used, sizeof text - (size_t)used,
				",\n {\"key\": \"%s\", \"tag_kind\": \"%s\", \"tag\": \"%s\", \"set\": [\"%s\", %s]}", name,
				kind_names[place.kind], tag_escaped, second, value);
		edits++;
	}
	if (!edits)
	{
		snprintf(path_out, (size_t)size, "nothing changed on %s", key);
		return 0;
	}
	if (used >= (int)sizeof text - 4)
	{
		snprintf(path_out, (size_t)size, "the tune file would be too long");
		return 0;
	}
	snprintf(text + used, sizeof text - (size_t)used, "\n]}\n");
	config_folder(folder, sizeof folder);
	snprintf(directory, sizeof directory, "%s/tune", folder);
	posix_make_directory(directory);
	ae_tune_safe_name(key, safe, sizeof safe);
	snprintf(path, sizeof path, "%s/%s.json", directory, safe);
	snprintf(new_path, sizeof new_path, "%s.new", path);
	if (!(file = fopen(new_path, "wb")))
	{
		snprintf(path_out, (size_t)size, "%s could not be written", path);
		return 0;
	}
	written = fwrite(text, 1, strlen(text), file) == strlen(text);
	written = fclose(file) == 0 && written;
	if (!written || posix_replace_file(new_path, path) != 0)
	{
		snprintf(path_out, (size_t)size, "%s could not be written", path);
		return 0;
	}
	snprintf(path_out, (size_t)size, "%s", path);
	platform_log("dev tune: %d numbers of %s written to %s", edits, key, path);
	return 1;
}
