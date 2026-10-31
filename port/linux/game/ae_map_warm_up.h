/*
AE_MAP_WARM_UP.H

The WARM-UP group of the PC map list (ui_map_list.c, its AE hook lines):
the small arenas for a warm-up Slayer (octagon and octagon_b, Xbox v5
community maps) come first after the Xbox's maps, each named with [WARM-UP]
as a Custom Edition map is with [CE], and described as one for Slayer.
*/

#ifndef AE_MAP_WARM_UP_H
#define AE_MAP_WARM_UP_H

#include <stddef.h>

/* whether a map (its file's name, any folder or family suffix) is a warm-up map (pure) */
int ae_map_warm_up(const char *map_name);
/* a community map's row text: the name given, or for a warm-up map that name with " [WARM-UP]" (lobby: one line)
or "\r\n[WARM-UP]" (the map list's two lines); and its description. Each a buffer of its own, until the next call
of the same kind */
wchar_t const *ae_map_warm_up_name(const char *map_name, wchar_t const *name, int lobby);
wchar_t const *ae_map_warm_up_description(const char *map_name, wchar_t const *description);
/* the rows from first (count of them, each size bytes, its map name at name_offset): the warm-up maps' moved to the
front, in their order, the others after them in theirs */
void ae_map_warm_up_first(void *first, long count, size_t size, size_t name_offset);

#endif
