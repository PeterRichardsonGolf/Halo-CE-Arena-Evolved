/* ae_social_view_rules: the words the social career's screens show. */
#include <stdio.h>
#include <string.h>

#include "../game/ae_social_view_rules.h"
#include "../game/ae_social_rules.h"

static int failures;

#define CHECK_TEXT(call, want) \
	do { char t[200]; call; if (strcmp(t, want)) { printf("FAIL line %d: \"%s\" wanted \"%s\"\n", __LINE__, t, want); failures++; } } while (0)

int main(void)
{
	struct ae_social_playlist list;
	struct ae_social_counts counts;

	CHECK_TEXT(ae_social_view_rank(14, t, 200), "RANK 14");
	CHECK_TEXT(ae_social_view_exp(37, t, 200), "37 EXP");
	/* table A: rank 14 at 35, rank 15 at 40: 37 EXP is 3 wins from rank 15 */
	CHECK_TEXT(ae_social_view_wins_to(37, t, 200), "3 WINS TO RANK 15");
	CHECK_TEXT(ae_social_view_wins_to(39, t, 200), "1 WIN TO RANK 15");
	CHECK_TEXT(ae_social_view_wins_to(5000, t, 200), "TOP RANK");
	CHECK_TEXT(ae_social_view_overall_to(312, t, 200), "38 EXP TO OVERALL RANK 23");
	CHECK_TEXT(ae_social_view_overall_to(99999, t, 200), "TOP OVERALL RANK");
	memset(&list, 0, sizeof(list));
	list.teams = 2;
	list.size = 4;
	CHECK_TEXT(ae_social_view_shape(&list, 19, t, 200), "4 V 4 \xC2\xB7 19 MAPS");
	list.teams = 0;
	list.size = 8;
	CHECK_TEXT(ae_social_view_shape(&list, 1, t, 200), "8 PLAYERS \xC2\xB7 1 MAP");
	memset(&counts, 0, sizeof(counts));
	counts.pack_total = 19;
	counts.pack_present = 7;
	counts.stock_total = 7;
	counts.stock_present = 7;
	counts.stock_in_use = 1;
	CHECK_TEXT(ae_social_view_maps(&counts, t, 200), "7 of 19 SOCIAL MAP PACK + 7 STOCK");
	if (!ae_social_view_maps_missing(&counts))
		failures++;
	counts.pack_present = 19;
	counts.stock_in_use = 0;
	CHECK_TEXT(ae_social_view_maps(&counts, t, 200), "19 of 19 SOCIAL MAP PACK");
	if (ae_social_view_maps_missing(&counts))
		failures++;
	{
		char t[200], second[200];
		struct ae_social_playlist classic;

		/* (the "get the rest" lines: a pack playlist names the Workshop and joining; CLASSIC COMP names AE #5; none when nothing is missing) */
		counts.pack_present = 7;
		memset(&classic, 0, sizeof(classic));
		if (!ae_social_view_get_rest(&list, &counts, t, 200, second, 200))
			failures++;
		if (strcmp(t, "Get the rest: subscribe to the Social Map Pack in MCC's")) { printf("FAIL line %d: \"%s\"\n", __LINE__, t); failures++; }
		if (strcmp(second, "Workshop and convert it. Joining an AE server downloads them.")) { printf("FAIL line %d: \"%s\"\n", __LINE__, second); failures++; }
		counts.pack_present = 19;
		if (ae_social_view_get_rest(&list, &counts, t, 200, second, 200) || t[0] || second[0])
			failures++;
		classic.weapons_own = 1;
		memset(&counts, 0, sizeof(counts));
		counts.stock_total = 27;
		counts.stock_present = 5;
		if (!ae_social_view_get_rest(&classic, &counts, t, 200, second, 200))
			failures++;
		if (strcmp(t, "Get the rest of the classic 2v2 maps: join AE #5 once,")) { printf("FAIL line %d: \"%s\"\n", __LINE__, t); failures++; }
		if (strcmp(second, "its maps download to this PC.")) { printf("FAIL line %d: \"%s\"\n", __LINE__, second); failures++; }
		counts.stock_present = 27;
		if (ae_social_view_get_rest(&classic, &counts, t, 200, second, 200))
			failures++;
	}
	memset(&counts, 0, sizeof(counts));
	counts.stock_present = 8;
	CHECK_TEXT(ae_social_view_maps(&counts, t, 200), "8 STOCK MAPS");
	CHECK_TEXT(ae_social_view_bot_line(0, t, 200), "Matched to how you play");
	CHECK_TEXT(ae_social_view_bot_line(4, t, 200), "Every bot HEROIC");
	CHECK_TEXT(ae_social_view_postgame_sub("TEAM SLAYER", 0, 1, 8, 40, t, 200), "TEAM SLAYER \xC2\xB7 RANK 15 \xC2\xB7 10 TO RANK 16");
	CHECK_TEXT(ae_social_view_postgame_sub("FREE FOR ALL", 1, 0, 8, 52, t, 200),
		"FREE FOR ALL \xC2\xB7 4TH OR BETTER EARNS EXP \xC2\xB7 RANK 16");
	CHECK_TEXT(ae_social_view_ordinal(0, t, 200), "1ST");
	CHECK_TEXT(ae_social_view_ordinal(10, t, 200), "11TH");
	CHECK_TEXT(ae_social_view_ordinal(21, t, 200), "22ND");
	CHECK_TEXT(ae_social_view_quit_body("TEAM SLAYER", 37, t, 200),
		"This is a tournament TEAM SLAYER game. Leaving before the end costs 1 EXP (37 to 36) and counts as left early. "
		"Leaving at the post-game costs nothing.");
	CHECK_TEXT(ae_social_view_quit_help("TEAM SLAYER", t, 200), "Leave the game: -1 EXP in TEAM SLAYER.");
	CHECK_TEXT(ae_social_view_dev_line(2200, t, 200), "DEV \xC2\xB7 MATCH 23.0 \xC2\xB7 TIER 9 HEROIC / NORMAL \xC2\xB7 RANGE 17-30");
	if (failures)
		return 1;
	printf("ae_social_view_rules: ok\n");
	return 0;
}
