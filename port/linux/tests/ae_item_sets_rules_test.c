#include <stdio.h>
#include <string.h>
#include "ae_item_sets_data.h"
#include "ae_item_swaps_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static void names_and_paths(void)
{
	char text[256], lines[8][96];
	char const *path = "cmt\\weapons\\evolved_h1-spirit\\pistol\\pistol_spirit_variant_mp";
	int count;

	CHECK(!strcmp(ae_is_kind_name(0), "WEAPONS") && !strcmp(ae_is_kind_name(6), "DOORS & DEVICES") && !strcmp(ae_is_kind_name(8), "FIRST PERSON HANDS"));
	CHECK(!strcmp(ae_is_kind_word(4), "BIPED") && !ae_is_kind_name(9)[0]);
	ae_is_item_name(path, text, sizeof(text));
	CHECK(!strcmp(text, "pistol_spirit_variant_mp"));
	ae_is_item_name("noslash", text, sizeof(text));
	CHECK(!strcmp(text, "noslash"));
	CHECK(ae_is_folder_count(path) == 4 && ae_is_folder_count("a") == 0);
	/* the folders above the name: all, else the first, an ellipsis, then the last ones that fit */
	ae_is_item_folders(path, -1, text, sizeof(text));
	CHECK(!strcmp(text, "cmt \xE2\x80\xBA weapons \xE2\x80\xBA evolved_h1-spirit \xE2\x80\xBA pistol"));
	ae_is_item_folders(path, 2, text, sizeof(text));
	CHECK(!strcmp(text, "cmt \xE2\x80\xBA \xE2\x80\xA6 \xE2\x80\xBA evolved_h1-spirit \xE2\x80\xBA pistol"));
	ae_is_item_folders(path, 0, text, sizeof(text));
	CHECK(!strcmp(text, "cmt \xE2\x80\xBA \xE2\x80\xA6"));
	ae_is_item_folders("name", -1, text, sizeof(text));
	CHECK(!text[0]);
	/* a name cut in the middle only when too long: both ends stay */
	ae_is_middle_cut("pistol_spirit_variant_mp", 20, text, sizeof(text));
	CHECK(!strncmp(text, "pistol_spi", 10) && !strcmp(text + strlen(text) - 8, "riant_mp") && strstr(text, "\xE2\x80\xA6"));
	ae_is_middle_cut("short", 20, text, sizeof(text));
	CHECK(!strcmp(text, "short"));
	/* a path in lines broken between folders, each broken line ending with the chevron */
	count = ae_is_path_lines(path, 30, lines, 8);
	CHECK(count > 1 && count <= 8);
	CHECK(strlen(lines[0]) > 0 && !strcmp(lines[0] + strlen(lines[0]) - 3, "\xE2\x80\xBA"));
	CHECK(!strstr(lines[count - 1], "\xE2\x80\xBA ") || strlen(lines[count - 1]) > 0);
	CHECK(!strcmp(lines[count - 1] + strlen(lines[count - 1]) - strlen("pistol_spirit_variant_mp"), "pistol_spirit_variant_mp"));
	count = ae_is_path_lines(path, 200, lines, 8);
	CHECK(count == 1 && strstr(lines[0], "evolved_h1-spirit"));
}

static void words(void)
{
	char text[300];

	CHECK(ae_is_set_name_ok("Spirit Kit") && ae_is_set_name_ok("a-b_c 1") && !ae_is_set_name_ok("") && !ae_is_set_name_ok("   ") &&
		!ae_is_set_name_ok("bad/name") && !ae_is_set_name_ok("x:y") && !ae_is_set_name_ok(NULL));
	ae_is_set_line(0, 0, 0, text, sizeof(text));
	CHECK(!strcmp(text, "No items yet"));
	ae_is_set_line(23, 61ull * 1024 * 1024, 3, text, sizeof(text));
	CHECK(!strcmp(text, "23 items \xE2\x80\xA2 61 MB \xE2\x80\xA2 from 3 maps"));
	ae_is_set_line(1, 2048, 1, text, sizeof(text));
	CHECK(strstr(text, "1 item ") && strstr(text, "from 1 map") && !strstr(text, "maps"));
	ae_is_set_state(1, 23, 61ull * 1024 * 1024, text, sizeof(text));
	CHECK(!strcmp(text, "PLAYING \xE2\x80\xA2 23 ITEMS \xE2\x80\xA2 61 MB"));
	ae_is_set_state(0, 1, 1024 * 1024, text, sizeof(text));
	CHECK(!strncmp(text, "OFF", 3) && strstr(text, "1 ITEM "));
	/* the engine's reasons in a player's words: the word "tag" is never shown */
	ae_is_reason_words("It has no first-person model or animations: its map never lets a player hold it, and it cannot be added.", text, sizeof(text));
	CHECK(strstr(text, "no first-person model") && !strstr(text, " tag"));
	ae_is_reason_words("It names a tag this build cannot take out: weapons\\x\\y", text, sizeof(text));
	CHECK(strstr(text, "a part this version can't take out: weapons\\x\\y") && !strstr(text, "tag"));
	ae_is_reason_words("The map cannot be read again.", text, sizeof(text));
	CHECK(strstr(text, "open it again"));
	ae_is_reason_words("The profile cannot be written (the log says why).", text, sizeof(text));
	CHECK(strstr(text, "The set can't be written") && !strstr(text, "profile"));
	ae_is_reason_words("Make a profile first.", text, sizeof(text));
	CHECK(!strcmp(text, "Make a set first."));
	ae_is_reason_words("Out of memory.", text, sizeof(text));
	CHECK(!strcmp(text, "Not enough memory."));
	ae_is_reason_words("", text, sizeof(text));
	CHECK(!text[0]);
	/* every failed value the engine has gets a sentence of its own, none says "profile" or "tag" */
	{
		static char const *const values[] = { "missing", "unreadable", "too large", "unplayable", "no memory", "not holdable", "bitmaps", "models" };
		int each, other;
		char second[300];

		for (each = 0; each < 8; each++)
		{
			ae_is_failed_words(values[each], text, sizeof(text));
			CHECK(text[0] && !strstr(text, "profile") && !strstr(text, " tag") && !strstr(text, "bundle"));
			for (other = 0; other < each; other++)
			{
				ae_is_failed_words(values[other], second, sizeof(second));
				CHECK(strcmp(text, second));
			}
		}
		ae_is_failed_words("", text, sizeof(text));
		CHECK(!text[0]);
		ae_is_failed_todo("too large", text, sizeof(text));
		CHECK(strstr(text, "Turn off items"));
	}
}

/* PVP isolation: a set's items are added to a campaign level played alone through the randomizer, and to no other kind of game */
static void plays(void)
{
	int game;

	CHECK(ae_is_plays_in(AE_IS_GAME_CAMPAIGN_ALONE, 1));
	CHECK(!ae_is_plays_in(AE_IS_GAME_CAMPAIGN_ALONE, 0));
	for (game = 0; game < AE_IS_GAMES; game++)
		if (game != AE_IS_GAME_CAMPAIGN_ALONE)
			CHECK(!ae_is_plays_in(game, 1) && !ae_is_plays_in(game, 0));
	CHECK(!ae_is_plays_in(AE_IS_GAME_MULTIPLAYER, 1) && !ae_is_plays_in(AE_IS_GAME_ONLINE, 1) && !ae_is_plays_in(AE_IS_GAME_CHALLENGE, 1) &&
		!ae_is_plays_in(AE_IS_GAME_FIREFIGHT, 1) && !ae_is_plays_in(AE_IS_GAME_CAMPAIGN_COOP, 1));
	CHECK(strstr(ae_is_plays_words(1), "campaign levels you play alone") && strstr(ae_is_plays_words(0), "off"));
	CHECK(strstr(ae_is_others_words(), "Multiplayer") && strstr(ae_is_others_words(), "never"));
}

