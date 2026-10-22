/*
AE_SETUP_MAPS.C

The game setup spike's CHANGE MAP model. See ae_setup_maps.h.
*/

#include <stddef.h>
#include "ae_setup_maps.h"

static const char *const filter_names[AE_MAP_FILTERS] = { "ALL", "AE FFA SLAY", "AE TEAM SLAY", "AE KING",
	"AE FFA BALL", "AE CTF", "NHE" };

const char *ae_setup_map_filter_name(int filter)
{
	return filter >= 0 && filter < AE_MAP_FILTERS ? filter_names[filter] : "";
}

/* the mode bits a filter asks for, 0 for none (ALL, NHE) */
static int filter_mode(int filter)
{
	switch (filter)
	{
	case AE_MF_FFA_SLAYER:
	case AE_MF_TEAM_SLAYER: return AE_SETUP_MODE_SLAYER;
	case AE_MF_KING: return AE_SETUP_MODE_KING;
	case AE_MF_BALL: return AE_SETUP_MODE_ODDBALL;
	case AE_MF_CTF: return AE_SETUP_MODE_CTF;
	}
	return 0;
}

/* by the facts alone: whether a read map plays the filter's mode (a free-for-all slayer: not a large map) */
static int facts_suit(struct ae_setup_map const *map, int filter)
{
	if (!(map->modes & filter_mode(filter)))
		return 0;
	return filter != AE_MF_FFA_SLAYER || map->size != 'l';
}

int ae_setup_map_suits(struct ae_setup_map const *map, int filter)
{
	if (!map || filter < 0 || filter >= AE_MAP_FILTERS)
		return 0;
	if (filter == AE_MF_ALL)
		return 1;
	if (filter == AE_MF_NHE)
		return map->nhe != 0;
	/* (not read, or unreadable: never hidden for want of facts) */
	if (map->facts != AE_SETUP_FACTS_READ)
		return 1;
	return facts_suit(map, filter);
}

short ae_setup_map_suiting(struct ae_setup_maps const *maps, int filter)
{
	short index, count = 0;

	for (index = 0; maps && index < maps->count; index++)
		count = (short)(count + ae_setup_map_suits(&maps->maps[index], filter));
	return count;
}

short ae_setup_map_group_count(struct ae_setup_maps const *maps, int group, int filter)
{
	short index, count = 0;

	if (!maps || group < 0 || group >= maps->group_count)
		return 0;
	for (index = 0; index < maps->count; index++)
	{
		struct ae_setup_map const *map = &maps->maps[index];

		count = (short)(count + ((map->groups & (1u << group)) && ae_setup_map_suits(map, filter)));
	}
	return count;
}

short ae_setup_map_rows(struct ae_setup_maps const *maps, int filter, int open_group, short *rows, short maximum)
{
	short written = 0, index;
	int group;

	if (!maps || !rows || maximum <= 0)
		return 0;
	for (group = 0; group < maps->group_count && written < maximum; group++)
	{
		if (!ae_setup_map_group_count(maps, group, filter))
			continue;
		rows[written++] = (short)(-1 - group);
		if (group != open_group)
			continue;
		for (index = 0; index < maps->count && written < maximum; index++)
		{
			struct ae_setup_map const *map = &maps->maps[index];

			if ((map->groups & (1u << group)) && ae_setup_map_suits(map, filter))
				rows[written++] = index;
		}
	}
	return written;
}

short ae_setup_map_group_step(short const *rows, short count, short index, short direction)
{
	short step, at;

	if (!rows || count <= 0)
		return index;
	for (step = 1; step <= count; step++)
	{
		at = (short)(((index + (direction > 0 ? step : -step)) % count + count) % count);
		if (ae_setup_map_row_is_group(rows[at]))
			return at;
	}
	return index;
}

short ae_setup_map_group_row(short const *rows, short count, int group)
{
	short index;

	for (index = 0; rows && index < count; index++)
	{
		if (rows[index] == -1 - group)
			return index;
	}
	return -1;
}

short ae_setup_map_plays_well(struct ae_setup_map const *map, int *filters, short maximum)
{
	short written = 0;
	int filter;

	if (!map || !filters)
		return 0;
	for (filter = AE_MF_FFA_SLAYER; filter < AE_MAP_FILTERS && written < maximum; filter++)
	{
		if (filter == AE_MF_NHE ? map->nhe != 0 : map->facts == AE_SETUP_FACTS_READ && facts_suit(map, filter))
			filters[written++] = filter;
	}
	return written;
}
