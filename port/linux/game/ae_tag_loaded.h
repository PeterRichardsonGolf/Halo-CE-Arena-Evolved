/*
AE_TAG_LOADED.H

(cache_files.c's tag_loaded, AE hooks; 0.9.4 perf audit, owner 2026-11-16) tag_loaded through the hashed index of
ae_tag_name_index.c, not a walk of every tag comparing names: the menus asked it per widget per event each frame,
8 % of the game's CPU in the box's benchmark profile.
*/

#ifndef __AE_TAG_LOADED_H
#define __AE_TAG_LOADED_H

struct cache_file_tag_instance;

/* the tags' table was loaded, unloaded or replaced (the menus' tags added): the index is built again when next asked */
void ae_tag_table_changed(void);
/* how many times the tags' table has changed: what is kept by tag index is of another table once it moves */
unsigned long ae_tag_table_generation(void);

/* TRUE with *result tag_loaded's answer (a tag index or NONE); FALSE when the index cannot answer (more tags than it
holds), and tag_loaded walks the tags as before */
unsigned char ae_tag_loaded_indexed(long group_tag, char const *name, struct cache_file_tag_instance const *instances,
	long count, unsigned long checksum, long *result);

#endif /* __AE_TAG_LOADED_H */
