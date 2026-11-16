/*
AE_BOTS_LEARNT_RULES.C

What the bots keep of a map from game to game, as pure rules (ae_bots_learnt_rules.h).
*/

#include "ae_bots_learnt_rules.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void clear(struct ae_learnt_map *map, char const *header)
{
	memset(map, 0, sizeof *map);
	if (header)
	{
		strncpy(map->header, header, sizeof map->header - 1);
		map->header[sizeof map->header - 1] = 0;
	}
}

static int usable(float const *point)
{
	int axis;

	for (axis = 0; axis < 3; axis++)
	{
		/* (NaN is not equal to itself; no isfinite in gnu89) */
		if (!(point[axis] == point[axis]) || point[axis] > AE_LEARNT_FARTHEST || point[axis] < -AE_LEARNT_FARTHEST)
			return 0;
	}
	return 1;
}

/* the next line of text into line (without its end), and the text after it */
static char const *next_line(char const *text, char *line, int size)
{
	int length = 0;

	while (*text && *text != '\n')
	{
		if (length < size - 1 && *text != '\r')
			line[length++] = *text;
		text++;
	}
	line[length] = 0;
	return *text ? text + 1 : text;
}

int ae_learnt_read(struct ae_learnt_map *map, char const *text, char const *header)
{
	char line[256];

	clear(map, header);
	if (!text || !header)
		return 0;
	text = next_line(text, line, (int)sizeof line);
	if (strcmp(line, header))
		return 0;
	while (*text)
	{
		long surface;
		int count, uses, misses;
		float a[3], b[3];

		text = next_line(text, line, (int)sizeof line);
		if (sscanf(line, "wedge %ld %d", &surface, &count) == 2)
		{
			if (map->wedge_count < AE_LEARNT_WEDGES && surface >= 0 && count > 0)
			{
				map->wedges[map->wedge_count].surface = surface;
				map->wedges[map->wedge_count].count = count > AE_LEARNT_WEDGE_MOST ? AE_LEARNT_WEDGE_MOST : count;
				map->wedge_count++;
			}
		}
		else if (sscanf(line, "stuck %ld %d %f %f %f", &surface, &count, &a[0], &a[1], &a[2]) == 5)
		{
			if (map->stuck_count < AE_LEARNT_STUCKS && surface >= 0 && count > 0 && usable(a))
			{
				struct ae_learnt_stuck *stuck = &map->stucks[map->stuck_count++];

				stuck->surface = surface;
				stuck->count = count > AE_LEARNT_STUCK_MOST ? AE_LEARNT_STUCK_MOST : count;
				stuck->x = a[0];
				stuck->y = a[1];
				stuck->z = a[2];
			}
		}
		else if (sscanf(line, "jump %f %f %f %f %f %f %d %d", &a[0], &a[1], &a[2], &b[0], &b[1], &b[2], &uses, &misses) == 8)
		{
			if (map->jump_count < AE_LEARNT_JUMPS && uses > 0 && misses >= 0 && misses < AE_LEARNT_JUMP_MISSES && usable(a) &&
				usable(b))
			{
				struct ae_learnt_jump *jump = &map->jumps[map->jump_count++];

				memcpy(jump->from, a, sizeof a);
				memcpy(jump->to, b, sizeof b);
				jump->uses = uses > AE_LEARNT_JUMP_USES ? AE_LEARNT_JUMP_USES : uses;
				jump->misses = misses;
			}
		}
	}
	return 1;
}

