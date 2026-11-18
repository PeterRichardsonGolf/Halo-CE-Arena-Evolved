#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ae_social_playlists.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)
#define EQ(a, b) do { long x_ = (long)(a), y_ = (long)(b); if (x_ != y_) { printf("FAIL %s:%d %s = %ld, want %ld\n", __FILE__, __LINE__, #a, x_, y_); failures++; } } while (0)

/* a presence set: every pack map up to pack_present of the list, every stock map present unless hidden */
struct world
{
	int pack_present;
	int stock_present;
	int demake_present;
	int workshop_present;
	char const *pack[32];
	int pack_count;
};

static int present(void *context, char const *map)
{
	struct world *world = context;
	char const *at = strchr(map, '@');
	int index;

	/* (a Steam Workshop map, <name>@ws<item>: here when the world has the Workshop pack) */
	if (at && !strncmp(at, "@ws", 3))
		return world->workshop_present;

	if (!at)
		return !strncmp(map, "h2d_", 4) ? world->demake_present : world->stock_present;
	/* (the Custom Edition Warlock, h2_warlock@ce, in place of h2d_warlock: a demake too) */
	if (!strcmp(at, "@ce"))
		return !strncmp(map, "h2_", 3) ? world->demake_present : world->stock_present;
	for (index = 0; index < world->pack_count && index < world->pack_present; index++)
		if (!strncmp(map, world->pack[index], (size_t)(at - map)) && !world->pack[index][at - map])
			return 1;
	return 0;
}

static char text[131072];

/* the first classic_here entries' maps are here, and every fallback's */
static int classic_here;

static int present_first(void *context, char const *map)
{
	struct ae_social_playlist const *list = context;
	int index;

	for (index = 0; index < list->count; index++)
		if (!strcmp(list->entry[index].map, map))
			return list->entry[index].fallback || index < classic_here;
	return 0;
}

static void test_synthetic(void)
{
	static char const sample[] =
		"# header words: with a colon\r\n"
		"# social: duel\r\n# label: DUEL\r\n# about: One on one.\r\n# players: 2\r\n# kind: social\r\n"
		"warren@smp:file=AE FFA SLAY  # weight 9\r\n"
		"prisoner:file=AE FFA SLAY  # weight 1  # fallback\r\n"
		":file=AE FFA SLAY\r\n"
		"wizard:file=  # weight 2\r\n"
		"\r\n"
		"# social: broken\r\n# label: NOTHING\r\n"
		"# social: pair\r\n# teams: 2 x 2\r\n# party: 5\r\n"
		"hektik@smp:file=AE 2V2 SLY\r\n";
	struct ae_social_playlist lists[4];
	int count = ae_social_playlists_parse(sample, lists, 4);

	EQ(count, 2);
	CHECK(!strcmp(lists[0].id, "duel") && !strcmp(lists[0].label, "DUEL") && !strcmp(lists[0].about, "One on one."));
	EQ(lists[0].teams, 0); EQ(lists[0].size, 2); EQ(lists[0].party, 1); EQ(lists[0].count, 2);
	CHECK(!strcmp(lists[0].entry[0].map, "warren@smp") && !strcmp(lists[0].entry[0].gametype, "AE FFA SLAY"));
	EQ(lists[0].entry[0].weight, 5); EQ(lists[0].entry[0].pack, 1); EQ(lists[0].entry[0].fallback, 0);
	EQ(lists[0].entry[1].weight, 1); EQ(lists[0].entry[1].fallback, 1); EQ(lists[0].entry[1].pack, 0);
	/* the second: teams 2 x 2, party 5 over the size: the team's size; weight none: 2; the label its id */
	CHECK(!strcmp(lists[1].id, "pair") && !strcmp(lists[1].label, "pair"));
	EQ(lists[1].teams, 2); EQ(lists[1].size, 2); EQ(lists[1].party, 2); EQ(lists[1].entry[0].weight, 2);
	EQ(ae_social_playlist_seats(&lists[1]), 4);
	EQ(ae_social_playlist_find(lists, count, "PAIR"), 1);
	EQ(ae_social_playlist_find(lists, count, "none"), -1);
	EQ(lists[0].hide_empty, 0);
	EQ(ae_social_playlists_parse("", lists, 4), 0);
	EQ(ae_social_playlists_parse(sample, lists, 1), 1);
}

