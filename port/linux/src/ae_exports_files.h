/*
AE_EXPORTS_FILES.H

The folder a player's exported files go to and come from: <save root>/exports/ (a profile's progress, ae_glue_profile_move.c)
and <save root>/exports/backup/ (what an import took the place of). Unlike the private folder this one is the player's to
open: it is where a file is copied from one PC to another. A file is read whole, and written whole beside itself as <name>.new,
then put in its place, so a crash leaves the old one or the new one. The paths are SDL's (UTF-8 on Windows). No game dependency.

A name is a bare file name of letters, digits, "." "-" and "_" (80 characters at most); anything else is refused.
*/

#ifndef __AE_EXPORTS_FILES_H
#define __AE_EXPORTS_FILES_H

#include <stddef.h>

/* the folder's path (made when it is not there): 1, else 0 */
int ae_exports_folder(int backup, char *path, size_t size);
/* a file's text into text (size bytes, ended): its length; -1 when there is none, -2 when it is larger than size - 1 bytes */
long ae_exports_read(int backup, const char *name, char *text, long size);
/* a file written whole: 1, else 0 (an old one as it was) */
int ae_exports_write(int backup, const char *name, const char *text, long length);
int ae_exports_exists(int backup, const char *name);
int ae_exports_remove(int backup, const char *name);
/* every file name ending in suffix (".aeprofile"), visited in no order; the count, -1 when the folder cannot be read */
int ae_exports_list(int backup, const char *suffix, void (*visit)(const char *name, void *context), void *context);
/* a file: URL (ae_profile_move_rules.h's ae_pm_file_url) shown by the system: 1 when asked */
int ae_exports_open_url(const char *url);

#endif
