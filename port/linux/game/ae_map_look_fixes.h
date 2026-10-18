/*
AE_MAP_LOOK_FIXES.H

Look fixes (ae_map_look_fixes.c): tag fields that are wrong in a map's own
data, so that the stock engine draws the map wrongly too (h2d_coagulation's
teleporter: an opaque black card), changed in the map's loaded tags, and tag
references pointed at another of the map's tags (h2d_salicreek's overshield
drawn with the camouflage's model); and the
rasterizer's "incompatible bitmap type" error said once for each bitmap and
type until the next map, not at every draw. The table and its rules:
ae_map_look_fixes_rules.h. Include after cseries.h.
*/

#ifndef __AE_MAP_LOOK_FIXES_H
#define __AE_MAP_LOOK_FIXES_H

/* (cache_files.c, a map's tags loaded and checked) the table's fixes for the map named map_name (its header's
name) made, one line each in the log; the bitmap type errors said are forgotten */
void ae_map_look_fixes_tags_loaded(char const *map_name);

/* (rasterizer_xbox.c, rasterizer_set_texture and its non-blocking twin) whether the "incompatible bitmap type"
error for this bitmap and expected type is to be said: the first time since the map's tags loaded */
boolean ae_map_look_bitmap_type_error_first(void const *bitmap, short type);

#endif