static void footers(void)
{
	struct ae_is_hint hints[AE_IS_HINTS_MOST];
	int step, count;

	for (step = 0; step < AE_IS_STEPS; step++)
	{
		count = ae_is_hints(step, 0, 0, hints);
		CHECK(count >= 2 && count <= AE_IS_HINTS_MOST);
		CHECK(hints[count - 1].hint == AE_IS_HINT_BACK || (step == AE_IS_STEP_KINDS && hints[count - 1].hint == AE_IS_HINT_CLOSE_MAP));
	}
	count = ae_is_hints(AE_IS_STEP_SETS, 1, 1, hints);
	CHECK(hints[1].hint == AE_IS_HINT_STOP && !strcmp(hints[1].label, "Stop playing"));
	count = ae_is_hints(AE_IS_STEP_SETS, 0, 1, hints);
	CHECK(hints[1].hint == AE_IS_HINT_PLAY && !strcmp(hints[1].label, "Play this set"));
	count = ae_is_hints(AE_IS_STEP_SETS, 0, -1, hints);
	CHECK(count == 2 && hints[0].hint == AE_IS_HINT_NEW);
	count = ae_is_hints(AE_IS_STEP_ITEMS, 0, 1, hints);
	CHECK(hints[0].hint == AE_IS_HINT_REMOVE && !strcmp(hints[0].label, "Remove"));
	count = ae_is_hints(AE_IS_STEP_ITEMS, 0, 0, hints);
	CHECK(hints[0].hint == AE_IS_HINT_ADD && !strcmp(hints[0].label, "Add to set"));
}

/* the made-up table behind the staged scenes works as the engine's does: sets, play, add, remove, copy, rename, delete */
static void staged(void)
{
	struct ae_is_backend const *be = ae_is_backend_staged(0);
	static struct ae_is_set sets[AE_IS_SETS_MOST];
	static struct ae_is_entry entries[AE_IS_ENTRIES_MOST];
	static struct ae_is_map maps[AE_IS_MAPS_MOST];
	static struct ae_is_item items[AE_IS_ITEMS_MOST];
	struct ae_is_added added;
	char folder[64], reason[200];
	int count, entry_count, map_count, item_count, each, tsce = -1;

	count = be->sets(sets, AE_IS_SETS_MOST);
	CHECK(count == 4 && !strcmp(sets[0].name, "Spirit Kit") && sets[0].playing && !sets[1].playing && sets[3].items == 0);
	CHECK(sets[0].items == 9 && sets[0].on == 7 && sets[0].kinds[0] == 4);
	/* one plays at a time */
	CHECK(be->play(sets[1].folder));
	be->sets(sets, AE_IS_SETS_MOST);
	CHECK(!sets[0].playing && sets[1].playing);
	CHECK(be->play(NULL));
	be->sets(sets, AE_IS_SETS_MOST);
	CHECK(!sets[0].playing && !sets[1].playing);
	CHECK(!be->play("nothing"));
	/* a map of Custom Edition opens; items are listed by kind; a turret's gun is refused; a biped brings its hands */
	map_count = be->maps(sets[3].folder, maps, AE_IS_MAPS_MOST);
	CHECK(map_count == 10);
	for (each = 0; each < map_count; each++)
		if (!strcmp(maps[each].file, "tsce_multiplayerv1"))
			tsce = each;
	CHECK(tsce >= 0 && maps[tsce].in_set == 0);
	CHECK(be->maps_reason(AE_IS_SOURCE_CE) == NULL);
	CHECK(be->map_open(tsce, reason, sizeof(reason)));
	CHECK(be->kind_count(0) == 8 && be->kind_count(4) == 2 && be->kind_count(5) == 12);
	item_count = be->kind_items(0, sets[3].folder, items, AE_IS_ITEMS_MOST);
	CHECK(item_count == 8);
	for (each = 0; each < item_count; each++)
		if (strstr(items[each].path, "gun_turret_mounted"))
		{
			be->item_look(0, (int)items[each].index, &items[each]);
			CHECK(items[each].refused && strstr(items[each].reason, "no first-person"));
			be->item_add(sets[3].folder, 0, (int)items[each].index, &added);
			CHECK(!added.ok && added.reason[0]);
		}
		else if (strstr(items[each].path, "needler"))
		{
			be->item_add(sets[3].folder, 0, (int)items[each].index, &added);
			CHECK(added.ok && added.kept > 0);
		}
	entry_count = be->entries(sets[3].folder, entries, AE_IS_ENTRIES_MOST);
	CHECK(entry_count == 1 && entries[0].on && entries[0].kind == 0);
	item_count = be->kind_items(0, sets[3].folder, items, AE_IS_ITEMS_MOST);
	for (each = 0; each < item_count; each++)
		CHECK(items[each].in_set == (strstr(items[each].path, "needler") != NULL));
	item_count = be->kind_items(4, sets[3].folder, items, AE_IS_ITEMS_MOST);
	be->item_add(sets[3].folder, 4, (int)items[0].index, &added);
	CHECK(added.ok && added.carried == 1 && strstr(added.carried_names, "hands"));
	be->map_close();
	/* on / off, failed turned on again, remove */
	entry_count = be->entries(sets[3].folder, entries, AE_IS_ENTRIES_MOST);
	CHECK(entry_count == 2 && be->entry_enable(sets[3].folder, entries[0].tag, 0));
	be->entries(sets[3].folder, entries, AE_IS_ENTRIES_MOST);
	CHECK(!entries[0].on);
	entry_count = be->entries(sets[0].folder, entries, AE_IS_ENTRIES_MOST);
	for (each = 0; each < entry_count; each++)
		if (entries[each].failed[0])
		{
			CHECK(!entries[each].on && be->entry_enable(sets[0].folder, entries[each].tag, 1));
			be->entries(sets[0].folder, entries, AE_IS_ENTRIES_MOST);
			CHECK(!entries[each].failed[0] && entries[each].on);
		}
	CHECK(be->entry_remove(sets[3].folder, entries[0].tag) || 1);
	/* copy, rename, delete */
	CHECK(be->copy(sets[0].folder, "Spirit Copy", folder, sizeof(folder)));
	count = be->sets(sets, AE_IS_SETS_MOST);
	CHECK(count == 5 && sets[4].items == sets[0].items);
	CHECK(be->rename(sets[4].folder, "Renamed Kit", folder, sizeof(folder)));
	be->sets(sets, AE_IS_SETS_MOST);
	CHECK(!strcmp(sets[4].name, "Renamed Kit"));
	CHECK(be->remove_set(sets[4].folder));
	CHECK(be->sets(sets, AE_IS_SETS_MOST) == 4);
	CHECK(be->create("Fresh", folder, sizeof(folder)) && be->sets(sets, AE_IS_SETS_MOST) == 5);
	/* the empty table has no set; a Custom Edition map can be made to refuse as while one is played */
	be = ae_is_backend_staged(1);
	CHECK(be->sets(sets, AE_IS_SETS_MOST) == 0);
	ae_is_staged_block_ce(1);
	CHECK(be->maps_reason(AE_IS_SOURCE_CE) && strstr(be->maps_reason(AE_IS_SOURCE_CE), "Custom Edition map is being played"));
	CHECK(be->maps_reason(AE_IS_SOURCE_MCC) == NULL && be->maps_reason(AE_IS_SOURCE_WORKSHOP) == NULL);
	CHECK(!be->map_open(0, reason, sizeof(reason)));
}

