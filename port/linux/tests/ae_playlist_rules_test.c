#define _DEFAULT_SOURCE
#include <stdio.h>
#include <string.h>
#include "ae_playlist_rules.h"
#include "ae_server_listing_rules.h"
#include "ae_rotation_shuffle.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static void weights(void)
{
	int w;

	CHECK(ae_pl_weight(0) == 1 && ae_pl_weight(9) == 1 && ae_pl_weight(-2) == 1);
	CHECK(ae_pl_weight(3) == 3 && ae_pl_weight(5) == 5);
	CHECK(ae_pl_weight_parse("4", &w) && w == 4);
	CHECK(!ae_pl_weight_parse("0", &w) && !ae_pl_weight_parse("6", &w) && !ae_pl_weight_parse("12", &w) &&
		!ae_pl_weight_parse("", &w) && !ae_pl_weight_parse("a", &w));
}

/* a weight 5 entry is drawn about five times as often as a weight 1 one, and no entry twice in one vote */
static void sample(void)
{
	int wt[4] = { 5, 1, 1, 1 };
	unsigned char all[4] = { 1, 1, 1, 1 };
	unsigned char three[4] = { 0, 1, 1, 1 };
	unsigned long state = 12345;
	int out[4], firsts[4] = { 0 }, trial, n, i;

	for (trial = 0; trial < 4000; trial++)
	{
		n = ae_pl_sample(wt, all, 4, 3, &state, out);
		CHECK(n == 3);
		CHECK(out[0] != out[1] && out[1] != out[2] && out[0] != out[2]);
		firsts[out[0]]++;
	}
	/* (5/8 of first draws are entry 0, 1/8 each of the others) */
	CHECK(firsts[0] > 2200 && firsts[0] < 2800);
	for (i = 1; i < 4; i++)
		CHECK(firsts[i] > 300 && firsts[i] < 700);
	/* (an ineligible entry is never drawn; fewer eligible than wanted: those) */
	n = ae_pl_sample(wt, three, 4, 4, &state, out);
	CHECK(n == 3);
	for (i = 0; i < n; i++)
		CHECK(out[i] != 0);
	CHECK(ae_pl_sample(NULL, all, 4, 0, &state, out) == 0);
	{
		unsigned char none[4] = { 0 };

		CHECK(ae_pl_sample(wt, none, 4, 2, &state, out) == 0);
	}
}

static void history(void)
{
	struct ae_pl_history h;
	int i;

	memset(&h, 0, sizeof(h));
	CHECK(!ae_pl_history_recent(&h, "a", 2));
	ae_pl_history_push(&h, "a");
	ae_pl_history_push(&h, "b");
	ae_pl_history_push(&h, "c");
	CHECK(ae_pl_history_recent(&h, "c", 2) && ae_pl_history_recent(&h, "B", 2));
	CHECK(!ae_pl_history_recent(&h, "a", 2));
	CHECK(ae_pl_history_recent(&h, "a", 3));
	CHECK(!ae_pl_history_recent(&h, "c", 0));
	for (i = 0; i < 20; i++)
	{
		char t[16];

		snprintf(t, sizeof(t), "e%d", i);
		ae_pl_history_push(&h, t);
	}
	CHECK(h.count == AE_PL_HISTORY_MAXIMUM);
	CHECK(ae_pl_history_recent(&h, "e19", 1) && !ae_pl_history_recent(&h, "e19", 0));
	CHECK(!ae_pl_history_recent(&h, "c", 99));
}

