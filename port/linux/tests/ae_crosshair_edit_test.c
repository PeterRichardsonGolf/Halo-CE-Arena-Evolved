/* ae_crosshair_edit_test.c: the EDIT CROSSHAIR rows (ae_crosshair_edit.c) against an in-memory profile: every row set to a new
value changes its own field of the saved crosshair and no other (the share code too: set, encode, decode); a weapons table
longer than 159 characters is kept; the lists are built for the player asked; DELETE leaves its slot; UNDO's whole-slot restore. */

#include <stdio.h>
#include <string.h>


#include "ae_crosshair_edit.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

/* ---------- the stand-ins: the profile (a value up to 1023 characters, as the game's), the lists, the log */
static struct { char name[64]; char value[1024]; int used; } store[4][32];
static char list_main[4][512], list_weapons[4][512];
static int list_calls;

void platform_log(char const *format, ...) { (void)format; }

int ae_player_setting_get(short player, const char *name, char *text, int size)
{
	int index;

	text[0] = 0;
	for (index = 0; index < 32; index++)
		if (store[player][index].used && !strcmp(store[player][index].name, name))
		{
			snprintf(text, (size_t)size, "%s", store[player][index].value);
			return 1;
		}
	return 0;
}

int ae_player_setting_put(short player, const char *name, const char *value)
{
	int index, free_at = -1;

	if (strlen(value) >= sizeof(store[0][0].value))
		return 0;
	for (index = 0; index < 32; index++)
	{
		if (store[player][index].used && !strcmp(store[player][index].name, name))
			break;
		if (!store[player][index].used && free_at < 0)
			free_at = index;
	}
	if (index == 32)
		index = free_at;
	snprintf(store[player][index].name, sizeof(store[0][0].name), "%s", name);
	snprintf(store[player][index].value, sizeof(store[0][0].value), "%s", value);
	store[player][index].used = 1;
	return 1;
}

static short last_player;
int ae_settings_set_values(const char *key, const char *const *texts, const char *const *values, short count)
{
	char *into = !strcmp(key, "xh.main") ? list_main[last_player] : list_weapons[last_player];
	int index, at = 0;

	list_calls++;
	into[0] = 0;
	for (index = 0; index < count; index++)
		at += snprintf(into + at, 512 - (size_t)at, "%s=%s;", values[index], texts[index]);
	(void)texts;
	return 1;
}

static void reset(void)
{
	memset(store, 0, sizeof(store));
	memset(list_main, 0, sizeof(list_main));
	memset(list_weapons, 0, sizeof(list_weapons));
	list_calls = 0;
}

/* a row key and a value that is not the default's */
static const struct { const char *key, *value; } rows[] =
{
	{ "xh.color", "3" }, { "xh.color_hex", "#FF8040" }, { "xh.opacity", "55" }, { "xh.centre", "2" }, { "xh.centre_size", "6" },
	{ "xh.inner_on", "false" }, { "xh.inner_length", "9" }, { "xh.inner_thickness", "4" }, { "xh.inner_offset", "7" },
	{ "xh.inner_opacity", "45" }, { "xh.outer_on", "true" }, { "xh.outer_length", "11" }, { "xh.outer_thickness", "3" },
	{ "xh.outer_offset", "9" }, { "xh.outer_opacity", "35" }, { "xh.frame_on", "true" }, { "xh.frame_size", "125" },
	{ "xh.frame_thickness", "6" }, { "xh.frame_opacity", "65" }, { "xh.outline_on", "false" }, { "xh.outline_thickness", "5" },
	{ "xh.outline_opacity", "75" }, { "xh.movement", "false" }, { "xh.firing", "false" }
};
#define ROWS ((int)(sizeof(rows) / sizeof(rows[0])))

static int same_but_name(const struct ae_crosshair *a, const struct ae_crosshair *b)
{
	struct ae_crosshair x = *a, y = *b;

	memset(x.name, 0, sizeof(x.name));
	memset(y.name, 0, sizeof(y.name));
	return !memcmp(&x, &y, sizeof(x));
}

