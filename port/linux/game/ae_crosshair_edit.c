/*
AE_CROSSHAIR_EDIT.C

CROSSHAIR v3's menu side (ae_crosshair_edit.h). The profile keeps the crosshairs as share codes; a row of the editor
reads the working crosshair from its slot, changes one field, clamps and writes it back, so what is on the screen is what
the game draws on the next frame (ae_crosshair.c reads the same keys).
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#include "ae_crosshair_edit.h"
#include "ae_glue_player_settings.h"
#include "ae_settings_rows.h"

void platform_log(char const *format, ...);

#define PLAYERS 4

static short edit_slot[PLAYERS];
enum { TEXT = 1024 };

static int valid_player(short player)
{
	return player >= 0 && player < PLAYERS;
}

static void slot_key(int slot, char *key, size_t size)
{
	snprintf(key, size, "display.crosshair_%d", slot + 1);
}

static int slot_load(short player, int slot, struct ae_crosshair *xh)
{
	char key[40], text[TEXT];

	if (slot < 0 || slot >= AE_XH_SLOTS)
		return 0;
	slot_key(slot, key, sizeof(key));
	text[0] = 0;
	ae_player_setting_get(player, key, text, sizeof(text));
	return ae_xh_slot_parse(text, xh);
}

static void slot_save(short player, int slot, const struct ae_crosshair *xh)
{
	char key[40], text[TEXT];

	slot_key(slot, key, sizeof(key));
	ae_xh_slot_format(xh, text, sizeof(text));
	ae_player_setting_put(player, key, text);
}

static void new_crosshair(int slot, struct ae_crosshair *xh)
{
	char name[AE_XH_NAME + 1];

	ae_xh_default(xh);
	snprintf(name, sizeof(name), "CROSSHAIR %u", (unsigned)(slot + 1) % 10u);
	ae_xh_name_clean(name, xh->name, sizeof(xh->name));
}

static int main_choice(short player)
{
	char text[TEXT];

	text[0] = 0;
	ae_player_setting_get(player, "display.crosshair", text, sizeof(text));
	return ae_xh_main_of(text);
}

static void main_put(short player, int choice)
{
	char text[8];

	if (choice >= 0 && choice < AE_XH_SLOTS)
		snprintf(text, sizeof(text), "%d", choice + 1);
	else
		snprintf(text, sizeof(text), "own");
	ae_player_setting_put(player, "display.crosshair", text);
}

static void weapons_load(short player, struct ae_xh_weapons *table)
{
	char text[TEXT];

	text[0] = 0;
	ae_player_setting_get(player, "display.crosshair_weapons", text, sizeof(text));
	ae_xh_weapons_parse(text, table);
}

static int weapons_save(short player, const struct ae_xh_weapons *table)
{
	char text[TEXT];

	ae_xh_weapons_format(table, text, sizeof(text));
	return ae_player_setting_put(player, "display.crosshair_weapons", text);
}

int ae_crosshair_edit_saved(short player)
{
	struct ae_crosshair xh;
	int slot, count = 0;

	for (slot = 0; valid_player(player) && slot < AE_XH_SLOTS; slot++)
		if (slot_load(player, slot, &xh))
			count++;
	return count;
}

int ae_crosshair_edit_owns(const char *key)
{
	return key && (!strncmp(key, "xh.", 3) || !strncmp(key, "xw.", 3));
}

int ae_crosshair_edit_slot(short player)
{
	return valid_player(player) ? edit_slot[player] : 0;
}

int ae_crosshair_edit_open(short player)
{
	struct ae_crosshair xh;
	int slot, choice;

	if (!valid_player(player))
		return 0;
	choice = main_choice(player);
	if (choice >= 0 && slot_load(player, choice, &xh))
		slot = choice;
	else
	{
		slot = 0;
		for (choice = 0; choice < AE_XH_SLOTS; choice++)
			if (!slot_load(player, choice, &xh))
			{
				slot = choice;
				break;
			}
	}
	edit_slot[player] = (short)slot;
	platform_log("ae crosshair: editing slot %d (player %d)", slot + 1, (int)player + 1);
	return slot;
}

int ae_crosshair_edit_current(short player, struct ae_crosshair *xh)
{
	int slot = ae_crosshair_edit_slot(player);

	if (valid_player(player) && slot_load(player, slot, xh))
		return 1;
	new_crosshair(slot, xh);
	return 0;
}

int ae_crosshair_edit_for_weapon(short player, const char *key, struct ae_crosshair *xh)
{
	struct ae_xh_weapons table;
	int choice;

	if (!valid_player(player))
		return 0;
	weapons_load(player, &table);
	choice = key ? ae_xh_weapons_get(&table, key) : AE_XH_CHOICE_MAIN;
	if (choice == AE_XH_CHOICE_MAIN)
		choice = main_choice(player);
	return choice >= 0 && slot_load(player, choice, xh);
}

/* the editor's crosshair changed: saved to its slot; the first one made becomes the main one */
static void editor_save(short player, const struct ae_crosshair *xh)
{
	int slot = edit_slot[player], was_saved;
	struct ae_crosshair before;

	was_saved = slot_load(player, slot, &before);
	slot_save(player, slot, xh);
	if (!was_saved && main_choice(player) == AE_XH_CHOICE_OWN)
	{
		main_put(player, slot);
		platform_log("ae crosshair: slot %d made, and your main crosshair (player %d)", slot + 1, (int)player + 1);
	}
	ae_crosshair_edit_lists(player);
}