static void nominate(void)
{
	char const *names[4] = { "bloodgulch", "sidewinder", "sidewinder", "hangemhigh" };
	char const *texts[4] = { "bloodgulch:ae_ctf", "sidewinder:ae_ctf", "sidewinder:ae_koh", "hangemhigh:ae_ts" };
	char const *same[3] = { "carousel", "carousel_two", "damnation" };
	char const *same_text[3] = { "carousel:a", "carousel_two:a", "damnation:a" };
	struct ae_pl_people p;
	int out[AE_PL_PEOPLE];

	CHECK(ae_pl_match_entry(names, texts, 4, "bloodgulch") == 0);
	CHECK(ae_pl_match_entry(names, texts, 4, "BLOOD") == 0);
	CHECK(ae_pl_match_entry(names, texts, 4, "side") == 1); /* (one map, two entries: the first) */
	CHECK(ae_pl_match_entry(names, texts, 4, "hang") == 3);
	CHECK(ae_pl_match_entry(names, texts, 4, "zzz") == -1);
	CHECK(ae_pl_match_entry(names, texts, 4, "") == -1);
	CHECK(ae_pl_match_entry(same, same_text, 3, "carousel") == 0); /* (exact beats a longer name) */
	CHECK(ae_pl_match_entry(same, same_text, 3, "carou") == -2);
	CHECK(ae_pl_match_entry(names, texts, 4, "g") == -2);
	ae_pl_people_reset(&p);
	CHECK(ae_pl_nominate(&p, 7, 1) && ae_pl_nominate(&p, 8, 3) && ae_pl_nominate(&p, 9, 1));
	CHECK(ae_pl_nominated(&p, out) == 2 && out[0] == 1 && out[1] == 3);
	CHECK(ae_pl_nominate(&p, 7, 0)); /* (one each: his second replaces his first) */
	CHECK(ae_pl_nominated(&p, out) == 3 && out[0] == 0);
	CHECK(!ae_pl_nominate(&p, 7, -1));
}

static void rtv(void)
{
	struct ae_pl_people p;
	long present[4] = { 1, 2, 3, 4 };

	ae_pl_people_reset(&p);
	CHECK(ae_pl_rtv_needed(0) == 0 && ae_pl_rtv_needed(1) == 1 && ae_pl_rtv_needed(2) == 2 && ae_pl_rtv_needed(3) == 2 &&
		ae_pl_rtv_needed(4) == 3 && ae_pl_rtv_needed(8) == 5);
	CHECK(!ae_pl_rtv_passed(&p, present, 4));
	CHECK(ae_pl_rtv_ask(&p, 1) == 1);
	CHECK(ae_pl_rtv_ask(&p, 1) == 1); /* (twice is once) */
	CHECK(!ae_pl_rtv_passed(&p, present, 4));
	ae_pl_rtv_ask(&p, 2);
	CHECK(!ae_pl_rtv_passed(&p, present, 4)); /* (2 of 4 is not a majority) */
	ae_pl_rtv_ask(&p, 3);
	CHECK(ae_pl_rtv_passed(&p, present, 4));
	/* (a person who left does not count: asks 1, 2, 3 against present 2, 3, 9) */
	{
		long now[3] = { 2, 3, 9 };

		CHECK(ae_pl_rtv_passed(&p, now, 3)); /* 2 of 3 */
	}
	{
		long now[5] = { 1, 5, 6, 7, 9 };

		CHECK(!ae_pl_rtv_passed(&p, now, 5)); /* 1 of 5 */
	}
	CHECK(!ae_pl_rtv_passed(&p, present, 0));
	CHECK(ae_pl_extend_allowed(0) && !ae_pl_extend_allowed(1));
	CHECK(ae_pl_extend_minutes(0) == 1 && ae_pl_extend_minutes(5) == 5 && ae_pl_extend_minutes(999) == 60);
}