/* an item's settings page: the rows of each kind, the values (AUTO first, the engine's keys), the typed rows */
static void settings(void)
{
	int rows[AE_IS_ROWS], count, each, kind;

	/* a weapon: faction, class, group and HUD; the campaign's place only when its tags are known */
	count = ae_is_setting_rows(0, 0, 0, rows);
	CHECK(count == 7 && rows[0] == AE_IS_ROW_NAME && rows[1] == AE_IS_ROW_ON && rows[2] == AE_IS_ROW_FACTION && rows[3] == AE_IS_ROW_CLASS &&
		rows[4] == AE_IS_ROW_GROUP && rows[5] == AE_IS_ROW_HUD && rows[6] == AE_IS_ROW_REMOVE);
	count = ae_is_setting_rows(0, 1, 0, rows);
	CHECK(count == 8 && rows[6] == AE_IS_ROW_REPLACE && rows[7] == AE_IS_ROW_REMOVE);
	/* an AI: faction, what it counts as, group */
	count = ae_is_setting_rows(1, 0, 0, rows);
	CHECK(count == 6 && rows[2] == AE_IS_ROW_FACTION && rows[3] == AE_IS_ROW_COUNTS_AS && rows[4] == AE_IS_ROW_GROUP);
	/* a biped: PLAY AS IT, and HANDS and MOVEMENT only while it is on */
	count = ae_is_setting_rows(4, 0, 0, rows);
	CHECK(count == 4 && rows[2] == AE_IS_ROW_PLAY_AS && rows[3] == AE_IS_ROW_REMOVE);
	count = ae_is_setting_rows(4, 0, 1, rows);
	CHECK(count == 6 && rows[2] == AE_IS_ROW_PLAY_AS && rows[3] == AE_IS_ROW_HANDS && rows[4] == AE_IS_ROW_MOVEMENT && rows[5] == AE_IS_ROW_REMOVE);
	/* the other kinds: a name, on, and the removal */
	for (kind = 2; kind < AE_IS_KINDS; kind++)
		if (kind != 4)
			CHECK(ae_is_setting_rows(kind, 0, 1, rows) == 3 && rows[2] == AE_IS_ROW_REMOVE);
	CHECK(ae_is_row_section(AE_IS_ROW_FACTION) == AE_IS_SECTION_RANDOMIZER && ae_is_row_section(AE_IS_ROW_HUD) == AE_IS_SECTION_LEVEL &&
		ae_is_row_section(AE_IS_ROW_MOVEMENT) == AE_IS_SECTION_BODY && ae_is_row_section(AE_IS_ROW_REMOVE) == AE_IS_SECTION_SET &&
		ae_is_row_section(AE_IS_ROW_NAME) == AE_IS_SECTION_NONE);
	CHECK(!strcmp(ae_is_row_label(AE_IS_ROW_REPLACE), "TAKES THE CAMPAIGN'S PLACE") && !strcmp(ae_is_row_label(AE_IS_ROW_GROUP), "GROUP NAME"));
	CHECK(ae_is_typed_ok("") && ae_is_typed_ok("Spirit Group-1_a") && !ae_is_typed_ok("a/b") && !ae_is_typed_ok("caf\xC3\xA9") &&
		ae_is_typed_ok("0123456789012345678901234567890") && !ae_is_typed_ok("01234567890123456789012345678901"));
	CHECK(ae_is_row_typed(AE_IS_ROW_NAME) && ae_is_row_typed(AE_IS_ROW_GROUP) && !ae_is_row_typed(AE_IS_ROW_FACTION));
	/* values: a weapon has two sides, an AI four and ANY; AUTO is first and keeps ""; the keys are the engine's */
	CHECK(ae_is_value_count(AE_IS_ROW_FACTION, 0) == 3 && ae_is_value_count(AE_IS_ROW_FACTION, 1) == 5);
	CHECK(!strcmp(ae_is_value_word(AE_IS_ROW_FACTION, 1, 4), "ANY POOL ONLY") && !strcmp(ae_is_value_key(AE_IS_ROW_FACTION, 1, 4), "any"));
	CHECK(!strcmp(ae_is_value_word(AE_IS_ROW_FACTION, 0, 0), "AUTO") && !ae_is_value_key(AE_IS_ROW_FACTION, 0, 0)[0]);
	CHECK(ae_is_value_count(AE_IS_ROW_CLASS, 0) == 5 && !strcmp(ae_is_value_key(AE_IS_ROW_CLASS, 0, 3), "power"));
	CHECK(ae_is_value_count(AE_IS_ROW_COUNTS_AS, 1) == 11 && !strcmp(ae_is_value_key(AE_IS_ROW_COUNTS_AS, 1, 9), "sentinel") &&
		!strcmp(ae_is_value_word(AE_IS_ROW_COUNTS_AS, 1, 5), "FLOOD COMBAT"));
	CHECK(ae_is_value_count(AE_IS_ROW_HUD, 0) == 11 && !ae_is_value_key(AE_IS_ROW_HUD, 0, 0)[0] && !strcmp(ae_is_value_key(AE_IS_ROW_HUD, 0, 1), "auto") &&
		!strcmp(ae_is_value_key(AE_IS_ROW_HUD, 0, 5), "rocket_launcher"));
	CHECK(ae_is_value_count(AE_IS_ROW_REPLACE, 0) == 2 && !strcmp(ae_is_value_key(AE_IS_ROW_REPLACE, 0, 1), "true"));
	CHECK(ae_is_value_count(AE_IS_ROW_MOVEMENT, 4) == 2 && !strcmp(ae_is_value_key(AE_IS_ROW_MOVEMENT, 4, 1), "own"));
	CHECK(ae_is_value_count(AE_IS_ROW_NAME, 0) == 0 && ae_is_value_count(AE_IS_ROW_GROUP, 0) == 0);
	/* a kept text finds its value again; one it does not know is AUTO (the first) */
	CHECK(ae_is_value_index(AE_IS_ROW_FACTION, 0, "HUMAN") == 2 && ae_is_value_index(AE_IS_ROW_FACTION, 0, "") == 0 &&
		ae_is_value_index(AE_IS_ROW_FACTION, 0, "zebra") == 0 && ae_is_value_index(AE_IS_ROW_REPLACE, 0, "true") == 1);
	for (each = 0; each < ae_is_value_count(AE_IS_ROW_HUD, 0); each++)
		CHECK(ae_is_value_index(AE_IS_ROW_HUD, 0, ae_is_value_key(AE_IS_ROW_HUD, 0, each)) == each);
	/* a step goes round the list either way */
	CHECK(ae_is_value_step(3, 2, 1) == 0 && ae_is_value_step(3, 0, -1) == 2 && ae_is_value_step(5, 1, 1) == 2 && ae_is_value_step(0, 0, 1) == 0);
	for (each = 0; each < AE_IS_ROWS; each++)
		CHECK(ae_is_row_card_text(each)[0] && ae_is_row_label(each)[0]);
	CHECK(strstr(ae_is_row_card_text(AE_IS_ROW_PLAY_AS), "One body per set"));
	CHECK(!strcmp(ae_is_hands_word(0), "ITS OWN") && !strcmp(ae_is_hands_word(1), "THE CAMPAIGN'S OWN"));
	{
		char key[300];

		ae_is_hands_key("cmt\\characters\\cyborg\\fp", "tsce_multiplayerv1", key, sizeof(key));
		CHECK(!strcmp(key, "cmt\\characters\\cyborg\\fp#tsce_multiplayerv1"));
	}
	/* the settings step has its own footer, ending with Back; a set's item has Settings first */
	{
		struct ae_is_hint hints[AE_IS_HINTS_MOST];

		count = ae_is_hints(AE_IS_STEP_SETTINGS, 0, 1, hints);
		CHECK(count >= 2 && hints[0].hint == AE_IS_HINT_CHANGE && hints[count - 1].hint == AE_IS_HINT_BACK);
		/* a row that cannot change has no Change: the removal and the waiting rows; a typed row is typed with A */
		count = ae_is_hints(AE_IS_STEP_SETTINGS, 0, 0, hints);
		CHECK(count == 2 && hints[0].hint == AE_IS_HINT_REMOVE && hints[1].hint == AE_IS_HINT_BACK);
		count = ae_is_hints(AE_IS_STEP_SETTINGS, 0, 2, hints);
		CHECK(count == 3 && hints[0].hint == AE_IS_HINT_OPEN && !strcmp(hints[0].label, "Type"));
		count = ae_is_hints(AE_IS_STEP_CONTENTS, 0, 1, hints);
		CHECK(hints[0].hint == AE_IS_HINT_SETTINGS && hints[1].hint == AE_IS_HINT_ON_OFF && hints[count - 1].hint == AE_IS_HINT_BACK);
		count = ae_is_hints(AE_IS_STEP_CONTENTS, 0, 0, hints);
		CHECK(hints[0].hint == AE_IS_HINT_ADD_ITEMS && hints[count - 1].hint == AE_IS_HINT_BACK);
	}
}

