/*
AE_MODS_LIST_TEST.C

The mods list (ae_mods_list.c, ae_mods_list_rules.c) over made-up mods folders: a struct ae_mods_source of fake mods
with their mod.toml fields, their files, a load order the engine's moves change, a store that can refuse to be
written, and a map that plays mods or not.
*/

#include "ae_mods_list.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int checks, failures;

#define CHECK(condition) \
	do \
	{ \
		checks++; \
		if (!(condition)) \
		{ \
			failures++; \
			printf("%s:%d: CHECK(%s) failed\n", __FILE__, __LINE__, #condition); \
		} \
	} while (0)

/* ---------- a made-up mods folder */

struct fake_mod
{
	const char *folder, *name, *id, *version, *authors, *description, *refusal;
	const char *maps[3];
	int off, damaged;
	const char *files[6];
	/* its engine status line (1 out of order, 2 another says it cannot be played with it) */
	int status;
	const char *status_text;
};

static struct fake_mod mods[8];
static long mod_count, ordered[8];
static int store_writable, map_active, saves, applies, scanned_played[8];

static long fake_count(void) { return mod_count; }
static long fake_at(long position) { return position >= 0 && position < mod_count ? ordered[position] : -1; }
static const char *fake_folder(long index) { return mods[index].folder; }
static const char *fake_name(long index) { return mods[index].name ? mods[index].name : mods[index].folder; }

static int fake_fields(long index, const char **id, const char **version, const char **authors,
	const char **description, const char **refusal, long *map_count, long *played_place, long *file_count, int *flags)
{
	long map = 0, file = 0;

	if (index < 0 || index >= mod_count)
		return 0;
	*id = mods[index].id ? mods[index].id : mods[index].folder;
	*version = mods[index].version ? mods[index].version : "";
	*authors = mods[index].authors ? mods[index].authors : "";
	*description = mods[index].description ? mods[index].description : "";
	*refusal = mods[index].refusal;
	while (map < 3 && mods[index].maps[map])
		map++;
	while (file < 6 && mods[index].files[file])
		file++;
	*map_count = map;
	*played_place = scanned_played[index] ? 0 : -1;
	*file_count = file;
	*flags = (mods[index].off ? 1 : 0) | (mods[index].damaged ? 2 : 0);
	return 1;
}

static const char *fake_map(long index, long map) { return map < 3 ? mods[index].maps[map] : NULL; }

static int fake_status(long index, char *text, unsigned long size)
{
	if (mods[index].status && text)
		snprintf(text, size, "%s", mods[index].status_text);
	return mods[index].status;
}

/* the files of the mods played (as the engine lists them: those on, not refused) */
static long file_mod[64];
static const char *file_key[64];
static long file_total;

static void fake_scan(void)
{
	long index, file;

	file_total = 0;
	for (index = 0; index < mod_count; index++)
	{
		scanned_played[index] = !mods[index].off && !mods[index].refusal;
		for (file = 0; scanned_played[index] && file < 6 && mods[index].files[file]; file++)
		{
			file_mod[file_total] = index;
			file_key[file_total++] = mods[index].files[file];
		}
	}
}

static long fake_files(void) { return file_total; }
static const char *fake_file_key(long file, long *mod) { *mod = file_mod[file]; return file_key[file]; }

static long fake_move(long position, long places)
{
	long target = position + places, mod = ordered[position], index;

	if (target < 0)
		target = 0;
	if (target >= mod_count)
		target = mod_count - 1;
	if (target > position)
		for (index = position; index < target; index++)
			ordered[index] = ordered[index + 1];
	else
		for (index = position; index > target; index--)
			ordered[index] = ordered[index - 1];
	ordered[target] = mod;
	return target;
}

static void fake_turn(long index, int on) { mods[index].off = !on; }
static int fake_save(void) { saves++; return store_writable; }
static void fake_apply(void) { applies++; fake_scan(); }
static int fake_active(void) { return map_active; }

static const struct ae_mods_source fake =
{
	fake_count, fake_at, fake_folder, fake_name, fake_fields, fake_map, fake_status, fake_files, fake_file_key,
	fake_move, fake_turn, fake_save, fake_apply, fake_active,
};

/* the folder: hd_audio (sounds, one also in voices), voices (sounds), german (text), later (made for a newer format),
broken (mod.toml unreadable, a .weapon file the game never reads from a mod), in that load order */
static void folder_make(void)
{
	static const struct fake_mod made[] =
	{
		{ "hd_audio", "High-res audio", "hdaudio", "1.0", "Someone, Someone else",
			"The game's sounds\n\n\tat 44 kHz\x01", NULL, { NULL }, 0, 0,
			{ "sound\\sfx\\ar\\fire.sound", "sound\\sfx\\pistol\\fire.sound", NULL }, 0, NULL },
		{ "voices", NULL, NULL, NULL, NULL, NULL, NULL, { "bloodgulch", "a10", NULL }, 0, 0,
			{ "sound\\sfx\\ar\\fire.sound", "sound\\dialog\\hello.sound", NULL }, 0, NULL },
		{ "german", "Deutsch", NULL, "2", NULL, NULL, NULL, { NULL }, 0, 0,
			{ "ui\\hud\\hud_item_messages.unicode_string_list", NULL }, 2,
			"Voices says it cannot be played with this mod." },
		{ "later", NULL, NULL, NULL, NULL, NULL, "made for a later build", { NULL }, 0, 0,
			{ "sound\\sfx\\ar\\fire.sound", NULL }, 0, NULL },
		{ "broken", NULL, NULL, NULL, NULL, NULL, NULL, { NULL }, 0, 1,
			{ "weapons\\ar\\ar.weapon", "weapons\\ar\\ar.bitmap", NULL }, 0, NULL },
	};
	long index;

	mod_count = (long)(sizeof(made) / sizeof(made[0]));
	for (index = 0; index < mod_count; index++)
	{
		mods[index] = made[index];
		ordered[index] = index;
	}
	store_writable = 1;
	map_active = 1;
	saves = applies = 0;
	fake_scan();
}

/* the list's item of a folder */
static const struct ae_mods_item *item(const struct ae_mods_list *list, const char *name)
{
	long row;

	for (row = 0; row < list->count; row++)
	{
		if (!strcmp(list->items[row].name, name))
			return &list->items[row];
	}
	return NULL;
}

static struct ae_mods_list list;

static void rules_test(void)
{
	char text[64];
	struct ae_mods_claim claims[5];
	struct ae_mods_overlap overlaps[4];

	/* (text: control characters and runs of spaces become one space; broken UTF-8 a space; cut with "...") */
	ae_mods_text_clean(text, sizeof(text), "  High\t\tres\x01 audio  ", 40, 0);
	CHECK(!strcmp(text, "High res audio"));
	ae_mods_text_clean(text, sizeof(text), "caf\xc3\xa9 \xff\xfe ok", 40, 0);
	CHECK(!strcmp(text, "caf\xc3\xa9 ok"));
	ae_mods_text_clean(text, sizeof(text), "abcdefghijklmnop", 10, 0);
	CHECK(!strcmp(text, "abcdefg..."));
	ae_mods_text_clean(text, 8, "abcdefghijklmnop", 40, 0);
	CHECK(!strcmp(text, "abcd...") && strlen(text) < 8);
	ae_mods_text_clean(text, sizeof(text), "one\n\n\ntwo\n", 40, 1);
	CHECK(!strcmp(text, "one\ntwo"));
	ae_mods_text_clean(text, sizeof(text), "one\ntwo", 40, 0);
	CHECK(!strcmp(text, "one two"));
	ae_mods_text_clean(text, sizeof(text), NULL, 40, 0);
	CHECK(!strcmp(text, ""));
	/* (an over-long form and a surrogate are broken UTF-8; a cut never splits a character) */
	ae_mods_text_clean(text, sizeof(text), "a\xc0\xafz \xed\xa0\x80y", 40, 0);
	CHECK(!strcmp(text, "a z y"));
	ae_mods_text_clean(text, sizeof(text), "\xc3\xa9\xc3\xa9\xc3\xa9\xc3\xa9\xc3\xa9", 4, 0);
	CHECK(!strcmp(text, "\xc3\xa9..."));

	/* (groups) */
	CHECK(ae_mods_group_of("sound\\x.SOUND") == AE_MODS_GROUP_SOUND);
	CHECK(ae_mods_group_of("ui\\x.unicode_string_list") == AE_MODS_GROUP_TEXT);
	CHECK(ae_mods_group_of("ui\\x.string_list") == AE_MODS_GROUP_TEXT);
	CHECK(ae_mods_group_of("w\\x.gbxmodel") == AE_MODS_GROUP_MODEL && ae_mods_group_of("w\\x.model") == AE_MODS_GROUP_MODEL);
	CHECK(ae_mods_group_of("w\\x.bitmap") == AE_MODS_GROUP_BITMAP);
	CHECK(ae_mods_group_of("w\\x.weapon") == AE_MODS_GROUP_OTHER && ae_mods_group_of(".sound") == AE_MODS_GROUP_OTHER);

	/* (states: refused beats off beats on; on and played on a map that takes mods: on; else waits) */
	CHECK(ae_mods_state_of(1, 0, 1, 1) == AE_MODS_STATE_REFUSED);
	CHECK(ae_mods_state_of(0, 1, 0, 1) == AE_MODS_STATE_OFF);
	CHECK(ae_mods_state_of(0, 0, 1, 1) == AE_MODS_STATE_ON);
	CHECK(ae_mods_state_of(0, 0, 1, 0) == AE_MODS_STATE_WAITS && ae_mods_state_of(0, 0, 0, 1) == AE_MODS_STATE_WAITS);

	/* (priority: 1 is the last loaded) */
	CHECK(ae_mods_priority_of(5, 4) == 1 && ae_mods_priority_of(5, 0) == 5);
	CHECK(ae_mods_priority_places(5, 0, 1) == 4 && ae_mods_priority_places(5, 4, 5) == -4);
	CHECK(ae_mods_priority_places(5, 2, 99) == -2 && ae_mods_priority_places(5, 2, 0) == 2);

	/* (overlaps: a key in three mods counts once a pair; the later loaded wins) */
	claims[0].key = "b.sound"; claims[0].mod = 7; claims[0].position = 2;
	claims[1].key = "a.sound"; claims[1].mod = 3; claims[1].position = 0;
	claims[2].key = "a.sound"; claims[2].mod = 7; claims[2].position = 2;
	claims[3].key = "a.sound"; claims[3].mod = 5; claims[3].position = 1;
	claims[4].key = "b.sound"; claims[4].mod = 3; claims[4].position = 0;
	CHECK(ae_mods_overlaps(claims, 5, overlaps, 4) == 3);
	CHECK(overlaps[0].first == 3 && overlaps[0].second == 5 && overlaps[0].files == 1 && overlaps[0].winner == 5);
	CHECK(overlaps[1].first == 3 && overlaps[1].second == 7 && overlaps[1].files == 2 && overlaps[1].winner == 7);
	CHECK(overlaps[2].first == 5 && overlaps[2].second == 7 && overlaps[2].winner == 7);
	CHECK(ae_mods_overlaps(claims, 1, overlaps, 4) == 0);
}

static void list_test(void)
{
	const struct ae_mods_item *audio, *voices, *german, *later, *broken;
	char answer[240];

	folder_make();
	CHECK(ae_mods_list_read(&fake, &list) == 5 && list.active);
	audio = item(&list, "hd_audio");
	voices = item(&list, "voices");
	german = item(&list, "german");
	later = item(&list, "later");
	broken = item(&list, "broken");
	CHECK(audio && voices && german && later && broken);
	if (!audio || !voices || !german || !later || !broken)
		return;
	/* (drawn top first: the last loaded has priority 1) */
	CHECK(list.items[0].priority == 1 && !strcmp(list.items[0].name, "broken") && audio->priority == 5);
	/* (what mod.toml says, cleaned) */
	CHECK(!strcmp(audio->title, "High-res audio") && !strcmp(audio->id, "hdaudio") && !strcmp(audio->version, "1.0"));
	CHECK(!strcmp(audio->authors, "Someone, Someone else"));
	CHECK(!strcmp(audio->description, "The game's sounds\nat 44 kHz"));
	CHECK(!strcmp(audio->maps, "") && !strcmp(voices->maps, "bloodgulch, a10"));
	CHECK(!strcmp(voices->title, "voices"));
	/* (states and reasons) */
	CHECK(audio->state == AE_MODS_STATE_ON && audio->on && !audio->reason[0]);
	CHECK(later->state == AE_MODS_STATE_REFUSED && strstr(later->reason, "newer version"));
	CHECK(broken->state == AE_MODS_STATE_ON && broken->manifest_damaged && strstr(broken->note, "cannot be read"));
	CHECK(strstr(german->note, "cannot be played with this mod"));
	/* (what each changes; a .weapon counted as a kind the game never reads from a mod; a refused mod's files unknown) */
	CHECK(audio->files_known && audio->files[AE_MODS_GROUP_SOUND] == 2);
	CHECK(german->files[AE_MODS_GROUP_TEXT] == 1);
	CHECK(broken->files[AE_MODS_GROUP_OTHER] == 1 && broken->files[AE_MODS_GROUP_BITMAP] == 1);
	CHECK(!later->files_known);
	/* (hd_audio and voices both have the assault rifle's fire sound: voices loads later and plays it) */
	CHECK(list.overlap_count == 1 && list.overlaps[0].files == 1);
	CHECK(strstr(audio->note, "Shares 1 files with voices: voices's play."));
	CHECK(strstr(voices->note, "this mod's play"));
	ae_mods_list_line(&list, answer, sizeof(answer));
	CHECK(!strcmp(answer, "5 mods, 4 on: broken 1 on, later 2 refused, german 3 on, voices 4 on, hd_audio 5 on; 1 pair share files"));

	/* (off: kept, played now; its overlap gone with it) */
	CHECK(ae_mods_list_turn(&fake, "VOICES", 0, answer, sizeof(answer)) == AE_MODS_CHANGE_NOW);
	CHECK(!strcmp(answer, "voices off; playing now") && saves == 1 && applies == 1);
	ae_mods_list_read(&fake, &list);
	CHECK(item(&list, "voices")->state == AE_MODS_STATE_OFF && list.overlap_count == 0);
	/* (by its mod.toml id; a refused mod cannot be turned on; no such mod) */
	CHECK(ae_mods_list_turn(&fake, "hdaudio", 0, answer, sizeof(answer)) == AE_MODS_CHANGE_NOW && mods[0].off);
	CHECK(ae_mods_list_turn(&fake, "later", 1, answer, sizeof(answer)) == AE_MODS_CHANGE_REFUSED &&
		strstr(answer, "cannot be played") && saves == 2);
	CHECK(ae_mods_list_turn(&fake, "nothing", 1, answer, sizeof(answer)) == AE_MODS_CHANGE_REFUSED);
	/* (a map that does not take mods: it waits for one; a store that cannot be written: kept until the game quits) */
	map_active = 0;
	CHECK(ae_mods_list_turn(&fake, "hd_audio", 1, answer, sizeof(answer)) == AE_MODS_CHANGE_NEXT_MAP);
	ae_mods_list_read(&fake, &list);
	CHECK(item(&list, "hd_audio")->state == AE_MODS_STATE_WAITS && strstr(item(&list, "hd_audio")->reason, "map"));
	map_active = 1;
	store_writable = 0;
	CHECK(ae_mods_list_turn(&fake, "voices", 1, answer, sizeof(answer)) == AE_MODS_CHANGE_NOT_KEPT &&
		strstr(answer, "cannot be written"));
	store_writable = 1;

	/* (order: hd_audio to priority 1, over every other; then german to the bottom) */
	CHECK(ae_mods_list_order(&fake, "hd_audio", 1, answer, sizeof(answer)) == AE_MODS_CHANGE_NOW);
	CHECK(!strcmp(answer, "hd_audio at priority 1 of 5; playing now"));
	ae_mods_list_read(&fake, &list);
	CHECK(!strcmp(list.items[0].name, "hd_audio") && !strcmp(list.items[1].name, "broken"));
	/* (both on again, hd_audio now later than voices: hd_audio's sound plays) */
	CHECK(list.overlap_count == 1 && strstr(item(&list, "voices")->note, "hd_audio's play"));
	CHECK(ae_mods_list_order(&fake, "german", 5, answer, sizeof(answer)) == AE_MODS_CHANGE_NOW);
	ae_mods_list_read(&fake, &list);
	CHECK(!strcmp(list.items[4].name, "german") && item(&list, "german")->priority == 5);
	CHECK(ae_mods_list_order(&fake, "german", 6, answer, sizeof(answer)) == AE_MODS_CHANGE_REFUSED);
	CHECK(ae_mods_list_order(&fake, "german", 0, answer, sizeof(answer)) == AE_MODS_CHANGE_REFUSED);

	/* (the dev command: one line each) */
	CHECK(ae_mods_list_command(&fake, "list", NULL, 0, answer, sizeof(answer)) && !strncmp(answer, "5 mods, 4 on:", 13));
	CHECK(ae_mods_list_command(&fake, "off", "broken", 0, answer, sizeof(answer)) && !strcmp(answer, "broken off; playing now"));
	CHECK(ae_mods_list_command(&fake, "on", "broken", 0, answer, sizeof(answer)));
	CHECK(ae_mods_list_command(&fake, "order", "broken", 2, answer, sizeof(answer)) &&
		!strcmp(answer, "broken at priority 2 of 5; playing now"));
	CHECK(!ae_mods_list_command(&fake, "on", "later", 0, answer, sizeof(answer)));
	CHECK(!ae_mods_list_command(&fake, "shuffle", "x", 0, answer, sizeof(answer)) && strstr(answer, "mods list"));
	/* (an empty folder) */
	mod_count = 0;
	fake_scan();
	CHECK(ae_mods_list_read(&fake, &list) == 0);
	ae_mods_list_line(&list, answer, sizeof(answer));
	CHECK(!strcmp(answer, "0 mods, 0 on"));
}

int main(void)
{
	rules_test();
	list_test();
	printf("ae_mods_list: %d checks, %d failures\n", checks, failures);
	return failures != 0;
}
