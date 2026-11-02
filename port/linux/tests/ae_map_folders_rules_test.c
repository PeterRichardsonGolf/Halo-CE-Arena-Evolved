/*
AE_MAP_FOLDERS_RULES_TEST.C

The map lists' pages (port/linux/game/ae_map_folders_rules.c): which folders are pages and their names, the
folders' order, the first of a name winning, the pages built (STOCK first, the folders A-Z, MULTIPLAYER last, no
empty one) and the chooser's values.
*/

#include <stdio.h>
#include <string.h>

#include "../game/ae_map_folders_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static struct ae_map_folder_index index_under_test;
static struct ae_map_pages pages;

static void folders_and_names(void)
{
	char out[AE_MAP_PAGE_NAME_SIZE];
	char tiny[4];
	char long_name[80];

	/* (pages: the folders a map pack ships, a player's own) */
	CHECK(ae_map_folders_folder_is_page("H2 Demakes"));
	CHECK(ae_map_folders_folder_is_page("Comp Variants"));
	CHECK(ae_map_folders_folder_is_page("CE+ X"));
	CHECK(ae_map_folders_folder_is_page("H2CE"));
	/* (not pages: Custom Edition's folder, the pictures and layouts, MCC's resources, hidden and own ones) */
	CHECK(!ae_map_folders_folder_is_page("ce"));
	CHECK(!ae_map_folders_folder_is_page("CE"));
	CHECK(!ae_map_folders_folder_is_page("previews"));
	CHECK(!ae_map_folders_folder_is_page("Layouts"));
	CHECK(!ae_map_folders_folder_is_page("_shared"));
	CHECK(!ae_map_folders_folder_is_page("_mine"));
	CHECK(!ae_map_folders_folder_is_page(".git"));
	CHECK(!ae_map_folders_folder_is_page("."));
	CHECK(!ae_map_folders_folder_is_page(".."));
	CHECK(!ae_map_folders_folder_is_page(""));
	CHECK(!ae_map_folders_folder_is_page(NULL));
	CHECK(!ae_map_folders_folder_is_page("a/b"));
	CHECK(!ae_map_folders_folder_is_page("a\\b"));
	CHECK(!ae_map_folders_folder_is_page("c:x"));
	CHECK(!ae_map_folders_folder_is_page("a|b"));
	CHECK(!ae_map_folders_folder_is_page("tab\there"));
	CHECK(!ae_map_folders_folder_is_page(" padded"));
	CHECK(!ae_map_folders_folder_is_page("padded "));
	memset(long_name, 'x', sizeof(long_name));
	long_name[AE_MAP_FOLDER_NAME_SIZE] = 0;
	CHECK(!ae_map_folders_folder_is_page(long_name));
	long_name[AE_MAP_FOLDER_NAME_SIZE - 1] = 0;
	CHECK(ae_map_folders_folder_is_page(long_name));

	/* (named in capitals) */
	CHECK(ae_map_folders_page_name("H2 Demakes", out, sizeof(out)) && !strcmp(out, "H2 DEMAKES"));
	CHECK(ae_map_folders_page_name("comp variants", out, sizeof(out)) && !strcmp(out, "COMP VARIANTS"));
	CHECK(ae_map_folders_page_name("CE+ X", out, sizeof(out)) && !strcmp(out, "CE+ X"));
	CHECK(ae_map_folders_page_name("caf\xc3\xa9", out, sizeof(out)) && !strcmp(out, "CAF??"));
	/* (cut to the page's size, no space left at the end) */
	CHECK(ae_map_folders_page_name("a very long folder name for maps", out, sizeof(out)) &&
		!strcmp(out, "A VERY LONG FOLDER NAME"));
	CHECK(ae_map_folders_page_name("abcdefghijklmnopqrstuvwxyz", out, sizeof(out)) &&
		strlen(out) == AE_MAP_PAGE_NAME_SIZE - 1 && !strcmp(out, "ABCDEFGHIJKLMNOPQRSTUVWX"));
	CHECK(ae_map_folders_page_name("abc def", tiny, sizeof(tiny)) && !strcmp(tiny, "ABC"));
	/* (the list's own pages' names are no folder's: its maps are loose) */
	CHECK(!ae_map_folders_page_name("Stock", out, sizeof(out)) && !out[0]);
	CHECK(!ae_map_folders_page_name("multiplayer", out, sizeof(out)) && !out[0]);
	CHECK(!ae_map_folders_page_name("SinglePlayer", out, sizeof(out)) && !out[0]);
	CHECK(!ae_map_folders_page_name("layouts", out, sizeof(out)) && !out[0]);
	CHECK(!ae_map_folders_page_name("x", NULL, 4));
	CHECK(!ae_map_folders_page_name("x", out, 0));
}

