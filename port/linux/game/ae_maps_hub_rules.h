/*
AE_MAPS_HUB_RULES.H

What MAPS & MODS decides without drawing (ae_maps_hub_rules.c; ae_screen_maps_hub.c draws it): the six pages and how
LB / RB step through them, the sources a map comes from and what may be done to each, the options list of an
installed map, the footer's prompts of each page and state, how many list rows fit, the sizes and counts as written,
and the pages and states DEV MODE's `menu maps` can stage. Plain C, no engine headers, so the unit tests build it alone.
*/

#ifndef AE_MAPS_HUB_RULES_H
#define AE_MAPS_HUB_RULES_H

enum { AE_HUB_TAB_INSTALLED, AE_HUB_TAB_REQUIRED, AE_HUB_TAB_BROWSE, AE_HUB_TAB_WORKSHOP, AE_HUB_TAB_MODS, AE_HUB_TAB_ITEM_SETS,
	AE_HUB_TABS };

/* what a page shows besides its plain list */
enum
{
	AE_HUB_STATE_LIST,
	AE_HUB_STATE_LOADING,
	AE_HUB_STATE_EMPTY,
	AE_HUB_STATE_OPTIONS,
	AE_HUB_STATE_FOLDER,
	AE_HUB_STATE_DELETE,
	AE_HUB_STATE_IN_USE,
	AE_HUB_STATE_UNREADABLE,
	AE_HUB_STATE_MISSING,
	AE_HUB_STATE_FOUND,
	AE_HUB_STATE_CONSENT,
	AE_HUB_STATE_RESULTS,
	AE_HUB_STATE_DOWNLOAD,
	AE_HUB_STATE_OFFLINE,
	AE_HUB_STATE_DISK_FULL,
	AE_HUB_STATE_BAD_LIST,
	AE_HUB_STATE_FAILED,
	AE_HUB_STATE_SIGNED_OUT,
	AE_HUB_STATE_MOVING,
	AE_HUB_STATE_RESTART,
	/* (staged only) the install-again question over a map this PC has; the folder name being typed; CREATE GAME and CAMPAIGN opened
	on a map; a Workshop map's options; the focused map's folder opened */
	AE_HUB_STATE_AGAIN,
	AE_HUB_STATE_NEW_FOLDER,
	AE_HUB_STATE_CREATE,
	AE_HUB_STATE_PLAY,
	AE_HUB_STATE_STEAM_PAGE,
	AE_HUB_STATE_OPEN_FOLDER,
	/* (staged, WORKSHOP: SHOW: COLLECTIONS) in the order of ae_collections_data.h's AE_COLL_SCENE_* from 1: the list with SHOW
	focused, the list, a collection opened, a map of it focused, being got, all here, the get question, the remove question,
	no room */
	AE_HUB_STATE_COLL_SHOW,
	AE_HUB_STATE_COLL_LIST,
	AE_HUB_STATE_COLL_OPEN,
	AE_HUB_STATE_COLL_MAP,
	AE_HUB_STATE_COLL_GETTING,
	AE_HUB_STATE_COLL_INSTALLED,
	AE_HUB_STATE_COLL_GET_CONFIRM,
	AE_HUB_STATE_COLL_REMOVE_CONFIRM,
	AE_HUB_STATE_COLL_NO_ROOM,
	/* (staged, INSTALLED) the list with a Workshop map's card, or a stock map's, on show: each with a made-up picture */
	AE_HUB_STATE_CARD_WORKSHOP,
	AE_HUB_STATE_CARD_STOCK,
	/* (staged, INSTALLED) the list with SHOW on COMMUNITY, or on CUSTOM EDITION */
	AE_HUB_STATE_SHOW_COMMUNITY,
	AE_HUB_STATE_SHOW_CE,
	AE_HUB_STATES
};

/* where a map file came from */
enum
{
	AE_HUB_SOURCE_CUSTOM_EDITION,
	AE_HUB_SOURCE_MCC,
	AE_HUB_SOURCE_WORKSHOP_STEAM,
	AE_HUB_SOURCE_WORKSHOP_INSTALLED,
	AE_HUB_SOURCE_MAP_PACK,
	AE_HUB_SOURCE_STOCK,
	AE_HUB_SOURCE_HOST,
	AE_HUB_SOURCE_ARCHIVE,
	AE_HUB_SOURCE_DISC,
	AE_HUB_SOURCE_XBOX,
	AE_HUB_SOURCES
};

/* the SHOW row's values of INSTALLED, in order */
enum { AE_HUB_SHOW_ALL, AE_HUB_SHOW_CUSTOM_EDITION, AE_HUB_SHOW_COMMUNITY, AE_HUB_SHOW_MCC, AE_HUB_SHOW_WORKSHOP, AE_HUB_SHOW_MAP_PACKS,
	AE_HUB_SHOW_STOCK, AE_HUB_SHOW_HOSTS, AE_HUB_SHOWS };

/* the options list's rows */
enum { AE_HUB_OPTION_PLAY, AE_HUB_OPTION_MOVE, AE_HUB_OPTION_OPEN_FOLDER, AE_HUB_OPTION_DELETE, AE_HUB_OPTION_STEAM_PAGE,
	AE_HUB_OPTIONS };