static void audiences(void)
{
	int aud[5] = { AE_AUDIENCE_ANYONE, AE_AUDIENCE_AE, AE_AUDIENCE_AE_MCC, AE_AUDIENCE_AE, AE_AUDIENCE_ANYONE };
	unsigned char ok[5] = { 1, 1, 1, 1, 1 };
	unsigned char only_ae[5] = { 0, 1, 0, 1, 0 };
	char text[160];
	int mask;

	CHECK(ae_pl_audience(0, 0) == AE_AUDIENCE_ANYONE && ae_pl_audience(1, 0) == AE_AUDIENCE_AE &&
		ae_pl_audience(0, 1) == AE_AUDIENCE_AE_MCC && ae_pl_audience(1, 1) == AE_AUDIENCE_AE_MCC);
	CHECK(!strcmp(ae_pl_audience_word(0), "all") && !strcmp(ae_pl_audience_word(1), "ae") &&
		!strcmp(ae_pl_audience_word(2), "ae+mcc"));
	CHECK(ae_pl_can_play(AE_AUDIENCE_ANYONE, 0, 0));
	CHECK(!ae_pl_can_play(AE_AUDIENCE_AE, 0, 1));
	CHECK(ae_pl_can_play(AE_AUDIENCE_AE, 1, 0));
	CHECK(ae_pl_can_play(AE_AUDIENCE_AE_MCC, 1, 1) && ae_pl_can_play(AE_AUDIENCE_AE_MCC, 1, -1));
	CHECK(!ae_pl_can_play(AE_AUDIENCE_AE_MCC, 1, 0) && !ae_pl_can_play(AE_AUDIENCE_AE_MCC, 0, 1));
	/* skip to playable: everyone AE with MCC reaches entry 1; a non-AE person skips to entry 4 (after 1, 2, 3 wrap) */
	CHECK(ae_pl_next_playable(aud, ok, 5, 1, 0, 0) == 1);
	CHECK(ae_pl_next_playable(aud, ok, 5, 1, 1, 0) == 4);
	CHECK(ae_pl_next_playable(aud, ok, 5, 0, 1, 0) == 0);
	CHECK(ae_pl_next_playable(aud, ok, 5, 2, 0, 1) == 3); /* (someone without MCC: not entry 2) */
	CHECK(ae_pl_next_playable(aud, only_ae, 5, 0, 1, 0) == -1); /* (nothing for a non-AE person) */
	CHECK(ae_pl_next_playable(aud, only_ae, 5, 4, 0, 0) == 1); /* (wraps) */
	CHECK(ae_pl_next_playable(aud, ok, 5, 3, 0, 0) == 3);
	mask = ae_pl_range_mask(aud, ok, 5);
	CHECK(mask == 7 && ae_pl_range_mixed(mask));
	mask = ae_pl_range_mask(aud, only_ae, 5);
	CHECK(mask == (1 << AE_AUDIENCE_AE) && !ae_pl_range_mixed(mask));
	CHECK(ae_pl_range_mask(aud, ok, 0) == 0);
	ae_pl_refusal_text(AE_AUDIENCE_AE, text, sizeof(text));
	CHECK(strstr(text, "needs Arena Evolved 0.7.7 or later"));
	ae_pl_refusal_text(AE_AUDIENCE_AE_MCC, text, sizeof(text));
	CHECK(strstr(text, "MCC"));
}

static void listing(void)
{
	char block[AE_LISTING_EXTRAS_SIZE];

	CHECK(ae_listing_extras_canonical_range(block, sizeof(block), "ae", 0, "na", -1) > 0);
	CHECK(!strcmp(block, "{\"entries\":[],\"join_now\":\"ae\",\"join_range\":[\"ae\"],\"region\":\"na\"}"));
	CHECK(ae_listing_extras_canonical_range(block, sizeof(block), "ae+mcc", 7, "na", 3) > 0);
	CHECK(!strcmp(block, "{\"bots\":3,\"entries\":[],\"join_now\":\"ae+mcc\",\"join_range\":[\"all\",\"ae\",\"ae+mcc\"],"
		"\"region\":\"na\"}"));
	CHECK(ae_listing_extras_canonical_range(block, sizeof(block), "all", 1, "", -1) > 0);
	CHECK(strstr(block, "\"join_range\":[\"all\"]"));
	CHECK(!ae_listing_extras_canonical_range(block, sizeof(block), "BAD WORD", 7, "na", -1));
	CHECK(!ae_listing_extras_canonical_range(block, 20, "ae", 7, "na", -1));
	/* (the single word form is the old canonical form) */
	{
		char old[AE_LISTING_EXTRAS_SIZE];

		ae_listing_extras_canonical(old, sizeof(old), "ae", "na");
		ae_listing_extras_canonical_range(block, sizeof(block), "ae", 0, "na", -1);
		CHECK(!strcmp(old, block));
	}
	CHECK(!strcmp(ae_browser_badge_word("all"), "ANYONE") && !strcmp(ae_browser_badge_word("ae"), "AE") &&
		!strcmp(ae_browser_badge_word("ae+mcc"), "AE+MCC"));
}