static int has_map(struct ae_social_playlist const *list, char const *map)
{
	int entry;

	for (entry = 0; entry < list->count; entry++)
		if (!strcmp(list->entry[entry].map, map))
			return 1;
	return 0;
}

/* a DIGSITE playlist: label, a team of size players, social, the maps' own weapons, the pistol start, hidden without the pack,
   entries Workshop maps of item 3477809506 only, no map twice with one game type, the distinct maps */
static void digsite_check(struct ae_social_playlist const *list, char const *label, int size, int entries, int maps, int casual_starts)
{
	int entry, other, distinct = 0;

	CHECK(!strcmp(list->label, label));
	EQ(list->kind, AE_SOCIAL_KIND_SOCIAL); EQ(list->teams, 2); EQ(list->size, size); EQ(list->party, size);
	EQ(list->weapons_own, 1); EQ(list->hide_empty, 1); EQ(list->count, entries);
	if (casual_starts)
	{
		EQ(list->starts, 2); EQ(list->start_kind[0], AE_SOCIAL_START_BR); EQ(list->start_kind[1], AE_SOCIAL_START_EVO);
	}
	else
	{
		/* (rc2: DIGSITE BIG TEAM starts with the PROTOMAG, the Digsite pack's starter pistol) */
		EQ(list->starts, 1); EQ(list->start_kind[0], AE_SOCIAL_START_PROTOMAG);
	}
	for (entry = 0; entry < list->count; entry++)
	{
		int first = 1;

		CHECK(strstr(list->entry[entry].map, "@ws3477809506") && !list->entry[entry].fallback);
		for (other = 0; other < entry; other++)
		{
			CHECK(strcmp(list->entry[other].map, list->entry[entry].map) || strcmp(list->entry[other].gametype, list->entry[entry].gametype));
			first &= !!strcmp(list->entry[other].map, list->entry[entry].map);
		}
		distinct += first;
	}
	EQ(distinct, maps);
}

