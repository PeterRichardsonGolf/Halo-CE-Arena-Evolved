/*
AE_MAP_LIBRARY.H

The installed maps library (ae_map_library_rules.h has its rules): every map this machine has, by source, read in slices
so a folder of hundreds of maps never holds a frame; searched, filtered and sorted in memory; the required files of
Custom Edition and MCC maps checked; Custom Edition and downloaded maps deleted or moved to a folder, every refusal with
its reason as text. The screens call these each frame they are shown; DEV MODE's "maps" drives the same calls.
*/

#ifndef AE_MAP_LIBRARY_H
#define AE_MAP_LIBRARY_H

#include "ae_map_library_rules.h"

/* a new scan begun (the list kept until it ends; then the new one replaces it) */
void ae_map_library_refresh(void);
/* one slice of the scan (at most 16 map headers, about 2 ms), begun first if the list was never read; call each frame
the library's screen is shown */
void ae_map_library_update(void);
/* the whole scan at once (DEV MODE, and a screen that must have the list now): how long it took, microseconds */
unsigned long long ae_map_library_finish(void);
/* whether a scan is under way, and whether a list was ever read */
int ae_map_library_scanning(void);
int ae_map_library_ready(void);

/* the list: its count, an entry (NULL past it), the entries a filter keeps in an order (ae_map_library_select's) */
int ae_map_library_count(void);
struct ae_map_entry const *ae_map_library_entry(int index);
/* 1 when the entry's file lies at the top of that game folder of the scan ("d:\\custom_maps\\"), not in a folder under it */
int ae_map_library_entry_at_top_of(struct ae_map_entry const *entry, char const *game_folder);
int ae_map_library_query(struct ae_map_filter const *filter, int sort, int *indexes, int maximum);
/* an entry by name ([<map>@<pack>] as the lists name them): its index, -1 none, -2 more than one */
int ae_map_library_find_named(char const *name, char const *pack);
/* the name the game's lists know an entry by ("bloodgulch", "<map>@<pack>", "<map>@mcc", "<map>@ws<item>") */
void ae_map_library_list_name(struct ae_map_entry const *entry, char *name, unsigned int size);
/* an entry's picture as an ae_draw image (0: none), its size in width / height; loaded once, kept */
int ae_map_library_picture(int index, int *width, int *height);

/* the files a kind of map needs (AE_MAP_REQUIRED_*), found or where each must go: how many listed */
int ae_map_library_missing(int kind, struct ae_map_required_file *files, int maximum);

/* an entry's map deleted (its .map and the .bmp and .txt beside it), or moved to a folder ("" its root's top) with
them: AE_MAP_CHANGE_ALLOWED when done, else the refusal (ae_map_library_refusal_text has its text) */
int ae_map_library_delete(int index);
int ae_map_library_move(int index, char const *folder);
/* whether it may be deleted or moved now, the same checks without the change */
int ae_map_library_may_change_now(int index);
/* whether a lobby or a game is on this map now (a Workshop map included, which may_change_now never says in use) */
int ae_map_library_entry_in_use(int index);
/* the library maps of one Workshop item (its whole folder goes when it is removed): how many, and whether any of them is in use
now (a lobby or a game on it) in *in_use (may be NULL) */
int ae_map_library_workshop_item_maps(unsigned long long item, int *in_use);

/* DEV MODE "maps <words>": one answer line; 1 when it did what was asked, 0 for a refusal or a usage line */
int ae_map_library_dev(char const *words, char *line, unsigned int size);

#endif
