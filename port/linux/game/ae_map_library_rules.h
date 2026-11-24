/*
AE_MAP_LIBRARY_RULES.H

The installed maps library, as rules with no game in them (port/linux/tests/ae_map_library_rules_test.c): one list of
every map file this machine has, by where it came from (stock, Custom Edition, an Xbox map of the player's, a map the
game downloaded, a map pack, MCC, the Workshop); the list searched, filtered and sorted in memory; the files a kind of
map needs (Custom Edition's bitmaps.map, sounds.map and loc.map; MCC's _shared singleplayer and multiplayer files) checked in fake or real folders;
which maps may be deleted or moved, and each refusal's reason as text a screen can show; the scan of the folders in
slices (a step reads at most so many map headers and stops when its time is up), through a folder interface the game
and the tests each give; and the DEV MODE command "maps", parsed, with its one answer line.

A map is known by its name (its file's name without .map) and, for a map pack's, its pack (<map>@<pack>). Paths here are
the host's (the game's d:\ paths turned into real ones by the caller).
*/

#ifndef AE_MAP_LIBRARY_RULES_H
#define AE_MAP_LIBRARY_RULES_H

enum
{
	/* the maps the library holds; more found are counted, not kept */
	AE_MAP_LIBRARY_MAXIMUM = 2048,
	AE_MAP_LIBRARY_NAME_SIZE = 64,
	AE_MAP_LIBRARY_TITLE_SIZE = 64,
	AE_MAP_LIBRARY_PACK_SIZE = 64,
	AE_MAP_LIBRARY_PATH_SIZE = 512,
	/* the folders a scan looks in (each with one level of its folders) */
	AE_MAP_LIBRARY_ROOTS = 64,
	/* a map file's header (the part read of each file) */
	AE_MAP_HEADER_SIZE = 2048,
	/* the maps in use a change is checked against (the game's, the lobby's) */
	AE_MAP_LIBRARY_IN_USE = 8,
	/* the files a required-files check lists at most */
	AE_MAP_REQUIRED_MAXIMUM = 8
};

/* where a map came from */
enum
{
	/* the stock multiplayer and campaign maps of the maps folder */
	AE_MAP_SOURCE_STOCK,
	/* a Custom Edition map (maps_ce, maps\ce and their folders) */
	AE_MAP_SOURCE_CUSTOM_EDITION,
	/* an Xbox map of the player's own in the maps folder or its folders */
	AE_MAP_SOURCE_XBOX_CUSTOM,
	/* a map the game downloaded (from a host or the map download screen) */
	AE_MAP_SOURCE_DOWNLOADED,
	/* a map pack's map (<map>@<pack>) */
	AE_MAP_SOURCE_MAP_PACK,
	/* an MCC install's map */
	AE_MAP_SOURCE_MCC,
	/* a Steam Workshop item's map */
	AE_MAP_SOURCE_WORKSHOP,
	NUMBER_OF_AE_MAP_SOURCES
};

#define AE_MAP_SOURCE_BIT(source) (1u << (source))

/* what a map serves (its scenario's type) */
enum
{
	AE_MAP_SERVES_CAMPAIGN = 1,
	AE_MAP_SERVES_MULTIPLAYER = 2,
	AE_MAP_SERVES_MENU = 4
};

/* the orders a list is sorted in (ties by name, then pack, then path, so the order is the same each time) */
enum
{
	AE_MAP_SORT_NAME,
	AE_MAP_SORT_TITLE,
	AE_MAP_SORT_SOURCE,
	AE_MAP_SORT_SIZE_LARGEST,
	AE_MAP_SORT_SIZE_SMALLEST,
	NUMBER_OF_AE_MAP_SORTS
};

/* a delete's or a move's answer: allowed, or why not */
enum
{
	AE_MAP_CHANGE_ALLOWED,
	AE_MAP_REFUSED_UNKNOWN_MAP,
	AE_MAP_REFUSED_TWO_OF_A_NAME,
	AE_MAP_REFUSED_STOCK,
	AE_MAP_REFUSED_XBOX_CUSTOM,
	AE_MAP_REFUSED_MAP_PACK,
	AE_MAP_REFUSED_MCC,
	AE_MAP_REFUSED_WORKSHOP,
	AE_MAP_REFUSED_IN_USE,
	AE_MAP_REFUSED_OUTSIDE_ROOTS,
	AE_MAP_REFUSED_LINK,
	AE_MAP_REFUSED_NOT_A_FILE,
	AE_MAP_REFUSED_FOLDER_NAME,
	AE_MAP_REFUSED_SAME_FOLDER,
	AE_MAP_REFUSED_TARGET_TAKEN,
	AE_MAP_REFUSED_PATH_TOO_LONG,
	AE_MAP_REFUSED_SCANNING,
	AE_MAP_REFUSED_FAILED,
	NUMBER_OF_AE_MAP_CHANGE_ANSWERS
};