/* AUTO's answers (the randomizer's words), the pools they make, plain words for a biped that does not fit, and what each row takes */
static void auto_answers(void)
{
	char text[200];
	struct ae_is_fit fit;
	struct ae_biped_facts stock, lacks;

	CHECK(ae_is_auto_answer(AE_IS_ROW_FACTION, 0, "cmt\\weapons\\pistol\\pistol_spirit_variant_mp", "", text, sizeof(text)) && !strcmp(text, "HUMAN"));
	CHECK(ae_is_auto_answer(AE_IS_ROW_CLASS, 0, "cmt\\weapons\\pistol\\pistol_spirit_variant_mp", "", text, sizeof(text)) && !strcmp(text, "PRECISION"));
	CHECK(ae_is_auto_answer(AE_IS_ROW_FACTION, 0, "cmt\\weapons\\covenant\\plasma_rifle\\plasma_rifle_mp", "", text, sizeof(text)) && !strcmp(text, "COVENANT"));
	CHECK(ae_is_auto_answer(AE_IS_ROW_CLASS, 0, "x\\rocket_launcher", "", text, sizeof(text)) && !strcmp(text, "POWER"));
	CHECK(ae_is_auto_answer(AE_IS_ROW_CLASS, 0, "x\\assault_rifle_mp", "", text, sizeof(text)) && !strcmp(text, "AUTOMATIC"));
	CHECK(ae_is_auto_answer(AE_IS_ROW_CLASS, 0, "x\\thing", "", text, sizeof(text)) && !strcmp(text, "NONE"));
	/* (a name a player gave counts as words of it too) */
	CHECK(ae_is_auto_answer(AE_IS_ROW_CLASS, 0, "x\\weapon 2", "Sniper Kit", text, sizeof(text)) && !strcmp(text, "PRECISION"));
	CHECK(ae_is_auto_answer(AE_IS_ROW_FACTION, 1, "cmt\\characters\\covenant\\grunt\\grunt_heavy", "", text, sizeof(text)) && !strcmp(text, "COVENANT"));
	CHECK(ae_is_auto_answer(AE_IS_ROW_COUNTS_AS, 1, "cmt\\characters\\covenant\\grunt\\grunt_heavy", "", text, sizeof(text)) && !strcmp(text, "GRUNT"));
	CHECK(ae_is_auto_answer(AE_IS_ROW_FACTION, 1, "x\\flood_combat_elite", "", text, sizeof(text)) && !strcmp(text, "FLOOD"));
	CHECK(ae_is_auto_answer(AE_IS_ROW_COUNTS_AS, 1, "x\\flood_combat_elite", "", text, sizeof(text)) && !strcmp(text, "FLOOD COMBAT"));
	CHECK(ae_is_auto_answer(AE_IS_ROW_FACTION, 1, "x\\marine_odst", "", text, sizeof(text)) && !strcmp(text, "HUMAN"));
	CHECK(ae_is_auto_answer(AE_IS_ROW_FACTION, 1, "x\\thing", "", text, sizeof(text)) && !strcmp(text, "ANY POOL ONLY"));
	CHECK(ae_is_auto_answer(AE_IS_ROW_HUD, 0, "cmt\\weapons\\pistol\\pistol_spirit_variant_mp", "", text, sizeof(text)) && !strcmp(text, "THE PISTOL'S"));
	CHECK(ae_is_auto_answer(AE_IS_ROW_HUD, 0, "x\\plasma_pistol", "", text, sizeof(text)) && !strcmp(text, "THE PLASMA PISTOL'S"));
	CHECK(ae_is_auto_answer(AE_IS_ROW_HUD, 0, "x\\fuel_rod_gun", "", text, sizeof(text)) && !strcmp(text, "THE ROCKET LAUNCHER'S"));
	CHECK(ae_is_auto_answer(AE_IS_ROW_HUD, 0, "x\\thing", "", text, sizeof(text)) && !strcmp(text, "ITS PARTS'"));
	/* the rows that have no answer */
	CHECK(!ae_is_auto_answer(AE_IS_ROW_NAME, 0, "x", "", text, sizeof(text)) && !ae_is_auto_answer(AE_IS_ROW_CLASS, 1, "x", "", text, sizeof(text)) &&
		!ae_is_auto_answer(AE_IS_ROW_FACTION, 4, "x", "", text, sizeof(text)));
	/* the pools: ANY and the side and the class, from what is set else from AUTO's answers */
	ae_is_pools_words(0, "cmt\\weapons\\pistol\\pistol_spirit_variant_mp", "", "", "", "", 0, text, sizeof(text));
	CHECK(!strcmp(text, "ANY \xE2\x80\xA2 HUMAN \xE2\x80\xA2 PRECISION"));
	ae_is_pools_words(0, "cmt\\weapons\\pistol\\pistol_spirit_variant_mp", "", "", "", "", 1, text, sizeof(text));
	CHECK(!strcmp(text, "Human \xE2\x80\xA2 Precision (auto)"));
	ae_is_pools_words(0, "cmt\\weapons\\pistol\\pistol_spirit_variant_mp", "", "covenant", "power", "", 1, text, sizeof(text));
	CHECK(!strcmp(text, "Covenant \xE2\x80\xA2 Power"));
	ae_is_pools_words(1, "cmt\\characters\\covenant\\grunt\\grunt_heavy", "", "", "", "", 0, text, sizeof(text));
	CHECK(!strcmp(text, "ANY \xE2\x80\xA2 COVENANT \xE2\x80\xA2 GRUNT"));
	ae_is_pools_words(0, "x\\thing", "", "", "", "", 1, text, sizeof(text));
	CHECK(!strcmp(text, "Human (auto)"));
	ae_is_pools_words(4, "x", "", "", "", "", 0, text, sizeof(text));
	CHECK(!text[0]);
	/* why a biped does not fit, in plain words from the facts (no engine string) */
	memset(&stock, 0, sizeof(stock));
	stock.radius = stock.standing_height = 0.70f;
	stock.standing_eye = 0.64f;
	stock.crouching_eye = 0.40f;
	stock.has_animation_graph = stock.uses_player_physics = 1;
	stock.seats_stock = 14;
	stock.labels_checked = 11;
	lacks = stock;
	lacks.uses_player_physics = 0;
	lacks.seats_missing = 3;
	snprintf(lacks.seat_first, sizeof(lacks.seat_first), "w-driver");
	ae_is_fit_from_facts(&stock, &lacks, &fit);
	ae_is_fit_words(&fit, text, sizeof(text));
	CHECK(!strcmp(text, "It has no player physics, and it has no seat for 3 of the stock player's 14 vehicle seats."));
	CHECK(!strstr(text, "w-driver") && !strstr(text, "flag") && !strstr(text, "graph"));
	ae_is_fit_short(&fit, text, sizeof(text));
	CHECK(!strcmp(text, "it has no player physics, and it has no seat for 3 of the stock player's 14 vehicle seats"));
	lacks = stock;
	lacks.standing_height = 1.5f;
	lacks.labels_missing = 2;
	ae_is_fit_from_facts(&stock, &lacks, &fit);
	ae_is_fit_words(&fit, text, sizeof(text));
	CHECK(strstr(text, "can't hold 2 of the campaign's 11 weapons") && strstr(text, "too big: 214 %") && strstr(text, ", and "));
	lacks = stock;
	lacks.has_animation_graph = 0;
	ae_is_fit_from_facts(&stock, &lacks, &fit);
	ae_is_fit_words(&fit, text, sizeof(text));
	CHECK(strstr(text, "no animations"));
	/* a verdict with no numbers says it generally; a fit says nothing */
	memset(&fit, 0, sizeof(fit));
	fit.known = 1;
	ae_is_fit_words(&fit, text, sizeof(text));
	CHECK(!strcmp(text, "A level that loaded it found it does not fit a player."));
	fit.fits = 1;
	ae_is_fit_words(&fit, text, sizeof(text));
	CHECK(!text[0]);
	/* the rows' help and what each takes: HANDS and MOVEMENT wait until the spawn plays them; the removal and a biped that does not fit take no change */
	CHECK(!ae_is_body_rows_live());
	CHECK(ae_is_row_change(AE_IS_ROW_FACTION, 0) == 1 && ae_is_row_change(AE_IS_ROW_ON, 0) == 1 && ae_is_row_change(AE_IS_ROW_NAME, 0) == 2 &&
		ae_is_row_change(AE_IS_ROW_GROUP, 0) == 2 && ae_is_row_change(AE_IS_ROW_REMOVE, 0) == 0 && ae_is_row_change(AE_IS_ROW_HANDS, 0) == 0 &&
		ae_is_row_change(AE_IS_ROW_MOVEMENT, 0) == 0 && ae_is_row_change(AE_IS_ROW_PLAY_AS, 0) == 1 && ae_is_row_change(AE_IS_ROW_PLAY_AS, 1) == 0);
	CHECK(strstr(ae_is_row_help(AE_IS_ROW_FACTION), "AUTO finds it") && strstr(ae_is_row_help(AE_IS_ROW_PLAY_AS), "next mission") &&
		strstr(ae_is_row_help(AE_IS_ROW_HANDS), "next build") && !ae_is_row_help(AE_IS_ROW_NAME)[0]);
}

