/*
AE_SHIPPED_FILES.H

(menus phase 1) A text file that ships beside the game (matchmaking.txt, CHANGELOG.md): read from the folder of
config.toml, else from the program's own folder (SDL_GetBasePath: a macOS bundle's Resources, an install whose
settings live elsewhere), as master_list.c reads masters.txt. Not on Android and the web (no such folder): none.
*/

#ifndef __AE_SHIPPED_FILES_H
#define __AE_SHIPPED_FILES_H

/* the file's text into text (size bytes, ended): its length, -1 when found nowhere (it may be cut to size - 1) */
long ae_shipped_file_read(const char *name, char *text, long size);

#endif
