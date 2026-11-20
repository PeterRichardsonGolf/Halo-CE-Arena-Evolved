/*
AE_MAPS_HUB_DATA.H

What MAPS & MODS' INSTALLED and REQUIRED FILES pages read (ae_maps_hub_data.c): one list of every map this PC has and
the files the maps need. The map library supplies these when this build has it; DEV MODE's `menu maps` pages draw the
made-up data below, with no files read and nothing sent.
*/

#ifndef AE_MAPS_HUB_DATA_H
#define AE_MAPS_HUB_DATA_H

#include "ae_maps_hub_rules.h"

enum { AE_HUB_MAPS_MOST = 64, AE_HUB_FILES_MOST = 8 };

struct ae_hub_map
{
	char name[64];
	char file[64];
	int source;
	/* the folder under the maps folder it sits in ("" none) */
	char folder[48];
	unsigned long long size;
	int in_use;
	int kind_campaign;
	int unreadable;
	/* the game types it supports, when it is a multiplayer map ("Slayer, CTF"), else "" */
	char plays[64];
	/* where and when it came ("With the game", "From a host, 3 days ago") */
	char added[64];
	char description[160];
	/* the map library's index of it (-1: a staged map) */
	int library_index;
	/* its picture as an ae_draw image and its size (0: none) */
	int picture, picture_width, picture_height;
	/* the folder its file sits in, for the file manager */
	char path[512];
	/* the name the game's map lists know it by, what it serves (AE_MAP_SERVES_*: 1 campaign, 2 multiplayer, 4 menu), and its Steam
	Workshop item (0: none) */
	char list_name[160];
	int serves;
	unsigned long long workshop_item;
	/* a Workshop map the game's own downloader recorded (its folder may be removed by this game) */
	int downloaded;
	/* the Steam client was open when `downloaded` was read (closed: it is 0 because nothing is loaded) */
	int client_open;
};

struct ae_hub_maps
{
	struct ae_hub_map maps[AE_HUB_MAPS_MOST];
	int count;
	/* what the count line says: the maps and their bytes on this PC (the made-up list holds fewer than it claims) */
	unsigned long long total_count, total_bytes;
	int loading;
};

enum { AE_HUB_FILE_FOUND, AE_HUB_FILE_MISSING, AE_HUB_FILE_ABSENT, AE_HUB_FILE_COUNT_ONLY };

struct ae_hub_file
{
	char name[40];
	/* AE_HUB_FILE_*; ABSENT is a file nothing needs here (muted); COUNT_ONLY a row that only shows its value */
	int state;
	/* the value shown instead of FOUND / MISSING ("2 FOUND", "STEAM"), "" none */
	char value[24];
	/* a group header the row opens ("CUSTOM EDITION MAPS" ...): rows with a header text start a group */
	char group[40];
	char group_value[40];
	char from[64];
	unsigned long long size;
	char unlocks[240];
	/* what A does: 1 STEAM ACCOUNT, 2 the mods page, 0 nothing */
	int door;
};

struct ae_hub_files
{
	struct ae_hub_file files[AE_HUB_FILES_MOST];
	int count;
	int missing;
};

/* whether this build has the map library: the real pages read it; 0 draws their quiet "not in this build" line */
int ae_hub_data_library(void);
/* the real lists: the map library's (ae_map_library.h). update is called each frame the page is shown */
void ae_hub_data_update(void);
int ae_hub_data_scanning(void);
int ae_hub_data_ready(void);
/* the maps a SHOW value and a search keep, sorted (sort: 0 name, 1 size, 2 source): indexes of the library; how many */
int ae_hub_data_query(int show, char const *text, int sort, int *indexes, int maximum);
unsigned long long ae_hub_data_total_count(void);
unsigned long long ae_hub_data_total_bytes(void);
/* how many of the maps are Workshop maps (Steam's folder and the game's own downloads) */
int ae_hub_data_workshop_count(void);
/* a library map as the page draws it (1; 0 past the list) */
int ae_hub_data_fill(int index, struct ae_hub_map *map, int with_picture);
/* a delete or a move (folder "" the top): 1 done; else 0 and the refusal's sentence */
int ae_hub_data_delete(int index, char *reason, int size);
/* the delete of a Workshop map the game's downloader put in its folder: handed to the client, which removes it on its own
thread (1 started; else 0 and the refusal's sentence). The poll: 0 while it runs, else 1 done and *removed 1 or 0 failed; the
lists are read again when it is done */
int ae_hub_data_delete_downloaded(int index, char *reason, int size);
int ae_hub_data_delete_poll(int *removed);
/* how many maps a delete of this one takes with it (a Workshop item's folder holds all of its maps; 1 for any other map) */
int ae_hub_data_item_maps(int index);
int ae_hub_data_move(int index, char const *folder, char *reason, int size);
/* whether a folder's name is one a map may be moved to (a name the maps lists show as a page) */
int ae_hub_data_folder_name_ok(char const *name);
/* a library generation: it changes whenever the list the scan holds is replaced or a map is changed, so views kept from it are asked again */
unsigned long ae_hub_data_generation(void);
/* the folders the map could move to, with how many maps each holds: how many listed */
int ae_hub_data_folders(int index, char names[][48], int *counts, int maximum);
/* the required files, with the modded discs found */
int ae_hub_data_read_files(struct ae_hub_files *files, int discs);

/* made-up lists for DEV MODE: with the case's state (AE_HUB_STATE_*) */
void ae_hub_data_stage_maps(struct ae_hub_maps *maps, int state);
void ae_hub_data_stage_files(struct ae_hub_files *files, int state);

#endif
