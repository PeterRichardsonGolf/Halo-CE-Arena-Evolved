/*
AE_MAP_LOOK_FIXES.H

Look fixes (ae_map_look_fixes.c): tag fields that are wrong in a map's own
data, so that the stock engine draws the map wrongly too (h2d_coagulation's
teleporter: an opaque black card), changed in the map's loaded tags, and tag
references pointed at another of the map's tags (h2d_salicreek's overshield
drawn with the camouflage's model); and the
rasterizer's "incompatible bitmap type" error said once for each bitmap and
type until the next map, not at every draw; and shaders of a map's models
not drawn (Fallout's spawn rings); and a map's BSP's cluster portals that are
wrong in its file (cyberdyneb2's: a room drawn grey from the alley beside it),
fixed in the loaded BSP. The table and its rules:
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

/* (rasterizer_xbox_models.c, a model's transparent part submitted) whether the shader (its tag data) is one of the
table's hidden shaders, not drawn: Fallout's spawn rings */
boolean ae_map_look_shader_hidden(void const *shader);

/* (ae_layout.c's ae_layout_apply, a map loaded: its BSP is, and the map's file and hash prefix known) the portal
table's fixes for the map file map_file (its hash prefix map_hash) made in the BSP loaded, one line each in the log.
Every machine makes them: they change what is drawn from where (and which clusters an object near the portal is in,
as the stock engine has them for a portal that is right) */
void ae_map_look_fixes_map_loaded(char const *map_file, unsigned long map_hash);

/* (ae_glue_ce_maps.c, a Custom Edition map's tag read outside its tags) whether the size bytes at the Xbox address
lie in a portal fix's new list: read in place of the map's (ce_map_checks.c's ce_tags_pointer reads anything else
there as zeroes) */
int ae_map_look_fixes_memory(unsigned long address, long size);

#endif
