/*
AE_DOWNLOADED_MAPS.H

map_families.c's downloaded maps (their scripts are held to tighter rules): the names kept in the table of
ae_downloaded_maps_rules.h, read from the list maps_ce/downloaded_maps.txt the downloaders append to. The list is read in
pieces whatever its size, and read again when it grows while the game runs (a download thread of another unit writes
its line before the map has its final name), so a map just installed is a downloaded map from its first load.
*/

#ifndef __AE_DOWNLOADED_MAPS_H
#define __AE_DOWNLOADED_MAPS_H

#include "ae_downloaded_maps_rules.h"

/* the list of downloaded maps appended to by every writer (port/linux/src/ae_marks_file.h) */
int ae_marks_append(char const *game_path, char const *name, char *error, int error_size);

/* the list at path (a game path) read into the table: its names added */
void ae_downloaded_maps_read_file(char const *path);
/* a name added (logged when the table is full: every map is then played by the downloaded maps' rules) */
void ae_downloaded_maps_mark(char const *map_name);
/* whether map_name is in the table; when it is not, the list is looked at once more (it is read again if it grew) */
int ae_downloaded_maps_known(char const *map_name, char const *path);

/* the scripts' check (hs.c): whether the loaded map, asked for by requested, is a downloaded map by that name or by the file
the game opened for it */
int ae_downloaded_maps_script_check(char const *requested);

#endif
