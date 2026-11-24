/*
AE_TAG_NAME_INDEX.H

A hashed index of the loaded tags by group and name, for cache_files.c's tag_loaded (0.9.4 perf audit, owner
2026-11-16: "lets optmize this"). tag_loaded walked every tag comparing names (_stricmp), and the menus asked it up to
three times per widget per event per frame (menu_functions.c's pc_menu_screen_reads_every_controller): 8 % of the
game's CPU in the box's benchmark profile. The index answers as the walk did: the lowest tag index of that group whose
name is the same, letters' case aside (ASCII, as _stricmp), or none.

Pure (no engine includes): the table of tags is read through a getter. It is built again when the table it was made
from, its count or the caller's generation (moved on each load, unload and replacement of the tags' table) differs.
A found tag is checked by name again before it is given; a table too big for the index says so (AE_TAG_NAME_INDEX_
UNUSABLE) and the caller walks the tags as before.
*/

#ifndef __AE_TAG_NAME_INDEX_H
#define __AE_TAG_NAME_INDEX_H

/* the most tags the index holds: a tag's absolute index is 16 bits */
#define AE_TAG_NAME_INDEX_MAXIMUM_TAGS 65536
#define AE_TAG_NAME_INDEX_UNUSABLE (-2L)

/* tag index's group and name (NULL: no name, never found) */
typedef void (*ae_tag_name_index_get)(void *context, long index, long *group, char const **name);

/* the lowest index of a tag of that group and name (NONE, -1, when there is none), or AE_TAG_NAME_INDEX_UNUSABLE.
table and generation say which tags count tags are: a change of either builds the index again */
long ae_tag_name_index_find(long group, char const *name, void const *table, long count, unsigned long generation,
	ae_tag_name_index_get get, void *context);

/* how many times the index was built (the units, the log) */
unsigned long ae_tag_name_index_builds(void);

#endif /* __AE_TAG_NAME_INDEX_H */