/* ---------- the rows */

static unsigned char *field_of(struct ae_crosshair *xh, const char *name, int *boolean)
{
	*boolean = 0;
	if (!strcmp(name, "opacity")) return &xh->opacity;
	if (!strcmp(name, "centre")) return &xh->centre;
	if (!strcmp(name, "centre_size")) return &xh->centre_size;
	if (!strcmp(name, "inner_on")) { *boolean = 1; return &xh->inner.on; }
	if (!strcmp(name, "inner_length")) return &xh->inner.length;
	if (!strcmp(name, "inner_thickness")) return &xh->inner.thickness;
	if (!strcmp(name, "inner_offset")) return &xh->inner.offset;
	if (!strcmp(name, "inner_opacity")) return &xh->inner.opacity;
	if (!strcmp(name, "outer_on")) { *boolean = 1; return &xh->outer.on; }
	if (!strcmp(name, "outer_length")) return &xh->outer.length;
	if (!strcmp(name, "outer_thickness")) return &xh->outer.thickness;
	if (!strcmp(name, "outer_offset")) return &xh->outer.offset;
	if (!strcmp(name, "outer_opacity")) return &xh->outer.opacity;
	if (!strcmp(name, "frame_on")) { *boolean = 1; return &xh->frame_on; }
	if (!strcmp(name, "frame_size")) return &xh->frame_size;
	if (!strcmp(name, "frame_thickness")) return &xh->frame_thickness;
	if (!strcmp(name, "frame_opacity")) return &xh->frame_opacity;
	if (!strcmp(name, "outline_on")) { *boolean = 1; return &xh->outline_on; }
	if (!strcmp(name, "outline_thickness")) return &xh->outline_thickness;
	if (!strcmp(name, "outline_opacity")) return &xh->outline_opacity;
	if (!strcmp(name, "movement")) { *boolean = 1; return &xh->movement; }
	if (!strcmp(name, "firing")) { *boolean = 1; return &xh->firing; }
	return NULL;
}

