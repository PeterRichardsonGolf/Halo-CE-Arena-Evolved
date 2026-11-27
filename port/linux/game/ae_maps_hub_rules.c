/*
AE_MAPS_HUB_RULES.C

MAPS & MODS' rules (ae_maps_hub_rules.h). Plain C.
*/

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "ae_maps_hub_rules.h"

static char const *const tab_names[AE_HUB_TABS] = { "INSTALLED", "REQUIRED FILES", "BROWSE CE MAPS", "WORKSHOP", "MODS",
	"ITEM SETS" };
static char const *const tab_words[AE_HUB_TABS] = { "installed", "required", "browse", "workshop", "mods", "sets" };
static char const *const source_names[AE_HUB_SOURCES] = { "CE MAP", "MCC", "WORKSHOP", "WORKSHOP", "MAP PACK",
	"STOCK", "FROM A HOST", "CE MAP", "MODDED DISC", "COMMUNITY" };
static char const *const show_names[AE_HUB_SHOWS] = { "ALL", "CUSTOM EDITION", "COMMUNITY", "MCC", "WORKSHOP", "MAP PACKS", "STOCK",
	"FROM HOSTS" };

static int same(char const *a, char const *b)
{
	for (; *a && *b; a++, b++)
		if (tolower((unsigned char)*a) != tolower((unsigned char)*b))
			return 0;
	return *a == *b;
}

const char *ae_hub_tab_name(int tab)
{
	return tab >= 0 && tab < AE_HUB_TABS ? tab_names[tab] : "";
}

int ae_hub_tab_visible(int tab, int dev_mode)
{
	/* (every page is shown to everyone: ITEM SETS too, marked as a testing feature on its page) */
	(void)dev_mode;
	return tab >= 0 && tab < AE_HUB_TABS;
}

int ae_hub_tabs_shown(int dev_mode)
{
	int tab, shown = 0;

	for (tab = 0; tab < AE_HUB_TABS; tab++)
		shown += ae_hub_tab_visible(tab, dev_mode);
	return shown;
}

int ae_hub_tab_resolve(int tab, int dev_mode)
{
	return ae_hub_tab_visible(tab, dev_mode) ? tab : AE_HUB_TAB_INSTALLED;
}

int ae_hub_tab_find(const char *word)
{
	int tab;

	if (!word)
		return -1;
	for (tab = 0; tab < AE_HUB_TABS; tab++)
		if (same(word, tab_words[tab]))
			return tab;
	return -1;
}

int ae_hub_tab_step(int tab, int direction, int dev_mode)
{
	int next, tries;

	if (!ae_hub_tab_visible(tab, dev_mode))
		return AE_HUB_TAB_INSTALLED;
	next = tab;
	for (tries = 0; tries < AE_HUB_TABS; tries++)
	{
		next = (next + (direction < 0 ? AE_HUB_TABS - 1 : 1)) % AE_HUB_TABS;
		if (ae_hub_tab_visible(next, dev_mode))
			return next;
	}
	return AE_HUB_TAB_INSTALLED;
}

struct stage { char const *word; int state; };
static struct stage const installed_stages[] = { { "list", AE_HUB_STATE_LIST }, { "options", AE_HUB_STATE_OPTIONS },
	{ "folder", AE_HUB_STATE_FOLDER }, { "delete", AE_HUB_STATE_DELETE }, { "in-use", AE_HUB_STATE_IN_USE },
	{ "loading", AE_HUB_STATE_LOADING }, { "empty", AE_HUB_STATE_EMPTY }, { "unreadable", AE_HUB_STATE_UNREADABLE },
	{ "new-folder", AE_HUB_STATE_NEW_FOLDER }, { "create", AE_HUB_STATE_CREATE }, { "play", AE_HUB_STATE_PLAY },
	{ "steam", AE_HUB_STATE_STEAM_PAGE }, { "open-folder", AE_HUB_STATE_OPEN_FOLDER },
	{ "card-workshop", AE_HUB_STATE_CARD_WORKSHOP }, { "card-stock", AE_HUB_STATE_CARD_STOCK },
	{ "show-community", AE_HUB_STATE_SHOW_COMMUNITY }, { "show-ce", AE_HUB_STATE_SHOW_CE }, { NULL, 0 } };
