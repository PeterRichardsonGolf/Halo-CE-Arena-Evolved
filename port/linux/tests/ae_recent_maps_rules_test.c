/*
AE_RECENT_MAPS_RULES_TEST.C

The recent-maps memory (port/linux/game/ae_recent_maps_rules.c): rotation, a stand-in and its Workshop map as one map, small playlists,
the text that survives a restart.
*/

#include <stdio.h>
#include <string.h>

#include "../game/ae_recent_maps_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)
#define EQ(a, b) do { long x_ = (long)(a), y_ = (long)(b); if (x_ != y_) { printf("FAIL %s:%d %s = %ld, wanted %ld\n", __FILE__, __LINE__, #a, x_, y_); failures++; } } while (0)

/* a playlist of count maps named m0, m1, ...; entry stand_in (when > 0) stands in for entry 0 */
static void make(struct ae_social_playlist *list, int count, int stand_in)
{
	int at;

	memset(list, 0, sizeof(*list));
	snprintf(list->id, sizeof(list->id), "tdm");
	list->count = count;
	for (at = 0; at < count; at++)
	{
		snprintf(list->entry[at].map, sizeof(list->entry[at].map), "m%d", at);
		list->entry[at].weight = 2;
	}
	if (stand_in > 0)
		list->entry[stand_in].stand_for = 1;
}

static int all_candidates(int count, int *out)
{
	int at;

	for (at = 0; at < count; at++)
		out[at] = at;
	return count;
}

static void test_window(void)
{
	EQ(ae_recent_window(27), 8);
	EQ(ae_recent_window(24), 8);
	EQ(ae_recent_window(18), 6);
	EQ(ae_recent_window(9), 3);
	EQ(ae_recent_window(6), 2);
	/* (fewer than three others would remain: the window shrinks) */
	EQ(ae_recent_window(5), 1);
	EQ(ae_recent_window(4), 1);
	EQ(ae_recent_window(3), 0);
	EQ(ae_recent_window(2), 0);
	EQ(ae_recent_window(0), 0);
}

static void test_rotation(void)
{
	struct ae_recent_maps recent;
	struct ae_recent_list *list;
	int at;

	memset(&recent, 0, sizeof(recent));
	list = ae_recent_list_get(&recent, "tdm", 1);
	CHECK(list != NULL);
	CHECK(ae_recent_list_get(&recent, "tdm", 0) == list);
	CHECK(ae_recent_list_get(&recent, "ffa", 0) == NULL);
	for (at = 0; at < 12; at++)
	{
		char key[16];

		snprintf(key, sizeof(key), "m%d", at);
		ae_recent_push(list, key);
	}
	EQ(list->count, AE_RECENT_MAX);
	CHECK(!strcmp(list->key[0], "m11") && !strcmp(list->key[7], "m4"));
	EQ(ae_recent_age(list, "m11", 8), 0);
	EQ(ae_recent_age(list, "m4", 8), 7);
	EQ(ae_recent_age(list, "m4", 7), -1);
	EQ(ae_recent_age(list, "m3", 8), -1);
	/* (a map played again moves to the front: the list keeps one of it) */
	ae_recent_push(list, "m8");
	CHECK(!strcmp(list->key[0], "m8") && !strcmp(list->key[1], "m11"));
	EQ(list->count, AE_RECENT_MAX);
	ae_recent_push(list, "");
	EQ(list->count, AE_RECENT_MAX);
}

static void test_hidden(void)
{
	struct ae_social_playlist list;
	struct ae_recent_maps recent;
	struct ae_recent_list *mem;
	int candidates[AE_SOCIAL_ENTRIES], hidden[AE_SOCIAL_ENTRIES], count;

	/* 12 maps: the window is 4; m5 (newest) .. m2 are hidden, m1 fell out of the window */
	make(&list, 12, 0);
	count = all_candidates(12, candidates);
	memset(&recent, 0, sizeof(recent));
	mem = ae_recent_list_get(&recent, "tdm", 1);
	ae_recent_push(mem, "m1"); ae_recent_push(mem, "m2"); ae_recent_push(mem, "m3"); ae_recent_push(mem, "m4");
	ae_recent_push(mem, "m5");
	EQ(ae_recent_hidden(&list, candidates, count, mem, 3, hidden), 4);
	CHECK(hidden[5] && hidden[4] && hidden[3] && hidden[2] && !hidden[1] && !hidden[0]);
	/* (no memory: nothing hidden) */
	EQ(ae_recent_hidden(&list, candidates, count, NULL, 3, hidden), 0);

	/* 5 maps: the window is 1 (4 others remain for a vote of 3): only m5 would be recent, and it is not here */
	make(&list, 5, 0);
	count = all_candidates(5, candidates);
	EQ(ae_recent_hidden(&list, candidates, count, mem, 3, hidden), 0);
	/* 6 maps: the window is 2, m5 and m4 are hidden, 4 others remain */
	make(&list, 6, 0);
	count = all_candidates(6, candidates);
	EQ(ae_recent_hidden(&list, candidates, count, mem, 3, hidden), 2);
	CHECK(hidden[5] && hidden[4]);
	/* a vote that wants 5 of the 6: one recent map comes back, the oldest of the two */
	EQ(ae_recent_hidden(&list, candidates, count, mem, 5, hidden), 1);
	CHECK(hidden[5] && !hidden[4]);
	/* wanting all of them: none hidden */
	EQ(ae_recent_hidden(&list, candidates, count, mem, 6, hidden), 0);
}

