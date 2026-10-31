/*
AE_MCC.H

Halo: The Master Chief Collection's Custom Edition resource maps, read in
place (roadmap 60; notes/mcc-install-check-research-2026-10-10.md). A Steam
install of MCC keeps Custom Edition's bitmaps.map, sounds.map and loc.map in
halo1/maps/custom_edition; AE finds the install (game.mcc_path, then
HALO_MCC_PATH, then Steam's libraries) and, with game.mcc_use "yes", reads the
three files from there when the player's own folders (maps_ce, maps/ce,
custom_maps) lack them. AE never writes in the MCC folder.

Two halves: the detection itself (ae_mcc.c: pure C and stdio, unit tested in
port/linux/tests/ae_mcc_test.c over fake Steam trees), and the game's use of it
(ae_mcc_platform.c: the settings, the log, the cache, the folder dialog, the
"mcc:\" paths the engine opens). Plain types only: the game's units call it.
*/

#ifndef __AE_MCC_H
#define __AE_MCC_H

#include <stddef.h>

/* (a path's room: Steam's paths, the files' below them) */
#define AE_MCC_PATH_SIZE 1024
/* the most Steam roots and libraries looked at */
#define AE_MCC_MAXIMUM_ROOTS 8
#define AE_MCC_MAXIMUM_LIBRARIES 16
/* the largest libraryfolders.vdf or appmanifest read (they are a few KB) */
#define AE_MCC_MAXIMUM_VDF_SIZE (1024 * 1024)
/* the deepest a VDF file's blocks go (libraryfolders.vdf's are 3 deep) */
#define AE_MCC_MAXIMUM_VDF_DEPTH 16
/* MCC's Steam app */
#define AE_MCC_STEAM_APP "976730"
#define AE_MCC_DEFAULT_INSTALL_DIR "Halo The Master Chief Collection"

enum ae_mcc_state
{
	/* no MCC anywhere looked */
	AE_MCC_NOT_FOUND,
	/* Steam has MCC (listed, a manifest, or its folder) but not the three
	files whole: an empty shell, an update under way, a removed halo1 */
	AE_MCC_INCOMPLETE,
	/* the three files are there, readable and resource maps */
	AE_MCC_FOUND,
};

enum ae_mcc_use
{
	AE_MCC_USE_ASK,
	AE_MCC_USE_YES,
	AE_MCC_USE_NO,
};

/* where the root came from (the log's word) */
enum ae_mcc_source
{
	AE_MCC_SOURCE_NONE,
	AE_MCC_SOURCE_CONFIG,
	AE_MCC_SOURCE_ENVIRONMENT,
	AE_MCC_SOURCE_STEAM,
	AE_MCC_SOURCE_BROWSE,
};

struct ae_mcc_result
{
	int state;
	int source;
	/* the MCC folder (the one holding halo1/), FOUND's, or INCOMPLETE's when
	one was named */
	char root[AE_MCC_PATH_SIZE];
	/* why not FOUND (or where FOUND came from), for debug.txt */
	char reason[384];
};

/* ---------- the detection (ae_mcc.c, pure) */

/* a resource map's name, by its type: 0 "bitmaps", 1 "sounds", 2 "loc"; NULL past them */
const char *ae_mcc_resource_name(int type);
/* the type of a resource map's name ("bitmaps", "sounds", "loc"; no other, not "ui"), -1 for none */
int ae_mcc_resource_type(const char *name);
/* <root>/halo1/maps/custom_edition/<name>.map */
void ae_mcc_resource_path(const char *root, const char *name, char *path, size_t size);
/* an MCC map's shared resource files of a type (0 bitmaps, 1 sounds, 2 loc) in an MCC install (root), in the order an
MCC map's are looked for (mcc_resources.c): MCC's own halo1/maps, then its custom_edition; how many written */
int ae_mcc_resource_candidate_paths(const char *install_root, int type, char (*paths)[AE_MCC_PATH_SIZE], int maximum);

