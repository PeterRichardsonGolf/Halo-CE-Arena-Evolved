#include <stdio.h>
#include <string.h>
#include "ae_item_sets_data.h"

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

int main(void)
{
	names_and_paths();
	words();
	plays();
	footers();
	staged();
	if (failures)
		printf("%d failures\n", failures);
	else
		printf("ae_item_sets_rules_test: ok\n");
	return failures != 0;
}
