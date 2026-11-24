/*
AE_PRIVATE_FILES.H

AE's small text files in the save root's private folder (port_folders.h: <save root>/private/, what is the player's
alone): the pickers' folders and RECENT (ae_glue_picker_stores.c). A file is read whole, and written whole beside
itself as <name>.new, then put in its place in one step (posix_replace_file), so a crash leaves the old one or the new
one, never half of either. A file the reader refuses is kept aside as <name>.bad (one at a time: an older one goes).
The game's units call these with a file's bare name; the paths are the host's (the C library's own stdio here, not the
game units' Xbox paths). No SDL or game dependency.
*/

#ifndef __AE_PRIVATE_FILES_H
#define __AE_PRIVATE_FILES_H

/* a file's text into text (size bytes, ended): its length; -1 when there is none, 0 when it is empty, -2 when it is
larger than size - 1 bytes (nothing read) */
long ae_private_file_read(const char *name, char *text, long size);
/* a file's text from the save root itself (a player puts it there: bot_names.txt), else from the private folder: as
ae_private_file_read */
long ae_save_root_file_read(const char *name, char *text, long size);
/* a file written whole (the folder made if it is not there): 1, else 0 (the old one as it was) */
int ae_private_file_write(const char *name, const char *text, long length);
/* a file written beside itself as <name>.import and not yet put in its place (a change of several files that is all or none):
1 when staged; the commit puts it in the file's place (1 when done), the discard takes it away (1 when it was there) */
int ae_private_file_stage(const char *name, const char *text, long length);
int ae_private_file_commit(const char *name);
int ae_private_file_discard(const char *name);
/* a file kept aside as <name>.bad: 1 when moved */
int ae_private_file_set_aside(const char *name);
/* whether a file is there; a file taken away (1 when it was) */
int ae_private_file_exists(const char *name);
int ae_private_file_remove(const char *name);

/* The crash mark (0.9.3, AE TOURNAMENT): arm it before a game that must be forgiven only for a real crash is written as pending: a
   crash (SIGSEGV, SIGBUS, SIGILL, SIGFPE, SIGABRT here; the exception filter on Windows, win32_crash.c, calls the touch) then makes
   the private file <name> before the game ends as it would have, nothing but open(2) and close(2) done in the handler. A closed
   window, a killed process or a power loss leaves no mark. Disarm it when no such game is pending. 1 when armed. The mark is read
   with ae_private_file_exists at the next start, and taken away. Android and web cannot mark a crash; supported answers whether
   this platform can distinguish one from an ordinary close */
int ae_private_crash_mark_arm(const char *name);
int ae_private_crash_mark_supported(void);
void ae_private_crash_mark_disarm(void);
void ae_private_crash_mark_touch(void);

/* the settings file (port_config.c): written whole aside, then put in its
place, SDL's (UTF-8 paths on Windows); a refused rename (an editor holding it
on Windows) writes it in place. 1 written */
int ae_config_file_save(const char *path, const char *text);
/* a settings file that is there but was not read at start: logged once,
never written over; 0 */
int ae_config_file_refuse(const char *path);
/* there (a path SDL sees): the settings file not read was not missing */
int ae_config_file_there(const char *path);

#endif