/* whether the file is a resource map of the type, as ce_resources.c's
ce_resource_map_open checks one (its type in the first word, its resources'
table within the file, at most 0x10000 resources, under 2 GB); 0 with why in
reason if not */
int ae_mcc_resource_file_valid(const char *path, int type, char *reason, size_t reason_size);
/* whether root is an MCC folder whose three files are all valid; 0 with why */
int ae_mcc_root_valid(const char *root, char *reason, size_t reason_size);

/* a VDF (KeyValues) text, Valve's: "key" "value" pairs and "key" { ... }
blocks, // comments, \\ \" \n \t escapes in quoted strings. Each pair and each
block's start is handed to visit with the keys of the blocks it is in
(keys[0] outermost, depth of them); value is NULL for a block's start. visit
returns 0 to stop. The text is untrusted: a string or block left open, a block
deeper than AE_MCC_MAXIMUM_VDF_DEPTH, a token longer than AE_MCC_PATH_SIZE - 1
or a stray character makes it fail (0); 1 when the text was read to its end. */
typedef int (*ae_vdf_visit)(void *context, const char *const *keys, int depth, const char *key, const char *value);
int ae_vdf_parse(const char *text, size_t length, ae_vdf_visit visit, void *context);

/* libraryfolders.vdf's libraries: each "path" (and the older format's bare
"<n>" "<path>"), with whether it lists MCC under "apps"; how many (at most
maximum) */
struct ae_mcc_library
{
	char path[AE_MCC_PATH_SIZE];
	int lists_mcc;
};
int ae_mcc_parse_library_folders(const char *text, size_t length, struct ae_mcc_library *libraries, int maximum);

/* appmanifest_976730.acf's StateFlags (-1 if none) and installdir ("" if none
or not a plain folder name); 0 if it does not read as one */
int ae_mcc_parse_manifest(const char *text, size_t length, long *state_flags, char *install_dir, size_t size);

/* the Linux Steam roots to look in, in order: $XDG_DATA_HOME/Steam,
~/.local/share/Steam, ~/.steam/steam, ~/.steam/root, Flatpak's and Snap's
(either may be NULL or empty: those left out); how many */
int ae_mcc_linux_steam_roots(const char *xdg_data_home, const char *home, char (*roots)[AE_MCC_PATH_SIZE],
	int maximum);

/* what the detection looks at: the folder the settings keep, the
environment's, the Steam roots, and how a folder is told to exist (NULL:
every folder is taken as there, and only the files tell) */
struct ae_mcc_inputs
{
	const char *config_path;
	const char *environment_path;
	const char *const *steam_roots;
	int steam_root_count;
	int (*directory_exists)(const char *path);
};

/* the first of: config_path if valid, environment_path if valid, an
install in a Steam root's libraries (a library listing MCC first; a
manifest's StateFlags must be 4, fully installed; its installdir, else
AE_MCC_DEFAULT_INSTALL_DIR). Returns the state (result filled) */
int ae_mcc_detect(const struct ae_mcc_inputs *inputs, struct ae_mcc_result *result);
/* the one Steam root's libraries looked through (as ae_mcc_detect does each
root); the state, result filled when FOUND or INCOMPLETE */
int ae_mcc_scan_steam_root(const char *steam_root, int (*directory_exists)(const char *path),
	struct ae_mcc_result *result);

/* every Steam library the roots list (libraryfolders.vdf, each root itself too), plus the one holding mcc_root when
that is <library>/steamapps/common/<install dir> (NULL or "" for none), without repeats (ae_mcc_same_folder);
how many paths were written (at most maximum) */
int ae_mcc_libraries(const char *const *steam_roots, int root_count, const char *mcc_root,
	char (*paths)[AE_MCC_PATH_SIZE], int maximum);