/* the kinds of map a required-files check is for */
enum
{
	AE_MAP_REQUIRED_CUSTOM_EDITION,
	AE_MAP_REQUIRED_MCC,
	NUMBER_OF_AE_MAP_REQUIRED_KINDS
};

/* ---------- a map file */

struct ae_map_header_info
{
	/* the cache's version: 5 Xbox, 7 PC, 609 Custom Edition, 13 MCC */
	int version;
	/* the scenario's type: 0 campaign, 1 multiplayer, 2 the main menu */
	int type;
	char name[32];
};

struct ae_map_entry
{
	char name[AE_MAP_LIBRARY_NAME_SIZE];
	char title[AE_MAP_LIBRARY_TITLE_SIZE];
	/* the map pack's name, the Workshop item's id, or "" */
	char pack[AE_MAP_LIBRARY_PACK_SIZE];
	/* the folder of its root it sits in ("" its root's top) */
	char folder[AE_MAP_LIBRARY_NAME_SIZE];
	char path[AE_MAP_LIBRARY_PATH_SIZE];
	unsigned long long size;
	int source;
	int serves;
	int version;
	/* the root it was found under (the scan's index) */
	int root;
	/* the picture's handle, -1 for none (the caller's to fill) */
	int picture;
};

/* a header read: 1 for a map file of a known version (info filled), 0 for anything else */
int ae_map_library_header_read(unsigned char const *header, unsigned int size, struct ae_map_header_info *info);
/* the AE_MAP_SERVES_* of a scenario type (0 for an unknown one) */
int ae_map_library_serves(int type);
/* whether a name is one of the stock maps (multiplayer or campaign), letters' case aside */
int ae_map_library_stock_name(char const *name);
/* a map's shown name from its file's: the stock maps' own titles, else underscores as spaces, each word capitalised */
void ae_map_library_title(char const *name, char *title, unsigned int size);
/* the source of a map found under a root of the given source (a stock root's maps are stock by name, else the player's
own: Xbox ones, or Custom Edition ones by version) */
int ae_map_library_source_of(int root_source, struct ae_map_header_info const *info);
/* the short names of the sources ("stock", "ce", "xbox", "downloaded", "pack", "mcc", "workshop") */
char const *ae_map_library_source_name(int source);
/* a source by its short name (or a few others: "custom", "packs"), -1 for none */
int ae_map_library_source_by_name(char const *name);

/* ---------- search, filter, sort */

struct ae_map_filter
{
	/* words that must each be in the name, title or pack (letters' case aside); "" matches all */
	char text[AE_MAP_LIBRARY_TITLE_SIZE];
	/* AE_MAP_SOURCE_BIT()s to keep, 0 for all */
	unsigned int sources;
	/* AE_MAP_SERVES_* to keep (any of them), 0 for all */
	int serves;
};

int ae_map_library_matches(struct ae_map_entry const *entry, struct ae_map_filter const *filter);
/* the indexes of the entries the filter keeps, sorted: how many (at most maximum) */
int ae_map_library_select(struct ae_map_entry const *entries, int count, struct ae_map_filter const *filter, int sort,
	int *indexes, int maximum);
/* the index of the entry of a name (and pack, "" for any): -1 none, -2 more than one with it (with no pack named, the
one entry of the name that has no pack, when there is one) */
int ae_map_library_find(struct ae_map_entry const *entries, int count, char const *name, char const *pack);

/* ---------- delete and move */

/* whether the entry's map may be deleted or moved, by its source and the maps in use (the loaded map's name, the
lobby's), before its path is looked at (ae_map_library_files.h's ae_map_files_path_check) */
int ae_map_library_may_change(struct ae_map_entry const *entry, char const *const *in_use, int in_use_count);
/* whether a map's name is one of the maps in use (a lobby or a game on it), compared without case: also for a Workshop map, whose
files are never changed by may_change but whose item is removed whole */
int ae_map_library_name_in_use(char const *name, char const *const *in_use, int in_use_count);
/* whether a path is inside a root (both real, made by realpath): the root itself or under it at a separator */
int ae_map_library_path_inside(char const *path, char const *root);
/* a folder name a map may be moved to: "" (the root's top) or one the map lists show as a page; the name's text
checked as a file's (no separators or reserved characters, no dot or space at its end) */
int ae_map_library_folder_name_ok(char const *folder);
/* the root a map moves within for its version: an Xbox map (5) the maps folder's, the others (Custom Edition, PC, MCC)
the Custom Edition root's (AE_MAP_SOURCE_STOCK or AE_MAP_SOURCE_CUSTOM_EDITION) */
int ae_map_library_move_root_source(int version);
/* <root>/<folder>/<name>.map (folder "" for the root's top): 1 written, 0 too long */
int ae_map_library_move_target(char const *root, char const *folder, char const *name, char *path, unsigned int size);
/* a move's last check, its paths made (both real): AE_MAP_REFUSED_SAME_FOLDER when the target is the map's own path,
AE_MAP_REFUSED_TARGET_TAKEN when something is at the target (taken: 1), else AE_MAP_CHANGE_ALLOWED */
int ae_map_library_move_answer(char const *real, char const *target, int taken);
/* a refusal's text, as a screen shows it */
char const *ae_map_library_refusal_text(int answer);