static struct stage const required_stages[] = { { "list", AE_HUB_STATE_MISSING }, { "missing", AE_HUB_STATE_MISSING },
	{ "found", AE_HUB_STATE_FOUND }, { NULL, 0 } };
static struct stage const browse_stages[] = { { "consent", AE_HUB_STATE_CONSENT }, { "results", AE_HUB_STATE_RESULTS },
	{ "list", AE_HUB_STATE_RESULTS }, { "download", AE_HUB_STATE_DOWNLOAD }, { "offline", AE_HUB_STATE_OFFLINE },
	{ "disk-full", AE_HUB_STATE_DISK_FULL }, { "bad-list", AE_HUB_STATE_BAD_LIST }, { "failed", AE_HUB_STATE_FAILED },
	{ "again", AE_HUB_STATE_AGAIN }, { NULL, 0 } };
static struct stage const workshop_stages[] = { { "list", AE_HUB_STATE_LIST }, { "signed-out", AE_HUB_STATE_SIGNED_OUT },
	{ "collections-show", AE_HUB_STATE_COLL_SHOW }, { "collections", AE_HUB_STATE_COLL_LIST },
	{ "collection", AE_HUB_STATE_COLL_OPEN }, { "collection-map", AE_HUB_STATE_COLL_MAP },
	{ "collection-getting", AE_HUB_STATE_COLL_GETTING }, { "collection-installed", AE_HUB_STATE_COLL_INSTALLED },
	{ "collection-get-confirm", AE_HUB_STATE_COLL_GET_CONFIRM },
	{ "collection-remove-confirm", AE_HUB_STATE_COLL_REMOVE_CONFIRM }, { "collection-no-room", AE_HUB_STATE_COLL_NO_ROOM },
	{ NULL, 0 } };
static struct stage const mods_stages[] = { { "list", AE_HUB_STATE_LIST }, { "moving", AE_HUB_STATE_MOVING },
	{ "restart", AE_HUB_STATE_RESTART }, { NULL, 0 } };
/* (ITEM SETS: the state is the scene, ae_item_sets_rules.h's AE_IS_SCENE_* in this order) */
static struct stage const item_sets_stages[] = { { "list", 0 }, { "empty", 1 }, { "options", 2 }, { "delete", 3 }, { "pick", 4 },
	{ "cant-open", 5 }, { "kinds", 6 }, { "items", 7 }, { "adding", 8 }, { "added", 9 }, { "refused", 10 }, { "contents", 11 },
	{ "remove", 12 }, { "failed", 13 }, { "biped", 14 }, { "switched", 15 }, { "new", 16 }, { "real-kinds", 17 }, { "real-items", 18 },
	{ "real-add", 19 }, { "settings", 20 }, { "body", 21 }, { "body-nofit", 22 }, { "body-card", 23 },
	{ "swaps", 24 }, { "swaps-empty", 25 }, { "swap-weapons", 26 }, { "swap-enemies", 27 }, { "swap-ranks", 28 }, { "swap-slot", 29 },
	{ "swap-delete", 30 }, { "swap-new", 31 }, { "range", 32 }, { "range-wait", 33 }, { "picker", 34 }, { "picker-waits", 35 }, { "home", 36 }, { NULL, 0 } };

static struct stage const *stages_of(int tab)
{
	switch (tab)
	{
	case AE_HUB_TAB_INSTALLED: return installed_stages;
	case AE_HUB_TAB_REQUIRED: return required_stages;
	case AE_HUB_TAB_BROWSE: return browse_stages;
	case AE_HUB_TAB_WORKSHOP: return workshop_stages;
	case AE_HUB_TAB_MODS: return mods_stages;
	case AE_HUB_TAB_ITEM_SETS: return item_sets_stages;
	}
	return NULL;
}