/* YOUR BODY: only a campaign level played alone has one; a campaign swap that plays decides, the set's PLAY AS IT otherwise */
static void body(void)
{
	int game;
	struct ae_biped_facts stock, fits, lacks;
	struct ae_is_fit fit;
	char lines[6][160];

	/* the precedence: the swap's choice wins over the set's PLAY AS IT, even when the swap names none */
	CHECK(ae_is_body_source(AE_IS_GAME_CAMPAIGN_ALONE, 1, 1, 1, 0, 0) == AE_IS_BODY_SET);
	CHECK(ae_is_body_source(AE_IS_GAME_CAMPAIGN_ALONE, 1, 1, 1, 1, 1) == AE_IS_BODY_SWAP);
	CHECK(ae_is_body_source(AE_IS_GAME_CAMPAIGN_ALONE, 1, 1, 1, 1, 0) == AE_IS_BODY_NONE);
	CHECK(ae_is_body_source(AE_IS_GAME_CAMPAIGN_ALONE, 1, 0, 0, 1, 1) == AE_IS_BODY_SWAP);
	/* (a set's body plays with the randomizer, which a swap passes over) */
	CHECK(ae_is_body_source(AE_IS_GAME_CAMPAIGN_ALONE, 0, 1, 1, 0, 0) == AE_IS_BODY_NONE);
	CHECK(ae_is_body_source(AE_IS_GAME_CAMPAIGN_ALONE, 0, 1, 1, 1, 1) == AE_IS_BODY_SWAP);
	CHECK(ae_is_body_source(AE_IS_GAME_CAMPAIGN_ALONE, 1, 0, 1, 0, 0) == AE_IS_BODY_NONE);
	CHECK(ae_is_body_source(AE_IS_GAME_CAMPAIGN_ALONE, 1, 1, 0, 0, 0) == AE_IS_BODY_NONE);
	/* never in any other kind of game, whatever is set */
	for (game = 0; game < AE_IS_GAMES; game++)
		if (game != AE_IS_GAME_CAMPAIGN_ALONE)
			CHECK(ae_is_body_source(game, 1, 1, 1, 0, 0) == AE_IS_BODY_NONE && ae_is_body_source(game, 1, 1, 1, 1, 1) == AE_IS_BODY_NONE);
	/* the hands and the movement kept with a body, and what the engine's layer holds */
	CHECK(ae_is_hands_choice("") == AE_IS_HANDS_OWN && ae_is_hands_choice(NULL) == AE_IS_HANDS_OWN &&
		ae_is_hands_choice("campaign") == AE_IS_HANDS_CAMPAIGN && ae_is_hands_choice("a\\fp#m") == AE_IS_HANDS_CHOSEN);
	CHECK(ae_is_movement_own("own") && !ae_is_movement_own("") && !ae_is_movement_own(NULL) && !ae_is_movement_own("spartan"));
	CHECK(ae_is_body_layer(AE_IS_BODY_NONE, 1, 1) == AE_IS_LAYER_LEAVE && ae_is_body_layer(AE_IS_BODY_NONE, 0, 0) == AE_IS_LAYER_LEAVE);
	CHECK(ae_is_body_layer(AE_IS_BODY_SET, 1, 1) == AE_IS_LAYER_BODY && ae_is_body_layer(AE_IS_BODY_SWAP, 1, 1) == AE_IS_LAYER_BODY);
	CHECK(ae_is_body_layer(AE_IS_BODY_SET, 1, 0) == AE_IS_LAYER_CLEAR && ae_is_body_layer(AE_IS_BODY_SWAP, 0, 1) == AE_IS_LAYER_CLEAR &&
		ae_is_body_layer(AE_IS_BODY_SWAP, 0, 0) == AE_IS_LAYER_CLEAR);
	/* (a body only in a campaign level played alone: every other game's source is none, so the layer is left) */
	for (game = 0; game < AE_IS_GAMES; game++)
		if (game != AE_IS_GAME_CAMPAIGN_ALONE)
			CHECK(ae_is_body_layer(ae_is_body_source(game, 1, 1, 1, 1, 1), 1, 1) == AE_IS_LAYER_LEAVE);
	/* a path is the same biped whatever the case or the slash */
	CHECK(ae_is_body_is("cmt\\characters\\Cyborg\\cyborg_mp", "cmt/characters/cyborg/cyborg_mp") && !ae_is_body_is("a\\b", "a\\c") &&
		!ae_is_body_is("", "") && !ae_is_body_is("a", "ab") && !ae_is_body_is("ab", "a"));
	/* the fitness: the biped packs' own rule over a fitting biped (cyborg_mp's) and a failing one (an elite) */
	memset(&stock, 0, sizeof(stock));
	stock.radius = 0.70f;
	stock.standing_height = 0.70f;
	stock.standing_eye = 0.64f;
	stock.crouching_eye = 0.40f;
	stock.has_animation_graph = 1;
	stock.uses_player_physics = 1;
	stock.seats_stock = 14;
	stock.labels_checked = 11;
	fits = stock;
	fits.standing_height = 0.714f;
	CHECK(ae_is_fit_from_facts(&stock, &fits, &fit) && fit.known && fit.fits && !fit.reason[0] && fit.size_percent == 102 && fit.physics &&
		fit.seats_have == 14 && fit.seats_total == 14 && fit.weapons_held == 11 && fit.weapons_total == 11);
	lacks = stock;
	lacks.uses_player_physics = 0;
	lacks.seats_missing = 3;
	snprintf(lacks.seat_first, sizeof(lacks.seat_first), "w-driver");
	CHECK(!ae_is_fit_from_facts(&stock, &lacks, &fit) && fit.known && !fit.fits && fit.seats_have == 11 && !fit.physics);
	CHECK(strstr(fit.reason, "player's physics") && strstr(fit.reason, "lacks 3 of the stock player's 14 seats") && strchr(fit.reason, ';'));
	/* too big or too small is a reason too */
	lacks = stock;
	lacks.standing_height = 1.5f;
	CHECK(!ae_is_fit_from_facts(&stock, &lacks, &fit) && strstr(fit.reason, "standing height"));
	CHECK(!ae_is_fit_from_facts(NULL, &fits, &fit) && !fit.known);
	/* the reasons as sentences, one a line */
	CHECK(ae_is_fit_reason_lines("the biped is not built; it lacks 3 seats", lines, 6) == 2 && !strcmp(lines[0], "The biped is not built.") &&
		!strcmp(lines[1], "It lacks 3 seats."));
	CHECK(ae_is_fit_reason_lines("", lines, 6) == 0 && ae_is_fit_reason_lines("one", lines, 6) == 1 && ae_is_fit_reason_lines("a; b; c", lines, 2) == 2);
}