int ae_learnt_write(struct ae_learnt_map const *map, char *text, int size)
{
	int used, index, wrote;

	if (size <= 0)
		return 0;
	used = snprintf(text, (size_t)size, "%s\n", map->header);
	if (used < 0 || used >= size)
		return 0;
	for (index = 0; index < map->wedge_count; index++)
	{
		wrote = snprintf(text + used, (size_t)(size - used), "wedge %ld %d\n", map->wedges[index].surface,
			map->wedges[index].count);
		if (wrote < 0 || used + wrote >= size)
			return 0;
		used += wrote;
	}
	for (index = 0; index < map->stuck_count; index++)
	{
		struct ae_learnt_stuck const *stuck = &map->stucks[index];

		wrote = snprintf(text + used, (size_t)(size - used), "stuck %ld %d %.2f %.2f %.2f\n", stuck->surface, stuck->count,
			(double)stuck->x, (double)stuck->y, (double)stuck->z);
		if (wrote < 0 || used + wrote >= size)
			return 0;
		used += wrote;
	}
	for (index = 0; index < map->jump_count; index++)
	{
		struct ae_learnt_jump const *jump = &map->jumps[index];

		wrote = snprintf(text + used, (size_t)(size - used), "jump %.2f %.2f %.2f %.2f %.2f %.2f %d %d\n",
			(double)jump->from[0], (double)jump->from[1], (double)jump->from[2], (double)jump->to[0], (double)jump->to[1],
			(double)jump->to[2], jump->uses, jump->misses);
		if (wrote < 0 || used + wrote >= size)
			return 0;
		used += wrote;
	}
	return used;
}

void ae_learnt_wedge(struct ae_learnt_map *map, long surface)
{
	int index, least = 0;

	if (surface < 0)
		return;
	for (index = 0; index < map->wedge_count; index++)
	{
		if (map->wedges[index].surface == surface)
		{
			if (map->wedges[index].fresh < AE_LEARNT_WEDGE_MOST)
				map->wedges[index].fresh++;
			return;
		}
		if (map->wedges[index].count + map->wedges[index].fresh < map->wedges[least].count + map->wedges[least].fresh)
			least = index;
	}
	/* (a full list: the least wedged piece gives way) */
	index = map->wedge_count < AE_LEARNT_WEDGES ? map->wedge_count++ : least;
	map->wedges[index].surface = surface;
	map->wedges[index].count = 0;
	map->wedges[index].fresh = 1;
}

void ae_learnt_stuck(struct ae_learnt_map *map, long surface, int bot, float x, float y, float z)
{
	int index, least = 0;
	struct ae_learnt_stuck *stuck = NULL;

	if (surface < 0 || bot < 0 || bot >= AE_LEARNT_BOTS)
		return;
	for (index = 0; index < map->stuck_count && !stuck; index++)
	{
		if (map->stucks[index].surface == surface)
			stuck = &map->stucks[index];
		else if (map->stucks[index].count + map->stucks[index].fresh < map->stucks[least].count + map->stucks[least].fresh)
			least = index;
	}
	if (!stuck)
	{
		index = map->stuck_count < AE_LEARNT_STUCKS ? map->stuck_count++ : least;
		stuck = &map->stucks[index];
		memset(stuck, 0, sizeof *stuck);
		stuck->surface = surface;
		stuck->x = x;
		stuck->y = y;
		stuck->z = z;
	}
	if (stuck->reported[bot] || stuck->fresh >= AE_LEARNT_STUCK_A_GAME)
		return;
	stuck->reported[bot] = 1;
	stuck->fresh++;
}

void ae_learnt_jump_result(struct ae_learnt_map *map, int jump, int made)
{
	if (jump < 0 || jump >= map->jump_count)
		return;
	if (made)
		map->jumps[jump].game_makes++;
	else
		map->jumps[jump].game_misses++;
}

int ae_learnt_jump_keep(float const *from, float const *to, float seconds_in_air, float alive_seconds_after)
{
	float dx = to[0] - from[0], dy = to[1] - from[1], dz = to[2] - from[2];

	if (!((dz >= AE_LEARNT_JUMP_HEIGHT || -dz >= AE_LEARNT_JUMP_HEIGHT) && seconds_in_air >= AE_LEARNT_JUMP_AIR &&
		alive_seconds_after >= AE_LEARNT_JUMP_ALIVE))
	{
		return 0;
	}
	return (float)sqrt(dx * dx + dy * dy) <= AE_LEARNT_JUMP_SPEED * seconds_in_air;
}

