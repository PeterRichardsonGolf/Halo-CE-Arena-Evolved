/*
AE_FORGE_MAPS.H

The maps Forge saved on this PC (Content Evolved's Forge writes them as custom_maps\<name>.map: a Custom Edition cache, or
the stock map's own cache with Forge's trailer, holding the saved state's text and a 140 by 114 picture), for MY MAPS,
CHANGE MAP's FORGE MAPS group and the loader. A map is found by its file, read for its header and its text, and played
under the level name its cache version gives (ae_forge_maps_rules.h). The maps the game downloaded (maps_shared, maps_ce)
that carry Forge's trailer are listed as DOWNLOADED.
*/

#ifndef __AE_FORGE_MAPS_H
#define __AE_FORGE_MAPS_H

#include "ae_forge_maps_rules.h"

enum { AE_FORGE_MAPS_MAX = 256 };

struct ae_forge_map
{
	char file[64];   /* the file's name, without .map */
	char name[64];   /* the name the map shows: its text's "Map Name", else the file's */
	char base[64];   /* the stock map it was made on, upper case, without .map */
	char level[72];  /* the level name it is played as ("" when it is not one this plays) */
	char path[260];  /* d:\...\<file>.map */
	int kind;        /* AE_FORGE_MAP_* */
	int downloaded;  /* in a folder of downloads, not custom_maps */
	int readable;    /* its header and trailer read */
	int objects, squads;
	unsigned long saved; /* the file's last write, seconds since 1970 */
	long size;
	long picture_offset, picture_size;
	int picture;     /* ae_draw's image id once drawn (0: not yet, or none) */
};

/* reads the folders again; the number of saved maps found */
int ae_forge_maps_scan(void);

/* the list as SHOW and SORT make it (ae_forge_maps_rules.h's enums); the number shown, and the maps by place */
int ae_forge_maps_view(int show, int sort);
struct ae_forge_map *ae_forge_maps_at(int place);

/* the map at an index of the last scan's files (not the view's order), or NULL */
struct ae_forge_map *ae_forge_maps_get(int index);

/* the counts of the last scan: all, and downloaded among them */
int ae_forge_maps_total(void);
int ae_forge_maps_downloaded(void);

/* the map's picture as an ae_draw image (0: none), made on the first ask */
int ae_forge_maps_picture(struct ae_forge_map *map);

/* the saved map of a file's name, or -1 */
int ae_forge_maps_find(char const *file);

/* the file of a saved map by its name, as Forge's loader and its saves look: custom_maps\<name>.map, then the older
forge_maps\<name>\<name>.map and Faber_data\forge_maps\<name>\<name>.map; 1 with the path found */
int ae_forge_maps_path(char const *name, char *path, int size);

/* when the saved map of a name was last written (seconds since 1970), 0 when there is none */
unsigned long ae_forge_maps_saved_time(char const *name);

/* the file of a level the game has no map of by that name (maps\<name>.map is not there): the saved map's of that name, for
the loader (cache_files_map_file_path); 1 with the path */
int ae_forge_maps_level_path(char const *level, char *path, int size);

/* cache_files_map_file_path's last word: when no file of the name is where the game looks (path), a saved Forge map's of that
name (an Xbox cache) is the path; a name the game has a map for is never taken over */
void ae_forge_maps_xbox_path(char const *level, char *path, long size);

/* the saved maps that are Xbox caches and readable, for the game's map list (ui_map_list.c): the folders read again, their number; then the
index-th one's level name and the title it shows (upper case) */
int ae_forge_maps_xbox_count(void);
int ae_forge_maps_xbox_entry(int index, char *level, int level_size, char *title, int title_size);

/* whether a level name (as the map list names it) is a saved Forge map's, of either kind, as the last scan found them */
int ae_forge_maps_is_level(char const *level);

/* MY MAPS' RENAME, MAKE A COPY and DELETE on the map of a file: 1 done, else 0 with the reason in text. A name is refused when
a map or a stock map has it, or it is no name Forge's save box makes */
int ae_forge_maps_rename(char const *file, char const *name, char *text, int size);
int ae_forge_maps_copy(char const *file, char const *name, char *text, int size);
int ae_forge_maps_delete(char const *file, char *text, int size);

#endif /* __AE_FORGE_MAPS_H */