int ae_hub_stage_find(int tab, const char *word, int *state)
{
	struct stage const *stage = stages_of(tab);

	if (!stage || !word)
		return 0;
	for (; stage->word; stage++)
		if (same(word, stage->word))
		{
			*state = stage->state;
			return 1;
		}
	return 0;
}

const char *ae_hub_stage_names(int tab)
{
	switch (tab)
	{
	case AE_HUB_TAB_INSTALLED: return "list|options|folder|delete|in-use|loading|empty|unreadable|new-folder|create|play|steam|open-folder|card-workshop|card-stock|show-community|show-ce";
	case AE_HUB_TAB_REQUIRED: return "missing|found";
	case AE_HUB_TAB_BROWSE: return "consent|results|download|offline|disk-full|bad-list|failed|again";
	case AE_HUB_TAB_WORKSHOP: return "list|signed-out|collections-show|collections|collection|collection-map|collection-getting|"
		"collection-installed|collection-get-confirm|collection-remove-confirm|collection-no-room";
	case AE_HUB_TAB_MODS: return "list|moving|restart";
	case AE_HUB_TAB_ITEM_SETS: return "list|empty|options|delete|pick|cant-open|kinds|items|adding|added|refused|contents|remove|failed|biped|switched|new|real-kinds|real-items|real-add|settings|body|body-nofit|body-card|swaps|swaps-empty|swap-weapons|swap-enemies|swap-ranks|swap-slot|swap-delete|swap-new|range|range-wait|picker|picker-waits|home";
	}
	return "";
}

const char *ae_hub_source_name(int source)
{
	return source >= 0 && source < AE_HUB_SOURCES ? source_names[source] : "";
}

const char *ae_hub_show_name(int show)
{
	return show >= 0 && show < AE_HUB_SHOWS ? show_names[show] : "";
}

static int has_suffix(char const *text, char const *suffix)
{
	size_t length = text ? strlen(text) : 0, tail = strlen(suffix);

	return length >= tail && !strcmp(text + length - tail, suffix);
}

const char *ae_hub_map_source_name(int source, char const *list_name)
{
	if (source == AE_HUB_SOURCE_MAP_PACK && (has_suffix(list_name, "@smp") || has_suffix(list_name, "@pro")))
		return "CONVERTED";
	return ae_hub_source_name(source);
}

void ae_hub_folder_label(char const *folder_path, char *label, int size)
{
	char const *start = NULL, *at;
	int length = 0;

	if (size <= 0)
		return;
	label[0] = 0;
	if (!folder_path)
		return;
	/* (the last segment that is a maps folder: maps, maps_ce, maps_smp, custom_maps) */
	for (at = folder_path; *at; at++)
	{
		if ((at == folder_path || at[-1] == '/' || at[-1] == '\\') &&
			(!strncmp(at, "maps", 4) || !strncmp(at, "custom_maps", 11)))
			start = at;
	}
	for (at = start; at && *at && length < size - 4; at++)
	{
		if (*at == '/' || *at == '\\')
		{
			if (!at[1])
				break;
			length += snprintf(label + length, (size_t)(size - length), " \xE2\x80\xBA ");
		}
		else
			label[length++] = *at;
		label[length] = 0;
	}
}

int ae_hub_show_has(int show, int source)
{
	switch (show)
	{
	case AE_HUB_SHOW_ALL: return 1;
	case AE_HUB_SHOW_CUSTOM_EDITION: return source == AE_HUB_SOURCE_CUSTOM_EDITION || source == AE_HUB_SOURCE_ARCHIVE;
	case AE_HUB_SHOW_COMMUNITY: return source == AE_HUB_SOURCE_XBOX;
	case AE_HUB_SHOW_MCC: return source == AE_HUB_SOURCE_MCC;
	case AE_HUB_SHOW_WORKSHOP: return source == AE_HUB_SOURCE_WORKSHOP_STEAM || source == AE_HUB_SOURCE_WORKSHOP_INSTALLED;
	case AE_HUB_SHOW_MAP_PACKS: return source == AE_HUB_SOURCE_MAP_PACK || source == AE_HUB_SOURCE_DISC;
	case AE_HUB_SHOW_STOCK: return source == AE_HUB_SOURCE_STOCK;
	case AE_HUB_SHOW_HOSTS: return source == AE_HUB_SOURCE_HOST;
	}
	return 0;
}

