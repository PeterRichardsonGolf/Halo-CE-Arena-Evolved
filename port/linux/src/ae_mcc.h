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

/* game.mcc_use's text as its value (anything else is ask) */
int ae_mcc_use_from_text(const char *text);
/* whether a path is one the settings can keep: not empty, under 256
characters, no control characters */
int ae_mcc_path_storable(const char *path);

/* ---------- the game's use of it (ae_mcc_platform.c) */

/* the validated MCC folder, or NULL (detected the first time asked, then
kept for the process; roadmap 61's later features read MCC's other files
from here) */
const char *ae_mcc_root(void);
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
Steam." when Steam has an incomplete MCC, else "" */
const char *ae_mcc_refusal_note(void);
/* the question's answers: USE MCC FILES (game.mcc_use "yes", game.mcc_path
the folder), NEVER ASK AGAIN ("no"); NOT NOW changes nothing */
void ae_mcc_answer_use(void);
void ae_mcc_answer_never(void);
void ae_mcc_answer_not_now(void);
/* Settings' MCC FILES row: game.mcc_use shown as ON ("yes") or OFF ("no";
"ask" shows as OFF, and stays ask unless the row is changed) */
void ae_mcc_menu_text(const char *name, char *text, size_t size, int default_value);
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