int ae_crosshair_edit_get(short player, const char *key, char *text, int size)
{
	struct ae_crosshair xh;
	int boolean;
	unsigned char *field;

	if (!valid_player(player) || !ae_crosshair_edit_owns(key))
		return 0;
	if (!strcmp(key, "xh.main"))
	{
		int choice = main_choice(player);

		if (choice >= 0 && slot_load(player, choice, &xh))
			snprintf(text, (size_t)size, "%d", choice + 1);
		else
			snprintf(text, (size_t)size, "own");
		return 1;
	}
	if (!strncmp(key, "xw.", 3))
	{
		struct ae_xh_weapons table;
		int choice;

		weapons_load(player, &table);
		choice = ae_xh_weapons_get(&table, key + 3);
		if (choice >= 0 && !slot_load(player, choice, &xh))
			choice = AE_XH_CHOICE_MAIN;
		if (choice == AE_XH_CHOICE_MAIN)
			snprintf(text, (size_t)size, "main");
		else if (choice == AE_XH_CHOICE_OWN)
			snprintf(text, (size_t)size, "own");
		else
			snprintf(text, (size_t)size, "%d", choice + 1);
		return 1;
	}
	ae_crosshair_edit_current(player, &xh);
	if (!strcmp(key, "xh.color_hex"))
	{
		ae_xh_hex_format(((unsigned long)xh.color[0] << 16) | ((unsigned long)xh.color[1] << 8) | xh.color[2], text,
			(size_t)size);
		return 1;
	}
	if (!strcmp(key, "xh.color"))
	{
		int index = ae_xh_palette_of(&xh);

		if (index >= 0)
			snprintf(text, (size_t)size, "%d", index);
		else
			snprintf(text, (size_t)size, "custom");
		return 1;
	}
	field = field_of(&xh, key + 3, &boolean);
	if (!field)
		return 0;
	if (boolean)
		snprintf(text, (size_t)size, "%s", *field ? "true" : "false");
	else
		snprintf(text, (size_t)size, "%d", (int)*field);
	return 1;
}

int ae_crosshair_edit_set(short player, const char *key, const char *text)
{
	struct ae_crosshair xh;
	int boolean;
	unsigned char *field;

	if (!valid_player(player) || !ae_crosshair_edit_owns(key) || !text)
		return 0;
	if (!strcmp(key, "xh.main"))
	{
		int choice = !strcmp(text, "own") ? AE_XH_CHOICE_OWN : atoi(text) - 1;

		if (choice != AE_XH_CHOICE_OWN && !slot_load(player, choice, &xh))
			return 0;
		main_put(player, choice);
		return 1;
	}
	if (!strncmp(key, "xw.", 3))
	{
		struct ae_xh_weapons table;
		int choice = !strcmp(text, "main") ? AE_XH_CHOICE_MAIN : !strcmp(text, "own") ? AE_XH_CHOICE_OWN : atoi(text) - 1;

		if (choice >= 0 && !slot_load(player, choice, &xh))
			return 0;
		weapons_load(player, &table);
		if (!ae_xh_weapons_set(&table, key + 3, choice))
			return 0;
		return weapons_save(player, &table);
	}
	ae_crosshair_edit_current(player, &xh);
	if (!strcmp(key, "xh.color_hex"))
	{
		/* (CUSTOM COLOR: six hex digits, nothing else) */
		unsigned long rgb;

		if (!ae_xh_hex_parse(text, &rgb))
			return 0;
		ae_xh_color_set(&xh, rgb);
	}
	else if (!strcmp(key, "xh.color"))
	{
		/* (CUSTOM in the list keeps the colour typed in CUSTOM COLOR: no change) */
		if (!strcmp(text, "custom"))
			return 1;
		ae_xh_color_set(&xh, ae_xh_palette_rgb(atoi(text)));
	}
	else
	{
		field = field_of(&xh, key + 3, &boolean);
		if (!field)
			return 0;
		*field = (unsigned char)(boolean ? !strcmp(text, "true") : atoi(text));
	}
	ae_xh_clamp(&xh);
	editor_save(player, &xh);
	return 1;
}

int ae_crosshair_edit_default(const char *key, char *text, int size)
{
	struct ae_crosshair xh;
	int boolean;
	unsigned char *field;

	if (!ae_crosshair_edit_owns(key))
		return 0;
	if (!strcmp(key, "xh.main"))
		snprintf(text, (size_t)size, "own");
	else if (!strncmp(key, "xw.", 3))
		snprintf(text, (size_t)size, "main");
	else if (!strcmp(key, "xh.color"))
		snprintf(text, (size_t)size, "0");
	else if (!strcmp(key, "xh.color_hex"))
		ae_xh_hex_format(ae_xh_palette_rgb(0), text, (size_t)size);
	else
	{
		ae_xh_default(&xh);
		field = field_of(&xh, key + 3, &boolean);
		if (!field)
			return 0;
		if (boolean)
			snprintf(text, (size_t)size, "%s", *field ? "true" : "false");
		else
			snprintf(text, (size_t)size, "%d", (int)*field);
	}
	return 1;
}