/* only files this game or its player put there may be deleted; the maps folders' own may also be moved between folders */
int ae_hub_source_may_delete(int source)
{
	return source == AE_HUB_SOURCE_CUSTOM_EDITION || source == AE_HUB_SOURCE_ARCHIVE || source == AE_HUB_SOURCE_HOST ||
		source == AE_HUB_SOURCE_WORKSHOP_INSTALLED;
}

int ae_hub_source_may_move(int source)
{
	return source == AE_HUB_SOURCE_CUSTOM_EDITION || source == AE_HUB_SOURCE_ARCHIVE || source == AE_HUB_SOURCE_HOST;
}

const char *ae_hub_source_reason(int source)
{
	switch (source)
	{
	case AE_HUB_SOURCE_STOCK: return "STOCK MAP";
	case AE_HUB_SOURCE_MCC: return "MCC'S FILE";
	case AE_HUB_SOURCE_WORKSHOP_STEAM: return "STEAM KEEPS IT";
	case AE_HUB_SOURCE_DISC: return "PART OF A DISC";
	case AE_HUB_SOURCE_XBOX: return "YOUR OWN FILE";
	case AE_HUB_SOURCE_MAP_PACK: return "PART OF A PACK";
	case AE_HUB_SOURCE_WORKSHOP_INSTALLED: return "NOT MOVABLE";
	}
	return NULL;
}

const char *ae_hub_map_delete_reason(struct ae_hub_map_flags const *map)
{
	if (map->in_use)
		return "IN USE";
	if (!ae_hub_source_may_delete(map->source))
		return ae_hub_source_reason(map->source);
	/* (a Workshop folder the downloader did not record, such as one copied in by hand, is left alone) */
	if (map->source == AE_HUB_SOURCE_WORKSHOP_INSTALLED && !map->downloaded)
		return map->client_open ? "NOT DOWNLOADED BY AE" : "OPEN STEAM ACCOUNT FIRST";
	return NULL;
}

int ae_hub_map_options(struct ae_hub_map_flags const *map, struct ae_hub_option options[AE_HUB_OPTIONS])
{
	int count = 4;

	memset(options, 0, sizeof(struct ae_hub_option) * AE_HUB_OPTIONS);
	options[0].option = AE_HUB_OPTION_PLAY;
	options[0].label = map->kind_campaign ? "PLAY IT" : "CREATE A GAME ON IT";
	options[0].reason = map->unreadable ? "CAN'T READ" : map->host_reason;
	options[1].option = AE_HUB_OPTION_MOVE;
	options[1].label = "MOVE TO A FOLDER";
	options[1].reason = map->in_use ? "IN USE" : !ae_hub_source_may_move(map->source) ? ae_hub_source_reason(map->source) : NULL;
	options[2].option = AE_HUB_OPTION_OPEN_FOLDER;
	options[2].label = "OPEN ITS FOLDER";
	options[2].reason = map->source == AE_HUB_SOURCE_STOCK ? "NO FOLDER" : NULL;
	options[3].option = AE_HUB_OPTION_DELETE;
	options[3].label = "DELETE";
	options[3].reason = ae_hub_map_delete_reason(map);
	if (map->source == AE_HUB_SOURCE_WORKSHOP_STEAM || map->source == AE_HUB_SOURCE_WORKSHOP_INSTALLED)
	{
		options[4].option = AE_HUB_OPTION_STEAM_PAGE;
		options[4].label = "OPEN ON STEAM";
		options[4].reason = NULL;
		count = 5;
	}
	return count;
}

static void hint(struct ae_hub_hint *hints, int *count, int what, char const *label)
{
	if (*count < AE_HUB_HINTS_MOST)
	{
		hints[*count].hint = what;
		hints[*count].label = label;
		(*count)++;
	}
}