static void order(void)
{
	char folders[5][AE_MAP_FOLDER_NAME_SIZE] = { "h2 demakes", "Comp Variants", "CE+ X", "H2 Demakes", "alpha" };

	CHECK(ae_map_folders_compare("alpha", "Beta") < 0);
	CHECK(ae_map_folders_compare("Beta", "alpha") > 0);
	CHECK(ae_map_folders_compare("same", "same") == 0);
	CHECK(ae_map_folders_compare("H2", "h2") != 0);
	ae_map_folders_sort(folders, 5);
	CHECK(!strcmp(folders[0], "alpha"));
	CHECK(!strcmp(folders[1], "CE+ X"));
	CHECK(!strcmp(folders[2], "Comp Variants"));
	CHECK(!strcmp(folders[3], "H2 Demakes"));
	CHECK(!strcmp(folders[4], "h2 demakes"));
}

static void first_wins(void)
{
	int i;
	char name[16];

	ae_map_folder_index_clear(&index_under_test);
	CHECK(ae_map_folder_index_add(&index_under_test, "h2_relic", "H2 Demakes") == 1);
	CHECK(ae_map_folder_index_add(&index_under_test, "H2_Relic", "Comp Variants") == 0);
	CHECK(!strcmp(ae_map_folder_index_find(&index_under_test, "H2_RELIC"), "H2 Demakes"));
	CHECK(ae_map_folder_index_find(&index_under_test, "h2_relic_v2") == NULL);
	CHECK(ae_map_folder_index_find(&index_under_test, NULL) == NULL);
	CHECK(ae_map_folder_index_add(&index_under_test, "", "x") == -1);
	CHECK(ae_map_folder_index_add(&index_under_test, "x", NULL) == -1);
	for (i = index_under_test.count; i < AE_MAP_FOLDER_INDEX_SIZE; i++)
	{
		snprintf(name, sizeof(name), "map%d", i);
		CHECK(ae_map_folder_index_add(&index_under_test, name, "F") == 1);
	}
	CHECK(ae_map_folder_index_add(&index_under_test, "one_more", "F") == -1);
	CHECK(ae_map_folder_index_add(&index_under_test, "map3", "F") == 0);
}

static void built_pages(void)
{
	/* a list: 13 stock rows, then community and Custom Edition maps */
	enum { COUNT = 20 };
	unsigned char stock[COUNT];
	char const *folder_pages[COUNT];
	int i;

	memset(stock, 0, sizeof(stock));
	memset(folder_pages, 0, sizeof(folder_pages));
	for (i = 0; i < 13; i++)
		stock[i] = 1;
	folder_pages[13] = "H2 DEMAKES";
	folder_pages[14] = NULL;
	folder_pages[15] = "COMP VARIANTS";
	folder_pages[16] = "H2 DEMAKES";
	folder_pages[17] = "";
	folder_pages[18] = "STOCK";
	folder_pages[19] = "COMP VARIANTS";
	/* (a stock map in a folder is still STOCK's) */
	folder_pages[9] = "H2 DEMAKES";
	ae_map_pages_build(&pages, COUNT, stock, folder_pages);
	CHECK(pages.page_count == 4);
	CHECK(!strcmp(pages.names[0], "STOCK") && pages.count[0] == 13);
	CHECK(!strcmp(pages.names[1], "COMP VARIANTS") && pages.count[1] == 2);
	CHECK(!strcmp(pages.names[2], "H2 DEMAKES") && pages.count[2] == 2);
	CHECK(!strcmp(pages.names[3], "MULTIPLAYER") && pages.count[3] == 3);
	CHECK(ae_map_pages_map_at(&pages, 0, 9) == 9);
	CHECK(ae_map_pages_map_at(&pages, 1, 0) == 15 && ae_map_pages_map_at(&pages, 1, 1) == 19);
	CHECK(ae_map_pages_map_at(&pages, 2, 0) == 13 && ae_map_pages_map_at(&pages, 2, 1) == 16);
	CHECK(ae_map_pages_map_at(&pages, 3, 0) == 14 && ae_map_pages_map_at(&pages, 3, 1) == 17 &&
		ae_map_pages_map_at(&pages, 3, 2) == 18);
	CHECK(ae_map_pages_map_at(&pages, 3, 3) == -1);
	CHECK(ae_map_pages_map_at(&pages, 4, 0) == -1);
	CHECK(ae_map_pages_map_at(&pages, -1, 0) == -1);
	CHECK(ae_map_pages_page_of(&pages, 16) == 2 && ae_map_pages_entry_of(&pages, 2, 16) == 1);
	CHECK(ae_map_pages_page_of(&pages, 9) == 0);
	CHECK(ae_map_pages_page_of(&pages, 18) == 3);
	CHECK(ae_map_pages_page_of(&pages, 25) == -1 && ae_map_pages_page_of(&pages, -1) == -1);
	CHECK(ae_map_pages_find(&pages, "H2 DEMAKES") == 2);
	CHECK(ae_map_pages_find(&pages, "H2CE") == -1);

	/* (no loose maps, no folders: STOCK alone; nothing: no page) */
	ae_map_pages_build(&pages, 13, stock, NULL);
	CHECK(pages.page_count == 1 && !strcmp(pages.names[0], "STOCK"));
	ae_map_pages_build(&pages, 0, NULL, NULL);
	CHECK(pages.page_count == 0);
	/* (no stock maps: the list starts with the folders) */
	ae_map_pages_build(&pages, 3, NULL, folder_pages + 13);
	CHECK(pages.page_count == 3 && !strcmp(pages.names[0], "COMP VARIANTS") && !strcmp(pages.names[1], "H2 DEMAKES") &&
		!strcmp(pages.names[2], "MULTIPLAYER"));
}