/* the made-up table's item settings, a set's body and the fit of its two bipeds */
static void staged_settings(void)
{
	struct ae_is_backend const *be = ae_is_backend_staged(0);
	struct ae_is_set sets[AE_IS_SETS_MOST];
	struct ae_is_entry entries[AE_IS_ENTRIES_MOST];
	char value[300];
	struct ae_is_fit fit;
	int count, each, cyborg = -1, elite = -1;
	long tag;

	be->sets(sets, AE_IS_SETS_MOST);
	count = be->entries(sets[0].folder, entries, AE_IS_ENTRIES_MOST);
	tag = entries[0].tag;
	/* an item's setting is kept: written, read back, cleared; a key the set does not have is refused */
	CHECK(be->entry_option(sets[0].folder, tag, "faction", value, sizeof(value)) && !value[0]);
	CHECK(be->entry_set_option(sets[0].folder, tag, "faction", "human") && be->entry_option(sets[0].folder, tag, "faction", value, sizeof(value)) &&
		!strcmp(value, "human"));
	CHECK(be->entry_set_option(sets[0].folder, tag, "custom_type", "Spirit Group") && be->entry_option(sets[0].folder, tag, "custom_type", value, sizeof(value)) &&
		!strcmp(value, "Spirit Group"));
	CHECK(be->entry_set_option(sets[0].folder, tag, "faction", "") && be->entry_option(sets[0].folder, tag, "faction", value, sizeof(value)) && !value[0]);
	CHECK(!be->entry_option(sets[0].folder, tag, "colour", value, sizeof(value)) && !be->entry_option(sets[0].folder, 9999, "faction", value, sizeof(value)));
	CHECK(be->replace_known());
	/* the set's body keys */
	CHECK(be->body_option(sets[0].folder, "player_biped", value, sizeof(value)) && !value[0]);
	for (each = 0; each < count; each++)
		if (strstr(entries[each].path, "cyborg_mp"))
			cyborg = each;
	CHECK(cyborg >= 0 && be->body_set_option(sets[0].folder, "player_biped", entries[cyborg].path) &&
		be->body_option(sets[0].folder, "player_biped", value, sizeof(value)) && ae_is_body_is(value, entries[cyborg].path));
	CHECK(!be->body_option(sets[0].folder, "other", value, sizeof(value)) && !be->body_set_option("nothing", "player_biped", "x"));
	CHECK(be->body_set_option(sets[0].folder, "player_movement", "own") && be->body_option(sets[0].folder, "player_movement", value, sizeof(value)) && !strcmp(value, "own"));
	/* the fit: the Spirit Kit's cyborg fits a player; the Covenant Armory's elite does not; a weapon has none */
	be->entry_fit(sets[0].folder, entries[cyborg].tag, &fit);
	CHECK(fit.known && fit.fits && fit.size_percent == 102);
	be->entry_fit(sets[0].folder, tag, &fit);
	CHECK(!fit.known);
	count = be->entries(sets[1].folder, entries, AE_IS_ENTRIES_MOST);
	for (each = 0; each < count; each++)
		if (strstr(entries[each].path, "elite_mp"))
			elite = each;
	CHECK(elite >= 0 && entries[elite].kind == 4);
	be->entry_fit(sets[1].folder, entries[elite].tag, &fit);
	CHECK(fit.known && !fit.fits && strstr(fit.reason, "physics") && fit.seats_have == 11 && fit.seats_total == 14);
}


/* ---------- campaign swaps and the lobby's picker */