void ae_crosshair_edit_summary(const struct ae_crosshair *xh, char *text, int size)
{
	int palette = ae_xh_palette_of(xh);
	char parts[96];

	snprintf(parts, sizeof(parts), "%s%s%s%s", xh->inner.on ? "INNER" : "", xh->inner.on && xh->outer.on ? " + " : "",
		xh->outer.on ? "OUTER LINES" : "", xh->frame_on ? (xh->inner.on || xh->outer.on ? ", FRAME" : "FRAME") : "");
	snprintf(text, (size_t)size, "%s, %s%s%s", xh->shape ? ae_xh_shape_name(xh->shape) : "NO SHAPE",
		palette >= 0 ? ae_xh_palette_name(palette) : "CUSTOM COLOR", parts[0] ? ", " : "", parts);
}

int ae_crosshair_edit_shown(short player, const char *key, char *text, int size)
{
	struct ae_crosshair xh;

	if (!valid_player(player) || !key)
		return 0;
	if (!strcmp(key, "action.crosshair_name"))
	{
		ae_crosshair_edit_current(player, &xh);
		snprintf(text, (size_t)size, "%s", xh.name);
		return 1;
	}
	if (!strcmp(key, "action.crosshair_hex"))
	{
		return ae_crosshair_edit_get(player, "xh.color_hex", text, size);
	}
	if (!strcmp(key, "action.crosshair_shape"))
	{
		ae_crosshair_edit_current(player, &xh);
		snprintf(text, (size_t)size, "%s", xh.shape ? ae_xh_shape_name(xh.shape) : "NONE");
		return 1;
	}
	if (!strcmp(key, "action.crosshair_edit"))
	{
		int choice = main_choice(player);

		if (choice >= 0 && slot_load(player, choice, &xh))
			snprintf(text, (size_t)size, "%s", xh.name);
		else
			snprintf(text, (size_t)size, "NEW");
		return 1;
	}
	if (!strcmp(key, "action.crosshair_weapons"))
	{
		struct ae_xh_weapons table;
		int own, custom;

		weapons_load(player, &table);
		ae_xh_weapons_summary(&table, &own, &custom);
		if (!own && !custom)
			snprintf(text, (size_t)size, "ALL USE MAIN");
		else
			snprintf(text, (size_t)size, "%d OWN, %d CUSTOM", own, custom);
		return 1;
	}
	if (!strcmp(key, "action.crosshair_code") || !strcmp(key, "action.crosshair_delete"))
	{
		text[0] = 0;
		return 1;
	}
	return 0;
}

void ae_crosshair_edit_lists(short player)
{
	static char value[AE_XH_SLOTS + 2][4], name[AE_XH_SLOTS + 2][24];
	const char *main_values[AE_XH_SLOTS + 1], *main_texts[AE_XH_SLOTS + 1], *weapon_values[AE_XH_SLOTS + 2],
		*weapon_texts[AE_XH_SLOTS + 2];
	int main_count = 0, weapon_count = 0, slot;
	struct ae_crosshair xh;

	if (!valid_player(player))
		return;
	main_values[main_count] = "own";
	main_texts[main_count++] = "STOCK";
	weapon_values[weapon_count] = "main";
	weapon_texts[weapon_count++] = "USE MAIN";
	weapon_values[weapon_count] = "own";
	weapon_texts[weapon_count++] = "STOCK";
	for (slot = 0; slot < AE_XH_SLOTS; slot++)
	{
		if (!slot_load(player, slot, &xh))
			continue;
		snprintf(value[slot], sizeof(value[slot]), "%u", (unsigned)(slot + 1) % 10u);
		snprintf(name[slot], sizeof(name[slot]), "%s", xh.name[0] ? xh.name : "CROSSHAIR");
		main_values[main_count] = value[slot];
		main_texts[main_count++] = name[slot];
		weapon_values[weapon_count] = value[slot];
		weapon_texts[weapon_count++] = name[slot];
	}
	/* (the saved ones' names in the log when they change: "ae crosshair: saved PRECISE, SNAP, OCTA (player 1)") */
	{
		static char logged[PLAYERS][128];
		char names[128];
		int at = 0, index;

		names[0] = 0;
		for (index = 2; index < weapon_count && at < (int)sizeof(names) - 1; index++)
			at += snprintf(names + at, sizeof(names) - (size_t)at, "%s%s", index > 2 ? ", " : "", weapon_texts[index]);
		if (strcmp(names, logged[player]))
		{
			snprintf(logged[player], sizeof(logged[player]), "%s", names);
			platform_log("ae crosshair: saved %s (player %d)", names[0] ? names : "none", (int)player + 1);
		}
	}
	ae_settings_set_values("xh.main", main_texts, main_values, (short)main_count);
	ae_settings_set_values("xw.list", weapon_texts, weapon_values, (short)weapon_count);
}

