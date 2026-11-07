/*
AE_MCC_WORKSHOP.H

MCC Workshop items, read in place (the launcher plan, phase A). A Halo CE
Workshop item is a folder, <library>/steamapps/workshop/content/976730/<id>/,
holding a ModInfo.json (its title, engine and multiplayer maps' json paths)
and, per map, a json whose ScenarioFile names the .map file in the item's
maps folder. Two readers parse that text; the scan walks Steam's libraries,
checks each map file's header and says per map: playable or hidden, and why.
It only opens files to read them. Both texts are untrusted: a huge, truncated
or garbled one is refused with a reason, never a crash.

Pure C over server_json. Unit tested in port/linux/tests/ae_mcc_workshop_test.c
with synthetic text.
*/

#ifndef __AE_MCC_WORKSHOP_H
#define __AE_MCC_WORKSHOP_H

#include <stddef.h>
#include <stdio.h>

#include "ae_mcc.h"

/* larger ModInfo/map json: refused */
#define AE_MCC_WORKSHOP_MAXIMUM_JSON (256 * 1024)
#define AE_MCC_WORKSHOP_MAXIMUM_ITEM_MAPS 256
/* the deepest the json nests */
#define AE_MCC_WORKSHOP_MAXIMUM_DEPTH 16

struct ae_mcc_workshop_item
{
	/* SteamWorkshopId as decimal text; "" when absent */
	char id[24];
	/* Title.Neutral, UTF-8 ("" when absent) */
	char title[128];
	/* Engine == "Halo1" */
	int halo1;
	int map_count;
	/* item-relative, '/' separators */
	char map_json[AE_MCC_WORKSHOP_MAXIMUM_ITEM_MAPS][128];
};

struct ae_mcc_workshop_map_info
{
	/* ScenarioFile's last part: the .map file's name without extension */
	char name[64];
	/* Title.Neutral ("" when absent) */
	char title[128];
	/* Images.Thumbnail and Images.Large: item-relative, '/' separators; "" when absent or not a plain relative path
	(the map still reads) */
	char thumbnail[128];
	char large[128];
};

/* a ModInfo.json's text (length bytes, not NUL terminated is fine). A
MultiplayerMaps path that is absolute, holds ".." or a control character, or
is too long refuses the whole item. 1 when read; 0 with a one-line reason */
int ae_mcc_workshop_read_item(const char *text, size_t length, struct ae_mcc_workshop_item *item,
	char *reason, size_t reason_size);
/* a map json's text. 1 when read; 0 with a one-line reason (no ScenarioFile,
an empty or too long name, a name with a control character) */
int ae_mcc_workshop_read_map_info(const char *text, size_t length, struct ae_mcc_workshop_map_info *info,
	char *reason, size_t reason_size);

enum ae_mcc_workshop_status
{
	AE_MCC_WORKSHOP_PLAYABLE,
	AE_MCC_WORKSHOP_HIDDEN
};

struct ae_mcc_workshop_map
{
	char item_id[24];
	char item_title[128];
	char name[64];
	char title[128];
	/* the .map file, host path */
	char path[AE_MCC_PATH_SIZE];
	int status;
	/* HIDDEN's, shown to the player later */
	char reason[128];
	/* (a PLAYABLE map's pictures, host paths found case-blind in its item; "" for none) */
	char thumbnail[AE_MCC_PATH_SIZE];
	char large[AE_MCC_PATH_SIZE];
};
/* (a HIDDEN map "being updated by Steam" keeps its .map file's path: ae_mcc_workshop_recheck) */

/* the MCC v13 map header: 1 when whole; 0 with reason "not an MCC Halo CE map" or "being updated by Steam" (the
file is shorter than its header's size) */
int ae_mcc_workshop_map_file_valid(const char *path, char *reason, size_t reason_size);
/* every Halo1 item's maps under each library's steamapps/workshop/content/976730; how many were written
(<= maximum); *hidden counts the HIDDEN ones. Items of other engines and campaign-only items are skipped, not
counted. A map name that is "." or ".." or holds ':' '/' '\\' never reaches a path: it is one HIDDEN entry */
int ae_mcc_workshop_scan(const char *const *library_paths, int library_count,
	struct ae_mcc_workshop_map *maps, int maximum, int *hidden);

/* the item folders (any engine) under each library's steamapps/workshop/content/976730, as their names (the
item ids), sorted, each once; how many were written (<= maximum). A rescan compares them with the last full scan's
to see items added or removed */
int ae_mcc_workshop_item_ids(const char *const *library_paths, int library_count, char (*ids)[24], int maximum);
/* whether two such lists hold the same ids (their texts compared, not what lies past each one's end) */
int ae_mcc_workshop_same_items(char (*a)[24], int a_count, char (*b)[24], int b_count);
/* the maps HIDDEN "being updated by Steam" checked again (ae_mcc_workshop_map_file_valid): a whole one is PLAYABLE
now, one still short keeps its reason, one otherwise wrong takes its new reason. How many became PLAYABLE */
int ae_mcc_workshop_recheck(struct ae_mcc_workshop_map *maps, int count);

/* ---------- MCC's stock multiplayer maps in an install (ae_mcc_stock_map_name: the 19 names) */

enum
{
	AE_MCC_STOCK_PLAYABLE,
	/* no such file (or not a stock name, or no install): not listed */
	AE_MCC_STOCK_MISSING,
	/* a file that cannot be played, with reason ("being updated by Steam", "not an MCC Halo CE map") */
	AE_MCC_STOCK_HIDDEN
};
/* a stock map's state in the install at install_root: <install_root>/halo1/maps/<name, in lower case>.map */
int ae_mcc_stock_map_state(const char *install_root, const char *name, char *reason, size_t reason_size);

/* ---------- the platform's file access (ae_mcc_workshop.c off Windows, win32_ae_mcc.c on it; no windows.h here) */

/* called per entry of a folder, never for "." or "..": is_folder, or is_regular (a plain file; not a folder or a
device). Returns nonzero to stop */
typedef int (*ae_mcc_workshop_entry_function)(void *context, const char *name, int is_folder, int is_regular);
/* every entry of folder to entry(); 0 if the folder cannot be opened */
int ae_mcc_workshop_list_folder(const char *folder, ae_mcc_workshop_entry_function entry, void *context);
/* a file opened to read ("rb"); NULL if it cannot be, or is not a seekable file (a pipe) */
FILE *ae_mcc_workshop_open(const char *path);
/* the open file's size in bytes (64-bit), -1 if unknown; leaves the position at the end */
long long ae_mcc_workshop_file_size(FILE *file);

#endif