/* whether a manifest's StateFlags (Steam's bit field) say MCC's files are
whole: fully installed (4) set, and none of the bits for files missing,
corrupt or changing now (32, 128, an update running, paused or started 256,
512, 1024, uninstalling 2048, validating, downloading, staging, committing).
An update only queued (6) passes; the files' own check is the real gate */
int ae_mcc_state_flags_installed(long state_flags);
/* whether two paths name one folder: exactly; or, on Windows (windows not
0), case-blind, / and \ alike, a trailing separator ignored */
int ae_mcc_same_folder(const char *a, const char *b, int windows);
/* a path for one line of at most `most` characters: as it is when it fits, else its end from a path separator
(/ or \) on, after "..." ("...\common\Halo The Master Chief Collection"); with no separator in that end (a last
folder longer than the line), its last most - 3 characters after "..." */
void ae_mcc_short_path(const char *path, char *out, size_t size, size_t most);
/* the engine's name of one of MCC's files, "mcc:\<name>.map" (either
separator, any case): the resource's type (0 to 2); "mcc:\halo1\<name>.map",
one of MCC's own maps read in place (ae_mcc_boot.c): its main menu, ui, its
stock multiplayer maps (ae_mcc_stock_map_name), and their shared bitmaps.map
and sounds.map (mcc_precache.c stamps them): AE_MCC_VIRTUAL_HALO1;
AE_MCC_VIRTUAL_INVALID for any other "mcc:\..." path (a folder, .., ui.map,
a long name, a campaign level), which opens nothing; AE_MCC_VIRTUAL_NOT for a
path that is not one */
enum { AE_MCC_VIRTUAL_NOT = -2, AE_MCC_VIRTUAL_INVALID = -1, AE_MCC_VIRTUAL_HALO1 = 3, AE_MCC_VIRTUAL_WORKSHOP = 4 };
/* "mcc:\ws\<item id>\<name>.map" (AE_MCC_VIRTUAL_WORKSHOP: an MCC Workshop item's map, read in place where the
Workshop catalog found it; the id 1 to 20 digits, the name a file's, no folder, not starting with a dot): its id
and name (1); 0 for any other path, or parts that do not fit */
int ae_mcc_workshop_virtual_parts(const char *xbox_path, char *id, size_t id_size, char *name, size_t name_size);
/* whether a name (case-blind, no .map) is one of the 19 stock multiplayer maps in MCC's halo1/maps: the Xbox's 13
and Halo PC's 6 */
int ae_mcc_stock_map_name(const char *name);
/* the 19 stock names in order (the Xbox's 13, then Halo PC's 6), lower case: NULL past them */
int ae_mcc_stock_map_count(void);
const char *ae_mcc_stock_map(int index);
int ae_mcc_virtual_path_type(const char *xbox_path);
/* where the main menu's ui.map comes from: the data root's (Xbox) when it has one, as always; else MCC's
halo1/maps/ui.map when MCC was found and game.mcc_use is not "no" (with no Xbox data there is no menu to ask
in: "ask" boots from MCC); else none */
enum { AE_MCC_BOOT_XBOX, AE_MCC_BOOT_MCC, AE_MCC_BOOT_NONE };
int ae_mcc_boot_source(int xbox_ui_present, int mcc_found, int mcc_use);
/* game.mcc_use's text as its value (anything else is ask) */
int ae_mcc_use_from_text(const char *text);
/* whether a path is one the settings can keep: not empty, under 256
characters, no control characters */
int ae_mcc_path_storable(const char *path);

/* ---------- the game's use of it (ae_mcc_platform.c) */