int ae_crosshair_edit_name_set(short player, const char *typed)
{
	struct ae_crosshair xh;
	char name[AE_XH_NAME + 1];

	if (!valid_player(player) || !ae_xh_name_clean(typed, name, sizeof(name)))
		return 0;
	ae_crosshair_edit_current(player, &xh);
	memset(xh.name, 0, sizeof(xh.name));
	memcpy(xh.name, name, strlen(name));
	editor_save(player, &xh);
	return 1;
}

int ae_crosshair_edit_shape_set(short player, int shape)
{
	struct ae_crosshair xh;

	if (!valid_player(player) || shape < 0 || shape > ae_xh_shape_count())
		return 0;
	ae_crosshair_edit_current(player, &xh);
	ae_xh_apply_shape(&xh, shape);
	editor_save(player, &xh);
	return 1;
}

int ae_crosshair_edit_delete(short player)
{
	struct ae_crosshair xh;
	struct ae_xh_weapons table;
	char key[40];
	int slot, index;

	if (!valid_player(player))
		return 0;
	slot = edit_slot[player];
	if (!slot_load(player, slot, &xh))
		return 0;
	slot_key(slot, key, sizeof(key));
	ae_player_setting_put(player, key, "");
	if (main_choice(player) == slot)
		main_put(player, AE_XH_CHOICE_OWN);
	weapons_load(player, &table);
	for (index = table.count - 1; index >= 0; index--)
		if (table.entry[index].choice == slot)
		{
			char weapon[AE_XH_WEAPON_KEY];

			memcpy(weapon, table.entry[index].key, sizeof(weapon));
			ae_xh_weapons_set(&table, weapon, AE_XH_CHOICE_MAIN);
		}
	weapons_save(player, &table);
	ae_crosshair_edit_lists(player);
	platform_log("ae crosshair: slot %d deleted (player %d)", slot + 1, (int)player + 1);
	/* (leave the deleted slot: the editor moves to a crosshair that is still saved, so the next edit never makes the deleted
	one again; none left: the slot stays and the screen goes back, ae_screen_settings.c) */
	for (index = 0; index < AE_XH_SLOTS; index++)
		if (slot_load(player, index, &xh))
		{
			edit_slot[player] = (short)index;
			platform_log("ae crosshair: editing slot %d (player %d)", index + 1, (int)player + 1);
			break;
		}
	return 1;
}

int ae_crosshair_edit_slot_text(short player, char *text, int size)
{
	char key[40];

	text[0] = 0;
	if (!valid_player(player))
		return 0;
	slot_key(edit_slot[player], key, sizeof(key));
	ae_player_setting_get(player, key, text, size);
	return text[0] != 0;
}

int ae_crosshair_edit_slot_restore(short player, const char *text)
{
	char key[40];
	int slot;

	if (!valid_player(player) || !text)
		return 0;
	slot = edit_slot[player];
	slot_key(slot, key, sizeof(key));
	if (!text[0])
	{
		/* (the slot was free when the editor opened: the edits that made it are undone with it) */
		ae_player_setting_put(player, key, "");
		if (main_choice(player) == slot)
			main_put(player, AE_XH_CHOICE_OWN);
	}
	else if (!ae_player_setting_put(player, key, text))
		return 0;
	ae_crosshair_edit_lists(player);
	return 1;
}

