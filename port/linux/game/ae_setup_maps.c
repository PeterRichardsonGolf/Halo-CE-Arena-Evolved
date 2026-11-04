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

		count = (short)(count + (ae_setup_map_in(map, group) && ae_setup_map_suits(map, filter)));
	}
	return count;
}

int ae_setup_map_group_parent(struct ae_setup_maps const *maps, int group)
{
	if (!maps || group < 0 || group >= maps->group_count || group >= AE_SETUP_MAP_GROUPS)
		return -1;
	return maps->group_parent[group] > 0 && maps->group_parent[group] <= maps->group_count ?
		maps->group_parent[group] - 1 : -1;
}

int ae_setup_map_group_open(struct ae_setup_maps const *maps, int open_group, int group)
{
	return open_group >= 0 && group >= 0 && (open_group == group || ae_setup_map_group_parent(maps, open_group) == group);
}

static int has_children(struct ae_setup_maps const *maps, int group)
{
	int child;

	for (child = 0; child < maps->group_count; child++)
	{
		if (ae_setup_map_group_parent(maps, child) == group)
			return 1;
	}
	return 0;
}

/* a group's row if it is listed, then, open, its maps (or its empty line) */
static short group_rows(struct ae_setup_maps const *maps, int filter, int group, int open, short *rows, short written,
	short maximum)
{
	short index;

	if (written >= maximum ||
		(!ae_setup_map_group_count(maps, group, filter) && !(maps->group_flags[group] & AE_SETUP_GROUP_LISTED)))
		return written;
	rows[written++] = (short)(-1 - group);
	if (!open || written >= maximum)
		return written;
	/* (a folder of the player's with nothing in it: its line) */
	if (!ae_setup_map_group_count(maps, group, AE_MF_ALL))
	{
		rows[written++] = AE_SETUP_ROW_EMPTY;
		return written;
	}
	for (index = 0; index < maps->count && written < maximum; index++)
	{
		struct ae_setup_map const *map = &maps->maps[index];

		if (ae_setup_map_in(map, group) && ae_setup_map_suits(map, filter))
			rows[written++] = index;
	}
	return written;
}

short ae_setup_map_rows(struct ae_setup_maps const *maps, int filter, int open_group, short recent, short *rows,
	short maximum)
{
	short written = 0, index, before;
	int group, child;

	if (!maps || !rows || maximum <= 0)
		return 0;
	/* RECENT: open unless folded (its own flag, whatever folder is open) */
	if (recent > 0)
	{
		rows[written++] = AE_SETUP_ROW_RECENT;
		for (index = 0; !maps->recent_folded && index < recent && index < AE_SETUP_ROW_RECENT_MAXIMUM && written < maximum;
			index++)
			rows[written++] = (short)(AE_SETUP_ROW_RECENT_GAME - index);
	}
	for (group = 0; group < maps->group_count && written < maximum; group++)
	{
		int parent = has_children(maps, group);

		/* (a group in a folder: under its parent's row) */
		if (ae_setup_map_group_parent(maps, group) >= 0)
			continue;
		before = written;
		written = group_rows(maps, filter, group, !parent && group == open_group, rows, written, maximum);
		if (!parent || written == before || !ae_setup_map_group_open(maps, open_group, group))
			continue;
		for (child = 0; child < maps->group_count && written < maximum; child++)
		{
			if (ae_setup_map_group_parent(maps, child) == group)
				written = group_rows(maps, filter, child, child == open_group, rows, written, maximum);
		}
	}
	return written;
}

static char upper(char c)
{
	return c >= 'a' && c <= 'z' ? (char)(c - 32) : c;
}

int ae_setup_map_match(const char *text, const char *query)
{
	int at, index;

	if (!text || !query || !query[0])
		return -1;
	for (at = 0; text[at]; at++)
	{
		for (index = 0; query[index] && text[at + index] && upper(text[at + index]) == upper(query[index]); index++)
			;
		if (!query[index])
			return at;
	}
	return -1;
}

int ae_setup_map_matches(struct ae_setup_maps const *maps, short index, int filter, const char *query)
{
	struct ae_setup_map const *map;

	if (!maps || index < 0 || index >= maps->count || !ae_setup_map_suits(&maps->maps[index], filter))
		return 0;
	if (!query || !query[0])
		return 1;
	map = &maps->maps[index];
	return ae_setup_map_match(map->name, query) >= 0 || ae_setup_map_match(map->credits, query) >= 0 ||
		(map->source >= 0 && map->source < maps->group_count &&
		ae_setup_map_match(maps->group_names[map->source], query) >= 0);
}

short ae_setup_map_matching(struct ae_setup_maps const *maps, int filter, const char *query)
{
	short index, count = 0;

	for (index = 0; maps && index < maps->count; index++)
		count = (short)(count + ae_setup_map_matches(maps, index, filter, query));
	return count;
}

short ae_setup_map_search(struct ae_setup_maps const *maps, int filter, const char *query, short *rows, short maximum)
{
	short written = 0, index;
	int group;

	if (!maps || !rows || maximum <= 0)
		return 0;
	/* (by source, in the groups' order; a map of no source last) */
	for (group = 0; group <= maps->group_count; group++)
	{
		for (index = 0; index < maps->count && written < maximum; index++)
		{
			short from = maps->maps[index].source;

			if ((from < 0 || from >= maps->group_count ? maps->group_count : from) != group)
				continue;
			if (ae_setup_map_matches(maps, index, filter, query))
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