static void check_shipped(void)
{
	static struct ae_social_playlist lists[AE_SOCIAL_PLAYLISTS];
	FILE *file = fopen(AE_SOCIAL_TXT, "rb");
	size_t length;
	int count, index, entry;
	struct world world;
	struct ae_social_counts counts;
	int c11[AE_SOCIAL_ENTRIES];
	static char const *const expect[13] = {"team-slayer", "free-for-all", "doubles", "team-objective", "big-team", "swat-snipers",
		"comp-team-slayer", "comp-free-for-all", "comp-doubles", "comp-team-objective", "classic-comp", "digsite-arena", "digsite-big-team"};
	static char const *const pack[19] = {"ascetic", "blamitemines", "blood_covenant", "co2", "cold_shoulder", "delta_ruined",
		"dredwerkz", "fissurefall", "fractured_peak", "frigid_keep", "gothra", "gridiron", "gutter_nm", "hektik",
		"intensity_v2", "nitra_v2", "ruins_of_alph", "sympathy", "warren"};

	CHECK(file != NULL);
	if (!file)
		return;
	length = fread(text, 1, sizeof(text) - 1, file);
	text[length] = 0;
	fclose(file);
	count = ae_social_playlists_parse(text, lists, AE_SOCIAL_PLAYLISTS);
	EQ(count, 13);
	for (index = 0; index < 13; index++)
		CHECK(!strcmp(lists[index].id, expect[index]));
	/* the starts: the casual ones mix br + evo (swat-snipers none), the comp ones evo + magnum (swat-snipers: none named, the
	   default is the comp one: the game's own types have no such start, the game layer finds none supported) */
	for (index = 0; index < 4; index++)
	{
		EQ(lists[index].kind, AE_SOCIAL_KIND_SOCIAL); EQ(lists[index].starts, 2);
		EQ(lists[index].start_kind[0], AE_SOCIAL_START_BR); EQ(lists[index].start_kind[1], AE_SOCIAL_START_EVO);
		EQ(lists[index].start_weight[0], 1); EQ(lists[index].start_weight[1], 1);
		EQ(lists[index + 6].kind, AE_SOCIAL_KIND_COMP); EQ(lists[index + 6].starts, 2);
		EQ(lists[index + 6].start_kind[0], AE_SOCIAL_START_EVO); EQ(lists[index + 6].start_kind[1], AE_SOCIAL_START_MAGNUM);
		/* a mirror: the same shape and party */
		EQ(lists[index + 6].teams, lists[index].teams); EQ(lists[index + 6].size, lists[index].size);
		EQ(lists[index + 6].party, lists[index].party);
	}
	/* ACTION SACK keeps the old SWAT & SNIPERS id, so the rank carries over */
	CHECK(!strcmp(lists[5].label, "ACTION SACK"));
	EQ(lists[5].starts, 0); EQ(lists[5].kind, AE_SOCIAL_KIND_SOCIAL); EQ(lists[9].kind, AE_SOCIAL_KIND_COMP);
	/* CLASSIC COMP: 2 x 2, party 2, comp, no starts (the default mix kept away), the maps' own weapons; 25 @ce maps (weight 2), 5
	   stock fallbacks, every game TS 50 */
	CHECK(!strcmp(lists[10].label, "CLASSIC COMP"));
	EQ(lists[10].kind, AE_SOCIAL_KIND_COMP); EQ(lists[10].teams, 2); EQ(lists[10].size, 2); EQ(lists[10].party, 2);
	EQ(lists[10].starts, 0); EQ(lists[10].weapons_own, 1); EQ(lists[10].count, 29);
	for (entry = 0; entry < 29; entry++)
		CHECK(!strcmp(lists[10].entry[entry].gametype, "TS 50") && lists[10].entry[entry].fallback == (entry >= 24));
	for (index = 0; index < 10; index++)
		EQ(lists[index].weapons_own, 0);
	/* the shapes */
	EQ(lists[0].teams, 2); EQ(lists[0].size, 4); EQ(lists[1].teams, 0); EQ(lists[1].size, 8); EQ(lists[1].party, 1);
	EQ(lists[2].teams, 2); EQ(lists[2].size, 2); EQ(lists[2].party, 2); EQ(lists[4].size, 8);
	for (index = 0; index < count; index++)
	{
		CHECK(lists[index].label[0] && lists[index].about[0]);
		for (entry = 0; entry < lists[index].count; entry++)
		{
			CHECK(lists[index].entry[entry].weight >= 1 && lists[index].entry[entry].weight <= 5);
			/* the map names are plain (no spaces, no colon) and a pack map is only <file>@smp; a Custom Edition map
			(<file>@ce) is no pack map */
			CHECK(!strpbrk(lists[index].entry[entry].map, " :"));
			CHECK(!strchr(lists[index].entry[entry].map, '@') || lists[index].entry[entry].pack ||
				!strcmp(strchr(lists[index].entry[entry].map, '@'), "@ce") ||
				!strcmp(strchr(lists[index].entry[entry].map, '@'), "@ws3477809506"));
		}
	}
	memset(&world, 0, sizeof(world));
	for (index = 0; index < 19; index++)
		world.pack[world.pack_count++] = pack[index];
	/* every pack map here: TEAM SLAYER has 18 of 18 (Dredwerkz out of the hoppers in 0.9.4), its fallbacks out, 7 stock and 10
	   demakes named (no demake here; Elongation out in 0.9.4) */
	world.pack_present = 19; world.stock_present = 1;
	ae_social_playlist_counts(&lists[0], present, &world, &counts);
	EQ(counts.pack_total, 18); EQ(counts.pack_present, 18); EQ(counts.stock_total, 17); EQ(counts.stock_present, 7);
	EQ(counts.stock_in_use, 0);
	/* none here: 0 of 18, the 7 stock in use */
	world.pack_present = 0;
	ae_social_playlist_counts(&lists[0], present, &world, &counts);
	EQ(counts.pack_present, 0); EQ(counts.stock_in_use, 1);
	/* BIG TEAM: stock and 7 demakes, never "in use" (no pack maps named) */
	ae_social_playlist_counts(&lists[4], present, &world, &counts);
	EQ(counts.pack_total, 0); EQ(counts.stock_total, 15); EQ(counts.stock_in_use, 0);
	/* FFA: 18 pack maps, hektik / co2 / sympathy / nitra_v2 twice: distinct */
	ae_social_playlist_counts(&lists[1], present, &world, &counts);
	EQ(counts.pack_total, 18);
	/* doubles: 6 pack maps */
	ae_social_playlist_counts(&lists[2], present, &world, &counts);
	EQ(counts.pack_total, 6);
	/* CLASSIC COMP's plain map names: its fallbacks play only while fewer than 3 of its maps are here (heard_present: the first n
	   entries' maps are here, the fallbacks always) */
	{
		int c2[AE_SOCIAL_ENTRIES];

		classic_here = 2;
		EQ(ae_social_playlist_candidates(&lists[10], present_first, &lists[10], c2), 2 + 5);
		classic_here = 3;
		EQ(ae_social_playlist_candidates(&lists[10], present_first, &lists[10], c2), 3);
		classic_here = 24;
		EQ(ae_social_playlist_candidates(&lists[10], present_first, &lists[10], c2), 24);
	}
	/* DIGSITE ARENA (4v4) and DIGSITE BIG TEAM (8v8): the Steam Workshop pack's 75 maps split by size and vehicles, the maps' own
	   weapons, ARENA the casual br + evo starts (BIG TEAM the pistol start), listed only with the pack. A mirrored map goes with its original (Hang 'Em High 31 and 55). */
	digsite_check(&lists[11], "DIGSITE ARENA", 4, 189, 65, 1);
	digsite_check(&lists[12], "DIGSITE BIG TEAM", 8, 35, 10, 0);
	for (entry = 0; entry < lists[11].count; entry++)
		CHECK(!strcmp(lists[11].entry[entry].gametype, "AE TEAM SLY") || !strcmp(lists[11].entry[entry].gametype, "AE CTF") ||
			!strcmp(lists[11].entry[entry].gametype, "AE KING"));
	for (entry = 0; entry < lists[12].count; entry++)
		CHECK(!strncmp(lists[12].entry[entry].gametype, "AE BTB ", 7));
	CHECK(has_map(&lists[11], "31@ws3477809506") && has_map(&lists[11], "55@ws3477809506"));
	CHECK(!has_map(&lists[11], "20@ws3477809506") && !has_map(&lists[12], "31@ws3477809506"));
	CHECK(has_map(&lists[12], "20@ws3477809506") && has_map(&lists[12], "62@ws3477809506"));
	for (index = 0; index < 11; index++)
		EQ(lists[index].hide_empty, 0);
	{
		struct world dig;

		memset(&dig, 0, sizeof(dig));
		dig.stock_present = 1; dig.demake_present = 1;
		EQ(ae_social_playlist_shown(&lists[11], present, &dig), 0);
		EQ(ae_social_playlist_shown(&lists[12], present, &dig), 0);
		EQ(ae_social_playlist_shown(&lists[0], present, &dig), 1);
		dig.workshop_present = 1;
		EQ(ae_social_playlist_shown(&lists[11], present, &dig), 1);
		EQ(ae_social_playlist_shown(&lists[12], present, &dig), 1);
		/* the Workshop maps are no pack maps: no pack count, no "get the rest" of the Social Map Pack */
		EQ(ae_social_playlist_candidates(&lists[11], present, &dig, c11), 189);
		ae_social_playlist_counts(&lists[11], present, &dig, &counts);
		EQ(counts.pack_total, 0); EQ(counts.stock_total, 65); EQ(counts.stock_present, 65); EQ(counts.workshop, 1);
		ae_social_playlist_counts(&lists[3], present, &dig, &counts);
		EQ(counts.workshop, 0);
	}
	/* every playlist can play with nothing but stock maps, and with only pack maps */
	for (index = 0; index < count; index++)
	{
		int c[AE_SOCIAL_ENTRIES];

		/* (DIGSITE needs its Workshop pack: shown only with it) */
		if (lists[index].hide_empty)
			continue;
		world.pack_present = 0; world.stock_present = 1;
		CHECK(ae_social_playlist_candidates(&lists[index], present, &world, c) > 0);
		world.pack_present = 19; world.stock_present = 0;
		if (lists[index].entry[0].pack)
			CHECK(ae_social_playlist_candidates(&lists[index], present, &world, c) > 0);
	}
}