int ae_hub_hints(int tab, int state, int keyboard, struct ae_hub_hint hints[AE_HUB_HINTS_MOST])
{
	int count = 0;

	(void)keyboard;
	memset(hints, 0, sizeof(struct ae_hub_hint) * AE_HUB_HINTS_MOST);
	switch (tab)
	{
	case AE_HUB_TAB_INSTALLED:
		if (state == AE_HUB_STATE_LOADING)
		{
			hint(hints, &count, AE_HUB_HINT_SEARCH, "Search");
			hint(hints, &count, AE_HUB_HINT_BACK, "Back");
			break;
		}
		if (state == AE_HUB_STATE_EMPTY)
		{
			hint(hints, &count, AE_HUB_HINT_SELECT, "Select");
			hint(hints, &count, AE_HUB_HINT_FOLDER, "Open folder");
			hint(hints, &count, AE_HUB_HINT_BACK, "Back");
			break;
		}
		hint(hints, &count, AE_HUB_HINT_OPTIONS, "Options");
		hint(hints, &count, AE_HUB_HINT_SEARCH, "Search");
		hint(hints, &count, AE_HUB_HINT_FOLDER, "Open folder");
		hint(hints, &count, AE_HUB_HINT_PAGE, "Page");
		hint(hints, &count, AE_HUB_HINT_BACK, "Back");
		break;
	case AE_HUB_TAB_REQUIRED:
		hint(hints, &count, AE_HUB_HINT_GET, state == AE_HUB_STATE_MISSING ? "Get it" : "Select");
		hint(hints, &count, AE_HUB_HINT_AGAIN, "Look again");
		hint(hints, &count, AE_HUB_HINT_FOLDER, "Open folder");
		hint(hints, &count, AE_HUB_HINT_BACK, "Back");
		break;
	case AE_HUB_TAB_BROWSE:
		if (state == AE_HUB_STATE_CONSENT)
		{
			hint(hints, &count, AE_HUB_HINT_SELECT, "Select");
			hint(hints, &count, AE_HUB_HINT_BACK, "Not now");
			break;
		}
		if (state == AE_HUB_STATE_OFFLINE || state == AE_HUB_STATE_BAD_LIST)
		{
			hint(hints, &count, AE_HUB_HINT_TRY_AGAIN, "Try again");
			hint(hints, &count, AE_HUB_HINT_BACK, "Back");
			break;
		}
		hint(hints, &count, state == AE_HUB_STATE_DOWNLOAD ? AE_HUB_HINT_CANCEL : AE_HUB_HINT_INSTALL,
			state == AE_HUB_STATE_DOWNLOAD ? "Cancel" : state == AE_HUB_STATE_DISK_FULL || state == AE_HUB_STATE_FAILED ?
			"Try again" : "Install");
		hint(hints, &count, AE_HUB_HINT_SEARCH, "Search");
		hint(hints, &count, AE_HUB_HINT_PAGE, "Page");
		hint(hints, &count, AE_HUB_HINT_BACK, "Back");
		break;
	case AE_HUB_TAB_WORKSHOP:
		hint(hints, &count, AE_HUB_HINT_SELECT, state == AE_HUB_STATE_SIGNED_OUT ? "Select" : "Install");
		if (state != AE_HUB_STATE_SIGNED_OUT)
		{
			hint(hints, &count, AE_HUB_HINT_SEARCH, "Search");
			hint(hints, &count, AE_HUB_HINT_PAGE, "Page");
		}
		hint(hints, &count, AE_HUB_HINT_BACK, "Back");
		break;
	case AE_HUB_TAB_MODS:
		if (state == AE_HUB_STATE_MOVING)
		{
			hint(hints, &count, AE_HUB_HINT_PUT_DOWN, "Put down");
			hint(hints, &count, AE_HUB_HINT_BACK, "Put it back");
			break;
		}
		if (state == AE_HUB_STATE_RESTART)
		{
			hint(hints, &count, AE_HUB_HINT_RESTART, "Restart now");
			hint(hints, &count, AE_HUB_HINT_LATER, "At the next start");
			break;
		}
		hint(hints, &count, AE_HUB_HINT_ON_OFF, "On / off");
		hint(hints, &count, AE_HUB_HINT_MOVE, "Move");
		hint(hints, &count, AE_HUB_HINT_AGAIN, "Find mods again");
		hint(hints, &count, AE_HUB_HINT_BACK, "Back");
		break;
	case AE_HUB_TAB_ITEM_SETS:
		/* (the page's own screen draws its footer: ae_item_sets_rules.c) */
		hint(hints, &count, AE_HUB_HINT_SELECT, "Open");
		hint(hints, &count, AE_HUB_HINT_BACK, "Back");
		break;
	}
	return count;
}

