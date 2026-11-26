#include <stdio.h>
#include <string.h>
#include "ae_maps_hub_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static int has_hint(struct ae_hub_hint const *hints, int count, int hint)
{
	int each;

	for (each = 0; each < count; each++)
		if (hints[each].hint == hint)
			return 1;
	return 0;
}

int main(void)
{
	struct ae_hub_hint hints[AE_HUB_HINTS_MOST];
	struct ae_hub_option options[AE_HUB_OPTIONS];
	struct ae_hub_map_flags map;
	char text[40];
	int tab, state, count, source;

	/* the pages: six, in the strip's order, LB and RB wrap */
	CHECK(AE_HUB_TABS == 6);
	CHECK(!strcmp(ae_hub_tab_name(AE_HUB_TAB_INSTALLED), "INSTALLED") && !strcmp(ae_hub_tab_name(AE_HUB_TAB_MODS), "MODS"));
	CHECK(!strcmp(ae_hub_tab_name(AE_HUB_TAB_BROWSE), "BROWSE CE MAPS") && !strcmp(ae_hub_tab_name(AE_HUB_TAB_REQUIRED), "REQUIRED FILES"));
	CHECK(ae_hub_tab_step(AE_HUB_TAB_INSTALLED, -1, 1) == AE_HUB_TAB_ITEM_SETS && ae_hub_tab_step(AE_HUB_TAB_ITEM_SETS, 1, 1) == AE_HUB_TAB_INSTALLED);
	CHECK(ae_hub_tab_step(AE_HUB_TAB_MODS, 1, 1) == AE_HUB_TAB_ITEM_SETS && !strcmp(ae_hub_tab_name(AE_HUB_TAB_ITEM_SETS), "ITEM SETS"));
	CHECK(ae_hub_tab_find("sets") == AE_HUB_TAB_ITEM_SETS && ae_hub_stage_find(AE_HUB_TAB_ITEM_SETS, "refused", &state) && state == 10 &&
		!ae_hub_stage_find(AE_HUB_TAB_ITEM_SETS, "consent", &state));
	CHECK(ae_hub_tab_step(AE_HUB_TAB_REQUIRED, 1, 1) == AE_HUB_TAB_BROWSE && ae_hub_tab_step(AE_HUB_TAB_REQUIRED, -1, 1) == AE_HUB_TAB_INSTALLED);
	CHECK(ae_hub_tab_step(99, 1, 1) == AE_HUB_TAB_INSTALLED);
	for (tab = 0; tab < AE_HUB_TABS; tab++)
		CHECK(ae_hub_tab_step(ae_hub_tab_step(tab, 1, 1), -1, 1) == tab);
	/* ITEM SETS is absent outside DEV MODE: requests fall back and page cycling passes from MODS to INSTALLED. */
	CHECK(!ae_hub_tab_visible(AE_HUB_TAB_ITEM_SETS, 0) && ae_hub_tab_visible(AE_HUB_TAB_ITEM_SETS, 1));
	CHECK(ae_hub_tab_resolve(AE_HUB_TAB_ITEM_SETS, 0) == AE_HUB_TAB_INSTALLED);
	CHECK(ae_hub_tab_step(AE_HUB_TAB_MODS, 1, 0) == AE_HUB_TAB_INSTALLED &&
		ae_hub_tab_step(AE_HUB_TAB_INSTALLED, -1, 0) == AE_HUB_TAB_MODS);

	/* DEV MODE's names: any case, a page and its own states only */
	CHECK(ae_hub_tab_find("Browse") == AE_HUB_TAB_BROWSE && ae_hub_tab_find("mods") == AE_HUB_TAB_MODS && ae_hub_tab_find("maps") == -1);
	CHECK(ae_hub_tab_find(NULL) == -1);
	CHECK(ae_hub_stage_find(AE_HUB_TAB_INSTALLED, "delete", &state) && state == AE_HUB_STATE_DELETE);
	CHECK(ae_hub_stage_find(AE_HUB_TAB_BROWSE, "disk-full", &state) && state == AE_HUB_STATE_DISK_FULL);
	CHECK(ae_hub_stage_find(AE_HUB_TAB_MODS, "RESTART", &state) && state == AE_HUB_STATE_RESTART);
	CHECK(!ae_hub_stage_find(AE_HUB_TAB_MODS, "delete", &state));
	CHECK(!ae_hub_stage_find(AE_HUB_TAB_WORKSHOP, "offline", &state));
	CHECK(ae_hub_stage_find(AE_HUB_TAB_WORKSHOP, "signed-out", &state) && state == AE_HUB_STATE_SIGNED_OUT);
	CHECK(ae_hub_stage_find(AE_HUB_TAB_REQUIRED, "found", &state) && state == AE_HUB_STATE_FOUND);

	/* what may be done with a map by where it came from: only files this game or its player put there */
	CHECK(ae_hub_source_may_delete(AE_HUB_SOURCE_CUSTOM_EDITION) && ae_hub_source_may_delete(AE_HUB_SOURCE_ARCHIVE));
	CHECK(ae_hub_source_may_delete(AE_HUB_SOURCE_HOST) && ae_hub_source_may_delete(AE_HUB_SOURCE_WORKSHOP_INSTALLED));
	CHECK(!ae_hub_source_may_delete(AE_HUB_SOURCE_STOCK) && !ae_hub_source_may_delete(AE_HUB_SOURCE_MCC));
	CHECK(!ae_hub_source_may_delete(AE_HUB_SOURCE_WORKSHOP_STEAM) && !ae_hub_source_may_delete(AE_HUB_SOURCE_DISC));
	CHECK(!strcmp(ae_hub_source_reason(AE_HUB_SOURCE_STOCK), "STOCK MAP") && !strcmp(ae_hub_source_reason(AE_HUB_SOURCE_MCC), "MCC'S FILE"));
	CHECK(!strcmp(ae_hub_source_reason(AE_HUB_SOURCE_WORKSHOP_STEAM), "STEAM KEEPS IT") && !strcmp(ae_hub_source_reason(AE_HUB_SOURCE_DISC), "PART OF A DISC"));
	CHECK(ae_hub_source_reason(AE_HUB_SOURCE_CUSTOM_EDITION) == NULL);
	for (source = 0; source < AE_HUB_SOURCES; source++)
		CHECK(!ae_hub_source_may_move(source) || ae_hub_source_may_delete(source));

	/* SHOW: ALL has every source; each other value its own */
	for (source = 0; source < AE_HUB_SOURCES; source++)
		CHECK(ae_hub_show_has(AE_HUB_SHOW_ALL, source));
	CHECK(ae_hub_show_has(AE_HUB_SHOW_WORKSHOP, AE_HUB_SOURCE_WORKSHOP_STEAM) && ae_hub_show_has(AE_HUB_SHOW_WORKSHOP, AE_HUB_SOURCE_WORKSHOP_INSTALLED));
	CHECK(!ae_hub_show_has(AE_HUB_SHOW_STOCK, AE_HUB_SOURCE_MCC) && ae_hub_show_has(AE_HUB_SHOW_HOSTS, AE_HUB_SOURCE_HOST));
	CHECK(ae_hub_show_has(AE_HUB_SHOW_CUSTOM_EDITION, AE_HUB_SOURCE_ARCHIVE));
	/* CUSTOM EDITION holds Custom Edition maps only; Xbox-format community maps (demakes too) are COMMUNITY's */
	CHECK(ae_hub_show_has(AE_HUB_SHOW_CUSTOM_EDITION, AE_HUB_SOURCE_CUSTOM_EDITION) && !ae_hub_show_has(AE_HUB_SHOW_CUSTOM_EDITION, AE_HUB_SOURCE_XBOX));
	CHECK(ae_hub_show_has(AE_HUB_SHOW_COMMUNITY, AE_HUB_SOURCE_XBOX) && !ae_hub_show_has(AE_HUB_SHOW_COMMUNITY, AE_HUB_SOURCE_STOCK));
	CHECK(ae_hub_show_has(AE_HUB_SHOW_STOCK, AE_HUB_SOURCE_STOCK) && !ae_hub_show_has(AE_HUB_SHOW_STOCK, AE_HUB_SOURCE_XBOX));
	/* every source is under exactly one SHOW value past ALL */
	for (source = 0; source < AE_HUB_SOURCES; source++)
	{
		int shows = 0, show;

		for (show = AE_HUB_SHOW_CUSTOM_EDITION; show < AE_HUB_SHOWS; show++)
			shows += ae_hub_show_has(show, source) != 0;
		CHECK(shows <= 1);
	}
	CHECK(!strcmp(ae_hub_show_name(AE_HUB_SHOW_COMMUNITY), "COMMUNITY"));
	/* the source words are CHANGE MAP's chips */
	CHECK(!strcmp(ae_hub_map_source_name(AE_HUB_SOURCE_XBOX, "h2d_midship"), "COMMUNITY"));
	CHECK(!strcmp(ae_hub_map_source_name(AE_HUB_SOURCE_STOCK, NULL), "STOCK"));
	CHECK(!strcmp(ae_hub_map_source_name(AE_HUB_SOURCE_CUSTOM_EDITION, "grifball@ce"), "CE MAP"));
	CHECK(!strcmp(ae_hub_map_source_name(AE_HUB_SOURCE_ARCHIVE, NULL), "CE MAP"));
	CHECK(!strcmp(ae_hub_map_source_name(AE_HUB_SOURCE_MCC, "timberland@mcc"), "MCC"));
	CHECK(!strcmp(ae_hub_map_source_name(AE_HUB_SOURCE_WORKSHOP_INSTALLED, "x@ws1"), "WORKSHOP"));
	CHECK(!strcmp(ae_hub_map_source_name(AE_HUB_SOURCE_MAP_PACK, "pat_chillout@pro"), "CONVERTED"));
	CHECK(!strcmp(ae_hub_map_source_name(AE_HUB_SOURCE_MAP_PACK, "warren@smp"), "CONVERTED"));
	CHECK(!strcmp(ae_hub_map_source_name(AE_HUB_SOURCE_MAP_PACK, "beavercreek@cex"), "MAP PACK"));
	/* the folder a file is in, from the maps folder down */
	{
		char label[120];

		ae_hub_folder_label("/srv/x/Apps/Main/maps/H2 Demakes", label, sizeof(label));
		CHECK(!strcmp(label, "maps \xE2\x80\xBA H2 Demakes"));
		ae_hub_folder_label("/srv/x/Apps/Main/maps/ce", label, sizeof(label));
		CHECK(!strcmp(label, "maps \xE2\x80\xBA ce"));
		ae_hub_folder_label("C:\\g\\maps_ce\\Arena", label, sizeof(label));
		CHECK(!strcmp(label, "maps_ce \xE2\x80\xBA Arena"));
		ae_hub_folder_label("/h/maps_ce", label, sizeof(label));
		CHECK(!strcmp(label, "maps_ce"));
		ae_hub_folder_label("/s/steamapps/workshop/content/1/2899", label, sizeof(label));
		CHECK(!label[0]);
		ae_hub_folder_label(NULL, label, sizeof(label));
		CHECK(!label[0]);
	}

	/* the options list: delete is its own row, muted with the reason for a file that is not ours, and while a lobby has the map */
	memset(&map, 0, sizeof(map));
	map.source = AE_HUB_SOURCE_CUSTOM_EDITION;
	CHECK(ae_hub_map_options(&map, options) == 4);
	CHECK(!strcmp(options[0].label, "CREATE A GAME ON IT") && options[0].reason == NULL);
	CHECK(options[1].reason == NULL && options[2].reason == NULL && options[3].reason == NULL && !strcmp(options[3].label, "DELETE"));
	map.in_use = 1;
	ae_hub_map_options(&map, options);
	CHECK(!strcmp(options[1].reason, "IN USE") && !strcmp(options[3].reason, "IN USE") && options[2].reason == NULL);
	map.in_use = 0;
	map.source = AE_HUB_SOURCE_STOCK;
	ae_hub_map_options(&map, options);
	CHECK(!strcmp(options[3].reason, "STOCK MAP") && !strcmp(options[1].reason, "STOCK MAP"));
	map.source = AE_HUB_SOURCE_WORKSHOP_STEAM;
	ae_hub_map_options(&map, options);
	CHECK(!strcmp(options[3].reason, "STEAM KEEPS IT"));
	map.kind_campaign = 1;
	ae_hub_map_options(&map, options);
	CHECK(!strcmp(options[0].label, "PLAY IT"));
	/* a map a game can't be hosted on says why in its first row; a Workshop map has a fifth row, its page on Steam */
	map.kind_campaign = 0;
	map.host_reason = "MENU MAP";
	ae_hub_map_options(&map, options);
	CHECK(!strcmp(options[0].reason, "MENU MAP"));
	map.host_reason = NULL;
	map.source = AE_HUB_SOURCE_WORKSHOP_INSTALLED;
	/* a Workshop folder is deleted only when the game's downloader recorded it */
	ae_hub_map_options(&map, options);
	CHECK(!strcmp(options[3].reason, "OPEN STEAM ACCOUNT FIRST"));
	map.client_open = 1;
	ae_hub_map_options(&map, options);
	CHECK(!strcmp(options[3].reason, "NOT DOWNLOADED BY AE") && ae_hub_map_delete_reason(&map));
	map.downloaded = 1;
	ae_hub_map_options(&map, options);
	CHECK(options[3].reason == NULL && ae_hub_map_delete_reason(&map) == NULL);
	map.in_use = 1;
	CHECK(!strcmp(ae_hub_map_delete_reason(&map), "IN USE"));
	map.in_use = 0;
	map.source = AE_HUB_SOURCE_WORKSHOP_STEAM;
	CHECK(!strcmp(ae_hub_map_delete_reason(&map), "STEAM KEEPS IT"));
	map.downloaded = 0;
	map.source = AE_HUB_SOURCE_WORKSHOP_INSTALLED;
	CHECK(ae_hub_map_options(&map, options) == 5 && options[4].option == AE_HUB_OPTION_STEAM_PAGE && options[4].reason == NULL);
	map.source = AE_HUB_SOURCE_CUSTOM_EDITION;
	CHECK(ae_hub_map_options(&map, options) == 4);

	/* a folder's address: an absolute path, backslashes as slashes, other marks percent-encoded; the folder of a file's path */
	{
		char url[200], folder[200];

		CHECK(ae_hub_folder_url("/home/p/maps ce/comp", url, sizeof(url)) && !strcmp(url, "file:///home/p/maps%20ce/comp"));
		CHECK(ae_hub_folder_url("C:\\ae\\maps_ce", url, sizeof(url)) && !strcmp(url, "file:///C:/ae/maps_ce"));
		CHECK(!ae_hub_folder_url("", url, sizeof(url)) && !ae_hub_folder_url("/a/very/long/path", url, 16));
		ae_hub_path_folder("/data/maps_ce/comp/coldwater.map", folder, sizeof(folder));
		CHECK(!strcmp(folder, "/data/maps_ce/comp"));
		ae_hub_path_folder("C:\\maps\\x.map", folder, sizeof(folder));
		CHECK(!strcmp(folder, "C:\\maps"));
		ae_hub_path_folder("x.map", folder, sizeof(folder));
		CHECK(folder[0] == 0);
		ae_hub_workshop_url(3477809506ull, url, sizeof(url));
		CHECK(!strcmp(url, "https://steamcommunity.com/sharedfiles/filedetails/?id=3477809506"));
	}
	/* DEV MODE's states for the fixed actions */
	CHECK(ae_hub_stage_find(AE_HUB_TAB_INSTALLED, "new-folder", &state) && state == AE_HUB_STATE_NEW_FOLDER);
	CHECK(ae_hub_stage_find(AE_HUB_TAB_INSTALLED, "steam", &state) && ae_hub_stage_find(AE_HUB_TAB_INSTALLED, "create", &state));
	CHECK(ae_hub_stage_find(AE_HUB_TAB_BROWSE, "again", &state) && state == AE_HUB_STATE_AGAIN && !ae_hub_stage_find(AE_HUB_TAB_MODS, "again", &state));

	/* the footers: every page ends on Back; a loading list offers no options; the consent card says Not now; moving puts down */
	for (tab = 0; tab < AE_HUB_TABS; tab++)
	{
		count = ae_hub_hints(tab, AE_HUB_STATE_LIST, 0, hints);
		CHECK(count >= 2 && count <= AE_HUB_HINTS_MOST);
		CHECK(hints[count - 1].hint == AE_HUB_HINT_BACK);
	}
	count = ae_hub_hints(AE_HUB_TAB_INSTALLED, AE_HUB_STATE_LOADING, 0, hints);
	CHECK(!has_hint(hints, count, AE_HUB_HINT_OPTIONS) && has_hint(hints, count, AE_HUB_HINT_SEARCH));
	count = ae_hub_hints(AE_HUB_TAB_INSTALLED, AE_HUB_STATE_LIST, 1, hints);
	CHECK(has_hint(hints, count, AE_HUB_HINT_OPTIONS) && has_hint(hints, count, AE_HUB_HINT_FOLDER) && has_hint(hints, count, AE_HUB_HINT_PAGE));
	count = ae_hub_hints(AE_HUB_TAB_BROWSE, AE_HUB_STATE_CONSENT, 0, hints);
	CHECK(!strcmp(hints[count - 1].label, "Not now") && !has_hint(hints, count, AE_HUB_HINT_INSTALL));
	count = ae_hub_hints(AE_HUB_TAB_BROWSE, AE_HUB_STATE_DOWNLOAD, 0, hints);
	CHECK(has_hint(hints, count, AE_HUB_HINT_CANCEL) && !has_hint(hints, count, AE_HUB_HINT_INSTALL));
	count = ae_hub_hints(AE_HUB_TAB_BROWSE, AE_HUB_STATE_OFFLINE, 0, hints);
	CHECK(has_hint(hints, count, AE_HUB_HINT_TRY_AGAIN) && !has_hint(hints, count, AE_HUB_HINT_PICTURE));
	count = ae_hub_hints(AE_HUB_TAB_REQUIRED, AE_HUB_STATE_MISSING, 0, hints);
	CHECK(!strcmp(hints[0].label, "Get it"));
	count = ae_hub_hints(AE_HUB_TAB_REQUIRED, AE_HUB_STATE_FOUND, 0, hints);
	CHECK(!strcmp(hints[0].label, "Select"));
	count = ae_hub_hints(AE_HUB_TAB_MODS, AE_HUB_STATE_MOVING, 0, hints);
	CHECK(has_hint(hints, count, AE_HUB_HINT_PUT_DOWN) && !strcmp(hints[count - 1].label, "Put it back"));
	count = ae_hub_hints(AE_HUB_TAB_MODS, AE_HUB_STATE_RESTART, 0, hints);
	CHECK(has_hint(hints, count, AE_HUB_HINT_RESTART) && has_hint(hints, count, AE_HUB_HINT_LATER));

	/* the rows of a list that fit: 6 at 100 %, fewer as the UI grows, never none */
	CHECK(ae_hub_list_rows(1.0f) == 6);
	CHECK(ae_hub_list_rows(1.3f) < ae_hub_list_rows(1.0f) && ae_hub_list_rows(1.3f) >= 1);
	CHECK(ae_hub_list_rows(0.9f) >= 6 && ae_hub_list_rows(3.0f) == 1);

	/* sizes and counts as written */
	ae_hub_size_text(38ull * 1024 * 1024, text, sizeof(text));
	CHECK(!strcmp(text, "38 MB"));
	ae_hub_size_text((unsigned long long)(21.6 * 1024.0 * 1024.0 * 1024.0), text, sizeof(text));
	CHECK(!strcmp(text, "21.6 GB"));
	ae_hub_size_text(812ull * 1024, text, sizeof(text));
	CHECK(!strcmp(text, "812 KB"));
	ae_hub_count_text(1284, text, sizeof(text));
	CHECK(!strcmp(text, "1,284"));
	ae_hub_count_text(5717, text, sizeof(text));
	CHECK(!strcmp(text, "5,717"));
	ae_hub_count_text(12, text, sizeof(text));
	CHECK(!strcmp(text, "12"));
	ae_hub_count_text(1000000, text, sizeof(text));
	CHECK(!strcmp(text, "1,000,000"));
	ae_hub_badge_text(2, text, sizeof(text));
	CHECK(!strcmp(text, "DOWNLOADING 2"));
	ae_hub_badge_text(0, text, sizeof(text));
	CHECK(text[0] == 0);

	if (failures)
		printf("%d failures\n", failures);
	else
		printf("ok\n");
	return failures ? 1 : 0;
}
