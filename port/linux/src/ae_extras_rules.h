/*
AE_EXTRAS_RULES.H

Where the macOS download keeps the game's own folders (packs/, voices/, sounds/) and which of their files are copied
into the data root (ae_extras.c). The pure part: no files, no SDL, no game; port/linux/tests/ae_extras_rules_test.c.
*/

#ifndef __AE_EXTRAS_RULES_H
#define __AE_EXTRAS_RULES_H

#include <stddef.h>

/* the folders of a release the game reads from its data root */
#define AE_EXTRAS_FOLDER_COUNT 3
extern const char *const ae_extras_folders[AE_EXTRAS_FOLDER_COUNT];

/* the application's two places for them, from the executable's path
(<where>/Name.app/Contents/MacOS/halo): resources = <where>/Name.app/Contents/Resources, beside = <where>, the
folder the download was unpacked into. 1 when the path is an application's, else 0 (both empty) */
int ae_extras_app_places(const char *executable, char *resources, size_t resources_size, char *beside,
	size_t beside_size);

/* whether the application runs from macOS's temporary read-only copy (Gatekeeper's app translocation, for an
application opened where the download put it): nothing beside it can be read */
int ae_extras_translocated(const char *executable);

/* whether a folder's name in the player's Downloads folder looks like an unpacked release ("arena-evolved-0.7.5-beta-
macos", "Arena Evolved", "ArenaEvolved 2"): case ignored, the words may be joined by '-', '_', ' ' or nothing */
int ae_extras_download_name_matches(const char *name);

/* the xattr line a player runs on the unpacked folder to lift the quarantine mark (the text, for the log) */
#define AE_EXTRAS_QUARANTINE_HINT "xattr -dr com.apple.quarantine <the unpacked folder>"

/* whether an entry's name is copied: not empty, not starting with a dot (.DS_Store), no path separator */
int ae_extras_name_allowed(const char *name);

/* whether a file is copied: when there is none in the data root, or the one there is older than the download's.
Times in seconds. A newer file in the data root is the player's, and stays. */
int ae_extras_should_copy(int destination_exists, unsigned long source_seconds, unsigned long destination_seconds);

#endif
