#include <stdio.h>
#include <string.h>
#include "ae_map_dupes_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)
#define EQ(a, b) do { long x_ = (long)(a), y_ = (long)(b); if (x_ != y_) { printf("FAIL %s:%d %s = %ld, want %ld\n", __FILE__, __LINE__, #a, x_, y_); failures++; } } while (0)

static struct ae_map_dupe_entry entries[16];
static short head[16], order[16], versions[16];

static void add(int index, char const *list_name, char const *name, int stock, int groupable)
{
	entries[index].list_name = list_name;
	entries[index].name = name;
	entries[index].kind = ae_map_dupe_kind(list_name, stock);
	entries[index].groupable = groupable;
}

int main(void)
{
	EQ(ae_map_dupe_kind("pat_beavercreek@ws2899342285", 0), AE_MAP_DUPE_WORKSHOP);
	EQ(ae_map_dupe_kind("intensity_v2@smp", 0), AE_MAP_DUPE_CONVERTED);
	EQ(ae_map_dupe_kind("pat_beavercreek@PRO", 0), AE_MAP_DUPE_CONVERTED);
	EQ(ae_map_dupe_kind("beavercreek@cex", 0), AE_MAP_DUPE_CEX);
	EQ(ae_map_dupe_kind("beavercreek", 1), AE_MAP_DUPE_STOCK);
	EQ(ae_map_dupe_kind("intensity_v2", 0), AE_MAP_DUPE_COMMUNITY);
	EQ(ae_map_dupe_kind("x@wsx", 0), AE_MAP_DUPE_PACK);
	CHECK(!strcmp(ae_map_dupe_chip(AE_MAP_DUPE_CEX), "CE+ X"));
	CHECK(!strcmp(ae_map_dupe_chip(99), ""));
	CHECK(ae_map_dupe_help(AE_MAP_DUPE_CONVERTED)[0]);
	CHECK(!ae_map_dupe_help(99)[0]);

	/* the Pro Edition map: the Workshop item and the converted copy, by the alias table's stem; the Workshop item is shown */
	add(0, "pat_beavercreek@pro", "Better Creek Pro", 0, 1);
	add(1, "pat_beavercreek@ws2899342285", "Better Creek Pro", 0, 1);
	/* the stock map, and the CE+ X copy of another map under the same file name: neither is grouped */
	add(2, "levels\\test\\beavercreek\\beavercreek", "Battle Creek", 1, 1);
	add(3, "beavercreek@cex", "Delta Ruined", 0, 1);
	/* Delta Ruined: the Workshop item (by its old name's stem) and the converted copy; CE+ X's copy stays alone */
	add(4, "delta_ruined flycam mcc@ws2899747345", "Delta Ruined", 0, 1);
	add(5, "delta_ruined@smp", "Delta Ruined Remake", 0, 1);
	/* intensity: the plain community file, the converted copy and the Workshop item by stem, different shown names */
	add(6, "intensity_v2@smp", "Intensity", 0, 1);
	add(7, "intensity_v2", "Intensity V2", 0, 1);
	add(8, "intensity_v2 debug flycam pm@ws2900242317", "Intensity (Workshop)", 0, 1);
	/* a map Steam is writing is never grouped */
	add(9, "intensity_v2 debug flycam pm@ws2900242318", "Intensity (Workshop)", 0, 0);
	ae_map_dupes_group(entries, 10, head, order, versions);

	EQ(head[0], 1);
	EQ(head[1], 1);
	EQ(versions[0], 2);
	EQ(order[1], 0);
	EQ(order[0], 1);
	EQ(head[2], 2);
	EQ(versions[2], 1);
	EQ(head[3], 3);
	EQ(versions[3], 1);
	EQ(head[4], 4);
	EQ(head[5], 4);
	EQ(versions[4], 2);
	EQ(order[5], 1);
	/* the Workshop item is best; the plain file before the converted one */
	EQ(head[6], 8);
	EQ(head[7], 8);
	EQ(order[7], 1);
	EQ(order[6], 2);
	EQ(versions[8], 3);
	EQ(head[9], 9);
	EQ(versions[9], 1);

	/* the Digsite collection's remakes share the stock and community names and are different maps: never grouped, and a
	stock map is never taken out of its folder by one (pairs seen in a real maps folder) */
	{
		static char const *const digsite[][2] = {
			{ "42@ws3477809506", "levels\\test\\prisoner\\prisoner" }, { "54@ws3477809506", "levels\\test\\chillout\\chillout" },
			{ "45@ws3477809506", "levels\\test\\ratrace\\ratrace" }, { "58@ws3477809506", "levels\\test\\boardingaction\\boardingaction" },
			{ "62@ws3477809506", "levels\\test\\sidewinder\\sidewinder" }, { "66@ws3477809506", "temple" },
			{ "72@ws3477809506", "weld" }, { "73@ws3477809506", "whiskey" }, { "74@ws3477809506", "worthy" },
			{ "35@ws3477809506", "longshot" }, { "41@ws3477809506", "outbound" }, { "48@ws3477809506", "jaywalk" },
			{ "50@ws3477809506", "redshift" }, { "52@ws3477809506", "doubletake" }, { "68@ws3477809506", "tinker" },
			{ "51@ws3477809506", "overflow" }, { "51@ws3477809506", "overflow@pe" }, { "32@ws3477809506", "intensity_v2" },
			{ "32@ws3477809506", "intensity_v2@smp" }, { "40@ws3477809506", "nitra_v2@smp" }, { "49@ws3477809506", "gothra@smp" },
			{ "56@ws3477809506", "ruins_of_alph@smp" }, { "61@ws3477809506", "blamitemines@smp" }, { "71@ws3477809506", "warren@smp" }
		};
		int each;

		for (each = 0; each < (int)(sizeof(digsite) / sizeof(digsite[0])); each++)
		{
			add(0, digsite[each][0], "Same Name", 0, 1);
			add(1, digsite[each][1], "Same Name", !strchr(digsite[each][1], '@') && strchr(digsite[each][1], '\\') != NULL, 1);
			ae_map_dupes_group(entries, 2, head, order, versions);
			EQ(head[0], 0);
			EQ(head[1], 1);
			EQ(versions[0], 1);
			EQ(versions[1], 1);
		}
	}

	/* no Workshop item: the plain file wins; only a converted copy: it is shown */
	add(0, "intensity_v2@smp", "Intensity", 0, 1);
	add(1, "intensity_v2", "Intensity V2", 0, 1);
	add(2, "warren@smp", "Warren", 0, 1);
	/* two plain files of one stem (a community file and a pack's) are not one map */
	add(3, "overflow", "Overflow", 0, 1);
	add(4, "overflow@pe", "Overflow", 0, 1);
	ae_map_dupes_group(entries, 5, head, order, versions);
	EQ(head[0], 1);
	EQ(head[2], 2);
	EQ(versions[2], 1);
	EQ(head[3], 3);
	EQ(head[4], 4);

	/* every family's chip */
	EQ(ae_map_dupe_kind("timberland@mcc", 0), AE_MAP_DUPE_MCC);
	EQ(ae_map_dupe_kind("ascetic@ce", 0), AE_MAP_DUPE_CUSTOM_EDITION);
	EQ(ae_map_dupe_kind("x@pc", 0), AE_MAP_DUPE_CUSTOM_EDITION);
	EQ(ae_map_dupe_kind("x@md", 0), AE_MAP_DUPE_HALOMD);
	EQ(ae_map_dupe_kind("overflow@pe", 0), AE_MAP_DUPE_PACK);
	EQ(ae_map_dupe_kind("temple", 0), AE_MAP_DUPE_COMMUNITY);
	EQ(ae_map_dupe_kind("levels\\test\\prisoner\\prisoner", 1), AE_MAP_DUPE_STOCK);
	CHECK(!strcmp(ae_map_dupe_chip(AE_MAP_DUPE_MCC), "MCC") && !strcmp(ae_map_dupe_chip(AE_MAP_DUPE_PACK), "MAP PACK"));
	{
		int kind;

		for (kind = 0; kind < AE_MAP_DUPE_KINDS; kind++)
			CHECK(ae_map_dupe_chip(kind)[0] && ae_map_dupe_help(kind)[0]);
	}

	/* maps.txt's rows hold for a Gearbox map's copies in every family: the @mcc and @pc copy looks up its @ce row, then the plain one */
	{
		char key[64];

		CHECK(ae_map_facts_key("timberland@mcc", 0, key, sizeof(key)) && !strcmp(key, "timberland@mcc"));
		CHECK(ae_map_facts_key("timberland@mcc", 1, key, sizeof(key)) && !strcmp(key, "timberland@ce"));
		CHECK(ae_map_facts_key("timberland@mcc", 2, key, sizeof(key)) && !strcmp(key, "timberland"));
		CHECK(!ae_map_facts_key("timberland@mcc", 3, key, sizeof(key)));
		CHECK(ae_map_facts_key("icefields@pc", 1, key, sizeof(key)) && !strcmp(key, "icefields@ce"));
		CHECK(!ae_map_facts_key("icefields@ce", 1, key, sizeof(key)) && !ae_map_facts_key("warren@smp", 1, key, sizeof(key)));
		CHECK(!ae_map_facts_key("27@ws3477809506", 1, key, sizeof(key)) && !ae_map_facts_key("bloodgulch", 1, key, sizeof(key)));
		CHECK(!ae_map_facts_key("timberland@mcc", 1, key, 8));
		/* BTB keeps one copy of a base map: CE file, plain, MCC, Halo PC; a pack's or Workshop map is never compared */
		EQ(ae_map_btb_rank("icefields@ce"), 0);
		EQ(ae_map_btb_rank("bloodgulch"), 1);
		EQ(ae_map_btb_rank("icefields@mcc"), 2);
		EQ(ae_map_btb_rank("icefields@pc"), 3);
		EQ(ae_map_btb_rank("5@ws3477809506"), 4);
		EQ(ae_map_btb_rank("icefields@pe"), 4);
		CHECK(ae_map_same_base("Icefields@ce", "icefields@mcc") && ae_map_same_base("icefields", "icefields@pc"));
		CHECK(!ae_map_same_base("icefields@ce", "icefield@mcc") && !ae_map_same_base("icefields2", "icefields@ce"));
	}

	if (failures)
		return 1;
	printf("ok\n");
	return 0;
}