/* the shuffle with weights: every entry once a pass; heavier entries come earlier on average */
static void shuffle(void)
{
	char const *map[6] = { "a", "b", "c", "d", "e", "f" };
	char const *gt[6] = { "g1", "g2", "g3", "g4", "g5", "g6" };
	int wt[6] = { 5, 1, 1, 1, 1, 1 };
	int order[6], total_heavy = 0, total_light = 0, trial, i;

	for (trial = 0; trial < 3000; trial++)
	{
		int seen[6] = { 0 };

		ae_rotation_shuffle_order_weighted(6, map, gt, wt, NULL, NULL, 77UL + (unsigned long)trial * 7919UL, order);
		for (i = 0; i < 6; i++)
		{
			CHECK(order[i] >= 0 && order[i] < 6);
			seen[order[i]]++;
			if (order[i] == 0)
				total_heavy += i;
			if (order[i] == 5)
				total_light += i;
		}
		for (i = 0; i < 6; i++)
			CHECK(seen[i] == 1);
	}
	/* (the weight 5 entry's mean place is well before the weight 1 entry's mean 2.5-ish... a plain shuffle has both at 2.5) */
	CHECK(total_heavy * 10 < total_light * 7);
	/* (no weights = the old function's order for the same seed) */
	{
		int a[6], b[6];

		ae_rotation_shuffle_order(6, map, gt, NULL, NULL, 99, a);
		ae_rotation_shuffle_order_weighted(6, map, gt, NULL, NULL, NULL, 99, b);
		CHECK(!memcmp(a, b, sizeof(a)));
	}
}

