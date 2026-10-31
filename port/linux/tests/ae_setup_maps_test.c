#include <stdio.h>
#include <string.h>
#include "ae_setup_maps.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

/* (static: the maps are about 100 KB) */
static struct ae_setup_maps maps;

enum { HALO_CE, WARM_UP, BTB, MOD_MAPS, GROUPS };

static void add(const char *file, unsigned int groups, int facts, int modes, char size, int nhe)
{
	struct ae_setup_map *map = &maps.maps[maps.count];
	int group;

	memset(map, 0, sizeof(*map));
	snprintf(map->file, sizeof(map->file), "%s", file);
	snprintf(map->name, sizeof(map->name), "%s", file);
	for (group = 0; group < 32; group++)
	{
		if (groups & (1u << group))
			ae_setup_map_join(map, group);
	}
	/* (its source: its first group) */
	for (group = 0; group < 32 && !(groups & (1u << group)); group++)
		;
	map->source = (short)(group < 32 ? group : -1);
	map->facts = facts;
	map->modes = modes;
	map->size = size;
	map->nhe = nhe;
	map->list_index = maps.count;
	maps.count++;
}

int main(void)
{
	short rows[64], count, index;
	int filters[AE_MAP_FILTERS];
	int all = AE_SETUP_MODE_CTF | AE_SETUP_MODE_SLAYER | AE_SETUP_MODE_ODDBALL | AE_SETUP_MODE_KING;

	memset(&maps, 0, sizeof(maps));
	maps.group_count = GROUPS;
	strcpy(maps.group_names[HALO_CE], "HALO CE");
	strcpy(maps.group_names[WARM_UP], "WARM UP");
	strcpy(maps.group_names[BTB], "BTB");
	strcpy(maps.group_names[MOD_MAPS], "MOD MAPS");
	/* 0 prisoner: small, slayer + ball + king; 1 bloodgulch: large, every mode (HALO CE and BTB); 2 octagon: slayer only
	(WARM UP); 3 hotbox: not read yet (MOD MAPS); 4 dammy: an NHE map, CTF + slayer, medium (MOD MAPS); 5 broken:
	unreadable (MOD MAPS) */
	add("prisoner", 1u << HALO_CE, AE_SETUP_FACTS_READ, AE_SETUP_MODE_SLAYER | AE_SETUP_MODE_ODDBALL | AE_SETUP_MODE_KING,
		's', 0);
	add("bloodgulch", 1u << HALO_CE | 1u << BTB, AE_SETUP_FACTS_READ, all, 'l', 0);
	add("octagon", 1u << WARM_UP, AE_SETUP_FACTS_READ, AE_SETUP_MODE_SLAYER, 's', 0);
	add("hotbox", 1u << MOD_MAPS, AE_SETUP_FACTS_NONE, 0, 0, 0);
	add("dammy", 1u << MOD_MAPS, AE_SETUP_FACTS_READ, AE_SETUP_MODE_CTF | AE_SETUP_MODE_SLAYER, 'm', 1);
	add("broken", 1u << MOD_MAPS, AE_SETUP_FACTS_UNREADABLE, 0, 0, 0);

	/* the filters */
	CHECK(!strcmp(ae_setup_map_filter_name(AE_MF_TEAM_SLAYER), "AE TEAM SLAY") && !ae_setup_map_filter_name(99)[0]);
	CHECK(ae_setup_map_suiting(&maps, AE_MF_ALL) == 6);
	CHECK(ae_setup_map_suiting(&maps, AE_MF_CTF) == 4);           /* bloodgulch, dammy, and the two unread */
	CHECK(ae_setup_map_suiting(&maps, AE_MF_TEAM_SLAYER) == 6);
	CHECK(ae_setup_map_suiting(&maps, AE_MF_FFA_SLAYER) == 5);    /* not bloodgulch: large */
	CHECK(ae_setup_map_suiting(&maps, AE_MF_NHE) == 1);           /* NHE: its tag, never "unknown suits" */
	CHECK(!ae_setup_map_suits(&maps.maps[0], AE_MF_CTF) && ae_setup_map_suits(&maps.maps[3], AE_MF_CTF));
	CHECK(!ae_setup_map_suits(NULL, AE_MF_ALL) && !ae_setup_map_suits(&maps.maps[0], AE_MAP_FILTERS));
	/* the groups' counts */
	CHECK(ae_setup_map_group_count(&maps, HALO_CE, AE_MF_ALL) == 2 && ae_setup_map_group_count(&maps, BTB, AE_MF_KING) == 1);
	CHECK(ae_setup_map_group_count(&maps, WARM_UP, AE_MF_CTF) == 0 && ae_setup_map_group_count(&maps, GROUPS, 0) == 0);

	/* the rows: groups only, then one open in place */
	count = ae_setup_map_rows(&maps, AE_MF_ALL, -1, 0, rows, 64);
	CHECK(count == 4 && rows[0] == -1 - HALO_CE && rows[3] == -1 - MOD_MAPS);
	count = ae_setup_map_rows(&maps, AE_MF_ALL, MOD_MAPS, 0, rows, 64);
	CHECK(count == 7 && rows[4] == 3 && rows[5] == 4 && rows[6] == 5);
	count = ae_setup_map_rows(&maps, AE_MF_ALL, HALO_CE, 0, rows, 64);
	CHECK(count == 6 && rows[1] == 0 && rows[2] == 1 && rows[3] == -1 - WARM_UP);
	/* a filter hides a group with nothing for it (WARM UP has no CTF map) and the open group's misfits */
	count = ae_setup_map_rows(&maps, AE_MF_CTF, HALO_CE, 0, rows, 64);
	CHECK(count == 4 && rows[0] == -1 - HALO_CE && rows[1] == 1 && rows[2] == -1 - BTB && rows[3] == -1 - MOD_MAPS);
	CHECK(ae_setup_map_group_row(rows, count, WARM_UP) == -1 && ae_setup_map_group_row(rows, count, BTB) == 2);
	/* a short list */
	CHECK(ae_setup_map_rows(&maps, AE_MF_ALL, MOD_MAPS, 0, rows, 5) == 5 && ae_setup_map_rows(NULL, 0, 0, 0, rows, 5) == 0);

	/* LB / RB: group rows only, wrapping */
	count = ae_setup_map_rows(&maps, AE_MF_ALL, HALO_CE, 0, rows, 64);
	CHECK(ae_setup_map_group_step(rows, count, 1, 1) == 3);      /* from prisoner: WARM UP */
	CHECK(ae_setup_map_group_step(rows, count, 0, -1) == 5);     /* HALO CE back: MOD MAPS */
	CHECK(ae_setup_map_group_step(rows, count, 5, 1) == 0);
	for (index = 0; index < count; index++)
		CHECK(ae_setup_map_row_is_group(rows[ae_setup_map_group_step(rows, count, index, 1)]));

	/* plays well with: read facts only; NHE for an NHE map */
	CHECK(ae_setup_map_plays_well(&maps.maps[0], filters, AE_MAP_FILTERS) == 4 && filters[0] == AE_MF_FFA_SLAYER &&
		filters[3] == AE_MF_BALL);
	CHECK(ae_setup_map_plays_well(&maps.maps[1], filters, AE_MAP_FILTERS) == 4 && filters[0] == AE_MF_TEAM_SLAYER);
	CHECK(ae_setup_map_plays_well(&maps.maps[3], filters, AE_MAP_FILTERS) == 0);
	CHECK(ae_setup_map_plays_well(&maps.maps[4], filters, AE_MAP_FILTERS) == 4 && filters[2] == AE_MF_CTF &&
		filters[3] == AE_MF_NHE);
	CHECK(ae_setup_map_plays_well(&maps.maps[1], filters, 2) == 2);

	/* (Phase 2) RECENT's games first, always open; groups past 32 */
	count = ae_setup_map_rows(&maps, AE_MF_ALL, -1, 3, rows, 64);
	CHECK(count == 8 && rows[0] == AE_SETUP_ROW_RECENT && rows[1] == AE_SETUP_ROW_RECENT_GAME &&
		ae_setup_map_row_is_recent(rows[3]) && ae_setup_map_row_recent(rows[3]) == 2 && rows[4] == -1 - HALO_CE);
	CHECK(!ae_setup_map_row_is_group(rows[0]) && !ae_setup_map_row_is_group(rows[1]) && !ae_setup_map_row_is_recent(rows[0]));
	/* RECENT folded: its header only, whatever folder is open (its own flag); open by default */
	CHECK(!maps.recent_folded);
	maps.recent_folded = 1;
	count = ae_setup_map_rows(&maps, AE_MF_ALL, -1, 3, rows, 64);
	CHECK(count == 5 && rows[0] == AE_SETUP_ROW_RECENT && rows[1] == -1 - HALO_CE);
	count = ae_setup_map_rows(&maps, AE_MF_ALL, HALO_CE, 3, rows, 64);
	CHECK(rows[0] == AE_SETUP_ROW_RECENT && rows[1] == -1 - HALO_CE && rows[2] >= 0);
	maps.recent_folded = 0;
	CHECK(ae_setup_map_group_step(rows, count, 1, 1) == 4);
	/* a folder of the player's: listed with nothing in it, its empty line when open; listed under a filter too */
	strcpy(maps.group_names[GROUPS], "Comp practice");
	maps.group_flags[GROUPS] = AE_SETUP_GROUP_LISTED | AE_SETUP_GROUP_OWN;
	strcpy(maps.group_names[GROUPS + 1], "Infection maps");
	maps.group_flags[GROUPS + 1] = AE_SETUP_GROUP_LISTED | AE_SETUP_GROUP_OWN;
	strcpy(maps.group_names[100], "FAR");
	maps.group_count = 101;
	ae_setup_map_join(&maps.maps[2], GROUPS + 1);
	ae_setup_map_join(&maps.maps[2], 100);
	CHECK(ae_setup_map_in(&maps.maps[2], 100) && !ae_setup_map_in(&maps.maps[1], 100) && !ae_setup_map_in(&maps.maps[2], 128));
	count = ae_setup_map_rows(&maps, AE_MF_ALL, GROUPS, 0, rows, 64);
	CHECK(count == 8 && rows[4] == -1 - GROUPS && rows[5] == AE_SETUP_ROW_EMPTY && rows[6] == -1 - (GROUPS + 1) &&
		rows[7] == -1 - 100);
	count = ae_setup_map_rows(&maps, AE_MF_CTF, GROUPS + 1, 0, rows, 64);
	CHECK(count == 5 && rows[3] == -1 - GROUPS && rows[4] == -1 - (GROUPS + 1));   /* octagon: no CTF; no line */
	ae_setup_map_leave(&maps.maps[2], 100);
	CHECK(!ae_setup_map_in(&maps.maps[2], 100) && ae_setup_map_group_count(&maps, 100, AE_MF_ALL) == 0);

	/* the search: name, credits, the source group's name; by source in the groups' order */
	strcpy(maps.maps[4].credits, "Halo 1: NHE");
	CHECK(ae_setup_map_match("Battle Creek", "cr") == 7 && ae_setup_map_match("x", "") == -1);
	CHECK(ae_setup_map_matching(&maps, AE_MF_ALL, "on") == 2);         /* prisoner, octagon */
	CHECK(ae_setup_map_matching(&maps, AE_MF_ALL, "nhe") == 1 && ae_setup_map_matching(&maps, AE_MF_ALL, "mod") == 3);
	CHECK(ae_setup_map_matching(&maps, AE_MF_ALL, NULL) == 6 && ae_setup_map_matching(&maps, AE_MF_FFA_SLAYER, "") == 5);
	maps.maps[0].source = MOD_MAPS;
	count = ae_setup_map_search(&maps, AE_MF_ALL, "on", rows, 64);
	CHECK(count == 2 && rows[0] == 2 && rows[1] == 0);
	maps.maps[2].source = -1;
	count = ae_setup_map_search(&maps, AE_MF_ALL, "on", rows, 64);
	CHECK(count == 2 && rows[0] == 0 && rows[1] == 2 && ae_setup_map_search(&maps, AE_MF_ALL, "zzz", rows, 64) == 0);

	/* folders in a folder (0.6.0, WORKSHOP's collections): a child listed only while its parent or a sibling is open,
	right under the parent; an open parent lists its children, not its own maps; a map in two children counts once */
	{
		enum { STOCK, WORKSHOP, PRO, SOCIAL, MODS };

		memset(&maps, 0, sizeof(maps));
		maps.group_count = MODS + 1;
		maps.group_parent[PRO] = WORKSHOP + 1;
		maps.group_parent[SOCIAL] = WORKSHOP + 1;
		add("bloodgulch", 1u << STOCK, AE_SETUP_FACTS_READ, all, 'l', 0);
		add("creek_pro", 1u << WORKSHOP | 1u << PRO, AE_SETUP_FACTS_READ, all, 's', 0);
		add("ratrace_pro", 1u << WORKSHOP | 1u << PRO, AE_SETUP_FACTS_READ, AE_SETUP_MODE_SLAYER, 's', 0);
		add("coldsnap", 1u << WORKSHOP | 1u << SOCIAL, AE_SETUP_FACTS_READ, all, 'm', 0);
		add("both", 1u << WORKSHOP | 1u << PRO | 1u << SOCIAL, AE_SETUP_FACTS_READ, all, 'm', 0);
		add("hotbox", 1u << MODS, AE_SETUP_FACTS_READ, all, 's', 0);
		CHECK(ae_setup_map_group_parent(&maps, PRO) == WORKSHOP && ae_setup_map_group_parent(&maps, WORKSHOP) == -1);
		CHECK(ae_setup_map_group_parent(&maps, -1) == -1 && ae_setup_map_group_parent(&maps, MODS + 1) == -1);
		CHECK(ae_setup_map_group_count(&maps, WORKSHOP, AE_MF_ALL) == 4);
		count = ae_setup_map_rows(&maps, AE_MF_ALL, -1, 0, rows, 64);
		CHECK(count == 3 && rows[0] == -1 - STOCK && rows[1] == -1 - WORKSHOP && rows[2] == -1 - MODS);
		count = ae_setup_map_rows(&maps, AE_MF_ALL, WORKSHOP, 0, rows, 64);
		CHECK(count == 5 && rows[1] == -1 - WORKSHOP && rows[2] == -1 - PRO && rows[3] == -1 - SOCIAL &&
			rows[4] == -1 - MODS);
		count = ae_setup_map_rows(&maps, AE_MF_ALL, PRO, 0, rows, 64);
		CHECK(count == 8 && rows[2] == -1 - PRO && rows[3] == 1 && rows[4] == 2 && rows[5] == 4 &&
			rows[6] == -1 - SOCIAL && rows[7] == -1 - MODS);
		count = ae_setup_map_rows(&maps, AE_MF_ALL, SOCIAL, 0, rows, 64);
		CHECK(count == 7 && rows[3] == -1 - SOCIAL && rows[4] == 3 && rows[5] == 4 && rows[6] == -1 - MODS);
		/* (a child with no map for the filter is not listed; its parent still is while another child has one) */
		count = ae_setup_map_rows(&maps, AE_MF_FFA_SLAYER, WORKSHOP, 0, rows, 64);
		CHECK(count == 4 && rows[0] == -1 - WORKSHOP);   /* bloodgulch: large */
		maps.maps[3].modes = maps.maps[4].modes = AE_SETUP_MODE_SLAYER;
		count = ae_setup_map_rows(&maps, AE_MF_CTF, WORKSHOP, 0, rows, 64);
		CHECK(count == 4 && rows[2] == -1 - PRO && rows[3] == -1 - MODS);
		/* open: the open group or the open group's parent */
		CHECK(ae_setup_map_group_open(&maps, PRO, PRO) && ae_setup_map_group_open(&maps, PRO, WORKSHOP));
		CHECK(!ae_setup_map_group_open(&maps, PRO, SOCIAL) && !ae_setup_map_group_open(&maps, -1, WORKSHOP));
		CHECK(ae_setup_map_rows(&maps, AE_MF_ALL, PRO, 0, rows, 4) == 4);
	}

	if (failures) return 1;
	printf("ae_setup_maps: ok\n");
	return 0;
}