static void swap_rules(void)
{
	struct ae_is_swap_row rows[AE_IS_SWAP_ROWS_MOST];
	char text[160];
	int count, each, slots;

	/* the limits (Content Evolved's) */
	CHECK(!ae_is_limit_word(0, 63)[0] && !strcmp(ae_is_limit_word(0, 64), "64 SETS: THE MOST"));
	CHECK(!ae_is_limit_word(1, 255)[0] && !strcmp(ae_is_limit_word(1, 256), "256 ITEMS: THE MOST"));
	CHECK(!ae_is_limit_word(2, 63)[0] && !strcmp(ae_is_limit_word(2, 64), "64 SWAPS: THE MOST"));
	/* a swap's state: its set gone or empty wins over being the one in use */
	CHECK(ae_is_swap_state(1, 1, 3) == AE_IS_SWAP_IN_USE && ae_is_swap_state(0, 1, 3) == AE_IS_SWAP_OK);
	CHECK(ae_is_swap_state(1, 0, 3) == AE_IS_SWAP_SET_MISSING && ae_is_swap_state(1, 1, 0) == AE_IS_SWAP_NOTHING_ON);
	CHECK(!strcmp(ae_is_swap_state_word(AE_IS_SWAP_IN_USE), "IN USE") && !strcmp(ae_is_swap_state_word(AE_IS_SWAP_SET_MISSING), "SET MISSING") &&
		!strcmp(ae_is_swap_state_word(AE_IS_SWAP_NOTHING_ON), "NOTHING ON") && !ae_is_swap_state_word(AE_IS_SWAP_OK)[0]);
	CHECK(ae_is_swap_state_tone(AE_IS_SWAP_IN_USE) == 2 && ae_is_swap_state_tone(AE_IS_SWAP_SET_MISSING) == 3 && ae_is_swap_state_tone(AE_IS_SWAP_OK) == 0);
	ae_is_swap_sub(1, "Spirit Kit", "manual", "random", text, sizeof(text));
	CHECK(!strcmp(text, "Spirit Kit \xE2\x80\xA2 weapons by weapon \xE2\x80\xA2 enemies at random"));
	ae_is_swap_sub(1, "Covenant Armory", "random", "manual", text, sizeof(text));
	CHECK(!strcmp(text, "Covenant Armory \xE2\x80\xA2 weapons at random \xE2\x80\xA2 enemies by enemy"));
	ae_is_swap_sub(0, "", "random", "random", text, sizeof(text));
	CHECK(!strcmp(text, "Its set is gone"));
	/* the page's rows: the families hide while they do not apply */
	count = ae_is_swap_rows(0, 0, 0, rows);
	CHECK(count == 7 && rows[0].row == AE_IS_SW_NAME && rows[1].row == AE_IS_SW_SET && rows[6].row == AE_IS_SW_BIPED);
	count = ae_is_swap_rows(1, 0, 0, rows);
	for (slots = 0, each = 0; each < count; each++)
		slots += rows[each].row == AE_IS_SW_WEAPON_SLOT;
	CHECK(slots == AE_IS_SWAP_WEAPON_SLOTS && count == 7 + AE_IS_SWAP_WEAPON_SLOTS);
	count = ae_is_swap_rows(0, 1, 1, rows);
	for (slots = 0, each = 0; each < count; each++)
		slots += rows[each].row == AE_IS_SW_ENEMY_SLOT;
	CHECK(slots == AE_IS_SWAP_ENEMY_SLOTS && rows[count - 2].row == AE_IS_SW_HANDS && rows[count - 1].row == AE_IS_SW_MOVEMENT);
	count = ae_is_swap_rows(1, 1, 1, rows);
	CHECK(count == AE_IS_SWAP_ROWS_MOST);
	CHECK(ae_is_swap_row_section(AE_IS_SW_NAME) == AE_IS_SWS_NONE && ae_is_swap_row_section(AE_IS_SW_WEAPON_PICK) == AE_IS_SWS_WEAPONS &&
		ae_is_swap_row_section(AE_IS_SW_ENEMY_SLOT) == AE_IS_SWS_ENEMIES && ae_is_swap_row_section(AE_IS_SW_MOVEMENT) == AE_IS_SWS_BODY);
	/* the values are Content Evolved's keys, the default first */
	CHECK(ae_is_swap_value_count(AE_IS_SW_WEAPON_ROT) == 2 && !strcmp(ae_is_swap_value_key(AE_IS_SW_WEAPON_ROT, 0), "add") &&
		!strcmp(ae_is_swap_value_key(AE_IS_SW_WEAPON_ROT, 1), "only") && !strcmp(ae_is_swap_value_word(AE_IS_SW_WEAPON_ROT, 0), "MIXED IN"));
	CHECK(!strcmp(ae_is_swap_value_key(AE_IS_SW_WEAPON_PICK, 1), "manual") && !strcmp(ae_is_swap_value_word(AE_IS_SW_WEAPON_PICK, 1), "BY WEAPON") &&
		!strcmp(ae_is_swap_value_word(AE_IS_SW_ENEMY_PICK, 1), "BY ENEMY") && !strcmp(ae_is_swap_value_key(AE_IS_SW_ENEMY_PICK, 0), "random"));
	CHECK(!strcmp(ae_is_swap_value_key(AE_IS_SW_MOVEMENT, 1), "own") && !ae_is_swap_value_key(AE_IS_SW_MOVEMENT, 0)[0] &&
		ae_is_swap_value_index(AE_IS_SW_MOVEMENT, "own") == 1 && ae_is_swap_value_index(AE_IS_SW_MOVEMENT, "other") == 0 &&
		ae_is_swap_value_index(AE_IS_SW_ENEMY_ROT, "only") == 1 && ae_is_swap_value_index(AE_IS_SW_ENEMY_ROT, "") == 0);
	CHECK(!strcmp(ae_is_swap_row_file_key(AE_IS_SW_SET), "profile") && !strcmp(ae_is_swap_row_file_key(AE_IS_SW_ENEMY_PICK), "enemy_replace") &&
		!strcmp(ae_is_swap_row_file_key(AE_IS_SW_BIPED), "player_biped") && !ae_is_swap_row_file_key(AE_IS_SW_WEAPON_SLOT)[0]);
	CHECK(!strcmp(ae_is_swap_slot_section(AE_IS_SW_WEAPON_SLOT, 0), "weapons") && !strcmp(ae_is_swap_slot_section(AE_IS_SW_ENEMY_SLOT, 0), "enemies") &&
		!strcmp(ae_is_swap_slot_section(AE_IS_SW_ENEMY_SLOT, 1), "loadout"));
	CHECK(ae_is_swap_row_change(AE_IS_SW_NAME) == 2 && ae_is_swap_row_change(AE_IS_SW_WEAPON_SLOT) == 3 && ae_is_swap_row_change(AE_IS_SW_SET) == 1);
	/* a slot's words */
	ae_is_slot_value(0, text, sizeof(text));
	CHECK(!strcmp(text, "AS IT IS"));
	ae_is_slot_value(2, text, sizeof(text));
	CHECK(!strcmp(text, "2 PICKED"));
	ae_is_slot_count_line(1, text, sizeof(text));
	CHECK(!strcmp(text, "1 PICKED"));
	ae_is_slot_count_line(3, text, sizeof(text));
	CHECK(!strcmp(text, "3 PICKED \xE2\x80\xA2 ONE AT RANDOM EACH TIME"));
	CHECK(!strcmp(ae_is_slot_replaced_head(1), "BECOMES") && !strcmp(ae_is_slot_replaced_head(0), "REPLACED BY") &&
		!strcmp(ae_is_slot_own_word(0), "THE MISSION'S OWN") && !strcmp(ae_is_slot_own_word(1), "THEIR OWN"));
	CHECK(strstr(ae_is_slot_card_text(1, "pistol", 1), "MIXED IN keeps the original") && !strstr(ae_is_slot_card_text(1, "pistol", 0), "MIXED IN"));
}

static void picker_rules(void)
{
	struct ae_is_pick_writes writes;
	char text[160];

	/* what each pick writes: NONE clears both, a swap sets the swap alone, a set sets the randomizer's set and clears the swap */
	ae_is_pick_writes(AE_IS_PICK_NONE, &writes);
	CHECK(writes.clear_swap && writes.clear_set && !writes.swap_file && !writes.set_folder && !writes.injection);
	ae_is_pick_writes(AE_IS_PICK_SWAP, &writes);
	CHECK(writes.swap_file && !writes.clear_swap && !writes.clear_set && !writes.set_folder && writes.injection);
	ae_is_pick_writes(AE_IS_PICK_SET, &writes);
	CHECK(writes.set_folder && writes.clear_swap && !writes.swap_file && writes.injection);
	/* the waits, in the picker's order of reasons */
	CHECK(!strcmp(ae_is_pick_set_wait(0, 1, 0), "NO ITEMS ON") && !strcmp(ae_is_pick_set_wait(3, 0, 0), "NEEDS RANDOMIZER") &&
		!strcmp(ae_is_pick_set_wait(3, 1, 1), "NOT ON MCC MISSIONS") && !ae_is_pick_set_wait(3, 1, 0)[0]);
	CHECK(!strcmp(ae_is_pick_swap_wait(AE_IS_SWAP_SET_MISSING), "SET MISSING") && !strcmp(ae_is_pick_swap_wait(AE_IS_SWAP_NOTHING_ON), "NOTHING ON") &&
		!ae_is_pick_swap_wait(AE_IS_SWAP_IN_USE)[0] && !ae_is_pick_swap_wait(AE_IS_SWAP_OK)[0]);
	ae_is_pick_swap_value(AE_IS_SWAP_IN_USE, "Spirit Kit", text, sizeof(text));
	CHECK(!strcmp(text, "IN USE"));
	ae_is_pick_swap_value(AE_IS_SWAP_OK, "Spirit Kit", text, sizeof(text));
	CHECK(!strcmp(text, "Spirit Kit"));
	ae_is_pick_set_value(0, 4, text, sizeof(text));
	CHECK(!strcmp(text, "4 ITEMS ON"));
	ae_is_pick_set_value(1, 4, text, sizeof(text));
	CHECK(!strcmp(text, "IN USE") && !strcmp(ae_is_pick_none_value(1), "IN USE") && !strcmp(ae_is_pick_none_value(0), "THE CAMPAIGN'S OWN"));
	ae_is_pick_swap_help("Spirit Run", "Spirit Kit", text, sizeof(text));
	CHECK(strstr(text, "Spirit Run: Spirit Kit's weapons and enemies take the mission's places, and the randomizer is passed over."));
	/* the lobby row: only with a set that has an item on, never in a challenge or an online co-op lobby */
	CHECK(ae_is_lobby_row_shown(1, 0, 0) && !ae_is_lobby_row_shown(0, 0, 0) && !ae_is_lobby_row_shown(1, 1, 0) && !ae_is_lobby_row_shown(1, 0, 1));
	ae_is_lobby_value(AE_IS_PICK_NONE, "", text, sizeof(text));
	CHECK(!strcmp(text, "NONE"));
	ae_is_lobby_value(AE_IS_PICK_SWAP, "Spirit Run", text, sizeof(text));
	CHECK(!strcmp(text, "SPIRIT RUN"));
	ae_is_lobby_value(AE_IS_PICK_SET, "Spirit Kit", text, sizeof(text));
	CHECK(!strcmp(text, "SPIRIT KIT \xE2\x80\xA2 RANDOMIZER"));
	CHECK(!strcmp(ae_is_randomizer_wait(1), "SWAP PLAYS INSTEAD") && !ae_is_randomizer_wait(0)[0] && strstr(ae_is_randomizer_wait_help(), "Pick NONE or a set"));
	/* the firing range row's waits */
	CHECK(!strcmp(ae_is_range_wait(0, 1), "NOTHING ON") && !strcmp(ae_is_range_wait(2, 0), "NEEDS BLOOD GULCH") && !ae_is_range_wait(2, 1)[0]);
}