int main(void)
{
	int index, other;
	char text[1100], before[ROWS][32], after[32], code[AE_XH_CODE_TEXT];

	reset();
	/* every row, set to a new value: its own value changes to it (a colour's two rows together) and no other row's does; the
	crosshair's share code decodes to what the rows read back as */
	for (index = 0; index < ROWS; index++)
	{
		struct ae_crosshair made, read;
		int unknown;

		reset();
		ae_crosshair_edit_open(0);
		for (other = 0; other < ROWS; other++)
			ae_crosshair_edit_get(0, rows[other].key, before[other], sizeof(before[other]));
		CHECK(ae_crosshair_edit_set(0, rows[index].key, rows[index].value));
		for (other = 0; other < ROWS; other++)
		{
			int colour_pair = (!strncmp(rows[index].key, "xh.color", 8) && !strncmp(rows[other].key, "xh.color", 8));

			ae_crosshair_edit_get(0, rows[other].key, after, sizeof(after));
			if (other == index || colour_pair)
			{
				if (other == index && strcmp(rows[index].key, "xh.color_hex"))
					CHECK(!strcmp(after, rows[index].value));
				continue;
			}
			if (strcmp(after, before[other]))
				printf("FAIL setting %s changed %s: %s -> %s\n", rows[index].key, rows[other].key, before[other], after);
			CHECK(!strcmp(after, before[other]));
		}
		ae_crosshair_edit_current(0, &made);
		ae_crosshair_edit_code(0, code, sizeof(code));
		CHECK(ae_crosshair_edit_code_read(code, &read, &unknown) == AE_XH_CODE_OK);
		CHECK(same_but_name(&made, &read));
	}

	/* a weapons table over 159 characters is kept whole, and read back the same */
	{
		static const char *const keys[] = { "pistol", "assault_rifle", "shotgun", "sniper_rifle", "rocket_launcher",
			"plasma_pistol", "plasma_rifle", "needler", "flamethrower", "gravity_rifle", "warthog_gun", "scorpion_cannon",
			"ghost_gun", "battle_rifle", "dmr", "evo_rifle", "sidearm", "magnum_scoped", "brute_shot", "carbine", "smg" };
		char key[64], value[8], back[1100];

		reset();
		ae_crosshair_edit_open(0);
		ae_crosshair_edit_set(0, "xh.opacity", "55");
		for (index = 0; index < (int)(sizeof(keys) / sizeof(keys[0])); index++)
		{
			snprintf(key, sizeof(key), "xw.%s", keys[index]);
			CHECK(ae_crosshair_edit_set(0, key, "1"));
		}
		ae_player_setting_get(0, "display.crosshair_weapons", back, sizeof(back));
		CHECK(strlen(back) > 159);
		for (index = 0; index < (int)(sizeof(keys) / sizeof(keys[0])); index++)
		{
			snprintf(key, sizeof(key), "xw.%s", keys[index]);
			CHECK(ae_crosshair_edit_get(0, key, value, sizeof(value)) && !strcmp(value, "1"));
		}
	}

	/* the lists are built for the player asked: player 2's saved crosshairs are not player 1's */
	{
		reset();
		ae_crosshair_edit_open(0);
		ae_crosshair_edit_set(0, "xh.opacity", "55");
		ae_crosshair_edit_open(1);
		ae_crosshair_edit_set(1, "xh.opacity", "60");
		ae_crosshair_edit_name_set(1, "SECOND");
		last_player = 0;
		ae_crosshair_edit_lists(0);
		last_player = 1;
		ae_crosshair_edit_lists(1);
		CHECK(!strstr(list_main[0], "SECOND"));
		CHECK(strstr(list_main[1], "SECOND"));
		CHECK(ae_crosshair_edit_saved(0) == 1 && ae_crosshair_edit_saved(1) == 1);
	}

	/* DELETE leaves its slot: the editor moves to a crosshair still saved, and a later edit never makes the deleted one again */
	{
		reset();
		ae_crosshair_edit_open(0);
		ae_crosshair_edit_set(0, "xh.opacity", "55");
		ae_crosshair_edit_name_set(0, "KEEPER");
		/* a second one, made as a new crosshair */
		store[0][0].used = store[0][0].used;
		ae_player_setting_put(0, "display.crosshair_2", "SECOND|AE1-0426-QXFR-Z9J0-J1G9-CG7G-C6SW-CG36-80R2-A3NQ-C");
		ae_crosshair_edit_open(0);
		CHECK(ae_crosshair_edit_saved(0) == 2);
		{
			int deleted = ae_crosshair_edit_slot(0);

			CHECK(ae_crosshair_edit_delete(0));
			CHECK(ae_crosshair_edit_slot(0) != deleted);
			ae_crosshair_edit_set(0, "xh.opacity", "40");
			snprintf(text, sizeof(text), "display.crosshair_%d", deleted + 1);
			ae_player_setting_get(0, text, before[0], sizeof(before[0]));
			CHECK(before[0][0] == 0);
		}
		/* none left: deleting the last leaves none saved (the screen then goes back) */
		CHECK(ae_crosshair_edit_delete(0));
		CHECK(ae_crosshair_edit_saved(0) == 0);
	}

	/* UNDO's whole slot: the name and the shape come back with the rows */
	{
		char opened[1100], now[1100];

		reset();
		ae_crosshair_edit_open(0);
		ae_crosshair_edit_set(0, "xh.opacity", "55");
		ae_crosshair_edit_name_set(0, "FIRST");
		ae_crosshair_edit_slot_text(0, opened, sizeof(opened));
		CHECK(opened[0]);
		ae_crosshair_edit_name_set(0, "OTHER");
		ae_crosshair_edit_shape_set(0, 3);
		ae_crosshair_edit_set(0, "xh.frame_size", "90");
		ae_crosshair_edit_slot_text(0, now, sizeof(now));
		CHECK(strcmp(now, opened));
		CHECK(ae_crosshair_edit_slot_restore(0, opened));
		ae_crosshair_edit_slot_text(0, now, sizeof(now));
		CHECK(!strcmp(now, opened));
		/* a slot that was free: restoring "" frees it again */
		reset();
		ae_crosshair_edit_open(0);
		ae_crosshair_edit_set(0, "xh.opacity", "55");
		CHECK(ae_crosshair_edit_saved(0) == 1);
		CHECK(ae_crosshair_edit_slot_restore(0, ""));
		CHECK(ae_crosshair_edit_saved(0) == 0);
	}

	/* (0.7.7 CROSSHAIR screen) NEW is a fresh default crosshair in the first free slot; DUPLICATE copies the edited one as
	"<NAME> 2" (then 3), both edited at once; all 8 used: -1; select; the whole-profile snapshot puts everything back */
	{
		struct ae_crosshair_edit_profile before, after;
		struct ae_crosshair xh, other;
		const char *taken[] = { "OCTA", "OCTA 2" };
		char name[32];
		int slot;

		reset();
		ae_crosshair_edit_open(0);
		ae_crosshair_edit_profile_take(0, &before);
		slot = ae_crosshair_edit_new(0);
		CHECK(slot == 0 && ae_crosshair_edit_slot(0) == 0 && ae_crosshair_edit_saved(0) == 1);
		CHECK(ae_crosshair_edit_slot_saved(0, 0, &xh) && !strcmp(xh.name, "CROSSHAIR 1"));
		CHECK(ae_crosshair_edit_main(0) == 0);
		ae_crosshair_edit_name_set(0, "OCTA");
		ae_crosshair_edit_set(0, "xh.opacity", "55");
		slot = ae_crosshair_edit_duplicate(0);
		CHECK(slot == 1 && ae_crosshair_edit_slot(0) == 1);
		CHECK(ae_crosshair_edit_slot_saved(0, 1, &other) && !strcmp(other.name, "OCTA 2") && other.opacity == 55);
		CHECK(ae_crosshair_edit_duplicate(0) == 2 && ae_crosshair_edit_slot_saved(0, 2, &other) && !strcmp(other.name, "OCTA 3"));
		CHECK(ae_crosshair_edit_main(0) == 0);
		CHECK(ae_crosshair_edit_select(0, 0) && ae_crosshair_edit_slot(0) == 0);
		while (ae_crosshair_edit_new(0) >= 0)
			;
		CHECK(ae_crosshair_edit_saved(0) == 8 && ae_crosshair_edit_new(0) == -1 && ae_crosshair_edit_duplicate(0) == -1);
		ae_crosshair_edit_profile_take(0, &after);
		CHECK(!ae_crosshair_edit_profile_same(&before, &after));
		CHECK(ae_crosshair_edit_profile_restore(0, &before));
		ae_crosshair_edit_profile_take(0, &after);
		CHECK(ae_crosshair_edit_profile_same(&before, &after) && ae_crosshair_edit_saved(0) == 0);
		ae_crosshair_edit_copy_name("OCTA", taken, 2, name, sizeof(name));
		CHECK(!strcmp(name, "OCTA 3"));
		ae_crosshair_edit_copy_name("TWIN CROSS X", NULL, 0, name, sizeof(name));
		CHECK(!strcmp(name, "TWIN CROSS 2") && strlen(name) <= 12);
		ae_crosshair_edit_copy_name("ABCD EFGH IJ", NULL, 0, name, sizeof(name));
		CHECK(!strcmp(name, "ABCD EFGH 2"));
		/* the weapons naming a slot */
		reset();
		ae_crosshair_edit_open(0);
		ae_crosshair_edit_new(0);
		ae_crosshair_edit_set(0, "xw.battle_rifle", "1");
		ae_crosshair_edit_set(0, "xw.pistol", "own");
		{
			char keys[4][AE_XH_WEAPON_KEY];

			CHECK(ae_crosshair_edit_weapons_using(0, 0, keys, 4) == 1 && !strcmp(keys[0], "battle_rifle"));
		}
	}

	if (failures)
	{
		printf("%d failure(s)\n", failures);
		return 1;
	}
	printf("ae_crosshair_edit: ok\n");
	return 0;
}