static void too_many_folders(void)
{
	enum { COUNT = 40 };
	static char names[COUNT][16];
	char const *folder_pages[COUNT];
	int i, total = 0;

	for (i = 0; i < COUNT; i++)
	{
		snprintf(names[i], sizeof(names[i]), "F%02d", i);
		folder_pages[i] = names[i];
	}
	ae_map_pages_build(&pages, COUNT, NULL, folder_pages);
	CHECK(pages.page_count == AE_MAP_PAGES_MAXIMUM - 1);
	CHECK(!strcmp(pages.names[0], "F00") && !strcmp(pages.names[pages.page_count - 1], "MULTIPLAYER"));
	/* (the folders past the room on MULTIPLAYER: none lost) */
	CHECK(pages.count[pages.page_count - 1] == COUNT - (AE_MAP_PAGES_MAXIMUM - 2));
	for (i = 0; i < pages.page_count; i++)
		total += pages.count[i];
	CHECK(total == COUNT);
}

static void chooser(void)
{
	/* (New Game and the Map screen when hosting: SINGLEPLAYER first) */
	CHECK(ae_map_chooser_count(4, 1) == 5);
	CHECK(ae_map_chooser_page(0, 4, 1) == -1);
	CHECK(ae_map_chooser_page(1, 4, 1) == 0);
	CHECK(ae_map_chooser_page(4, 4, 1) == 3);
	CHECK(ae_map_chooser_page(9, 4, 1) == 3);
	CHECK(ae_map_chooser_page(-3, 4, 1) == -1);
	CHECK(ae_map_chooser_value(-1, 4, 1) == 0);
	CHECK(ae_map_chooser_value(2, 4, 1) == 3);
	CHECK(ae_map_chooser_value(7, 4, 1) == 4);
	/* (split screen: the pages alone) */
	CHECK(ae_map_chooser_count(4, 0) == 4);
	CHECK(ae_map_chooser_page(0, 4, 0) == 0);
	CHECK(ae_map_chooser_page(3, 4, 0) == 3);
	CHECK(ae_map_chooser_value(-1, 4, 0) == 0);
	CHECK(ae_map_chooser_value(3, 4, 0) == 3);
	/* (never an empty spinner) */
	CHECK(ae_map_chooser_count(0, 0) == 1);
	CHECK(ae_map_chooser_count(0, 1) == 1);
}

int main(void)
{
	folders_and_names();
	order();
	first_wins();
	built_pages();
	too_many_folders();
	chooser();
	if (failures)
	{
		printf("%d failed\n", failures);
		return 1;
	}
	printf("ae_map_folders_rules: all passed\n");
	return 0;
}
