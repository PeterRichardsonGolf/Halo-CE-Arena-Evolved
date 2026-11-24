#include <stdio.h>
#include <string.h>
#include "ae_collections_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static void item(struct ae_coll *coll, char const *id, char const *title, unsigned long long mb, int state, int steam)
{
	struct ae_coll_item *it = &coll->items[coll->count++];

	memset(it, 0, sizeof(*it));
	snprintf(it->id, sizeof(it->id), "%s", id);
	snprintf(it->title, sizeof(it->title), "%s", title);
	it->size = mb * 1000000ull;
	it->state = state;
	it->steam_owned = steam;
	it->percent = 42;
}

int main(void)
{
	struct ae_coll coll;
	struct ae_coll_summary sum;
	char text[200], label[80], value[40], ids[AE_COLL_ITEMS][AE_COLL_ID];
	int ink, action, count;

	/* sizes and names */
	ae_coll_size_text(96000000ull, text, sizeof(text));
	CHECK(!strcmp(text, "96 MB"));
	ae_coll_size_text(1840000000ull, text, sizeof(text));
	CHECK(!strcmp(text, "1.8 GB"));
	ae_coll_size_text(0, text, sizeof(text));
	CHECK(!text[0]);
	ae_coll_map_name("Halo CE - Cold Shoulder -SMP", text, sizeof(text));
	CHECK(!strcmp(text, "Cold Shoulder"));
	ae_coll_map_name("Halo CE - Dredwerkz - SMP", text, sizeof(text));
	CHECK(!strcmp(text, "Dredwerkz"));
	ae_coll_map_name("Plain Name", text, sizeof(text));
	CHECK(!strcmp(text, "Plain Name"));
	ae_coll_display_name("SOCIAL MAP PACK", text, sizeof(text));
	CHECK(!strcmp(text, "Social Map Pack"));
	ae_coll_display_name("2V2 ARENA MAPS", text, sizeof(text));
	CHECK(!strcmp(text, "2v2 Arena Maps"));
	ae_coll_display_name("PRO EDITION", text, sizeof(text));
	CHECK(!strcmp(text, "Pro Edition"));
	ae_coll_display_name("A VERY LONG COLLECTION NAME", text, 8);
	CHECK(strlen(text) < 8);

	/* part of a collection on this PC: three present, one the game cannot remove, 16 not here */
	memset(&coll, 0, sizeof(coll));
	item(&coll, "1", "Halo CE - Blood Covenant - SMP", 100, AE_COLL_ITEM_PRESENT, 0);
	item(&coll, "2", "Halo CE - Dredwerkz - SMP", 150, AE_COLL_ITEM_PRESENT, 1);
	item(&coll, "3", "Halo CE - Hektik - SMP", 90, AE_COLL_ITEM_PRESENT, 0);
	item(&coll, "4", "Halo CE - Ascetic - SMP", 96, AE_COLL_ITEM_ABSENT, 0);
	item(&coll, "5", "Halo CE - Co2 - SMP", 91, AE_COLL_ITEM_ABSENT, 0);
	ae_coll_summarize(&coll, &sum);
	CHECK(sum.count == 5 && sum.present == 3 && sum.absent == 2 && sum.updates == 0);
	CHECK(sum.removable == 2 && sum.steam_kept == 1 && sum.removable_bytes == 190000000ull);
	CHECK(sum.absent_bytes == 187000000ull && sum.sizes_known);
	ae_coll_row_value(&sum, 1, text, sizeof(text), &ink);
	CHECK(!strcmp(text, "3 OF 5 ON THIS PC") && ink == AE_COLL_INK_MUTED);
	ae_coll_row_value(&sum, 0, text, sizeof(text), &ink);
	CHECK(!strcmp(text, "NO ROOM") && ink == AE_COLL_INK_WARNING);
	action = ae_coll_first_action(&sum);
	CHECK(action == AE_COLL_ACT_GET);
	ae_coll_action_text(action, &sum, 1, label, sizeof(label), value, sizeof(value), &ink);
	CHECK(!strcmp(label, "GET THE 2 MAPS NOT ON THIS PC") && !strcmp(value, "187 MB"));
	ae_coll_action_text(action, &sum, 0, label, sizeof(label), value, sizeof(value), &ink);
	CHECK(!strcmp(value, "NO ROOM") && ink == AE_COLL_INK_WARNING);
	count = ae_coll_get_ids(&coll, &sum, ids, AE_COLL_ITEMS);
	CHECK(count == 2 && !strcmp(ids[0], "4") && !strcmp(ids[1], "5"));
	count = ae_coll_remove_ids(&coll, ids, AE_COLL_ITEMS);
	CHECK(count == 2 && !strcmp(ids[0], "1") && !strcmp(ids[1], "3"));
	count = ae_coll_remove_ids(&coll, ids, 1);
	CHECK(count == 1);
	ae_coll_remove_body("Pack", &coll, &sum, text, sizeof(text));
	CHECK(strstr(text, "2 maps, 190 MB, are deleted") && strstr(text, "Dredwerkz stays"));
	ae_coll_get_body("Pack", &sum, 41000000000ull, text, sizeof(text));
	CHECK(strstr(text, "2 maps, 187 MB") && strstr(text, "41.0 GB free"));

	/* nothing here: GET ALL, and the value with its size */
	memset(&coll, 0, sizeof(coll));
	item(&coll, "1", "A", 300, AE_COLL_ITEM_ABSENT, 0);
	item(&coll, "2", "B", 250, AE_COLL_ITEM_ABSENT, 0);
	ae_coll_summarize(&coll, &sum);
	ae_coll_row_value(&sum, 1, text, sizeof(text), &ink);
	CHECK(!strcmp(text, "2 MAPS \xE2\x80\xA2 550 MB"));
	CHECK(ae_coll_first_action(&sum) == AE_COLL_ACT_GET_ALL);
	CHECK(ae_coll_get_asks(ae_coll_get_bytes(&sum)));
	CHECK(!ae_coll_get_asks(499999999ull) && ae_coll_get_asks(500000000ull));
	CHECK(ae_coll_room(0, 99) && ae_coll_room(100, 100) && !ae_coll_room(99, 100));
	/* (a size Steam has not told) */
	coll.items[1].size = 0;
	ae_coll_summarize(&coll, &sum);
	CHECK(!sum.sizes_known);
	ae_coll_row_value(&sum, 1, text, sizeof(text), &ink);
	CHECK(!strcmp(text, "2 MAPS"));

	/* being got: a map done, one under way, one waiting */
	memset(&coll, 0, sizeof(coll));
	item(&coll, "1", "A", 100, AE_COLL_ITEM_PRESENT, 0);
	item(&coll, "2", "B", 100, AE_COLL_ITEM_GETTING, 0);
	item(&coll, "3", "C", 100, AE_COLL_ITEM_WAITING, 0);
	ae_coll_summarize(&coll, &sum);
	ae_coll_row_value(&sum, 1, text, sizeof(text), &ink);
	CHECK(!strcmp(text, "GETTING 1 OF 3") && ink == AE_COLL_INK_ACCENT);
	action = ae_coll_first_action(&sum);
	CHECK(action == AE_COLL_ACT_STOP);
	ae_coll_action_text(action, &sum, 1, label, sizeof(label), value, sizeof(value), &ink);
	CHECK(!strcmp(label, "STOP GETTING THEM") && !strcmp(value, "1 OF 3") && ink == AE_COLL_INK_ACCENT);
	count = ae_coll_stop_ids(&coll, ids, AE_COLL_ITEMS);
	CHECK(count == 2 && !strcmp(ids[0], "2") && !strcmp(ids[1], "3"));
	CHECK(sum.left_bytes == 200000000ull);
	/* (a queued map is never in a remove) */
	count = ae_coll_remove_ids(&coll, ids, AE_COLL_ITEMS);
	CHECK(count == 1 && !strcmp(ids[0], "1"));
	ae_coll_item_value(&coll.items[1], text, sizeof(text), &ink);
	CHECK(!strcmp(text, "42 %") && ink == AE_COLL_INK_ACCENT);
	ae_coll_item_value(&coll.items[2], text, sizeof(text), &ink);
	CHECK(!strcmp(text, "WAITING"));
	/* only waiting: STOP WAITING */
	coll.items[1].state = AE_COLL_ITEM_PRESENT;
	ae_coll_summarize(&coll, &sum);
	CHECK(ae_coll_first_action(&sum) == AE_COLL_ACT_STOP_WAITING);
	ae_coll_row_value(&sum, 1, text, sizeof(text), &ink);
	CHECK(!strcmp(text, "WAITING"));

	/* all here: INSTALLED, then UPDATE with updates, then REMOVE alone */
	memset(&coll, 0, sizeof(coll));
	item(&coll, "1", "A", 100, AE_COLL_ITEM_PRESENT, 0);
	item(&coll, "2", "B", 112, AE_COLL_ITEM_PRESENT, 1);
	ae_coll_summarize(&coll, &sum);
	ae_coll_row_value(&sum, 1, text, sizeof(text), &ink);
	CHECK(!strcmp(text, "INSTALLED") && ae_coll_first_action(&sum) == AE_COLL_ACT_NONE);
	ae_coll_item_value(&coll.items[1], text, sizeof(text), &ink);
	CHECK(!strcmp(text, "IN STEAM"));
	coll.items[0].state = AE_COLL_ITEM_UPDATE;
	ae_coll_summarize(&coll, &sum);
	ae_coll_row_value(&sum, 1, text, sizeof(text), &ink);
	CHECK(!strcmp(text, "UPDATE") && ink == AE_COLL_INK_ACCENT);
	action = ae_coll_first_action(&sum);
	CHECK(action == AE_COLL_ACT_UPDATES);
	ae_coll_action_text(action, &sum, 1, label, sizeof(label), value, sizeof(value), &ink);
	CHECK(!strcmp(label, "GET 1 UPDATE") && !strcmp(value, "100 MB"));
	CHECK(ae_coll_get_ids(&coll, &sum, ids, AE_COLL_ITEMS) == 1 && !strcmp(ids[0], "1"));
	/* every map is Steam's: nothing to remove */
	coll.items[0].steam_owned = 1;
	ae_coll_summarize(&coll, &sum);
	CHECK(sum.removable == 0 && ae_coll_remove_ids(&coll, ids, AE_COLL_ITEMS) == 0);

	/* sizes not known yet: the get waits, no room claim; a map Steam did not answer; a collection too large for the view */
	memset(&coll, 0, sizeof(coll));
	item(&coll, "1", "A", 300, AE_COLL_ITEM_ABSENT, 0);
	item(&coll, "2", "B", 0, AE_COLL_ITEM_ABSENT, 0);
	ae_coll_summarize(&coll, &sum);
	CHECK(ae_coll_get_unknown(&sum) == 1 && !ae_coll_get_ready(&sum));
	CHECK(ae_coll_room_for(&sum, 1) == 1);
	ae_coll_action_text(ae_coll_first_action(&sum), &sum, 1, label, sizeof(label), value, sizeof(value), &ink);
	CHECK(!strcmp(value, "ASKING STEAM"));
	coll.items[1].failed = 1;
	ae_coll_summarize(&coll, &sum);
	CHECK(sum.failed_get == 1 && !ae_coll_get_ready(&sum));
	ae_coll_action_text(ae_coll_first_action(&sum), &sum, 1, label, sizeof(label), value, sizeof(value), &ink);
	CHECK(!strcmp(value, "NO ANSWER") && ink == AE_COLL_INK_WARNING);
	ae_coll_item_value(&coll.items[1], text, sizeof(text), &ink);
	CHECK(!strcmp(text, "NO ANSWER") && ink == AE_COLL_INK_WARNING);
	coll.items[1].failed = 0;
	coll.items[1].size = 100000000ull;
	ae_coll_summarize(&coll, &sum);
	CHECK(ae_coll_get_ready(&sum) && !ae_coll_room_for(&sum, 100));
	coll.cut = 1;
	ae_coll_summarize(&coll, &sum);
	ae_coll_row_value(&sum, 1, text, sizeof(text), &ink);
	CHECK(!strcmp(text, "TOO MANY MAPS") && ink == AE_COLL_INK_WARNING && ae_coll_first_action(&sum) == AE_COLL_ACT_NONE);
	/* a map in the game's folder with no download record is kept, and says so */
	memset(&coll, 0, sizeof(coll));
	item(&coll, "1", "A", 100, AE_COLL_ITEM_PRESENT, 2);
	ae_coll_summarize(&coll, &sum);
	ae_coll_item_value(&coll.items[0], text, sizeof(text), &ink);
	CHECK(!strcmp(text, "ON THIS PC") && sum.removable == 0 && sum.steam_kept == 1);

	/* an empty collection */
	memset(&coll, 0, sizeof(coll));
	ae_coll_summarize(&coll, &sum);
	CHECK(sum.count == 0 && ae_coll_first_action(&sum) == AE_COLL_ACT_NONE);

	if (failures)
		return 1;
	printf("ae_collections_rules_test: ok\n");
	return 0;
}
