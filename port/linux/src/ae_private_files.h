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
/* a file written whole (the folder made if it is not there): 1, else 0 (the old one as it was) */
int ae_private_file_write(const char *name, const char *text, long length);
/* a file kept aside as <name>.bad: 1 when moved */
int ae_private_file_set_aside(const char *name);

#endif