static float distance3(float const *a, float const *b)
{
	float dx = a[0] - b[0], dy = a[1] - b[1], dz = a[2] - b[2];

	return (float)sqrt(dx * dx + dy * dy + dz * dz);
}

int ae_learnt_jump_find(struct ae_learnt_map const *map, float const *from, float const *to)
{
	int index;

	for (index = 0; index < map->jump_count; index++)
	{
		if (distance3(map->jumps[index].from, from) <= AE_LEARNT_JUMP_SAME &&
			distance3(map->jumps[index].to, to) <= AE_LEARNT_JUMP_SAME)
		{
			return index;
		}
	}
	return -1;
}

int ae_learnt_jump_add(struct ae_learnt_map *map, float const *from, float const *to)
{
	int index = ae_learnt_jump_find(map, from, to), least = 0;
	struct ae_learnt_jump *jump;

	if (index >= 0)
	{
		if (map->jumps[index].uses < AE_LEARNT_JUMP_USES)
			map->jumps[index].uses++;
		return index;
	}
	for (index = 1; index < map->jump_count; index++)
	{
		if (map->jumps[index].uses < map->jumps[least].uses)
			least = index;
	}
	index = map->jump_count < AE_LEARNT_JUMPS ? map->jump_count++ : least;
	jump = &map->jumps[index];
	memset(jump, 0, sizeof *jump);
	memcpy(jump->from, from, 3 * sizeof(float));
	memcpy(jump->to, to, 3 * sizeof(float));
	jump->uses = 1;
	return index;
}

void ae_learnt_game_over(struct ae_learnt_map *map, int drive_seconds, int play_seconds)
{
	int index, kept;

	for (index = kept = 0; index < map->wedge_count; index++)
	{
		struct ae_learnt_wedge wedge = map->wedges[index];

		if (wedge.fresh)
			wedge.count += wedge.fresh;
		else if (drive_seconds >= AE_LEARNT_DRIVE_SECONDS)
			wedge.count--;
		if (wedge.count > AE_LEARNT_WEDGE_MOST)
			wedge.count = AE_LEARNT_WEDGE_MOST;
		wedge.fresh = 0;
		if (wedge.count > 0)
			map->wedges[kept++] = wedge;
	}
	map->wedge_count = kept;
	for (index = kept = 0; index < map->stuck_count; index++)
	{
		struct ae_learnt_stuck *stuck = &map->stucks[index];

		if (stuck->fresh)
			stuck->count += stuck->fresh;
		else if (play_seconds >= AE_LEARNT_PLAY_SECONDS)
			stuck->count--;
		if (stuck->count > AE_LEARNT_STUCK_MOST)
			stuck->count = AE_LEARNT_STUCK_MOST;
		stuck->fresh = 0;
		memset(stuck->reported, 0, sizeof stuck->reported);
		if (stuck->count > 0)
		{
			if (kept != index)
				map->stucks[kept] = *stuck;
			kept++;
		}
	}
	map->stuck_count = kept;
	for (index = kept = 0; index < map->jump_count; index++)
	{
		struct ae_learnt_jump jump = map->jumps[index];

		if (jump.game_misses >= AE_LEARNT_JUMP_GAME_MISSES && !jump.game_makes)
			jump.misses++;
		jump.game_misses = jump.game_makes = 0;
		if (jump.misses < AE_LEARNT_JUMP_MISSES)
			map->jumps[kept++] = jump;
	}
	map->jump_count = kept;
}

int ae_learnt_stuck_levels(int count)
{
	int levels = count - AE_LEARNT_STUCK_TO_AVOID + 1;

	if (levels <= 0)
		return 0;
	return levels > AE_LEARNT_STUCK_LEVELS ? AE_LEARNT_STUCK_LEVELS : levels;
}