static void test_pick(void)
{
	static struct ae_social_playlist lists[AE_SOCIAL_PLAYLISTS];
	FILE *file = fopen(AE_SOCIAL_TXT, "rb");
	size_t length;
	struct world world;
	int c[AE_SOCIAL_ENTRIES], count, index, got, last, random, seen_stock, run;
	static char const *const pack[19] = {"ascetic", "blamitemines", "blood_covenant", "co2", "cold_shoulder", "delta_ruined",
		"dredwerkz", "fissurefall", "fractured_peak", "frigid_keep", "gothra", "gridiron", "gutter_nm", "hektik",
		"intensity_v2", "nitra_v2", "ruins_of_alph", "sympathy", "warren"};

	if (!file)
		return;
	length = fread(text, 1, sizeof(text) - 1, file);
	text[length] = 0;
	fclose(file);
	EQ(ae_social_playlists_parse(text, lists, AE_SOCIAL_PLAYLISTS), 13);
	memset(&world, 0, sizeof(world));
	for (index = 0; index < 19; index++)
		world.pack[world.pack_count++] = pack[index];
	world.stock_present = 1;
	/* 18 pack maps named (Dredwerkz out in 0.9.4): only pack entries are offered */
	world.pack_present = 19;
	count = ae_social_playlist_candidates(&lists[0], present, &world, c);
	EQ(count, 18);
	for (index = 0; index < count; index++)
		CHECK(!lists[0].entry[c[index]].fallback);
	/* 3 pack maps: still no fallbacks; 2: the fallbacks join (2 + 7) */
	world.pack_present = 3;
	EQ(ae_social_playlist_candidates(&lists[0], present, &world, c), 3);
	world.pack_present = 2;
	EQ(ae_social_playlist_candidates(&lists[0], present, &world, c), 9);
	world.pack_present = 0;
	EQ(ae_social_playlist_candidates(&lists[0], present, &world, c), 7);
	/* no pack map but the 10 demakes: they play, the fallbacks out */
	world.demake_present = 1;
	EQ(ae_social_playlist_candidates(&lists[0], present, &world, c), 10);
	world.demake_present = 0;
	/* nothing here at all: no pick */
	world.stock_present = 0;
	EQ(ae_social_playlist_candidates(&lists[0], present, &world, c), 0);
	EQ(ae_social_playlist_pick(&lists[0], present, &world, -1, 5), -1);
	/* the pick never repeats the last map while 2+ are on offer; with 2 pack maps stock joins, seen over many randoms */
	world.stock_present = 1; world.pack_present = 2; last = -1; seen_stock = 0;
	for (random = 0; random < 2000; random++)
	{
		got = ae_social_playlist_pick(&lists[0], present, &world, last, (unsigned long)random * 2654435761ul);
		CHECK(got >= 0);
		if (last >= 0)
			CHECK(strcmp(lists[0].entry[got].map, lists[0].entry[last].map) != 0);
		seen_stock += lists[0].entry[got].fallback;
		last = got;
	}
	CHECK(seen_stock > 0);
	/* with everything here (the demakes too) every pack and demake entry comes up sometimes, a fallback never */
	{
		int counts[AE_SOCIAL_ENTRIES];

		world.pack_present = 19; world.demake_present = 1;
		memset(counts, 0, sizeof(counts));
		for (run = 0; run < 3000; run++)
		{
			got = ae_social_playlist_pick(&lists[0], present, &world, -1, (unsigned long)run);
			counts[got]++;
		}
		for (index = 0; index < lists[0].count; index++)
			if (!lists[0].entry[index].fallback)
				CHECK(counts[index] > 0);
			else
				EQ(counts[index], 0);
	}
	/* one candidate map only: it repeats (nothing else to give) */
	world.pack_present = 0; world.stock_present = 1; world.demake_present = 0;
	{
		struct ae_social_playlist one;

		memset(&one, 0, sizeof(one));
		strcpy(one.id, "one"); one.size = 2; one.count = 1;
		strcpy(one.entry[0].map, "prisoner"); strcpy(one.entry[0].gametype, "AE FFA SLAY"); one.entry[0].weight = 2;
		EQ(ae_social_playlist_pick(&one, present, &world, 0, 3), 0);
	}
}

