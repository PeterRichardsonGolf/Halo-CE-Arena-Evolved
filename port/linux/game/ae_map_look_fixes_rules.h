/*
AE_MAP_LOOK_FIXES_RULES.H

The rules of AE's look fixes (ae_map_look_fixes.h): the table of a map's tag
fields that are wrong in the map's own data, fixed in its loaded tags, and the
set that keeps the rasterizer's "incompatible bitmap type" error to one line
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
	AE_BITMAP_TYPE_ERRORS_KEPT = 256
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
	AE_MAP_LOOK_FIX_OUTSIDE
};

/* the table */
extern struct ae_map_look_fix const ae_map_look_fixes[];
extern unsigned int const ae_map_look_fix_count;

/* whether the fix is for the map named map_name (a header's name: at most AE_MAP_LOOK_FIX_MAP_NAME_SIZE
bytes are read, any case); 0 for no name */
int ae_map_look_fix_for_map(struct ae_map_look_fix const *fix, char const *map_name);

/* the fix applied to a tag's data of size bytes (the field in the machine's byte order, as the game reads it) */
enum ae_map_look_fix_result ae_map_look_fix_apply(struct ae_map_look_fix const *fix, unsigned char *data,
	unsigned int size);

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