void ae_crosshair_edit_code(short player, char *text, int size)
{
	struct ae_crosshair xh;

	ae_crosshair_edit_current(player, &xh);
	ae_xh_code_encode(&xh, text, (size_t)size);
}

int ae_crosshair_edit_code_read(const char *text, struct ae_crosshair *xh, int *unknown_shape)
{
	memset(xh, 0, sizeof(*xh));
	return ae_xh_code_decode(text, xh, unknown_shape);
}

int ae_crosshair_edit_code_apply(short player, const struct ae_crosshair *xh)
{
	struct ae_crosshair mine, made = *xh;

	if (!valid_player(player))
		return 0;
	ae_crosshair_edit_current(player, &mine);
	memcpy(made.name, mine.name, sizeof(made.name));
	ae_xh_clamp(&made);
	editor_save(player, &made);
	return 1;
}

int ae_crosshair_edit_code_save_new(short player, const struct ae_crosshair *xh)
{
	struct ae_crosshair probe, made = *xh;
	int slot;

	if (!valid_player(player))
		return -1;
	for (slot = 0; slot < AE_XH_SLOTS; slot++)
		if (!slot_load(player, slot, &probe))
		{
			char name[AE_XH_NAME + 1];

			snprintf(name, sizeof(name), "PASTED %u", (unsigned)(slot + 1) % 10u);
			memset(made.name, 0, sizeof(made.name));
			memcpy(made.name, name, strlen(name));
			edit_slot[player] = (short)slot;
			editor_save(player, &made);
			return slot;
		}
	return -1;
}

/* ---------- (0.7.7) the CROSSHAIR screen's saved strip */

int ae_crosshair_edit_slot_saved(short player, int slot, struct ae_crosshair *xh)
{
	return valid_player(player) && slot_load(player, slot, xh);
}

int ae_crosshair_edit_main(short player)
{
	struct ae_crosshair xh;
	int choice;

	if (!valid_player(player))
		return AE_XH_CHOICE_OWN;
	choice = main_choice(player);
	return choice >= 0 && slot_load(player, choice, &xh) ? choice : AE_XH_CHOICE_OWN;
}

int ae_crosshair_edit_select(short player, int slot)
{
	if (!valid_player(player) || slot < 0 || slot >= AE_XH_SLOTS)
		return 0;
	edit_slot[player] = (short)slot;
	platform_log("ae crosshair: editing slot %d (player %d)", slot + 1, (int)player + 1);
	return 1;
}

static int free_slot(short player)
{
	struct ae_crosshair probe;
	int slot;

	for (slot = 0; slot < AE_XH_SLOTS; slot++)
		if (!slot_load(player, slot, &probe))
			return slot;
	return -1;
}

int ae_crosshair_edit_new(short player)
{
	struct ae_crosshair xh;
	int slot;

	if (!valid_player(player) || (slot = free_slot(player)) < 0)
		return -1;
	new_crosshair(slot, &xh);
	edit_slot[player] = (short)slot;
	editor_save(player, &xh);
	platform_log("ae crosshair: new %s in slot %d (player %d)", xh.name, slot + 1, (int)player + 1);
	return slot;
}

void ae_crosshair_edit_copy_name(const char *name, const char *const *taken, int taken_count, char *out, int size)
{
	char stem[AE_XH_NAME + 1], made[AE_XH_NAME + 16];
	int length, number, index, used;

	snprintf(stem, sizeof(stem), "%s", name && name[0] ? name : "CROSSHAIR");
	length = (int)strlen(stem);
	/* ("OCTA 2" goes on from its number: "OCTA 3") */
	number = 2;
	if (length >= 2 && stem[length - 2] == ' ' && stem[length - 1] >= '1' && stem[length - 1] <= '9')
	{
		number = stem[length - 1] - '0' + 1;
		stem[length - 2] = 0;
	}
	for (; number <= 99; number++)
	{
		char digits[12];
		int room;

		snprintf(digits, sizeof(digits), "%d", number);
		room = AE_XH_NAME - 1 - (int)strlen(digits);
		{
			char cut[AE_XH_NAME + 1];
			int end;

			/* (a cut stem keeps no space at its end) */
			snprintf(cut, sizeof(cut), "%.*s", room, stem);
			for (end = (int)strlen(cut); end > 0 && cut[end - 1] == ' '; end--)
				cut[end - 1] = 0;
			snprintf(made, sizeof(made), "%s %s", cut, digits);
		}
		used = 0;
		for (index = 0; index < taken_count; index++)
			if (taken[index] && !strcmp(taken[index], made))
				used = 1;
		if (!used)
			break;
	}
	snprintf(out, (size_t)size, "%s", made);
}