/* ---------- required files */

struct ae_map_required_file
{
	/* its file's name, what it is, what it unlocks */
	char const *file;
	char const *what;
	char const *unlocks;
	/* where it was found, or where it must go (the first folder given) */
	char where[AE_MAP_LIBRARY_PATH_SIZE];
	int found;
};

typedef int (*ae_map_library_exists)(void *context, char const *path);
/* each file a kind of map needs, looked for in the folders in order (exists says whether a path is a file): how many
listed (at most maximum) */
int ae_map_library_required(int kind, char const *const *folders, int folder_count, ae_map_library_exists exists,
	void *context, struct ae_map_required_file *files, int maximum);

/* ---------- the scan, in slices */

struct ae_map_root
{
	char path[AE_MAP_LIBRARY_PATH_SIZE];
	int source;
	/* the pack's name or Workshop id its maps get ("" none) */
	char pack[AE_MAP_LIBRARY_PACK_SIZE];
	/* 1: its folders are looked in too (page folders, or each a pack named after it: pack_folders) */
	int folders;
	/* 1: each folder of it is a pack (or Workshop item) named after the folder */
	int pack_folders;
};

/* the folders the scan reads through, the game's or the tests' */
struct ae_map_scan_io
{
	void *context;
	/* a folder opened (NULL: none) */
	void *(*open_folder)(void *context, char const *path);
	/* its next entry: 1 (name filled, *is_folder set), 0 at its end */
	int (*next_entry)(void *context, void *folder, char *name, unsigned int size, int *is_folder);
	void (*close_folder)(void *context, void *folder);
	/* a file's first bytes read: how many (header up to size), *file_size its size; -1 when it can't be opened */
	int (*read_header)(void *context, char const *path, unsigned char *header, unsigned int size,
		unsigned long long *file_size);
	/* a clock in microseconds */
	unsigned long long (*now_us)(void *context);
};

struct ae_map_scan
{
	struct ae_map_root roots[AE_MAP_LIBRARY_ROOTS];
	int root_count;
	struct ae_map_entry *entries;
	int maximum;
	int count;
	/* maps found past maximum, files that were not maps, folders that could not be opened */
	int dropped;
	int not_maps;
	int unopened;
	/* where it is: the root, the folder of it being read ("" its top), the folders of the root still to read */
	int root;
	int root_started;
	void *folder;
	char folder_name[AE_MAP_LIBRARY_NAME_SIZE];
	char pending[64][AE_MAP_LIBRARY_NAME_SIZE];
	int pending_count;
	int done;
	/* what it took: steps, headers read, the longest step and all of them (microseconds) */
	int steps;
	int headers;
	unsigned long long longest_step_us;
	unsigned long long total_us;
};

/* a scan begun over the roots, into the entries (none read yet) */
void ae_map_scan_begin(struct ae_map_scan *scan, struct ae_map_root const *roots, int root_count,
	struct ae_map_entry *entries, int maximum);
/* one slice: at most headers map headers read, stopped when budget_us has passed; 1 while there is more to do */
int ae_map_scan_step(struct ae_map_scan *scan, struct ae_map_scan_io const *io, int headers, unsigned long long budget_us);
/* a scan stopped (its open folder closed) */
void ae_map_scan_end(struct ae_map_scan *scan, struct ae_map_scan_io const *io);

/* ---------- DEV MODE "maps" */

enum
{
	AE_MAP_COMMAND_LIST,
	AE_MAP_COMMAND_MISSING,
	AE_MAP_COMMAND_DELETE,
	AE_MAP_COMMAND_MOVE,
	AE_MAP_COMMAND_SCAN,
	NUMBER_OF_AE_MAP_COMMANDS
};

struct ae_map_command
{
	int verb;
	/* list: the search text ("<source>:" first keeps that source); delete / move: the map ([<map>@<pack>]) */
	char text[AE_MAP_LIBRARY_TITLE_SIZE];
	char pack[AE_MAP_LIBRARY_PACK_SIZE];
	/* move: the folder ("" or "." for the root's top; quotes around it taken off) */
	char folder[AE_MAP_LIBRARY_NAME_SIZE];
	/* list: the source kept (-1 all) */
	int source;
};

/* the words after "maps": 1 parsed, 0 not (a usage line is the answer) */
int ae_map_library_command_parse(char const *words, struct ae_map_command *command);
/* list's answer line: "maps: <shown> of <all> (<source> <n>, ...): <name>, <name>, ..." cut at size with "..." */
void ae_map_library_list_line(struct ae_map_entry const *entries, int count, int const *indexes, int shown, char *line,
	unsigned int size);
/* missing's answer line: "required: <kind> <file> ok|MISSING (<where>), ..." */
void ae_map_library_missing_line(int kind, struct ae_map_required_file const *files, int file_count, char *line,
	unsigned int size);

#endif
