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

	memset(map, 0, sizeof(*map));
	snprintf(map->file, sizeof(map->file), "%s", file);
	snprintf(map->name, sizeof(map->name), "%s", file);
	map->groups = groups;
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
	count = ae_setup_map_rows(&maps, AE_MF_ALL, -1, rows, 64);
	CHECK(count == 4 && rows[0] == -1 - HALO_CE && rows[3] == -1 - MOD_MAPS);
	count = ae_setup_map_rows(&maps, AE_MF_ALL, MOD_MAPS, rows, 64);
	CHECK(count == 7 && rows[4] == 3 && rows[5] == 4 && rows[6] == 5);
	count = ae_setup_map_rows(&maps, AE_MF_ALL, HALO_CE, rows, 64);
	CHECK(count == 6 && rows[1] == 0 && rows[2] == 1 && rows[3] == -1 - WARM_UP);
	/* a filter hides a group with nothing for it (WARM UP has no CTF map) and the open group's misfits */
	count = ae_setup_map_rows(&maps, AE_MF_CTF, HALO_CE, rows, 64);
	CHECK(count == 4 && rows[0] == -1 - HALO_CE && rows[1] == 1 && rows[2] == -1 - BTB && rows[3] == -1 - MOD_MAPS);
	CHECK(ae_setup_map_group_row(rows, count, WARM_UP) == -1 && ae_setup_map_group_row(rows, count, BTB) == 2);
	/* a short list */
	CHECK(ae_setup_map_rows(&maps, AE_MF_ALL, MOD_MAPS, rows, 5) == 5 && ae_setup_map_rows(NULL, 0, 0, rows, 5) == 0);

	/* LB / RB: group rows only, wrapping */
	count = ae_setup_map_rows(&maps, AE_MF_ALL, HALO_CE, rows, 64);
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

	if (failures) return 1;
	printf("ae_setup_maps: ok\n");
	return 0;
}
