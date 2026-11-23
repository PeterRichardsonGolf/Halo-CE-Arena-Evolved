/*
AE_TAG_IMPORT_SPIKE.H

(spike, DEV only) The tag importer (tag_import.h) run on a level as it
loads, for roots an environment variable names. Off unless
HALO_SPIKE_IMPORT is set.
*/

#ifndef AE_TAG_IMPORT_SPIKE_H
#define AE_TAG_IMPORT_SPIKE_H

/* a level's tags are loaded: the roots copied in from their pack */
void ae_tag_import_spike_tags_loaded(char const *map_name);
/* the level goes: the copies let go, the level's own table back */
void ae_tag_import_spike_tags_unloaded(void);
/* a copied tag's pixels or samples read from its pack: TRUE and the flag set when the tag is a copy */
boolean ae_tag_import_spike_read(long tag_index, long offset, long size, void *buffer, boolean *completion_flag);
#endif /* AE_TAG_IMPORT_SPIKE_H */