int ae_hub_list_rows(float scale)
{
	/* the list's room: from the controls' end to the "n more" line, in the frame's 1080 units; a row and its gap are 56 */
	float top, rows;

	if (scale < 0.5f)
		scale = 0.5f;
	top = 252.0f + 254.0f * scale;
	rows = (880.0f - top) / (56.0f * scale);
	return rows < 1.0f ? 1 : (int)rows;
}

int ae_hub_folder_url(char const *path, char *url, int size)
{
	static char const unreserved[] = "-._~/:";
	int length = 0, at;

	if (!path || !path[0] || size < 16)
		return 0;
	length = snprintf(url, (size_t)size, "file://%s", path[0] == '/' ? "" : "/");
	for (at = 0; path[at]; at++)
	{
		unsigned char c = (unsigned char)path[at];

		if (length >= size - 4)
			return 0;
		if (c == '\\')
			url[length++] = '/';
		else if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || strchr(unreserved, c))
			url[length++] = (char)c;
		else
			length += snprintf(url + length, (size_t)(size - length), "%%%02X", c);
	}
	url[length] = 0;
	return 1;
}

void ae_hub_path_folder(char const *path, char *folder, int size)
{
	char const *last = NULL, *each;
	size_t length;

	folder[0] = 0;
	for (each = path; each && *each; each++)
		if (*each == '/' || *each == '\\')
			last = each;
	if (!last || size < 1)
		return;
	length = (size_t)(last - path);
	if (length >= (size_t)size)
		length = (size_t)size - 1;
	memcpy(folder, path, length);
	folder[length] = 0;
}

void ae_hub_workshop_url(unsigned long long item, char *url, int size)
{
	snprintf(url, (size_t)size, "https://steamcommunity.com/sharedfiles/filedetails/?id=%llu", item);
}

void ae_hub_size_text(unsigned long long bytes, char *text, int size)
{
	if (bytes >= 1024ull * 1024ull * 1024ull)
	{
		unsigned long long tenths = (bytes * 10ull + 512ull * 1024ull * 1024ull) / (1024ull * 1024ull * 1024ull);

		snprintf(text, (size_t)size, "%llu.%llu GB", tenths / 10ull, tenths % 10ull);
	}
	else if (bytes >= 1024ull * 1024ull)
		snprintf(text, (size_t)size, "%llu MB", (bytes + 512ull * 1024ull) / (1024ull * 1024ull));
	else
		snprintf(text, (size_t)size, "%llu KB", (bytes + 512ull) / 1024ull);
}

void ae_hub_count_text(unsigned long long count, char *text, int size)
{
	char digits[32];
	int length, at, out = 0;

	snprintf(digits, sizeof(digits), "%llu", count);
	length = (int)strlen(digits);
	for (at = 0; at < length && out + 2 < size; at++)
	{
		if (at && (length - at) % 3 == 0)
			text[out++] = ',';
		text[out++] = digits[at];
	}
	text[out] = 0;
}

void ae_hub_badge_text(int downloads, char *text, int size)
{
	if (downloads > 0)
		snprintf(text, (size_t)size, "DOWNLOADING %d", downloads);
	else if (size > 0)
		text[0] = 0;
}