static void test_stand_in(void)
{
	struct ae_social_playlist list;
	struct ae_recent_maps recent;
	struct ae_recent_list *mem;
	int candidates[AE_SOCIAL_ENTRIES], hidden[AE_SOCIAL_ENTRIES], count;
	char key[AE_RECENT_KEY];

	/* 13 entries; entry 12 stands in for entry 0: the same map */
	make(&list, 13, 12);
	ae_recent_key_of(&list, 12, key, sizeof(key));
	CHECK(!strcmp(key, "m0"));
	ae_recent_key_of(&list, 3, key, sizeof(key));
	CHECK(!strcmp(key, "m3"));
	ae_recent_key_of(&list, 99, key, sizeof(key));
	CHECK(!key[0]);
	count = all_candidates(13, candidates);
	EQ(ae_recent_playable(&list, candidates, count), 12);
	memset(&recent, 0, sizeof(recent));
	mem = ae_recent_list_get(&recent, "tdm", 1);
	ae_recent_push(mem, "m0");
	/* (the Workshop map was played: its stand-in is hidden with it) */
	EQ(ae_recent_hidden(&list, candidates, count, mem, 3, hidden), 2);
	CHECK(hidden[0] && hidden[12]);
}

static void test_limits(void)
{
	static struct ae_recent_maps full, again;
	static char text[AE_RECENT_TEXT];
	struct ae_recent_list *mem;
	int list, key, length;
	char name[16], longkey[AE_RECENT_KEY];

	/* (the worst case: every list full of keys of the longest name fits the text and reads back whole) */
	for (list = 0; list < AE_RECENT_LISTS; list++)
	{
		char id[AE_SOCIAL_ID];

		memset(id, 'p', sizeof(id) - 1);
		id[sizeof(id) - 1] = 0;
		id[18] = (char)(48 + (10 + list) / 10 % 10); id[19] = (char)(48 + (10 + list) % 10); id[20] = 0;
		mem = ae_recent_list_get(&full, id, 1);
		CHECK(mem != NULL);
		for (key = 0; key < AE_RECENT_MAX; key++)
		{
			size_t used;

			snprintf(longkey, sizeof(longkey), "%02d_%d", list, key);
			used = strlen(longkey);
			memset(longkey + used, 'x', sizeof(longkey) - 1 - used);
			longkey[sizeof(longkey) - 1] = 0;
			ae_recent_push(mem, longkey);
		}
	}
	EQ(full.count, AE_RECENT_LISTS);
	length = ae_recent_write(&full, text, sizeof(text));
	CHECK(length > 8000 && length < (int)sizeof(text));
	EQ(ae_recent_read(&again, text), AE_RECENT_LISTS);
	for (list = 0; list < AE_RECENT_LISTS; list++)
		EQ(again.list[list].count, AE_RECENT_MAX);
	/* (all lists taken: a new playlist gets a memory, the oldest list goes) */
	snprintf(name, sizeof(name), "brandnew");
	mem = ae_recent_list_get(&full, name, 1);
	CHECK(mem != NULL);
	EQ(full.count, AE_RECENT_LISTS);
	CHECK(ae_recent_list_get(&full, "brandnew", 0) == mem);
	CHECK(!strcmp(full.list[0].playlist, again.list[1].playlist));
}

static void test_text(void)
{
	struct ae_recent_maps recent, again;
	struct ae_recent_list *mem;
	char text[AE_RECENT_TEXT];
	int length;

	memset(&recent, 0, sizeof(recent));
	mem = ae_recent_list_get(&recent, "tdm", 1);
	ae_recent_push(mem, "warren flycam pm@ws2900072240"); ae_recent_push(mem, "45@ws3477809506"); ae_recent_push(mem, "prisoner");
	mem = ae_recent_list_get(&recent, "ffa", 1);
	ae_recent_push(mem, "wizard");
	length = ae_recent_write(&recent, text, sizeof(text));
	CHECK(length > 0);
	CHECK(!strcmp(text, "tdm\tprisoner\t45@ws3477809506\twarren flycam pm@ws2900072240\nffa\twizard\n"));
	EQ(ae_recent_read(&again, text), 2);
	mem = ae_recent_list_get(&again, "tdm", 0);
	CHECK(mem && mem->count == 3 && !strcmp(mem->key[0], "prisoner") && !strcmp(mem->key[2], "warren flycam pm@ws2900072240"));
	/* (after a restart the next push goes first) */
	ae_recent_push(mem, "hang");
	CHECK(!strcmp(mem->key[0], "hang") && !strcmp(mem->key[1], "prisoner"));
	EQ(ae_recent_write(&recent, text, 8), -1);
	EQ(ae_recent_read(&again, ""), 0);
	EQ(ae_recent_read(&again, "\n\n\t\nonly\n"), 1);
	mem = ae_recent_list_get(&again, "only", 0);
	CHECK(mem && mem->count == 0);
}

int main(void)
{
	test_window();
	test_rotation();
	test_hidden();
	test_stand_in();
	test_limits();
	test_text();
	if (failures)
		printf("%d failures\n", failures);
	else
		printf("ok\n");
	return failures ? 1 : 0;
}
