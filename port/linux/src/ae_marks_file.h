/*
AE_MARKS_FILE.H

The list of downloaded maps the game reads (maps_ce/downloaded_maps.txt: map_families.c, ae_downloaded_maps.h), written by
every writer through one function, under one lock: a name already in the list is not written again, the list is written
whole to a file beside it and put in its place in one step (flushed to disk first), a last line a torn write left is ended
before the name, and a list that the name would make longer than the game's table can take is refused. Plain
types only: the game's units and the platform's both call it.
*/

#ifndef AE_MARKS_FILE_H
#define AE_MARKS_FILE_H

/* name (a map's name, then "@ce"; at most 63 characters, no line feed) added to the list at path (a host path): 1 when it
is written and flushed; else 0 and why in error (may be NULL) */
int ae_marks_append_host(char const *path, char const *name, char *error, int error_size);
/* ... the list at a game path (d:\maps_ce\downloaded_maps.txt), its folder made when it is not there */
int ae_marks_append(char const *game_path, char const *name, char *error, int error_size);

#endif