/* the made-up table: three swaps (one in use, one whose set is gone), their lists and the picks it keeps */
static void staged_swaps(void)
{
	struct ae_is_backend const *be = ae_is_backend_staged(0);
	struct ae_is_swap swaps[AE_IS_SWAPS_MOST];
	struct ae_is_set sets[AE_IS_SETS_MOST];
	char value[256], id[64], file[64];
	int count = be->swaps(swaps, AE_IS_SWAPS_MOST), kind;

	CHECK(count == 3 && !strcmp(swaps[0].name, "Spirit Run") && swaps[0].in_use && swaps[0].set_found && swaps[0].set_items_on > 0);
	CHECK(!strcmp(swaps[0].weapon_replace, "manual") && !strcmp(swaps[0].enemy_replace, "random"));
	CHECK(!swaps[1].in_use && !strcmp(swaps[1].enemy_replace, "manual") && !swaps[2].set_found);
	CHECK(ae_is_swap_state(swaps[0].in_use, swaps[0].set_found, swaps[0].set_items_on) == AE_IS_SWAP_IN_USE &&
		ae_is_swap_state(swaps[2].in_use, swaps[2].set_found, swaps[2].set_items_on) == AE_IS_SWAP_SET_MISSING);
	/* the lists: a slot's picks, toggled */
	CHECK(be->swap_list_count("Spirit Run", "weapons", "pistol") == 2 && be->swap_list_has("Spirit Run", "weapons", "pistol", "cmt\\weapons\\evolved_h1-spirit\\assault_rifle\\assault_rifle_mp"));
	CHECK(be->swap_list_set("Spirit Run", "weapons", "pistol", "original", 1) && be->swap_list_count("Spirit Run", "weapons", "pistol") == 3);
	CHECK(be->swap_list_set("Spirit Run", "weapons", "pistol", "original", 0) && be->swap_list_count("Spirit Run", "weapons", "pistol") == 2 &&
		!be->swap_list_has("Spirit Run", "weapons", "pistol", "original"));
	CHECK(be->swap_list_get("Spirit Run", "weapons", "pistol", 0, value, sizeof(value)) && strstr(value, "pistol_spirit_variant_mp"));
	CHECK(be->swap_list_count("Brute Invasion", "loadout", "elite_major") == 1 && be->swap_list_count("Spirit Run", "loadout", "elite_major") == 0);
	/* the file's keys */
	CHECK(be->swap_option("Spirit Run", "weapon_replace", value, sizeof(value)) && !strcmp(value, "manual"));
	CHECK(be->swap_set_option("Spirit Run", "weapon_replace", "random") && be->swap_option("Spirit Run", "weapon_replace", value, sizeof(value)) && !strcmp(value, "random"));
	CHECK(be->swap_option("Spirit Run", "player_biped", value, sizeof(value)) && strstr(value, "cyborg_mp"));
	CHECK(!be->swap_option("Nothing", "profile", value, sizeof(value)) && !be->swap_set_option("Nothing", "profile", "x"));
	CHECK(be->swap_rename("Spirit Run", "Spirit Sprint") && be->swap_option("Spirit Run", "name", value, sizeof(value)) && !strcmp(value, "Spirit Sprint"));
	/* a swap made, then deleted; the one in use deleted leaves the lobby on NONE */
	CHECK(be->swap_create("Test Swap", "Test Bench", file, sizeof(file)) && !strcmp(file, "Test Swap") && be->swaps(swaps, AE_IS_SWAPS_MOST) == 4);
	CHECK(!be->swap_create("Test Swap", "Test Bench", file, sizeof(file)));
	CHECK(be->swap_option(file, "profile", value, sizeof(value)) && !strcmp(value, "Test Bench") && be->swap_option(file, "weapon_rotation", value, sizeof(value)) &&
		!strcmp(value, "add") && be->swap_option(file, "enemy_replace", value, sizeof(value)) && !strcmp(value, "random"));
	CHECK(be->swap_remove("Test Swap") && be->swaps(swaps, AE_IS_SWAPS_MOST) == 3 && !be->swap_remove("Test Swap"));
	kind = be->picked(id, sizeof(id));
	CHECK(kind == AE_IS_PICK_SWAP && !strcmp(id, "Spirit Run"));
	CHECK(be->swap_remove("Spirit Run") && be->picked(id, sizeof(id)) == AE_IS_PICK_NONE && !id[0]);
	/* the picks: a set plays through the randomizer, NONE puts the choice back */
	be = ae_is_backend_staged(0);
	be->sets(sets, AE_IS_SETS_MOST);
	CHECK(be->pick(AE_IS_PICK_SET, sets[1].folder) && be->picked(id, sizeof(id)) == AE_IS_PICK_SET && !strcmp(id, sets[1].folder));
	CHECK(be->pick(AE_IS_PICK_NONE, "") && be->picked(id, sizeof(id)) == AE_IS_PICK_NONE);
	CHECK(be->pick(AE_IS_PICK_SWAP, "Brute Invasion") && be->picked(id, sizeof(id)) == AE_IS_PICK_SWAP && !strcmp(id, "Brute Invasion"));
	/* the firing range and the campaign's paths */
	CHECK(be->range_ready() && be->range_start("Spirit Kit", value, sizeof(value)));
	ae_is_staged_block_ce(1);
	CHECK(!be->range_ready() && !be->range_start("Spirit Kit", value, sizeof(value)) && strstr(value, "bloodgulch"));
	ae_is_staged_block_ce(0);
	be->sets(sets, AE_IS_SETS_MOST);
	{
		struct ae_is_entry entries[AE_IS_ENTRIES_MOST];
		int each, stock = 0, other = 0, entry_count = be->entries(sets[0].folder, entries, AE_IS_ENTRIES_MOST);

		for (each = 0; each < entry_count; each++)
		{
			if (be->entry_stock(sets[0].folder, entries[each].tag))
				stock++;
			else
				other++;
		}
		CHECK(stock > 0 && other > 0);
	}
}

int main(void)
{
	swap_rules();
	picker_rules();
	staged_swaps();
	names_and_paths();
	words();
	plays();
	footers();
	staged();
	settings();
	auto_answers();
	body();
	staged_settings();
	if (failures)
		printf("%d failures\n", failures);
	else
		printf("ae_item_sets_rules_test: ok\n");
	return failures != 0;
}