/* the prompts a footer can hold */
enum { AE_HUB_HINT_NONE, AE_HUB_HINT_SELECT, AE_HUB_HINT_OPTIONS, AE_HUB_HINT_SEARCH, AE_HUB_HINT_FOLDER,
	AE_HUB_HINT_PAGE, AE_HUB_HINT_BACK, AE_HUB_HINT_GET, AE_HUB_HINT_AGAIN, AE_HUB_HINT_INSTALL, AE_HUB_HINT_PICTURE,
	AE_HUB_HINT_CANCEL, AE_HUB_HINT_ON_OFF, AE_HUB_HINT_MOVE, AE_HUB_HINT_PUT_DOWN, AE_HUB_HINT_RESTART,
	AE_HUB_HINT_LATER, AE_HUB_HINT_TRY_AGAIN };
struct ae_hub_hint { int hint; const char *label; };
enum { AE_HUB_HINTS_MOST = 6 };

/* what a map is, for the rules of what may be done with it */
struct ae_hub_map_flags
{
	int source;
	int in_use;
	int kind_campaign;
	int unreadable;
	/* a Workshop map in the game's own folder that the game's downloader recorded (only those may be deleted) */
	int downloaded;
	/* whether the Steam client was open when `downloaded` was read (closed: nothing is loaded, so it says nothing) */
	int client_open;
	/* NULL: a game can be hosted (or a level played) on it; else why not, as the value shown ("MENU MAP") */
	char const *host_reason;
};

struct ae_hub_option
{
	int option;
	const char *label;
	/* NULL: offered; else why not, in the value's place */
	const char *reason;
};

const char *ae_hub_tab_name(int tab);
/* whether a page is in the strip now */
int ae_hub_tab_visible(int tab, int dev_mode);
/* the requested page, or INSTALLED when it is invalid or hidden */
int ae_hub_tab_resolve(int tab, int dev_mode);
/* the page named (installed, required, browse, workshop, mods, sets; any case), or -1 */
int ae_hub_tab_find(const char *word);
/* the visible page LB (-1) or RB (+1) goes to: wraps */
int ae_hub_tab_step(int tab, int direction, int dev_mode);
/* the state named for that page (list, loading, ...), 1 when the page has it: DEV MODE's staging */
int ae_hub_stage_find(int tab, const char *word, int *state);
const char *ae_hub_stage_names(int tab);

const char *ae_hub_source_name(int source);
/* the words of a map's source, the same as CHANGE MAP's chips (ae_map_dupes_rules.h): STOCK, COMMUNITY, WORKSHOP, CONVERTED
(a converted copy, <file>@smp or @pro, whose source is a map pack's), MCC, CE MAP, MAP PACK; list_name may be NULL */
const char *ae_hub_map_source_name(int source, char const *list_name);
/* where a map's file is, for the source line: the folders from the maps folder down, joined with " \xE2\x80\xBA " ("maps
\xE2\x80\xBA H2 Demakes", "maps_ce", "maps \xE2\x80\xBA ce"); "" when the path has no maps folder (a Workshop item's) */
void ae_hub_folder_label(char const *folder_path, char *label, int size);
const char *ae_hub_show_name(int show);
/* whether a map of that source is shown under that SHOW value */
int ae_hub_show_has(int show, int source);
/* what may be done with a map: may_move, may_delete; the reason when not (NULL when both are allowed) */
int ae_hub_source_may_delete(int source);
int ae_hub_source_may_move(int source);
const char *ae_hub_source_reason(int source);

/* why a map can't be deleted now (IN USE, its source's reason, NOT DOWNLOADED BY AE); NULL when it may be */
const char *ae_hub_map_delete_reason(struct ae_hub_map_flags const *map);

/* the options of an installed map, in order; how many (the Workshop page on Steam only for a Workshop map's) */
int ae_hub_map_options(struct ae_hub_map_flags const *map, struct ae_hub_option options[AE_HUB_OPTIONS]);

/* the footer of a page in a state, for a keyboard or a pad; how many */
int ae_hub_hints(int tab, int state, int keyboard, struct ae_hub_hint hints[AE_HUB_HINTS_MOST]);

/* the rows of a list that fit between the controls above it and the help line, at a UI scale (1.0 = 100 %) */
int ae_hub_list_rows(float scale);
/* the rows of a list of a page at 4:3 or 16:9: the same (the layout is in units of the frame's height) */

/* the file:// address of a folder (an absolute path; backslashes as slashes, anything else but a letter, a digit and -._~/: percent-encoded);
1, or 0 when it does not fit */
int ae_hub_folder_url(char const *path, char *url, int size);
/* the folder of a map file's path (its directory, without the last separator); "" when it has none */
void ae_hub_path_folder(char const *path, char *folder, int size);
/* the Steam page of a Workshop item */
void ae_hub_workshop_url(unsigned long long item, char *url, int size);

/* "38 MB", "21.6 GB", "812 KB": a size as written */
void ae_hub_size_text(unsigned long long bytes, char *text, int size);
/* "1,284": a count with thousands separators */
void ae_hub_count_text(unsigned long long count, char *text, int size);
/* "DOWNLOADING 2" for the main menu's row; empty when nothing runs */
void ae_hub_badge_text(int downloads, char *text, int size);

#endif
