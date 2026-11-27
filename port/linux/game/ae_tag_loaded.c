/*
AE_TAG_LOADED.C

(ae_tag_loaded.h) the loaded tags' table read for ae_tag_name_index.c. The index is keyed by the table's address,
its count and a generation of the table's changes mixed with the map's checksum: an Xbox map's tags sit at the same
address on every map.
*/

#include "cseries.h"
#include "ae_tag_loaded.h"
#include "ae_tag_name_index.h"

/* (cache_files.c's, as ae_menus_swap.c has it) */
struct cache_file_tag_instance
{
	long group_tag;
	long parent_group_tags[2];
	long tag_index;
	XPTR(char) name;
	XPTR(void) base_address;
	unsigned long unused[2];
};

typedef char verify_tag_loaded_instance_size[sizeof(struct cache_file_tag_instance) == 0x20 ? 1 : -1];

static unsigned long table_generation;

void ae_tag_table_changed(void)
{
	table_generation++;
}

unsigned long ae_tag_table_generation(void)
{
	return table_generation;
}

static void instance_get(void *context, long index, long *group, char const **name)
{
	struct cache_file_tag_instance const *instances = context;

	*group = instances[index].group_tag;
	*name = instances[index].name ? (char const *)xbox_pointer(instances[index].name) : NULL;
}

unsigned char ae_tag_loaded_indexed(long group_tag, char const *name, struct cache_file_tag_instance const *instances,
	long count, unsigned long checksum, long *result)
{
	long absolute_index;

	if (!instances)
		return FALSE;
	absolute_index = ae_tag_name_index_find(group_tag, name, instances, count,
		(table_generation * 2654435761UL) ^ checksum, instance_get, (void *)instances);
	/* (a miss is the walk's to decide, a precaution: a hit is read from the live table, so only a miss could come from a
	table changed after the index was built without a word to ae_tag_table_changed (the menus' tags are named once their
	table is set)) */
	if (absolute_index == AE_TAG_NAME_INDEX_UNUSABLE || absolute_index == NONE)
		return FALSE;
	*result = instances[absolute_index].tag_index;
	return TRUE;
}