/* the map check: the table, what a machine told, who can play which entry, what it lacks */
static void map_check(void)
{
	struct ae_pl_map_table table, other;
	struct ae_pl_holding none, no_mcc, mcc;
	unsigned char bits[AE_PL_MAPS / 8];
	char text[160];
	int a, b, c, audience = AE_AUDIENCE_AE_MCC;

	memset(&table, 0, sizeof(table));
	a = ae_pl_map_table_slot(&table, "bloodgulch@mcc");
	b = ae_pl_map_table_slot(&table, "pat_beavercreek@ws2899342285");
	c = ae_pl_map_table_slot(&table, "BLOODGULCH@mcc");
	CHECK(a == 0 && b == 1 && c == 0 && table.count == 2);
	CHECK(ae_pl_map_table_find(&table, "pat_beavercreek@WS2899342285") == 1 && ae_pl_map_table_find(&table, "x@mcc") == -1);
	CHECK(ae_pl_map_table_slot(&table, "") == -1 && ae_pl_map_table_slot(&table, NULL) == -1);
	{
		char longest[AE_PL_MAP_NAME + 4];

		memset(longest, 'a', sizeof(longest) - 1);
		longest[sizeof(longest) - 1] = 0;
		CHECK(ae_pl_map_table_slot(&table, longest) == -1 && table.count == 2);
	}
	other = table;
	CHECK(ae_pl_map_table_same(&table, &other));
	ae_pl_map_table_slot(&other, "wizard@mcc");
	CHECK(!ae_pl_map_table_same(&table, &other));
	{
		char name[32];
		int i;

		for (i = 0; i < AE_PL_MAPS + 5; i++)
		{
			snprintf(name, sizeof(name), "m%d@mcc", i);
			ae_pl_map_table_slot(&other, name);
		}
		CHECK(other.count == AE_PL_MAPS && ae_pl_map_table_slot(&other, "overflow@mcc") == -1);
	}
	CHECK(ae_pl_ask_messages(0) == 0 && ae_pl_ask_messages(1) == 1 && ae_pl_ask_messages(6) == 1 &&
		ae_pl_ask_messages(7) == 2 && ae_pl_ask_messages(64) == 11);

	/* a machine that told nothing (an older build) is counted as holding every map */
	ae_pl_holding_reset(&none);
	CHECK(ae_pl_holding_state(&none, 0) == -1 && ae_pl_can_play(audience, 1, ae_pl_holding_state(&none, 0)));
	CHECK(ae_pl_missing(&none, 0) == AE_MISSING_NOTHING);
	/* no MCC at all: nothing of MCC's, but a map that needs no MCC (ANYONE, AE) is as before */
	memset(bits, 0, sizeof(bits));
	ae_pl_holding_set(&no_mcc, 0, bits, table.count);
	CHECK(ae_pl_holding_state(&no_mcc, 0) == 0 && ae_pl_holding_state(&no_mcc, 1) == 0);
	CHECK(!ae_pl_can_play(audience, 1, ae_pl_holding_state(&no_mcc, 0)));
	CHECK(ae_pl_can_play(AE_AUDIENCE_AE, 1, ae_pl_holding_state(&no_mcc, -1)) &&
		ae_pl_can_play(AE_AUDIENCE_ANYONE, 0, ae_pl_holding_state(&no_mcc, -1)));
	CHECK(ae_pl_missing(&no_mcc, 0) == AE_MISSING_MCC);
	ae_pl_missing_text(AE_MISSING_MCC, "bloodgulch@mcc", text, sizeof(text));
	CHECK(strstr(text, "needs MCC"));
	/* MCC, one map of two: the held one plays, the other lacks its item */
	memset(bits, 0, sizeof(bits));
	ae_pl_bits_set(bits, 0);
	ae_pl_bits_set(bits, 40);	/* (past the table: dropped) */
	ae_pl_holding_set(&mcc, 1, bits, table.count);
	CHECK(ae_pl_holding_state(&mcc, 0) == 1 && ae_pl_holding_state(&mcc, 1) == 0 && !(mcc.bits[5] & 1));
	CHECK(ae_pl_can_play(audience, 1, ae_pl_holding_state(&mcc, 0)) && !ae_pl_can_play(audience, 1, ae_pl_holding_state(&mcc, 1)));
	CHECK(ae_pl_missing(&mcc, 0) == AE_MISSING_NOTHING && ae_pl_missing(&mcc, 1) == AE_MISSING_MAP);
	ae_pl_missing_text(AE_MISSING_MAP, table.name[1], text, sizeof(text));
	CHECK(!strcmp(text, "Subscribe to pat_beavercreek (item 2899342285) in the Steam Workshop."));
	ae_pl_missing_text(AE_MISSING_MAP, "hangemhigh@mcc", text, sizeof(text));
	CHECK(strstr(text, "hangemhigh@mcc") && strstr(text, "MCC"));
	/* a join request told has MCC but no map bits: counted as holding the maps; told no MCC: nothing of MCC's */
	{
		struct ae_pl_holding joined;

		ae_pl_holding_set_join(&joined, 1);
		CHECK(ae_pl_holding_state(&joined, 0) == -1 && ae_pl_missing(&joined, 0) == AE_MISSING_NOTHING);
		ae_pl_holding_set_join(&joined, 0);
		CHECK(ae_pl_holding_state(&joined, 0) == 0 && ae_pl_missing(&joined, 0) == AE_MISSING_MCC);
	}
	/* (a non-AE machine never plays an MCC entry, whatever its holding says) */
	CHECK(!ae_pl_can_play(audience, 0, ae_pl_holding_state(&mcc, 0)));
}

int main(void)
{
	weights();
	sample();
	history();
	nominate();
	rtv();
	audiences();
	map_check();
	listing();
	shuffle();
	if (failures)
		printf("%d FAILED\n", failures);
	else
		printf("ok\n");
	return failures ? 1 : 0;
}
