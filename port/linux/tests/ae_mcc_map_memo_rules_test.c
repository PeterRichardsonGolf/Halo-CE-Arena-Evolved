/* ae_mcc_map_memo_rules_test.c: an MCC map's presence remembered a while (ae_mcc_map_memo_rules.h) */

#include <stdio.h>
#include <string.h>

#include "ae_mcc_map_memo_rules.h"

static int failures;

#define CHECK(condition) \
	do { if (!(condition)) { printf("%s:%d: %s\n", __FILE__, __LINE__, #condition); failures++; } } while (0)

static struct ae_mcc_map_memo memo;

/* the answer a find gives: -1 check the map, else the presence kept */
static int F(char const *identity, unsigned long generation, unsigned long now, unsigned int frame)
{
	int present = 7;

	if (!ae_mcc_map_memo_rules_find(&memo, identity, generation, now, frame, &present))
		return -1;
	return present;
}

int main(void)
{
	char name[AE_MCC_MAP_MEMO_IDENTITY + 8];
	int index, checked;

	/* a name never kept is checked; kept, it stands while fresh, present or not */
	CHECK(F("5@ws3477809506", 1, 1000, 1) == -1);
	ae_mcc_map_memo_rules_keep(&memo, "5@ws3477809506", 1, 1000, 1);
	ae_mcc_map_memo_rules_keep(&memo, "37@ws3477809506", 1, 1000, 0);
	CHECK(F("5@ws3477809506", 1, 1000, 1) == 1);
	CHECK(F("37@ws3477809506", 1, 2999, 9) == 0);
	/* (the name itself, not a near one) */
	CHECK(F("5@ws347780950", 1, 1000, 1) == -1);
	CHECK(F("5@WS3477809506", 1, 1000, 1) == -1);

	/* another catalog generation: checked */
	CHECK(F("5@ws3477809506", 2, 1001, 1) == -1);

	/* older than fresh: two checks a frame, the rest keep the answer; the next frame two more */
	memset(&memo, 0, sizeof(memo));
	for (index = 0; index < 10; index++)
	{
		snprintf(name, sizeof(name), "%d@ws3477809506", index);
		ae_mcc_map_memo_rules_keep(&memo, name, 1, 0, 1);
	}
	checked = 0;
	for (index = 0; index < 10; index++)
	{
		snprintf(name, sizeof(name), "%d@ws3477809506", index);
		if (F(name, 1, AE_MCC_MAP_MEMO_FRESH_MILLISECONDS, 50) == -1)
			checked++;
		else
			CHECK(F(name, 1, AE_MCC_MAP_MEMO_FRESH_MILLISECONDS, 50) == 1);
	}
	CHECK(checked == AE_MCC_MAP_MEMO_CHECKS_A_FRAME);
	checked = 0;
	for (index = 0; index < 10; index++)
	{
		snprintf(name, sizeof(name), "%d@ws3477809506", index);
		checked += F(name, 1, AE_MCC_MAP_MEMO_FRESH_MILLISECONDS + 1, 51) == -1;
	}
	CHECK(checked == AE_MCC_MAP_MEMO_CHECKS_A_FRAME);
	/* (a frame number that stands still: the budget comes back after 100 ms) */
	checked = 0;
	for (index = 0; index < 10; index++)
	{
		snprintf(name, sizeof(name), "%d@ws3477809506", index);
		checked += F(name, 1, AE_MCC_MAP_MEMO_FRESH_MILLISECONDS + 1, 51) == -1;
	}
	CHECK(checked == 0);
	checked = 0;
	for (index = 0; index < 10; index++)
	{
		snprintf(name, sizeof(name), "%d@ws3477809506", index);
		checked += F(name, 1, AE_MCC_MAP_MEMO_FRESH_MILLISECONDS + 1 + AE_MCC_MAP_MEMO_FRAME_MILLISECONDS, 51) == -1;
	}
	CHECK(checked == AE_MCC_MAP_MEMO_CHECKS_A_FRAME);

	/* stale: always checked, whatever the frame's budget */
	memset(&memo, 0, sizeof(memo));
	ae_mcc_map_memo_rules_keep(&memo, "a@ws1", 1, 0, 1);
	ae_mcc_map_memo_rules_keep(&memo, "b@ws1", 1, 0, 1);
	ae_mcc_map_memo_rules_keep(&memo, "c@ws1", 1, 0, 1);
	CHECK(F("a@ws1", 1, AE_MCC_MAP_MEMO_STALE_MILLISECONDS, 3) == -1);
	CHECK(F("b@ws1", 1, AE_MCC_MAP_MEMO_STALE_MILLISECONDS, 3) == -1);
	CHECK(F("c@ws1", 1, AE_MCC_MAP_MEMO_STALE_MILLISECONDS, 3) == -1);
	/* (a clock that went back: as stale) */
	ae_mcc_map_memo_rules_keep(&memo, "a@ws1", 1, 5000, 1);
	CHECK(F("a@ws1", 1, 4000, 3) == -1);
	/* (the clock wrapping: still fresh) */
	ae_mcc_map_memo_rules_keep(&memo, "a@ws1", 1, (unsigned long)-500, 1);
	CHECK(F("a@ws1", 1, 500, 3) == 1);

	/* a name too long, an empty one: never kept */
	memset(name, 'x', sizeof(name) - 1);
	name[sizeof(name) - 1] = 0;
	ae_mcc_map_memo_rules_keep(&memo, name, 1, 0, 1);
	CHECK(F(name, 1, 0, 1) == -1);
	ae_mcc_map_memo_rules_keep(&memo, "", 1, 0, 1);
	CHECK(F("", 1, 0, 1) == -1);
	CHECK(!ae_mcc_map_memo_rules_find(&memo, NULL, 1, 0, 1, &index));
	CHECK(!ae_mcc_map_memo_rules_find(NULL, "a@ws1", 1, 0, 1, &index));

	/* names whose search starts in one place: 8 are kept; a 9th takes the oldest one's place, the rest stay */
	memset(&memo, 0, sizeof(memo));
	{
		char names[AE_MCC_MAP_MEMO_PLACES + 1][16];
		int found = 0, at;

		for (at = 0; at < 100000 && found <= AE_MCC_MAP_MEMO_PLACES; at++)
		{
			snprintf(names[found], sizeof(names[found]), "m%d", at);
			if (!found || ae_mcc_map_memo_rules_slot(names[found]) == ae_mcc_map_memo_rules_slot(names[0]))
				found++;
		}
		CHECK(found == AE_MCC_MAP_MEMO_PLACES + 1);
		for (at = 0; at < AE_MCC_MAP_MEMO_PLACES; at++)
			ae_mcc_map_memo_rules_keep(&memo, names[at], 1, (unsigned long)(10 + at), at & 1);
		for (at = 0; at < AE_MCC_MAP_MEMO_PLACES; at++)
			CHECK(F(names[at], 1, 100, 1) == (at & 1));
		ae_mcc_map_memo_rules_keep(&memo, names[AE_MCC_MAP_MEMO_PLACES], 1, 50, 1);
		CHECK(F(names[0], 1, 100, 1) == -1);
		for (at = 1; at <= AE_MCC_MAP_MEMO_PLACES; at++)
			CHECK(F(names[at], 1, 100, 1) != -1);
		/* (a name kept again keeps its place) */
		ae_mcc_map_memo_rules_keep(&memo, names[1], 1, 60, 0);
		CHECK(F(names[1], 1, 100, 1) == 0 && F(names[2], 1, 100, 1) == 0);
	}
	/* a playlist's 66 names: every one kept */
	memset(&memo, 0, sizeof(memo));
	{
		int at, kept_all = 1;

		for (at = 0; at < 66; at++)
		{
			snprintf(name, sizeof(name), "%d@ws3477809506", at);
			ae_mcc_map_memo_rules_keep(&memo, name, 1, 0, 1);
		}
		for (at = 0; at < 66; at++)
		{
			snprintf(name, sizeof(name), "%d@ws3477809506", at);
			kept_all &= F(name, 1, 0, 1) == 1;
		}
		CHECK(kept_all);
	}

	if (failures)
		printf("%d failures\n", failures);
	else
		printf("ae_mcc_map_memo_rules_test: ok\n");
	return failures ? 1 : 0;
}