/* the validated MCC folder, or NULL (detected the first time asked, then
kept for the process; roadmap 61's later features read MCC's other files
from here). The game's main thread only: Settings' BROWSE, on that thread,
may change it (another thread copies it: ae_mcc_copy_root) */
const char *ae_mcc_root(void);
/* the same, copied under the detection's lock, for any thread; 0 for none */
int ae_mcc_copy_root(char *root, size_t size);
/* map_family_resource asked only for the player's own folders (the menus'
question, ae_glue_mcc.c): while on, ae_mcc_resource gives nothing and logs
nothing */
void ae_mcc_probe_own_files(int on);
/* the detection's state (detected the first time asked) */
int ae_mcc_state(void);
/* game.mcc_use */
int ae_mcc_use(void);
/* map_family_resource's last place (an AE hook): with game.mcc_use "yes" and
MCC found, a resource map's ("bitmaps", "sounds", "loc"; never "ui") path,
as the engine opens it ("mcc:\<name>.map", which ae_mcc_translate_path
gives the host path of); 0 otherwise. With game.mcc_use "ask" outside the
menus' question (a test host, a joined game) it is not used, and that is
logged once */
int ae_mcc_resource(const char *name, char *path, long size);
/* platform_translate_path's (an AE hook): an "mcc:\<name>.map" path as the
host's file in MCC's folder; 0 for any other path */
int ae_mcc_translate_path(const char *xbox_path, char *host_path, unsigned long size);
/* ce_resources.c's refusal of a missing resource map (an AE hook): ". MCC
found, but its Custom Edition files are missing. Check the install in
Steam." when Steam has an incomplete MCC; ". MCC found: turn on Settings >
MAP FILES to use its files." when MCC has them and game.mcc_use is "ask";
else "" (and with "no", nothing of Steam's is read for it) */
const char *ae_mcc_refusal_note(void);
/* the question's answers: USE MCC FILES (game.mcc_use "yes", game.mcc_path
the folder), NEVER ASK AGAIN ("no"); NOT NOW changes nothing */
void ae_mcc_answer_use(void);
/* the Workshop catalog logged once a process, when MCC is found (sdl_platform.c's start, an AE hook): "mcc workshop:
<items> items, <maps> maps, <hidden> hidden", then "mcc workshop hidden: <item id> <name>: <reason>" per hidden map.
Nothing for NOT_FOUND or INCOMPLETE; nothing on screen */
void ae_mcc_workshop_log(void);
/* the menus' (CHANGE MAP opening, the main thread; after the start's scan): the catalog looked at again cheaply.
When the item folders are the ones the last full scan saw, only the maps HIDDEN "being updated by Steam" are
checked again; when an item was added or removed, all is scanned again. Logged with its milliseconds */
void ae_mcc_workshop_rescan(void);
/* the catalog copied (every map, HIDDEN ones with their reasons too, struct ae_mcc_workshop_map of
ae_mcc_workshop.h), at most maximum; how many. 0 before the start's scan or without MCC */
struct ae_mcc_workshop_map;
int ae_mcc_workshop_catalog_copy(struct ae_mcc_workshop_map *maps, int maximum);
/* a playable map of the catalog that scan found, by its item id and file name (case-blind): its .map file's host
path (1); 0 when the catalog has none (no MCC, game.mcc_use "no", not scanned, or hidden) */
int ae_mcc_workshop_map_path(const char *item_id, const char *name, char *path, size_t size);
void ae_mcc_answer_never(void);
void ae_mcc_answer_not_now(void);
/* Settings' BROWSE: the system's folder dialog (SDL_ShowOpenFolderDialog;
debug: HALO_MCC_BROWSE_RESULT names the folder chosen instead). The folder
chosen is checked and, if it is MCC's, kept in game.mcc_path */
void ae_mcc_browse(void);
/* Settings' status line (UTF-8, upper case as the menus' text is): FOUND:
<folder>, IN USE: <folder>, NOT FOUND, INCOMPLETE..., or the last BROWSE's
result. Polls the dialog's answer */
const char *ae_mcc_status_text(void);
/* the same, as the menus' wide text */
const unsigned short *ae_mcc_status_wide(void);

#endif
