/*
AE_MAP_LOOK_FIXES_RULES.H

The rules of AE's look fixes (ae_map_look_fixes.h): the tables of a map's tag
fields that are wrong in the map's own data, fixed in its loaded tags (a 16-bit
value changed; a tag reference pointed at another of the map's tags), the cluster
portals a map's BSP has wrong (a portal into a room the room's portals leave
out), and the set that keeps the rasterizer's "incompatible bitmap type" error to one line
for each bitmap and type. Pure (no game types): unit tested in
port/linux/tests/ae_map_look_fixes_rules_test.c.
*/

#ifndef __AE_MAP_LOOK_FIXES_RULES_H
#define __AE_MAP_LOOK_FIXES_RULES_H

#include <stdint.h>

enum
{
	/* the longest map name a fix is matched against (a map header's name: 32 bytes, no terminator needed) */
	AE_MAP_LOOK_FIX_MAP_NAME_SIZE = 32,
	/* the bitmaps and types whose "incompatible bitmap type" error is said, each once, until the next map */
	AE_BITMAP_TYPE_ERRORS_KEPT = 256,
	/* the portals a portal fix's cluster may list */
	AE_MAP_PORTAL_FIX_LIST_SIZE = 8
};

/* one fix: in the map named map_name (its header's name, any case), the tag of group (its four characters,
'schi') named tag_name, the 16-bit field at offset in the tag's data is changed from old_value to new_value */
struct ae_map_look_fix
{
	char const *map_name;
	unsigned long group;
	char const *tag_name;
	unsigned int offset;
	short old_value;
	short new_value;
	/* what the fix does, for the log */
	char const *what;
};

enum ae_map_look_fix_result
{
	/* the field held old_value and now holds new_value */
	AE_MAP_LOOK_FIX_APPLIED,
	/* the field holds new_value already (a map fixed by its author): left as it is */
	AE_MAP_LOOK_FIX_ALREADY,
	/* the field holds neither (another version of the map): left as it is */
	AE_MAP_LOOK_FIX_UNEXPECTED,
	/* the field does not lie in the tag's data, or there is no data: nothing read */
	AE_MAP_LOOK_FIX_OUTSIDE,
	/* (a reference's fix) the tag it expects or the one it gives is not the map's: left as it is */
	AE_MAP_LOOK_FIX_MISSING
};

/* one reference's fix: in the map named map_name, the tag of group named tag_name has, at offset in its data, a
tag reference (16 bytes: the group, the name, the name's length, the tag index) to the tag of target_group named
expected_name; it is pointed at the map's tag of target_group named new_name (its group and index: the name it
was written with is left, as the game reads a reference by its index) */
struct ae_map_look_retarget
{
	char const *map_name;
	unsigned long group;
	char const *tag_name;
	unsigned int offset;
	unsigned long target_group;
	char const *expected_name;
	char const *new_name;
	/* what the fix does, for the log */
	char const *what;
};

/* a shader that is not drawn on a model (a scenery's), on any map that has it: name is the tag's (of group,
'schi') */
struct ae_map_look_hidden_shader
{
	unsigned long group;
	char const *tag_name;
	/* what hiding it does, for the log */
	char const *what;
};

/* one cluster portal's fix (what the game draws from where: structure_visibility.c walks the BSP's clusters by their
portals, each cluster by its own list of them): in the map whose file is map_file (any case) with the hash prefix
map_hash (map_file_hash.h: a layout's, "layout 1 <file> <hash>"), its BSP of cluster_count clusters and
portal_count portals, the portal portal_index joins the clusters old_clusters (as the map has it) and is given
new_clusters, and it is added to the list of the cluster list_cluster, which holds old_list_count portals
(old_list) */
struct ae_map_portal_fix
{
	char const *map_file;
	unsigned long map_hash;
	short cluster_count;
	short portal_count;
	short portal_index;
	short old_clusters[2];
	short new_clusters[2];
	short list_cluster;
	short old_list_count;
	short old_list[AE_MAP_PORTAL_FIX_LIST_SIZE];
	/* what the fix does, for the log */
	char const *what;
};

/* the table */
extern struct ae_map_look_fix const ae_map_look_fixes[];
extern unsigned int const ae_map_look_fix_count;
extern struct ae_map_look_retarget const ae_map_look_retargets[];
extern unsigned int const ae_map_look_retarget_count;
extern struct ae_map_look_hidden_shader const ae_map_look_hidden_shaders[];
extern unsigned int const ae_map_look_hidden_shader_count;
extern struct ae_map_portal_fix const ae_map_portal_fixes[];
extern unsigned int const ae_map_portal_fix_count;

/* whether the fix is for the map named map_name (a header's name: at most AE_MAP_LOOK_FIX_MAP_NAME_SIZE
bytes are read, any case); 0 for no name */
int ae_map_look_fix_for_map(struct ae_map_look_fix const *fix, char const *map_name);

/* the fix applied to a tag's data of size bytes (the field in the machine's byte order, as the game reads it) */
enum ae_map_look_fix_result ae_map_look_fix_apply(struct ae_map_look_fix const *fix, unsigned char *data,
	unsigned int size);

/* whether the reference's fix is for the map named map_name (as ae_map_look_fix_for_map) */
int ae_map_look_retarget_for_map(struct ae_map_look_retarget const *fix, char const *map_name);

/* the reference's fix applied to a tag's data of size bytes, expected_index and new_index being the indices of the
map's tags of the fix's target_group named expected_name and new_name (-1 for none): the reference is changed only
if it is to expected_index, with the target group; to new_index already, it is left */
enum ae_map_look_fix_result ae_map_look_retarget_apply(struct ae_map_look_retarget const *fix, unsigned char *data,
	unsigned int size, int32_t expected_index, int32_t new_index);

/* whether the portal fix is for the map file map_file (any case) of hash prefix map_hash; 0 for no file */
int ae_map_portal_fix_for_map(struct ae_map_portal_fix const *fix, char const *map_file, unsigned long map_hash);

/* the portal fix checked against the BSP: its counts, the portal's two clusters (clusters) and the list of the fix's
cluster (list, list_count portals). APPLIED: they are as the fix expects, clusters is given the new ones and
new_list (room for new_list_size) the cluster's new list (*new_list_count), for the game to put in the BSP; ALREADY:
the portal joins the new clusters and the list has it (fixed already: this load's, or a map its author fixed);
UNEXPECTED: anything else (another version of the map); OUTSIDE: the fix's portal or cluster is not in the BSP, or no
room. Only APPLIED writes clusters and new_list */
enum ae_map_look_fix_result ae_map_portal_fix_apply(struct ae_map_portal_fix const *fix, long cluster_count,
	long portal_count, short clusters[2], short const *list, long list_count, short *new_list, long new_list_size,
	long *new_list_count);

/* the bitmaps and types said: reset when a map's tags load (a bitmap's address is the map's) */
struct ae_bitmap_type_errors
{
	uintptr_t bitmaps[AE_BITMAP_TYPE_ERRORS_KEPT];
	short types[AE_BITMAP_TYPE_ERRORS_KEPT];
	unsigned int count;
};

void ae_bitmap_type_errors_reset(struct ae_bitmap_type_errors *errors);

/* whether the error for this bitmap and expected type is the first since the reset (it is then kept): 0 for one
said already, and for any once AE_BITMAP_TYPE_ERRORS_KEPT have been */
int ae_bitmap_type_errors_first(struct ae_bitmap_type_errors *errors, uintptr_t bitmap, short type);

#endif