static void test_starts(void)
{
	static char const sample[] =
		"# social: a\n# players: 2\n# starts: br 3, evo, bogus 2, br 5\nwarren@smp:file=AE FFA SLAY\n"
		"# social: b\n# players: 2\n# kind: comp\nwarren@smp:file=AE COMP FFA\n"
		"# social: c\n# players: 2\n# kind: social\nwarren@smp:file=AE FFA SLAY\n"
		"# social: d\n# players: 2\n# kind: comp\n# starts: magnum 2\nwarren@smp:file=AE COMP FFA\n";
	struct ae_social_playlist lists[4];
	int count = ae_social_playlists_parse(sample, lists, 4), seed, got[4];
	unsigned all = (1u << AE_SOCIAL_START_BR) | (1u << AE_SOCIAL_START_EVO) | (1u << AE_SOCIAL_START_MAGNUM);

	EQ(count, 4);
	/* a starts line: the words in order, a weight (none 1), an unknown word and a repeat skipped */
	EQ(lists[0].starts, 2); EQ(lists[0].start_kind[0], AE_SOCIAL_START_BR); EQ(lists[0].start_weight[0], 3);
	EQ(lists[0].start_kind[1], AE_SOCIAL_START_EVO); EQ(lists[0].start_weight[1], 1);
	/* a comp playlist with no line mixes evo and magnum, evenly; a social one with no line mixes none (as before) */
	EQ(lists[1].starts, 2); EQ(lists[1].start_kind[0], AE_SOCIAL_START_EVO); EQ(lists[1].start_kind[1], AE_SOCIAL_START_MAGNUM);
	EQ(lists[1].start_weight[0], 1); EQ(lists[1].start_weight[1], 1);
	EQ(lists[2].starts, 0);
	EQ(ae_social_playlist_start_pick(&lists[2], all, 7), AE_SOCIAL_START_NONE);
	EQ(lists[3].starts, 1); EQ(lists[3].start_kind[0], AE_SOCIAL_START_MAGNUM); EQ(lists[3].start_weight[0], 2);
	/* the pick: the same seed, the same start; by weight (3 : 1 over 4 seeds); only a supported start */
	memset(got, 0, sizeof(got));
	for (seed = 0; seed < 4; seed++)
	{
		EQ(ae_social_playlist_start_pick(&lists[0], all, (unsigned long)seed), ae_social_playlist_start_pick(&lists[0], all, (unsigned long)seed));
		got[ae_social_playlist_start_pick(&lists[0], all, (unsigned long)seed)]++;
	}
	EQ(got[AE_SOCIAL_START_BR], 3); EQ(got[AE_SOCIAL_START_EVO], 1);
	for (seed = 0; seed < 8; seed++)
	{
		EQ(ae_social_playlist_start_pick(&lists[0], 1u << AE_SOCIAL_START_EVO, (unsigned long)seed), AE_SOCIAL_START_EVO);
		EQ(ae_social_playlist_start_pick(&lists[3], (1u << AE_SOCIAL_START_BR) | (1u << AE_SOCIAL_START_EVO), (unsigned long)seed),
			AE_SOCIAL_START_NONE);
		/* the even mix of a comp playlist: both come up */
		got[0] += ae_social_playlist_start_pick(&lists[1], all, (unsigned long)seed) == AE_SOCIAL_START_MAGNUM;
	}
	CHECK(got[0] > 0 && got[0] < 8);
	EQ(ae_social_playlist_start_pick(&lists[0], 0, 3), AE_SOCIAL_START_NONE);
	CHECK(!strcmp(ae_social_start_label(AE_SOCIAL_START_BR), "BATTLE RIFLE") && !ae_social_start_label(0)[0]);
	/* (rc2) the PROTOMAG start: a word of its own, its own weight and label, picked only where the game type has a pistol start */
	{
		static char const proto[] = "# social: e\n# players: 2\n# starts: protomag 2, magnum\nwarren@smp:file=AE BTB SLY\n";
		struct ae_social_playlist one[1];
		unsigned with_pistol = (1u << AE_SOCIAL_START_MAGNUM) | (1u << AE_SOCIAL_START_PROTOMAG);

		EQ(ae_social_playlists_parse(proto, one, 1), 1);
		EQ(one[0].starts, 2); EQ(one[0].start_kind[0], AE_SOCIAL_START_PROTOMAG); EQ(one[0].start_weight[0], 2);
		EQ(one[0].start_kind[1], AE_SOCIAL_START_MAGNUM);
		EQ(AE_SOCIAL_STARTS, 4);
		CHECK(!strcmp(ae_social_start_label(AE_SOCIAL_START_PROTOMAG), "PROTOMAG"));
		for (seed = 0; seed < 8; seed++)
		{
			EQ(ae_social_playlist_start_pick(&one[0], 1u << AE_SOCIAL_START_PROTOMAG, (unsigned long)seed), AE_SOCIAL_START_PROTOMAG);
			EQ(ae_social_playlist_start_pick(&one[0], 1u << AE_SOCIAL_START_BR, (unsigned long)seed), AE_SOCIAL_START_NONE);
			CHECK(ae_social_playlist_start_pick(&one[0], with_pistol, (unsigned long)seed) != AE_SOCIAL_START_NONE);
		}
	}
}

static void test_shown(void)
{
	static char const sample[] =
		"# social: dig\r\n# teams: 2 x 4\r\n# shown: with maps\r\n"
		"1@ws9:file=AE CTF  # weight 3\r\n"
		"# social: open\r\n# teams: 2 x 4\r\n"
		"1@ws9:file=AE CTF  # weight 3\r\n";
	struct ae_social_playlist lists[2];
	struct world world;

	memset(&world, 0, sizeof(world));
	EQ(ae_social_playlists_parse(sample, lists, 2), 2);
	EQ(lists[0].hide_empty, 1); EQ(lists[1].hide_empty, 0);
	/* no map here: the one with the line is hidden, the other shown (as before) */
	EQ(ae_social_playlist_shown(&lists[0], present, &world), 0);
	EQ(ae_social_playlist_shown(&lists[1], present, &world), 1);
	world.workshop_present = 1;
	EQ(ae_social_playlist_shown(&lists[0], present, &world), 1);
}

int main(void)
{
	test_starts();
	test_synthetic();
	test_shown();
	check_shipped();
	test_pick();
	if (failures)
		return 1;
	printf("ae_social_playlists_test: ok\n");
	return 0;
}