int ae_crosshair_edit_duplicate(short player)
{
	struct ae_crosshair xh, other;
	char names[AE_XH_SLOTS][AE_XH_NAME + 1];
	const char *taken[AE_XH_SLOTS];
	char name[AE_XH_NAME + 8];
	int slot, index, count = 0;

	if (!valid_player(player) || !slot_load(player, edit_slot[player], &xh) || (slot = free_slot(player)) < 0)
		return -1;
	for (index = 0; index < AE_XH_SLOTS; index++)
		if (slot_load(player, index, &other))
		{
			snprintf(names[count], sizeof(names[count]), "%s", other.name);
			taken[count] = names[count];
			count++;
		}
	ae_crosshair_edit_copy_name(xh.name, taken, count, name, sizeof(name));
	memset(xh.name, 0, sizeof(xh.name));
	ae_xh_name_clean(name, xh.name, sizeof(xh.name));
	edit_slot[player] = (short)slot;
	editor_save(player, &xh);
	platform_log("ae crosshair: duplicate %s in slot %d (player %d)", xh.name, slot + 1, (int)player + 1);
	return slot;
}

int ae_crosshair_edit_weapons_using(short player, int slot, char (*keys)[AE_XH_WEAPON_KEY], int maximum)
{
	struct ae_xh_weapons table;
	int index, count = 0;

	if (!valid_player(player))
		return 0;
	weapons_load(player, &table);
	for (index = 0; index < table.count; index++)
		if (table.entry[index].choice == slot)
		{
			if (keys && count < maximum)
				memcpy(keys[count], table.entry[index].key, AE_XH_WEAPON_KEY);
			count++;
		}
	return count;
}

void ae_crosshair_edit_profile_take(short player, struct ae_crosshair_edit_profile *profile)
{
	char key[40];
	int slot;

	memset(profile, 0, sizeof(*profile));
	if (!valid_player(player))
		return;
	for (slot = 0; slot < AE_XH_SLOTS; slot++)
	{
		slot_key(slot, key, sizeof(key));
		ae_player_setting_get(player, key, profile->slot[slot], sizeof(profile->slot[slot]));
	}
	ae_player_setting_get(player, "display.crosshair", profile->main, sizeof(profile->main));
	ae_player_setting_get(player, "display.crosshair_weapons", profile->weapons, sizeof(profile->weapons));
}

int ae_crosshair_edit_profile_same(const struct ae_crosshair_edit_profile *a, const struct ae_crosshair_edit_profile *b)
{
	int slot;

	for (slot = 0; slot < AE_XH_SLOTS; slot++)
		if (strcmp(a->slot[slot], b->slot[slot]))
			return 0;
	return !strcmp(a->main, b->main) && !strcmp(a->weapons, b->weapons);
}

int ae_crosshair_edit_profile_restore(short player, const struct ae_crosshair_edit_profile *profile)
{
	struct ae_crosshair_edit_profile now;
	char key[40];
	int slot, ok = 1;

	if (!valid_player(player))
		return 0;
	ae_crosshair_edit_profile_take(player, &now);
	for (slot = 0; slot < AE_XH_SLOTS; slot++)
	{
		if (!strcmp(now.slot[slot], profile->slot[slot]))
			continue;
		slot_key(slot, key, sizeof(key));
		ok &= ae_player_setting_put(player, key, profile->slot[slot]);
	}
	if (strcmp(now.main, profile->main))
		ok &= ae_player_setting_put(player, "display.crosshair", profile->main);
	if (strcmp(now.weapons, profile->weapons))
		ok &= ae_player_setting_put(player, "display.crosshair_weapons", profile->weapons);
	ae_crosshair_edit_lists(player);
	return ok;
}
