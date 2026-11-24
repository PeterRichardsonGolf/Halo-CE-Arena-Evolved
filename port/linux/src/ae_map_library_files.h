/*
AE_MAP_LIBRARY_FILES.H

The installed maps library's host file calls (port/linux/game/ae_map_library.c calls them): real paths, a change's
path checked (a plain file, not a link, inside the real roots), a file deleted, renamed only to a name not taken, a
folder made, a file's size and first bytes, and a clock in microseconds. Paths are the host's. Only int, char and
unsigned long long cross here, so the game's units and the platform layer agree on every call in each build.
*/

#ifndef AE_MAP_LIBRARY_FILES_H
#define AE_MAP_LIBRARY_FILES_H

/* a path's real one (links and dots resolved): 1 written, 0 none or too long */
int ae_map_files_real_path(char const *path, char *real, int size);
/* the real path of a map file, checked: not a link itself, a plain file, and under one of the real roots (made by
ae_map_files_real_path): AE_MAP_CHANGE_ALLOWED and real filled, or the refusal (ae_map_library_rules.h) */
int ae_map_files_path_check(char const *path, char const *const *real_roots, int root_count, char *real, int size);
/* whether a path is a plain file (links followed), and whether anything is at a path (links not followed) */
int ae_map_files_is_file(char const *path);
/* whether a path is itself a link (a symbolic link, or on Windows a junction) */
int ae_map_files_is_link(char const *path);
int ae_map_files_exists(char const *path);
/* a plain file's size: 1 and *size, or 0 */
int ae_map_files_size(char const *path, unsigned long long *size);
/* a file's first bytes: how many read, -1 when it can't be opened */
int ae_map_files_read(char const *path, unsigned char *bytes, int size);
/* a file deleted: 1 done */
int ae_map_files_delete(char const *path);
/* a file renamed to a path where nothing is: 1 done, 0 failed or the name is taken (never replaced) */
int ae_map_files_rename_new(char const *from, char const *to);
/* a folder made (1: made or there already as a folder) */
int ae_map_files_make_folder(char const *path);

/* a folder's entries read (a scan's, on any one thread at a time: two folders open at most): the folder opened, NULL
for none; its next entry (1: name, *is_folder and *file_size filled; 0 at its end; an entry whose name does not fit
in size, or that is gone before it is looked at, is passed over); the folder closed */
void *ae_map_files_folder_open(char const *path);
int ae_map_files_folder_next(void *folder, char *name, int size, int *is_folder, unsigned long long *file_size);
void ae_map_files_folder_close(void *folder);

#endif
