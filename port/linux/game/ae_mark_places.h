/*
AE_MARK_PLACES.H

MARKS: what a spot is called, from the map's own points (item_timers.c): RED BASE, ROCKETS; a map without useful points has
the distance alone. Include after math/real_math.h.
*/

#ifndef __AE_MARK_PLACES_H
#define __AE_MARK_PLACES_H

#include <wchar.h>

/* a place's id, with its name in name, or 0 and an empty name: the distance alone */
int ae_mark_place_at(real_point3d const *point, wchar_t *name, int size);

#endif
