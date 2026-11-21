/*
AE_MARK_PLACES.C

MARKS: what a spot is called (ae_mark_places.h). The id crosses the network, and names the same place on every machine,
whatever each one's SHOTGUN AS POWER says of the item table.
*/

#include "cseries.h"
#include "math/real_math.h"
#include "game/item_timers.h"

#include "ae_mark_places.h"
#include "ae_mark_rules.h"

enum { MARK_PLACE_POINTS = 2 + 64 };

int ae_mark_place_at(real_point3d const *point, wchar_t *name, int size)
{
	struct ae_mark_place_point points[MARK_PLACE_POINTS];
	real_point3d bases[2];
	int count = 0;
	short index;
	int id;

	if (item_timers_bases(bases))
	{
		for (index = 0; index < 2; index++)
		{
			points[count].id = index ? AE_MARK_PLACE_BLUE_BASE : AE_MARK_PLACE_RED_BASE;
			points[count].x = bases[index].x;
			points[count].y = bases[index].y;
			points[count].z = bases[index].z;
			count++;
		}
	}
	for (index = 0; index < item_timers_count() && count < MARK_PLACE_POINTS; index++)
	{
		struct item_timer const *timer = item_timers_get(index);

		if (timer->timer_class >= NUMBER_OF_ITEM_TIMER_POWER_CLASSES)
			continue;
		points[count].id = ae_mark_place_item_id(timer->timer_class, timer->side_prefix ? timer->side : 0);
		points[count].x = timer->position.x;
		points[count].y = timer->position.y;
		points[count].z = timer->position.z;
		count++;
	}
	id = ae_mark_place_pick(points, count, point->x, point->y, point->z);
	ae_mark_place_name(id, name, size);
	return id;
}
